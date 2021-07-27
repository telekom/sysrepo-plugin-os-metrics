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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <math.h>
#include <mutex>
#include <thread>

namespace metrics {
using libyang::Data_Node;
using libyang::Data_Node_Leaf_List;
using libyang::S_Context;
using libyang::S_Data_Node;
using libyang::S_Schema_Node;
using libyang::Schema_Node_Leaf;
using libyang::Schema_Node_Leaflist;
using libyang::Schema_Node_List;

struct UsageMonitoring {
    using thresholdMap_t = std::unordered_map<std::string, long double>;
    using fsThresholdTuple_t = std::tuple<uint32_t, thresholdMap_t>;

    void notify() {
        mCV.notify_all();
    }

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
        if (mThread.joinable()) {
            mCV.notify_all();
            mThread.join();
        }
    }

    void startThread() {
        if (mThread.joinable()) {
            mThread.join();
        }
        mThread = std::thread(&MemoryMonitoring::runFunc, this);
    }

    void runFunc() {
        std::unique_lock<std::mutex> lk(mNotificationMtx);
        while (mCV.wait_for(lk, std::chrono::seconds(pollInterval)) == std::cv_status::timeout) {
            long double value = MemoryStats::getInstance().getUsage();
            for (auto const& [name, thrValue] : mMemoryThesholds) {
                logMessage(SR_LL_DBG, std::string("Trigger notification for: ") + name + ": " +
                                          std::to_string(value));
            }
        }
    }

    void populateConfigData(sysrepo::S_Session& session, char const* module_name) {
        std::string const data_xpath(std::string("/") + module_name + ":system-metrics/memory");
        S_Data_Node toplevel(session->get_data(data_xpath.c_str()));
        std::shared_ptr<std::pair<std::string, long double>> threshold;
        mMemoryThesholds.clear();
        for (S_Data_Node& root : toplevel->tree_for()) {
            for (S_Data_Node const& node : root->tree_dfs()) {
                S_Schema_Node schema = node->schema();
                switch (schema->nodetype()) {
                case LYS_LIST: {
                    if (std::string(schema->name()) == "threshold" && threshold) {
                        mMemoryThesholds[threshold->first] = threshold->second;
                    }
                    break;
                }
                case LYS_LEAF: {
                    Data_Node_Leaf_List leaf(node);
                    Schema_Node_Leaf sleaf(schema);
                    if (sleaf.is_key()) {
                        threshold = std::make_shared<std::pair<std::string, long double>>();
                        threshold->first = leaf.value_str();
                    } else if (std::string(sleaf.name()) == "value") {
                        threshold->second = leaf.value()->dec64().value /
                                            std::pow(10, leaf.value()->dec64().digits);
                    }

                    if (std::string(sleaf.name()) == "poll-interval") {
                        pollInterval = leaf.value()->uint32();
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
        if (threshold) {
            mMemoryThesholds[threshold->first] = threshold->second;
        }
    }

private:
    MemoryMonitoring() : pollInterval(1){};
    thresholdMap_t mMemoryThesholds;
    std::thread mThread;
    uint32_t pollInterval;
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
        for (auto& [name, thread] : mFsThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void stopThreads() {
        for (auto& [_, thread] : mFsThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        mFsThreads.clear();
    }

    void startThreads() {
        for (auto const& [name, _] : mFsThresholds) {
            std::unordered_map<std::string, std::thread>::iterator itr;

            if ((itr = mFsThreads.find(name)) != mFsThreads.end() && itr->second.joinable()) {
                itr->second.join();
            }
            if (itr != mFsThreads.end()) {
                itr->second = std::thread(&FilesystemMonitoring::runFunc, this, name);
            } else {
                mFsThreads[name] = std::thread(&FilesystemMonitoring::runFunc, this, name);
            }
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
                }
            }
        }
    }

    void populateConfigData(sysrepo::S_Session& session, char const* module_name) {
        std::string const data_xpath(std::string("/") + module_name +
                                     ":system-metrics/filesystems");
        S_Data_Node toplevel(session->get_data(data_xpath.c_str()));
        thresholdMap_t thresholdMap;
        std::shared_ptr<std::pair<std::string, long double>> threshold;
        std::shared_ptr<fsThresholdTuple_t> pollingTuple;
        std::string mountPoint("");
        uint32_t poll(0);
        mFsThresholds.clear();
        for (S_Data_Node& root : toplevel->tree_for()) {
            for (S_Data_Node const& node : root->tree_dfs()) {
                S_Schema_Node schema = node->schema();
                switch (schema->nodetype()) {
                case LYS_LIST: {
                    if (std::string(schema->name()) == "threshold" && threshold) {
                        thresholdMap[threshold->first] = threshold->second;
                    }
                    break;
                }
                case LYS_LEAF: {
                    Data_Node_Leaf_List leaf(node);
                    Schema_Node_Leaf sleaf(schema);
                    if (sleaf.is_key() && std::string(sleaf.name()) == "mount-point") {
                        if (threshold) {
                            thresholdMap[threshold->first] = threshold->second;
                        }
                        if (!thresholdMap.empty()) {
                            mFsThresholds[mountPoint] = std::make_tuple(poll, thresholdMap);
                        }
                        thresholdMap.clear();
                        mountPoint = leaf.value_str();
                        threshold.reset();
                    } else if (sleaf.is_key() && std::string(sleaf.name()) == "name") {
                        threshold = std::make_shared<std::pair<std::string, long double>>();
                        threshold->first = leaf.value_str();
                    } else if (std::string(sleaf.name()) == "value") {
                        threshold->second = leaf.value()->dec64().value /
                                            std::pow(10, leaf.value()->dec64().digits);
                    }

                    if (std::string(sleaf.name()) == "poll-interval") {
                        poll = leaf.value()->uint32();
                    }
                    break;
                }
                default:
                    break;
                }
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
                std::cout << "\t thrname: " << thrName << " thrvalue" << thrValue << std::endl;
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
