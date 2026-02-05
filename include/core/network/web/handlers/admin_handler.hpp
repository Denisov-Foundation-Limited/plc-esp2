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

class AdminHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/admin", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleAdmin(web, request); });
    }

    static void handleAdmin(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (web._cli_auth && web._cli_auth->adminPasswordSet())
        {
            if (!web.checkAuth_(request, &set_cookie))
                return;
        }
        String page = FPSTR(kWebInterfaceAdminHtml);
        page.reserve(page.length() + 768);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%ADMIN_STATUS%", (web._cli_auth && web._cli_auth->adminPasswordSet()) ? "установлен" : "не установлен");
        String rtc_date = web.rtcDateStr_();
        String rtc_time = web.rtcTimeOnlyStr_();
        String rtc_date_val = (rtc_date == "n/a") ? "" : rtc_date;
        String rtc_time_val = (rtc_time == "n/a") ? "" : rtc_time;
        page.replace("%RTC_DATE%", rtc_date);
        page.replace("%RTC_TIME%", rtc_time);
        page.replace("%RTC_DATE_VAL%", rtc_date_val);
        page.replace("%RTC_TIME_VAL%", rtc_time_val);
        web.sendHtml_(request, page, set_cookie);
    }
};
