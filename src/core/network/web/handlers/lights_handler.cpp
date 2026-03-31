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

#include "core/network/web/handlers/lights_handler.hpp"

#include <atomic>

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

#include "core/network/web/web_interface.hpp"

namespace
{
constexpr uint32_t kLightsMinIntervalMs = 250u;
constexpr uint32_t kLightsPortsMinIntervalMs = 400u;
constexpr uint32_t kLightsLowHeapBytes = 20u * 1024u;

std::atomic<uint32_t> g_last_lights_request_ms{0};
std::atomic<uint32_t> g_last_lights_ports_request_ms{0};
std::atomic<bool> g_lights_request_inflight{false};
std::atomic<bool> g_lights_ports_request_inflight{false};

bool requestTooFrequentLights_(std::atomic<uint32_t> &stamp, uint32_t now_ms, uint32_t min_interval_ms)
{
    const uint32_t prev = stamp.load(std::memory_order_relaxed);
    if (prev != 0 && (uint32_t)(now_ms - prev) < min_interval_ms)
        return true;
    stamp.store(now_ms, std::memory_order_relaxed);
    return false;
}

uint32_t requestedStackNodeId_(WebInterface &web, AsyncWebServerRequest *request)
{
    if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
        return 0;
    if (!request)
        return 0;
    String node;
    if (request->hasParam("node_id", true))
        node = request->getParam("node_id", true)->value();
    else if (request->hasParam("node_id", false))
        node = request->getParam("node_id", false)->value();
    else if (request->hasParam("node_id"))
        node = request->getParam("node_id")->value();
    if (node.length() == 0)
    {
        if (request->hasParam("node", true))
            node = request->getParam("node", true)->value();
        else if (request->hasParam("node", false))
            node = request->getParam("node", false)->value();
        else if (request->hasParam("node"))
            node = request->getParam("node")->value();
    }
    if (node.length() == 0)
        return 0;
    char *end = nullptr;
    const unsigned long value = strtoul(node.c_str(), &end, 0);
    if (!end || end == node.c_str())
        return 0;
    return (uint32_t)value;
}
}

void LightsHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/lights/list", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleLightsList(web, request); });
        server.on("/lights/ports_options", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleLightsPortsOptions(web, request); });
        server.on("/lights/toggle", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleLightsToggle(web, request); });
        server.on("/lights/toggle", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleLightsToggle(web, request); });
        server.on("/lights/enable", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleLightsEnable(web, request); });
        server.on("/lights/enable", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleLightsEnable(web, request); });
        server.on("/lights", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleLightsSave(web, request, "/lights"); });
        server.on("/lights", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleLights(web, request); });
    }

void LightsHandler::handleLights(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Lights, node_id))
            return;
        const uint32_t started_ms = millis();
        const uint32_t free_heap0 = ESP.getFreeHeap();
        if (g_lights_request_inflight.exchange(true, std::memory_order_acq_rel))
        {
            web.sendText_(request, 503, "text/plain", "WEB busy", set_cookie);
            return;
        }
        if (free_heap0 < kLightsLowHeapBytes ||
            requestTooFrequentLights_(g_last_lights_request_ms, started_ms, kLightsMinIntervalMs))
        {
            g_lights_request_inflight.store(false, std::memory_order_release);
            web.sendText_(request, 503, "text/plain", "WEB busy", set_cookie);
            return;
        }
        String page = FPSTR(kWebInterfaceLightsHtml);
        const uint8_t page_size = 8u;
        const bool stack_view = web.isStackLightsView_(node_id);
        const bool groups_available = stack_view ? web.hasGroups_(node_id) : web.hasGroups_();
        if (stack_view)
        {
            web.requestStackLights_(node_id);
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
        uint8_t end = SocketController::kLightCount;
        if (!stack_view && !groups_available)
        {
            const size_t visible = web.lightsLocalRenderCount_();
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
            end = SocketController::kLightCount;
        }
        else
        {
            const size_t visible = web.stackLightsVisibleCount_(node_id);
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        const size_t extra = 4096u + (size_t)page_size * 900u;
        page.reserve(page.length() + extra);
        page.replace("%NAV%", web.navHtml_());
        const String initial_html = stack_view
                                        ? web.listStackLightsHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                                   groups_available ? SIZE_MAX : page_size)
                                        : web.listLightsHtml_(start, end);
        page.replace("%LIGHTS%", initial_html);
        page.replace("%LIGHTS_PAGE_TITLE%", WebUiRu::Lights::kPageTitle);
        page.replace("%LIGHTS_PAGE_PREV%", WebUiRu::Lights::kPagePrev);
        page.replace("%LIGHTS_PAGE_LABEL%", WebUiRu::Lights::kPagePage);
        page.replace("%LIGHTS_PAGE_NEXT%", WebUiRu::Lights::kPageNext);
        page.replace("%LIGHTS_ON_TEXT%", WebUiRu::Lights::kText3);
        page.replace("%LIGHTS_OFF_TEXT%", WebUiRu::Lights::kText4);
        page.replace("%LIGHTS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%LIGHTS_PAGES%", String((unsigned)max_pages));
        if (stack_view)
        {
            page.replace("%DINPUT_JSON%", "[]");
            page.replace("%RELAY_JSON%", "[]");
            page.replace("%DINPUT_USED_JSON%", "[]");
            page.replace("%RELAY_USED_JSON%", "[]");
            page.replace("%LIGHTS_STATUS%", web.stackLightsStatusText_(node_id));
            page.replace("%LIGHTS_PAGINATION_STYLE%", (groups_available || max_pages <= 1) ? "style=\"display:none\"" : "");
            page.replace("%LIGHTS_SAVE_BTN%", web.webSessionIsAdmin_() ? String("<button class=\"btn\" type=\"submit\">") + WebUiRu::kSave + "</button>" : String(""));
            page.replace("%LIGHTS_UNIT%", "stack");
            page.replace("%LIGHTS_NODE_ID%", String((unsigned long)node_id));
            String hidden;
            hidden.reserve(96);
            hidden += "<input type=\"hidden\" name=\"unit\" value=\"stack\">";
            hidden += "<input type=\"hidden\" name=\"node\" value=\"";
            hidden += String((unsigned long)node_id);
            hidden += "\">";
            hidden += "<input type=\"hidden\" name=\"page\" value=\"";
            hidden += String((unsigned)(page_idx + 1));
            hidden += "\">";
            page.replace("%LIGHTS_FORM_HIDDEN%", hidden);
        }
        else
        {
            page.replace("%DINPUT_JSON%", "[]");
            page.replace("%RELAY_JSON%", "[]");
            page.replace("%DINPUT_USED_JSON%", "[]");
            page.replace("%RELAY_USED_JSON%", "[]");
            page.replace("%LIGHTS_STATUS%", web._lights_status);
            page.replace("%LIGHTS_PAGINATION_STYLE%", groups_available ? "style=\"display:none\"" : "");
            page.replace("%LIGHTS_SAVE_BTN%", web.webSessionIsAdmin_() ? String("<button class=\"btn\" type=\"submit\">") + WebUiRu::kSave + "</button>" : String(""));
            page.replace("%LIGHTS_UNIT%", "local");
            page.replace("%LIGHTS_NODE_ID%", "0");
            page.replace("%LIGHTS_FORM_HIDDEN%", "");
        }
        page.replace("%LIGHTS_DEVICE_SELECT%",
                     web.composeTopFiltersHtml_(web.lightsDeviceSelectHtml_(node_id, stack_view),
                                                groups_available ? web.groupFilterHtml_("lights-group-filter", stack_view ? node_id : 0u) : String("")));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        g_lights_request_inflight.store(false, std::memory_order_release);
        web.sendHtml_(request, page, set_cookie);
    }

void LightsHandler::handleLightsList(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Lights, node_id))
            return;

        const bool stack_view = web.isStackLightsView_(node_id);
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
        uint8_t end = SocketController::kLightCount;
        if (!stack_view && !groups_available)
        {
            const size_t visible = web.lightsLocalRenderCount_();
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            start = (uint8_t)(page_idx * page_size + 1);
            end = (uint8_t)(start + page_size - 1);
        }
        else if (stack_view)
        {
            const size_t visible = web.stackLightsVisibleCount_(node_id);
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }

        const String html = stack_view
                                ? web.listStackLightsHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                           groups_available ? SIZE_MAX : page_size)
                                : web.listLightsHtml_(start, end);
        web.sendText_(request, 200, "text/html; charset=utf-8", html, set_cookie);
    }

void LightsHandler::handleLightsPortsOptions(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Lights, node_id))
            return;
        const uint32_t started_ms = millis();
        const bool stack_view = web.isStackLightsView_(node_id);
        const uint32_t free_heap0 = ESP.getFreeHeap();
        if (g_lights_ports_request_inflight.exchange(true, std::memory_order_acq_rel))
        {
            web.sendText_(request, 200, "application/json",
                          "{\"ready\":false,\"pending\":true,\"dinput\":[],\"relay\":[],\"dinput_used\":[],\"relay_used\":[]}",
                          set_cookie);
            return;
        }
        if (free_heap0 < kLightsLowHeapBytes ||
            requestTooFrequentLights_(g_last_lights_ports_request_ms, started_ms, kLightsPortsMinIntervalMs))
        {
            g_lights_ports_request_inflight.store(false, std::memory_order_release);
            web.sendText_(request, 200, "application/json",
                          "{\"ready\":false,\"pending\":true,\"dinput\":[],\"relay\":[],\"dinput_used\":[],\"relay_used\":[]}",
                          set_cookie);
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
        const bool ready = !stack_view;
        const bool pending = false;
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
        g_lights_ports_request_inflight.store(false, std::memory_order_release);
        web.sendText_(request, 200, "application/json", body, set_cookie);
    }

void LightsHandler::handleLightsSave(WebInterface &web, AsyncWebServerRequest *request, const char *redirect) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Lights))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackLightsView_(node_id))
        {
            String back = String("/lights?unit=stack&node=") + String((unsigned long)node_id);
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
            if (!web.network())
            {
                web._lights_status = "Stack unavailable";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::CacheState cache{};
            if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                cache.light_count == 0)
            {
                web.requestStackLights_(node_id);
                web.requestStackPorts_(node_id);
                web._lights_status = "No data";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            bool changed_stack = false;
            web.network()->forEachStackLight(node_id, cache.light_count, [&](uint8_t, const StackUnitSnapshot::SocketItem &cfg) {
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
                    return;
                if (!web.webAclCanControlItem_(UsersRegistry::AclController::Lights, cfg.id, node_id))
                {
                    web._lights_status = String("ACL deny item: ") + idx;
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
                    web._lights_status = String("Invalid port for light ") + idx;
                    web.sendRedirect_(request, back, set_cookie);
                    return;
                }
                bool send = false;
                bool desired_state = cfg.state;
                bool set_state = false;
                StaticJsonDocument<256> doc;
                doc["source"] = "localweb";
                if (const auto *u = web.sessionUser_())
                    doc["source_user"] = u->username;
                JsonArray items = doc["items"].to<JsonArray>();
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
                    return;
                if (!web.network()->stackRoute().sendEvent(node_id, "sockets", "set_lights", &doc, StackRouteAdapter::Mode::Json))
                {
                    web._lights_status = String("Send failed for light ") + idx;
                    web.sendRedirect_(request, back, set_cookie);
                    return;
                }
                changed_stack = true;
                (void)desired_state;
                (void)set_state;
            });
            if (changed_stack)
            {
                web.requestStackLights_(node_id);
                web.requestStackIndexState_(node_id);
                web.refreshStackPorts_(node_id);
                web._lights_status = "Updated";
            }
            else
            {
                web._lights_status = "Saved";
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
        for (size_t i = 0; i < SocketController::kLightCount; ++i)
        {
            const auto *cfg = sockets.lightConfigByIndex(i);
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
            if (!web.webAclCanControlItem_(UsersRegistry::AclController::Lights, cfg->id))
            {
                web._lights_status = String("ACL deny item: ") + idx;
                web.sendRedirect_(request, redirect ? redirect : "/lights", set_cookie);
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
                web._lights_status = String("Invalid port for light ") + idx;
                break;
            }
            if (cfg->name != name)
                sockets.setLightName(cfg->id, name);
            if (cfg->button_port != btn_port)
                sockets.setLightButtonPort(cfg->id, btn_port);
            if (cfg->relay_port != relay_port)
                sockets.setLightRelayPort(cfg->id, relay_port);
            if (cfg->group_id != group_id)
                sockets.setLightGroupId(cfg->id, group_id);
            if (cfg->enabled != enabled)
                sockets.setLightEnabled(cfg->id, enabled);
            if (action.length())
            {
                String act = action;
                act.toLowerCase();
                if (act == "on")
                {
                    sockets.setLightRelay(cfg->id, true);
                    changed = true;
                }
                else if (act == "off")
                {
                    sockets.setLightRelay(cfg->id, false);
                    changed = true;
                }
                else if (act == "toggle")
                {
                    sockets.toggleLightRelay(cfg->id);
                    changed = true;
                }
            }
        }
        if (ok)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._lights_status = "Config manager missing";
            }
            else if (!web._configs_manager->save())
            {
                ok = false;
                web._lights_status = "Save failed";
            }
        }
        if (ok)
            web._lights_status = changed ? "Updated" : "Saved";
        web.sendRedirect_(request, redirect ? redirect : "/lights", set_cookie);
    }

void LightsHandler::handleLightsToggle(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Lights))
            return;
        const uint32_t requested_node_id = requestedStackNodeId_(web, request);
        const uint32_t node_id = requested_node_id ? requested_node_id : web.parseStackNodeIdParam_(request);
        if (requested_node_id != 0)
        {
            if (!web.isStackLightsView_(node_id))
            {
                web.sendText_(request, 409, "text/plain", "Stack offline", set_cookie);
                return;
            }
            web.handleStackLightsToggle_(request, node_id, set_cookie);
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
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Lights, id, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        SocketController &sockets = web._controllers->sockets();
        String name = "-";
        {
            auto sockets_guard = sockets.lockGuard(300);
            if (!sockets_guard.locked())
            {
                web.sendText_(request, 503, "text/plain", "Controller busy", set_cookie);
                return;
            }
            if (id == 0 || !sockets.lightConfig(id))
            {
                web.sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
                return;
            }
            const SocketController::LightConfig *cfg = sockets.lightConfig(id);
            if (cfg && cfg->name.length())
                name = cfg->name;
        }
        String action = web.paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        bool ok = false;
        bool state = false;
        bool state_known = false;
        const bool state_poll = (action == "state");
        if (action == "state")
        {
            ok = sockets.lightRelayStateById(id, state, 300);
            state_known = ok;
        }
        else if (action.length() == 0 || action == "toggle")
        {
            ok = sockets.toggleLightRelayById(id, 300);
        }
        else if (action == "on")
        {
            ok = sockets.setLightRelayById(id, true, 300);
        }
        else if (action == "off")
        {
            ok = sockets.setLightRelayById(id, false, 300);
        }
        if (!ok)
        {
            if (web._log && !state_poll)
                web._log->warn(F("WEB"), F("Lights toggle failed: id: %u name: %s action: %s"),
                               (unsigned)id, name.c_str(), action.c_str());
            web.sendText_(request, 400, "text/plain", "Toggle failed", set_cookie);
            return;
        }
        if (!state_poll)
        {
            bool actual = false;
            if (sockets.lightRelayStateById(id, actual, 300))
            {
                state = actual;
                state_known = true;
            }
        }
        if (web._log && !state_poll)
        {
            const bool toggle_action = (action == "toggle" || action.length() == 0);
            if (toggle_action)
                web._log->info(F("WEB"), F("Lights toggle ok: id: %u name: %s action: toggle state: %s"),
                               (unsigned)id, name.c_str(), state ? "on" : "off");
            else
                web._log->info(F("WEB"), F("Lights toggle ok: id: %u name: %s action: %s state: %s"),
                               (unsigned)id, name.c_str(), action.c_str(),
                               state_known ? (state ? "on" : "off") : "?");
        }
        if (state_known)
            web.sendText_(request, 200, "text/plain", state ? "on" : "off", set_cookie);
        else
            web.sendText_(request, 200, "text/plain", "OK", set_cookie);
    }

void LightsHandler::handleLightsEnable(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Lights))
            return;
        const uint32_t requested_node_id = requestedStackNodeId_(web, request);
        const uint32_t node_id = requested_node_id ? requested_node_id : web.parseStackNodeIdParam_(request);
        if (requested_node_id != 0)
        {
            if (!web.isStackLightsView_(node_id))
            {
                web.sendText_(request, 409, "text/plain", "Stack offline", set_cookie);
                return;
            }
            web.handleStackLightsEnable_(request, node_id, set_cookie);
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
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Lights, id, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        SocketController &sockets = web._controllers->sockets();
        auto sockets_guard = sockets.lockGuard();
        if (id == 0 || !sockets.lightConfig(id))
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
        if (!sockets.setLightEnabled(id, enable))
        {
            String dbg = String("{\"ok\":false,\"err\":\"enable failed\"");
            dbg += ",\"id\":\"" + id_str + "\"";
            dbg += ",\"enabled\":\"" + enabled_str + "\"";
            dbg += ",\"parsed\":" + String(enable ? "true" : "false");
            dbg += "}";
            web.sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        if (!web._configs_manager)
        {
            web._lights_status = "Config manager missing";
        }
        else if (!web._configs_manager->save())
        {
            web._lights_status = "Save failed";
        }
        String dbg = String("{\"ok\":true");
        dbg += ",\"id\":\"" + id_str + "\"";
        dbg += ",\"enabled\":\"" + enabled_str + "\"";
        dbg += ",\"parsed\":" + String(enable ? "true" : "false");
        dbg += ",\"result\":\"" + String(enable ? "1" : "0") + "\"";
        dbg += "}";
        web.sendText_(request, 200, "application/json", dbg, set_cookie);
    }
