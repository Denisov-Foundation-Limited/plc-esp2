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

class LightsHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
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

    static void handleLights(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceLightsHtml);
        const uint8_t page_size = 8u;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = web.isStackLightsView_(node_id);
        if (stack_view)
            web.requestStackLights_(node_id);
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
        if (!stack_view)
        {
            max_pages = (uint8_t)((SocketController::kLightCount + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            start = (uint8_t)(page_idx * page_size + 1);
            end = (uint8_t)(start + page_size - 1);
        }
        else
        {
            page_idx = 0;
        }
        const size_t extra = 4096u + (size_t)page_size * 900u;
        page.reserve(page.length() + extra);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%LIGHTS%", stack_view ? web.listStackLightsHtml_(node_id) : web.listLightsHtml_(start, end));
        page.replace("%LIGHTS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%LIGHTS_PAGES%", String((unsigned)max_pages));
        if (stack_view)
        {
            page.replace("%DINPUT_JSON%", "[]");
            page.replace("%RELAY_JSON%", "[]");
            page.replace("%DINPUT_USED_JSON%", "[]");
            page.replace("%RELAY_USED_JSON%", "[]");
            page.replace("%LIGHTS_STATUS%", web.stackLightsStatusText_(node_id));
            page.replace("%LIGHTS_PAGINATION_STYLE%", "style=\"display:none\"");
            page.replace("%LIGHTS_SAVE_BTN%", "");
            page.replace("%LIGHTS_UNIT%", "stack");
            page.replace("%LIGHTS_NODE_ID%", String((unsigned long)node_id));
        }
        else
        {
            page.replace("%DINPUT_JSON%", web.socketPortOptionsJson_(PortIO::PinType::DInput));
            page.replace("%RELAY_JSON%", web.socketPortOptionsJson_(PortIO::PinType::Relay));
            page.replace("%DINPUT_USED_JSON%", web.globalUsedPortsJson_(PortIO::PinType::DInput));
            page.replace("%RELAY_USED_JSON%", web.globalUsedPortsJson_(PortIO::PinType::Relay));
            page.replace("%LIGHTS_STATUS%", web._lights_status);
            page.replace("%LIGHTS_PAGINATION_STYLE%", "");
            page.replace("%LIGHTS_SAVE_BTN%", "<button class=\"btn\" type=\"submit\">Сохранить</button>");
            page.replace("%LIGHTS_UNIT%", "local");
            page.replace("%LIGHTS_NODE_ID%", "0");
        }
        page.replace("%LIGHTS_DEVICE_SELECT%", web.lightsDeviceSelectHtml_(node_id, stack_view));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleLightsSave(WebInterface &web, AsyncWebServerRequest *request, const char *redirect)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackLightsView_(node_id))
        {
            web._lights_status = "Доступно только на локальном устройстве";
            web.sendRedirect_(request, redirect ? redirect : "/lights", set_cookie);
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
            const String action_key = prefix + "action";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(btn_key, true) ||
                                 request->hasParam(relay_key, true) ||
                                 request->hasParam(action_key, true);
            if (!has_any)
                continue;
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
                web._lights_status = String("Invalid port for light ") + idx;
                break;
            }
            if (cfg->name != name)
                sockets.setLightName(cfg->id, name);
            if (cfg->button_port != btn_port)
                sockets.setLightButtonPort(cfg->id, btn_port);
            if (cfg->relay_port != relay_port)
                sockets.setLightRelayPort(cfg->id, relay_port);
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

    static void handleLightsToggle(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackLightsView_(node_id))
        {
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
        SocketController &sockets = web._controllers->sockets();
        if (id == 0 || !sockets.lightConfig(id))
        {
            web.sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        String action = web.paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        bool ok = false;
        bool state = false;
        if (action == "state")
        {
            ok = sockets.lightRelayStateById(id, state);
        }
        else if (action.length() == 0 || action == "toggle")
        {
            ok = sockets.toggleLightRelayById(id);
            if (ok)
                ok = sockets.lightRelayStateById(id, state);
        }
        else if (action == "on")
        {
            ok = sockets.setLightRelayById(id, true);
            if (ok)
                ok = sockets.lightRelayStateById(id, state);
        }
        else if (action == "off")
        {
            ok = sockets.setLightRelayById(id, false);
            if (ok)
                ok = sockets.lightRelayStateById(id, state);
        }
        if (!ok)
        {
            web.sendText_(request, 400, "text/plain", "Toggle failed", set_cookie);
            return;
        }
        web.sendText_(request, 200, "text/plain", state ? "on" : "off", set_cookie);
    }

    static void handleLightsEnable(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackLightsView_(node_id))
        {
            web.sendText_(request, 400, "text/plain", "Read-only", set_cookie);
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
        SocketController &sockets = web._controllers->sockets();
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
};
