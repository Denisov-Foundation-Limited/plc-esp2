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

class PortsHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/ports", HTTP_GET, [&web](AsyncWebServerRequest *request) { handlePorts(web, request); });
    }

    static void handlePorts(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfacePortsHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%PORTS_PAGE_TITLE%", WebUiRu::PortsPage::kPageTitle);
        page.replace("%PORTS_TITLE%", WebUiRu::PortsPage::kTitle);
        page.replace("%PORTS_EXTENDERS%", WebUiRu::PortsPage::kExtenders);
        page.replace("%PORTS_SECTION_PORTS%", WebUiRu::PortsPage::kPorts);
        page.replace("%PORTS_COL_BUS%", WebUiRu::PortsPage::kBus);
        page.replace("%PORTS_COL_ADDR%", WebUiRu::PortsPage::kAddress);
        page.replace("%PORTS_COL_TYPE%", WebUiRu::PortsPage::kType);
        page.replace("%PORTS_COL_BACKEND%", WebUiRu::PortsPage::kBackend);
        page.replace("%PORTS_COL_LOCAL%", WebUiRu::PortsPage::kLocalShort);
        page.replace("%PORTS_COL_TYPE2%", WebUiRu::PortsPage::kType);
        page.replace("%PORTS_COL_CTRL%", WebUiRu::PortsPage::kControllerShort);
        page.replace("%PORTS_COL_DEV%", WebUiRu::PortsPage::kDeviceShort);
        page.replace("%PORTS_COL_PIN%", WebUiRu::PortsPage::kPin);
        page.replace("%PORTS%", web.listPortsHtml_());
        page.replace("%EXTENDERS%", web.listExtendersHtml_());
        page.replace("%PORTS_DEVICE_SELECT%", "");
        page.replace("%PORTS_STACK_STATUS%", "");
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }
};
