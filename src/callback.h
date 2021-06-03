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
#include <memory_stats.h>

namespace metrics {

struct Callback {

    static int cpuStateCallback(sysrepo::S_Session session,
                                const char* module_name,
                                const char* /* path */,
                                const char* request_xpath,
                                uint32_t /* request_id */,
                                libyang::S_Data_Node& parent) {
        CpuStats stats;
        stats.readCpuTimes();
        stats.setXpathValues(session, parent);
        double loadavg[3];
        if (getloadavg(loadavg, 3) != -1) {
            setXpath(session, parent, "/dt-metrics:metrics/cpu-state/average-load/avg-1min-load",
                     std::to_string(loadavg[0]));
            setXpath(session, parent, "/dt-metrics:metrics/cpu-state/average-load/avg-5min-load",
                     std::to_string(loadavg[1]));
            setXpath(session, parent, "/dt-metrics:metrics/cpu-state/average-load/avg-15min-load",
                     std::to_string(loadavg[2]));
        }
        return SR_ERR_OK;
    }

    static int memoryStateCallback(sysrepo::S_Session session,
                                   const char* module_name,
                                   const char* /* path */,
                                   const char* request_xpath,
                                   uint32_t /* request_id */,
                                   libyang::S_Data_Node& parent) {
        MemoryStats stats;
        stats.readMemoryStats();
        stats.setXpathValues(session, parent);

        return SR_ERR_OK;
    }
};

}  // namespace metrics

#endif  // CALLBACK_H
