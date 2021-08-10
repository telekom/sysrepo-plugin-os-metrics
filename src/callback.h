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

    static int cpuStateCallback(sysrepo::S_Session session,
                                const char* /* module_name */,
                                const char* /* path */,
                                const char* /* request_xpath */,
                                uint32_t /* request_id */,
                                libyang::S_Data_Node& parent) {
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
        return SR_ERR_OK;
    }

    static int memoryStateCallback(sysrepo::S_Session session,
                                   const char* /* module_name */,
                                   const char* /* path */,
                                   const char* /* request_xpath */,
                                   uint32_t /* request_id */,
                                   libyang::S_Data_Node& parent) {
        MemoryMonitoring::getInstance().setXpaths(session, parent);
        MemoryStats::getInstance().readMemoryStats();
        MemoryStats::getInstance().setXpathValues(session, parent);
        return SR_ERR_OK;
    }

    static int filesystemStateCallback(sysrepo::S_Session session,
                                       const char* module_name,
                                       const char* /* path */,
                                       const char* request_xpath,
                                       uint32_t /* request_id */,
                                       libyang::S_Data_Node& parent) {
        FilesystemMonitoring::getInstance().setXpaths(session, parent);
        FilesystemStats::getInstance().readFilesystemStats();
        FilesystemStats::getInstance().setXpathValues(session, parent);
        return SR_ERR_OK;
    }

    static int processesStateCallback(sysrepo::S_Session session,
                                      const char* module_name,
                                      const char* /* path */,
                                      const char* request_xpath,
                                      uint32_t /* request_id */,
                                      libyang::S_Data_Node& parent) {
        ProcessStats::getInstance().readAndSetAll(session, parent);
        return SR_ERR_OK;
    }

    static int memoryConfigCallback(sysrepo::S_Session session,
                                    const char* module_name,
                                    const char* /*xpath*/,
                                    sr_event_t /*event*/,
                                    uint32_t /*request_id*/) {
        printCurrentConfig(session, module_name, "system-metrics/memory//*");
        if (session->get_context()->get_module(module_name)->feature_state("usage-notifications") ==
            1) {
            {
                std::lock_guard<std::mutex> lk(MemoryMonitoring::getInstance().mNotificationMtx);
                MemoryMonitoring::getInstance().notify();
                MemoryMonitoring::getInstance().populateConfigData(session, module_name);
            }
            MemoryMonitoring::getInstance().startThread();
        } else {
            logMessage(SR_LL_WRN, "Feature not enabled: usage-notifications");
        }
        return SR_ERR_OK;
    }

    static int filesystemsConfigCallback(sysrepo::S_Session session,
                                         const char* module_name,
                                         const char* /*xpath*/,
                                         sr_event_t /*event*/,
                                         uint32_t /*request_id*/) {
        printCurrentConfig(session, module_name, "system-metrics/filesystems//*");
        if (session->get_context()->get_module(module_name)->feature_state("usage-notifications") ==
            1) {
            {
                std::lock_guard<std::mutex> lk(
                    FilesystemMonitoring::getInstance().mNotificationMtx);
                FilesystemMonitoring::getInstance().notify();
                FilesystemMonitoring::getInstance().populateConfigData(session, module_name);
            }
            FilesystemMonitoring::getInstance().startThreads();
        } else {
            logMessage(SR_LL_WRN, "Feature not enabled: usage-notifications");
        }
        return SR_ERR_OK;
    }

    static void printCurrentConfig(sysrepo::S_Session& session,
                                   char const* module_name,
                                   std::string const& node) {
        try {
            std::string xpath(std::string("/") + module_name + std::string(":") + node);
            auto values = session->get_items(xpath.c_str());
            if (values == nullptr)
                return;

            for (unsigned int i = 0; i < values->val_cnt(); i++)
                logMessage(SR_LL_DBG, values->val(i)->to_string());

        } catch (const std::exception& e) {
            logMessage(SR_LL_WRN, e.what());
        }
    }
};

}  // namespace metrics

#endif  // CALLBACK_H
