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

#ifndef GLOBALS_H
#define GLOBALS_H

#define FILESYSTEM_STATS_LOCATION "/tmp/df_output1.tmp"
#define FILESYSTEM_STATS_LOCATION2 "/tmp/df_output2.tmp"

#include <sysrepo-cpp/Session.hpp>
#include <sysrepo.h>

static void logMessage(sr_log_level_t log, std::string const& msg) {
    static std::string const _("DT-Metrics");
    switch (log) {
    case SR_LL_ERR:
        SRPLG_LOG_ERR(_.c_str(), msg.c_str());
        break;
    case SR_LL_WRN:
        SRPLG_LOG_WRN(_.c_str(), msg.c_str());
        break;
    case SR_LL_INF:
        SRPLG_LOG_INF(_.c_str(), msg.c_str());
        break;
    case SR_LL_DBG:
    default:
        SRPLG_LOG_DBG(_.c_str(), msg.c_str());
    }
}

static bool setXpath(sysrepo::Session& session,
                     std::optional<libyang::DataNode>& parent,
                     std::string const& node_xpath,
                     std::string const& value) {
    try {
        if (parent) {
            parent.value().newPath(node_xpath.c_str(), value.c_str());
        } else {
            parent = session.getContext().newPath(node_xpath.c_str(), value.c_str());
        }
    } catch (std::runtime_error const& e) {
        logMessage(SR_LL_WRN,
                   "At path " + node_xpath + ", value " + value + " " + ", error: " + e.what());
        return false;
    }
    return true;
}

#endif  // GLOBALS_H
