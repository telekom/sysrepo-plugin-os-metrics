// (C) 2020 Deutsche Telekom AG.
//
// Deutsche Telekom AG and all other contributors /
// copyright owners license this file to you under the Apache
// License, Version 2.0 (the "License"); you may not use this
// file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef THRESHOLD_MANAGER_H
#define THRESHOLD_MANAGER_H

#include <filesystem_stats.h>
#include <memory_stats.h>
#include <utils/globals.h>

#include <chrono>
#include <condition_variable>
#include <map>
#include <math.h>
#include <mutex>
#include <thread>

namespace metrics {
using libyang::Context;
using libyang::DataNode;
using sysrepo::Connection;

struct Threshold {
    Threshold(long double val = 0.0) : value(val), rising(false), falling(false){};
    long double value;
    bool rising;
    bool falling;
};

struct UsageMonitoring {
    using thresholdMap_t = std::unordered_map<std::string, Threshold>;
    using fsThresholdTuple_t = std::tuple<uint32_t, thresholdMap_t>;

    void notify() {
        mCV.notify_all();
    }

    void injectConnection(Connection conn) {
        mConn = std::make_shared<Connection>(conn);
    }

    void checkAndTriggerNotification(std::string const& sensName,
                                     Threshold const& thr,
                                     long double value,
                                     std::string const& type,
                                     std::string mountPoint = std::string()) {
        std::string notifPath("/dt-metrics:" + type + "-threshold-crossed");

        /* start session */
        if (!mConn) {
            return;
        }
        auto sess = mConn->sessionStart();

        auto input = sess.getContext().newPath((notifPath + "/name").c_str(), sensName.c_str());
        if (type == "filesystem") {
            input.newPath((notifPath + "/mount-point").c_str(), mountPoint.c_str());
        }
        if (value >= thr.value) {
            input.newPath((notifPath + "/rising").c_str(), nullptr);
        } else {
            input.newPath((notifPath + "/falling").c_str(), nullptr);
        }
        std::stringstream stream;
        stream << std::fixed << std::setprecision(2) << value;
        input.newPath((notifPath + "/usage").c_str(), stream.str().c_str());

        sess.sendNotification(input, sysrepo::Wait::No);
    }

    std::shared_ptr<Connection> mConn;
    std::mutex mNotificationMtx;
    std::condition_variable mCV;
};

struct MemoryMonitoring : public UsageMonitoring {

    static MemoryMonitoring& getInstance() {
        static MemoryMonitoring instance;
        return instance;
    }

    MemoryMonitoring(MemoryMonitoring const&) = delete;
    void operator=(MemoryMonitoring const&) = delete;

    ~MemoryMonitoring() {
        notifyAndJoin();
    }

    void notifyAndJoin() {
        mCV.notify_all();
        if (mThread.joinable()) {
            mThread.join();
        }
    }

    void startThread() {
        if (mMemoryThesholds.empty()) {
            return;
        }
        if (mThread.joinable()) {
            mThread.join();
        }
        logMessage(SR_LL_DBG, "Thread for memory thresholds started.");
        mThread = std::thread(&MemoryMonitoring::runFunc, this);
    }

    void runFunc() {
        std::unique_lock<std::mutex> lk(mNotificationMtx);
        while (mCV.wait_for(lk, std::chrono::seconds(mPollInterval)) == std::cv_status::timeout) {
            long double value = MemoryStats::getInstance().getUsage();
            for (auto const& [name, thrValue] : mMemoryThesholds) {
                logMessage(SR_LL_DBG, std::string("Trigger notification for: ") + name + ": " +
                                          std::to_string(value));
                checkAndTriggerNotification(name, thrValue, value, "memory");
            }
        }
        logMessage(SR_LL_DBG, "Thread for memory thresholds ended.");
    }

    void populateConfigData(sysrepo::Session& session, std::string_view moduleName) {
        std::string const data_xpath(std::string("/") + std::string(moduleName) +
                                     ":system-metrics/memory");
        auto const& data(session.getData(data_xpath.c_str()));
        if (!data) {
            logMessage(SR_LL_ERR, "No data found for population.");
            return;
        }
        std::shared_ptr<std::pair<std::string, Threshold>> threshold;
        mMemoryThesholds.clear();

        for (libyang::DataNode const& node : data.value().childrenDfs()) {
            libyang::SchemaNode schema = node.schema();
            switch (schema.nodeType()) {
            case libyang::NodeType::List: {
                if (std::string(schema.name()) == "threshold" && threshold) {
                    mMemoryThesholds[threshold->first] = threshold->second;
                }
                break;
            }
            case libyang::NodeType::Leaf: {
                if (schema.asLeaf().isKey()) {
                    threshold = std::make_shared<std::pair<std::string, Threshold>>();
                    threshold->first = node.asTerm().valueStr();
                } else if (std::string(schema.name()) == "value") {
                    threshold->second.value =
                        std::get<libyang::Decimal64>(node.asTerm().value()).number /
                        std::pow(10, std::get<libyang::Decimal64>(node.asTerm().value()).digits);
                }

                if (std::string(schema.name()) == "poll-interval") {
                    mPollInterval = std::get<uint32_t>(node.asTerm().value());
                }
                break;
            }
            default:
                break;
            }
        }
        if (threshold) {
            mMemoryThesholds[threshold->first] = threshold->second;
        }
    }

    void setXpaths(sysrepo::Session session, std::optional<libyang::DataNode>& parent) const {
        std::string configPath("/dt-metrics:system-metrics/memory/usage-monitoring/");
        setXpath(session, parent, configPath + "poll-interval", std::to_string(mPollInterval));
        for (auto const& [name, thr] : mMemoryThesholds) {
            std::stringstream stream;
            stream << std::fixed << std::setprecision(2) << thr.value;
            setXpath(session, parent, configPath + "threshold[name='" + name + "']/value",
                     stream.str());
        }
    }

private:
    MemoryMonitoring() : mPollInterval(60){};
    thresholdMap_t mMemoryThesholds;
    std::thread mThread;
    uint32_t mPollInterval;
};

struct FilesystemMonitoring : public UsageMonitoring {
    static FilesystemMonitoring& getInstance() {
        static FilesystemMonitoring instance;
        return instance;
    }

    FilesystemMonitoring(FilesystemMonitoring const&) = delete;
    void operator=(FilesystemMonitoring const&) = delete;

    ~FilesystemMonitoring() {
        notifyAndJoin();
    }

    void notifyAndJoin() {
        mCV.notify_all();
        stopThreads();
    }

    void stopThreads() {
        int32_t numThreadsStopped(0);
        for (auto& [_, thread] : mFsThreads) {
            if (thread.joinable()) {
                thread.join();
                numThreadsStopped++;
            }
        }
        logMessage(SR_LL_DBG, std::to_string(numThreadsStopped) +
                                  " filesystem threads stopped, out of: " +
                                  std::to_string(mFsThreads.size()) + " started.");
        mFsThreads.clear();
    }

    void startThreads() {
        if (mFsThresholds.empty()) {
            return;
        }
        for (auto const& [name, _] : mFsThresholds) {
            logMessage(SR_LL_DBG, "Starting thread for filesystem: " + name + ".");
            mFsThreads[name] = std::thread(&FilesystemMonitoring::runFunc, this, name);
        }
    }

    void runFunc(std::string const& name) {
        std::unique_lock<std::mutex> lk(mNotificationMtx);
        std::unordered_map<std::string, fsThresholdTuple_t>::iterator itr;
        if ((itr = mFsThresholds.find(name)) != mFsThresholds.end()) {
            while (mCV.wait_for(lk, std::chrono::seconds(std::get<0>(itr->second))) ==
                   std::cv_status::timeout) {
                std::optional<long double> usageValue =
                    FilesystemStats::getInstance().getUsage(name);
                if (!usageValue) {
                    break;
                }
                for (auto const& [thrName, thrValue] : std::get<1>(itr->second)) {
                    logMessage(SR_LL_DBG, std::string("Trigger notification for: ") + thrName +
                                              ": " + std::to_string(usageValue.value()));
                    checkAndTriggerNotification(thrName, thrValue, usageValue.value(), "filesystem",
                                                name);
                }
            }
        }
        logMessage(SR_LL_DBG, "Thread for filesystem: " + name + " ended.");
    }

    void populateConfigData(sysrepo::Session& session, std::string_view moduleName) {
        std::string const data_xpath(std::string("/") + std::string(moduleName) +
                                     ":system-metrics/filesystems");
        auto const& data(session.getData(data_xpath.c_str()));
        if (!data) {
            logMessage(SR_LL_ERR, "No data found for population.");
            return;
        }

        thresholdMap_t thresholdMap;
        std::shared_ptr<std::pair<std::string, Threshold>> threshold;
        std::string mountPoint("");
        uint32_t poll(60);
        mFsThresholds.clear();
        for (libyang::DataNode const& node : data.value().childrenDfs()) {
            libyang::SchemaNode schema = node.schema();
            switch (schema.nodeType()) {
            case libyang::NodeType::List: {
                if (std::string(schema.name()) == "threshold" && threshold) {
                    thresholdMap[threshold->first] = threshold->second;
                }
                break;
            }
            case libyang::NodeType::Leaf: {
                if (schema.asLeaf().isKey() && std::string(schema.name()) == "mount-point") {
                    if (threshold) {
                        thresholdMap[threshold->first] = threshold->second;
                    }
                    if (!thresholdMap.empty()) {
                        mFsThresholds[mountPoint] = std::make_tuple(poll, thresholdMap);
                    }
                    thresholdMap.clear();
                    mountPoint = node.asTerm().valueStr();
                    threshold.reset();
                } else if (schema.asLeaf().isKey() && std::string(schema.name()) == "name") {
                    threshold = std::make_shared<std::pair<std::string, Threshold>>();
                    threshold->first = node.asTerm().valueStr();
                } else if (std::string(schema.name()) == "value") {
                    threshold->second.value =
                        std::get<libyang::Decimal64>(node.asTerm().value()).number /
                        std::pow(10, std::get<libyang::Decimal64>(node.asTerm().value()).digits);
                }

                if (std::string(schema.name()) == "poll-interval") {
                    poll = std::get<uint32_t>(node.asTerm().value());
                }
                break;
            }
            default:
                break;
            }
        }
        if (threshold) {
            thresholdMap[threshold->first] = threshold->second;
            mFsThresholds[mountPoint] = std::make_tuple(poll, thresholdMap);
        }
    }

    void printFsConfig() {
        for (auto const& [name, thresholds] : mFsThresholds) {
            std::cout << "name: " << name << "\t poll:" << std::get<0>(thresholds);
            for (auto const& [thrName, thrValue] : std::get<1>(thresholds)) {
                std::cout << "\t thr-name: " << thrName << " thr-value" << thrValue.value << " "
                          << thrValue.rising << " " << thrValue.falling << std::endl;
            }
        }
    }

    void setXpaths(sysrepo::Session session, std::optional<libyang::DataNode>& parent) const {
        for (auto const& [fsName, thresholdTuple] : mFsThresholds) {
            std::string const configPath(
                "/dt-metrics:system-metrics/filesystems/filesystem[mount-point='" + fsName +
                "']/usage-monitoring/");
            setXpath(session, parent, configPath + "poll-interval",
                     std::to_string(std::get<0>(thresholdTuple)));
            for (auto const& [name, thr] : std::get<1>(thresholdTuple)) {
                std::stringstream stream;
                stream << std::fixed << std::setprecision(2) << thr.value;
                setXpath(session, parent, configPath + "threshold[name='" + name + "']/value",
                         stream.str());
            }
        }
    }

private:
    FilesystemMonitoring() = default;
    std::unordered_map<std::string, fsThresholdTuple_t> mFsThresholds;
    std::unordered_map<std::string, std::thread> mFsThreads;
};

}  // namespace metrics

#endif  // THRESHOLD_MANAGER_H
