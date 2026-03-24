/**********************************************************************/
/*                                                                    */
/* Programmable Logic Controller for ESP microcontrollers             */
/*                                                                    */
/* Copyright (C) 2026 Denisov Foundation Limited                      */
/* License: GPLv3                                                     */
/* Written by Sergey Denisov aka LittleBuster                         */
/* Email: DenisovFoundationLtd@gmail.com                              */
/*                                                                    */
/**********************************************************************/

#include "core/network/web/handlers/stack_handler.hpp"

#include <atomic>

#include "core/network/network.hpp"
#include "core/network/web/web_interface.hpp"

namespace
{
constexpr uint32_t kStackPageCacheMs = 1500u;
String g_stack_page_cache;
uint32_t g_stack_page_cache_built_ms = 0;
uint8_t g_stack_page_cache_role = 0xFF;

bool canUseStackPageCache_(uint32_t now_ms, uint8_t role)
{
    if (!g_stack_page_cache.length())
        return false;
    if (g_stack_page_cache_role != role)
        return false;
    return (uint32_t)(now_ms - g_stack_page_cache_built_ms) <= kStackPageCacheMs;
}

String stackRuntimeBadgeClass_(const WebInterface &web)
{
    Network *network = web.network();
    if (!network)
        return "bad";
    const Network::StackRuntimeState st = network->stackRuntimeState();
    return (st == Network::StackRuntimeState::Online || st == Network::StackRuntimeState::FallbackMaster) ? "ok" : "bad";
}

String stackRuntimeBadgeText_(const WebInterface &web)
{
    Network *network = web.network();
    if (!network)
        return "stack: unavailable";

    switch (network->stackRuntimeState())
    {
    case Network::StackRuntimeState::Stopped:
        return "stack: stopped";
    case Network::StackRuntimeState::Starting:
        return "stack: starting";
    case Network::StackRuntimeState::AuthPending:
        return "stack: auth pending";
    case Network::StackRuntimeState::Online:
        return "stack: online";
    case Network::StackRuntimeState::Degraded:
        return "stack: degraded";
    case Network::StackRuntimeState::FallbackMaster:
        return "stack: fallback master";
    }
    return "stack: unknown";
}

String stackDiagnosticsHtml_(const WebInterface &web)
{
    Network *network = web.network();
    if (!network)
        return "";

    const Network::StackDiagnostics diag = network->stackDiagnostics();
    String html;
    html.reserve(960);
    html += "<div class=\"section\"><p class=\"status\">stack diagnostics</p><table><tbody>";
    auto addRow = [&html](const char *key, const String &value) {
        html += "<tr><th>";
        html += key;
        html += "</th><td><strong>";
        html += value;
        html += "</strong></td></tr>";
    };
    addRow("state", String(diag.runtime_state));
    addRow("master_active", diag.master_active ? "true" : "false");
    addRow("fallback_active", diag.fallback_active ? "true" : "false");
    addRow("online_devices", String((unsigned)diag.online_devices));
    addRow("network_lock_ms", String((unsigned long)diag.network_lock_held_ms));
    addRow("route_lock_ms", String((unsigned long)diag.exchange.lock_held_ms));
    addRow("exchange_slave_q", String((unsigned)diag.exchange.slave_outbox_used));
    addRow("exchange_master_q", String((unsigned)diag.exchange.master_inbox_used));
    addRow("notify_q", String((unsigned)diag.exchange.notify_outbox_used));
    addRow("retried", String((unsigned long)diag.exchange.retried));
    addRow("expired", String((unsigned long)diag.exchange.expired));
    addRow("dropped", String((unsigned long)diag.exchange.dropped));
    addRow("rs485_started", diag.rs485.started ? "true" : "false");
    addRow("rs485_state", String((unsigned)diag.rs485.bus_state));
    addRow("rs485_tx_q", String((unsigned)diag.rs485.tx_queue_used));
    addRow("rs485_pending", String((unsigned)diag.rs485.pending_used));
    addRow("rs485_timeouts", String((unsigned long)diag.rs485.request_timeouts));
    addRow("rs485_tx_drop", String((unsigned long)diag.rs485.tx_queue_drops));
    addRow("rs485_pend_drop", String((unsigned long)diag.rs485.pending_full_drops));
    addRow("rs485_last_tx", String((unsigned long)diag.rs485.last_tx_size));
    addRow("rs485_lock_ms", String((unsigned long)diag.rs485.lock_held_ms));
    html += "</tbody></table></div>";
    return html;
}
}

void StackHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/stack/nodes_tbody", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleNodesTbody(web, request); });
        server.on("/stack/slave_link", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleSlaveLinkStatus(web, request); });
        server.on("/stack/online_snapshot", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleOnlineSnapshot(web, request); });
        server.on("/stack", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleStack(web, request); });
    }

void StackHandler::handleStack(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        const auto role = web.stackRole_();
        const uint32_t now_ms = millis();
        String page;
        if (canUseStackPageCache_(now_ms, (uint8_t)role))
        {
            page = g_stack_page_cache;
        }
        else
        {
            page = FPSTR(kWebInterfaceStackHtml);
            page.reserve(page.length() + 4096);
            page.replace("%NAV%", web.navHtml_());
            page.replace("%STACK_ROLE%", web.stackRoleName_(role));
            page.replace("%STACK_ROLE_MASTER_SEL%", role == ConfigsManagerIface::StackRole::Master ? "selected" : "");
            page.replace("%STACK_ROLE_SLAVE_SEL%", role == ConfigsManagerIface::StackRole::Slave ? "selected" : "");
            page.replace("%STACK_SLAVE_STYLE%", role == ConfigsManagerIface::StackRole::Slave ? "" : "display:none");
            page.replace("%STACK_LINK_STYLE%", "");
            page.replace("%STACK_PAGE_TITLE%", WebUiRu::StackPage::kPageTitle);
            page.replace("%STACK_TITLE%", WebUiRu::StackPage::kTitle);
            page.replace("%STACK_LABEL_ROLE%", WebUiRu::StackPage::kRole);
            page.replace("%STACK_LABEL_MASTER_HOST%", WebUiRu::StackPage::kMasterHost);
            page.replace("%STACK_LABEL_EXCHANGE_POLICY%", WebUiRu::StackPage::kExchangePolicy);
            page.replace("%STACK_POLICY_AUTO_TEXT%", WebUiRu::StackPage::kPolicyAuto);
            page.replace("%STACK_POLICY_DIRECT_TEXT%", WebUiRu::StackPage::kPolicyDirect);
            page.replace("%STACK_POLICY_POLL_TEXT%", WebUiRu::StackPage::kPolicyPoll);
            page.replace("%STACK_LABEL_TRANSPORT%", WebUiRu::StackPage::kTransport);
            page.replace("%STACK_TRANSPORT_WS_TEXT%", WebUiRu::StackPage::kTransportWebSocket);
            page.replace("%STACK_TRANSPORT_RS485_TEXT%", WebUiRu::StackPage::kTransportRs485);
            page.replace("%STACK_LABEL_PAYLOAD_MODE%", WebUiRu::StackPage::kPayloadMode);
            page.replace("%STACK_PAYLOAD_AUTO_TEXT%", WebUiRu::StackPage::kPayloadAuto);
            page.replace("%STACK_PAYLOAD_JSON_TEXT%", WebUiRu::StackPage::kPayloadJson);
            page.replace("%STACK_PAYLOAD_BINARY_TEXT%", WebUiRu::StackPage::kPayloadBinary);
            page.replace("%STACK_LABEL_FALLBACK_MASTER%", WebUiRu::StackPage::kFallbackMaster);
            page.replace("%STACK_LABEL_ENABLE%", WebUiRu::StackPage::kEnable);
            page.replace("%STACK_LABEL_FALLBACK_HOST%", WebUiRu::StackPage::kFallbackHost);
            page.replace("%STACK_LABEL_SLAVE_CONTROLLER%", WebUiRu::StackPage::kSlaveController);
            page.replace("%STACK_LABEL_FULL_CONTROLLER%", WebUiRu::StackPage::kFullController);
            page.replace("%STACK_LABEL_API_KEY%", WebUiRu::StackPage::kApiKey);
            page.replace("%STACK_API_KEY_PLACEHOLDER%", WebUiRu::StackPage::kApiKeyPlaceholder);
            page.replace("%STACK_BTN_GEN_KEY%", WebUiRu::StackPage::kGenerate);
            g_stack_page_cache = page;
            g_stack_page_cache_built_ms = now_ms;
            g_stack_page_cache_role = (uint8_t)role;
        }
        page.replace("%STACK_SLAVE_LINK_DISCONNECTED%", "stack: offline");
        String slave_link_class = stackRuntimeBadgeClass_(web);
        String slave_link_text = stackRuntimeBadgeText_(web);
        page.replace("%STACK_SLAVE_LINK_CLASS%", slave_link_class);
        page.replace("%STACK_SLAVE_LINK_TEXT%", slave_link_text);
        page.replace("%STACK_MASTER_HOST%", web.stackMasterHost_());
        const auto policy = web.stackExchangePolicy_();
        page.replace("%STACK_POLICY_AUTO_SEL%", policy == ConfigsManagerIface::StackExchangePolicy::Auto ? "selected" : "");
        page.replace("%STACK_POLICY_DIRECT_SEL%", policy == ConfigsManagerIface::StackExchangePolicy::Direct ? "selected" : "");
        page.replace("%STACK_POLICY_POLL_SEL%", policy == ConfigsManagerIface::StackExchangePolicy::Poll ? "selected" : "");
        const auto transport = web.stackTransport_();
        page.replace("%STACK_TRANSPORT_WS_SEL%", transport == ConfigsManagerIface::StackTransportKind::WebSocket ? "selected" : "");
        page.replace("%STACK_TRANSPORT_RS485_SEL%", transport == ConfigsManagerIface::StackTransportKind::Rs485 ? "selected" : "");
        const auto payload_mode = web.stackPayloadMode_();
        page.replace("%STACK_PAYLOAD_AUTO_SEL%", payload_mode == ConfigsManagerIface::StackPayloadMode::Auto ? "selected" : "");
        page.replace("%STACK_PAYLOAD_JSON_SEL%", payload_mode == ConfigsManagerIface::StackPayloadMode::Json ? "selected" : "");
        page.replace("%STACK_PAYLOAD_BINARY_SEL%", payload_mode == ConfigsManagerIface::StackPayloadMode::Binary ? "selected" : "");
        page.replace("%STACK_FALLBACK_ENABLED_CHECKED%", web.stackFallbackEnabled_() ? "checked" : "");
        page.replace("%STACK_FALLBACK_HOST%", web.stackFallbackHost_());
        page.replace("%STACK_SLAVE_CONTROLLER_CHECKED%", web.stackSlaveController_() ? "checked" : "");
        page.replace("%STACK_API_KEY%", WebInterface::maskSecretValue_(web.stackApiKey_()));
        page.replace("%SAVE_TEXT%", WebUiRu::kSave);
        page.replace("%STACK_STATUS%", web._stack_status);
        page.replace("%STACK_DIAG_BLOCK%", stackDiagnosticsHtml_(web));
        if (role == ConfigsManagerIface::StackRole::Master)
        {
            String self = String("<p class=\"status\">") + WebUiRu::StackPage::kCurrentControllerPrefix + ": <strong>" +
                          web.deviceName_() + "</strong> | IP: <strong>" + web.wifiIp_() + "</strong></p>";
            page.replace("%STACK_SELF_BLOCK%", self);
            page.replace("%STACK_NODES_BLOCK%", web.stackNodesBlockHtml_());
        }
        else
        {
            page.replace("%STACK_SELF_BLOCK%", "");
            page.replace("%STACK_NODES_BLOCK%", "");
        }
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

void StackHandler::handleSlaveLinkStatus(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String cls = stackRuntimeBadgeClass_(web);
        String text = stackRuntimeBadgeText_(web);
        String json;
        json.reserve(96);
        json += "{\"class\":\"";
        json += cls;
        json += "\",\"text\":\"";
        json += text;
        json += "\"}";
        web.sendText_(request, 200, "application/json", json, set_cookie);
    }

void StackHandler::handleNodesTbody(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master)
        {
            web.sendText_(request, 200, "text/html; charset=utf-8", "", set_cookie);
            return;
        }
        web.sendText_(request, 200, "text/html; charset=utf-8", web.listStackNodesHtml_(), set_cookie);
    }

void StackHandler::handleOnlineSnapshot(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        Network *network = web.network();
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !network)
        {
            web.sendText_(request, 200, "application/json", "[]", set_cookie);
            return;
        }
        String out;
        out.reserve(512);
        out += "[";
        const size_t count = network->stackOnlineDeviceCount();
        bool first = true;
        for (size_t i = 0; i < count; ++i)
        {
            StackDeviceRegistry::DeviceInfo device;
            if (!network->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
                continue;
            const uint32_t id = device.node_id;
            String name = device.name[0] ? String(device.name) : String();
            bool sync_ready = false;
            if (network)
            {
                StackUnitSnapshot::Snapshot snapshot{};
                const bool has_snapshot = network->stackIndexStateSnapshot(id, snapshot);
                sync_ready = has_snapshot &&
                             snapshot.updated_ms != 0 &&
                             snapshot.socket_count >= snapshot.sockets_enabled &&
                             snapshot.light_count >= snapshot.lights_enabled;
            }
            if (!first)
                out += ",";
            out += "{\"id\":";
            out += String((unsigned long)id);
            out += ",\"name\":\"";
            if (name.length())
                web.appendJsonEscaped_(out, name.c_str());
            else
                out += web.stackNodeIdHex_(id);
            out += "\",\"sync\":";
            out += sync_ready ? "1" : "0";
            out += "}";
            first = false;
        }
        out += "]";
        web.sendText_(request, 200, "application/json", out, set_cookie);
    }
