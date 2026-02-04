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

class StackHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/stack", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleStack(web, request); });
    }

    static void handleStack(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceStackHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", web.navHtml_());
        const auto role = web.stackRole_();
        page.replace("%STACK_ROLE%", web.stackRoleName_(role));
        page.replace("%STACK_ROLE_MASTER_SEL%", role == ConfigsManagerIface::StackRole::Master ? "selected" : "");
        page.replace("%STACK_ROLE_SLAVE_SEL%", role == ConfigsManagerIface::StackRole::Slave ? "selected" : "");
        page.replace("%STACK_MASTER_HOST%", web.stackMasterHost_());
        page.replace("%STACK_API_KEY%", web.stackApiKey_());
        page.replace("%STACK_STATUS%", web._stack_status);
        if (role == ConfigsManagerIface::StackRole::Master)
        {
            String self = String("<p class=\"status\">Текущий контроллер: <strong>") + web.deviceName_() +
                          "</strong> | IP: <strong>" + web.wifiIp_() + "</strong></p>";
            page.replace("%STACK_SELF_BLOCK%", self);
            page.replace("%STACK_NODES_BLOCK%", web.stackNodesBlockHtml_());
        }
        else
        {
            page.replace("%STACK_SELF_BLOCK%", "");
            page.replace("%STACK_NODES_BLOCK%", "");
        }
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }
};
