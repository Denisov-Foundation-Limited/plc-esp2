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

class BusesHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/buses", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleBuses(web, request); });
    }

    static void handleBuses(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceBusesHtml);
        page.reserve(page.length() + 3072);
        page.replace("%NAV%", web.navHtml_());
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = web.isStackBusesView_(node_id);
        if (stack_view)
        {
            String scan = request->hasParam("scan") ? request->getParam("scan")->value() : "";
            scan.toLowerCase();
            const bool run_i2c = (scan == "i2c");
            const bool run_ow = (scan == "ow");
            if (run_i2c)
                web.requestStackI2c_(node_id, true);
            else
                web.requestStackI2c_(node_id, false);
            if (run_ow)
                web.requestStackOw_(node_id, true);
            else
                web.requestStackOw_(node_id, false);
        }
        page.replace("%I2C%", stack_view ? web.listStackI2cHtml_(node_id) : web.listI2cHtml_());
        page.replace("%OW%", stack_view ? web.listStackOwHtml_(node_id) : web.listOwHtml_());
        page.replace("%BUS_DEVICE_SELECT%", web.busesDeviceSelectHtml_(node_id, stack_view));
        page.replace("%BUS_STACK_STATUS%", stack_view ? web.stackBusesStatusText_(node_id) : "");
        page.replace("%BUS_I2C_SCAN_URL%",
                     stack_view ? (String("/buses?node=") + String(node_id) + "&scan=i2c") : String("/buses"));
        page.replace("%BUS_OW_SCAN_URL%",
                     stack_view ? (String("/buses?node=") + String(node_id) + "&scan=ow") : String("/buses"));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }
};
