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

#include <callback.h>

#include <stdio.h>
#include <sysrepo-cpp/Connection.hpp>

extern "C" {
void sr_plugin_cleanup_cb(sr_session_ctx_t* session, void* private_data);
int sr_plugin_init_cb(sr_session_ctx_t* session, void** private_data);
}

struct MetricsModel {
    std::shared_ptr<sysrepo::Subscription> sub;
    static std::string const moduleName;
};

std::string const MetricsModel::moduleName = "dt-metrics";

static MetricsModel theModel;

int sr_plugin_init_cb(sr_session_ctx_t* session, void** /*private_data*/) {
    sysrepo::Connection conn;
    sysrepo::Session ses = conn.sessionStart();
    std::string const cpu_state_xpath("/" + MetricsModel::moduleName + ":" +
                                      "system-metrics/cpu-statistics");
    std::string const memory_state_xpath("/" + MetricsModel::moduleName + ":" +
                                         "system-metrics/memory/statistics");
    std::string const memory_config_xpath("/" + MetricsModel::moduleName + ":" +
                                          "system-metrics/memory");
    std::string const filesystem_state_xpath("/" + MetricsModel::moduleName + ":" +
                                             "system-metrics/filesystems");
    std::string const processes_state_spath("/" + MetricsModel::moduleName + ":" +
                                            "system-metrics/processes");
    try {
        metrics::MemoryMonitoring::getInstance().injectConnection(conn);
        metrics::FilesystemMonitoring::getInstance().injectConnection(conn);

        sysrepo::Subscription sub = ses.onModuleChange(
            MetricsModel::moduleName.c_str(), &metrics::Callback::memoryConfigCallback,
            memory_config_xpath.c_str(), 0,
            sysrepo::SubscribeOptions::Enabled | sysrepo::SubscribeOptions::DoneOnly);
        sub.onModuleChange(
            MetricsModel::moduleName.c_str(), &metrics::Callback::filesystemsConfigCallback,
            filesystem_state_xpath.c_str(), 0,
            sysrepo::SubscribeOptions::Enabled | sysrepo::SubscribeOptions::DoneOnly);
        sub.onOperGet(MetricsModel::moduleName.c_str(), &metrics::Callback::cpuStateCallback,
                      cpu_state_xpath.c_str());
        sub.onOperGet(MetricsModel::moduleName.c_str(), &metrics::Callback::memoryStateCallback,
                      memory_state_xpath.c_str());
        sub.onOperGet(MetricsModel::moduleName.c_str(), &metrics::Callback::filesystemStateCallback,
                      filesystem_state_xpath.c_str());
        sub.onOperGet(MetricsModel::moduleName.c_str(), &metrics::Callback::processesStateCallback,
                      processes_state_spath.c_str());
        theModel.sub = std::make_shared<sysrepo::Subscription>(std::move(sub));
    } catch (std::exception const& e) {
        logMessage(SR_LL_ERR, std::string("sr_plugin_init_cb: ") + e.what());
        theModel.sub.reset();
        return SR_ERR_OPERATION_FAILED;
    }

    return SR_ERR_OK;
}

void sr_plugin_cleanup_cb(sr_session_ctx_t* /*session*/, void* /*private_data*/) {
    theModel.sub.reset();
    logMessage(SR_LL_DBG, "plugin cleanup finished.");
}
