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

class CloudHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/cloud", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleCloudSave(web, request); });
        server.on("/cloud", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleCloud(web, request); });
    }

    static void handleCloud(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceCloudHtml);
        page.reserve(page.length() + 1536);
        page.replace("%NAV%", web.navHtml_());
        const bool connected = web.cloudConnected_();
        page.replace("%CLOUD_CONNECTED_CLASS%", connected ? "ok" : "bad");
        page.replace("%CLOUD_CONNECTED_TEXT%", connected ? "Подключен" : "Отключен");
        page.replace("%CLOUD_ENABLED_CHECKED%", web.cloudEnabled_() ? "checked" : "");
        page.replace("%CLOUD_HOST%", web.cloudHost_());
        page.replace("%CLOUD_PORT%", web.cloudPort_() ? String(web.cloudPort_()) : String(""));
        page.replace("%CLOUD_PATH%", web.cloudPath_());
        page.replace("%CLOUD_SSL_CHECKED%", web.cloudUseSsl_() ? "checked" : "");
        page.replace("%CLOUD_RECONNECT_MS%", String(web.cloudReconnectMs_()));
        page.replace("%CLOUD_EVENT_MS%", String(web.cloudEventMs_()));
        page.replace("%CLOUD_API_KEY%", web.cloudApiKey_());
        page.replace("%CLOUD_FW_VERSION%", web.cloudFwVersion_());
        page.replace("%CLOUD_DEVICE_ID%", String(web.cloudDeviceId_()));
        page.replace("%CLOUD_STATUS%", web._cloud_status);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleCloudSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web._configs_manager)
        {
            web._cloud_status = "Менеджер конфигурации недоступен";
            web.sendRedirect_(request, "/cloud", set_cookie);
            return;
        }

        bool changed = false;
        const bool enabled = request->hasParam("cloud_enabled", true);
        if (enabled != web._configs_manager->cloudEnabled())
        {
            web._configs_manager->setCloudEnabled(enabled);
            changed = true;
        }

        String host = request->hasParam("host", true) ? request->getParam("host", true)->value() : String("");
        host.trim();
        if (host != web._configs_manager->cloudHost())
        {
            web._configs_manager->setCloudHost(host);
            changed = true;
        }

        String port_str = request->hasParam("port", true) ? request->getParam("port", true)->value() : String("");
        port_str.trim();
        const uint16_t port = port_str.length() ? (uint16_t)strtoul(port_str.c_str(), nullptr, 10) : 0;
        if (port != web._configs_manager->cloudPort())
        {
            web._configs_manager->setCloudPort(port);
            changed = true;
        }

        String path = request->hasParam("path", true) ? request->getParam("path", true)->value() : String("");
        path.trim();
        if (path.length() == 0)
            path = "/";
        if (path != web._configs_manager->cloudPath())
        {
            web._configs_manager->setCloudPath(path);
            changed = true;
        }

        const bool use_ssl = request->hasParam("ssl", true);
        if (use_ssl != web._configs_manager->cloudUseSsl())
        {
            web._configs_manager->setCloudUseSsl(use_ssl);
            changed = true;
        }

        String reconnect_str = request->hasParam("reconnect_ms", true)
                                   ? request->getParam("reconnect_ms", true)->value()
                                   : String("");
        reconnect_str.trim();
        const uint32_t reconnect_ms = reconnect_str.length()
                                          ? (uint32_t)strtoul(reconnect_str.c_str(), nullptr, 10)
                                          : web._configs_manager->cloudReconnectMs();
        if (reconnect_ms != web._configs_manager->cloudReconnectMs())
        {
            web._configs_manager->setCloudReconnectMs(reconnect_ms);
            changed = true;
        }

        String event_str = request->hasParam("event_ms", true) ? request->getParam("event_ms", true)->value() : String("");
        event_str.trim();
        const uint32_t event_ms = event_str.length()
                                      ? (uint32_t)strtoul(event_str.c_str(), nullptr, 10)
                                      : web._configs_manager->cloudEventIntervalMs();
        if (event_ms != web._configs_manager->cloudEventIntervalMs())
        {
            web._configs_manager->setCloudEventIntervalMs(event_ms);
            changed = true;
        }

        String api_key = request->hasParam("api_key", true) ? request->getParam("api_key", true)->value() : String("");
        api_key.trim();
        if (api_key != web._configs_manager->cloudApiKey())
        {
            web._configs_manager->setCloudApiKey(api_key);
            changed = true;
        }

        bool save_ok = true;
        if (changed)
            save_ok = web.saveWifiConfig_();

        if (!changed)
            web._cloud_status = "Без изменений";
        else if (!save_ok)
            web._cloud_status = "Ошибка сохранения";
        else
            web._cloud_status = "Сохранено";

        web.sendRedirect_(request, "/cloud", set_cookie);
    }
};
