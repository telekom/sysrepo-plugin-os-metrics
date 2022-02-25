// telekom / sysrepo-plugin-os-metrics
//
// This program is made available under the terms of the
// BSD 3-Clause license which is available at
// https://opensource.org/licenses/BSD-3-Clause
//
// SPDX-FileCopyrightText: 2022 Deutsche Telekom AG
//
// SPDX-License-Identifier: BSD-3-Clause

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

// Main moduleName entry-point
std::string const MetricsModel::moduleName = "os-metrics";

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
        metrics::MemoryMonitoring::getInstance().injectConnection(conn, MetricsModel::moduleName);
        metrics::FilesystemMonitoring::getInstance().injectConnection(conn,
                                                                      MetricsModel::moduleName);

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
