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

class RingHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/ring/trigger", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleRingTrigger(web, request); });
        server.on("/ring", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleRingSave(web, request); });
        server.on("/ring", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleRing(web, request); });
    }

    static void handleRing(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Ring, node_id))
            return;
        if (!web.webAclCanViewItem_(UsersRegistry::AclController::Ring, 1, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        const bool stack_view = web.isStackRingView_(node_id);
        String page = FPSTR(kWebInterfaceRingHtml);
        page.reserve(page.length() + 1024);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%RING_DEVICE_SELECT%", web.ringDeviceSelectHtml_(node_id, stack_view));
        if (!web._controllers)
        {
            page.replace("%RING_STATUS%", "Контроллеры недоступны");
            page.replace("%RING_ENABLED_CHECKED%", "");
            page.replace("%RING_ENABLED_LABEL%", "");
            page.replace("%RING_BUTTON_SELECTED%", "");
            page.replace("%RING_RELAY_SELECTED%", "");
            page.replace("%RING_DINPUT_JSON%", "[]");
            page.replace("%RING_RELAY_JSON%", "[]");
            page.replace("%RING_DINPUT_USED_JSON%", "[]");
            page.replace("%RING_RELAY_USED_JSON%", "[]");
            page.replace("%RING_FORM_DISABLED%", "disabled");
            page.replace("%RING_SAVE_DISABLED%", "disabled");
            web.sendHtml_(request, page, set_cookie);
            return;
        }
        const RingController &ring = web._controllers->ring();
        const auto &cfg = ring.config();
        const String status = stack_view ? String("Стек: управление слейвом") : web._ring_status;
        page.replace("%RING_STATUS%", status);
        page.replace("%SAVE_TEXT%", WebUiRu::kSave);
        if (stack_view)
        {
            page.replace("%RING_ENABLED_CHECKED%", "");
            page.replace("%RING_ENABLED_LABEL%", "");
            page.replace("%RING_BUTTON_SELECTED%", "");
            page.replace("%RING_RELAY_SELECTED%", "");
            page.replace("%RING_DINPUT_JSON%", "[]");
            page.replace("%RING_RELAY_JSON%", "[]");
            page.replace("%RING_DINPUT_USED_JSON%", "[]");
            page.replace("%RING_RELAY_USED_JSON%", "[]");
            page.replace("%RING_FORM_DISABLED%", "disabled");
            page.replace("%RING_SAVE_DISABLED%", "disabled");
        }
        else
        {
            page.replace("%RING_ENABLED_CHECKED%", "");
            page.replace("%RING_ENABLED_LABEL%", "");
            page.replace("%RING_BUTTON_SELECTED%",
                         cfg.button_port != RingController::kInvalidPort ? String(cfg.button_port) : String());
            page.replace("%RING_RELAY_SELECTED%",
                         cfg.relay_port != RingController::kInvalidPort ? String(cfg.relay_port) : String());
            page.replace("%RING_DINPUT_JSON%", web.socketPortOptionsJson_(PortIO::PinType::DInput));
            page.replace("%RING_RELAY_JSON%", web.socketPortOptionsJson_(PortIO::PinType::Relay));
            page.replace("%RING_DINPUT_USED_JSON%", web.globalUsedPortsJson_(PortIO::PinType::DInput));
            page.replace("%RING_RELAY_USED_JSON%", web.globalUsedPortsJson_(PortIO::PinType::Relay));
            const bool can_edit = web.webSessionIsAdmin_();
            page.replace("%RING_FORM_DISABLED%", can_edit ? "" : "disabled");
            page.replace("%RING_SAVE_DISABLED%", can_edit ? "" : "disabled");
        }
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleRingSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Ring))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = web.isStackRingView_(node_id);
        if (!request->hasParam("ring_save", true))
        {
            if (stack_view)
            {
                const String path = String("/ring?node=") + String((unsigned long)node_id) + "&unit=stack";
                web.sendRedirect_(request, path.c_str(), set_cookie);
            }
            else
            {
                web.sendRedirect_(request, "/ring", set_cookie);
            }
            return;
        }
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Ring, 1, node_id))
        {
            web._ring_status = "ACL deny";
            if (stack_view)
            {
                const String path = String("/ring?node=") + String((unsigned long)node_id) + "&unit=stack";
                web.sendRedirect_(request, path.c_str(), set_cookie);
            }
            else
            {
                web.sendRedirect_(request, "/ring", set_cookie);
            }
            return;
        }
        if (stack_view)
        {
            web._ring_status = "Настройки доступны только локально";
            const String path = String("/ring?node=") + String((unsigned long)node_id) + "&unit=stack";
            web.sendRedirect_(request, path.c_str(), set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        RingController &ring = web._controllers->ring();
        bool changed = false;
        if (request->hasParam("ring_enabled", true))
        {
            const String enabled_str = request->getParam("ring_enabled", true)->value();
            const bool enabled = enabled_str.length() == 0 || enabled_str == "1" || enabled_str == "true" ||
                                 enabled_str == "on";
            if (ring.controllerEnabled() != enabled)
            {
                ring.setControllerEnabled(enabled);
                changed = true;
            }
        }
        const String button_str = web.paramValue_(request, "ring_button");
        const String relay_str = web.paramValue_(request, "ring_relay");
        uint8_t button_port = RingController::kInvalidPort;
        uint8_t relay_port = RingController::kInvalidPort;
        if (!web.parseSocketPort_(button_str, button_port))
        {
            web._ring_status = "Неверный порт кнопки";
            web.sendRedirect_(request, "/ring", set_cookie);
            return;
        }
        if (!web.parseSocketPort_(relay_str, relay_port))
        {
            web._ring_status = "Неверный порт реле";
            web.sendRedirect_(request, "/ring", set_cookie);
            return;
        }
        if (ring.setButtonPort(button_port))
            changed = true;
        if (ring.setRelayPort(relay_port))
            changed = true;
        bool ok = true;
        if (changed)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._ring_status = "Config manager missing";
            }
            else if (!web._configs_manager->save())
            {
                ok = false;
                web._ring_status = "Save failed";
            }
        }
        if (ok)
            web._ring_status = changed ? "Сохранено" : "Нет изменений";
        web.sendRedirect_(request, "/ring", set_cookie);
    }

    static void handleRingTrigger(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Ring, 1, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Ring, node_id))
            return;
        const bool stack_view = web.isStackRingView_(node_id);
        if (!web._controllers)
        {
            if (request->hasParam("state", true))
            {
                web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
                return;
            }
            web._ring_status = "Контроллеры недоступны";
            web.sendRedirect_(request, "/ring", set_cookie);
            return;
        }
        if (stack_view)
        {
            const bool has_state = request->hasParam("state", true);
            if (has_state)
            {
                const String state = request->getParam("state", true)->value();
                const bool on = state == "1" || state == "true" || state == "on";
                bool ok = true;
                if (!web.sendStackRingCmdAll_(true, on))
                    ok = false;
                if (ok && on)
                    web.notifyRingPress_(true, node_id);
                web.sendText_(request, ok ? 200 : 400, "text/plain", ok ? (on ? "on" : "off") : "Failed",
                              set_cookie);
                return;
            }
            web.sendText_(request, 400, "text/plain", "Missing state", set_cookie);
            return;
        }
        RingController &ring = web._controllers->ring();
        const auto &cfg = ring.config();
        const bool has_state = request->hasParam("state", true);
        if (has_state)
        {
            const String state = request->getParam("state", true)->value();
            const bool on = state == "1" || state == "true" || state == "on";
            if (!cfg.enabled)
            {
                web.sendText_(request, 400, "text/plain", "Ring disabled", set_cookie);
                return;
            }
            if (cfg.relay_port == RingController::kInvalidPort)
            {
                web.sendText_(request, 400, "text/plain", "Relay missing", set_cookie);
                return;
            }
            if (!ring.setHoldRelayWithSource(on, RingController::Source::Web))
            {
                web.sendText_(request, 400, "text/plain", "Failed", set_cookie);
                return;
            }
            web.sendText_(request, 200, "text/plain", on ? "on" : "off", set_cookie);
            return;
        }
        web.sendText_(request, 400, "text/plain", "Missing state", set_cookie);
    }
};
