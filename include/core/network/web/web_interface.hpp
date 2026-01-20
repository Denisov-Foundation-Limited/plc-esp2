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
#include <string.h>

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
#include "core/network/web/pages/web_interface_controllers.hpp"
#include "core/network/web/pages/web_interface_meteo.hpp"
#include "core/network/web/pages/web_interface_thermo.hpp"
#include "core/network/web/pages/web_interface_tanks.hpp"
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
#include "core/network/stack/stack_master.hpp"
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
    void setStackMaster(StackMaster &master) { _stack_master = &master; }

    void registerRoutes()
    {
        _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { handleIndex_(request); });
        _server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) { handleWifi_(request); });
        _server.on("/manage", HTTP_GET, [this](AsyncWebServerRequest *request) { handleManage_(request); });
        _server.on("/controllers", HTTP_GET, [this](AsyncWebServerRequest *request) { handleControllers_(request); });
        _server.on("/controllers", HTTP_POST, [this](AsyncWebServerRequest *request) { handleControllersSave_(request); });
        _server.on("/ports", HTTP_GET, [this](AsyncWebServerRequest *request) { handlePorts_(request); });
        _server.on("/buses", HTTP_GET, [this](AsyncWebServerRequest *request) { handleBuses_(request); });
        _server.on("/stack", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStack_(request); });
        _server.on("/sockets", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSockets_(request); });
        _server.on("/sockets", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSocketsSave_(request); });
        _server.on("/meteo", HTTP_GET, [this](AsyncWebServerRequest *request) { handleMeteo_(request); });
        _server.on("/meteo", HTTP_POST, [this](AsyncWebServerRequest *request) { handleMeteoSave_(request); });
        _server.on("/thermo", HTTP_GET, [this](AsyncWebServerRequest *request) { handleThermo_(request); });
        _server.on("/thermo", HTTP_POST, [this](AsyncWebServerRequest *request) { handleThermoSave_(request); });
        _server.on("/tanks", HTTP_GET, [this](AsyncWebServerRequest *request) { handleTanks_(request); });
        _server.on("/tanks", HTTP_POST, [this](AsyncWebServerRequest *request) { handleTanksSave_(request); });
        _server.on("/telegram", HTTP_GET, [this](AsyncWebServerRequest *request) { handleTelegram_(request); });
        _server.on("/telegram", HTTP_POST, [this](AsyncWebServerRequest *request) { handleTelegramSave_(request); });
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
        String page = FPSTR(kWebInterfaceIndexHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", navHtml_());
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
        page.replace("%RTC_DATE%", rtcDateStr_());
        page.replace("%RTC_TIME%", rtcTimeOnlyStr_());
        page.replace("%RTC_TEMP%", formatTemp_(rtcTemp_()));
        page.replace("%FAN_STATUS_ICON%", fanStatusIcon_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleWifi_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceWifiHtml);
        page.reserve(page.length() + 1536);
        page.replace("%NAV%", navHtml_());
        page.replace("%WIFI_MODE%", _wifi.ap() ? "AP" : "STA");
        page.replace("%WIFI_CUR_SSID%", _wifi.ap() ? _wifi.apSsid() : _wifi.ssid());
        page.replace("%WIFI_IP%", wifiIp_());
        if (_wifi.ap())
        {
            page.replace("%WIFI_STA_ROW%", "");
        }
        else
        {
            String row = "<tr><td>STA</td><td><strong>";
            row += wifiStaStatus_();
            row += "</strong></td></tr>";
            page.replace("%WIFI_STA_ROW%", row);
        }
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
        String page = FPSTR(kWebInterfaceManageHtml);
        page.reserve(page.length() + 4096);
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
        String page = FPSTR(kWebInterfaceLogsHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", navHtml_());
        String lines;
        if (_log)
        {
            const size_t count = _log->recentCount();
            lines.reserve(count * 96 + 64);
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
        String page = FPSTR(kWebInterfaceAdminHtml);
        page.reserve(page.length() + 512);
        page.replace("%NAV%", navHtml_());
        page.replace("%ADMIN_STATUS%", (_cli_auth && _cli_auth->adminPasswordSet()) ? "установлен" : "не установлен");
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
        sendRedirect_(request, "/", set_cookie);
    }

    void handlePorts_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfacePortsHtml);
        page.reserve(page.length() + 2048);
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
        String page = FPSTR(kWebInterfaceBusesHtml);
        page.reserve(page.length() + 3072);
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
        String page = FPSTR(kWebInterfaceStackHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", navHtml_());
        const auto role = stackRole_();
        page.replace("%STACK_ROLE%", stackRoleName_(role));
        page.replace("%STACK_ROLE_MASTER_SEL%", role == ConfigsManagerIface::StackRole::Master ? "selected" : "");
        page.replace("%STACK_ROLE_SLAVE_SEL%", role == ConfigsManagerIface::StackRole::Slave ? "selected" : "");
        page.replace("%STACK_MASTER_HOST%", stackMasterHost_());
        page.replace("%STACK_STATUS%", _stack_status);
        if (role == ConfigsManagerIface::StackRole::Master)
        {
            String self = String("<p class=\"status\">Текущий контроллер: <strong>") + deviceName_() +
                          "</strong> | IP: <strong>" + wifiIp_() + "</strong></p>";
            page.replace("%STACK_SELF_BLOCK%", self);
            page.replace("%STACK_NODES_BLOCK%", stackNodesBlockHtml_());
        }
        else
        {
            page.replace("%STACK_SELF_BLOCK%", "");
            page.replace("%STACK_NODES_BLOCK%", "");
        }
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleControllers_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceControllersHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", navHtml_());
        if (_controllers)
        {
            const bool enabled = _controllers->sockets().controllerEnabled();
            page.replace("%SOCKETS_ENABLED_CHECKED%", enabled ? "checked" : "");
            page.replace("%SOCKETS_ENABLED_LABEL%", enabled ? "включены" : "выключены");
            const bool meteo_enabled = _controllers->meteo().controllerEnabled();
            page.replace("%METEO_ENABLED_CHECKED%", meteo_enabled ? "checked" : "");
            page.replace("%METEO_ENABLED_LABEL%", meteo_enabled ? "включено" : "выключено");
            const bool thermo_enabled = _controllers->thermo().controllerEnabled();
            page.replace("%THERMO_ENABLED_CHECKED%", thermo_enabled ? "checked" : "");
            page.replace("%THERMO_ENABLED_LABEL%", thermo_enabled ? "включено" : "выключено");
            const bool tanks_enabled = _controllers->tanks().controllerEnabled();
            page.replace("%TANKS_ENABLED_CHECKED%", tanks_enabled ? "checked" : "");
            page.replace("%TANKS_ENABLED_LABEL%", tanks_enabled ? "включены" : "выключены");
        }
        else
        {
            page.replace("%SOCKETS_ENABLED_CHECKED%", "");
            page.replace("%SOCKETS_ENABLED_LABEL%", "недоступно");
            page.replace("%METEO_ENABLED_CHECKED%", "");
            page.replace("%METEO_ENABLED_LABEL%", "недоступно");
            page.replace("%THERMO_ENABLED_CHECKED%", "");
            page.replace("%THERMO_ENABLED_LABEL%", "недоступно");
            page.replace("%TANKS_ENABLED_CHECKED%", "");
            page.replace("%TANKS_ENABLED_LABEL%", "недоступно");
        }
        page.replace("%CONTROLLERS_STATUS%", _controllers_status);
        page.replace("%METEO_STATUS%", _meteo_status);
        page.replace("%THERMO_STATUS%", _thermo_status);
        page.replace("%TANKS_STATUS%", _tanks_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleControllersSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            _controllers_status = "Контроллеры недоступны";
            sendRedirect_(request, "/controllers", set_cookie);
            return;
        }
        const String ctrl = paramValue_(request, "ctrl");
        bool changed = false;
        bool power_changed = false;
        if (ctrl.length() == 0 || ctrl == "sockets")
        {
            const bool enabled = request->hasParam("sockets_enabled", true);
            if (_controllers->sockets().controllerEnabled() != enabled)
            {
                _controllers->sockets().setControllerEnabled(enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "meteo")
        {
            const bool meteo_enabled = request->hasParam("meteo_enabled", true);
            if (_controllers->meteo().controllerEnabled() != meteo_enabled)
            {
                _controllers->meteo().setControllerEnabled(meteo_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "thermo")
        {
            const bool thermo_enabled = request->hasParam("thermo_enabled", true);
            if (_controllers->thermo().controllerEnabled() != thermo_enabled)
            {
                _controllers->thermo().setControllerEnabled(thermo_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "tanks")
        {
            const bool tanks_enabled = request->hasParam("tanks_enabled", true);
            if (_controllers->tanks().controllerEnabled() != tanks_enabled)
            {
                _controllers->tanks().setControllerEnabled(tanks_enabled);
                changed = true;
            }
        }
        bool ok = true;
        if (changed)
        {
            if (!_configs_manager)
            {
                ok = false;
                _controllers_status = "Config manager missing";
            }
            else if (!_configs_manager->save())
            {
                ok = false;
                _controllers_status = "Save failed";
            }
        }
        if (ok)
            _controllers_status = changed ? "Updated" : "No changes";
        _meteo_status = _controllers_status;
        _thermo_status = _controllers_status;
        _tanks_status = _controllers_status;
        sendRedirect_(request, "/controllers", set_cookie);
    }

    void handleSockets_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceSocketsHtml);
        page.reserve(page.length() + 8192);
        page.replace("%NAV%", navHtml_());
        page.replace("%SOCKETS%", listSocketsHtml_());
        page.replace("%DINPUT_JSON%", socketPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%RELAY_JSON%", socketPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%DINPUT_USED_JSON%", socketUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%RELAY_USED_JSON%", socketUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%SOCKETS_STATUS%", _sockets_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleMeteo_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceMeteoHtml);
        page.reserve(page.length() + 8192);
        page.replace("%NAV%", navHtml_());
        page.replace("%METEO_ROWS%", listMeteoHtml_());
        page.replace("%METEO_STATUS%", _meteo_status);
        page.replace("%SENSOR_JSON%", meteoPortOptionsJson_());
        page.replace("%SENSOR_USED_JSON%", meteoUsedPinsJson_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleThermo_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceThermoHtml);
        page.reserve(page.length() + 8192);
        page.replace("%NAV%", navHtml_());
        page.replace("%THERMO_ROWS%", listThermoHtml_());
        page.replace("%THERMO_STATUS%", _thermo_status);
        page.replace("%THERMO_DINPUT_JSON%", thermoPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%THERMO_RELAY_JSON%", thermoPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%THERMO_DINPUT_USED_JSON%", thermoUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%THERMO_RELAY_USED_JSON%", thermoUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleTelegram_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceTelegramHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", navHtml_());
        page.replace("%TGBOT_TOKEN%", _tgbot ? _tgbot->token() : String(""));
        page.replace("%TGBOT_CHAT_ID%", _tgbot ? String((long long)_tgbot->chatId()) : String("0"));
        page.replace("%TGBOT_INSECURE_CHECKED%", _tgbot && _tgbot->insecure() ? "checked" : "");
        page.replace("%TGBOT_CLIENT%", _tgbot ? _tgbot->clientKindName() : "none");
        page.replace("%TGBOT_USE_PROXY_CHECKED%", _tgbot && _tgbot->useProxy() ? "checked" : "");
        page.replace("%TGBOT_PROXY_HOST%", _tgbot ? _tgbot->proxyHost() : String(""));
        page.replace("%TGBOT_PROXY_PORT%", _tgbot ? String((unsigned)_tgbot->proxyPort()) : String("0"));
        page.replace("%TGBOT_PROXY_PATH%", _tgbot ? _tgbot->proxyPath() : String(""));
        page.replace("%TGBOT_ALLOWED_USERS_ROWS%", allowedUsersRowsHtml_());
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
            const auto *cfg = sockets.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("s") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String btn_key = prefix + "btn";
            const String relay_key = prefix + "relay";
            const String action_key = prefix + "action";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(btn_key, true) ||
                                 request->hasParam(relay_key, true) ||
                                 request->hasParam(action_key, true);
            if (!has_any)
                continue;
            const bool enabled = request->hasParam(en_key, true);
            String name = paramValue_(request, name_key);
            String btn = paramValue_(request, btn_key);
            String relay = paramValue_(request, relay_key);
            String action = paramValue_(request, action_key);
            name.trim();
            uint8_t btn_port = SocketController::kInvalidPort;
            uint8_t relay_port = SocketController::kInvalidPort;
            if (!parseSocketPort_(btn, btn_port) || !parseSocketPort_(relay, relay_port))
            {
                ok = false;
                _sockets_status = String("Invalid port for socket ") + idx;
                break;
            }
            if (cfg->name != name)
                sockets.setName(cfg->id, name);
            if (cfg->button_port != btn_port)
                sockets.setButtonPort(cfg->id, btn_port);
            if (cfg->relay_port != relay_port)
                sockets.setRelayPort(cfg->id, relay_port);
            if (cfg->enabled != enabled)
                sockets.setEnabled(cfg->id, enabled);
            if (action.length())
            {
                String act = action;
                act.toLowerCase();
                if (act == "on")
                {
                    sockets.setRelay(cfg->id, true);
                    changed = true;
                }
                else if (act == "off")
                {
                    sockets.setRelay(cfg->id, false);
                    changed = true;
                }
                else if (act == "toggle")
                {
                    sockets.toggleRelay(cfg->id);
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

    void handleMeteoSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        MeteoController &meteo = _controllers->meteo();
        bool ok = true;
        bool changed = false;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("m") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String type_key = prefix + "type";
            const String pin_key = prefix + "pin";
            const String addr_key = prefix + "addr";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(type_key, true) ||
                                 request->hasParam(pin_key, true) ||
                                 request->hasParam(addr_key, true);
            if (!has_any)
                continue;

            const bool enabled = request->hasParam(en_key, true);
            String name = paramValue_(request, name_key);
            name.trim();
            const String type_str = paramValue_(request, type_key);
            const String pin_str = paramValue_(request, pin_key);
            const String addr_str = paramValue_(request, addr_key);

            MeteoController::SensorType type = MeteoController::SensorType::None;
            if (!parseMeteoType_(type_str, type))
            {
                ok = false;
                _meteo_status = String("Invalid type for sensor ") + idx;
                break;
            }

            uint8_t pin = MeteoController::kInvalidPin;
            if (!parseMeteoPin_(pin_str, pin))
            {
                ok = false;
                _meteo_status = String("Invalid pin for sensor ") + idx;
                break;
            }

            uint8_t addr[MeteoController::kAddrLen] = {};
            bool addr_set = false;
            if (!parseMeteoAddr_(addr_str, addr, addr_set))
            {
                ok = false;
                _meteo_status = String("Invalid addr for sensor ") + idx;
                break;
            }

            if (cfg->enabled != enabled)
            {
                meteo.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->name != name)
            {
                meteo.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->type != type)
            {
                meteo.setType(cfg->id, type);
                changed = true;
            }
            if (type == MeteoController::SensorType::Dht22)
            {
                if (cfg->dht_pin != pin)
                {
                    meteo.setDht22Pin(cfg->id, pin);
                    changed = true;
                }
            }
            else if (type == MeteoController::SensorType::Ds18b20)
            {
                const bool addr_equal = (cfg->ds18_addr_set == addr_set) &&
                                        (!addr_set || (memcmp(cfg->ds18_addr, addr, MeteoController::kAddrLen) == 0));
                if (!addr_equal)
                {
                    meteo.setDs18b20Addr(cfg->id, addr, addr_set);
                    changed = true;
                }
            }
        }

        if (ok)
        {
            if (!_configs_manager)
            {
                ok = false;
                _meteo_status = "Config manager missing";
            }
            else if (changed && !_configs_manager->save())
            {
                ok = false;
                _meteo_status = "Save failed";
            }
        }
        if (ok)
            _meteo_status = changed ? "Updated" : "Saved";
        sendRedirect_(request, "/meteo", set_cookie);
    }

    void handleThermoSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        ThermoController &thermo = _controllers->thermo();
        uint8_t sensor_used[MeteoController::kSensorCount + 1] = {};
        bool ok = true;
        bool changed = false;
        bool power_changed = false;
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("t") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String sensor_key = prefix + "sensor";
            const String mode_key = prefix + "mode";
            const String target_key = prefix + "target";
            const String hyst_key = prefix + "hyst";
            const String heat_key = prefix + "heat";
            const String cool_key = prefix + "cool";
            const String button_key = prefix + "button";
            const String power_key = prefix + "power";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(sensor_key, true) ||
                                 request->hasParam(mode_key, true) ||
                                 request->hasParam(target_key, true) ||
                                 request->hasParam(hyst_key, true) ||
                                 request->hasParam(heat_key, true) ||
                                 request->hasParam(cool_key, true) ||
                                 request->hasParam(button_key, true) ||
                                 request->hasParam(power_key, true);
            if (!has_any)
                continue;

            const bool enabled = request->hasParam(en_key, true);
            const String sensor_str = paramValue_(request, sensor_key);
            const String mode_str = paramValue_(request, mode_key);
            const String target_str = paramValue_(request, target_key);
            const String hyst_str = paramValue_(request, hyst_key);
            const String heat_str = paramValue_(request, heat_key);
            const String cool_str = paramValue_(request, cool_key);
            const String button_str = paramValue_(request, button_key);
            const String power_str = paramValue_(request, power_key);
            String name = paramValue_(request, name_key);
            name.trim();
            const bool has_power = (power_str == "on" || power_str == "off" || power_str == "1" || power_str == "0" ||
                                    power_str == "true" || power_str == "false");
            const bool power_on = (power_str == "on" || power_str == "1" || power_str == "true");

            uint8_t sensor_id = ThermoController::kInvalidSensor;
            if (!parseThermoSensor_(sensor_str, sensor_id))
            {
                ok = false;
                _thermo_status = String("Invalid sensor for device ") + idx;
                break;
            }
            if (enabled && sensor_id != ThermoController::kInvalidSensor)
            {
                if (!isMeteoSensorActive_(sensor_id))
                {
                    ok = false;
                    _thermo_status = String("Датчик не активен (") + idx + ")";
                    break;
                }
                if (sensor_id <= MeteoController::kSensorCount && sensor_used[sensor_id])
                {
                    ok = false;
                    _thermo_status = String("Датчик уже используется (") + idx + ")";
                    break;
                }
                if (sensor_id <= MeteoController::kSensorCount)
                    sensor_used[sensor_id] = 1;
            }

            ThermoController::Mode mode = ThermoController::Mode::Off;
            if (!parseThermoMode_(mode_str, mode))
            {
                ok = false;
                _thermo_status = String("Invalid mode for device ") + idx;
                break;
            }

            float target = cfg->target_c;
            if (!parseThermoFloat_(target_str, target))
            {
                ok = false;
                _thermo_status = String("Invalid target for device ") + idx;
                break;
            }

            float hyst = cfg->hysteresis;
            if (!parseThermoFloat_(hyst_str, hyst))
            {
                ok = false;
                _thermo_status = String("Invalid hyst for device ") + idx;
                break;
            }

            uint8_t heat_port = ThermoController::kInvalidPort;
            uint8_t cool_port = ThermoController::kInvalidPort;
            uint8_t button_port = ThermoController::kInvalidPort;
            if (!parseSocketPort_(heat_str, heat_port) ||
                !parseSocketPort_(cool_str, cool_port) ||
                !parseSocketPort_(button_str, button_port))
            {
                ok = false;
                _thermo_status = String("Invalid port for device ") + idx;
                break;
            }

            if (cfg->enabled != enabled)
            {
                thermo.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->name != name)
            {
                thermo.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->sensor_id != sensor_id)
            {
                thermo.setSensor(cfg->id, sensor_id);
                changed = true;
            }
            if (cfg->mode != mode)
            {
                thermo.setMode(cfg->id, mode);
                changed = true;
            }
            if (cfg->target_c != target)
            {
                thermo.setTarget(cfg->id, target);
                changed = true;
            }
            if (cfg->hysteresis != hyst)
            {
                thermo.setHysteresis(cfg->id, hyst);
                changed = true;
            }
            if (cfg->heat_port != heat_port)
            {
                thermo.setHeatPort(cfg->id, heat_port);
                changed = true;
            }
            if (cfg->cool_port != cool_port)
            {
                thermo.setCoolPort(cfg->id, cool_port);
                changed = true;
            }
            if (cfg->button_port != button_port)
            {
                thermo.setButtonPort(cfg->id, button_port);
                changed = true;
            }
            if (has_power)
            {
                const auto *st = thermo.state(cfg->id);
                const bool cur_power = st ? st->power_on : true;
                if (cur_power != power_on)
                {
                    thermo.setPower(cfg->id, power_on, "web");
                    power_changed = true;
                }
            }
        }

        if (ok)
        {
            if (!_configs_manager)
            {
                ok = false;
                _thermo_status = "Config manager missing";
            }
            else if (changed && !_configs_manager->save())
            {
                ok = false;
                _thermo_status = "Save failed";
            }
        }
        if (ok)
            _thermo_status = (changed || power_changed) ? "Updated" : "Saved";
        sendRedirect_(request, "/thermo", set_cookie);
    }

    void handleTanks_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceTanksHtml);
        page.reserve(page.length() + 8192);
        page.replace("%NAV%", navHtml_());
        page.replace("%TANK_STATUS%", _tanks_status);
        page.replace("%TANK_ITEMS%", listTanksHtml_());
        page.replace("%TANK_DINPUT_JSON%", tankPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%TANK_RELAY_JSON%", tankPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%TANK_DINPUT_USED_JSON%", tankUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%TANK_RELAY_USED_JSON%", tankUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleTanksSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        TankController &tanks = _controllers->tanks();
        bool ok = true;
        bool changed = false;
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = tanks.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("k") + idx + "_";
            const String en_key = prefix + "en";
            const String power_key = prefix + "power";
            const String name_key = prefix + "name";
            const String low_key = prefix + "low";
            const String mid_key = prefix + "mid";
            const String full_key = prefix + "full";
            const String valve_key = prefix + "valve";
            const String pump_key = prefix + "pump";
            const String alarm_key = prefix + "alarm";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(power_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(low_key, true) ||
                                 request->hasParam(mid_key, true) ||
                                 request->hasParam(full_key, true) ||
                                 request->hasParam(valve_key, true) ||
                                 request->hasParam(pump_key, true) ||
                                 request->hasParam(alarm_key, true);
            if (!has_any)
                continue;

            const bool enabled = request->hasParam(en_key, true);
            const bool power_on = request->hasParam(power_key, true);
            String name = paramValue_(request, name_key);
            name.trim();
            const String low_str = paramValue_(request, low_key);
            const String mid_str = paramValue_(request, mid_key);
            const String full_str = paramValue_(request, full_key);
            const String valve_str = paramValue_(request, valve_key);
            const String pump_str = paramValue_(request, pump_key);
            const String alarm_str = paramValue_(request, alarm_key);

            uint8_t low_port = TankController::kInvalidPort;
            uint8_t mid_port = TankController::kInvalidPort;
            uint8_t full_port = TankController::kInvalidPort;
            uint8_t valve_port = TankController::kInvalidPort;
            uint8_t pump_port = TankController::kInvalidPort;
            uint8_t alarm_port = TankController::kInvalidPort;
            if (!parseSocketPort_(low_str, low_port) ||
                !parseSocketPort_(mid_str, mid_port) ||
                !parseSocketPort_(full_str, full_port) ||
                !parseSocketPort_(valve_str, valve_port) ||
                !parseSocketPort_(pump_str, pump_port) ||
                !parseSocketPort_(alarm_str, alarm_port))
            {
                ok = false;
                _tanks_status = String("Неверный порт для бака ") + idx;
                break;
            }

            if (cfg->enabled != enabled)
            {
                tanks.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->power_on != power_on)
            {
                tanks.setPower(cfg->id, power_on);
                changed = true;
            }
            if (cfg->name != name)
            {
                tanks.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->level_low != low_port)
            {
                tanks.setLevelLow(cfg->id, low_port);
                changed = true;
            }
            if (cfg->level_mid != mid_port)
            {
                tanks.setLevelMid(cfg->id, mid_port);
                changed = true;
            }
            if (cfg->level_full != full_port)
            {
                tanks.setLevelFull(cfg->id, full_port);
                changed = true;
            }
            if (cfg->relay_valve != valve_port)
            {
                tanks.setValveRelay(cfg->id, valve_port);
                changed = true;
            }
            if (cfg->relay_pump != pump_port)
            {
                tanks.setPumpRelay(cfg->id, pump_port);
                changed = true;
            }
            if (cfg->relay_alarm != alarm_port)
            {
                tanks.setAlarmRelay(cfg->id, alarm_port);
                changed = true;
            }
        }

        if (ok)
        {
            if (changed)
            {
                if (!_configs_manager)
                {
                    ok = false;
                    _tanks_status = "Менеджер конфигурации недоступен";
                }
                else if (!_configs_manager->save())
                {
                    ok = false;
                    _tanks_status = "Сохранение не удалось";
                }
            }
        }
        if (ok)
            _tanks_status = changed ? "Обновлено" : "Сохранено";
        sendRedirect_(request, "/tanks", set_cookie);
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

        if (_tgbot_menu)
        {
            std::vector<TelegramMenu::AllowedUser> users;
            String err;
            if (!parseAllowedUsers_(request, users, err))
            {
                _tgbot_status = err.length() ? err : "Invalid allowed users";
                sendRedirect_(request, "/telegram", set_cookie);
                return;
            }
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

    String listFilesHtml_()
    {
        String items;
        items.reserve(2048);
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
        items.reserve(2048);
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
            return "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></td></tr>";
        String items;
        items.reserve(4096);
        SocketController &sockets = _controllers->sockets();
        bool tmp_state = false;
        auto appendRow = [&](const SocketController::SocketConfig &cfg, bool enabled) {
            const bool on = enabled && sockets.relayState(cfg.id, tmp_state) ? tmp_state : false;
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)cfg.id);
            items += "</strong></td><td><input type=\"checkbox\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"></td><td><select class=\"field mini socket-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.button_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_btn\"></select></td><td><select class=\"field mini socket-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.relay_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_relay\"></select></td><td class=\"center\"><span class=\"status-dot ";
            items += on ? "status-on" : "status-off";
            items += "\"></span></td><td>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-toggle\" data-action=\"s";
            items += String((unsigned)cfg.id);
            items += "_action\"";
            if (on)
                items += " checked";
            if (!enabled)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "<input type=\"hidden\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_action\" value=\"\">";
            items += "</td></tr>";
        };

        const SocketController::SocketConfig *first_disabled = nullptr;
        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const auto *cfg = sockets.configByIndex(i);
            if (!cfg)
                continue;
            if (cfg->enabled)
            {
                appendRow(*cfg, true);
            }
            else if (!first_disabled)
            {
                first_disabled = cfg;
            }
        }
        if (first_disabled)
            appendRow(*first_disabled, false);
        if (items.length() == 0)
            items = "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Розетки отсутствуют</strong></td></tr>";
        return items;
    }


    String listMeteoHtml_()
    {
        if (!_controllers)
            return "<tr><td colspan=\"10\" style=\"color:#94a3b8\"><strong>Meteo unavailable</strong></td></tr>";
        String items;
        items.reserve(4096);
        MeteoController &meteo = _controllers->meteo();
        const uint32_t now = millis();

        auto appendTypeOption = [&](const char *value, const char *label, bool selected) {
            items += "<option value=\"";
            items += value;
            items += "\"";
            if (selected)
                items += " selected";
            items += ">";
            items += label;
            items += "</option>";
        };

        auto appendRow = [&](const MeteoController::SensorConfig &cfg, const MeteoController::SensorState &st,
                             bool enabled) {
            const bool has_read = st.last_read_ms != 0;
            char temp_buf[12] = {};
            char hum_buf[12] = {};
            char age_buf[16] = {};
            const char *temp = "--";
            const char *hum = "--";
            String ok = "<span class=\"status-dot status-na\" title=\"N/A\"></span>";
            const char *age = "-";

            if (st.has_temp)
            {
                dtostrf(st.temp_c, 0, 2, temp_buf);
                temp = temp_buf;
            }
            if (st.has_humidity)
            {
                dtostrf(st.humidity, 0, 1, hum_buf);
                hum = hum_buf;
            }
            if (has_read)
            {
                ok = st.ok ? "<span class=\"status-dot status-ok\" title=\"OK\"></span>"
                           : "<span class=\"status-dot status-err\" title=\"ERR\"></span>";
                const uint32_t age_s = (uint32_t)((now - st.last_read_ms) / 1000u);
                snprintf(age_buf, sizeof(age_buf), "%lus", (unsigned long)age_s);
                age = age_buf;
            }

            String pin;
            if (cfg.type == MeteoController::SensorType::Dht22 && cfg.dht_pin != MeteoController::kInvalidPin)
                pin = String((unsigned)cfg.dht_pin);

            String addr;
            if (cfg.type == MeteoController::SensorType::Ds18b20 && cfg.ds18_addr_set)
            {
                char hex[17] = {};
                MeteoController::formatHexAddr(cfg.ds18_addr, hex);
                addr = hex;
            }

            items += "<tr data-row=\"";
            items += String((unsigned)cfg.id);
            items += "\"><td class=\"right\"><strong>";
            items += String((unsigned)cfg.id);
            items += "</strong></td><td><input type=\"checkbox\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"></td><td><select class=\"field mini meteo-type\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_type\">";
            appendTypeOption("none", "none", cfg.type == MeteoController::SensorType::None);
            appendTypeOption("ds18b20", "ds18b20", cfg.type == MeteoController::SensorType::Ds18b20);
            appendTypeOption("dht22", "dht22", cfg.type == MeteoController::SensorType::Dht22);
            items += "</select></td><td class=\"pin-cell\"><select class=\"field mini meteo-pin\" data-selected=\"";
            items += pin;
            items += "\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_pin\"></select></td><td class=\"addr-cell\"><input class=\"field addr meteo-addr\" type=\"text\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_addr\" value=\"";
            items += addr;
            items += "\"></td><td class=\"right\">";
            items += temp;
            items += "</td><td class=\"right hum-cell\">";
            items += hum;
            items += "</td><td class=\"center\">";
            items += ok;
            items += "</td><td class=\"right\">";
            items += age;
            items += "</td></tr>";
        };

        const MeteoController::SensorConfig *first_disabled = nullptr;
        const MeteoController::SensorState *first_disabled_state = nullptr;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            const auto *st = meteo.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (cfg->enabled)
            {
                appendRow(*cfg, *st, true);
            }
            else if (!first_disabled)
            {
                first_disabled = cfg;
                first_disabled_state = st;
            }
        }
        if (first_disabled && first_disabled_state)
            appendRow(*first_disabled, *first_disabled_state, false);
        if (items.length() == 0)
            items = "<tr><td colspan=\"10\" style=\"color:#94a3b8\"><strong>Meteo empty</strong></td></tr>";
        return items;
    }

    String listThermoHtml_()
    {
        if (!_controllers)
            return "<tr><td colspan=\"12\" style=\"color:#94a3b8\"><strong>Thermo unavailable</strong></td></tr>";
        String items;
        items.reserve(4096);
        ThermoController &thermo = _controllers->thermo();
        uint8_t sensor_used[MeteoController::kSensorCount + 1] = {};

        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            if (cfg->sensor_id && cfg->sensor_id <= MeteoController::kSensorCount)
                sensor_used[cfg->sensor_id]++;
        }

        auto appendRow = [&](const ThermoController::DeviceConfig &cfg, const ThermoController::DeviceState &st,
                             bool enabled) {
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)cfg.id);
            items += "</strong></td><td><input type=\"checkbox\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"></td><td><select class=\"field mini\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_sensor\">";
            items += meteoSensorOptionsHtml_(cfg.sensor_id, sensor_used);
            items += "</select></td><td><select class=\"field mini\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_mode\">";
            items += "<option value=\"off\"";
            if (cfg.mode == ThermoController::Mode::Off)
                items += " selected";
            items += ">off</option>";
            items += "<option value=\"heat\"";
            if (cfg.mode == ThermoController::Mode::Heat)
                items += " selected";
            items += ">heat only</option>";
            items += "<option value=\"cool\"";
            if (cfg.mode == ThermoController::Mode::Cool)
                items += " selected";
            items += ">cool only</option>";
            items += "<option value=\"auto\"";
            if (cfg.mode == ThermoController::Mode::Auto)
                items += " selected";
            items += ">auto</option>";
            items += "</select></td><td class=\"right\"><input class=\"field temp\" type=\"text\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_target\" value=\"";
            items += String(cfg.target_c, 2);
            items += "\"></td><td class=\"right\"><input class=\"field temp\" type=\"text\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_hyst\" value=\"";
            items += String(cfg.hysteresis, 2);
            items += "\"></td><td class=\"right\"><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.heat_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.heat_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_heat\"></select></td><td class=\"right\"><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.cool_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.cool_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_cool\"></select></td><td class=\"right\"><select class=\"field mini thermo-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.button_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_button\"></select></td><td class=\"center\">";
            if (st.heat_on)
                items += "<span class=\"status-dot status-heat\" title=\"Heat\"></span>";
            else if (st.cool_on)
                items += "<span class=\"status-dot status-cool\" title=\"Cool\"></span>";
            else
                items += "<span class=\"status-dot status-idle\" title=\"Idle\"></span>";
            items += "</td><td class=\"center\">";
            const bool ui_power_on = enabled ? st.power_on : false;
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"thermo-power\" data-action=\"t";
            items += String((unsigned)cfg.id);
            items += "_power\"";
            if (ui_power_on)
                items += " checked";
            if (!enabled)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "<input type=\"hidden\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_power\" value=\"\"></td></tr>";
        };

        const ThermoController::DeviceConfig *first_disabled = nullptr;
        const ThermoController::DeviceState *first_disabled_state = nullptr;
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            const auto *st = thermo.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (cfg->enabled)
            {
                appendRow(*cfg, *st, true);
            }
            else if (!first_disabled)
            {
                first_disabled = cfg;
                first_disabled_state = st;
            }
        }
        if (first_disabled && first_disabled_state)
            appendRow(*first_disabled, *first_disabled_state, false);
        if (items.length() == 0)
            items = "<tr><td colspan=\"12\" style=\"color:#94a3b8\"><strong>Thermo empty</strong></td></tr>";
        return items;
    }

    String listTanksHtml_()
    {
        if (!_controllers)
            return "<tr><td colspan=\"11\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></td></tr>";
        String items;
        items.reserve(4096);
        TankController &tanks = _controllers->tanks();

        auto appendRow = [&](const TankController::TankConfig &cfg, const TankController::TankState &st,
                             bool enabled) {
            const char *level = "пусто";
            if (st.level_full)
                level = "полный";
            else if (st.level_mid)
                level = "средний";
            else if (st.level_low)
                level = "низкий";

            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)cfg.id);
            items += "</strong></td><td><input type=\"checkbox\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"></td><td class=\"right\"><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.level_low != TankController::kInvalidPort)
                items += String((unsigned)cfg.level_low);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_low\"></select></td><td class=\"right\"><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.level_mid != TankController::kInvalidPort)
                items += String((unsigned)cfg.level_mid);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_mid\"></select></td><td class=\"right\"><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.level_full != TankController::kInvalidPort)
                items += String((unsigned)cfg.level_full);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_full\"></select></td><td class=\"right\"><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_valve != TankController::kInvalidPort)
                items += String((unsigned)cfg.relay_valve);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_valve\"></select></td><td class=\"right\"><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_pump != TankController::kInvalidPort)
                items += String((unsigned)cfg.relay_pump);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_pump\"></select></td><td class=\"right\"><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_alarm != TankController::kInvalidPort)
                items += String((unsigned)cfg.relay_alarm);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_alarm\"></select></td><td class=\"center\">";
            items += level;
            items += "</td><td class=\"center\"><label class=\"switch\"><input type=\"checkbox\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_power\"";
            if (cfg.power_on)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></td></tr>";
        };

        const TankController::TankConfig *first_disabled = nullptr;
        const TankController::TankState *first_disabled_state = nullptr;
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = tanks.configByIndex(i);
            const auto *st = tanks.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (cfg->enabled)
            {
                appendRow(*cfg, *st, true);
            }
            else if (!first_disabled)
            {
                first_disabled = cfg;
                first_disabled_state = st;
            }
        }
        if (first_disabled && first_disabled_state)
            appendRow(*first_disabled, *first_disabled_state, false);
        if (items.length() == 0)
            items = "<tr><td colspan=\"11\" style=\"color:#94a3b8\"><strong>Баки отсутствуют</strong></td></tr>";
        return items;
    }

    String listI2cHtml_()
    {
        if (!_i2c)
            return "<tr><td colspan=\"2\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
        String items;
        items.reserve(1024);
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

    String stackNodesBlockHtml_() const
    {
        String out;
        out.reserve(1024);
        out += "<div class=\"section\">";
        out += "<h2>Контроллеры</h2>";
        out += "<table><thead><tr>";
        out += "<th>Unit</th><th>DeviceName</th><th>NodeID</th><th>IP</th>";
        out += "</tr></thead><tbody>";
        out += listStackNodesHtml_();
        out += "</tbody></table>";
        out += "</div>";
        return out;
    }

    String listStackNodesHtml_() const
    {
        if (!_stack_master)
            return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Стек недоступен</strong></td></tr>";
        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Контроллеров нет</strong></td></tr>";
        String items;
        items.reserve(1024);
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            const String name = _stack_master->nodeNameAt(i);
            const String ip = _stack_master->nodeIpAt(i);
            items += "<tr><td><strong>";
            if (name.length())
                appendHtmlEscaped_(items, name.c_str());
            else
                items += stackNodeIdHex_(id);
            items += "</strong></td><td>";
            if (name.length())
                appendHtmlEscaped_(items, name.c_str());
            else
                items += "-";
            items += "</td><td>";
            items += stackNodeIdHex_(id);
            items += "</td><td>";
            if (ip.length())
                appendHtmlEscaped_(items, ip.c_str());
            else
                items += "-";
            items += "</td></tr>";
        }
        return items;
    }

    String socketPortOptionsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = ActiveBoardProfile::PORTS[i];
            if (p.caps == Cap::None || p.type != type)
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
            if (!first)
                out += ",";
            out += String((unsigned)i);
            first = false;
        }
        out += "]";
        return out;
    }

    String socketUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            SocketController &sockets = _controllers->sockets();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = sockets.configByIndex(i);
                if (!cfg)
                    continue;
                const uint8_t btn = cfg->button_port;
                const uint8_t relay = cfg->relay_port;
                if (btn != SocketController::kInvalidPort && btn < PortIO::PORT_COUNT)
                    used[btn] = true;
                if (relay != SocketController::kInvalidPort && relay < PortIO::PORT_COUNT)
                    used[relay] = true;
            }
            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                if (!used[i])
                    continue;
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != type)
                    continue;
                if (!first)
                    out += ",";
                out += String((unsigned)i);
                first = false;
            }
        }
        out += "]";
        return out;
    }

    String meteoPortOptionsJson_() const
    {
        return socketPortOptionsJson_(PortIO::PinType::Sensor);
    }

    String meteoUsedPinsJson_() const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            MeteoController &meteo = _controllers->meteo();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = meteo.configByIndex(i);
                if (!cfg)
                    continue;
                if (cfg->type != MeteoController::SensorType::Dht22)
                    continue;
                const uint8_t pin = cfg->dht_pin;
                if (pin != MeteoController::kInvalidPin && pin < PortIO::PORT_COUNT)
                    used[pin] = true;
            }
            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                if (!used[i])
                    continue;
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != PortIO::PinType::Sensor)
                    continue;
                if (!first)
                    out += ",";
                out += String((unsigned)i);
                first = false;
            }
        }
        out += "]";
        return out;
    }

    String thermoPortOptionsJson_(PortIO::PinType type) const
    {
        return socketPortOptionsJson_(type);
    }

    String thermoUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            ThermoController &thermo = _controllers->thermo();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = thermo.configByIndex(i);
                if (!cfg)
                    continue;
                const uint8_t heat = cfg->heat_port;
                const uint8_t cool = cfg->cool_port;
                const uint8_t button = cfg->button_port;
                if (heat != ThermoController::kInvalidPort && heat < PortIO::PORT_COUNT)
                    used[heat] = true;
                if (cool != ThermoController::kInvalidPort && cool < PortIO::PORT_COUNT)
                    used[cool] = true;
                if (button != ThermoController::kInvalidPort && button < PortIO::PORT_COUNT)
                    used[button] = true;
            }
            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                if (!used[i])
                    continue;
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != type)
                    continue;
                if (!first)
                    out += ",";
                out += String((unsigned)i);
                first = false;
            }
        }
        out += "]";
        return out;
    }

    String tankPortOptionsJson_(PortIO::PinType type) const
    {
        return socketPortOptionsJson_(type);
    }

    String tankUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            TankController &tanks = _controllers->tanks();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = tanks.configByIndex(i);
                if (!cfg)
                    continue;
                if (type == PortIO::PinType::DInput)
                {
                    const uint8_t low = cfg->level_low;
                    const uint8_t mid = cfg->level_mid;
                    const uint8_t full = cfg->level_full;
                    if (low != TankController::kInvalidPort && low < PortIO::PORT_COUNT)
                        used[low] = true;
                    if (mid != TankController::kInvalidPort && mid < PortIO::PORT_COUNT)
                        used[mid] = true;
                    if (full != TankController::kInvalidPort && full < PortIO::PORT_COUNT)
                        used[full] = true;
                }
                else if (type == PortIO::PinType::Relay)
                {
                    const uint8_t valve = cfg->relay_valve;
                    const uint8_t pump = cfg->relay_pump;
                    const uint8_t alarm = cfg->relay_alarm;
                    if (valve != TankController::kInvalidPort && valve < PortIO::PORT_COUNT)
                        used[valve] = true;
                    if (pump != TankController::kInvalidPort && pump < PortIO::PORT_COUNT)
                        used[pump] = true;
                    if (alarm != TankController::kInvalidPort && alarm < PortIO::PORT_COUNT)
                        used[alarm] = true;
                }
            }
            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                if (!used[i])
                    continue;
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != type)
                    continue;
                if (!first)
                    out += ",";
                out += String((unsigned)i);
                first = false;
            }
        }
        out += "]";
        return out;
    }

    String listOwHtml_()
    {
        if (!_ow)
            return "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>none</strong></td></tr>";
        String items;
        items.reserve(1024);
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
sendRedirect_(request, "/status", _upload_set_cookie);
        _upload_set_cookie = false;
    }

    void handleOtaDone_(AsyncWebServerRequest *request)
    {
        if (!_ota_ok)
            _last_status = _ota_error.length() ? _ota_error : "Firmware update failed";
        else
            _last_status = "Firmware updated. Rebooting...";
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
        sendRedirect_(request, "/", set_cookie);
        delay(100);
        ESP.restart();
#else
        sendRedirect_(request, "/", set_cookie);
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
            sendText_(request, 404, "text/plain", "File not found", set_cookie);
            return;
        }
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
        if (!LittleFS.exists(path))
        {
            sendText_(request, 404, "text/plain", "File not found", set_cookie);
            return;
        }
        if (!LittleFS.remove(path))
        {
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
        String page = FPSTR(kWebInterfaceStatusHtml);
        page.reserve(page.length() + 2048);
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
        nav += F("<a href=\"/\">FCPLC</a> | <a href=\"/wifi\">Wi-Fi</a> | <a href=\"/manage\">Прошивка и файлы</a> | ");
        nav += F("<a href=\"/ports\">Порты</a> | <a href=\"/buses\">Шины</a> | <a href=\"/stack\">Стек</a> | ");
        nav += F("<a href=\"/controllers\">Контроллеры</a> | <a href=\"/telegram\">Telegram</a> | <a href=\"/admin\">Админка</a> | <a href=\"/logs\">Logs</a>");
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

    static bool parseMeteoType_(const String &input, MeteoController::SensorType &out)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "none")
        {
            out = MeteoController::SensorType::None;
            return true;
        }
        if (t == "ds18b20")
        {
            out = MeteoController::SensorType::Ds18b20;
            return true;
        }
        if (t == "dht22")
        {
            out = MeteoController::SensorType::Dht22;
            return true;
        }
        return false;
    }

    static bool parseMeteoPin_(const String &input, uint8_t &out)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "-" || t == "none")
        {
            out = MeteoController::kInvalidPin;
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

    static bool parseMeteoAddr_(const String &input, uint8_t out[MeteoController::kAddrLen], bool &set)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "-" || t == "none")
        {
            set = false;
            return true;
        }
        set = MeteoController::parseHexAddr(t.c_str(), out);
        return set;
    }

    static bool parseThermoSensor_(const String &input, uint8_t &out)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "-" || t == "none")
        {
            out = ThermoController::kInvalidSensor;
            return true;
        }
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const unsigned long v = strtoul(t.c_str(), nullptr, 10);
        if (v > MeteoController::kSensorCount)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseThermoMode_(const String &input, ThermoController::Mode &out)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "off" || t == "none")
        {
            out = ThermoController::Mode::Off;
            return true;
        }
        if (t == "heat" || t == "heat_only" || t == "only_heat")
        {
            out = ThermoController::Mode::Heat;
            return true;
        }
        if (t == "cool" || t == "cool_only" || t == "only_cool")
        {
            out = ThermoController::Mode::Cool;
            return true;
        }
        if (t == "auto")
        {
            out = ThermoController::Mode::Auto;
            return true;
        }
        return false;
    }

    static bool parseThermoFloat_(const String &input, float &out)
    {
        String t = input;
        t.trim();
        if (t.length() == 0)
            return false;
        const char *c = t.c_str();
        char *end = nullptr;
        const float v = strtof(c, &end);
        if (end == c)
            return false;
        out = v;
        return true;
    }

    String meteoSensorOptionsHtml_(uint8_t selected, const uint8_t used[MeteoController::kSensorCount + 1]) const
    {
        String out;
        out += "<option value=\"\">-</option>";
        if (!_controllers)
            return out;
        MeteoController &meteo = _controllers->meteo();
        for (uint8_t id = 1; id <= MeteoController::kSensorCount; ++id)
        {
            const auto *cfg = meteo.config(id);
            if (!cfg || !cfg->enabled)
                continue;
            const bool is_used = (id <= MeteoController::kSensorCount) && (used[id] > 0) && (id != selected);
            out += "<option value=\"";
            out += String((unsigned)id);
            out += "\"";
            if (id == selected)
                out += " selected";
            if (is_used)
                out += " disabled";
            out += ">";
            out += String((unsigned)id);
            if (is_used)
                out += " (занят)";
            out += "</option>";
        }
        return out;
    }

    bool isMeteoSensorActive_(uint8_t id) const
    {
        if (!_controllers)
            return false;
        const auto *cfg = _controllers->meteo().config(id);
        return cfg && cfg->enabled;
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

    String rtcDateStr_() const
    {
        if (!_rtc)
            return String("n/a");
        Ds3231Mz::DateTime dt{};
        if (!_rtc->Time(dt))
            return String("n/a");
        char buf[16] = {};
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u",
                 (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
        return String(buf);
    }

    String rtcTimeOnlyStr_() const
    {
        if (!_rtc)
            return String("n/a");
        Ds3231Mz::DateTime dt{};
        if (!_rtc->Time(dt))
            return String("n/a");
        char buf[16] = {};
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                 (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
        return String(buf);
    }

    String allowedUsersRowsHtml_() const
    {
        if (!_tgbot_menu)
            return "";
        String out;
        const auto &users = _tgbot_menu->allowedUsers();
        const size_t max = TelegramMenu::kMaxAllowedUsers;
        auto appendRow = [&](size_t row, const TelegramMenu::AllowedUser &u, bool enabled) {
            out += "<tr><td>";
            out += String((unsigned)(row + 1));
            out += "</td><td><input class=\"mini\" type=\"text\" name=\"au";
            out += String((unsigned)row);
            out += "_user\" value=\"";
            appendHtmlEscaped_(out, u.username.c_str());
            out += "\"></td><td><input class=\"mini\" type=\"text\" name=\"au";
            out += String((unsigned)row);
            out += "_chat\" value=\"";
            if (u.chat_id)
                out += String((long long)u.chat_id);
            out += "\"></td><td><input type=\"checkbox\" name=\"au";
            out += String((unsigned)row);
            out += "_admin\"";
            if (u.is_admin)
                out += " checked";
            out += "></td><td><input type=\"checkbox\" name=\"au";
            out += String((unsigned)row);
            out += "_notify\"";
            if (u.is_notify)
                out += " checked";
            out += "></td><td><input type=\"checkbox\" name=\"au";
            out += String((unsigned)row);
            out += "_enabled\"";
            if (enabled)
                out += " checked";
            out += "></td></tr>";
        };

        size_t row = 0;
        bool added_disabled = false;
        for (size_t i = 0; i < users.size() && row < max; ++i)
        {
            const auto &u = users[i];
            if (u.enabled)
            {
                appendRow(row++, u, true);
            }
            else if (!added_disabled)
            {
                appendRow(row++, u, false);
                added_disabled = true;
            }
        }
        if (!added_disabled && row < max)
        {
            TelegramMenu::AllowedUser empty{};
            appendRow(row++, empty, false);
        }
        return out;
    }

    static bool parseAllowedUsers_(AsyncWebServerRequest *request, std::vector<TelegramMenu::AllowedUser> &out,
                                   String &err)
    {
        out.clear();
        if (!request)
            return true;
        const size_t max = TelegramMenu::kMaxAllowedUsers;
        for (size_t i = 0; i < max; ++i)
        {
            const String user_key = String("au") + String((unsigned)i) + "_user";
            const String chat_key = String("au") + String((unsigned)i) + "_chat";
            const String admin_key = String("au") + String((unsigned)i) + "_admin";
            const String notify_key = String("au") + String((unsigned)i) + "_notify";
            const String enabled_key = String("au") + String((unsigned)i) + "_enabled";
            const bool has_any = request->hasParam(user_key, true) ||
                                 request->hasParam(chat_key, true) ||
                                 request->hasParam(admin_key, true) ||
                                 request->hasParam(notify_key, true) ||
                                 request->hasParam(enabled_key, true);
            if (!has_any)
                continue;
            String user = request->hasParam(user_key, true) ? request->getParam(user_key, true)->value() : "";
            String chat = request->hasParam(chat_key, true) ? request->getParam(chat_key, true)->value() : "";
            user.trim();
            chat.trim();
            TelegramMenu::AllowedUser u{};
            u.username = user;
            u.is_admin = request->hasParam(admin_key, true);
            u.is_notify = request->hasParam(notify_key, true);
            u.enabled = request->hasParam(enabled_key, true);
            if (chat.length())
            {
                const char *c = chat.c_str();
                for (size_t j = 0; c[j]; ++j)
                {
                    if (c[j] < '0' || c[j] > '9')
                    {
                        err = "Invalid chat id";
                        return false;
                    }
                }
                u.chat_id = (int64_t)strtoll(c, nullptr, 10);
            }
            out.push_back(u);
        }
        return true;
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

    String fanStatusIcon_() const
    {
        if (!_plc)
            return "n/a";
        const bool on = _plc->fanStatus();
        String out = "<span class=\"status-dot ";
        out += on ? "status-on" : "status-off";
        out += "\" title=\"";
        out += on ? "включен" : "выключен";
        out += "\"></span>";
        return out;
    }

    void sendHtml_(AsyncWebServerRequest *request, const String &page, bool set_cookie)
    {
        auto *response = request->beginResponse(200, "text/html; charset=utf-8", page);
        if (set_cookie)
            response->addHeader("Set-Cookie", sessionCookie_());
        request->send(response);
    }

    void sendText_(AsyncWebServerRequest *request, int code, const char *type, const String &text, bool set_cookie)
    {
        String content_type = type;
        if (content_type.startsWith("text/") && content_type.indexOf("charset=") < 0)
            content_type += "; charset=utf-8";
        auto *response = request->beginResponse(code, content_type, text);
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

    static void appendHex_(String &out, uint32_t value)
    {
        char buf[9] = {};
        snprintf(buf, sizeof(buf), "%08lX", (unsigned long)value);
        out += buf;
    }

    static String stackNodeIdHex_(uint32_t value)
    {
        String out = "0x";
        appendHex_(out, value);
        return out;
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
    String _controllers_status;
    String _meteo_status;
    String _thermo_status;
    String _tanks_status;
    bool _auth_enabled = false;
    String _auth_user;
    String _auth_pass;
    CliConsole *_cli_auth = nullptr;
    Extender *_ext = nullptr;
    Logger *_log = nullptr;
    StackMaster *_stack_master = nullptr;
    String _session_token;
    uint32_t _session_expire_ms = 0;
    uint32_t _session_ttl_ms = 10u * 60u * 1000u;
    bool _upload_set_cookie = false;
    bool _ota_set_cookie = false;
};










