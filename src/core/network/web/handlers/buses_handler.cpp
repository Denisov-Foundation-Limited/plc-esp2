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

#include "core/network/web/handlers/buses_handler.hpp"

#include "core/network/web/web_interface.hpp"

void BusesHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/buses", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleBuses(web, request); });
    }

void BusesHandler::handleBuses(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceBusesHtml);
        page.reserve(page.length() + 3072);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%BUSES_PAGE_TITLE%", WebUiRu::BusesPage::kPageTitle);
        page.replace("%BUSES_TITLE%", WebUiRu::BusesPage::kTitle);
        page.replace("%BUSES_SCAN_I2C%", WebUiRu::BusesPage::kScanI2C);
        page.replace("%BUSES_SCAN_OW%", WebUiRu::BusesPage::kScanOw);
        page.replace("%I2C%", web.listI2cHtml_());
        page.replace("%OW%", web.listOwHtml_());
        page.replace("%BUS_DEVICE_SELECT%", "");
        page.replace("%BUS_STACK_STATUS%", "");
        page.replace("%BUS_I2C_SCAN_URL%", String("/buses"));
        page.replace("%BUS_OW_SCAN_URL%", String("/buses"));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }
