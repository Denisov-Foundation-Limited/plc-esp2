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

class ClientsHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/clients", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleClientsSave(web, request); });
        server.on("/clients", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleClients(web, request); });
    }

    static void handleClients(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceClientsHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", web.navHtml_());

        const String page_str = web.paramValueAny_(request, "page");
        uint8_t page_idx = 0;
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (uint8_t)(v - 1);
        }
        uint8_t pages = 1;
        page.replace("%CLIENTS_TILES%", web.listClientsTilesHtml_(page_idx, pages));
        page.replace("%CLIENTS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%CLIENTS_PAGES%", String((unsigned)(pages ? pages : 1)));
        if (!web._clients_status.length())
            web._clients_status = "OK";
        page.replace("%CLIENTS_STATUS%", web._clients_status);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleClientsSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web._configs_manager)
        {
            web._clients_status = "Config manager missing";
            web.sendRedirect_(request, "/clients", set_cookie);
            return;
        }
        const String client = web.paramValue_(request, "client");
        bool changed = false;
        if (client.length() == 0 || client == "rfid")
        {
            const bool enabled = request->hasParam("rfid_enabled", true);
            if (web._configs_manager->rfidEnabled() != enabled)
            {
                web._configs_manager->setRfidEnabled(enabled);
                changed = true;
            }
        }
        if (client == "ring")
        {
            const bool enabled = request->hasParam("ring_client_enabled", true);
            if (web._configs_manager->ringClientEnabled() != enabled)
            {
                web._configs_manager->setRingClientEnabled(enabled);
                changed = true;
            }
        }
        bool ok = true;
        if (changed)
        {
            if (!web._configs_manager->save())
            {
                ok = false;
                web._clients_status = "Save failed";
            }
        }
        if (ok)
            web._clients_status = changed ? "Updated" : "No changes";
        web.sendRedirect_(request, "/clients", set_cookie);
    }
};
