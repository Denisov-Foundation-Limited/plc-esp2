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

class StatusHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/status", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleStatus(web, request); });
    }

    static void handleStatus(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceStatusHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        page.replace("%STATUS%", web._last_status.length() ? web._last_status : "No data");
        web.sendHtml_(request, page, set_cookie);
    }
};
