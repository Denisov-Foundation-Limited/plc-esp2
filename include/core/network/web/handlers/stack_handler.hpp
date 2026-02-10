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
        server.on("/stack/nodes_tbody", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleNodesTbody(web, request); });
        server.on("/stack/slave_link", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleSlaveLinkStatus(web, request); });
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
        page.replace("%STACK_SLAVE_STYLE%", role == ConfigsManagerIface::StackRole::Slave ? "" : "display:none");
        String slave_link_class = "bad";
        String slave_link_text = "Slave link: disconnected";
        if (role == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
        {
            if (web._stack_slave->linkReadyAfterHello())
            {
                slave_link_class = "ok";
                slave_link_text = "Slave link: connected";
            }
            else if (web._stack_slave->nodeConnected())
            {
                slave_link_class = "bad";
                slave_link_text = "Slave link: connected, waiting hello";
            }
        }
        page.replace("%STACK_SLAVE_LINK_CLASS%", slave_link_class);
        page.replace("%STACK_SLAVE_LINK_TEXT%", slave_link_text);
        page.replace("%STACK_MASTER_HOST%", web.stackMasterHost_());
        page.replace("%STACK_FALLBACK_ENABLED_CHECKED%", web.stackFallbackEnabled_() ? "checked" : "");
        page.replace("%STACK_FALLBACK_HOST%", web.stackFallbackHost_());
        page.replace("%STACK_SLAVE_CONTROLLER_CHECKED%", web.stackSlaveController_() ? "checked" : "");
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

    static void handleSlaveLinkStatus(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String cls = "bad";
        String text = "Slave link: disconnected";
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
        {
            if (web._stack_slave->linkReadyAfterHello())
            {
                cls = "ok";
                text = "Slave link: connected";
            }
            else if (web._stack_slave->nodeConnected())
            {
                text = "Slave link: connected, waiting hello";
            }
        }
        String json;
        json.reserve(96);
        json += "{\"class\":\"";
        json += cls;
        json += "\",\"text\":\"";
        json += text;
        json += "\"}";
        web.sendText_(request, 200, "application/json", json, set_cookie);
    }

    static void handleNodesTbody(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master)
        {
            web.sendText_(request, 200, "text/html; charset=utf-8", "", set_cookie);
            return;
        }
        web.sendText_(request, 200, "text/html; charset=utf-8", web.listStackNodesHtml_(), set_cookie);
    }
};
