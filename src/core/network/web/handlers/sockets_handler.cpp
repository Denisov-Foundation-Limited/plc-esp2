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

#include "core/network/web/handlers/sockets_handler.hpp"

#include <atomic>

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

#include "core/network/web/web_interface.hpp"

namespace
{
constexpr uint32_t kSocketsMinIntervalMs = 250u;
constexpr uint32_t kSocketsPortsMinIntervalMs = 400u;
constexpr uint32_t kSocketsLowHeapBytes = 20u * 1024u;
constexpr uint32_t kSocketsLocalPortsCacheMs = 3000u;

std::atomic<uint32_t> g_last_sockets_request_ms{0};
std::atomic<uint32_t> g_last_sockets_ports_request_ms{0};
std::atomic<bool> g_sockets_request_inflight{false};
std::atomic<bool> g_sockets_ports_request_inflight{false};
String g_sockets_local_ports_cache_body;
uint32_t g_sockets_local_ports_cache_built_ms = 0;
bool g_sockets_local_ports_cache_valid = false;

bool requestTooFrequent_(std::atomic<uint32_t> &stamp, uint32_t now_ms, uint32_t min_interval_ms)
{
    const uint32_t prev = stamp.load(std::memory_order_relaxed);
    if (prev != 0 && (uint32_t)(now_ms - prev) < min_interval_ms)
        return true;
    stamp.store(now_ms, std::memory_order_relaxed);
    return false;
}

bool localSocketsPortsCacheFresh_(uint32_t now_ms)
{
    return g_sockets_local_ports_cache_valid &&
           g_sockets_local_ports_cache_body.length() != 0 &&
           (uint32_t)(now_ms - g_sockets_local_ports_cache_built_ms) < kSocketsLocalPortsCacheMs;
}

void invalidateLocalSocketsPortsCache_()
{
    g_sockets_local_ports_cache_valid = false;
    g_sockets_local_ports_cache_built_ms = 0;
    g_sockets_local_ports_cache_body = "";
}
}

void SocketsHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/sockets/list", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleSocketsList(web, request); });
        server.on("/sockets/ports_options", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleSocketsPortsOptions(web, request); });
        server.on("/sockets/toggle", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleSocketsToggle(web, request); });
        server.on("/sockets/toggle", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleSocketsToggle(web, request); });
        server.on("/sockets/enable", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleSocketsEnable(web, request); });
        server.on("/sockets/enable", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleSocketsEnable(web, request); });
        server.on("/sockets", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleSocketsSave(web, request, "/sockets"); });
        server.on("/sockets", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleSockets(web, request); });
    }

void SocketsHandler::handleSockets(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Sockets, node_id))
            return;
        const uint32_t started_ms = millis();
        const uint32_t free_heap0 = ESP.getFreeHeap();
        if (g_sockets_request_inflight.exchange(true, std::memory_order_acq_rel))
        {
            if (web._log)
                web._log->warn(F("WEB"), F("/sockets inflight shed: node: %lu heap: %lu"),
                               (unsigned long)node_id, (unsigned long)free_heap0);
            web.sendText_(request, 503, "text/plain", "WEB busy", set_cookie);
            return;
        }
        if (free_heap0 < kSocketsLowHeapBytes ||
            requestTooFrequent_(g_last_sockets_request_ms, started_ms, kSocketsMinIntervalMs))
        {
            if (web._log)
                web._log->warn(F("WEB"), F("/sockets shed: node: %lu heap: %lu"),
                               (unsigned long)node_id, (unsigned long)free_heap0);
            g_sockets_request_inflight.store(false, std::memory_order_release);
            web.sendText_(request, 503, "text/plain", "WEB busy", set_cookie);
            return;
        }
        String page = FPSTR(kWebInterfaceSocketsHtml);
        page.replace("%NAV%", web.navHtml_());
        const uint8_t page_size = 8u;
        const bool stack_view = web.isStackSocketsView_(node_id);
        const bool groups_available = stack_view ? web.hasGroups_(node_id) : web.hasGroups_();
        if (stack_view)
        {
            web.requestStackSockets_(node_id);
            web.requestStackPorts_(node_id);
        }
        const String page_str = web.paramValueAny_(request, "page");
        uint8_t page_idx = 0;
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (uint8_t)(v - 1);
        }
        uint8_t max_pages = 1;
        uint8_t start = 1;
        uint8_t end = SocketController::kSocketCount;
        if (!stack_view && !groups_available)
        {
            const size_t visible = web.socketsLocalRenderCount_();
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            start = (uint8_t)(page_idx * page_size + 1);
            end = (uint8_t)(start + page_size - 1);
        }
        else if (!stack_view)
        {
            page_idx = 0;
            max_pages = 1;
            start = 1;
            end = SocketController::kSocketCount;
        }
        else
        {
            const size_t visible = web.stackSocketsVisibleCount_(node_id);
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        const size_t extra = 4096u + (size_t)page_size * 900u;
        page.reserve(page.length() + extra);
        page.replace("%SOCKETS%", "<div class=\"tile empty\">Loading...</div>");
        page.replace("%SOCKETS_PAGE_TITLE%", WebUiRu::Sockets::kPageTitle);
        page.replace("%SOCKETS_PAGE_PREV%", WebUiRu::Sockets::kPagePrev);
        page.replace("%SOCKETS_PAGE_LABEL%", WebUiRu::Sockets::kPagePage);
        page.replace("%SOCKETS_PAGE_NEXT%", WebUiRu::Sockets::kPageNext);
        page.replace("%SOCKETS_ON_TEXT%", WebUiRu::Sockets::kText3);
        page.replace("%SOCKETS_OFF_TEXT%", WebUiRu::Sockets::kText4);
        page.replace("%SOCKETS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%SOCKETS_PAGES%", String((unsigned)max_pages));
        if (stack_view)
        {
            const auto *pcache = web._stack_cache ? web._stack_cache->portsCache(node_id) : nullptr;
            (void)pcache;
            page.replace("%DINPUT_JSON%", "[]");
            page.replace("%RELAY_JSON%", "[]");
            page.replace("%DINPUT_USED_JSON%", "[]");
            page.replace("%RELAY_USED_JSON%", "[]");
            page.replace("%SOCKETS_STATUS%", web.stackSocketsStatusText_(node_id));
            page.replace("%SOCKETS_PAGINATION_STYLE%", (groups_available || max_pages <= 1) ? "style=\"display:none\"" : "");
            page.replace("%SOCKETS_SAVE_BTN%", web.webSessionIsAdmin_() ? String("<button class=\"btn\" type=\"submit\">") + WebUiRu::kSave + "</button>" : "");
            page.replace("%SOCKETS_UNIT%", "stack");
            page.replace("%SOCKETS_NODE_ID%", String((unsigned long)node_id));
            String hidden;
            hidden.reserve(96);
            hidden += "<input type=\"hidden\" name=\"unit\" value=\"stack\">";
            hidden += "<input type=\"hidden\" name=\"node\" value=\"";
            hidden += String((unsigned long)node_id);
            hidden += "\">";
            hidden += "<input type=\"hidden\" name=\"page\" value=\"";
            hidden += String((unsigned)(page_idx + 1));
            hidden += "\">";
            page.replace("%SOCKETS_FORM_HIDDEN%", hidden);
        }
        else
        {
            page.replace("%DINPUT_JSON%", "[]");
            page.replace("%RELAY_JSON%", "[]");
            page.replace("%DINPUT_USED_JSON%", "[]");
            page.replace("%RELAY_USED_JSON%", "[]");
            page.replace("%SOCKETS_STATUS%", web._sockets_status);
            page.replace("%SOCKETS_PAGINATION_STYLE%", groups_available ? "style=\"display:none\"" : "");
            page.replace("%SOCKETS_SAVE_BTN%", web.webSessionIsAdmin_() ? String("<button class=\"btn\" type=\"submit\">") + WebUiRu::kSave + "</button>" : "");
            page.replace("%SOCKETS_UNIT%", "local");
            page.replace("%SOCKETS_NODE_ID%", "0");
            page.replace("%SOCKETS_FORM_HIDDEN%", "");
        }
        page.replace("%SOCKETS_DEVICE_SELECT%",
                     web.composeTopFiltersHtml_(web.socketsDeviceSelectHtml_(node_id, stack_view),
                                                groups_available ? web.groupFilterHtml_("sockets-group-filter", stack_view ? node_id : 0u) : String("")));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        g_sockets_request_inflight.store(false, std::memory_order_release);
        web.sendHtmlRaw_(request, page, set_cookie);
    }

void SocketsHandler::handleSocketsList(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Sockets, node_id))
            return;

        const bool stack_view = web.isStackSocketsView_(node_id);
        const bool groups_available = stack_view ? web.hasGroups_(node_id) : web.hasGroups_();
        const uint8_t page_size = 8u;
        const String page_str = web.paramValueAny_(request, "page");
        uint8_t page_idx = 0;
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (uint8_t)(v - 1);
        }

        uint8_t max_pages = 1;
        uint8_t start = 1;
        uint8_t end = SocketController::kSocketCount;
        if (!stack_view && !groups_available)
        {
            const size_t visible = web.socketsLocalRenderCount_();
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            start = (uint8_t)(page_idx * page_size + 1);
            end = (uint8_t)(start + page_size - 1);
        }
        else if (stack_view)
        {
            const size_t visible = web.stackSocketsVisibleCount_(node_id);
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }

        const String html = stack_view
                                ? web.listStackSocketsHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                            groups_available ? SIZE_MAX : page_size)
                                : web.listSocketsHtml_(start, end);
        web.sendText_(request, 200, "text/html; charset=utf-8", html, set_cookie);
    }

void SocketsHandler::handleSocketsSave(WebInterface &web, AsyncWebServerRequest *request, const char *redirect) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Sockets))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackSocketsView_(node_id))
        {
            String back = String("/sockets?unit=stack&node=") + String((unsigned long)node_id);
            const String page_str = web.paramValueAny_(request, "page");
            if (page_str.length())
            {
                const int pv = page_str.toInt();
                if (pv > 0)
                {
                    back += "&page=";
                    back += String((unsigned)pv);
                }
            }
            if (!web._stack_master || !web._stack_cache)
            {
                web._sockets_status = "Stack unavailable";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            const auto *cache = web._stack_cache->socketsCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                web.requestStackSockets_(node_id);
                web._sockets_status = "No data";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            auto *cache_mut = web._stack_cache->socketsCache(node_id);
            bool changed_stack = false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &cfg = cache->items[i];
                const String idx = String((unsigned)cfg.id);
                const String prefix = String("s") + idx + "_";
                const String en_key = prefix + "en";
                const String name_key = prefix + "name";
                const String btn_key = prefix + "btn";
                const String relay_key = prefix + "relay";
                const String group_key = prefix + "group";
                const String action_key = prefix + "action";
                const bool has_any = request->hasParam(en_key, true) ||
                                     request->hasParam(name_key, true) ||
                                     request->hasParam(btn_key, true) ||
                                     request->hasParam(relay_key, true) ||
                                     request->hasParam(group_key, true) ||
                                     request->hasParam(action_key, true);
                if (!has_any)
                    continue;
                if (!web.webAclCanControlItem_(UsersRegistry::AclController::Sockets, cfg.id, node_id))
                {
                    web._sockets_status = String("ACL deny item: ") + idx;
                    web.sendRedirect_(request, back, set_cookie);
                    return;
                }
                const bool enabled = request->hasParam(en_key, true);
                String name = web.paramValue_(request, name_key);
                String btn = web.paramValue_(request, btn_key);
                String relay = web.paramValue_(request, relay_key);
                const uint8_t group_id = web.parseGroupIdParam_(request, group_key);
                String action = web.paramValue_(request, action_key);
                name.trim();
                uint8_t btn_port = SocketController::kInvalidPort;
                uint8_t relay_port = SocketController::kInvalidPort;
                if (!web.parseSocketPort_(btn, btn_port) || !web.parseSocketPort_(relay, relay_port))
                {
                    web._sockets_status = String("Invalid port for socket ") + idx;
                    web.sendRedirect_(request, back, set_cookie);
                    return;
                }
                bool send = false;
                bool desired_state = cfg.state;
                bool set_state = false;
                StaticJsonDocument<256> doc;
                doc["cmd_id"] = web.nextStackCmdId_();
                doc["feature"] = (uint8_t)StackFeature::Sockets;
                doc["action"] = "set";
                JsonArray items = doc["params"]["items"].to<JsonArray>();
                JsonObject o = items.add<JsonObject>();
                o["id"] = (unsigned)cfg.id;
                if (cfg.enabled != enabled)
                {
                    o["enabled"] = enabled;
                    send = true;
                }
                if (strcmp(cfg.name, name.c_str()) != 0)
                {
                    o["name"] = name;
                    send = true;
                }
                if (cfg.button_port != btn_port)
                {
                    o["button"] = btn_port;
                    send = true;
                }
                if (cfg.relay_port != relay_port)
                {
                    o["relay"] = relay_port;
                    send = true;
                }
                if (cfg.group_id != group_id)
                {
                    o["group_id"] = group_id;
                    send = true;
                }
                if (action.length())
                {
                    String act = action;
                    act.toLowerCase();
                    if (act == "on")
                    {
                        o["state"] = true;
                        desired_state = true;
                        set_state = true;
                        send = true;
                    }
                    else if (act == "off")
                    {
                        o["state"] = false;
                        desired_state = false;
                        set_state = true;
                        send = true;
                    }
                    else if (act == "toggle")
                    {
                        o["toggle"] = true;
                        desired_state = !cfg.state;
                        set_state = true;
                        send = true;
                    }
                }
                if (!send)
                    continue;
                char payload[256] = {};
                const size_t len = serializeJson(doc, payload, sizeof(payload));
                if (len == 0 || !web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                           reinterpret_cast<const uint8_t *>(payload), len))
                {
                    web._sockets_status = String("Send failed for socket ") + idx;
                    web.sendRedirect_(request, back, set_cookie);
                    return;
                }
                changed_stack = true;
                if (cache_mut && cache_mut->items)
                {
                    for (size_t k = 0; k < cache_mut->item_count; ++k)
                    {
                        auto &dst = cache_mut->items[k];
                        if (dst.id != cfg.id)
                            continue;
                        dst.enabled = enabled;
                        if (set_state)
                            dst.state = desired_state;
                        dst.button_port = btn_port;
                        dst.relay_port = relay_port;
                        dst.group_id = group_id;
                        const char *src = name.c_str();
                        size_t p = 0;
                        for (; p + 1 < sizeof(dst.name) && src[p]; ++p)
                            dst.name[p] = src[p];
                        dst.name[p] = '\0';
                        break;
                    }
                    cache_mut->updated_ms = millis();
                }
            }
            if (changed_stack)
            {
                web.requestStackSockets_(node_id);
                web.refreshStackPorts_(node_id);
                web._sockets_status = "Updated";
            }
            else
            {
                web._sockets_status = "Saved";
            }
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        SocketController &sockets = web._controllers->sockets();
        auto sockets_guard = sockets.lockGuard();
        bool ok = true;
        bool changed = false;
        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const auto *cfg = sockets.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("s") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String btn_key = prefix + "btn";
            const String relay_key = prefix + "relay";
            const String group_key = prefix + "group";
            const String action_key = prefix + "action";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(btn_key, true) ||
                                 request->hasParam(relay_key, true) ||
                                 request->hasParam(group_key, true) ||
                                 request->hasParam(action_key, true);
            if (!has_any)
                continue;
            if (!web.webAclCanControlItem_(UsersRegistry::AclController::Sockets, cfg->id))
            {
                web._sockets_status = String("ACL deny item: ") + idx;
                web.sendRedirect_(request, redirect ? redirect : "/sockets", set_cookie);
                return;
            }
            const bool enabled = request->hasParam(en_key, true);
            String name = web.paramValue_(request, name_key);
            String btn = web.paramValue_(request, btn_key);
            String relay = web.paramValue_(request, relay_key);
            const uint8_t group_id = web.parseGroupIdParam_(request, group_key);
            String action = web.paramValue_(request, action_key);
            name.trim();
            uint8_t btn_port = SocketController::kInvalidPort;
            uint8_t relay_port = SocketController::kInvalidPort;
            if (!web.parseSocketPort_(btn, btn_port) || !web.parseSocketPort_(relay, relay_port))
            {
                ok = false;
                web._sockets_status = String("Invalid port for socket ") + idx;
                break;
            }
            if (cfg->name != name)
                sockets.setName(cfg->id, name);
            if (cfg->button_port != btn_port)
                sockets.setButtonPort(cfg->id, btn_port);
            if (cfg->relay_port != relay_port)
                sockets.setRelayPort(cfg->id, relay_port);
            if (cfg->group_id != group_id)
                sockets.setGroupId(cfg->id, group_id);
            if (cfg->enabled != enabled)
                sockets.setEnabled(cfg->id, enabled);
            if (action.length())
            {
                String act = action;
                act.toLowerCase();
                if (act == "on")
                {
                    sockets.setRelay(cfg->id, true);
                    changed = true;
                }
                else if (act == "off")
                {
                    sockets.setRelay(cfg->id, false);
                    changed = true;
                }
                else if (act == "toggle")
                {
                    sockets.toggleRelay(cfg->id);
                    changed = true;
                }
            }
        }
        if (ok)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._sockets_status = "Config manager missing";
            }
            else if (!web._configs_manager->save())
            {
                ok = false;
                web._sockets_status = "Save failed";
            }
        }
        if (changed && web._controllers)
            web._controllers->invalidateGpioUsageCache();
        if (ok)
            invalidateLocalSocketsPortsCache_();
        if (changed && web._controllers)
            web._controllers->invalidateGpioUsageCache();
        if (ok)
            web._sockets_status = changed ? "Updated" : "Saved";
        web.sendRedirect_(request, redirect ? redirect : "/sockets", set_cookie);
    }

void SocketsHandler::handleSocketsToggle(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Sockets))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackSocketsView_(node_id))
        {
            web.handleStackSocketsToggle_(request, node_id, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        const String id_str = web.paramValueAny_(request, "id");
        if (!id_str.length())
        {
            String dbg = String("{\"ok\":false,\"err\":\"missing id\"");
            dbg += ",\"id\":\"\"";
            dbg += ",\"enabled\":\"\"";
            dbg += "}";
            web.sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        const uint16_t id = (uint16_t)id_str.toInt();
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Sockets, id, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        SocketController &sockets = web._controllers->sockets();
        auto sockets_guard = sockets.lockGuard();
        if (id == 0 || !sockets.config(id))
        {
            web.sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        const SocketController::SocketConfig *cfg = sockets.config(id);
        const char *name = (cfg && cfg->name.length()) ? cfg->name.c_str() : "-";
        String action = web.paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        bool ok = false;
        bool state = false;
        const bool state_poll = (action == "state");
        if (action == "state")
        {
            ok = sockets.relayStateById(id, state);
        }
        else if (action.length() == 0 || action == "toggle")
        {
            ok = sockets.toggleRelayById(id);
            if (ok)
                ok = sockets.relayStateById(id, state);
        }
        else if (action == "on")
        {
            ok = sockets.setRelayById(id, true);
            if (ok)
                ok = sockets.relayStateById(id, state);
        }
        else if (action == "off")
        {
            ok = sockets.setRelayById(id, false);
            if (ok)
                ok = sockets.relayStateById(id, state);
        }
        if (!ok)
        {
            if (web._log && !state_poll)
                web._log->warn(F("WEB"), F("Sockets toggle failed: id: %u name: %s action: %s"),
                               (unsigned)id, name, action.c_str());
            web.sendText_(request, 400, "text/plain", "Toggle failed", set_cookie);
            return;
        }
        if (web._log && !state_poll)
        {
            if (action == "toggle" || action.length() == 0)
                web._log->info(F("WEB"), F("Sockets toggle ok: id: %u name: %s action: toggle state: %s"),
                               (unsigned)id, name, state ? "on" : "off");
            else
                web._log->info(F("WEB"), F("Sockets toggle ok: id: %u name: %s action: %s"),
                               (unsigned)id, name, action.c_str());
        }
        web.sendText_(request, 200, "text/plain", state ? "on" : "off", set_cookie);
    }

void SocketsHandler::handleSocketsPortsOptions(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Sockets, node_id))
            return;
        const uint32_t started_ms = millis();
        const bool stack_view = web.isStackSocketsView_(node_id);
        const uint32_t free_heap0 = ESP.getFreeHeap();
        if (!stack_view && localSocketsPortsCacheFresh_(started_ms))
        {
            web.sendText_(request, 200, "application/json", g_sockets_local_ports_cache_body, set_cookie);
            return;
        }
        if (g_sockets_ports_request_inflight.exchange(true, std::memory_order_acq_rel))
        {
            if (web._log)
                web._log->warn(F("WEB"), F("/sockets/ports_options inflight shed: node: %lu heap: %lu"),
                               (unsigned long)node_id, (unsigned long)free_heap0);
            if (!stack_view && g_sockets_local_ports_cache_valid && g_sockets_local_ports_cache_body.length())
            {
                web.sendText_(request, 200, "application/json", g_sockets_local_ports_cache_body, set_cookie);
                return;
            }
            web.sendText_(request, 503, "text/plain", "WEB busy", set_cookie);
            return;
        }
        if (free_heap0 < kSocketsLowHeapBytes ||
            requestTooFrequent_(g_last_sockets_ports_request_ms, started_ms, kSocketsPortsMinIntervalMs))
        {
            if (web._log)
                web._log->warn(F("WEB"), F("/sockets/ports_options shed: node: %lu heap: %lu"),
                               (unsigned long)node_id, (unsigned long)free_heap0);
            g_sockets_ports_request_inflight.store(false, std::memory_order_release);
            if (!stack_view && g_sockets_local_ports_cache_valid && g_sockets_local_ports_cache_body.length())
            {
                web.sendText_(request, 200, "application/json", g_sockets_local_ports_cache_body, set_cookie);
                return;
            }
            web.sendText_(request, 503, "text/plain", "WEB busy", set_cookie);
            return;
        }
        if (stack_view)
            web.requestStackPorts_(node_id);

        const String djson = stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::DInput)
                                        : web.socketPortOptionsJson_(PortIO::PinType::DInput);
        const String rjson = stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::Relay)
                                        : web.socketPortOptionsJson_(PortIO::PinType::Relay);
        const String duse = stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::DInput)
                                       : web.globalUsedPortsJson_(PortIO::PinType::DInput);
        const String ruse = stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::Relay)
                                       : web.globalUsedPortsJson_(PortIO::PinType::Relay);
        const auto *pcache = stack_view && web._stack_cache ? web._stack_cache->portsCache(node_id) : nullptr;
        const bool ready = !stack_view || (pcache && pcache->has_data);
        const bool pending = stack_view && pcache && pcache->pending;
        String body;
        body.reserve(djson.length() + rjson.length() + duse.length() + ruse.length() + 128);
        body += "{\"ready\":";
        body += ready ? "true" : "false";
        body += ",\"pending\":";
        body += pending ? "true" : "false";
        body += ",\"dinput\":";
        body += djson;
        body += ",\"relay\":";
        body += rjson;
        body += ",\"dinput_used\":";
        body += duse;
        body += ",\"relay_used\":";
        body += ruse;
        body += "}";
        if (!stack_view)
        {
            g_sockets_local_ports_cache_body = body;
            g_sockets_local_ports_cache_built_ms = started_ms;
            g_sockets_local_ports_cache_valid = true;
        }
        g_sockets_ports_request_inflight.store(false, std::memory_order_release);
        web.sendText_(request, 200, "application/json", body, set_cookie);
    }

void SocketsHandler::handleSocketsEnable(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Sockets))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackSocketsView_(node_id))
        {
            web.handleStackSocketsEnable_(request, node_id, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        const String id_str = web.paramValueAny_(request, "id");
        if (!id_str.length())
        {
            web.sendText_(request, 400, "text/plain", "Missing id", set_cookie);
            return;
        }
        const uint16_t id = (uint16_t)id_str.toInt();
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Sockets, id, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        SocketController &sockets = web._controllers->sockets();
        auto sockets_guard = sockets.lockGuard();
        if (id == 0 || !sockets.config(id))
        {
            String dbg = String("{\"ok\":false,\"err\":\"invalid id\"");
            dbg += ",\"id\":\"" + id_str + "\"";
            dbg += ",\"enabled\":\"\"";
            dbg += "}";
            web.sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        const String enabled_str = web.paramValueAny_(request, "enabled");
        const bool enable = enabled_str == "1" || enabled_str == "true" || enabled_str == "on";
        if (!sockets.setEnabled(id, enable))
        {
            String dbg = String("{\"ok\":false,\"err\":\"enable failed\"");
            dbg += ",\"id\":\"" + id_str + "\"";
            dbg += ",\"enabled\":\"" + enabled_str + "\"";
            dbg += ",\"parsed\":" + String(enable ? "true" : "false");
            dbg += "}";
            web.sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        invalidateLocalSocketsPortsCache_();
        web._controllers->invalidateGpioUsageCache();
        if (!web._configs_manager)
        {
            web._sockets_status = "Config manager missing";
        }
        else if (!web._configs_manager->save())
        {
            web._sockets_status = "Save failed";
        }
        String dbg = String("{\"ok\":true");
        dbg += ",\"id\":\"" + id_str + "\"";
        dbg += ",\"enabled\":\"" + enabled_str + "\"";
        dbg += ",\"parsed\":" + String(enable ? "true" : "false");
        dbg += ",\"result\":\"" + String(enable ? "1" : "0") + "\"";
        dbg += "}";
        web.sendText_(request, 200, "application/json", dbg, set_cookie);
    }
