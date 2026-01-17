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
#include <vector>
#include <LittleFS.h>
#include <WiFi.h>

#if defined(ESP32)
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <esp_system.h>
#endif

#include "boards/board_profile.hpp"
#include "core/cli/cli_console.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/web/pages/web_interface_page.hpp"
#include "core/network/web/pages/web_interface_manage.hpp"
#include "core/network/web/pages/web_interface_ports.hpp"
#include "core/network/web/pages/web_interface_buses.hpp"
#include "core/network/web/pages/web_interface_stack.hpp"
#include "core/network/web/pages/web_interface_wifi.hpp"
#include "core/network/web/pages/web_interface_admin.hpp"
#include "core/network/web/pages/web_interface_logs.hpp"
#include "core/network/web/pages/web_interface_telegram.hpp"
#include "core/network/web/pages/web_interface_status.hpp"
#include "core/network/web/pages/web_interface_sockets.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/fs_config.hpp"
#include "utils/configs_manager_iface.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "controllers/controllers.hpp"

class WebInterface
{
public:
    WebInterface(AsyncWebServer &server, CliConsole &cli, WifiManager &wifi, Configs &configs, PlcControl &plc,
                 RTC &rtc, TelegramClient &tgbot, TelegramMenu &tgbot_menu, Logger &logs, Extender &ext,
                 I2CManager &i2c, OneWireManager &ow, Controllers &controllers)
        : _server(server),
          _cli_auth(&cli),
          _wifi(wifi),
          _configs(configs),
          _plc(&plc),
          _rtc(&rtc),
          _tgbot(&tgbot),
          _tgbot_menu(&tgbot_menu),
          _controllers(&controllers),
          _ext(&ext),
          _i2c(&i2c),
          _ow(&ow),
          _log(&logs)
    {
    }

    bool begin(bool format_on_fail = false)
    {
        return LittleFS.begin(format_on_fail, FsConfig::kBasePath, FsConfig::kMaxOpenFiles,
                              FsConfig::kPartitionLabel);
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
        _server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) { handleWifi_(request); });
        _server.on("/manage", HTTP_GET, [this](AsyncWebServerRequest *request) { handleManage_(request); });
        _server.on("/ports", HTTP_GET, [this](AsyncWebServerRequest *request) { handlePorts_(request); });
        _server.on("/buses", HTTP_GET, [this](AsyncWebServerRequest *request) { handleBuses_(request); });
        _server.on("/stack", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStack_(request); });
        _server.on("/sockets", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSockets_(request); });
        _server.on("/sockets", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSocketsSave_(request); });
        _server.on("/telegram", HTTP_GET, [this](AsyncWebServerRequest *request) { handleTelegram_(request); });
        _server.on("/telegram", HTTP_POST, [this](AsyncWebServerRequest *request) { handleTelegramSave_(request); });
        _server.on("/logout", HTTP_GET, [this](AsyncWebServerRequest *request) { handleLogout_(request); });
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
        _server.on("/stack", HTTP_POST, [this](AsyncWebServerRequest *request) { handleStackSave_(request); });
        _server.on("/device", HTTP_POST, [this](AsyncWebServerRequest *request) { handleDeviceSave_(request); });
        _server.on("/admin", HTTP_GET, [this](AsyncWebServerRequest *request) { handleAdmin_(request); });
        _server.on("/admin", HTTP_POST, [this](AsyncWebServerRequest *request) { handleAdminSave_(request); });
        _server.on("/logs", HTTP_GET, [this](AsyncWebServerRequest *request) { handleLogs_(request); });
        _server.on("/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) { handleReboot_(request); });
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
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET / (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceIndexHtml);
        page.replace("%NAV%", navHtml_());
        const bool logged_out = request->hasParam("logout");
        page.replace("%LOGOUT_MSG%", logged_out ? "Logged out" : "");
        page.replace("%DEVICE_NAME%", deviceName_());
        page.replace("%DEVICE_STATUS%", _device_status);
        const auto role = stackRole_();
        page.replace("%STACK_ROLE%", stackRoleName_(role));
        page.replace("%STACK_ROLE_MASTER_SEL%", role == ConfigsManagerIface::StackRole::Master ? "selected" : "");
        page.replace("%STACK_ROLE_SLAVE_SEL%", role == ConfigsManagerIface::StackRole::Slave ? "selected" : "");
        page.replace("%STACK_MASTER_HOST%", stackMasterHost_());
        page.replace("%STACK_STATUS%", _stack_status);
        page.replace("%BOARD_TEMP%", formatTemp_(boardTemp_()));
        page.replace("%CPU_TEMP%", formatTemp_(cpuTemp_()));
        page.replace("%RTC_TIME%", rtcTimeStr_());
        page.replace("%RTC_TEMP%", formatTemp_(rtcTemp_()));
        page.replace("%FAN_STATUS%", fanStatusStr_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleWifi_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /wifi (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceWifiHtml);
        page.replace("%NAV%", navHtml_());
        page.replace("%WIFI_MODE%", _wifi.ap() ? "AP" : "STA");
        page.replace("%WIFI_CUR_SSID%", _wifi.ap() ? _wifi.apSsid() : _wifi.ssid());
        page.replace("%WIFI_IP%", wifiIp_());
        page.replace("%WIFI_STA_SEG%", wifiStaSegment_());
        page.replace("%WIFI_STA_SEL%", _wifi.ap() ? "" : "selected");
        page.replace("%WIFI_AP_SEL%", _wifi.ap() ? "selected" : "");
        page.replace("%WIFI_SSID%", _wifi.ssid());
        page.replace("%WIFI_AP_SSID%", _wifi.apSsid());
        page.replace("%WIFI_STATUS%", _wifi_status);
        sendHtml_(request, page, set_cookie);
    }

    void handleManage_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /manage (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceManageHtml);
        page.replace("%NAV%", navHtml_());
        page.replace("%FILES%", listFilesHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleLogs_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /logs (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceLogsHtml);
        page.replace("%NAV%", navHtml_());
        String lines;
        if (_log)
        {
            const size_t count = _log->recentCount();
            if (count == 0)
            {
                lines = "No logs";
            }
            else
            {
                char buf[LOGGER_BUFFER_SIZE] = {};
                for (size_t i = 0; i < count; ++i)
                {
                    if (_log->getRecentLine(i, buf, sizeof(buf)))
                    {
                        appendHtmlEscaped_(lines, buf);
                        lines += "\n";
                    }
                }
            }
        }
        else
        {
            lines = "Logger unavailable";
        }
        page.replace("%LOG_LINES%", lines);
        sendHtml_(request, page, set_cookie);
    }

    void handleAdmin_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (_cli_auth && _cli_auth->adminPasswordSet())
        {
            if (!checkAuth_(request, &set_cookie))
                return;
        }
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /admin (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceAdminHtml);
        page.replace("%NAV%", navHtml_());
        page.replace("%ADMIN_STATUS%", (_cli_auth && _cli_auth->adminPasswordSet()) ? "set" : "not set");
        sendHtml_(request, page, set_cookie);
    }

    void handleAdminSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (_cli_auth && _cli_auth->adminPasswordSet())
        {
            if (!checkAuth_(request, &set_cookie))
                return;
        }
        if (!_cli_auth)
        {
            sendText_(request, 500, "text/plain", "CLI auth unavailable", set_cookie);
            return;
        }
        if (!request->hasParam("password", true))
        {
            sendText_(request, 400, "text/plain", "Missing password", set_cookie);
            return;
        }
        String pass = request->getParam("password", true)->value();
        pass.trim();
        if (!_cli_auth->setAdminPassword_(pass))
        {
            sendText_(request, 400, "text/plain", "Invalid password", set_cookie);
            return;
        }
        if (_configs_manager)
            _configs_manager->save();
        if (_log && _log->ready())
            _log->info(F("WEB"), F("Admin password updated (ip=%s)"), requestIp_(request).c_str());
        sendRedirect_(request, "/", set_cookie);
    }

    void handlePorts_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /ports (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfacePortsHtml);
        page.replace("%NAV%", navHtml_());
        page.replace("%PORTS%", listPortsHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleBuses_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /buses (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceBusesHtml);
        page.replace("%NAV%", navHtml_());
        page.replace("%I2C%", listI2cHtml_());
        page.replace("%OW%", listOwHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleStack_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /stack (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceStackHtml);
        page.replace("%NAV%", navHtml_());
        const auto role = stackRole_();
        page.replace("%STACK_ROLE%", stackRoleName_(role));
        page.replace("%STACK_ROLE_MASTER_SEL%", role == ConfigsManagerIface::StackRole::Master ? "selected" : "");
        page.replace("%STACK_ROLE_SLAVE_SEL%", role == ConfigsManagerIface::StackRole::Slave ? "selected" : "");
        page.replace("%STACK_MASTER_HOST%", stackMasterHost_());
        page.replace("%STACK_STATUS%", _stack_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleSockets_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /sockets (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceSocketsHtml);
        page.replace("%NAV%", navHtml_());
        page.replace("%SOCKETS%", listSocketsHtml_());
        page.replace("%DINPUT_JSON%", socketPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%RELAY_JSON%", socketPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%SOCKETS_STATUS%", _sockets_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleTelegram_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /telegram (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceTelegramHtml);
        page.replace("%NAV%", navHtml_());
        page.replace("%TGBOT_TOKEN%", _tgbot ? _tgbot->token() : String(""));
        page.replace("%TGBOT_CHAT_ID%", _tgbot ? String((long long)_tgbot->chatId()) : String("0"));
        page.replace("%TGBOT_INSECURE_CHECKED%", _tgbot && _tgbot->insecure() ? "checked" : "");
        page.replace("%TGBOT_CLIENT%", _tgbot ? _tgbot->clientKindName() : "none");
        page.replace("%TGBOT_USE_PROXY_CHECKED%", _tgbot && _tgbot->useProxy() ? "checked" : "");
        page.replace("%TGBOT_PROXY_HOST%", _tgbot ? _tgbot->proxyHost() : String(""));
        page.replace("%TGBOT_PROXY_PORT%", _tgbot ? String((unsigned)_tgbot->proxyPort()) : String("0"));
        page.replace("%TGBOT_PROXY_PATH%", _tgbot ? _tgbot->proxyPath() : String(""));
        page.replace("%TGBOT_ALLOWED_USERS%", allowedUsersCsv_());
        page.replace("%TGBOT_STATUS%", _tgbot_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleSocketsSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        SocketController &sockets = _controllers->sockets();
        bool ok = true;
        bool changed = false;
        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const String idx = String((unsigned)i);
            const String prefix = String("s") + idx + "_";
            const bool enabled = request->hasParam(prefix + "en", true);
            String name = paramValue_(request, prefix + "name");
            String btn = paramValue_(request, prefix + "btn");
            String relay = paramValue_(request, prefix + "relay");
            String action = paramValue_(request, prefix + "action");
            name.trim();
            uint8_t btn_port = SocketController::kInvalidPort;
            uint8_t relay_port = SocketController::kInvalidPort;
            if (!parseSocketPort_(btn, btn_port) || !parseSocketPort_(relay, relay_port))
            {
                ok = false;
                _sockets_status = String("Invalid port for socket ") + idx;
                break;
            }
            const auto *cfg = sockets.config(i);
            if (!cfg)
                continue;
            if (cfg->name != name)
                sockets.setName(i, name);
            if (cfg->button_port != btn_port)
                sockets.setButtonPort(i, btn_port);
            if (cfg->relay_port != relay_port)
                sockets.setRelayPort(i, relay_port);
            if (cfg->enabled != enabled)
                sockets.setEnabled(i, enabled);
            if (action.length())
            {
                String act = action;
                act.toLowerCase();
                if (act == "on")
                {
                    sockets.setRelay(i, true);
                    changed = true;
                }
                else if (act == "off")
                {
                    sockets.setRelay(i, false);
                    changed = true;
                }
                else if (act == "toggle")
                {
                    sockets.toggleRelay(i);
                    changed = true;
                }
            }
        }
        if (ok)
        {
            if (!_configs_manager)
            {
                ok = false;
                _sockets_status = "Config manager missing";
            }
            else if (!_configs_manager->save())
            {
                ok = false;
                _sockets_status = "Save failed";
            }
        }
        if (ok)
            _sockets_status = changed ? "Updated" : "Saved";
        sendRedirect_(request, "/sockets", set_cookie);
    }

    void handleTelegramSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        bool changed = false;

        if (_tgbot && request->hasParam("token", true))
        {
            String token = request->getParam("token", true)->value();
            token.trim();
            if (token != _tgbot->token())
            {
                _tgbot->setToken(token);
                changed = true;
            }
        }
        if (_tgbot && request->hasParam("chat_id", true))
        {
            String chat = request->getParam("chat_id", true)->value();
            chat.trim();
            if (chat.length() > 0)
            {
                int64_t chat_id = (int64_t)strtoll(chat.c_str(), nullptr, 10);
                if (chat_id != _tgbot->chatId())
                {
                    _tgbot->setChatId(chat_id);
                    changed = true;
                }
            }
        }
        if (_tgbot)
        {
            const bool insecure = request->hasParam("insecure", true);
            if (insecure != _tgbot->insecure())
            {
                _tgbot->setInsecure(insecure);
                changed = true;
            }
        }

        if (_tgbot)
        {
            const bool use_proxy = request->hasParam("use_proxy", true);
            String host = request->hasParam("proxy_host", true) ? request->getParam("proxy_host", true)->value() : "";
            String port_str = request->hasParam("proxy_port", true) ? request->getParam("proxy_port", true)->value() : "0";
            String path = request->hasParam("proxy_path", true) ? request->getParam("proxy_path", true)->value() : "";
            host.trim();
            path.trim();
            const uint16_t port = (uint16_t)strtoul(port_str.c_str(), nullptr, 10);

            if (use_proxy)
            {
                if (host != _tgbot->proxyHost() || port != _tgbot->proxyPort() || path != _tgbot->proxyPath() || !_tgbot->useProxy())
                {
                    _tgbot->setProxy(host, port, path);
                    changed = true;
                }
            }
            else if (_tgbot->useProxy())
            {
                _tgbot->clearProxy();
                changed = true;
            }
        }

        if (_tgbot_menu && request->hasParam("allowed_users", true))
        {
            String raw = request->getParam("allowed_users", true)->value();
            std::vector<String> users = splitCsv_(raw);
            _tgbot_menu->setAllowedUsers(users);
            changed = true;
        }

        bool save_ok = true;
        if (changed)
            save_ok = saveWifiConfig_();

        if (!changed)
            _tgbot_status = "No changes";
        else if (!save_ok)
            _tgbot_status = "Save failed";
        else
            _tgbot_status = "Saved";

        sendRedirect_(request, "/telegram", set_cookie);
    }

    void handleLogout_(AsyncWebServerRequest *request)
    {
        clearSession_();
        auto *response = request->beginResponse(302);
        response->addHeader("Location", "/?logout=1");
        response->addHeader("Set-Cookie", clearSessionCookie_());
        request->send(response);
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
            items += "<strong>";
            items += name;
            items += "</strong>";
            items += "</a></td><td class=\"right\">";
            items += "<strong>";
            items += String((unsigned)file.size());
            items += "</strong>";
            items += " B</td><td class=\"right\"><a class=\"del\" onclick=\"return confirm('Delete file ";
            items += name;
            items += "?')\" href=\"/delete?path=";
            items += path;
            items += "\"><strong>Delete</strong></a></td></tr>";
            file = root.openNextFile();
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>No files</strong></td></tr>";
        return items;
    }

    static const char *extTypeName_(Extender::Type t)
    {
        switch (t)
        {
        case Extender::Type::PCF8574:
            return "PCF8574";
        case Extender::Type::MCP23017:
            return "MCP23017";
        default:
            return "None";
        }
    }

    static const char *extDevTypeName_(uint8_t dev)
    {
        if (dev >= ActiveBoardProfile::EXT_DEVS_COUNT)
            return "None";
        return extTypeName_(ActiveBoardProfile::EXT_DEVS[dev].type);
    }

    static const char *portTypeName_(PortIO::PinType t)
    {
        switch (t)
        {
        case PortIO::PinType::System:
            return "System";
        case PortIO::PinType::Relay:
            return "Relay";
        case PortIO::PinType::Led:
            return "Led";
        case PortIO::PinType::Sensor:
            return "Sensor";
        case PortIO::PinType::Button:
            return "Button";
        case PortIO::PinType::DInput:
            return "DInput";
        case PortIO::PinType::Buzzer:
            return "Buzzer";
        case PortIO::PinType::Fan:
            return "Fan";
        default:
            return "Unknown";
        }
    }

    static const char *locationName_(PortIO::Location loc)
    {
        switch (loc)
        {
        case PortIO::Location::Cpu:
            return "CPU";
        case PortIO::Location::Ext1:
            return "EXT_1";
        case PortIO::Location::Ext2:
            return "EXT_2";
        case PortIO::Location::Ext3:
            return "EXT_3";
        case PortIO::Location::Ext4:
            return "EXT_4";
        case PortIO::Location::Ext5:
            return "EXT_5";
        case PortIO::Location::Ext6:
            return "EXT_6";
        case PortIO::Location::Ext7:
            return "EXT_7";
        case PortIO::Location::Ext8:
            return "EXT_8";
        case PortIO::Location::Ext9:
            return "EXT_9";
        case PortIO::Location::Ext10:
            return "EXT_10";
        default:
            return "UNKNOWN";
        }
    }

    static const char *owBusName_(OneWireCfg::OwType t)
    {
        switch (t)
        {
        case OneWireCfg::OwType::iButton:
            return "iButton";
        case OneWireCfg::OwType::Temp:
            return "Temp";
        default:
            return "Unknown";
        }
    }

    static void owAddrToHex_(const uint8_t in[8], char out[17])
    {
        static const char kHex[] = "0123456789ABCDEF";
        for (uint8_t i = 0; i < 8; ++i)
        {
            out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
            out[i * 2 + 1] = kHex[in[i] & 0x0F];
        }
        out[16] = '\0';
    }

    String listPortsHtml_()
    {
        String items;
        for (uint16_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = ActiveBoardProfile::PORTS[i];
            if (p.caps == Cap::None)
                continue;
            if (p.backend == PortIO::Backend::Extender)
            {
                if (!_ext)
                    continue;
                const uint8_t dev = p.u.ext.dev;
                const auto *devs = _ext->devs();
                if (!devs || dev >= _ext->devCount())
                    continue;
                if (devs[dev].type != Extender::Type::MCP23017)
                    continue;
                if (!_ext->isPresent(dev))
                    continue;
            }

            items += "<tr><td class=\"right\"><strong>";
            items += String(i);
            items += "</strong></td><td><strong>";
            items += (p.backend == PortIO::Backend::Extender) ? "Extender" : "Esp32";
            items += "</strong></td><td><strong>";
            items += locationName_(p.location);
            items += "</strong></td><td><strong>";
            items += portTypeName_(p.type);
            items += "</strong></td><td><strong>";
            items += p.allow_control ? "yes" : "no";
            items += "</strong></td><td class=\"right\"><strong>";

            if (p.backend == PortIO::Backend::Extender)
            {
                items += String(p.u.ext.dev);
                items += "</strong></td><td class=\"right\"><strong>";
                items += String(p.u.ext.pin);
                items += "</strong></td><td><strong>";
                items += extDevTypeName_(p.u.ext.dev);
            }
            else
            {
                items += "--</strong></td><td class=\"right\"><strong>";
                items += String(p.u.esp.gpio);
                items += "</strong></td><td><strong>CPU";
            }
            items += "</strong></td></tr>";
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"8\" style=\"color:#94a3b8\"><strong>No ports</strong></td></tr>";
        return items;
    }

    String listSocketsHtml_()
    {
        if (!_controllers)
            return "<tr><td colspan=\"5\" style=\"color:#94a3b8\"><strong>No sockets</strong></td></tr>";
        String items;
        SocketController &sockets = _controllers->sockets();
        bool tmp_state = false;
        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const auto *cfg = sockets.config(i);
            if (!cfg)
                continue;
            const bool on = sockets.relayState(i, tmp_state) ? tmp_state : false;
            items += "<tr";
            items += on ? " class=\"row-on\"" : " class=\"row-off\"";
            items += "><td class=\"right\"><strong>";
            items += String((unsigned)i);
            items += "</strong></td><td><input type=\"checkbox\" name=\"s";
            items += String((unsigned)i);
            items += "_en\"";
            if (cfg->enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"s";
            items += String((unsigned)i);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg->name.c_str());
            items += "\"></td><td><select class=\"field mini socket-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg->button_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg->button_port);
            items += "\" name=\"s";
            items += String((unsigned)i);
            items += "_btn\"></select></td><td><select class=\"field mini socket-select\" data-type=\"relay\" data-selected=\"";
            if (cfg->relay_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg->relay_port);
            items += "\" name=\"s";
            items += String((unsigned)i);
            items += "_relay\"></select></td><td>";
            items += "<button class=\"btn btn-sm btn-on\" name=\"s";
            items += String((unsigned)i);
            items += "_action\" value=\"on\" type=\"submit\">ON</button> ";
            items += "<button class=\"btn btn-sm btn-off\" name=\"s";
            items += String((unsigned)i);
            items += "_action\" value=\"off\" type=\"submit\">OFF</button> ";
            items += "<button class=\"btn btn-sm btn-toggle\" name=\"s";
            items += String((unsigned)i);
            items += "_action\" value=\"toggle\" type=\"submit\">TOGGLE</button>";
            items += " <strong>";
            items += on ? "on" : "off";
            items += "</strong></td></tr>";
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"6\" style=\"color:#94a3b8\"><strong>No sockets</strong></td></tr>";
        return items;
    }

    String listI2cHtml_()
    {
        if (!_i2c)
            return "<tr><td colspan=\"2\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
        String items;
        bool scanned[3] = {false, false, false};
        for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
        {
            const uint8_t bus = ActiveBoardProfile::I2CS[i].bus_num;
            if (bus < 3 && scanned[bus])
                continue;
            if (bus < 3)
                scanned[bus] = true;
            bool present[127] = {};
            if (!_i2c->scanDevices(bus, present))
                continue;
            for (uint8_t addr = 1; addr < 127; ++addr)
                if (present[addr])
                {
                    char addr_buf[8] = {};
                    snprintf(addr_buf, sizeof(addr_buf), "0x%02X", addr);
                    items += "<tr><td class=\"right\"><strong>";
                    items += String((unsigned)bus);
                    items += "</strong></td><td><strong>";
                    items += addr_buf;
                    items += "</strong></td></tr>";
                }
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"2\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
        return items;
    }

    String socketPortOptionsJson_(PortIO::PinType type) const
    {
        String out;
        out += "[";
        bool first = true;
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = ActiveBoardProfile::PORTS[i];
            if (p.caps == Cap::None || p.type != type)
                continue;
            if (!first)
                out += ",";
            out += String((unsigned)i);
            first = false;
        }
        out += "]";
        return out;
    }

    String listOwHtml_()
    {
        if (!_ow)
            return "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
        String items;
        for (uint8_t i = 0; i < ActiveBoardProfile::ONEWIRE_COUNT; ++i)
        {
            OneWireBus *bus = _ow->busPtrByIndex(i);
            if (!bus)
                continue;
            const auto &cfg = ActiveBoardProfile::ONEWIRES[i];
            uint8_t addr[8] = {};
            bus->reset_search();
            while (bus->search(addr))
            {
                if (OneWireBus::crc8(addr, 7) != addr[7])
                    continue;
                char hex[17] = {};
                owAddrToHex_(addr, hex);
                items += "<tr><td class=\"right\"><strong>";
                items += String((unsigned)i);
                items += "</strong></td><td><strong>";
                items += owBusName_(cfg.bus_id);
                items += "</strong></td><td><strong>";
                items += hex;
                items += "</strong></td></tr>";
            }
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
        return items;
    }

    void handleUpload_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                       size_t len, bool final)
    {
        if (index == 0)
        {
            bool set_cookie = false;
            if (!checkAuth_(request, &set_cookie, true))
                return;
            _upload_set_cookie = set_cookie;
            _upload_ok = true;
            _upload_error = "";
            _upload_name = filename;
            String path = sanitizeUploadName_(filename);
            if (!path.length())
            {
                _upload_ok = false;
                _upload_error = "Invalid file name";
                return;
            }
            if (_log && _log->ready())
                _log->info(F("WEB"), F("Upload start %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
            if (!isAllowedExt_(path))
            {
                _upload_ok = false;
                _upload_error = "File extension not allowed";
                return;
            }
            _upload_size = 0;
            _upload = LittleFS.open(path, "w");
            if (!_upload)
            {
                _upload_ok = false;
                _upload_error = "Open failed";
                return;
            }
        }
        if (!_upload_ok)
            return;
        _upload_size += len;
        if (_max_upload > 0 && _upload_size > _max_upload)
        {
            _upload_ok = false;
            _upload_error = "File too large";
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
            bool set_cookie = false;
            if (!checkAuth_(request, &set_cookie, true))
                return;
            _ota_set_cookie = set_cookie;
#if !defined(ESP32)
            _ota_ok = false;
            _ota_error = "OTA not supported";
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
            _ota_error = "Firmware image too large";
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
            _last_status = _upload_error.length() ? _upload_error : "Upload failed";
        else
            _last_status = "Upload complete";
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
        sendRedirect_(request, "/status", _upload_set_cookie);
        _upload_set_cookie = false;
    }

    void handleOtaDone_(AsyncWebServerRequest *request)
    {
        if (!_ota_ok)
            _last_status = _ota_error.length() ? _ota_error : "Firmware update failed";
        else
            _last_status = "Firmware updated. Rebooting...";
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
        sendRedirect_(request, "/status", _ota_set_cookie);
        _ota_set_cookie = false;
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
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
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
            _wifi_status = "No changes";
        else if (!wifi_ok && !save_ok)
            _wifi_status = "Wi-Fi apply and save failed";
        else if (!wifi_ok)
            _wifi_status = "Wi-Fi apply failed";
        else if (!save_ok)
            _wifi_status = "Wi-Fi applied, but save failed";
        else
            _wifi_status = "Wi-Fi updated";

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
        sendRedirect_(request, "/", set_cookie);
    }

    void handleStackSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_configs_manager)
        {
            _stack_status = "Config manager missing";
            sendRedirect_(request, "/", set_cookie);
            return;
        }

        bool changed = false;
        if (request->hasParam("role", true))
        {
            String role = request->getParam("role", true)->value();
            role.trim();
            role.toLowerCase();
            const auto new_role = (role == "slave") ? ConfigsManagerIface::StackRole::Slave
                                                    : ConfigsManagerIface::StackRole::Master;
            if (new_role != _configs_manager->stackRole())
            {
                _configs_manager->setStackRole(new_role);
                changed = true;
            }
        }

        String host = request->hasParam("master_host", true)
                          ? request->getParam("master_host", true)->value()
                          : String("");
        host.trim();
        if (host != _configs_manager->stackMasterHost())
        {
            _configs_manager->setStackMasterHost(host);
            changed = true;
        }

        bool save_ok = true;
        if (changed)
            save_ok = saveWifiConfig_();

        if (!changed)
            _stack_status = "No changes";
        else if (!save_ok)
            _stack_status = "Save failed";
        else
            _stack_status = "Saved";

        sendRedirect_(request, "/stack", set_cookie);
    }

    void handleDeviceSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_plc)
        {
            _device_status = "PLC missing";
            sendRedirect_(request, "/", set_cookie);
            return;
        }
        if (!request->hasParam("device_name", true))
        {
            _device_status = "Missing name";
            sendRedirect_(request, "/", set_cookie);
            return;
        }
        String name = request->getParam("device_name", true)->value();
        name.trim();
        bool changed = (name != _plc->deviceName());
        if (changed)
            _plc->setDeviceName(name);

        bool save_ok = true;
        if (changed)
            save_ok = saveWifiConfig_();

        if (!changed)
            _device_status = "No changes";
        else if (!save_ok)
            _device_status = "Save failed";
        else
            _device_status = "Saved";

        sendRedirect_(request, "/", set_cookie);
    }

    void handleReboot_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
#if defined(ESP32)
        if (_log && _log->ready())
            _log->info(F("WEB"), F("Reboot request (ip=%s)"), requestIp_(request).c_str());
        sendText_(request, 200, "text/plain", "Rebooting", set_cookie);
        delay(100);
        ESP.restart();
#else
        sendText_(request, 200, "text/plain", "Not supported", set_cookie);
#endif
    }

    void handleFileDownload_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String path;
        if (request->hasParam("path"))
            path = request->getParam("path")->value();
        else
            path = request->url().substring(String("/files").length());
        path = sanitizePath_(path);
        if (!path.length())
        {
            sendText_(request, 400, "text/plain", "Invalid path", set_cookie);
            return;
        }
        if (!LittleFS.exists(path))
        {
            if (_log && _log->ready())
                _log->warn(F("WEB"), F("Download missing %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
            sendText_(request, 404, "text/plain", "File not found", set_cookie);
            return;
        }
        if (_log && _log->ready())
            _log->info(F("WEB"), F("Download %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
        auto *response = request->beginResponse(LittleFS, path, "application/octet-stream");
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }

    void handleDelete_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie, true))
            return;
        if (!request->hasParam("path"))
        {
            sendText_(request, 400, "text/plain", "Missing path", set_cookie);
            return;
        }
        String path = sanitizePath_(request->getParam("path")->value());
        if (!path.length())
        {
            sendText_(request, 400, "text/plain", "Invalid path", set_cookie);
            return;
        }
        if (_log && _log->ready())
            _log->info(F("WEB"), F("Delete request %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
        if (!LittleFS.exists(path))
        {
            if (_log && _log->ready())
                _log->warn(F("WEB"), F("Delete missing %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
            sendText_(request, 404, "text/plain", "File not found", set_cookie);
            return;
        }
        if (!LittleFS.remove(path))
        {
            if (_log && _log->ready())
                _log->warn(F("WEB"), F("Delete failed %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
            sendText_(request, 500, "text/plain", "Delete failed", set_cookie);
            return;
        }
        sendRedirect_(request, "/manage", set_cookie);
    }

    void handleStatus_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /status (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceStatusHtml);
        page.replace("%NAV%", navHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        page.replace("%STATUS%", _last_status.length() ? _last_status : "No data");
        sendHtml_(request, page, set_cookie);
    }

    bool checkAuth_(AsyncWebServerRequest *request, bool *set_cookie, bool require_session = false)
    {
        if (set_cookie)
            *set_cookie = false;

        String token;
        if (extractSessionToken_(request, token) && sessionValid_(token))
        {
            refreshSession_();
            return true;
        }
        if (require_session)
        {
            sendText_(request, 403, "text/plain", "Session required", false);
            return false;
        }

        if (_cli_auth)
        {
            if (!_cli_auth->adminPasswordSet())
            {
                if (_log && _log->ready())
                    _log->warn(F("WEB"), F("Auth rejected (admin password not set, ip=%s, url=%s)"),
                               requestIp_(request).c_str(), request->url().c_str());
                sendRedirect_(request, "/admin", false);
                return false;
            }
            const bool auth_present = request->hasHeader("Authorization");
            size_t auth_len = 0;
            String auth_scheme;
            if (auth_present)
            {
                const AsyncWebHeader *h = request->getHeader("Authorization");
                if (h)
                {
                    String v = h->value();
                    auth_len = v.length();
                    const int sp = v.indexOf(' ');
                    auth_scheme = (sp > 0) ? v.substring(0, sp) : v;
                }
            }

            String user;
            String pass;
            if (parseBasicAuth_(request, user, pass))
            {
                String ulow = user;
                ulow.toLowerCase();
                if (ulow == CliConsole::kAdminUser &&
                _cli_auth->checkAdminPassword(pass))
                {
                    issueSession_();
                    if (set_cookie)
                        *set_cookie = true;
                    return true;
                }
            }
            if (_log && _log->ready())
                _log->warn(F("WEB"), F("Auth failed (ip=%s, url=%s)"),
                           requestIp_(request).c_str(),
                           request->url().c_str());
            requestBasicAuth_(request);
            return false;
        }
        if (!_auth_enabled)
            return true;
        if (request->authenticate(_auth_user.c_str(), _auth_pass.c_str()))
        {
            issueSession_();
            if (set_cookie)
                *set_cookie = true;
            return true;
        }
        if (_log && _log->ready())
            _log->warn(F("WEB"), F("Auth failed (ip=%s, url=%s)"), requestIp_(request).c_str(),
                       request->url().c_str());
        requestBasicAuth_(request);
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
        return String(" | STA: <strong>") + wifiStaStatus_() + "</strong>";
    }

    String wifiStaStatus_() const
    {
        switch (WiFi.status())
        {
        case WL_IDLE_STATUS:
            return "Idle";
        case WL_NO_SSID_AVAIL:
            return "SSID not found";
        case WL_SCAN_COMPLETED:
            return "Scan complete";
        case WL_CONNECTED:
            return "Connected";
        case WL_CONNECT_FAILED:
            return "Connect failed";
        case WL_CONNECTION_LOST:
            return "Connection lost";
        case WL_DISCONNECTED:
            return "Disconnected";
        default:
            return "Unknown";
        }
    }

    String navHtml_() const
    {
        String nav = F("<div class=\"nav\">");
        nav += F("<a href=\"/\">FCPLC</a> | <a href=\"/wifi\">Wi-Fi</a> | <a href=\"/manage\">Прошивка и файлы</a> | <a href=\"/ports\">Порты</a> | <a href=\"/buses\">Шины</a> | <a href=\"/stack\">Стек</a> | <a href=\"/sockets\">Розетки</a> | <a href=\"/telegram\">Telegram</a> | <a href=\"/admin\">Admin</a> | <a href=\"/logs\">Logs</a> | <a href=\"/logout\">Logout</a>");
        nav += F("</div>");
        return nav;
    }

    String deviceName_() const
    {
        if (_plc)
            return _plc->deviceName();
        return "";
    }

    ConfigsManagerIface::StackRole stackRole_() const
    {
        if (_configs_manager)
            return _configs_manager->stackRole();
        return ConfigsManagerIface::StackRole::Master;
    }

    String stackMasterHost_() const
    {
        if (_configs_manager)
            return _configs_manager->stackMasterHost();
        return "";
    }

    static const char *stackRoleName_(ConfigsManagerIface::StackRole role)
    {
        return (role == ConfigsManagerIface::StackRole::Master) ? "master" : "slave";
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

    static bool parseSocketPort_(const String &input, uint8_t &out)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "-" || t == "none")
        {
            out = SocketController::kInvalidPort;
            return true;
        }
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const unsigned long v = strtoul(t.c_str(), nullptr, 10);
        if (v > 255)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static String paramValue_(AsyncWebServerRequest *request, const String &name)
    {
        if (!request || !request->hasParam(name, true))
            return "";
        return request->getParam(name, true)->value();
    }

    static void appendHtmlEscaped_(String &out, const char *in)
    {
        if (!in)
            return;
        while (*in)
        {
            switch (*in)
            {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                out += *in;
                break;
            }
            ++in;
        }
    }

    static bool parseBasicAuth_(AsyncWebServerRequest *request, String &user, String &pass)
    {
        if (!request || !request->hasHeader("Authorization"))
            return false;
        const AsyncWebHeader *h = request->getHeader("Authorization");
        if (!h)
            return false;
        String value = h->value();
        String vlow = value;
        vlow.toLowerCase();
        if (!vlow.startsWith("basic"))
            return false;
        int sp = -1;
        for (size_t i = 5; i < value.length(); ++i)
        {
            const char c = value.charAt(i);
            if (c == ' ' || c == '\t')
            {
                sp = (int)i;
                break;
            }
        }
        if (sp < 0)
            return false;
        String b64 = value.substring(sp + 1);
        b64.trim();
        String decoded;
        if (!decodeBase64_(b64, decoded))
            return false;
        const int colon = decoded.indexOf(':');
        if (colon < 0)
            return false;
        user = decoded.substring(0, colon);
        pass = decoded.substring(colon + 1);
        return true;
    }

    static int8_t b64Index_(char c)
    {
        if (c >= 'A' && c <= 'Z')
            return (int8_t)(c - 'A');
        if (c >= 'a' && c <= 'z')
            return (int8_t)(26 + (c - 'a'));
        if (c >= '0' && c <= '9')
            return (int8_t)(52 + (c - '0'));
        if (c == '+' || c == '-')
            return 62;
        if (c == '/' || c == '_')
            return 63;
        return -1;
    }

    static bool decodeBase64_(const String &in, String &out)
    {
        out = "";
        out.reserve((in.length() * 3) / 4 + 1);
        uint32_t acc = 0;
        int bits = 0;
        for (size_t i = 0; i < in.length(); ++i)
        {
            const char c = in.charAt(i);
            if (c == ' ' || c == '\r' || c == '\n' || c == '\t')
                continue;
            if (c == '=')
                break;
            const int8_t v = b64Index_(c);
            if (v < 0)
                return false;
            acc = (acc << 6) | (uint32_t)v;
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                const uint8_t b = (uint8_t)((acc >> bits) & 0xFFu);
                out += (char)b;
            }
        }
        return true;
    }

    static void requestBasicAuth_(AsyncWebServerRequest *request)
    {
        if (!request)
            return;
        auto *response = request->beginResponse(401);
        response->addHeader("WWW-Authenticate", "Basic realm=\"FCPLC\"");
        request->send(response);
    }

    String sanitizeUploadName_(const String &filename) const
    {
        String name = filename;
        name.trim();
        if (!name.length() || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf("..") >= 0)
            return "";
        return String("/") + name;
    }

    String sanitizePath_(const String &path) const
    {
        String out = path;
        out.trim();
        if (!out.length())
            return "";
        if (!out.startsWith("/"))
            out = "/" + out;
        if (out.indexOf("..") >= 0 || out.indexOf('\\') >= 0)
            return "";
        return out;
    }

    float boardTemp_() const
    {
        return _plc ? _plc->boardTemp() : 0.0f;
    }

    float cpuTemp_() const
    {
        return _plc ? _plc->cpuTemp() : 0.0f;
    }

    String formatTemp_(float temp_c) const
    {
        char buf[16] = {};
        dtostrf(temp_c, 0, 2, buf);
        return String(buf) + " C";
    }

    String rtcTimeStr_() const
    {
        if (!_rtc)
            return String("n/a");
        Ds3231Mz::DateTime dt{};
        if (!_rtc->Time(dt))
            return String("n/a");
        char buf[24] = {};
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
                 (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day,
                 (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
        return String(buf);
    }

    String allowedUsersCsv_() const
    {
        if (!_tgbot_menu)
            return "";
        const auto &users = _tgbot_menu->allowedUsers();
        String out;
        for (size_t i = 0; i < users.size(); ++i)
        {
            if (i > 0)
                out += ", ";
            out += users[i];
        }
        return out;
    }

    static std::vector<String> splitCsv_(const String &input)
    {
        std::vector<String> out;
        String s = input;
        size_t start = 0;
        while (start < s.length())
        {
            int comma = s.indexOf(',', (int)start);
            if (comma < 0)
                comma = s.length();
            String token = s.substring(start, (size_t)comma);
            token.trim();
            if (token.length())
                out.push_back(token);
            start = (size_t)comma + 1;
        }
        return out;
    }

    float rtcTemp_() const
    {
        if (!_rtc)
            return 0.0f;
        float temp_c = 0.0f;
        if (!_rtc->readTemp(temp_c))
            return 0.0f;
        return temp_c;
    }

    const char *fanStatusStr_() const
    {
        if (!_plc)
            return "n/a";
        return _plc->fanStatus() ? "On" : "Off";
    }

    void sendHtml_(AsyncWebServerRequest *request, const String &page, bool set_cookie)
    {
        auto *response = request->beginResponse(200, "text/html", page);
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }

    void sendText_(AsyncWebServerRequest *request, int code, const char *type, const String &text, bool set_cookie)
    {
        auto *response = request->beginResponse(code, type, text);
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }

    void sendRedirect_(AsyncWebServerRequest *request, const char *path, bool set_cookie)
    {
        auto *response = request->beginResponse(302);
        response->addHeader("Location", path);
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }

    String sessionCookie_() const
    {
        String cookie = String("plc_session=") + _session_token +
                        "; Max-Age=" + String(_session_ttl_ms / 1000) +
                        "; Path=/; HttpOnly; SameSite=Strict";
        return cookie;
    }

    static String clearSessionCookie_()
    {
        return "plc_session=; Max-Age=0; Path=/; HttpOnly; SameSite=Strict";
    }

    static void appendHex_(String &out, uint32_t value)
    {
        char buf[9] = {};
        snprintf(buf, sizeof(buf), "%08lX", (unsigned long)value);
        out += buf;
    }

    static uint32_t rand32_()
    {
#if defined(ESP32)
        return esp_random();
#else
        uint32_t r = (uint32_t)random(0x7FFFFFFF);
        r = (r << 1) ^ (uint32_t)micros();
        return r;
#endif
    }

    String makeSessionToken_() const
    {
        String out;
        out.reserve(32);
        for (uint8_t i = 0; i < 4; ++i)
            appendHex_(out, rand32_());
        return out;
    }

    void issueSession_()
    {
        _session_token = makeSessionToken_();
        const uint32_t now = millis();
        _session_expire_ms = now + _session_ttl_ms;
    }

    void clearSession_()
    {
        _session_token = "";
        _session_expire_ms = 0;
    }

    void refreshSession_()
    {
        const uint32_t now = millis();
        _session_expire_ms = now + _session_ttl_ms;
    }

    bool sessionValid_(const String &token) const
    {
        if (_session_token.length() == 0)
            return false;
        const uint32_t now = millis();
        if ((int32_t)(now - _session_expire_ms) >= 0)
            return false;
        return token == _session_token;
    }

    bool extractSessionToken_(AsyncWebServerRequest *request, String &out) const
    {
        if (!request)
            return false;
        if (!request->hasHeader("Cookie"))
            return false;
        const AsyncWebHeader *hdr = request->getHeader("Cookie");
        if (!hdr)
            return false;
        String cookies = hdr->value();
        const String key = "plc_session=";
        int pos = cookies.indexOf(key);
        if (pos < 0)
            return false;
        int start = pos + key.length();
        int end = cookies.indexOf(';', start);
        if (end < 0)
            end = cookies.length();
        out = cookies.substring(start, end);
        out.trim();
        return out.length() > 0;
    }

    AsyncWebServer &_server;
    WifiManager &_wifi;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;
    PlcControl *_plc = nullptr;
    RTC *_rtc = nullptr;
    TelegramClient *_tgbot = nullptr;
    TelegramMenu *_tgbot_menu = nullptr;
    Controllers *_controllers = nullptr;
    I2CManager *_i2c = nullptr;
    OneWireManager *_ow = nullptr;
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
    String _tgbot_status;
    String _stack_status;
    String _device_status;
    String _sockets_status;
    bool _auth_enabled = false;
    String _auth_user;
    String _auth_pass;
    CliConsole *_cli_auth = nullptr;
    Extender *_ext = nullptr;
    Logger *_log = nullptr;
    String _session_token;
    uint32_t _session_expire_ms = 0;
    uint32_t _session_ttl_ms = 10u * 60u * 1000u;
    bool _upload_set_cookie = false;
    bool _ota_set_cookie = false;
};







