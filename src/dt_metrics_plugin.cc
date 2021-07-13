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

extern "C" {
void sr_plugin_cleanup_cb(sr_session_ctx_t* session, void* private_data);
int sr_plugin_init_cb(sr_session_ctx_t* session, void** private_data);
}

using sysrepo::S_Session;
using sysrepo::S_Subscribe;
using sysrepo::Session;
using sysrepo::Subscribe;

struct MetricsModel {
    S_Subscribe subscription;
    S_Session sess;
    static std::string const moduleName;
};

std::string const MetricsModel::moduleName = "dt-metrics";

static MetricsModel theModel;

int sr_plugin_init_cb(sr_session_ctx_t* session, void** /*private_data*/) {
    theModel.sess = std::make_shared<Session>(session);
    theModel.subscription = std::make_shared<Subscribe>(theModel.sess);
    std::string cpu_state_xpath("/" + MetricsModel::moduleName + ":" +
                                "system-metrics/cpu-statistics");
    std::string memory_state_xpath("/" + MetricsModel::moduleName + ":" +
                                   "system-metrics/memory/statistics");
    std::string filesystem_state_xpath("/" + MetricsModel::moduleName + ":" +
                                       "system-metrics/filesystems");
    try {
        theModel.subscription->oper_get_items_subscribe(MetricsModel::moduleName.c_str(),
                                                        &metrics::Callback::cpuStateCallback,
                                                        cpu_state_xpath.c_str());
        theModel.subscription->oper_get_items_subscribe(MetricsModel::moduleName.c_str(),
                                                        &metrics::Callback::memoryStateCallback,
                                                        memory_state_xpath.c_str());
        theModel.subscription->oper_get_items_subscribe(MetricsModel::moduleName.c_str(),
                                                        &metrics::Callback::filesystemStateCallback,
                                                        filesystem_state_xpath.c_str());
    } catch (std::exception const& e) {
        logMessage(SR_LL_ERR, std::string("sr_plugin_init_cb: ") + e.what());
        theModel.subscription.reset();
        return SR_ERR_OPERATION_FAILED;
    }

    return SR_ERR_OK;
}

void sr_plugin_cleanup_cb(sr_session_ctx_t* /*session*/, void* /*private_data*/) {
    theModel.subscription.reset();
    theModel.sess.reset();
    logMessage(SR_LL_DBG, "plugin cleanup finished.");
}
