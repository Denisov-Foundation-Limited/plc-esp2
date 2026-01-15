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
#include "core/network/web/pages/web_interface_telegram.hpp"
#include "core/network/web/pages/web_interface_status.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"

class WebInterface
{
public:
    WebInterface(AsyncWebServer &server, const CliConsole &cli, WifiManager &wifi, Configs &configs, PlcControl &plc,
                 RTC &rtc, TelegramClient &tgbot, TelegramMenu &tgbot_menu, Logger &logs, Extender &ext,
                 I2CManager &i2c, OneWireManager &ow)
        : _server(server),
          _cli_auth(&cli),
          _wifi(wifi),
          _configs(configs),
          _plc(&plc),
          _rtc(&rtc),
          _tgbot(&tgbot),
          _tgbot_menu(&tgbot_menu),
          _ext(&ext),
          _i2c(&i2c),
          _ow(&ow),
          _log(&logs)
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
        _server.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest *request) { handleWifi_(request); });
        _server.on("/manage", HTTP_GET, [this](AsyncWebServerRequest *request) { handleManage_(request); });
        _server.on("/ports", HTTP_GET, [this](AsyncWebServerRequest *request) { handlePorts_(request); });
        _server.on("/buses", HTTP_GET, [this](AsyncWebServerRequest *request) { handleBuses_(request); });
        _server.on("/stack", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStack_(request); });
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
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET / (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceIndexHtml);
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
        request->send(200, "text/html", page);
    }

    void handleWifi_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /wifi (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceWifiHtml);
        page.replace("%WIFI_MODE%", _wifi.ap() ? "AP" : "STA");
        page.replace("%WIFI_CUR_SSID%", _wifi.ap() ? _wifi.apSsid() : _wifi.ssid());
        page.replace("%WIFI_IP%", wifiIp_());
        page.replace("%WIFI_STA_SEG%", wifiStaSegment_());
        page.replace("%WIFI_STA_SEL%", _wifi.ap() ? "" : "selected");
        page.replace("%WIFI_AP_SEL%", _wifi.ap() ? "selected" : "");
        page.replace("%WIFI_SSID%", _wifi.ssid());
        page.replace("%WIFI_AP_SSID%", _wifi.apSsid());
        page.replace("%WIFI_STATUS%", _wifi_status);
        request->send(200, "text/html", page);
    }

    void handleManage_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /manage (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceManageHtml);
        page.replace("%FILES%", listFilesHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        request->send(200, "text/html", page);
    }

    void handlePorts_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /ports (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfacePortsHtml);
        page.replace("%PORTS%", listPortsHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        request->send(200, "text/html", page);
    }

    void handleBuses_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /buses (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceBusesHtml);
        page.replace("%I2C%", listI2cHtml_());
        page.replace("%OW%", listOwHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        request->send(200, "text/html", page);
    }

    void handleStack_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /stack (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceStackHtml);
        const auto role = stackRole_();
        page.replace("%STACK_ROLE%", stackRoleName_(role));
        page.replace("%STACK_ROLE_MASTER_SEL%", role == ConfigsManagerIface::StackRole::Master ? "selected" : "");
        page.replace("%STACK_ROLE_SLAVE_SEL%", role == ConfigsManagerIface::StackRole::Slave ? "selected" : "");
        page.replace("%STACK_MASTER_HOST%", stackMasterHost_());
        page.replace("%STACK_STATUS%", _stack_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        request->send(200, "text/html", page);
    }

    void handleTelegram_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /telegram (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceTelegramHtml);
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
        request->send(200, "text/html", page);
    }

    void handleTelegramSave_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
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
            _tgbot_status = "Нет изменений";
        else if (!save_ok)
            _tgbot_status = "Ошибка сохранения";
        else
            _tgbot_status = "Сохранено";

        request->redirect("/telegram");
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
            items += " B</td><td class=\"right\"><a class=\"del\" onclick=\"return confirm('Удалить файл ";
            items += name;
            items += "?')\" href=\"/delete?path=";
            items += path;
            items += "\"><strong>Удалить</strong></a></td></tr>";
            file = root.openNextFile();
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>Нет файлов</strong></td></tr>";
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
        case PortIO::Location::Unit1:
            return "UNIT_1";
        case PortIO::Location::Unit2:
            return "UNIT_2";
        case PortIO::Location::Unit3:
            return "UNIT_3";
        case PortIO::Location::Unit4:
            return "UNIT_4";
        case PortIO::Location::Unit5:
            return "UNIT_5";
        case PortIO::Location::Unit6:
            return "UNIT_6";
        case PortIO::Location::Unit7:
            return "UNIT_7";
        case PortIO::Location::Unit8:
            return "UNIT_8";
        case PortIO::Location::Unit9:
            return "UNIT_9";
        case PortIO::Location::Unit10:
            return "UNIT_10";
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
            items = "<tr><td colspan=\"8\" style=\"color:#94a3b8\"><strong>Нет портов</strong></td></tr>";
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
            std::vector<uint8_t> addrs;
            if (!_i2c->scanDevices(bus, addrs))
                continue;
            for (size_t a = 0; a < addrs.size(); ++a)
            {
                char addr_buf[8] = {};
                snprintf(addr_buf, sizeof(addr_buf), "0x%02X", addrs[a]);
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
                _upload_error = "РќРµРґРѕРїСѓСЃС‚РёРјРѕРµ СЂР°СЃС€РёСЂРµРЅРёРµ С„Р°Р№Р»Р°";
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
            _upload_error = "Р¤Р°Р№Р» СЃР»РёС€РєРѕРј Р±РѕР»СЊС€РѕР№";
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
            _ota_error = "OTA РЅРµ РїРѕРґРґРµСЂР¶РёРІР°РµС‚СЃСЏ";
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
            _ota_error = "РЎР»РёС€РєРѕРј Р±РѕР»СЊС€РѕР№ С„Р°Р№Р» РїСЂРѕС€РёРІРєРё";
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
            _last_status = _upload_error.length() ? _upload_error : "Р—Р°РіСЂСѓР·РєР° РЅРµ СѓРґР°Р»Р°СЃСЊ";
        else
            _last_status = "Р—Р°РіСЂСѓР·РєР° Р·Р°РІРµСЂС€РµРЅР°";
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
            _last_status = _ota_error.length() ? _ota_error : "РћР±РЅРѕРІР»РµРЅРёРµ РїСЂРѕС€РёРІРєРё РЅРµ СѓРґР°Р»РѕСЃСЊ";
        else
            _last_status = "РџСЂРѕС€РёРІРєР° РѕР±РЅРѕРІР»РµРЅР°. РџРµСЂРµР·Р°РіСЂСѓР·РєР°...";
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
            _wifi_status = "РќРµС‚ РёР·РјРµРЅРµРЅРёР№";
        else if (!wifi_ok && !save_ok)
            _wifi_status = "РќРµ СѓРґР°Р»РѕСЃСЊ РїСЂРёРјРµРЅРёС‚СЊ Wi-Fi Рё СЃРѕС…СЂР°РЅРёС‚СЊ РєРѕРЅС„РёРіСѓСЂР°С†РёСЋ";
        else if (!wifi_ok)
            _wifi_status = "РќРµ СѓРґР°Р»РѕСЃСЊ РїСЂРёРјРµРЅРёС‚СЊ Wi-Fi";
        else if (!save_ok)
            _wifi_status = "Wi-Fi РїСЂРёРјРµРЅРµРЅ, РЅРѕ СЃРѕС…СЂР°РЅРёС‚СЊ РєРѕРЅС„РёРіСѓСЂР°С†РёСЋ РЅРµ СѓРґР°Р»РѕСЃСЊ";
        else
            _wifi_status = "Wi-Fi РѕР±РЅРѕРІР»РµРЅ";

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

    void handleStackSave_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (!_configs_manager)
        {
            _stack_status = "Config manager missing";
            request->redirect("/");
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

        request->redirect("/stack");
    }

    void handleDeviceSave_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (!_plc)
        {
            _device_status = "PLC missing";
            request->redirect("/");
            return;
        }
        if (!request->hasParam("device_name", true))
        {
            _device_status = "Missing name";
            request->redirect("/");
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

        request->redirect("/");
    }

    void handleReboot_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
#if defined(ESP32)
        if (_log && _log->ready())
            _log->info(F("WEB"), F("Reboot request (ip=%s)"), requestIp_(request).c_str());
        request->send(200, "text/plain", "Rebooting");
        delay(100);
        ESP.restart();
#else
        request->send(200, "text/plain", "Not supported");
#endif
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
            request->send(404, "text/plain", "Р¤Р°Р№Р» РЅРµ РЅР°Р№РґРµРЅ");
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
            request->send(400, "text/plain", "РќРµ СѓРєР°Р·Р°РЅ РїСѓС‚СЊ");
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
            request->send(404, "text/plain", "Р¤Р°Р№Р» РЅРµ РЅР°Р№РґРµРЅ");
            return;
        }
        if (!LittleFS.remove(path))
        {
            if (_log && _log->ready())
                _log->warn(F("WEB"), F("Delete failed %s (ip=%s)"), path.c_str(), requestIp_(request).c_str());
            request->send(500, "text/plain", "РќРµ СѓРґР°Р»РѕСЃСЊ СѓРґР°Р»РёС‚СЊ С„Р°Р№Р»");
            return;
        }
        request->redirect("/manage");
    }

    void handleStatus_(AsyncWebServerRequest *request)
    {
        if (!checkAuth_(request))
            return;
        if (_log && _log->ready())
            _log->info(F("WEB"), F("GET /status (ip=%s)"), requestIp_(request).c_str());
        String page = FPSTR(kWebInterfaceStatusHtml);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        page.replace("%STATUS%", _last_status.length() ? _last_status : "РќРµС‚ РґР°РЅРЅС‹С…");
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
        return String(" | STA: <strong>") + wifiStaStatus_() + "</strong>";
    }

    String wifiStaStatus_() const
    {
        switch (WiFi.status())
        {
        case WL_IDLE_STATUS:
            return "Ожидание";
        case WL_NO_SSID_AVAIL:
            return "SSID не найден";
        case WL_SCAN_COMPLETED:
            return "Сканирование завершено";
        case WL_CONNECTED:
            return "Подключено";
        case WL_CONNECT_FAILED:
            return "Ошибка подключения";
        case WL_CONNECTION_LOST:
            return "Связь потеряна";
        case WL_DISCONNECTED:
            return "Отключено";
        default:
            return "Неизвестно";
        }
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
        return _plc->fanStatus() ? "Вкл" : "Выкл";
    }

    AsyncWebServer &_server;
    WifiManager &_wifi;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;
    PlcControl *_plc = nullptr;
    RTC *_rtc = nullptr;
    TelegramClient *_tgbot = nullptr;
    TelegramMenu *_tgbot_menu = nullptr;
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
    bool _auth_enabled = false;
    String _auth_user;
    String _auth_pass;
    const CliConsole *_cli_auth = nullptr;
    Extender *_ext = nullptr;
    Logger *_log = nullptr;
};





