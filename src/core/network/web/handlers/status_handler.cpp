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

#include "core/network/web/handlers/status_handler.hpp"

#include "core/network/web/web_interface.hpp"

void StatusHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/status", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleStatus(web, request); });
    }

void StatusHandler::handleStatus(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceStatusHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%STATUS_PAGE_TITLE%", WebUiRu::StatusPage::kPageTitle);
        page.replace("%STATUS_TITLE%", WebUiRu::StatusPage::kTitle);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        page.replace("%STATUS%", web._last_status.length() ? web._last_status : WebUiRu::StatusPage::kNoData);
        web.sendHtml_(request, page, set_cookie);
    }
