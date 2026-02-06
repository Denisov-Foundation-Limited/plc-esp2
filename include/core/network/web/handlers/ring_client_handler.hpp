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

#include "clients/ring_client.hpp"

class WebInterface;
class AsyncWebServer;
class AsyncWebServerRequest;

class RingClientHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/client/ring", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleRingClientSave(web, request); });
        server.on("/client/ring", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleRingClient(web, request); });
    }

    static void handleRingClient(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceClientRingHtml);
        page.replace("%NAV%", web.navHtml_());
        const bool enabled = web._configs_manager ? web._configs_manager->ringClientEnabled() : false;
        page.replace("%RING_CLIENT_ENABLED_CHECKED%", enabled ? "checked" : "");
        const bool active = enabled && (web.stackRole_() == ConfigsManagerIface::StackRole::Slave);
        page.replace("%RING_CLIENT_ACTIVE_LABEL%", active ? "" : "");
        page.replace("%RING_CLIENT_ACTIVE_CLASS%", active ? "on" : "off");
        if (web._configs_manager)
        {
            const uint8_t port = web._configs_manager->ringClientButtonPort();
            if (port != RingClient::kInvalidPort)
                page.replace("%RING_CLIENT_BUTTON_SELECTED%", String(port));
            else
                page.replace("%RING_CLIENT_BUTTON_SELECTED%", "");
        }
        else
        {
            page.replace("%RING_CLIENT_BUTTON_SELECTED%", "");
        }
        page.replace("%RING_CLIENT_BUTTON_JSON%", web.socketPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%RING_CLIENT_DINPUT_USED_JSON%", web.globalUsedPortsJson_(PortIO::PinType::DInput));
        if (!web._configs_manager)
        {
            if (!web._ring_client_status.length())
                web._ring_client_status = "Config manager missing";
        }
        if (!web._ring_client_status.length())
            web._ring_client_status = "OK";
        page.replace("%RING_CLIENT_STATUS%", web._ring_client_status);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleRingClientSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web._configs_manager)
        {
            web._ring_client_status = "Config manager missing";
            web.sendRedirect_(request, "/client/ring", set_cookie);
            return;
        }
        const bool enabled = request->hasParam("ring_client_enabled", true);
        const String button_str = web.paramValue_(request, "ring_client_button");
        uint8_t button_port = RingClient::kInvalidPort;
        if (!web.parseSocketPort_(button_str, button_port))
        {
            web._ring_client_status = "Invalid button port";
            web.sendRedirect_(request, "/client/ring", set_cookie);
            return;
        }
        bool changed = false;
        if (web._configs_manager->ringClientEnabled() != enabled)
        {
            web._configs_manager->setRingClientEnabled(enabled);
            changed = true;
        }
        if (web._configs_manager->ringClientButtonPort() != button_port)
        {
            web._configs_manager->setRingClientButtonPort(button_port);
            changed = true;
        }
        bool ok = true;
        if (changed)
        {
            if (!web._configs_manager->save())
            {
                ok = false;
                web._ring_client_status = "Save failed";
            }
        }
        if (ok)
            web._ring_client_status = changed ? "Saved" : "No changes";
        web.sendRedirect_(request, "/client/ring", set_cookie);
    }
};
