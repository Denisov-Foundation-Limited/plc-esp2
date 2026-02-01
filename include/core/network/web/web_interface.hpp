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
#include "core/network/web/pages/web_interface_security.hpp"
#include "core/network/web/pages/web_interface_septic.hpp"
#include "core/network/web/pages/web_interface_meteo.hpp"
#include "core/network/web/pages/web_interface_thermo.hpp"
#include "core/network/web/pages/web_interface_tanks.hpp"
#include "core/network/web/pages/web_interface_admin.hpp"
#include "core/network/web/pages/web_interface_logs.hpp"
#include "core/network/web/pages/web_interface_telegram.hpp"
#include "core/network/web/pages/web_interface_status.hpp"
#include "core/network/web/pages/web_interface_sockets.hpp"
#include "core/network/web/pages/web_interface_lights.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/fs_config.hpp"
#include "utils/configs_manager_iface.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/gsm_modem.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/ibutton.hpp"
#include "controllers/controllers.hpp"

static const char kWebAutoRefreshScript[] PROGMEM = R"HTML(
<script>
(() => {
  const pollMs = 5000;
  const endpoint = '/ui/hash';
  let lastHash = '';
  const markDirty = () => { window.__plcDirty = true; };
  document.addEventListener('input', markDirty, true);
  document.addEventListener('change', markDirty, true);
  async function poll() {
    try {
      const path = location.pathname || '/';
      const res = await fetch(endpoint + '?path=' + encodeURIComponent(path), { cache: 'no-store' });
      if (!res.ok) {
        return;
      }
      const hash = (await res.text()).trim();
      if (!lastHash) {
        lastHash = hash;
        return;
      }
      if (hash && hash !== lastHash) {
        const active = document.activeElement;
        if (window.__plcDirty) {
          return;
        }
        if (active && (active.tagName === 'INPUT' || active.tagName === 'SELECT' || active.tagName === 'TEXTAREA')) {
          return;
        }
        location.reload();
      }
    } catch (e) {
    }
  }
  poll();
  setInterval(poll, pollMs);
})();
</script>
)HTML";

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

    void setGsmModem(GsmModem &modem) { _gsm = &modem; }
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
        _server.on("/stack/gen_key", HTTP_POST, [this](AsyncWebServerRequest *request) { handleStackGenKey_(request); });
        _server.on("/stack", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStack_(request); });
        _server.on("/sockets/toggle", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSocketsToggle_(request); });
        _server.on("/sockets/toggle", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSocketsToggle_(request); });
        _server.on("/sockets/enable", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSocketsEnable_(request); });
        _server.on("/sockets/enable", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSocketsEnable_(request); });
        _server.on("/sockets", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSockets_(request); });
        _server.on("/sockets", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSocketsSave_(request, "/sockets"); });
        _server.on("/lights/toggle", HTTP_POST, [this](AsyncWebServerRequest *request) { handleLightsToggle_(request); });
        _server.on("/lights/toggle", HTTP_GET, [this](AsyncWebServerRequest *request) { handleLightsToggle_(request); });
        _server.on("/lights/enable", HTTP_POST, [this](AsyncWebServerRequest *request) { handleLightsEnable_(request); });
        _server.on("/lights/enable", HTTP_GET, [this](AsyncWebServerRequest *request) { handleLightsEnable_(request); });
        _server.on("/lights", HTTP_GET, [this](AsyncWebServerRequest *request) { handleLights_(request); });
        _server.on("/lights", HTTP_POST, [this](AsyncWebServerRequest *request) { handleLightsSave_(request, "/lights"); });
        _server.on("/meteo", HTTP_GET, [this](AsyncWebServerRequest *request) { handleMeteo_(request); });
        _server.on("/meteo", HTTP_POST, [this](AsyncWebServerRequest *request) { handleMeteoSave_(request); });
        _server.on("/thermo", HTTP_GET, [this](AsyncWebServerRequest *request) { handleThermo_(request); });
        _server.on("/thermo", HTTP_POST, [this](AsyncWebServerRequest *request) { handleThermoSave_(request); });
        _server.on("/tanks", HTTP_GET, [this](AsyncWebServerRequest *request) { handleTanks_(request); });
        _server.on("/tanks", HTTP_POST, [this](AsyncWebServerRequest *request) { handleTanksSave_(request); });
        _server.on("/security", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSecurity_(request); });
        _server.on("/security", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSecuritySave_(request); });
        _server.on("/security/arm", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSecurityArm_(request); });
        _server.on("/septic", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSeptic_(request); });
        _server.on("/septic", HTTP_POST, [this](AsyncWebServerRequest *request) { handleSepticSave_(request); });
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
        _server.on("/ui/hash", HTTP_GET, [this](AsyncWebServerRequest *request) { handleUiHash_(request); });
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
        page.replace("%STACK_API_KEY%", stackApiKey_());
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
        page.replace("%GSM_STATUS%", _gsm_status);
        if (!_gsm)
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
            const bool enabled = available && _gsm->enabled();
            page.replace("%GSM_ENABLED_CHECKED%", enabled ? "checked" : "");
            page.replace("%GSM_ENABLED_LABEL%", available ? (enabled ? "включен" : "выключен") : "недоступен");
            page.replace("%GSM_STARTED_LABEL%", _gsm->started() ? "инициализирован" : "не инициализирован");
            page.replace("%GSM_IMEI%", safeHtmlValue_(_gsm->imei(), "n/a"));
            page.replace("%GSM_IMSI%", safeHtmlValue_(_gsm->imsi(), "n/a"));
            page.replace("%GSM_OPERATOR%", safeHtmlValue_(_gsm->operatorName(), "n/a"));
            page.replace("%GSM_SIGNAL%", safeHtmlValue_(_gsm->signalQuality(), "n/a"));
            page.replace("%GSM_REG_STATUS%", safeHtmlValue_(_gsm->regStatus(), "n/a"));
            page.replace("%GSM_LAST_ERROR%", safeHtmlValue_(_gsm->lastError(), "n/a"));
            page.replace("%GSM_LAST_URC%", safeHtmlValue_(_gsm->lastUrc(), "n/a"));
            page.replace("%GSM_LAST_SMS%", _gsm->lastSmsIndex() ? String(_gsm->lastSmsIndex()) : String("n/a"));
            page.replace("%GSM_LAST_CALL%", safeHtmlValue_(_gsm->lastCallNumber(), "n/a"));
            page.replace("%GSM_LAST_USSD%", safeHtmlValue_(_gsm->lastUssd(), "n/a"));
            page.replace("%GSM_HTTP_STATUS%", (_gsm->lastHttpStatus() >= 0) ? String(_gsm->lastHttpStatus()) : String("n/a"));
            page.replace("%GSM_HTTP_LEN%", (_gsm->lastHttpLen() >= 0) ? String(_gsm->lastHttpLen()) : String("n/a"));
        }
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
        page.replace("%STACK_API_KEY%", stackApiKey_());
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
            const bool lights_enabled = _controllers->sockets().lightsEnabled();
            page.replace("%LIGHTS_ENABLED_CHECKED%", lights_enabled ? "checked" : "");
            page.replace("%LIGHTS_ENABLED_LABEL%", lights_enabled ? "включены" : "выключены");
            const bool meteo_enabled = _controllers->meteo().controllerEnabled();
            page.replace("%METEO_ENABLED_CHECKED%", meteo_enabled ? "checked" : "");
            page.replace("%METEO_ENABLED_LABEL%", meteo_enabled ? "включено" : "выключено");
            const bool thermo_enabled = _controllers->thermo().controllerEnabled();
            page.replace("%THERMO_ENABLED_CHECKED%", thermo_enabled ? "checked" : "");
            page.replace("%THERMO_ENABLED_LABEL%", thermo_enabled ? "включено" : "выключено");
            const bool tanks_enabled = _controllers->tanks().controllerEnabled();
            page.replace("%TANKS_ENABLED_CHECKED%", tanks_enabled ? "checked" : "");
            page.replace("%TANKS_ENABLED_LABEL%", tanks_enabled ? "включены" : "выключены");
            const bool septic_enabled = _controllers->septic().controllerEnabled();
            page.replace("%SEPTIC_ENABLED_CHECKED%", septic_enabled ? "checked" : "");
            page.replace("%SEPTIC_ENABLED_LABEL%", septic_enabled ? "включены" : "выключены");
            const bool security_enabled = _controllers->security().controllerEnabled();
            page.replace("%SECURITY_ENABLED_CHECKED%", security_enabled ? "checked" : "");
            page.replace("%SECURITY_ENABLED_LABEL%", security_enabled ? "включена" : "выключена");
        }
        else
        {
            page.replace("%SOCKETS_ENABLED_CHECKED%", "");
            page.replace("%SOCKETS_ENABLED_LABEL%", "недоступно");
            page.replace("%LIGHTS_ENABLED_CHECKED%", "");
            page.replace("%LIGHTS_ENABLED_LABEL%", "недоступно");
            page.replace("%METEO_ENABLED_CHECKED%", "");
            page.replace("%METEO_ENABLED_LABEL%", "недоступно");
            page.replace("%THERMO_ENABLED_CHECKED%", "");
            page.replace("%THERMO_ENABLED_LABEL%", "недоступно");
            page.replace("%TANKS_ENABLED_CHECKED%", "");
            page.replace("%TANKS_ENABLED_LABEL%", "недоступно");
            page.replace("%SEPTIC_ENABLED_CHECKED%", "");
            page.replace("%SEPTIC_ENABLED_LABEL%", "недоступно");
            page.replace("%SECURITY_ENABLED_CHECKED%", "");
            page.replace("%SECURITY_ENABLED_LABEL%", "недоступно");
        }
        page.replace("%SOCKETS_STATUS%", _sockets_status);
        page.replace("%LIGHTS_STATUS%", _lights_status);
        page.replace("%METEO_STATUS%", _meteo_status);
        page.replace("%THERMO_STATUS%", _thermo_status);
        page.replace("%TANKS_STATUS%", _tanks_status);
        page.replace("%SEPTIC_STATUS%", _septic_status);
        page.replace("%SECURITY_STATUS%", _security_status);
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
        if (ctrl.length() == 0 || ctrl == "lights")
        {
            const bool lights_enabled = request->hasParam("lights_enabled", true);
            if (_controllers->sockets().lightsEnabled() != lights_enabled)
            {
                _controllers->sockets().setLightsEnabled(lights_enabled);
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
        if (ctrl.length() == 0 || ctrl == "septic")
        {
            const bool septic_enabled = request->hasParam("septic_enabled", true);
            if (_controllers->septic().controllerEnabled() != septic_enabled)
            {
                _controllers->septic().setControllerEnabled(septic_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "security")
        {
            const bool security_enabled = request->hasParam("security_enabled", true);
            if (_controllers->security().controllerEnabled() != security_enabled)
            {
                _controllers->security().setControllerEnabled(security_enabled);
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
        _sockets_status = _controllers_status;
        _lights_status = _controllers_status;
        _meteo_status = _controllers_status;
        _thermo_status = _controllers_status;
        _tanks_status = _controllers_status;
        _septic_status = _controllers_status;
        _security_status = _controllers_status;
        sendRedirect_(request, "/controllers", set_cookie);
    }

    void handleSockets_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceSocketsHtml);
        page.replace("%NAV%", navHtml_());
        const uint8_t page_size = 8u;
        const String page_str = paramValueAny_(request, "page");
        uint8_t page_idx = 0;
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (uint8_t)(v - 1);
        }
        const uint8_t max_pages = (uint8_t)((SocketController::kSocketCount + page_size - 1) / page_size);
        if (page_idx >= max_pages)
            page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        const uint8_t start = (uint8_t)(page_idx * page_size);
        const uint8_t end = (uint8_t)(start + page_size - 1);
        const size_t extra = 4096u + (size_t)page_size * 900u;
        page.reserve(page.length() + extra);
        page.replace("%SOCKETS%", listSocketsHtml_(start, end));
        page.replace("%SOCKETS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%SOCKETS_PAGES%", String((unsigned)max_pages));
        page.replace("%DINPUT_JSON%", socketPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%RELAY_JSON%", socketPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%DINPUT_USED_JSON%", socketUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%RELAY_USED_JSON%", socketUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%SOCKETS_STATUS%", _sockets_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtmlRaw_(request, page, set_cookie);
    }

    void handleLights_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceLightsHtml);
        const uint8_t page_size = 8u;
        const String page_str = paramValueAny_(request, "page");
        uint8_t page_idx = 0;
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (uint8_t)(v - 1);
        }
        const uint8_t max_pages = (uint8_t)((SocketController::kLightCount + page_size - 1) / page_size);
        if (page_idx >= max_pages)
            page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        const uint8_t start = (uint8_t)(page_idx * page_size + 1);
        const uint8_t end = (uint8_t)(start + page_size - 1);
        const size_t extra = 4096u + (size_t)page_size * 900u;
        page.reserve(page.length() + extra);
        page.replace("%NAV%", navHtml_());
        page.replace("%LIGHTS%", listLightsHtml_(start, end));
        page.replace("%LIGHTS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%LIGHTS_PAGES%", String((unsigned)max_pages));
        page.replace("%DINPUT_JSON%", socketPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%RELAY_JSON%", socketPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%DINPUT_USED_JSON%", socketUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%RELAY_USED_JSON%", socketUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%LIGHTS_STATUS%", _lights_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        sendHtml_(request, page, set_cookie);
    }

    void handleMeteo_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceMeteoHtml);
        page.reserve(page.length() + 16384);
        page.replace("%NAV%", navHtml_());
        page.replace("%METEO_TILES%", listMeteoHtml_());
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
        page.reserve(page.length() + 16384);
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
        page.replace("%TGBOT_LAST_CHAT_ID%", _tgbot ? String((long long)_tgbot->lastIncomingChatId()) : String("0"));
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

    void handleSocketsSave_(AsyncWebServerRequest *request, const char *redirect)
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
        sendRedirect_(request, redirect ? redirect : "/sockets", set_cookie);
    }

    void handleLightsSave_(AsyncWebServerRequest *request, const char *redirect)
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
        for (size_t i = 0; i < SocketController::kLightCount; ++i)
        {
            const auto *cfg = sockets.lightConfigByIndex(i);
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
                _lights_status = String("Invalid port for light ") + idx;
                break;
            }
            if (cfg->name != name)
                sockets.setLightName(cfg->id, name);
            if (cfg->button_port != btn_port)
                sockets.setLightButtonPort(cfg->id, btn_port);
            if (cfg->relay_port != relay_port)
                sockets.setLightRelayPort(cfg->id, relay_port);
            if (cfg->enabled != enabled)
                sockets.setLightEnabled(cfg->id, enabled);
            if (action.length())
            {
                String act = action;
                act.toLowerCase();
                if (act == "on")
                {
                    sockets.setLightRelay(cfg->id, true);
                    changed = true;
                }
                else if (act == "off")
                {
                    sockets.setLightRelay(cfg->id, false);
                    changed = true;
                }
                else if (act == "toggle")
                {
                    sockets.toggleLightRelay(cfg->id);
                    changed = true;
                }
            }
        }
        if (ok)
        {
            if (!_configs_manager)
            {
                ok = false;
                _lights_status = "Config manager missing";
            }
            else if (!_configs_manager->save())
            {
                ok = false;
                _lights_status = "Save failed";
            }
        }
        if (ok)
            _lights_status = changed ? "Updated" : "Saved";
        sendRedirect_(request, redirect ? redirect : "/lights", set_cookie);
    }

    void handleSocketsToggle_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuthApi_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        const String id_str = paramValueAny_(request, "id");
        if (!id_str.length())
        {
            String dbg = String("{\"ok\":false,\"err\":\"missing id\"");
            dbg += ",\"id\":\"\"";
            dbg += ",\"enabled\":\"\"";
            dbg += "}";
            sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        const uint16_t id = (uint16_t)id_str.toInt();
        SocketController &sockets = _controllers->sockets();
        if (id == 0 || !sockets.config(id))
        {
            sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        String action = paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        bool ok = false;
        bool state = false;
        if (action == "state")
        {
            ok = sockets.relayStateById(id, state);
        }
        else if (action.length() == 0 || action == "toggle")
        {
            ok = sockets.toggleRelayById(id);
            if (ok)
                ok = sockets.relayStateById(id, state);
        }
        else if (action == "on")
        {
            ok = sockets.setRelayById(id, true);
            if (ok)
                ok = sockets.relayStateById(id, state);
        }
        else if (action == "off")
        {
            ok = sockets.setRelayById(id, false);
            if (ok)
                ok = sockets.relayStateById(id, state);
        }
        if (!ok)
        {
            sendText_(request, 400, "text/plain", "Toggle failed", set_cookie);
            return;
        }
        sendText_(request, 200, "text/plain", state ? "on" : "off", set_cookie);
    }

    void handleSocketsEnable_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuthApi_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        const String id_str = paramValueAny_(request, "id");
        if (!id_str.length())
        {
            sendText_(request, 400, "text/plain", "Missing id", set_cookie);
            return;
        }
        const uint16_t id = (uint16_t)id_str.toInt();
        SocketController &sockets = _controllers->sockets();
        if (id == 0 || !sockets.config(id))
        {
            String dbg = String("{\"ok\":false,\"err\":\"invalid id\"");
            dbg += ",\"id\":\"" + id_str + "\"";
            dbg += ",\"enabled\":\"\"";
            dbg += "}";
            sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        const String enabled_str = paramValueAny_(request, "enabled");
        const bool enable = enabled_str == "1" || enabled_str == "true" || enabled_str == "on";
        if (!sockets.setEnabled(id, enable))
        {
            String dbg = String("{\"ok\":false,\"err\":\"enable failed\"");
            dbg += ",\"id\":\"" + id_str + "\"";
            dbg += ",\"enabled\":\"" + enabled_str + "\"";
            dbg += ",\"parsed\":" + String(enable ? "true" : "false");
            dbg += "}";
            sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        if (!_configs_manager)
        {
            _sockets_status = "Config manager missing";
        }
        else if (!_configs_manager->save())
        {
            _sockets_status = "Save failed";
        }
        String dbg = String("{\"ok\":true");
        dbg += ",\"id\":\"" + id_str + "\"";
        dbg += ",\"enabled\":\"" + enabled_str + "\"";
        dbg += ",\"parsed\":" + String(enable ? "true" : "false");
        dbg += ",\"result\":\"" + String(enable ? "1" : "0") + "\"";
        dbg += "}";
        sendText_(request, 200, "application/json", dbg, set_cookie);
    }

    void handleLightsToggle_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuthApi_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        const String id_str = paramValueAny_(request, "id");
        if (!id_str.length())
        {
            String dbg = String("{\"ok\":false,\"err\":\"missing id\"");
            dbg += ",\"id\":\"\"";
            dbg += ",\"enabled\":\"\"";
            dbg += "}";
            sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        const uint16_t id = (uint16_t)id_str.toInt();
        SocketController &sockets = _controllers->sockets();
        if (id == 0 || !sockets.lightConfig(id))
        {
            sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        String action = paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        bool ok = false;
        bool state = false;
        if (action == "state")
        {
            ok = sockets.lightRelayStateById(id, state);
        }
        else if (action.length() == 0 || action == "toggle")
        {
            ok = sockets.toggleLightRelayById(id);
            if (ok)
                ok = sockets.lightRelayStateById(id, state);
        }
        else if (action == "on")
        {
            ok = sockets.setLightRelayById(id, true);
            if (ok)
                ok = sockets.lightRelayStateById(id, state);
        }
        else if (action == "off")
        {
            ok = sockets.setLightRelayById(id, false);
            if (ok)
                ok = sockets.lightRelayStateById(id, state);
        }
        if (!ok)
        {
            sendText_(request, 400, "text/plain", "Toggle failed", set_cookie);
            return;
        }
        sendText_(request, 200, "text/plain", state ? "on" : "off", set_cookie);
    }

    void handleLightsEnable_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuthApi_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        const String id_str = paramValueAny_(request, "id");
        if (!id_str.length())
        {
            sendText_(request, 400, "text/plain", "Missing id", set_cookie);
            return;
        }
        const uint16_t id = (uint16_t)id_str.toInt();
        SocketController &sockets = _controllers->sockets();
        if (id == 0 || !sockets.lightConfig(id))
        {
            String dbg = String("{\"ok\":false,\"err\":\"invalid id\"");
            dbg += ",\"id\":\"" + id_str + "\"";
            dbg += ",\"enabled\":\"\"";
            dbg += "}";
            sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        const String enabled_str = paramValueAny_(request, "enabled");
        const bool enable = enabled_str == "1" || enabled_str == "true" || enabled_str == "on";
        if (!sockets.setLightEnabled(id, enable))
        {
            String dbg = String("{\"ok\":false,\"err\":\"enable failed\"");
            dbg += ",\"id\":\"" + id_str + "\"";
            dbg += ",\"enabled\":\"" + enabled_str + "\"";
            dbg += ",\"parsed\":" + String(enable ? "true" : "false");
            dbg += "}";
            sendText_(request, 400, "application/json", dbg, set_cookie);
            return;
        }
        if (!_configs_manager)
        {
            _lights_status = "Config manager missing";
        }
        else if (!_configs_manager->save())
        {
            _lights_status = "Save failed";
        }
        String dbg = String("{\"ok\":true");
        dbg += ",\"id\":\"" + id_str + "\"";
        dbg += ",\"enabled\":\"" + enabled_str + "\"";
        dbg += ",\"parsed\":" + String(enable ? "true" : "false");
        dbg += ",\"result\":\"" + String(enable ? "1" : "0") + "\"";
        dbg += "}";
        sendText_(request, 200, "application/json", dbg, set_cookie);
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
            const String en_force_key = prefix + "en_force";
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

            const String en_force_str = paramValue_(request, en_force_key);
            const bool en_force_known = (en_force_str == "1" || en_force_str == "0" ||
                                         en_force_str == "true" || en_force_str == "false" ||
                                         en_force_str == "on" || en_force_str == "off");
            const bool en_force_on = (en_force_str == "1" || en_force_str == "true" || en_force_str == "on");
            const bool enabled = en_force_known ? en_force_on : request->hasParam(en_key, true);
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
        page.reserve(page.length() + 16384);
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
            const String power_str = paramValue_(request, power_key);
            const bool power_on = (power_str == "on" || power_str == "1" || power_str == "true");
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

    void handleSeptic_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceSepticHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", navHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        page.replace("%SEPTIC_STATUS%", _septic_status);
        if (!_controllers)
        {
            page.replace("%SEPTIC_ITEMS%", "");
            page.replace("%SEPTIC_DINPUT_JSON%", "[]");
            page.replace("%SEPTIC_RELAY_JSON%", "[]");
            page.replace("%SEPTIC_DINPUT_USED_JSON%", "[]");
            page.replace("%SEPTIC_RELAY_USED_JSON%", "[]");
            page.replace("%SEPTIC_WARN_CLASS%", "status-off");
            page.replace("%SEPTIC_ALARM_CLASS%", "status-off");
            page.replace("%SEPTIC_RELAY_WARN_CLASS%", "status-off");
            page.replace("%SEPTIC_RELAY_ALARM_CLASS%", "status-off");
            page.replace("%SEPTIC_WARN_LABEL%", "off");
            page.replace("%SEPTIC_ALARM_LABEL%", "off");
            page.replace("%SEPTIC_RELAY_WARN_LABEL%", "off");
            page.replace("%SEPTIC_RELAY_ALARM_LABEL%", "off");
            page.replace("%SEPTIC_WATER_CLASS%", "water-low");
            page.replace("%SEPTIC_WATER_LEVEL%", "20%");
            page.replace("%SEPTIC_WATER_LABEL%", "Уровень: 20%");
            sendHtml_(request, page, set_cookie);
            return;
        }
        page.replace("%SEPTIC_ITEMS%", listSepticHtml_());
        page.replace("%SEPTIC_DINPUT_JSON%", septicPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%SEPTIC_RELAY_JSON%", septicPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%SEPTIC_DINPUT_USED_JSON%", septicUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%SEPTIC_RELAY_USED_JSON%", septicUsedPortsJson_(PortIO::PinType::Relay));
        {
            SepticController &septic = _controllers->septic();
            const auto *cfg = septic.configByIndex(0);
            const auto *st = septic.stateByIndex(0);
            const bool warn = st ? st->warning : false;
            const bool alarm = st ? st->alarm : false;
            const bool relay_warn = st ? st->relay_warning : false;
            const bool relay_alarm = st ? st->relay_alarm : false;
            const char *water_class = "water-low";
            const char *water_level = "20%";
            const char *water_label = "Уровень: 20%";
            if (alarm)
            {
                water_class = "water-alarm";
                water_level = "100%";
                water_label = "Уровень: 100%";
            }
            else if (warn)
            {
                water_class = "water-warn";
                water_level = "80%";
                water_label = "Уровень: 80%";
            }
            page.replace("%SEPTIC_WARN_CLASS%", warn ? "status-on" : "status-off");
            page.replace("%SEPTIC_ALARM_CLASS%", alarm ? "status-on" : "status-off");
            page.replace("%SEPTIC_RELAY_WARN_CLASS%", relay_warn ? "status-on" : "status-off");
            page.replace("%SEPTIC_RELAY_ALARM_CLASS%", relay_alarm ? "status-on" : "status-off");
            page.replace("%SEPTIC_WARN_LABEL%", warn ? "on" : "off");
            page.replace("%SEPTIC_ALARM_LABEL%", alarm ? "on" : "off");
            page.replace("%SEPTIC_RELAY_WARN_LABEL%", relay_warn ? "on" : "off");
            page.replace("%SEPTIC_RELAY_ALARM_LABEL%", relay_alarm ? "on" : "off");
            page.replace("%SEPTIC_WATER_CLASS%", water_class);
            page.replace("%SEPTIC_WATER_LEVEL%", water_level);
            page.replace("%SEPTIC_WATER_LABEL%", water_label);
            if (!cfg || !cfg->enabled)
                page.replace("%SEPTIC_STATUS%", "Септик выключен");
        }
        sendHtml_(request, page, set_cookie);
    }

    void handleSecurity_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceSecurityHtml);
        const uint8_t page_size = 8u;
        const String page_str = paramValueAny_(request, "page");
        uint8_t page_idx = 0;
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (uint8_t)(v - 1);
        }
        const uint8_t max_pages = (uint8_t)((SecurityController::kSensorCount + page_size - 1) / page_size);
        if (page_idx >= max_pages)
            page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        const uint8_t start = (uint8_t)(page_idx * page_size);
        const uint8_t end = (uint8_t)(start + page_size - 1);
        page.reserve(page.length() + 16384);
        page.replace("%NAV%", navHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        if (!_controllers)
        {
            page.replace("%SECURITY_ENABLED_CHECKED%", "");
            page.replace("%SECURITY_ENABLED_LABEL%", "недоступно");
            page.replace("%SECURITY_ARMED_LABEL%", "недоступно");
            page.replace("%SECURITY_ARMED_CHECKED%", "");
            page.replace("%SECURITY_ALARM_LABEL%", "недоступно");
            page.replace("%SECURITY_GSM_LABEL%", gsmStatusLabel_());
            page.replace("%SECURITY_SIREN%", "");
            page.replace("%SECURITY_KEYS_ROWS%", "");
            page.replace("%SECURITY_PHONES_ROWS%", "");
            page.replace("%SECURITY_SENSORS%", "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>");
            page.replace("%SECURITY_SENSORS_PAGE%", "1");
            page.replace("%SECURITY_SENSORS_PAGES%", String((unsigned)(max_pages ? max_pages : 1)));
            page.replace("%SECURITY_SENSOR_JSON%", "[]");
            page.replace("%SECURITY_SENSOR_USED_JSON%", "[]");
            page.replace("%SECURITY_SIREN_JSON%", "[]");
            page.replace("%SECURITY_SIREN_USED_JSON%", "[]");
            page.replace("%SECURITY_STATUS%", _security_status);
            sendHtml_(request, page, set_cookie);
            return;
        }

        SecurityController &sec = _controllers->security();
        page.replace("%SECURITY_ENABLED_CHECKED%", sec.controllerEnabled() ? "checked" : "");
        page.replace("%SECURITY_ENABLED_LABEL%", sec.controllerEnabled() ? "включено" : "выключено");
        page.replace("%SECURITY_ARMED_LABEL%", sec.armed() ? "под охраной" : "снято");
        page.replace("%SECURITY_ARMED_CHECKED%", sec.armed() ? "checked" : "");
        page.replace("%SECURITY_ALARM_LABEL%", sec.alarmOn() ? "on" : "off");
        page.replace("%SECURITY_GSM_LABEL%", gsmStatusLabel_());
        if (sec.sirenPort() != SecurityController::kInvalidPort)
            page.replace("%SECURITY_SIREN%", String((unsigned)sec.sirenPort()));
        else
            page.replace("%SECURITY_SIREN%", "");
        page.replace("%SECURITY_KEYS_ROWS%", listSecurityKeysHtml_());
        page.replace("%SECURITY_PHONES_ROWS%", listSecurityPhonesHtml_());
        page.replace("%SECURITY_SENSORS%", listSecuritySensorsTiles_(start, end));
        page.replace("%SECURITY_SENSORS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%SECURITY_SENSORS_PAGES%", String((unsigned)max_pages));
        page.replace("%SECURITY_SENSOR_JSON%", securityPortOptionsJson_());
        page.replace("%SECURITY_SENSOR_USED_JSON%", securityUsedPinsJson_());
        page.replace("%SECURITY_SIREN_JSON%", socketPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%SECURITY_SIREN_USED_JSON%", socketUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%SECURITY_STATUS%", _security_status);
        sendHtml_(request, page, set_cookie);
    }

    void handleSepticSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        SepticController &septic = _controllers->septic();
        bool ok = true;
        bool changed = false;
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = septic.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("sep") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String warn_key = prefix + "warn";
            const String alarm_key = prefix + "alarm";
            const String relay_warn_key = prefix + "relay_warn";
            const String relay_alarm_key = prefix + "relay_alarm";
            const String monitor_key = prefix + "mon";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(warn_key, true) ||
                                 request->hasParam(alarm_key, true) ||
                                 request->hasParam(relay_warn_key, true) ||
                                 request->hasParam(relay_alarm_key, true) ||
                                 request->hasParam(monitor_key, true);
            if (!has_any)
                continue;
            const bool enabled = request->hasParam(en_key, true);
            const String monitor_val = paramValue_(request, monitor_key);
            const bool monitoring = monitor_val == "on" || monitor_val == "1" || monitor_val == "true";
            String name = paramValue_(request, name_key);
            String warn = paramValue_(request, warn_key);
            String alarm = paramValue_(request, alarm_key);
            String relay_warn = paramValue_(request, relay_warn_key);
            String relay_alarm = paramValue_(request, relay_alarm_key);
            name.trim();
            uint8_t warn_port = SepticController::kInvalidPort;
            uint8_t alarm_port = SepticController::kInvalidPort;
            uint8_t relay_warn_port = SepticController::kInvalidPort;
            uint8_t relay_alarm_port = SepticController::kInvalidPort;
            if (!parseSocketPort_(warn, warn_port) ||
                !parseSocketPort_(alarm, alarm_port) ||
                !parseSocketPort_(relay_warn, relay_warn_port) ||
                !parseSocketPort_(relay_alarm, relay_alarm_port))
            {
                ok = false;
                _septic_status = String("Invalid port for septic ") + idx;
                break;
            }
            if (cfg->name != name)
                septic.setName(cfg->id, name);
            if (cfg->warning_port != warn_port)
                septic.setWarningPort(cfg->id, warn_port);
            if (cfg->alarm_port != alarm_port)
                septic.setAlarmPort(cfg->id, alarm_port);
            if (cfg->relay_warning != relay_warn_port)
                septic.setWarningRelay(cfg->id, relay_warn_port);
            if (cfg->relay_alarm != relay_alarm_port)
                septic.setAlarmRelay(cfg->id, relay_alarm_port);
            if (cfg->enabled != enabled)
                septic.setEnabled(cfg->id, enabled);
            if (cfg->monitoring_on != monitoring)
                septic.setMonitoring(cfg->id, monitoring);
            changed = true;
        }
        if (ok)
        {
            if (!_configs_manager)
            {
                ok = false;
                _septic_status = "Config manager missing";
            }
            else if (changed && !_configs_manager->save())
            {
                ok = false;
                _septic_status = "Save failed";
            }
        }
        if (ok)
            _septic_status = changed ? "Updated" : "Saved";
        sendRedirect_(request, "/septic", set_cookie);
    }

    void handleSecuritySave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            _security_status = "Security unavailable";
            sendRedirect_(request, "/security", set_cookie);
            return;
        }
        SecurityController &sec = _controllers->security();
        const String action = paramValue_(request, "action");
        if (action == "arm")
        {
            if (sec.armFrom("web", "admin"))
                _security_status = "Armed";
            else
                _security_status = "Security disabled";
            sendRedirect_(request, "/security", set_cookie);
            return;
        }
        if (action == "disarm")
        {
            sec.disarmFrom("web", "admin");
            _security_status = "Disarmed";
            sendRedirect_(request, "/security", set_cookie);
            return;
        }
        if (action == "clear")
        {
            sec.clearDetect();
            _security_status = "Detections cleared";
            sendRedirect_(request, "/security", set_cookie);
            return;
        }

        bool changed = false;
        if (request->hasParam("security_enabled", true))
        {
            const bool enabled = request->hasParam("security_enabled", true);
            if (sec.controllerEnabled() != enabled)
            {
                sec.setControllerEnabled(enabled);
                changed = true;
            }
        }

        String siren_str = paramValue_(request, "security_siren");
        siren_str.trim();
        if (siren_str.length() > 0)
        {
            uint8_t siren_port = sec.sirenPort();
            bool set_siren = false;
            if (siren_str == "none")
            {
                siren_port = SecurityController::kInvalidPort;
                set_siren = true;
            }
            else
            {
                const int v = siren_str.toInt();
                if (v >= 0 && v <= 255)
                {
                    siren_port = (uint8_t)v;
                    set_siren = true;
                }
            }
            if (set_siren && sec.sirenPort() != siren_port)
            {
                sec.setSirenPort(siren_port);
                changed = true;
            }
        }


        uint8_t new_keys[SecurityController::kKeyCount][8] = {};
        bool new_set[SecurityController::kKeyCount] = {};
        String new_key_names[SecurityController::kKeyCount];
        for (size_t i = 0; i < SecurityController::kKeyCount; ++i)
        {
            const String idx = String((unsigned)(i + 1));
            const String en_key = String("k") + idx + "_en";
            const String serial_key = String("k") + idx + "_serial";
            const String name_key = String("k") + idx + "_name";
            const bool enabled = request->hasParam(en_key, true);
            String serial = paramValue_(request, serial_key);
            serial.trim();
            String name = paramValue_(request, name_key);
            name.trim();
            if (!enabled)
            {
                new_key_names[i] = name;
                continue;
            }
            if (!parseSecurityKeyHex_(serial, new_keys[i]))
            {
                _security_status = String("Invalid key ") + idx;
                sendRedirect_(request, "/security", set_cookie);
                return;
            }
            new_set[i] = true;
            new_key_names[i] = name;
        }

        bool keys_changed = false;
        for (size_t i = 0; i < SecurityController::kKeyCount; ++i)
        {
            uint8_t old_addr[8] = {};
            bool old_enabled = false;
            sec.keySlot(i, old_addr, old_enabled);
            if (old_enabled != new_set[i])
            {
                keys_changed = true;
                break;
            }
            if (old_enabled && memcmp(old_addr, new_keys[i], 8) != 0)
            {
                keys_changed = true;
                break;
            }
            if (sec.keyNameByIndex(i) != new_key_names[i])
            {
                keys_changed = true;
                break;
            }
        }
        if (keys_changed)
        {
            for (size_t i = 0; i < SecurityController::kKeyCount; ++i)
                sec.setKeySlot(i, new_keys[i], new_set[i], new_key_names[i]);
            changed = true;
        }

        auto normalizePhone = [](const String &number) -> String {
            String out;
            out.reserve(number.length());
            for (size_t i = 0; i < number.length(); ++i)
            {
                const char c = number.charAt(i);
                if (c >= '0' && c <= '9')
                    out += c;
            }
            return out;
        };
        for (size_t i = 0; i < SecurityController::kPhoneCount; ++i)
        {
            const String idx = String((unsigned)(i + 1));
            const String en_key = String("p") + idx + "_en";
            const String num_key = String("p") + idx + "_num";
            const String name_key = String("p") + idx + "_name";
            const String notify_key = String("p") + idx + "_notify";
            const String call_key = String("p") + idx + "_call";
            const bool enabled = request->hasParam(en_key, true);
            const bool notify = request->hasParam(notify_key, true);
            const bool call = request->hasParam(call_key, true);
            String number = paramValue_(request, num_key);
            number.trim();
            String name = paramValue_(request, name_key);
            name.trim();
            String norm = enabled ? normalizePhone(number) : String();
            if (enabled && norm.length() == 0)
            {
                _security_status = String("Invalid phone ") + idx;
                sendRedirect_(request, "/security", set_cookie);
                return;
            }
            String old_number;
            bool old_enabled = false;
            sec.phoneSlot(i, old_number, old_enabled);
            if (old_enabled != enabled || old_number != norm ||
                sec.phoneNameByIndex(i) != name || sec.phoneNotifyByIndex(i) != notify ||
                sec.phoneCallByIndex(i) != call)
            {
                sec.setPhone(i, norm);
                sec.setPhoneEnabled(i, enabled);
                sec.setPhoneName(i, name);
                sec.setPhoneNotify(i, notify);
                sec.setPhoneCall(i, call);
                changed = true;
            }
        }

        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = sec.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("sec") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String type_key = prefix + "type";
            const String port_key = prefix + "port";
            const String silent_key = prefix + "silent";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(type_key, true) ||
                                 request->hasParam(port_key, true) ||
                                 request->hasParam(silent_key, true);
            if (!has_any)
                continue;
            const bool enabled = request->hasParam(en_key, true);
            const bool silent = request->hasParam(silent_key, true);
            String name = paramValue_(request, name_key);
            name.trim();
            SecurityController::SensorType type = SecurityController::SensorType::Pir;
            if (!parseSecurityType_(paramValue_(request, type_key), type))
            {
                _security_status = String("Invalid type for sensor ") + idx;
                sendRedirect_(request, "/security", set_cookie);
                return;
            }
            uint8_t port = SecurityController::kInvalidPort;
            if (!parseSocketPort_(paramValue_(request, port_key), port))
            {
                _security_status = String("Invalid port for sensor ") + idx;
                sendRedirect_(request, "/security", set_cookie);
                return;
            }
            if (cfg->enabled != enabled)
            {
                sec.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->name != name)
            {
                sec.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->type != type)
            {
                sec.setType(cfg->id, type);
                changed = true;
            }
            if (cfg->port != port)
            {
                sec.setPort(cfg->id, port);
                changed = true;
            }
            if (cfg->silent != silent)
            {
                sec.setSilent(cfg->id, silent);
                changed = true;
            }
        }

        bool ok = true;
        if (changed)
        {
            if (!_configs_manager)
            {
                ok = false;
                _security_status = "Config manager missing";
            }
            else if (!_configs_manager->save())
            {
                ok = false;
                _security_status = "Save failed";
            }
        }
        if (ok)
            _security_status = changed ? "Updated" : "No changes";
        sendRedirect_(request, "/security", set_cookie);
    }

    void handleSecurityArm_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_controllers)
        {
            sendText_(request, 500, "text/plain", "0", set_cookie);
            return;
        }
        SecurityController &sec = _controllers->security();
        const String armed_str = paramValueAny_(request, "armed");
        if (armed_str.length() == 0)
        {
            _security_status = "Bad request";
            sendText_(request, 400, "text/plain", "err", set_cookie);
            return;
        }
        const bool desired = (armed_str == "1" || armed_str == "true" || armed_str == "on");
        bool ok = true;
        if (sec.armed() != desired)
        {
            if (desired)
            {
                if (!sec.controllerEnabled())
                    sec.setControllerEnabled(true);
                ok = sec.armFrom("web", "admin");
            }
            else
            {
                sec.disarmFrom("web", "admin");
                ok = true;
            }
        }
        if (!ok)
            _security_status = "Security disabled";
        if (desired && !sec.armed())
        {
            _security_status = "Arm blocked";
            sendText_(request, 200, "text/plain", "blocked", set_cookie);
        }
        else
        {
            sendText_(request, 200, "text/plain", sec.armed() ? "1" : "0", set_cookie);
        }
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

    static uint8_t locationIndex_(PortIO::Location loc)
    {
        switch (loc)
        {
        case PortIO::Location::Cpu:
            return 0;
        case PortIO::Location::Ext1:
            return 1;
        case PortIO::Location::Ext2:
            return 2;
        case PortIO::Location::Ext3:
            return 3;
        case PortIO::Location::Ext4:
            return 4;
        case PortIO::Location::Ext5:
            return 5;
        case PortIO::Location::Ext6:
            return 6;
        case PortIO::Location::Ext7:
            return 7;
        case PortIO::Location::Ext8:
            return 8;
        case PortIO::Location::Ext9:
            return 9;
        case PortIO::Location::Ext10:
            return 10;
        default:
            return 0;
        }
    }

    static void appendPortLabel_(String &out, PortIO::PinType type, const PortIO::PortDesc &p, uint8_t ui_id)
    {
        const char *prefix = "p";
        if (type == PortIO::PinType::Relay)
            prefix = "rly";
        else if (type == PortIO::PinType::DInput)
            prefix = "in";
        else if (type == PortIO::PinType::Sensor)
            prefix = "sens";
        const uint8_t loc = locationIndex_(p.location);
        out += prefix;
        out += "-";
        if (type == PortIO::PinType::Sensor && loc == 0)
        {
            out += String((unsigned)ui_id);
            return;
        }
        out += String((unsigned)loc);
        out += "/";
        out += String((unsigned)ui_id);
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

    String listSocketsHtml_(uint8_t start_id, uint8_t end_id)
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
        String items;
        if (start_id == 0)
            start_id = 1;
        if (end_id < start_id)
            end_id = start_id;
        size_t reserve = 2048u + (size_t)(end_id - start_id + 1) * 700u;
        if (reserve < 16384u)
            reserve = 16384u;
        items.reserve(reserve);
        SocketController &sockets = _controllers->sockets();
        bool tmp_state = false;
        auto appendRow = [&](const SocketController::SocketConfig &cfg, bool enabled) {
            const bool on = enabled && sockets.relayState(cfg.id, tmp_state) ? tmp_state : false;
            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
            items += "\">";
            items += "<div class=\"sock-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sock-icon ";
            items += on ? "on" : "off";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            items += "<path fill=\"currentColor\" d=\"M16 10h32c3.3 0 6 2.7 6 6v32c0 3.3-2.7 6-6 6H16c-3.3 0-6-2.7-6-6V16c0-3.3 2.7-6 6-6zm0 4c-1.1 0-2 .9-2 2v32c0 1.1.9 2 2 2h32c1.1 0 2-.9 2-2V16c0-1.1-.9-2-2-2H16z\"/>";
            items += "<circle cx=\"24\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
            items += "<circle cx=\"40\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
            items += "<rect x=\"28\" y=\"36\" width=\"8\" height=\"10\" rx=\"2\" fill=\"currentColor\"/>";
            items += "</svg>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\">";
            items += "<strong>";
            if (cfg.name.length())
                appendHtmlEscaped_(items, cfg.name.c_str());
            else
                items += "Розетка";
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-enable\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "</div>";
            items += "<input class=\"field name\" type=\"text\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\">";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += on ? "status-on" : "status-off";
            items += "\"></span>";
            items += "<span class=\"status-text\">";
            items += on ? "Включена" : "Выключена";
            items += "</span>";
            items += "</div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Кнопка</label>";
            items += "<select class=\"field mini socket-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.button_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_btn\"></select></div>";
            items += "<div class=\"form-row\"><label>Реле</label>";
            items += "<select class=\"field mini socket-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.relay_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_relay\"></select></div>";
            items += "<div class=\"form-row\"><label>Перекл.</label>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-toggle\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\"";
            if (on)
                items += " checked";
            if (!enabled)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div>";
            items += "<input type=\"hidden\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_action\" value=\"\">";
            items += "</div></div>";
        };

        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const auto *cfg = sockets.configByIndex(i);
            if (!cfg)
                continue;
            if (cfg->id < start_id || cfg->id > end_id)
                continue;
            appendRow(*cfg, cfg->enabled);
        }
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Розетки отсутствуют</strong></div>";
        return items;
    }

    String listLightsHtml_(uint8_t start_id, uint8_t end_id)
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
        String items;
        if (start_id == 0)
            start_id = 1;
        if (end_id < start_id)
            end_id = start_id;
        size_t reserve = 2048u + (size_t)(end_id - start_id + 1) * 700u;
        if (reserve < 16384u)
            reserve = 16384u;
        items.reserve(reserve);
        SocketController &sockets = _controllers->sockets();
        bool tmp_state = false;
        auto appendRow = [&](const SocketController::LightConfig &cfg, bool enabled) {
            const bool on = enabled && sockets.lightRelayState(cfg.id, tmp_state) ? tmp_state : false;
            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
            items += "\">";
            items += "<div class=\"sock-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sock-icon ";
            items += on ? "on" : "off";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            items += "<path fill=\"currentColor\" d=\"M32 4c-9.9 0-18 8.1-18 18 0 7.1 4.1 13.2 10 16.2V50c0 2.2 1.8 4 4 4h8c2.2 0 4-1.8 4-4V38.2c5.9-3 10-9.1 10-16.2 0-9.9-8.1-18-18-18zm6 42H26v-4h12v4zm0-8H26v-4h12v4z\"/>";
            items += "</svg>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\">";
            items += "<strong>";
            if (cfg.name.length())
                appendHtmlEscaped_(items, cfg.name.c_str());
            else
                items += "Свет";
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-enable\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "</div>";
            items += "<input class=\"field name\" type=\"text\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\">";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += on ? "status-on" : "status-off";
            items += "\"></span>";
            items += "<span class=\"status-text\">";
            items += on ? "Включена" : "Выключена";
            items += "</span>";
            items += "</div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Кнопка</label>";
            items += "<select class=\"field mini socket-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.button_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_btn\"></select></div>";
            items += "<div class=\"form-row\"><label>Реле</label>";
            items += "<select class=\"field mini socket-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.relay_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_relay\"></select></div>";
            items += "<div class=\"form-row\"><label>Перекл.</label>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-toggle\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\"";
            if (on)
                items += " checked";
            if (!enabled)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div>";
            items += "<input type=\"hidden\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_action\" value=\"\">";
            items += "</div></div>";
        };

        for (size_t i = 0; i < SocketController::kLightCount; ++i)
        {
            const auto *cfg = sockets.lightConfigByIndex(i);
            if (!cfg)
                continue;
            if (cfg->id < start_id || cfg->id > end_id)
                continue;
            appendRow(*cfg, cfg->enabled);
        }
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Свет отсутствует</strong></div>";
        return items;
    }

    String listSecurityKeysHtml_()
    {
        if (!_controllers)
            return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></td></tr>";
        String items;
        items.reserve(1400);
        SecurityController &sec = _controllers->security();
        char last_hex[17] = {};
        const bool has_last = sec.lastKeyHex(last_hex);
        auto appendRow = [&](size_t idx, bool enabled, const char *hex, const String &name) {
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)(idx + 1));
            items += "</strong></td><td><input type=\"checkbox\" name=\"k";
            items += String((unsigned)(idx + 1));
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"k";
            items += String((unsigned)(idx + 1));
            items += "_name\" value=\"";
            if (name.length())
                appendHtmlEscaped_(items, name.c_str());
            items += "\"></td><td><input class=\"field serial\" type=\"text\" name=\"k";
            items += String((unsigned)(idx + 1));
            items += "_serial\" list=\"k";
            items += String((unsigned)(idx + 1));
            items += "_serial_list\" value=\"";
            if (enabled && hex)
                appendHtmlEscaped_(items, hex);
            items += "\">";
            if (has_last)
            {
                items += "<datalist id=\"k";
                items += String((unsigned)(idx + 1));
                items += "_serial_list\"><option value=\"";
                items += last_hex;
                items += "\"></option></datalist>";
            }
            items += "</td></tr>";
        };

        int first_disabled = -1;
        for (size_t i = 0; i < SecurityController::kKeyCount; ++i)
        {
            bool enabled = false;
            uint8_t addr[8] = {};
            sec.keySlot(i, addr, enabled);
            if (enabled)
            {
                char hex[17] = {};
                IButton::toHex(addr, hex);
                appendRow(i, true, hex, sec.keyNameByIndex(i));
            }
            else if (first_disabled < 0)
            {
                first_disabled = (int)i;
            }
        }
        if (first_disabled >= 0)
            appendRow((size_t)first_disabled, false, nullptr, "");
        if (items.length() == 0)
            items = "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Ключи отсутствуют</strong></td></tr>";
        return items;
    }

    String listSecurityPhonesHtml_()
    {
        if (!_controllers)
            return "<tr><td colspan=\"6\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></td></tr>";
        String items;
        items.reserve(1400);
        SecurityController &sec = _controllers->security();
        int first_empty = -1;
        for (size_t i = 0; i < SecurityController::kPhoneCount; ++i)
        {
            String number;
            bool enabled = false;
            if (!sec.phoneSlot(i, number, enabled))
                continue;
            const bool notify = sec.phoneNotifyByIndex(i);
            const bool call = sec.phoneCallByIndex(i);
            const String &name = sec.phoneNameByIndex(i);
            const bool has_data = enabled || notify || call || number.length() || name.length();
            if (!has_data)
            {
                if (first_empty < 0)
                    first_empty = (int)i;
                continue;
            }
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)(i + 1));
            items += "</strong></td><td><input type=\"checkbox\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_name\" value=\"";
            if (name.length())
                appendHtmlEscaped_(items, name.c_str());
            items += "\"></td><td><input class=\"field serial\" type=\"text\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_num\" value=\"";
            if (number.length())
                appendHtmlEscaped_(items, number.c_str());
            items += "\"></td><td><input type=\"checkbox\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_notify\"";
            if (notify)
                items += " checked";
            items += "></td><td><input type=\"checkbox\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_call\"";
            if (call)
                items += " checked";
            items += "></td></tr>";
        }
        if (first_empty >= 0)
        {
            const size_t i = (size_t)first_empty;
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)(i + 1));
            items += "</strong></td><td><input type=\"checkbox\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_en\"></td><td><input class=\"field name\" type=\"text\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_name\" value=\"\"></td><td><input class=\"field serial\" type=\"text\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_num\" value=\"\"></td><td><input type=\"checkbox\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_notify\"></td><td><input type=\"checkbox\" name=\"p";
            items += String((unsigned)(i + 1));
            items += "_call\"></td></tr>";
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"6\" style=\"color:#94a3b8\"><strong>Телефоны отсутствуют</strong></td></tr>";
        return items;
    }

    String listSecuritySensorsHtml_()
    {
        if (!_controllers)
            return "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></td></tr>";
        String items;
        items.reserve(16384);
        SecurityController &sec = _controllers->security();

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

        auto appendRow = [&](const SecurityController::SensorConfig &cfg, const SecurityController::SensorState &st,
                             bool enabled) {
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)cfg.id);
            items += "</strong></td><td><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"></td><td><select class=\"field mini\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_type\">";
            appendTypeOption("pir", "pir", cfg.type == SecurityController::SensorType::Pir);
            appendTypeOption("reed", "reed", cfg.type == SecurityController::SensorType::Reed);
            items += "</select></td><td><select class=\"field mini security-port\" data-type=\"dinput\" data-selected=\"";
            if (cfg.port != SecurityController::kInvalidPort)
                items += String((unsigned)cfg.port);
            items += "\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_port\"></select></td><td><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_silent\"";
            if (cfg.silent)
                items += " checked";
            items += "></td><td class=\"center\"><span class=\"status-dot ";
            items += st.is_detect ? "status-on" : "status-off";
            items += "\"></span></td></tr>";
        };

        const SecurityController::SensorConfig *first_disabled = nullptr;
        const SecurityController::SensorState *first_disabled_state = nullptr;
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = sec.configByIndex(i);
            const auto *st = sec.stateByIndex(i);
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
            items = "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Датчики отсутствуют</strong></td></tr>";
        return items;
    }

    String listSecuritySensorsTiles_(uint8_t start_idx, uint8_t end_idx)
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
        if (end_idx < start_idx)
            end_idx = start_idx;

        String items;
        items.reserve(16384);
        SecurityController &sec = _controllers->security();

        auto appendTypeOption = [&](String &out, const char *value, const char *label, bool selected) {
            out += "<option value=\"";
            out += value;
            out += "\"";
            if (selected)
                out += " selected";
            out += ">";
            out += label;
            out += "</option>";
        };

        auto appendTile = [&](const SecurityController::SensorConfig &cfg, const SecurityController::SensorState &st) {
            const bool enabled = cfg.enabled;
            const bool detected = st.is_detect;
            const bool is_reed = cfg.type == SecurityController::SensorType::Reed;
            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
            items += "\"><div class=\"sock-visual\"><span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sock-icon ";
            if (!enabled)
                items += "off";
            else if (detected)
                items += "alert";
            else
                items += "on";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            if (is_reed)
            {
                items += "<rect x=\"6\" y=\"18\" width=\"14\" height=\"28\" rx=\"3\" fill=\"currentColor\"/>";
                items += "<rect x=\"44\" y=\"18\" width=\"14\" height=\"28\" rx=\"3\" fill=\"currentColor\"/>";
                items += "<rect x=\"22\" y=\"30\" width=\"20\" height=\"4\" rx=\"2\" fill=\"currentColor\"/>";
            }
            else
            {
                items += "<circle cx=\"32\" cy=\"24\" r=\"6\" fill=\"currentColor\"/>";
                items += "<path d=\"M14 48c6-10 12-14 18-14s12 4 18 14\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"4\" stroke-linecap=\"round\"/>";
                items += "<path d=\"M8 20c6-6 12-10 18-12\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"3\" stroke-linecap=\"round\"/>";
                items += "<path d=\"M56 20c-6-6-12-10-18-12\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"3\" stroke-linecap=\"round\"/>";
            }
            items += "</svg></div><div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name.length())
                appendHtmlEscaped_(items, cfg.name.c_str());
            else
                items += String(F("Датчик #")) + String((unsigned)cfg.id);
            items += "</strong><label class=\"switch\"><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<input class=\"field name\" type=\"text\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\">";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            if (!enabled)
                items += "status-off";
            else if (detected)
                items += "status-bad";
            else
                items += "status-on";
            items += "\"></span><span class=\"status-text\">";
            if (!enabled)
                items += "Отключен";
            else if (detected)
                items += "Сработал";
            else
                items += "Активен";
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Тип</label><select class=\"field mini\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_type\">";
            appendTypeOption(items, "pir", "pir", cfg.type == SecurityController::SensorType::Pir);
            appendTypeOption(items, "reed", "reed", cfg.type == SecurityController::SensorType::Reed);
            items += "</select></div>";
            items += "<div class=\"form-row\"><label>Порт</label><select class=\"field mini security-port\" data-type=\"dinput\" data-selected=\"";
            if (cfg.port != SecurityController::kInvalidPort)
                items += String((unsigned)cfg.port);
            items += "\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_port\"></select></div>";
            items += "<div class=\"form-row\"><label>Тихий</label><label class=\"switch\"><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_silent\"";
            if (cfg.silent)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div></div></div>";
        };

        const size_t max_idx = SecurityController::kSensorCount ? (SecurityController::kSensorCount - 1) : 0;
        if (start_idx > max_idx)
            start_idx = (uint8_t)max_idx;
        if (end_idx > max_idx)
            end_idx = (uint8_t)max_idx;
        for (uint8_t idx = start_idx; idx <= end_idx && idx < SecurityController::kSensorCount; ++idx)
        {
            const auto *cfg = sec.configByIndex(idx);
            const auto *st = sec.stateByIndex(idx);
            if (!cfg || !st)
                continue;
            appendTile(*cfg, *st);
        }
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
        return items;
    }


    String listMeteoHtml_()
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Метео недоступно</strong></div>";
        String items;
        items.reserve(16384);
        MeteoController &meteo = _controllers->meteo();
        const uint32_t now = millis();
        static constexpr size_t kDs18Max = 32;
        char ds18_list[kDs18Max][17] = {};
        size_t ds18_count = 0;
        meteo.listDs18b20Serials(ds18_list, kDs18Max, ds18_count);
        char ds18_used[MeteoController::kSensorCount][17] = {};
        size_t ds18_used_count = 0;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            if (!cfg)
                continue;
            if (cfg->type != MeteoController::SensorType::Ds18b20 || !cfg->ds18_addr_set)
                continue;
            char hex[17] = {};
            MeteoController::formatHexAddr(cfg->ds18_addr, hex);
            bool exists = false;
            for (size_t j = 0; j < ds18_used_count; ++j)
            {
                if (strcmp(ds18_used[j], hex) == 0)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists && ds18_used_count < MeteoController::kSensorCount)
            {
                strncpy(ds18_used[ds18_used_count], hex, sizeof(ds18_used[ds18_used_count]) - 1);
                ++ds18_used_count;
            }
        }

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

            const bool show_hum = (cfg.type == MeteoController::SensorType::Dht22);
            const bool ok_on = has_read && st.ok;

            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
            items += "\">";
            items += "<div class=\"sensor-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sensor-icon ";
            if (!ok_on)
                items += "na";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            items += "<path fill=\"currentColor\" d=\"M32 6c-5.5 0-10 4.5-10 10v19.2c-2.6 2.4-4 5.7-4 9.3 0 7.2 5.8 13 13 13s13-5.8 13-13c0-3.6-1.4-6.9-4-9.3V16c0-5.5-4.5-10-10-10zm6 33.1V16c0-3.3-2.7-6-6-6s-6 2.7-6 6v23.1l-0.9 0.9c-1.8 1.7-2.8 3.9-2.8 6.4 0 4.9 4 9 9 9s9-4 9-9c0-2.5-1-4.8-2.8-6.4l-0.5-0.5z\"/>";
            items += "<rect x=\"30\" y=\"20\" width=\"4\" height=\"20\" rx=\"2\" fill=\"currentColor\"/>";
            items += "</svg>";
            items += "<div class=\"sensor-readout\">";
            items += "<div class=\"sensor-value\">";
            items += temp;
            items += "</div><div class=\"sensor-unit\">°C</div>";
            if (show_hum)
            {
                items += "<div class=\"sensor-hum\"><svg class=\"sensor-hum-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<path fill=\"currentColor\" d=\"M32 6c7 12 16 22 16 34 0 8.8-7.2 16-16 16S16 48.8 16 40c0-12 9-22 16-34z\"/>";
                items += "</svg><div class=\"sensor-value\">";
                items += hum;
                items += "</div><div class=\"sensor-unit\">%</div></div>";
            }
            items += "</div>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name.length())
                appendHtmlEscaped_(items, cfg.name.c_str());
            else
                items += "Датчик";
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"meteo-enable\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<input class=\"field name\" type=\"text\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\">";
            items += "<div class=\"status-line\">";
            items += ok;
            items += "<span>Возраст: ";
            items += age;
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Тип</label><select class=\"field mini meteo-type\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_type\">";
            appendTypeOption("none", "none", cfg.type == MeteoController::SensorType::None);
            appendTypeOption("ds18b20", "ds18b20", cfg.type == MeteoController::SensorType::Ds18b20);
            appendTypeOption("dht22", "dht22", cfg.type == MeteoController::SensorType::Dht22);
            items += "</select></div>";
            items += "<div class=\"form-row pin-cell\"><label>Пин</label><select class=\"field mini meteo-pin\" data-selected=\"";
            items += pin;
            items += "\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_pin\"></select></div>";
            items += "<div class=\"form-row addr-cell full\"><label>Адрес</label><select class=\"field addr meteo-addr\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_addr\">";
            items += "<option value=\"\">-</option>";
            bool addr_found = false;
            for (size_t i = 0; i < ds18_count; ++i)
            {
                bool used = false;
                for (size_t j = 0; j < ds18_used_count; ++j)
                {
                    if (strcmp(ds18_used[j], ds18_list[i]) == 0)
                    {
                        used = true;
                        break;
                    }
                }
                if (used && (!addr.length() || addr != ds18_list[i]))
                    continue;
                items += "<option value=\"";
                items += ds18_list[i];
                items += "\"";
                if (addr.length() && addr == ds18_list[i])
                {
                    items += " selected";
                    addr_found = true;
                }
                items += ">";
                items += ds18_list[i];
                items += "</option>";
            }
            if (addr.length() && !addr_found)
            {
                items += "<option value=\"";
                items += addr;
                items += "\" selected>";
                items += addr;
                items += "</option>";
            }
            items += "</select></div>";
            items += "</div>";
            items += "</div></div>";
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
            items = "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
        return items;
    }

    String listThermoHtml_()
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Thermo unavailable</strong></div>";
        String items;
        items.reserve(16384);
        ThermoController &thermo = _controllers->thermo();
        MeteoController &meteo = _controllers->meteo();
        uint8_t sensor_used[MeteoController::kSensorCount + 1] = {};

        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            if (cfg->sensor_id && cfg->sensor_id <= MeteoController::kSensorCount)
                sensor_used[cfg->sensor_id]++;
        }

        auto appendTile = [&](const ThermoController::DeviceConfig &cfg, const ThermoController::DeviceState &st,
                              bool enabled) {
            const MeteoController::SensorState *sensor_st = nullptr;
            const MeteoController::SensorConfig *sensor_cfg = nullptr;
            if (cfg.sensor_id != ThermoController::kInvalidSensor)
            {
                for (size_t s = 0; s < MeteoController::kSensorCount; ++s)
                {
                    const auto *scfg = meteo.configByIndex(s);
                    if (scfg && scfg->id == cfg.sensor_id)
                    {
                        sensor_cfg = scfg;
                        sensor_st = meteo.stateByIndex(s);
                        break;
                    }
                }
            }

            const char *sensor_label = "нет";
            const char *sensor_suffix = "";
            char sensor_buf[16] = {};
            if (cfg.sensor_id != ThermoController::kInvalidSensor)
            {
                if (sensor_st && sensor_st->has_temp)
                {
                    dtostrf(sensor_st->temp_c, 0, 1, sensor_buf);
                    sensor_label = sensor_buf;
                      sensor_suffix = "°C";
                }
                else
                {
                    sensor_label = "--";
                }
            }

            const char *mode_label = "авто";
            if (cfg.mode == ThermoController::Mode::Off)
                mode_label = "выкл";
            else if (cfg.mode == ThermoController::Mode::Heat)
                mode_label = "нагрев";
            else if (cfg.mode == ThermoController::Mode::Cool)
                mode_label = "охлаждение";

            const char *state_label = "ожидание";
            const char *state_class = "status-idle";
            if (st.heat_on)
            {
                state_label = "нагрев";
                state_class = "status-heat";
            }
            else if (st.cool_on)
            {
                state_label = "охлаждение";
                state_class = "status-cool";
            }

            String heat_class = "icon heat ";
            String cool_class = "icon cool ";
            if (cfg.mode == ThermoController::Mode::Off)
            {
                heat_class += "inactive";
                cool_class += "inactive";
            }
            else if (cfg.mode == ThermoController::Mode::Heat)
            {
                heat_class += "active";
                cool_class += "inactive";
            }
            else if (cfg.mode == ThermoController::Mode::Cool)
            {
                heat_class += "inactive";
                cool_class += "active";
            }
            else
            {
                heat_class += st.heat_on ? "active" : "inactive";
                cool_class += st.cool_on ? "active" : "inactive";
            }

            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
            items += "\"><div class=\"thermo-left\"><div class=\"thermo-visual\"><div class=\"temp-pill sensor\">Текущая: <span class=\"temp-value\">";
            items += sensor_label;
            items += sensor_suffix;
            items += "</span>";
            items += "</div><div class=\"temp-pill target\">Цель: <span class=\"temp-value\">";
            items += String(cfg.target_c, 1);
            items += "°C</span></div><svg class=\"";
            items += heat_class;
            items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"22\" y=\"30\" width=\"76\" height=\"60\" rx=\"10\"/><line x1=\"36\" y1=\"40\" x2=\"36\" y2=\"80\"/><line x1=\"52\" y1=\"40\" x2=\"52\" y2=\"80\"/><line x1=\"68\" y1=\"40\" x2=\"68\" y2=\"80\"/><line x1=\"84\" y1=\"40\" x2=\"84\" y2=\"80\"/></svg><svg class=\"";
            items += cool_class;
            items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"18\" y=\"28\" width=\"84\" height=\"46\" rx=\"10\"/><line x1=\"28\" y1=\"44\" x2=\"92\" y2=\"44\"/><line x1=\"28\" y1=\"56\" x2=\"92\" y2=\"56\"/><line x1=\"40\" y1=\"78\" x2=\"34\" y2=\"92\"/><line x1=\"60\" y1=\"78\" x2=\"60\" y2=\"94\"/><line x1=\"80\" y1=\"78\" x2=\"86\" y2=\"92\"/></svg></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += state_class;
            items += "\"></span><span><span class=\"status-value ";
            if (strcmp(state_class, "status-heat") == 0)
                items += "status-text-heat";
            else if (strcmp(state_class, "status-cool") == 0)
                items += "status-text-cool";
            else
                items += "status-text-idle";
            items += "\">";
            items += state_label;
            items += "</span></span></div>";
            items += "</div><div><div class=\"tile-head\"><div><strong>Термо #";
            items += String((unsigned)cfg.id);
            items += "</strong> <span class=\"badge\">";
            items += mode_label;
            items += "</span>";
            if (!enabled)
                items += " <span class=\"badge\">выкл</span>";
            items += "</div><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-enable\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div><input class=\"field name\" type=\"text\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"><div class=\"form-grid\"><div class=\"form-row\"><label>Активн.</label><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-power\" data-action=\"t";
            items += String((unsigned)cfg.id);
            items += "_power\"";
            const bool ui_power_on = enabled ? st.power_on : false;
            if (ui_power_on)
                items += " checked";
            if (!enabled)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div><input type=\"hidden\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_en_force\" value=\"\">";
            items += "<div class=\"form-row full\"><label>Датчик</label><select class=\"field mini\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_sensor\">";
            items += meteoSensorOptionsHtml_(cfg.sensor_id, sensor_used);
            items += "</select></div><div class=\"form-row\"><label>Режим</label><select class=\"field mini\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_mode\"><option value=\"off\"";
            if (cfg.mode == ThermoController::Mode::Off)
                items += " selected";
            items += ">off</option><option value=\"heat\"";
            if (cfg.mode == ThermoController::Mode::Heat)
                items += " selected";
            items += ">heat only</option><option value=\"cool\"";
            if (cfg.mode == ThermoController::Mode::Cool)
                items += " selected";
            items += ">cool only</option><option value=\"auto\"";
            if (cfg.mode == ThermoController::Mode::Auto)
                items += " selected";
            items += ">auto</option></select></div><div class=\"form-row\"><label>Цель</label><input class=\"field temp\" type=\"number\" step=\"0.1\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_target\" value=\"";
            items += String(cfg.target_c, 1);
            items += "\"></div><div class=\"form-row\"><label>Гист.</label><input class=\"field temp\" type=\"number\" step=\"0.1\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_hyst\" value=\"";
            items += String(cfg.hysteresis, 1);
            items += "\"></div><div class=\"form-row\"><label>Нагрев</label><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.heat_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.heat_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_heat\"></select></div><div class=\"form-row\"><label>Охлажд</label><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.cool_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.cool_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_cool\"></select></div><div class=\"form-row\"><label>Кнопка</label><select class=\"field mini thermo-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.button_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_button\"></select></div></div>";
            if (sensor_cfg && sensor_cfg->name.length())
            {
                items += "<div class=\"status-line\"><span class=\"badge\">";
                appendHtmlEscaped_(items, sensor_cfg->name.c_str());
                items += "</span></div>";
            }
            items += "<input type=\"hidden\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_power\" value=\"\">";
            items += "</div></div>";
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
                appendTile(*cfg, *st, true);
            }
            else if (!first_disabled)
            {
                first_disabled = cfg;
                first_disabled_state = st;
            }
        }
        if (first_disabled && first_disabled_state)
            appendTile(*first_disabled, *first_disabled_state, false);
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Thermo empty</strong></div>";
        return items;
    }

    String listTanksHtml_()
    {
        if (!_controllers)
            return "<div class=\"tile\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></div>";
        String items;
        items.reserve(16384);
        TankController &tanks = _controllers->tanks();

        auto appendRow = [&](const TankController::TankConfig &cfg, const TankController::TankState &st,
                             bool enabled) {
            const char *level = "пусто";
            const char *level_class = "level-empty";
            unsigned level_pct = 10;
            if (st.level_full)
            {
                level = "полный";
                level_class = "level-full";
                level_pct = 90;
            }
            else if (st.level_mid)
            {
                level = "средний";
                level_class = "level-mid";
                level_pct = 60;
            }
            else if (st.level_low)
            {
                level = "низкий";
                level_class = "level-low";
                level_pct = 30;
            }

            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
            items += "\">";
            items += "<div>";
            items += "<div class=\"tank-visual\">";
            items += "<div class=\"tank-fill ";
            items += level_class;
            items += "\" style=\"height:";
            items += String(level_pct);
            items += "%\"></div>";
            items += "<div class=\"tank-label\">";
            items += level;
            items += "</div></div>";
            items += "<div class=\"status-line\">";
            items += "<span class=\"badge\">ID ";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<span class=\"badge\">";
            items += enabled ? "вкл" : "выкл";
            items += "</span>";
            items += "</div></div>";
            items += "<div>";
            items += "<div class=\"tile-head\">";
            items += "<strong>";
            if (cfg.name.length())
                appendHtmlEscaped_(items, cfg.name.c_str());
            else
                items += "Бак";
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "</div>";
            items += "<input class=\"field name\" type=\"text\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\">";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Низкий</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.level_low != TankController::kInvalidPort)
                items += String((unsigned)cfg.level_low);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_low\"></select></div>";
            items += "<div class=\"form-row\"><label>Средний</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.level_mid != TankController::kInvalidPort)
                items += String((unsigned)cfg.level_mid);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_mid\"></select></div>";
            items += "<div class=\"form-row\"><label>Полный</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.level_full != TankController::kInvalidPort)
                items += String((unsigned)cfg.level_full);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_full\"></select></div>";
            items += "<div class=\"form-row\"><label>Вкл</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_valve != TankController::kInvalidPort)
                items += String((unsigned)cfg.relay_valve);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_valve\"></select></div>";
            items += "<div class=\"form-row\"><label>Насос</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_pump != TankController::kInvalidPort)
                items += String((unsigned)cfg.relay_pump);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_pump\"></select></div>";
            items += "<div class=\"form-row\"><label>Сигнал</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_alarm != TankController::kInvalidPort)
                items += String((unsigned)cfg.relay_alarm);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_alarm\"></select></div>";
            items += "<div>";
            items += "<div class=\"form-row\"><label>Питание</label><label class=\"switch\"><input type=\"checkbox\" class=\"tank-power\" data-action=\"k";
            items += String((unsigned)cfg.id);
            items += "_power\"";
            if (cfg.power_on)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label><input type=\"hidden\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_power\" value=\"";
            items += cfg.power_on ? "on" : "off";
            items += "\"></div>";
            items += "<div class=\"status-row\">";
            items += "<span class=\"status-dot ";
            items += st.pump_on ? "status-on" : "status-off";
            items += "\"></span><span>Насос</span>";
            items += "<span class=\"status-dot ";
            items += st.valve_on ? "status-on" : "status-off";
            items += "\"></span><span>Клапан</span>";
            items += "</div></div>";
            items += "</div></div>";
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
            items = "<div class=\"tile\" style=\"color:#94a3b8\"><strong>Баки отсутствуют</strong></div>";
        return items;
    }

    String listSepticHtml_()
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
        String items;
        items.reserve(2048);
        SepticController &septic = _controllers->septic();
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = septic.configByIndex(i);
            const auto *st = septic.stateByIndex(i);
            if (!cfg || !st)
                continue;
            const bool warn = st->warning;
            const bool alarm = st->alarm;
            const bool relay_warn = st->relay_warning;
            const bool relay_alarm = st->relay_alarm;
            const char *water_class = "water-low";
            const char *water_level = "20%";
            const char *water_label = "Уровень: 20%";
            if (alarm)
            {
                water_class = "water-alarm";
                water_level = "100%";
                water_label = "Уровень: 100%";
            }
            else if (warn)
            {
                water_class = "water-warn";
                water_level = "80%";
                water_label = "Уровень: 80%";
            }
            items += "<div class=\"tile";
            if (!cfg->enabled)
                items += " disabled";
            items += "\"><div class=\"septic-visual\"><div class=\"liquid ";
            items += water_class;
            items += "\" style=\"height:";
            items += water_level;
            items += ";\"></div><div class=\"level-label\">";
            items += water_label;
            items += "</div></div><div><div class=\"tile-head\"><div><strong>Септик #";
            items += String((unsigned)cfg->id);
            items += "</strong>";
            if (!cfg->enabled)
                items += " <span class=\"badge\">выкл</span>";
            items += "</div><label class=\"switch\"><input type=\"checkbox\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_en\"";
            if (cfg->enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div><input class=\"field name\" type=\"text\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg->name.c_str());
            items += "\"><div class=\"form-grid\"><div class=\"form-row\"><label>Предупр.</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg->warning_port != SepticController::kInvalidPort)
                items += String((unsigned)cfg->warning_port);
            items += "\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_warn\"></select></div><div class=\"form-row\"><label>Тревога</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg->alarm_port != SepticController::kInvalidPort)
                items += String((unsigned)cfg->alarm_port);
            items += "\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_alarm\"></select></div><div class=\"form-row\"><label>Реле предупр.</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
            if (cfg->relay_warning != SepticController::kInvalidPort)
                items += String((unsigned)cfg->relay_warning);
            items += "\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_relay_warn\"></select></div><div class=\"form-row\"><label>Реле тревоги</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
            if (cfg->relay_alarm != SepticController::kInvalidPort)
                items += String((unsigned)cfg->relay_alarm);
            items += "\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_relay_alarm\"></select></div></div><div class=\"status-grid\"><div class=\"status-line\"><span class=\"status-dot ";
            items += warn ? "status-on" : "status-off";
            items += "\"></span><span>Датчик предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
            items += alarm ? "status-on" : "status-off";
            items += "\"></span><span>Датчик тревоги</span></div><div class=\"status-line\"><span class=\"status-dot ";
            items += relay_warn ? "status-on" : "status-off";
            items += "\"></span><span>Реле предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
            items += relay_alarm ? "status-on" : "status-off";
            items += "\"></span><span>Реле тревоги</span></div></div><div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep";
            items += String((unsigned)cfg->id);
            items += "_mon\"";
            if (cfg->monitoring_on)
                items += " checked";
            if (!cfg->enabled)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label><input type=\"hidden\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_mon\" value=\"";
            items += cfg->monitoring_on ? "on" : "off";
            items += "\"></div></div></div>";
        }
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Септик отсутствует</strong></div>";
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
        uint8_t next_id[11] = {};
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
            uint8_t ui_id = p.ui_id;
            if (ui_id == 0)
            {
                const uint8_t loc = locationIndex_(p.location);
                if (loc < 11)
                    ui_id = ++next_id[loc];
                else
                    ui_id = 0;
            }
            if (!first)
                out += ",";
            out += "{\"v\":";
            out += String((unsigned)i);
            out += ",\"l\":\"";
            appendPortLabel_(out, type, p, ui_id);
            out += "\"}";
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
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = sockets.lightConfigByIndex(i);
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
                if (p.caps == Cap::None || p.type != PortIO::PinType::DInput)
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

    String securityPortOptionsJson_() const
    {
        return socketPortOptionsJson_(PortIO::PinType::DInput);
    }

    String securityUsedPinsJson_() const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            SecurityController &sec = _controllers->security();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                const auto *cfg = sec.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const uint8_t pin = cfg->port;
                if (pin != SecurityController::kInvalidPort && pin < PortIO::PORT_COUNT)
                    used[pin] = true;
            }
            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                if (!used[i])
                    continue;
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != PortIO::PinType::DInput)
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

    String septicPortOptionsJson_(PortIO::PinType type) const
    {
        return socketPortOptionsJson_(type);
    }

    String septicUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            auto mark_used = [](bool used[], uint8_t port)
            {
                if (port < PortIO::PORT_COUNT)
                    used[port] = true;
            };
            SepticController &septic = _controllers->septic();
            bool used[PortIO::PORT_COUNT] = {};
            SocketController &sockets = _controllers->sockets();
            ThermoController &thermo = _controllers->thermo();
            TankController &tanks = _controllers->tanks();
            SecurityController &security = _controllers->security();

            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = sockets.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->button_port);
                mark_used(used, cfg->relay_port);
            }
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = thermo.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->heat_port);
                mark_used(used, cfg->cool_port);
                mark_used(used, cfg->button_port);
            }
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = tanks.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->level_low);
                mark_used(used, cfg->level_mid);
                mark_used(used, cfg->level_full);
                mark_used(used, cfg->relay_valve);
                mark_used(used, cfg->relay_pump);
                mark_used(used, cfg->relay_alarm);
            }
            if (security.sirenPort() != SecurityController::kInvalidPort)
                mark_used(used, security.sirenPort());

            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = septic.configByIndex(i);
                if (!cfg)
                    continue;
                if (type == PortIO::PinType::DInput)
                {
                    const uint8_t w = cfg->warning_port;
                    const uint8_t a = cfg->alarm_port;
                    if (w != SepticController::kInvalidPort)
                        mark_used(used, w);
                    if (a != SepticController::kInvalidPort)
                        mark_used(used, a);
                }
                else if (type == PortIO::PinType::Relay)
                {
                    const uint8_t rw = cfg->relay_warning;
                    const uint8_t ra = cfg->relay_alarm;
                    if (rw != SepticController::kInvalidPort)
                        mark_used(used, rw);
                    if (ra != SepticController::kInvalidPort)
                        mark_used(used, ra);
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
            if (_upload_in_progress && _upload)
                _upload.close();
            _upload_in_progress = true;
            request->onDisconnect([this]() {
                if (!_upload_in_progress)
                    return;
                if (_upload)
                    _upload.close();
                _upload_ok = false;
                _upload_error = "Upload disconnected";
                _upload_in_progress = false;
            });
            _upload_set_cookie = set_cookie;
            _upload_ok = true;
            _upload_error = "";
            _upload_name = filename;
            String path = sanitizeUploadName_(filename);
            if (!path.length())
            {
                _upload_ok = false;
                _upload_error = "Invalid file name";
                _upload_in_progress = false;
                return;
            }
            if (!isAllowedExt_(path))
            {
                _upload_ok = false;
                _upload_error = "File extension not allowed";
                _upload_in_progress = false;
                return;
            }
            _upload_size = 0;
            _upload = LittleFS.open(path, "w");
            if (!_upload)
            {
                _upload_ok = false;
                _upload_error = "Open failed";
                _upload_in_progress = false;
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
            _upload_in_progress = false;
            return;
        }
        if (_upload)
            _upload.write(data, len);
        if (final)
        {
            if (_upload)
                _upload.close();
            _upload_in_progress = false;
        }
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
            _ota_in_progress = false;
            return;
#else
            if (_ota_in_progress)
                Update.abort();
            _ota_in_progress = true;
            request->onDisconnect([this]() {
                if (!_ota_in_progress)
                    return;
                Update.abort();
                _ota_ok = false;
                _ota_error = "OTA disconnected";
                _ota_in_progress = false;
            });
            _ota_ok = true;
            _ota_error = "";
            _ota_size = 0;
            _ota_name = filename;
            if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            {
                _ota_ok = false;
                _ota_error = Update.errorString();
                _ota_in_progress = false;
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
            _ota_in_progress = false;
            return;
        }
        if (Update.write(data, len) != len)
        {
            _ota_ok = false;
            _ota_error = Update.errorString();
            Update.abort();
            _ota_in_progress = false;
            return;
        }
        if (final)
        {
            if (!Update.end(true))
            {
                _ota_ok = false;
                _ota_error = Update.errorString();
            }
            _ota_in_progress = false;
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

        bool gsm_changed = false;
        bool gsm_ok = true;
        if (_gsm && ActiveBoardProfile::GSM.enabled)
        {
            const bool gsm_enabled = request->hasParam("gsm_enabled", true);
            gsm_changed = (gsm_enabled != _gsm->enabled());
            _gsm->setEnabled(gsm_enabled);
        }
        else if (_gsm)
        {
            _gsm->setEnabled(false);
        }

        bool wifi_ok = true;
        bool save_ok = true;
        if (changed)
            wifi_ok = _wifi.begin();
        if (changed || gsm_changed)
            save_ok = saveWifiConfig_();

        if (_gsm && _gsm->enabled() && !_gsm->started())
            gsm_ok = _gsm->begin(ActiveBoardProfile::GSM.uart_index);

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

        if (!_gsm)
            _gsm_status = "GSM unavailable";
        else if (!ActiveBoardProfile::GSM.enabled)
            _gsm_status = "GSM disabled by board profile";
        else if (!gsm_changed)
            _gsm_status = "No changes";
        else if (!gsm_ok && !save_ok)
            _gsm_status = "GSM apply and save failed";
        else if (!gsm_ok)
            _gsm_status = "GSM apply failed";
        else if (!save_ok)
            _gsm_status = "GSM applied, but save failed";
        else
            _gsm_status = "GSM updated";
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

        String api_key = request->hasParam("api_key", true)
                             ? request->getParam("api_key", true)->value()
                             : String("");
        api_key.trim();
        if (api_key != _configs_manager->stackApiKey())
        {
            _configs_manager->setStackApiKey(api_key);
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

    void handleStackGenKey_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        if (!_configs_manager)
        {
            sendText_(request, 500, "text/plain", "Config manager missing", set_cookie);
            return;
        }
        if (_configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            sendText_(request, 403, "text/plain", "Stack role is slave", set_cookie);
            return;
        }
        const String key = genApiKey_();
        _configs_manager->setStackApiKey(key);
        if (!_configs_manager->save())
        {
            sendText_(request, 500, "text/plain", "Save failed", set_cookie);
            return;
        }
        _stack_status = "Saved";
        sendText_(request, 200, "text/plain", key, set_cookie);
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
        name = sanitizeUtf8_(name);
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

    void handleUiHash_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!checkAuth_(request, &set_cookie))
            return;
        String path = "/";
        if (request->hasParam("path"))
        {
            path = request->getParam("path")->value();
            if (path.length() == 0)
                path = "/";
        }
        const uint32_t hash = uiPageHash_(path);
        char buf[9] = {};
        snprintf(buf, sizeof(buf), "%08lX", (unsigned long)hash);
        sendText_(request, 200, "text/plain", buf, set_cookie);
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

    bool checkAuthApi_(AsyncWebServerRequest *request, bool *set_cookie)
    {
        if (set_cookie)
            *set_cookie = false;
        if (!request)
            return false;

        String token;
        if (extractSessionToken_(request, token) && sessionValid_(token))
        {
            refreshSession_();
            return true;
        }

        if (_cli_auth)
        {
            String user;
            String pass;
            if (parseBasicAuth_(request, user, pass))
            {
                String ulow = user;
                ulow.toLowerCase();
                if (ulow == CliConsole::kAdminUser && _cli_auth->checkAdminPassword(pass))
                {
                    issueSession_();
                    if (set_cookie)
                        *set_cookie = true;
                    return true;
                }
            }
        }

        sendText_(request, 403, "application/json", "{\"ok\":false,\"err\":\"auth\"}", false);
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
        nav += F("<a href=\"/\">FCPLC</a> | <a href=\"/wifi\">Сеть</a> | ");
        nav += F("<a href=\"/manage\">Прошивка и файлы</a> | <a href=\"/ports\">Порты</a> | <a href=\"/buses\">Шины</a> | ");
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

    String stackApiKey_() const
    {
        if (_configs_manager)
            return _configs_manager->stackApiKey();
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

    static bool parseSecurityType_(const String &input, SecurityController::SensorType &out)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "pir")
        {
            out = SecurityController::SensorType::Pir;
            return true;
        }
        if (t == "reed")
        {
            out = SecurityController::SensorType::Reed;
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

    static bool parseSecurityKeyHex_(const String &s, uint8_t out[8])
    {
        if (s.length() != 16)
            return false;
        for (uint8_t i = 0; i < 8; ++i)
        {
            const char hi_c = s[i * 2];
            const char lo_c = s[i * 2 + 1];
            auto nibble = [](char c) -> int {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F')
                    return 10 + (c - 'A');
                return -1;
            };
            const int hi = nibble(hi_c);
            const int lo = nibble(lo_c);
            if (hi < 0 || lo < 0)
                return false;
            out[i] = (uint8_t)((hi << 4) | lo);
        }
        return true;
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
            if (is_used)
                continue;
            out += "<option value=\"";
            out += String((unsigned)id);
            out += "\"";
            if (id == selected)
                out += " selected";
            out += ">";
            if (cfg->name.length())
            {
                out += String((unsigned)id);
                out += ": ";
                appendHtmlEscaped_(out, cfg->name.c_str());
            }
            else
            {
                out += String((unsigned)id);
            }
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

    static String paramValueAny_(AsyncWebServerRequest *request, const String &name)
    {
        if (!request)
            return "";
        if (request->hasParam(name, true))
            return request->getParam(name, true)->value();
        if (request->hasParam(name, false))
            return request->getParam(name, false)->value();
        if (request->hasParam(name))
            return request->getParam(name)->value();
        return "";
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

    static String safeHtmlValue_(const String &value, const char *fallback)
    {
        if (value.length() == 0)
            return String(fallback);
        String out;
        out.reserve(value.length() + 8);
        appendHtmlEscaped_(out, value.c_str());
        return out;
    }

    static String sanitizeUtf8_(const String &in)
    {
        if (isValidUtf8_(in))
            return in;
        return cp1251ToUtf8_(in);
    }

    static bool isValidUtf8_(const String &in)
    {
        size_t i = 0;
        while (i < (size_t)in.length())
        {
            const uint8_t c = (uint8_t)in[i];
            if (c < 0x80)
            {
                ++i;
                continue;
            }
            size_t need = 0;
            if ((c & 0xE0) == 0xC0)
            {
                if (c < 0xC2)
                    return false;
                need = 1;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                need = 2;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                if (c > 0xF4)
                    return false;
                need = 3;
            }
            else
            {
                return false;
            }

            if (i + need >= (size_t)in.length())
                return false;

            for (size_t j = 1; j <= need; ++j)
            {
                const uint8_t cc = (uint8_t)in[i + j];
                if ((cc & 0xC0) != 0x80)
                    return false;
            }
            i += need + 1;
        }
        return true;
    }

    static void appendUtf8_(String &out, uint16_t code)
    {
        if (code < 0x80)
        {
            out += (char)code;
            return;
        }
        if (code < 0x800)
        {
            out += (char)(0xC0 | (code >> 6));
            out += (char)(0x80 | (code & 0x3F));
            return;
        }
        out += (char)(0xE0 | (code >> 12));
        out += (char)(0x80 | ((code >> 6) & 0x3F));
        out += (char)(0x80 | (code & 0x3F));
    }

    static String cp1251ToUtf8_(const String &in)
    {
        String out;
        out.reserve(in.length() * 2);
        for (size_t i = 0; i < (size_t)in.length(); ++i)
        {
            const uint8_t c = (uint8_t)in[i];
            if (c < 0x80)
            {
                out += (char)c;
                continue;
            }
            uint16_t code = '?';
            if (c == 0xA8)
                code = 0x0401;
            else if (c == 0xB8)
                code = 0x0451;
            else if (c >= 0xC0 && c <= 0xFF)
                code = (uint16_t)(0x0410 + (c - 0xC0));
            else
                code = '?';
            appendUtf8_(out, code);
        }
        return out;
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
        const auto users = _tgbot_menu->allowedUsers();
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
        for (size_t i = 0; i < users.size && row < max; ++i)
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
        String out = page;
        injectAutoRefresh_(out);
        auto *response = request->beginResponse(200, "text/html; charset=utf-8", out);
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

    void sendHtmlRaw_(AsyncWebServerRequest *request, const String &page, bool set_cookie)
    {
        auto *response = request->beginResponse(200, "text/html; charset=utf-8", page);
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

    void injectAutoRefresh_(String &page) const
    {
        const int idx = page.lastIndexOf("</body>");
        if (idx < 0)
            return;
        String out;
        out.reserve(page.length() + 512);
        out += page.substring(0, idx);
        out += FPSTR(kWebAutoRefreshScript);
        out += page.substring(idx);
        page = out;
    }

    static uint32_t fnv1a_(uint32_t hash, const uint8_t *data, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            hash ^= data[i];
            hash *= 16777619u;
        }
        return hash;
    }

    static void hashAdd_(uint32_t &hash, const String &value)
    {
        hash = fnv1a_(hash, reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
    }

    static void hashAdd_(uint32_t &hash, const char *value)
    {
        if (!value)
            return;
        hash = fnv1a_(hash, reinterpret_cast<const uint8_t *>(value), strlen(value));
    }

    static void hashAdd_(uint32_t &hash, uint32_t value)
    {
        hash = fnv1a_(hash, reinterpret_cast<const uint8_t *>(&value), sizeof(value));
    }

    static void hashAdd_(uint32_t &hash, int32_t value)
    {
        hash = fnv1a_(hash, reinterpret_cast<const uint8_t *>(&value), sizeof(value));
    }

    static void hashAdd_(uint32_t &hash, const uint8_t *data, size_t len)
    {
        if (!data || !len)
            return;
        hash = fnv1a_(hash, data, len);
    }

    static int32_t scaled10_(float value)
    {
        if (value >= 0.0f)
            return (int32_t)(value * 10.0f + 0.5f);
        return (int32_t)(value * 10.0f - 0.5f);
    }

    uint32_t uiPageHash_(const String &path)
    {
        uint32_t hash = 2166136261u;
        hashAdd_(hash, path);
        hashAdd_(hash, _last_status);
        hashAdd_(hash, _wifi_status);
        hashAdd_(hash, _gsm_status);
        hashAdd_(hash, _tgbot_status);
        hashAdd_(hash, _stack_status);
        hashAdd_(hash, _device_status);
        hashAdd_(hash, _sockets_status);
        hashAdd_(hash, _controllers_status);
        hashAdd_(hash, _sockets_status);
        hashAdd_(hash, _lights_status);
        hashAdd_(hash, _meteo_status);
        hashAdd_(hash, _thermo_status);
        hashAdd_(hash, _tanks_status);
        hashAdd_(hash, _septic_status);
        hashAdd_(hash, _security_status);

        if (path == "/" || path == "/index")
        {
            hashAdd_(hash, deviceName_());
            hashAdd_(hash, stackRoleName_(stackRole_()));
            hashAdd_(hash, wifiIp_());
            hashAdd_(hash, formatTemp_(boardTemp_()));
            hashAdd_(hash, formatTemp_(cpuTemp_()));
            hashAdd_(hash, formatTemp_(rtcTemp_()));
            return hash;
        }
        if (path == "/wifi")
        {
            hashAdd_(hash, _wifi.ap() ? "AP" : "STA");
            hashAdd_(hash, _wifi.ap() ? _wifi.apSsid() : _wifi.ssid());
            hashAdd_(hash, wifiIp_());
            if (_gsm)
            {
                hashAdd_(hash, _gsm->imei());
                hashAdd_(hash, _gsm->imsi());
                hashAdd_(hash, _gsm->operatorName());
                hashAdd_(hash, _gsm->signalQuality());
                hashAdd_(hash, _gsm->regStatus());
                hashAdd_(hash, _gsm->lastError());
                hashAdd_(hash, _gsm->lastUrc());
                hashAdd_(hash, _gsm->lastCallNumber());
                hashAdd_(hash, _gsm->lastUssd());
            }
            return hash;
        }
        if (path == "/manage")
        {
            hashAdd_(hash, listFilesHtml_());
            return hash;
        }
        if (path == "/controllers")
        {
            if (_controllers)
            {
                hashAdd_(hash, _controllers->sockets().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->sockets().lightsEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->meteo().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->thermo().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->tanks().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->septic().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->security().controllerEnabled() ? 1u : 0u);
            }
            return hash;
        }
        if (path == "/sockets")
        {
            if (_controllers)
            {
                SocketController &sockets = _controllers->sockets();
                for (size_t i = 0; i < SocketController::kSocketCount; ++i)
                {
                    const auto *cfg = sockets.configByIndex(i);
                    const auto *st = sockets.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->button_port);
                    hashAdd_(hash, (uint32_t)cfg->relay_port);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->relay_on ? 1u : 0u);
                }
            }
            return hash;
        }
        if (path == "/lights")
        {
            if (_controllers)
            {
                SocketController &sockets = _controllers->sockets();
                for (size_t i = 0; i < SocketController::kLightCount; ++i)
                {
                    const auto *cfg = sockets.lightConfigByIndex(i);
                    const auto *st = sockets.lightStateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->button_port);
                    hashAdd_(hash, (uint32_t)cfg->relay_port);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->relay_on ? 1u : 0u);
                }
            }
            return hash;
        }
        if (path == "/meteo")
        {
            if (_controllers)
            {
                MeteoController &meteo = _controllers->meteo();
                for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
                {
                    const auto *cfg = meteo.configByIndex(i);
                    const auto *st = meteo.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->type);
                    hashAdd_(hash, (uint32_t)cfg->dht_pin);
                    hashAdd_(hash, cfg->ds18_addr_set ? 1u : 0u);
                    hashAdd_(hash, cfg->name);
                    if (cfg->ds18_addr_set)
                        hashAdd_(hash, cfg->ds18_addr, MeteoController::kAddrLen);
                    hashAdd_(hash, st->ok ? 1u : 0u);
                    hashAdd_(hash, st->has_temp ? 1u : 0u);
                    hashAdd_(hash, st->has_humidity ? 1u : 0u);
                    if (st->has_temp)
                        hashAdd_(hash, scaled10_(st->temp_c));
                    if (st->has_humidity)
                        hashAdd_(hash, scaled10_(st->humidity));
                    hashAdd_(hash, st->last_read_ms);
                }
            }
            return hash;
        }
        if (path == "/thermo")
        {
            if (_controllers)
            {
                ThermoController &thermo = _controllers->thermo();
                MeteoController &meteo = _controllers->meteo();
                const MeteoController::SensorState *sensor_state_by_id[MeteoController::kSensorCount + 1] = {};
                for (size_t s = 0; s < MeteoController::kSensorCount; ++s)
                {
                    const auto *scfg = meteo.configByIndex(s);
                    const auto *sst = meteo.stateByIndex(s);
                    if (!scfg || !sst)
                        continue;
                    if (scfg->id <= MeteoController::kSensorCount)
                        sensor_state_by_id[scfg->id] = sst;
                }
                for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
                {
                    const auto *cfg = thermo.configByIndex(i);
                    const auto *st = thermo.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->sensor_id);
                    hashAdd_(hash, (uint32_t)cfg->heat_port);
                    hashAdd_(hash, (uint32_t)cfg->cool_port);
                    hashAdd_(hash, (uint32_t)cfg->button_port);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, (uint32_t)cfg->mode);
                    hashAdd_(hash, scaled10_(cfg->target_c));
                    hashAdd_(hash, scaled10_(cfg->hysteresis));
                    hashAdd_(hash, st->heat_on ? 1u : 0u);
                    hashAdd_(hash, st->cool_on ? 1u : 0u);
                    hashAdd_(hash, st->power_on ? 1u : 0u);
                    if (cfg->sensor_id <= MeteoController::kSensorCount)
                    {
                        const auto *sst = sensor_state_by_id[cfg->sensor_id];
                        hashAdd_(hash, sst && sst->has_temp ? 1u : 0u);
                        if (sst && sst->has_temp)
                            hashAdd_(hash, scaled10_(sst->temp_c));
                    }
                }
            }
            return hash;
        }
        if (path == "/tanks")
        {
            if (_controllers)
            {
                TankController &tanks = _controllers->tanks();
                for (size_t i = 0; i < TankController::kTankCount; ++i)
                {
                    const auto *cfg = tanks.configByIndex(i);
                    const auto *st = tanks.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, cfg->power_on ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->level_low);
                    hashAdd_(hash, (uint32_t)cfg->level_mid);
                    hashAdd_(hash, (uint32_t)cfg->level_full);
                    hashAdd_(hash, (uint32_t)cfg->relay_valve);
                    hashAdd_(hash, (uint32_t)cfg->relay_pump);
                    hashAdd_(hash, (uint32_t)cfg->relay_alarm);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->level_low ? 1u : 0u);
                    hashAdd_(hash, st->level_mid ? 1u : 0u);
                    hashAdd_(hash, st->level_full ? 1u : 0u);
                    hashAdd_(hash, st->levels_ok ? 1u : 0u);
                    hashAdd_(hash, st->valve_on ? 1u : 0u);
                    hashAdd_(hash, st->pump_on ? 1u : 0u);
                    hashAdd_(hash, st->alarm_on ? 1u : 0u);
                }
            }
            return hash;
        }
        if (path == "/septic")
        {
            if (_controllers)
            {
                SepticController &septic = _controllers->septic();
                for (size_t i = 0; i < SepticController::kSepticCount; ++i)
                {
                    const auto *cfg = septic.configByIndex(i);
                    const auto *st = septic.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, cfg->monitoring_on ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->warning_port);
                    hashAdd_(hash, (uint32_t)cfg->alarm_port);
                    hashAdd_(hash, (uint32_t)cfg->relay_warning);
                    hashAdd_(hash, (uint32_t)cfg->relay_alarm);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->warning ? 1u : 0u);
                    hashAdd_(hash, st->alarm ? 1u : 0u);
                    hashAdd_(hash, st->relay_warning ? 1u : 0u);
                    hashAdd_(hash, st->relay_alarm ? 1u : 0u);
                }
            }
            return hash;
        }
        if (path == "/security")
        {
            if (_controllers)
            {
                SecurityController &sec = _controllers->security();
                hashAdd_(hash, sec.controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, sec.armed() ? 1u : 0u);
                hashAdd_(hash, sec.alarmOn() ? 1u : 0u);
                hashAdd_(hash, static_cast<uint32_t>(sec.sirenPort()));
                for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
                {
                    const auto *cfg = sec.configByIndex(i);
                    const auto *st = sec.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->type);
                    hashAdd_(hash, (uint32_t)cfg->port);
                    hashAdd_(hash, cfg->silent ? 1u : 0u);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->raw ? 1u : 0u);
                    hashAdd_(hash, st->is_detect ? 1u : 0u);
                }
                for (size_t i = 0; i < SecurityController::kPhoneCount; ++i)
                {
                    hashAdd_(hash, sec.phoneByIndex(i));
                    hashAdd_(hash, sec.phoneNameByIndex(i));
                    hashAdd_(hash, sec.phoneNotifyByIndex(i) ? 1u : 0u);
                    hashAdd_(hash, sec.phoneCallByIndex(i) ? 1u : 0u);
                    bool enabled = false;
                    String number;
                    sec.phoneSlot(i, number, enabled);
                    hashAdd_(hash, enabled ? 1u : 0u);
                }
                for (size_t i = 0; i < SecurityController::kKeyCount; ++i)
                {
                    uint8_t addr[8] = {};
                    bool enabled = false;
                    if (sec.keySlot(i, addr, enabled))
                    {
                        hashAdd_(hash, enabled ? 1u : 0u);
                        if (enabled)
                            hashAdd_(hash, addr, sizeof(addr));
                    }
                }
            }
            return hash;
        }
        if (path == "/ports")
        {
            hashAdd_(hash, listPortsHtml_());
            return hash;
        }
        if (path == "/buses")
        {
            hashAdd_(hash, listI2cHtml_());
            hashAdd_(hash, listOwHtml_());
            return hash;
        }
        if (path == "/stack")
        {
            hashAdd_(hash, stackRoleName_(stackRole_()));
            hashAdd_(hash, stackMasterHost_());
            hashAdd_(hash, stackApiKey_());
            hashAdd_(hash, listStackNodesHtml_());
            return hash;
        }
        if (path == "/admin")
        {
            if (_cli_auth)
                hashAdd_(hash, _cli_auth->adminPasswordSet() ? 1u : 0u);
            return hash;
        }
        if (path == "/logs")
        {
            if (_log)
            {
                const size_t count = _log->recentCount();
                hashAdd_(hash, static_cast<uint32_t>(count));
                if (count > 0)
                {
                    char buf[LOGGER_BUFFER_SIZE] = {};
                    if (_log->getRecentLine(count - 1, buf, sizeof(buf)))
                        hashAdd_(hash, buf);
                }
            }
            return hash;
        }
        if (path == "/telegram")
        {
            if (_tgbot)
            {
                hashAdd_(hash, _tgbot->token());
                hashAdd_(hash, String((long long)_tgbot->chatId()));
                hashAdd_(hash, _tgbot->clientKindName());
                hashAdd_(hash, _tgbot->useProxy() ? "1" : "0");
                hashAdd_(hash, _tgbot->proxyHost());
                hashAdd_(hash, String((unsigned)_tgbot->proxyPort()));
                hashAdd_(hash, _tgbot->proxyPath());
            }
            hashAdd_(hash, allowedUsersRowsHtml_());
            return hash;
        }
        return hash;
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

    static String genApiKey_()
    {
        char buf[33] = {};
        static const char kHex[] = "0123456789abcdef";
        for (size_t i = 0; i < 16; ++i)
        {
            const uint8_t v = (uint8_t)random(0, 256);
            buf[i * 2] = kHex[(v >> 4) & 0x0F];
            buf[i * 2 + 1] = kHex[v & 0x0F];
        }
        buf[32] = '\0';
        return String(buf);
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

    String gsmStatusLabel_() const
    {
        if (!_gsm)
            return "недоступно";
        if (!_gsm->started())
            return "off";
        const String &err = _gsm->lastError();
        if (err.length())
            return "error";
        const String &reg = _gsm->regStatus();
        if (!reg.length())
            return "no reg";
        return "ok";
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
    GsmModem *_gsm = nullptr;
    I2CManager *_i2c = nullptr;
    OneWireManager *_ow = nullptr;
    File _upload;
    bool _upload_ok = true;
    bool _upload_in_progress = false;
    size_t _upload_size = 0;
    size_t _max_upload = 0;
    bool _ota_ok = false;
    size_t _ota_size = 0;
    String _ota_error;
    String _allowed_exts;
    String _last_status;
    String _wifi_status;
    String _gsm_status;
    String _upload_error;
    String _upload_name;
    String _ota_name;
    String _tgbot_status;
    String _stack_status;
    String _device_status;
    String _sockets_status;
    String _lights_status;
    String _controllers_status;
    String _meteo_status;
    String _thermo_status;
    String _tanks_status;
    String _septic_status;
    String _security_status;
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
    bool _ota_in_progress = false;
};


















