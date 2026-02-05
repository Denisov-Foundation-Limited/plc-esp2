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

class RfidHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/rfid", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleRfidSave(web, request); });
        server.on("/rfid", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleRfid(web, request); });
    }

    static void handleRfid(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceRfidHtml);
        page.replace("%NAV%", web.navHtml_());
        const bool enabled = web._configs_manager ? web._configs_manager->rfidEnabled() : false;
        page.replace("%RFID_ENABLED_CHECKED%", enabled ? "checked" : "");
        const bool active = enabled && (web.stackRole_() == ConfigsManagerIface::StackRole::Slave);
        page.replace("%RFID_ACTIVE_LABEL%", active ? "" : "");
        page.replace("%RFID_ACTIVE_CLASS%", active ? "on" : "off");
        String last_uid = "";
        if (web._rfid)
        {
            const String uid = web._rfid->lastUidString();
            if (uid.length())
                last_uid = uid;
        }
        page.replace("%RFID_LAST_UID%", last_uid);
        if (!web._configs_manager)
        {
            if (!web._rfid_status.length())
                web._rfid_status = "Config manager missing";
        }
        if (!web._rfid_status.length())
            web._rfid_status = "OK";
        page.replace("%RFID_STATUS%", web._rfid_status);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleRfidSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web._configs_manager)
        {
            web._rfid_status = "Config manager missing";
            web.sendRedirect_(request, "/rfid", set_cookie);
            return;
        }
        const bool enabled = request->hasParam("rfid_enabled", true);
        if (web._configs_manager->rfidEnabled() != enabled)
        {
            web._configs_manager->setRfidEnabled(enabled);
            if (!web._configs_manager->save())
            {
                web._rfid_status = "Save failed";
                web.sendRedirect_(request, "/rfid", set_cookie);
                return;
            }
            web._rfid_status = "Saved";
        }
        else
        {
            web._rfid_status = "No changes";
        }
        web.sendRedirect_(request, "/rfid", set_cookie);
    }
};
