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

#pragma once

class WebInterface;
class AsyncWebServer;
class AsyncWebServerRequest;

class SocketsHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
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

    static void handleSockets(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Sockets, node_id))
            return;
        String page = FPSTR(kWebInterfaceSocketsHtml);
        page.replace("%NAV%", web.navHtml_());
        const uint8_t page_size = 8u;
        const bool stack_view = web.isStackSocketsView_(node_id);
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
        if (!stack_view)
        {
            const size_t visible = web.socketsLocalRenderCount_();
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            start = (uint8_t)(page_idx * page_size + 1);
            end = (uint8_t)(start + page_size - 1);
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
        page.replace("%SOCKETS%", stack_view ? web.listStackSocketsHtml_(node_id, (size_t)page_idx * page_size, page_size)
                                             : web.listSocketsHtml_(start, end));
        page.replace("%SOCKETS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%SOCKETS_PAGES%", String((unsigned)max_pages));
        if (stack_view)
        {
            page.replace("%DINPUT_JSON%", web.stackPortOptionsJson_(node_id, PortIO::PinType::DInput));
            page.replace("%RELAY_JSON%", web.stackPortOptionsJson_(node_id, PortIO::PinType::Relay));
            page.replace("%DINPUT_USED_JSON%", web.stackUsedPortsJson_(node_id, PortIO::PinType::DInput));
            page.replace("%RELAY_USED_JSON%", web.stackUsedPortsJson_(node_id, PortIO::PinType::Relay));
            page.replace("%SOCKETS_STATUS%", web.stackSocketsStatusText_(node_id));
            page.replace("%SOCKETS_PAGINATION_STYLE%", (max_pages > 1) ? "" : "style=\"display:none\"");
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
            page.replace("%DINPUT_JSON%", web.socketPortOptionsJson_(PortIO::PinType::DInput));
            page.replace("%RELAY_JSON%", web.socketPortOptionsJson_(PortIO::PinType::Relay));
            page.replace("%DINPUT_USED_JSON%", web.globalUsedPortsJson_(PortIO::PinType::DInput));
            page.replace("%RELAY_USED_JSON%", web.globalUsedPortsJson_(PortIO::PinType::Relay));
            page.replace("%SOCKETS_STATUS%", web._sockets_status);
            page.replace("%SOCKETS_PAGINATION_STYLE%", "");
            page.replace("%SOCKETS_SAVE_BTN%", web.webSessionIsAdmin_() ? String("<button class=\"btn\" type=\"submit\">") + WebUiRu::kSave + "</button>" : "");
            page.replace("%SOCKETS_UNIT%", "local");
            page.replace("%SOCKETS_NODE_ID%", "0");
            page.replace("%SOCKETS_FORM_HIDDEN%", "");
        }
        page.replace("%SOCKETS_DEVICE_SELECT%", web.socketsDeviceSelectHtml_(node_id, stack_view));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtmlRaw_(request, page, set_cookie);
    }

    static void handleSocketsSave(WebInterface &web, AsyncWebServerRequest *request, const char *redirect)
    {
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
                const String action_key = prefix + "action";
                const bool has_any = request->hasParam(en_key, true) ||
                                     request->hasParam(name_key, true) ||
                                     request->hasParam(btn_key, true) ||
                                     request->hasParam(relay_key, true) ||
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
                web.requestStackPorts_(node_id);
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
            const String action_key = prefix + "action";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(btn_key, true) ||
                                 request->hasParam(relay_key, true) ||
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
        if (ok)
            web._sockets_status = changed ? "Updated" : "Saved";
        web.sendRedirect_(request, redirect ? redirect : "/sockets", set_cookie);
    }

    static void handleSocketsToggle(WebInterface &web, AsyncWebServerRequest *request)
    {
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

    static void handleSocketsEnable(WebInterface &web, AsyncWebServerRequest *request)
    {
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
};

