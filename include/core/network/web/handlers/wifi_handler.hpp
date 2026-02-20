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

class WifiHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/wifi", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleWifi(web, request); });
    }

    static void handleWifi(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceWifiHtml);
        page.reserve(page.length() + 1536);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%WIFI_MODE%", web._wifi.ap() ? "AP" : "STA");
        page.replace("%WIFI_CUR_SSID%", web._wifi.ap() ? web._wifi.apSsid() : web._wifi.ssid());
        page.replace("%WIFI_IP%", web.wifiIp_());
        if (web._wifi.ap())
        {
            page.replace("%WIFI_STA_ROW%", "");
        }
        else
        {
            String row = "<tr><td>STA</td><td><strong>";
            row += web.wifiStaStatus_();
            row += "</strong></td></tr>";
            page.replace("%WIFI_STA_ROW%", row);
        }
        page.replace("%WIFI_STA_SEL%", web._wifi.ap() ? "" : "selected");
        page.replace("%WIFI_AP_SEL%", web._wifi.ap() ? "selected" : "");
        page.replace("%WIFI_SSID%", web._wifi.ssid());
        page.replace("%WIFI_AP_SSID%", web._wifi.apSsid());
        page.replace("%SAVE_TEXT%", WebUiRu::kSave);
        page.replace("%WIFI_STATUS%", web._wifi_status);
        page.replace("%GSM_STATUS%", web._gsm_status);
        if (!web._gsm)
        {
            page.replace("%GSM_ENABLED_CHECKED%", "");
            page.replace("%GSM_ENABLED_LABEL%", "недоступно");
            page.replace("%GSM_STARTED_LABEL%", "недоступно");
            page.replace("%GSM_IMEI%", "n/a");
            page.replace("%GSM_IMSI%", "n/a");
            page.replace("%GSM_OPERATOR%", "n/a");
            page.replace("%GSM_SIGNAL%", "n/a");
            page.replace("%GSM_REG_STATUS%", "n/a");
            page.replace("%GSM_LAST_ERROR%", "n/a");
            page.replace("%GSM_LAST_URC%", "n/a");
            page.replace("%GSM_LAST_SMS%", "n/a");
            page.replace("%GSM_LAST_CALL%", "n/a");
            page.replace("%GSM_LAST_USSD%", "n/a");
            page.replace("%GSM_HTTP_STATUS%", "n/a");
            page.replace("%GSM_HTTP_LEN%", "n/a");
        }
        else
        {
            const bool available = ActiveBoardProfile::GSM.enabled;
            const bool enabled = available && web._gsm->enabled();
            page.replace("%GSM_ENABLED_CHECKED%", enabled ? "checked" : "");
            page.replace("%GSM_ENABLED_LABEL%", available ? (enabled ? "включен" : "выключен") : "недоступен");
            page.replace("%GSM_STARTED_LABEL%", web._gsm->started() ? "инициализирован" : "не инициализирован");
            page.replace("%GSM_IMEI%", web.safeHtmlValue_(web._gsm->imei(), "n/a"));
            page.replace("%GSM_IMSI%", web.safeHtmlValue_(web._gsm->imsi(), "n/a"));
            page.replace("%GSM_OPERATOR%", web.safeHtmlValue_(web._gsm->operatorName(), "n/a"));
            page.replace("%GSM_SIGNAL%", web.safeHtmlValue_(web._gsm->signalQuality(), "n/a"));
            page.replace("%GSM_REG_STATUS%", web.safeHtmlValue_(web._gsm->regStatus(), "n/a"));
            page.replace("%GSM_LAST_ERROR%", web.safeHtmlValue_(web._gsm->lastError(), "n/a"));
            page.replace("%GSM_LAST_URC%", web.safeHtmlValue_(web._gsm->lastUrc(), "n/a"));
            page.replace("%GSM_LAST_SMS%",
                         web._gsm->lastSmsIndex() ? String(web._gsm->lastSmsIndex()) : String("n/a"));
            page.replace("%GSM_LAST_CALL%", web.safeHtmlValue_(web._gsm->lastCallNumber(), "n/a"));
            page.replace("%GSM_LAST_USSD%", web.safeHtmlValue_(web._gsm->lastUssd(), "n/a"));
            page.replace("%GSM_HTTP_STATUS%",
                         (web._gsm->lastHttpStatus() >= 0) ? String(web._gsm->lastHttpStatus()) : String("n/a"));
            page.replace("%GSM_HTTP_LEN%",
                         (web._gsm->lastHttpLen() >= 0) ? String(web._gsm->lastHttpLen()) : String("n/a"));
        }
        web.sendHtml_(request, page, set_cookie);
    }
};
