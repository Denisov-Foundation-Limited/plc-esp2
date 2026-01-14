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

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>

#if defined(ESP32)
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#endif

#include "boards/board_profile.hpp"
#include "core/cli/cli_console.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/web/web_interface_page.hpp"
#include "core/network/web/web_interface_status.hpp"
#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"

class WebInterface
{
public:
    WebInterface(AsyncWebServer &server, const CliConsole &cli, WifiManager &wifi, Configs &configs, Logger &logs)
        : _server(server), _cli_auth(&cli), _wifi(wifi), _configs(configs), _log(&logs)
    {
    }

    bool begin(bool format_on_fail = false)
    {
        return LittleFS.begin(format_on_fail);
    }

    void setAuth(const String &user, const String &pass)
    {
        _cli_auth = nullptr;
        _auth_user = user;
        _auth_pass = pass;
        _auth_enabled = (_auth_user.length() > 0);
    }

    void setMaxUploadBytes(size_t bytes) { _max_upload = bytes; }

    void setAllowedExtensions(const String &exts_csv)
    {
        _allowed_exts = exts_csv;
        _allowed_exts.toLowerCase();
    }

    void setConfigsManager(ConfigsManagerIface &mgr) { _configs_manager = &mgr; }

    void registerRoutes()
    {
        _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { handleIndex_(request); });
        _server.on(
            "/upload", HTTP_POST,
            [this](AsyncWebServerRequest *request) { handleUploadDone_(request); },
            [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                   bool final) { handleUpload_(request, filename, index, data, len, final); });
        _server.on(
            "/ota", HTTP_POST,
            [this](AsyncWebServerRequest *request) { handleOtaDone_(request); },
            [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                   bool final) { handleOta_(request, filename, index, data, len, final); });
        _server.on("/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) { handleWifiSave_(request); });
        _server.on("/files", HTTP_GET, [this](AsyncWebServerRequest *request) { handleFileDownload_(request); });
        _server.on("/delete", HTTP_GET, [this](AsyncWebServerRequest *request) { handleDelete_(request); });
        _server.on("/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStatus_(request); });
        _server.onNotFound([this](AsyncWebServerRequest *request) {
            const String uri = request->url();
            if (uri.startsWith("/files/"))
            {
                handleFileDownload_(request);
                return;
            }
            request->send(404, "text/plain", String("Not found: ") + uri);
        });
    }

private:
    void handleIndex_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET / (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceIndexHtml);
        page.replace("%WIFI_MODE%", _wifi.ap() ? "AP" : "STA");
        page.replace("%WIFI_CUR_SSID%", _wifi.ap() ? _wifi.apSsid() : _wifi.ssid());
        page.replace("%WIFI_IP%", wifiIp_());
        page.replace("%WIFI_STA_SEG%", wifiStaSegment_());
        page.replace("%WIFI_STA_SEL%", _wifi.ap() ? "" : "selected");
        page.replace("%WIFI_AP_SEL%", _wifi.ap() ? "selected" : "");
        page.replace("%WIFI_SSID%", _wifi.ssid());
        page.replace("%WIFI_AP_SSID%", _wifi.apSsid());
        page.replace("%WIFI_STATUS%", _wifi_status);
        page.replace("%FILES%", listFilesHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        request->send(200, "text/html", page);
    }

    String listFilesHtml_()
    {
        String items;
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file)
        {
            String name = file.name();
            String path = name;
            if (!path.startsWith("/"))
                path = "/" + path;
            items += "<tr><td><a href=\"/files";
            items += path;
            items += "\">";
            items += name;
            items += "</a></td><td class=\"right\">";
            items += String((unsigned)file.size());
            items += " B</td><td class=\"right\"><a class=\"del\" onclick=\"return confirm('Удалить файл ";
            items += name;
            items += "?')\" href=\"/delete?path=";
            items += path;
            items += "\">Удалить</a></td></tr>";
            file = root.openNextFile();
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"3\" style=\"color:#94a3b8\">Файлы отсутствуют</td></tr>";
        return items;
    }

    void handleUpload_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                       size_t len, bool final)
    {
        if (index == 0)
        {
            if (!checkAuth_(request))
                return;
            _upload_ok = true;
            _upload_error = "";
            _upload_name = filename;
            String path = "/";
            path += filename;
            if (_log && _log->ready())
                _log->info(F("WEB"), F("Upload start %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
            if (!isAllowedExt_(path))
            {
                _upload_ok = false;
                _upload_error = "Недопустимое расширение файла";
                return;
            }
            _upload_size = 0;
            _upload = LittleFS.open(path, "w");
        }
        if (!_upload_ok)
            return;
        _upload_size += len;
        if (_max_upload > 0 && _upload_size > _max_upload)
        {
            _upload_ok = false;
            _upload_error = "Файл слишком большой";
            if (_upload)
                _upload.close();
            return;
        }
        if (_upload)
            _upload.write(data, len);
        if (final && _upload)
            _upload.close();
    }

    void handleOta_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final)
    {
        if (index == 0)
        {
            if (!checkAuth_(request))
                return;
#if !defined(ESP32)
            _ota_ok = false;
            _ota_error = "OTA не поддерживается";
            return;
#else
            _ota_ok = true;
            _ota_error = "";
            _ota_size = 0;
            _ota_name = filename;
            if (_log && _log->ready())
                _log->info(F("WEB"), F("OTA start %s (ip=%s)"), filename.c_str(), requestIp_(request).c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            {
                _ota_ok = false;
                _ota_error = Update.errorString();
            }
#endif
        }
#if defined(ESP32)
        if (!_ota_ok)
            return;
        _ota_size += len;
        if (_max_upload > 0 && _ota_size > _max_upload)
        {
            _ota_ok = false;
            _ota_error = "Слишком большой файл прошивки";
            Update.abort();
            return;
        }
        if (Update.write(data, len) != len)
        {
            _ota_ok = false;
            _ota_error = Update.errorString();
            Update.abort();
            return;
        }
        if (final)
        {
            if (!Update.end(true))
            {
                _ota_ok = false;
                _ota_error = Update.errorString();
            }
        }
#endif
    }

    void handleUploadDone_(AsyncWebServerRequest *request)
    {
        if (!_upload_ok)
            _last_status = _upload_error.length() ? _upload_error : "Загрузка не удалась";
        else
            _last_status = "Загрузка завершена";
        if (_log && _log->ready())
        {
            const String name = _upload_name.length() ? _upload_name : String("-");
            if (_upload_ok)
                _log->info(F("WEB"), F("Upload done %s size=%lu (ip=%s)"), name.c_str(), (unsigned long)_upload_size,
                           requestIp_(request).c_str());
            else
                _log->warn(F("WEB"), F("Upload fail %s size=%lu err=%s (ip=%s)"), name.c_str(),
                           (unsigned long)_upload_size, _upload_error.c_str(), requestIp_(request).c_str());
        }
        request->redirect("/status");
    }

    void handleOtaDone_(AsyncWebServerRequest *request)
    {
        if (!_ota_ok)
            _last_status = _ota_error.length() ? _ota_error : "Обновление прошивки не удалось";
        else
            _last_status = "Прошивка обновлена. Перезагрузка...";
        if (_log && _log->ready())
        {
            const String name = _ota_name.length() ? _ota_name : String("-");
            if (_ota_ok)
                _log->info(F("WEB"), F("OTA done %s size=%lu (ip=%s)"), name.c_str(), (unsigned long)_ota_size,
                           requestIp_(request).c_str());
            else
                _log->warn(F("WEB"), F("OTA fail %s size=%lu err=%s (ip=%s)"), name.c_str(), (unsigned long)_ota_size,
                           _ota_error.c_str(), requestIp_(request).c_str());
        }
        request->redirect("/status");
#if defined(ESP32)
        if (_ota_ok)
        {
            delay(500);
            ESP.restart();
        }
#endif
    }

    void handleWifiSave_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        bool changed = false;

        if (request->hasParam("mode", true))
        {
            String mode = request->getParam("mode", true)->value();
            mode.toLowerCase();
            const bool ap = (mode == "ap");
            if (ap != _wifi.ap())
            {
                _wifi.setAp(ap);
                changed = true;
            }
        }
        if (request->hasParam("ssid", true))
        {
            String ssid = request->getParam("ssid", true)->value();
            ssid.trim();
            if (ssid.length() > 0 && ssid != _wifi.ssid())
            {
                _wifi.setSsid(ssid);
                changed = true;
            }
        }
        if (request->hasParam("password", true))
        {
            String pass = request->getParam("password", true)->value();
            if (pass.length() > 0 && pass != _wifi.password())
            {
                _wifi.setPassword(pass);
                changed = true;
            }
        }
        if (request->hasParam("ap_ssid", true))
        {
            String ssid = request->getParam("ap_ssid", true)->value();
            ssid.trim();
            if (ssid.length() > 0 && ssid != _wifi.apSsid())
            {
                _wifi.setApSsid(ssid);
                changed = true;
            }
        }
        if (request->hasParam("ap_password", true))
        {
            String pass = request->getParam("ap_password", true)->value();
            if (pass.length() > 0 && pass != _wifi.apPassword())
            {
                _wifi.setApPassword(pass);
                changed = true;
            }
        }

        bool wifi_ok = true;
        bool save_ok = true;
        if (changed)
        {
            wifi_ok = _wifi.begin();
            save_ok = saveWifiConfig_();
        }

        if (!changed)
            _wifi_status = "Нет изменений";
        else if (!wifi_ok && !save_ok)
            _wifi_status = "Не удалось применить Wi-Fi и сохранить конфигурацию";
        else if (!wifi_ok)
            _wifi_status = "Не удалось применить Wi-Fi";
        else if (!save_ok)
            _wifi_status = "Wi-Fi применен, но сохранить конфигурацию не удалось";
        else
            _wifi_status = "Wi-Fi обновлен";

        if (_log && _log->ready())
        {
            if (!changed)
                _log->info(F("WEB"), F("WiFi save: no changes (ip=%s)"), requestIp_(request).c_str());
            else if (wifi_ok && save_ok)
                _log->info(F("WEB"), F("WiFi save ok (mode=%s, ssid=%s, ap_ssid=%s, ip=%s)"),
                           _wifi.ap() ? "AP" : "STA", _wifi.ssid().c_str(), _wifi.apSsid().c_str(),
                           requestIp_(request).c_str());
            else
                _log->warn(F("WEB"), F("WiFi save fail wifi=%s save=%s (ip=%s)"), wifi_ok ? "ok" : "err",
                           save_ok ? "ok" : "err", requestIp_(request).c_str());
        }
        request->redirect("/");
    }

    void handleFileDownload_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        String path;
        if (request->hasParam("path"))
            path = request->getParam("path")->value();
        else
            path = request->url().substring(String("/files").length());
        if (!path.startsWith("/"))
            path = "/" + path;
        if (!LittleFS.exists(path))
        {
            if (_log && _log->ready())
                _log->warn(F("WEB"), F("Download missing %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
            request->send(404, "text/plain", "Файл не найден");
            return;
        }
        if (_log && _log->ready())
            _log->info(F("WEB"), F("Download %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
        request->send(LittleFS, path, "application/octet-stream");
    }

    void handleDelete_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (!request->hasParam("path"))
        {
            request->send(400, "text/plain", "Не указан путь");
            return;
        }
        String path = request->getParam("path")->value();
        if (!path.startsWith("/"))
            path = "/" + path;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("Delete request %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
        if (!LittleFS.exists(path))
        {
            if (_log && _log->ready())
                _log->warn(F("WEB"), F("Delete missing %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
            request->send(404, "text/plain", "Файл не найден");
            return;
        }
        if (!LittleFS.remove(path))
        {
            if (_log && _log->ready())
                _log->warn(F("WEB"), F("Delete failed %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
            request->send(500, "text/plain", "Не удалось удалить файл");
            return;
        }
        request->redirect("/");
    }

    void handleStatus_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /status (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceStatusHtml);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        page.replace("%STATUS%", _last_status.length() ? _last_status : "Нет данных");
        request->send(200, "text/html", page);
    }

    bool checkAuth_(AsyncWebServerRequest *request)
    {
        if (_cli_auth)
        {
            if (!_cli_auth->adminPasswordSet())
                return true;
            if (request->authenticate(CliConsole::kAdminUser, _cli_auth->adminPassword().c_str()))
                return true;
            if (_log && _log->ready())
                _log->warn(F("WEB"), F("Auth failed (ip=%s, url=%s)"), requestIp_(request).c_str(),
                           request->url().c_str());
            request->requestAuthentication();
            return false;
        }
        if (!_auth_enabled)
            return true;
        if (request->authenticate(_auth_user.c_str(), _auth_pass.c_str()))
            return true;
        if (_log && _log->ready())
            _log->warn(F("WEB"), F("Auth failed (ip=%s, url=%s)"), requestIp_(request).c_str(),
                       request->url().c_str());
        request->requestAuthentication();
        return false;
    }

    String requestIp_(AsyncWebServerRequest *request) const
    {
        if (!request)
            return "unknown";
        auto *client = request->client();
        if (!client)
            return "unknown";
        return client->remoteIP().toString();
    }

    String wifiIp_() const
    {
        if (_wifi.ap())
            return WiFi.softAPIP().toString();
        if (WiFi.status() == WL_CONNECTED)
            return WiFi.localIP().toString();
        return "disconnected";
    }

    String wifiStaSegment_() const
    {
        if (_wifi.ap())
            return "";
        return String(" | STA: ") + wifiStaStatus_();
    }

    String wifiStaStatus_() const
    {
        switch (WiFi.status())
        {
        case WL_IDLE_STATUS:
            return "ожидание";
        case WL_NO_SSID_AVAIL:
            return "SSID не найден";
        case WL_SCAN_COMPLETED:
            return "сканирование завершено";
        case WL_CONNECTED:
            return "подключено";
        case WL_CONNECT_FAILED:
            return "не удалось подключиться";
        case WL_CONNECTION_LOST:
            return "соединение потеряно";
        case WL_DISCONNECTED:
            return "отключено";
        default:
            return "неизвестно";
        }
    }

    bool saveWifiConfig_()
    {
        if (_configs_manager)
            return _configs_manager->save();
        return false;
    }

    bool isAllowedExt_(const String &path) const
    {
        if (_allowed_exts.length() == 0)
            return true;
        int dot = path.lastIndexOf('.');
        if (dot < 0)
            return false;
        String ext = path.substring(dot + 1);
        ext.toLowerCase();
        String list = _allowed_exts;
        list.replace(" ", "");
        size_t start = 0;
        while (start < list.length())
        {
            int comma = list.indexOf(',', start);
            if (comma < 0)
                comma = list.length();
            String token = list.substring(start, comma);
            if (token == ext)
                return true;
            start = (size_t)comma + 1;
        }
        return false;
    }

    AsyncWebServer &_server;
    WifiManager &_wifi;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;
    File _upload;
    bool _upload_ok = true;
    size_t _upload_size = 0;
    size_t _max_upload = 0;
    bool _ota_ok = false;
    size_t _ota_size = 0;
    String _ota_error;
    String _allowed_exts;
    String _last_status;
    String _wifi_status;
    String _upload_error;
    String _upload_name;
    String _ota_name;
    bool _auth_enabled = false;
    String _auth_user;
    String _auth_pass;
    const CliConsole *_cli_auth = nullptr;
    Logger *_log = nullptr;
};
