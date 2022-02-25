// telekom / sysrepo-plugin-os-metrics
//
// This program is made available under the terms of the
// BSD 3-Clause license which is available at
// https://opensource.org/licenses/BSD-3-Clause
//
// SPDX-FileCopyrightText: 2022 Deutsche Telekom AG
//
// SPDX-License-Identifier: BSD-3-Clause

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
    using DataNode = libyang::DataNode;

    static ErrorCode cpuStateCallback(Session session,
                                      uint32_t /* subscriptionId */,
                                      std::string_view moduleName,
                                      std::optional<std::string_view> /* subXPath */,
                                      std::optional<std::string_view> /* requestXPath */,
                                      uint32_t /* requestId */,
                                      std::optional<DataNode>& parent) {
        CpuStats stats;
        stats.readCpuTimes();
        stats.setXpathValues(session, parent, moduleName);
        double loadavg[3];
        if (getloadavg(loadavg, 3) != -1) {
            setXpath(session, parent,
                     "/" + std::string(moduleName) +
                         ":system-metrics/cpu-statistics/average-load/avg-1min-load",
                     std::to_string(loadavg[0]));
            setXpath(session, parent,
                     "/" + std::string(moduleName) +
                         ":system-metrics/cpu-statistics/average-load/avg-5min-load",
                     std::to_string(loadavg[1]));
            setXpath(session, parent,
                     "/" + std::string(moduleName) +
                         ":system-metrics/cpu-statistics/average-load/avg-15min-load",
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
                                         std::optional<DataNode>& parent) {
        auto module = findModule(session, moduleName);
        if (module && module.value().featureEnabled("usage-notifications")) {
            MemoryMonitoring::getInstance().setXpaths(session, parent, moduleName);
        }
        MemoryStats::getInstance().readMemoryStats();
        MemoryStats::getInstance().setXpathValues(session, parent, moduleName);
        return ErrorCode::Ok;
    }

    static ErrorCode filesystemStateCallback(Session session,
                                             uint32_t /* subscriptionId */,
                                             std::string_view moduleName,
                                             std::optional<std::string_view> /* subXPath */,
                                             std::optional<std::string_view> /* requestXPath */,
                                             uint32_t /* requestId */,
                                             std::optional<DataNode>& parent) {
        auto module = findModule(session, moduleName);
        if (module && module.value().featureEnabled("usage-notifications")) {
            FilesystemMonitoring::getInstance().setXpaths(session, parent, moduleName);
        }
        FilesystemStats::getInstance().readFilesystemStats();
        FilesystemStats::getInstance().setXpathValues(session, parent, moduleName);
        return ErrorCode::Ok;
    }

    static ErrorCode processesStateCallback(Session session,
                                            uint32_t /* subscriptionId */,
                                            std::string_view moduleName,
                                            std::optional<std::string_view> /* subXPath */,
                                            std::optional<std::string_view> /* requestXPath */,
                                            uint32_t /* requestId */,
                                            std::optional<DataNode>& parent) {
        ProcessStats::getInstance().readAndSetAll(session, parent, moduleName);
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
};

}  // namespace metrics

#endif  // CALLBACK_H
