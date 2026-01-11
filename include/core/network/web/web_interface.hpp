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
#include <WebServer.h>
#include <WiFi.h>

#include "core/cli/cli_console.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/web/web_interface_page.hpp"
#include "core/network/web/web_interface_status.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"

class WebInterface
{
public:
    WebInterface(WebServer &server, const CliConsole &cli, WifiManager &wifi, Configs &configs)
        : _server(server), _cli_auth(&cli), _wifi(wifi), _configs(configs)
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
        _server.on("/", HTTP_GET, [this]() { handleIndex_(); });
        _server.on("/upload", HTTP_POST, [this]() { handleUploadDone_(); },
                   [this]() { handleUpload_(); });
        _server.on("/wifi", HTTP_POST, [this]() { handleWifiSave_(); });
        _server.on("/files", HTTP_GET, [this]() { handleFileDownload_(); });
        _server.on("/delete", HTTP_GET, [this]() { handleDelete_(); });
        _server.on("/status", HTTP_GET, [this]() { handleStatus_(); });
    }

private:
    void handleIndex_()
    {
        if (!checkAuth_())
            return;
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
        _server.send(200, "text/html", page);
    }

    String listFilesHtml_()
    {
        String items;
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file)
        {
            String name = file.name();
            items += "<tr><td><a href=\"/files";
            items += name;
            items += "\">";
            items += name;
            items += "</a></td><td class=\"right\">";
            items += String((unsigned)file.size());
            items += " B</td><td class=\"right\"><a class=\"del\" onclick=\"return confirm('Удалить файл ";
            items += name;
            items += "?')\" href=\"/delete?path=";
            items += name;
            items += "\">Удалить</a></td></tr>";
            file = root.openNextFile();
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"3\" style=\"color:#94a3b8\">Нет файлов</td></tr>";
        return items;
    }

    void handleUpload_()
    {
        if (!checkAuth_())
            return;
        HTTPUpload &up = _server.upload();
        if (up.status == UPLOAD_FILE_START)
        {
            _upload_ok = true;
            _upload_error = "";
            String filename = "/";
            filename += up.filename;
            if (!isAllowedExt_(filename))
            {
                _upload_ok = false;
                _upload_error = "Недопустимое расширение";
                return;
            }
            _upload_size = 0;
            _upload = LittleFS.open(filename, "w");
        }
        else if (up.status == UPLOAD_FILE_WRITE)
        {
            if (!_upload_ok)
                return;
            _upload_size += up.currentSize;
            if (_max_upload > 0 && _upload_size > _max_upload)
            {
                _upload_ok = false;
                _upload_error = "Файл слишком большой";
                if (_upload)
                    _upload.close();
                return;
            }
            if (_upload)
                _upload.write(up.buf, up.currentSize);
        }
        else if (up.status == UPLOAD_FILE_END)
        {
            if (_upload)
                _upload.close();
        }
    }

    void handleUploadDone_()
    {
        if (!_upload_ok)
            _last_status = _upload_error.length() ? _upload_error : "Загрузка не удалась";
        else
            _last_status = "Успешно";
        _server.sendHeader("Location", "/status");
        _server.send(303);
    }

    void handleWifiSave_()
    {
        if (!checkAuth_())
            return;
        bool changed = false;

        if (_server.hasArg("mode"))
        {
            String mode = _server.arg("mode");
            mode.toLowerCase();
            const bool ap = (mode == "ap");
            if (ap != _wifi.ap())
            {
                _wifi.setAp(ap);
                changed = true;
            }
        }
        if (_server.hasArg("ssid"))
        {
            String ssid = _server.arg("ssid");
            ssid.trim();
            if (ssid.length() > 0 && ssid != _wifi.ssid())
            {
                _wifi.setSsid(ssid);
                changed = true;
            }
        }
        if (_server.hasArg("password"))
        {
            String pass = _server.arg("password");
            if (pass.length() > 0 && pass != _wifi.password())
            {
                _wifi.setPassword(pass);
                changed = true;
            }
        }
        if (_server.hasArg("ap_ssid"))
        {
            String ssid = _server.arg("ap_ssid");
            ssid.trim();
            if (ssid.length() > 0 && ssid != _wifi.apSsid())
            {
                _wifi.setApSsid(ssid);
                changed = true;
            }
        }
        if (_server.hasArg("ap_password"))
        {
            String pass = _server.arg("ap_password");
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
            _wifi_status = "Без изменений";
        else if (!wifi_ok && !save_ok)
            _wifi_status = "Перезапуск Wi‑Fi не удался, сохранение конфигурации не удалось";
        else if (!wifi_ok)
            _wifi_status = "Перезапуск Wi‑Fi не удался";
        else if (!save_ok)
            _wifi_status = "Wi‑Fi обновлен, сохранение конфигурации не удалось";
        else
            _wifi_status = "Wi‑Fi обновлен";

        _server.sendHeader("Location", "/");
        _server.send(303);
    }

    void handleFileDownload_()
    {
        if (!checkAuth_())
            return;
        String path = _server.uri().substring(String("/files").length());
        if (!LittleFS.exists(path))
        {
            _server.send(404, "text/plain", "Не найдено");
            return;
        }
        File f = LittleFS.open(path, "r");
        _server.streamFile(f, "application/octet-stream");
        f.close();
    }

    void handleDelete_()
    {
        if (!checkAuth_())
            return;
        if (!_server.hasArg("path"))
        {
            _server.send(400, "text/plain", "Не указан путь");
            return;
        }
        String path = _server.arg("path");
        if (!path.startsWith("/"))
            path = "/" + path;
        if (!LittleFS.exists(path))
        {
            _server.send(404, "text/plain", "Не найдено");
            return;
        }
        if (!LittleFS.remove(path))
        {
            _server.send(500, "text/plain", "Ошибка удаления");
            return;
        }
        _server.sendHeader("Location", "/");
        _server.send(303);
    }

    void handleStatus_()
    {
        if (!checkAuth_())
            return;
        String page = FPSTR(kWebInterfaceStatusHtml);
        page.replace("%STATUS%", _last_status.length() ? _last_status : "Загрузок пока нет");
        _server.send(200, "text/html", page);
    }

    bool checkAuth_()
    {
        if (_cli_auth)
        {
            if (!_cli_auth->adminPasswordSet())
                return true;
            if (_server.authenticate(CliConsole::kAdminUser, _cli_auth->adminPassword().c_str()))
                return true;
            _server.requestAuthentication();
            return false;
        }
        if (!_auth_enabled)
            return true;
        if (_server.authenticate(_auth_user.c_str(), _auth_pass.c_str()))
            return true;
        _server.requestAuthentication();
        return false;
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
            return "Ожидание";
        case WL_NO_SSID_AVAIL:
            return "Сеть не найдена";
        case WL_SCAN_COMPLETED:
            return "Сканирование завершено";
        case WL_CONNECTED:
            return "Подключено";
        case WL_CONNECT_FAILED:
            return "Ошибка подключения";
        case WL_CONNECTION_LOST:
            return "Потеря связи";
        case WL_DISCONNECTED:
            return "Отключено";
        default:
            return "Неизвестно";
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

    WebServer &_server;
    WifiManager &_wifi;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;
    File _upload;
    bool _upload_ok = true;
    size_t _upload_size = 0;
    size_t _max_upload = 0;
    String _allowed_exts;
    String _last_status;
    String _wifi_status;
    String _upload_error;
    bool _auth_enabled = false;
    String _auth_user;
    String _auth_pass;
    const CliConsole *_cli_auth = nullptr;
};
