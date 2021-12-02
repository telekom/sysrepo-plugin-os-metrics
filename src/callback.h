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

#ifndef CALLBACK_H
#define CALLBACK_H

#include <cpu_stats.h>
#include <filesystem_stats.h>
#include <memory_stats.h>
#include <process_stats.h>
#include <threshold_manager.h>

namespace metrics {

struct Callback {
    using Session = sysrepo::Session;
    using ErrorCode = sysrepo::ErrorCode;
    using Event = sysrepo::Event;

    static ErrorCode cpuStateCallback(Session session,
                                      uint32_t /* subscriptionId */,
                                      std::string_view moduleName,
                                      std::optional<std::string_view> /* subXPath */,
                                      std::optional<std::string_view> /* requestXPath */,
                                      uint32_t /* requestId */,
                                      std::optional<libyang::DataNode>& parent) {
        CpuStats stats;
        stats.readCpuTimes();
        stats.setXpathValues(session, parent);
        double loadavg[3];
        if (getloadavg(loadavg, 3) != -1) {
            setXpath(session, parent,
                     "/dt-metrics:system-metrics/cpu-statistics/average-load/avg-1min-load",
                     std::to_string(loadavg[0]));
            setXpath(session, parent,
                     "/dt-metrics:system-metrics/cpu-statistics/average-load/avg-5min-load",
                     std::to_string(loadavg[1]));
            setXpath(session, parent,
                     "/dt-metrics:system-metrics/cpu-statistics/average-load/avg-15min-load",
                     std::to_string(loadavg[2]));
        } else {
            logMessage(SR_LL_ERR, "getloadavg call failed");
        }
        return ErrorCode::Ok;
    }

    static ErrorCode memoryStateCallback(Session session,
                                         uint32_t /* subscriptionId */,
                                         std::string_view moduleName,
                                         std::optional<std::string_view> /* subXPath */,
                                         std::optional<std::string_view> /* requestXPath */,
                                         uint32_t /* requestId */,
                                         std::optional<libyang::DataNode>& parent) {
        auto module = findModule(session, moduleName);
        if (module && module.value().featureEnabled("usage-notifications")) {
            MemoryMonitoring::getInstance().setXpaths(session, parent);
        }
        MemoryStats::getInstance().readMemoryStats();
        MemoryStats::getInstance().setXpathValues(session, parent);
        return ErrorCode::Ok;
    }

    static ErrorCode filesystemStateCallback(Session session,
                                             uint32_t /* subscriptionId */,
                                             std::string_view moduleName,
                                             std::optional<std::string_view> /* subXPath */,
                                             std::optional<std::string_view> /* requestXPath */,
                                             uint32_t /* requestId */,
                                             std::optional<libyang::DataNode>& parent) {
        auto module = findModule(session, moduleName);
        if (module && module.value().featureEnabled("usage-notifications")) {
            FilesystemMonitoring::getInstance().setXpaths(session, parent);
        }
        FilesystemStats::getInstance().readFilesystemStats();
        FilesystemStats::getInstance().setXpathValues(session, parent);
        return ErrorCode::Ok;
    }

    static ErrorCode processesStateCallback(Session session,
                                            uint32_t /* subscriptionId */,
                                            std::string_view moduleName,
                                            std::optional<std::string_view> /* subXPath */,
                                            std::optional<std::string_view> /* requestXPath */,
                                            uint32_t /* requestId */,
                                            std::optional<libyang::DataNode>& parent) {
        ProcessStats::getInstance().readAndSetAll(session, parent);
        return ErrorCode::Ok;
    }

    static ErrorCode memoryConfigCallback(Session session,
                                          uint32_t /* subscriptionId */,
                                          std::string_view moduleName,
                                          std::optional<std::string_view> /* subXPath */,
                                          Event /* event */,
                                          uint32_t /* request_id */) {
        printCurrentConfig(session, moduleName, "system-metrics/memory//*");
        auto module = findModule(session, moduleName);
        if (module && module.value().featureEnabled("usage-notifications")) {
            MemoryMonitoring::getInstance().notifyAndJoin();
            MemoryMonitoring::getInstance().populateConfigData(session, moduleName);
            MemoryMonitoring::getInstance().startThread();
        } else {
            logMessage(SR_LL_WRN, "Feature not enabled: usage-notifications");
        }
        return ErrorCode::Ok;
    }

    static ErrorCode filesystemsConfigCallback(Session session,
                                               uint32_t /* subscriptionId */,
                                               std::string_view moduleName,
                                               std::optional<std::string_view> /* subXPath */,
                                               Event /* event */,
                                               uint32_t /* request_id */) {
        printCurrentConfig(session, moduleName, "system-metrics/filesystems//*");
        auto module = findModule(session, moduleName);
        if (module && module.value().featureEnabled("usage-notifications")) {
            FilesystemMonitoring::getInstance().notifyAndJoin();
            FilesystemMonitoring::getInstance().populateConfigData(session, moduleName);
            FilesystemMonitoring::getInstance().startThreads();
        } else {
            logMessage(SR_LL_WRN, "Feature not enabled: usage-notifications");
        }
        return ErrorCode::Ok;
    }

    static void
    printCurrentConfig(Session& session, std::string_view module_name, std::string const& node) {
        try {
            std::string xpath(std::string("/") + std::string(module_name) + std::string(":") +
                              node);
            auto values = session.getData(xpath.c_str());
            if (!values)
                return;

            std::string const toPrint(
                values.value()
                    .printStr(libyang::DataFormat::JSON, libyang::PrintFlags::WithSiblings)
                    .value());
            logMessage(SR_LL_DBG, toPrint.c_str());
        } catch (const std::exception& e) {
            logMessage(SR_LL_WRN, e.what());
        }
    }

    static std::optional<libyang::Module> findModule(sysrepo::Session session,
                                                     std::string_view moduleName) {
        auto const& modules = session.getContext().modules();
        auto module = std::find_if(
            modules.begin(), modules.end(),
            [moduleName](libyang::Module const& module) { return moduleName == module.name(); });
        if (module == std::end(modules)) {
            return std::nullopt;
        }
        return *module;
    }
};

}  // namespace metrics

#endif  // CALLBACK_H
