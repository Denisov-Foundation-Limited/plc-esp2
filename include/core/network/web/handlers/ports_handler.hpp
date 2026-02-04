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
        String page = FPSTR(kWebInterfacePortsHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", web.navHtml_());
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = web.isStackPortsView_(node_id);
        if (stack_view)
        {
            web.requestStackPorts_(node_id);
            web.requestStackExtenders_(node_id);
        }
        page.replace("%PORTS%", stack_view ? web.listStackPortsHtml_(node_id) : web.listPortsHtml_());
        page.replace("%EXTENDERS%", stack_view ? web.listStackExtendersHtml_(node_id) : web.listExtendersHtml_());
        page.replace("%PORTS_DEVICE_SELECT%", web.portsDeviceSelectHtml_(node_id, stack_view));
        page.replace("%PORTS_STACK_STATUS%", stack_view ? web.stackPortsStatusText_(node_id) : "");
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }
};
