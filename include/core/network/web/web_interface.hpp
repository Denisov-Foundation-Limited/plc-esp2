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
#include "core/network/stack/stack_cache.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/stack/stack_slave_handler.hpp"
#include "core/network/web/interfaces/web_interface_pages.hpp"
#include "core/network/web/interfaces/web_interface_handler_fwd.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/users_registry.hpp"
#include "utils/fs_config.hpp"
#include "utils/configs_manager_iface.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "core/network/gsm_modem.hpp"
#include "core/network/cloud/cloud_client.hpp"
#include "core/display_slots.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/ibutton.hpp"
#include "controllers/controllers.hpp"
#include "core/rules_controller.hpp"
#include "core/network/web/interfaces/web_interface_assets.hpp"

class WebInterface
{
public:
    WebInterface(AsyncWebServer &server, CliConsole &cli, WifiManager &wifi, Configs &configs, PlcControl &plc,
                 RTC &rtc, TelegramClient &tgbot, TelegramBot &tgbot_bot, TelegramMenu &tgbot_menu, Logger &logs,
                 Extender &ext,
                 I2CManager &i2c, OneWireManager &ow, Controllers &controllers, RulesController &rules)
        : _server(server),
          _cli_auth(&cli),
          _wifi(wifi),
          _configs(configs),
          _plc(&plc),
          _rtc(&rtc),
          _tgbot(&tgbot),
          _tgbot_bot(&tgbot_bot),
          _tgbot_menu(&tgbot_menu),
          _controllers(&controllers),
          _rules(&rules),
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
    void setStackCache(StackCache &cache)
    {
        _stack_cache = &cache;
        if (_log)
            _stack_cache->setLogger(_log);
        if (_configs_manager)
            _stack_cache->setConfigsManager(_configs_manager);
        if (_stack_master)
            _stack_cache->setStackMaster(_stack_master);
    }
    void setConfigsManager(ConfigsManagerIface &mgr)
    {
        _configs_manager = &mgr;
        if (_stack_cache)
            _stack_cache->setConfigsManager(&mgr);
    }
    void setUsersRegistry(UsersRegistry &users) { _users = &users; }
    void setStackMaster(StackMaster &master)
    {
        _stack_master = &master;
        if (_stack_cache)
            _stack_cache->setStackMaster(&master);
        _stack_master->setFrameHandlerSecondary(&WebInterface::onStackFrame_, this);
    }
    void setStackSlave(StackSlaveHandler *slave) { _stack_slave = slave; }
    void setCloudClient(CloudClient &client) { _cloud = &client; }
    void setRules(RulesController &rules) { _rules = &rules; }

    StackCache &stackCache() { return *_stack_cache; }
    const StackCache &stackCache() const { return *_stack_cache; }
    void logStackCacheAllocations()
    {
        if (_stack_cache)
            _stack_cache->logAllocations();
    }

    void registerRoutes();

private:
    friend class ControllersHandler;
    friend class SocketsHandler;
    friend class LightsHandler;
    friend class ThermoHandler;
    friend class IndexHandler;
    friend class WifiHandler;
    friend class ManageHandler;
    friend class PortsHandler;
    friend class BusesHandler;
    friend class StackHandler;
    friend class UsersHandler;
    friend class DisplayHandler;
    friend class AdminHandler;
    friend class LogsHandler;
    friend class StatusHandler;
    friend class SepticHandler;
    friend class RingHandler;
    friend class SecurityHandler;
    friend class TelegramHandler;
    friend class CloudHandler;
    friend class MeteoHandler;
    friend class TankHandler;
    friend class WateringHandler;
    friend class AvrHandler;
    friend class LeakHandler;
    friend class RulesHandler;
    struct StackSocketItem;
    struct StackSocketsCache;
    struct StackLightItem;
    struct StackLightsCache;
    struct StackPortItem;
    struct StackPortsCache;
    struct StackExtenderItem;
    struct StackExtendersCache;
    struct StackI2cItem;
    struct StackI2cCache;
    struct StackOwItem;
    struct StackOwCache;
    struct StackSecuritySensorItem;
    struct StackSecurityCache;
    struct StackMeteoItem;
    struct StackMeteoCache;
    struct StackThermoItem;
    struct StackThermoCache;
    struct StackSepticItem;
    struct StackSepticCache;
    struct StackTankItem;
    struct StackTankCache;
    struct StackWateringItem;
    struct StackWateringCache;
    struct StackNodeStatusCache;
    void handleAdminSave_(AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (_cli_auth && _cli_auth->adminPasswordSet())
        {
            if (!checkAuth_(request, &set_cookie))
                return;
        }
        const bool has_rtc = request->hasParam("rtc_date", true) || request->hasParam("rtc_time", true);
        const bool has_pass = request->hasParam("password", true);

        if (has_rtc)
        {
            if (!_rtc)
            {
                sendText_(request, 500, "text/plain", "RTC unavailable", set_cookie);
                return;
            }
            if (!request->hasParam("rtc_date", true) || !request->hasParam("rtc_time", true))
            {
                sendText_(request, 400, "text/plain", "Missing RTC date/time", set_cookie);
                return;
            }
            String date = request->getParam("rtc_date", true)->value();
            String time = request->getParam("rtc_time", true)->value();
            date.trim();
            time.trim();
            if (!setRtc_(date, time))
            {
                sendText_(request, 400, "text/plain", "Invalid RTC datetime", set_cookie);
                return;
            }
        }

        if (has_pass)
        {
            if (!_cli_auth)
            {
                sendText_(request, 500, "text/plain", "CLI auth unavailable", set_cookie);
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
        }

        if (!has_rtc && !has_pass)
        {
            sendText_(request, 400, "text/plain", "Missing data", set_cookie);
            return;
        }

        sendRedirect_(request, "/admin", set_cookie);
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

            const char *state = "n/a";
            bool state_value = false;
            if (_plc && _plc->portState((uint8_t)i, state_value))
                state = state_value ? "1" : "0";

            if (p.backend == PortIO::Backend::Extender)
            {
                items += String(p.u.ext.dev);
                items += "</strong></td><td class=\"right\"><strong>";
                items += String(p.u.ext.pin);
                items += "</strong></td><td><strong>";
                items += state;
                items += "</strong></td><td><strong>";
                items += extDevTypeName_(p.u.ext.dev);
            }
            else
            {
                items += "--</strong></td><td class=\"right\"><strong>";
                items += String(p.u.esp.gpio);
                items += "</strong></td><td><strong>";
                items += state;
                items += "</strong></td><td><strong>CPU";
            }
            items += "</strong></td></tr>";
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"9\" style=\"color:#94a3b8\"><strong>No ports</strong></td></tr>";
        return items;
    }

    String listExtendersHtml_()
    {
        if (!_ext)
            return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Extenders unavailable</strong></td></tr>";
        const auto *devs = _ext->devs();
        if (!devs)
            return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Extenders unavailable</strong></td></tr>";
        String items;
        items.reserve(512);
        for (uint8_t i = 0; i < _ext->devCount(); ++i)
        {
            const auto &d = devs[i];
            if (d.i2c_addr == 0 || d.type == Extender::Type::None)
                continue;
            if (!_ext->isPresent(i))
                continue;
            char addr_buf[8] = {};
            snprintf(addr_buf, sizeof(addr_buf), "0x%02X", (unsigned)d.i2c_addr);
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)i);
            items += "</strong></td><td class=\"right\"><strong>";
            items += String((unsigned)d.bus_num);
            items += "</strong></td><td><strong>";
            items += addr_buf;
            items += "</strong></td><td><strong>";
            items += extTypeName_(d.type);
            items += "</strong></td></tr>";
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Extenders отсутствуют</strong></td></tr>";
        return items;
    }

    String listStackPortsHtml_(uint32_t node_id) const
    {
        const StackPortsCache *cache = findStackPortsCache_(node_id, false);
        if (!cache)
            return "<tr><td colspan=\"9\" style=\"color:#94a3b8\"><strong>n/a</strong></td></tr>";
        if (cache->pending)
            return "<tr><td colspan=\"9\" style=\"color:#94a3b8\"><strong>pending</strong></td></tr>";
        if (!cache->has_data)
            return "<tr><td colspan=\"9\" style=\"color:#94a3b8\"><strong>no data</strong></td></tr>";
        String items;
        items.reserve(cache->item_count * 120 + 128);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackPortItem &it = cache->items[i];
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)it.id);
            items += "</strong></td><td><strong>";
            if (it.backend[0])
                appendHtmlEscaped_(items, it.backend);
            else
                items += "n/a";
            items += "</strong></td><td><strong>";
            if (it.loc[0])
                appendHtmlEscaped_(items, it.loc);
            else
                items += "n/a";
            items += "</strong></td><td><strong>";
            if (it.type[0])
                appendHtmlEscaped_(items, it.type);
            else
                items += "n/a";
            items += "</strong></td><td><strong>";
            items += it.ctrl ? "yes" : "no";
            items += "</strong></td><td class=\"right\"><strong>";
            if (it.is_extender)
                items += String((int)it.dev);
            else
                items += "--";
            items += "</strong></td><td class=\"right\"><strong>";
            if (it.pin >= 0)
                items += String((int)it.pin);
            else
                items += "--";
            items += "</strong></td><td><strong>n/a";
            items += "</strong></td><td><strong>";
            if (it.hw[0])
                appendHtmlEscaped_(items, it.hw);
            else
                items += "n/a";
            items += "</strong></td></tr>";
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"9\" style=\"color:#94a3b8\"><strong>No ports</strong></td></tr>";
        return items;
    }

    String listStackExtendersHtml_(uint32_t node_id) const
    {
        const StackExtendersCache *cache = findStackExtendersCache_(node_id, false);
        if (!cache)
            return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>n/a</strong></td></tr>";
        if (cache->pending)
            return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>pending</strong></td></tr>";
        if (!cache->has_data)
            return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>no data</strong></td></tr>";
        String items;
        items.reserve(cache->item_count * 64 + 96);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackExtenderItem &it = cache->items[i];
            if (!it.present)
                continue;
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)it.id);
            items += "</strong></td><td class=\"right\"><strong>";
            items += String((unsigned)it.bus);
            items += "</strong></td><td><strong>";
            if (it.addr[0])
                appendHtmlEscaped_(items, it.addr);
            else
                items += "n/a";
            items += "</strong></td><td><strong>";
            if (it.type[0])
                appendHtmlEscaped_(items, it.type);
            else
                items += "n/a";
            items += "</strong></td></tr>";
        }
        if (items.length() == 0)
            items = "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Extenders отсутствуют</strong></td></tr>";
        return items;
    }

#include "core/network/web/interfaces/web_interface_controllers.hpp"

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

    static void appendHtmlEscaped_(String &out, const String &in)
    {
        appendHtmlEscaped_(out, in.c_str());
    }

    static void copyStr_(char *dst, size_t size, const char *src)
    {
        if (!dst || size == 0)
            return;
        if (!src)
        {
            dst[0] = '\0';
            return;
        }
        strlcpy(dst, src, size);
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
        dtostrf(temp_c, 0, 1, buf);
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

    bool setRtc_(const String &date, const String &time)
    {
        if (!_rtc)
            return false;
        const int p1 = date.indexOf('-');
        const int p2 = (p1 >= 0) ? date.indexOf('-', p1 + 1) : -1;
        const int t1 = time.indexOf(':');
        const int t2 = (t1 >= 0) ? time.indexOf(':', t1 + 1) : -1;
        if (p1 <= 0 || p2 <= p1 || t1 <= 0 || t2 <= t1)
            return false;
        const uint16_t year = (uint16_t)date.substring(0, p1).toInt();
        const uint8_t month = (uint8_t)date.substring(p1 + 1, p2).toInt();
        const uint8_t day = (uint8_t)date.substring(p2 + 1).toInt();
        const uint8_t hour = (uint8_t)time.substring(0, t1).toInt();
        const uint8_t min = (uint8_t)time.substring(t1 + 1, t2).toInt();
        const uint8_t sec = (uint8_t)time.substring(t2 + 1).toInt();
        if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31)
            return false;
        if (hour > 23 || min > 59 || sec > 59)
            return false;
        Ds3231Mz::DateTime dt{};
        dt.year = year;
        dt.month = month;
        dt.day = day;
        dt.day_of_week = calcDow_(year, month, day);
        dt.hour = hour;
        dt.minute = min;
        dt.second = sec;
        return _rtc->setTime(dt);
    }

    static uint8_t calcDow_(uint16_t y, uint8_t m, uint8_t d)
    {
        static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (m < 3)
            y -= 1;
        const uint8_t dow = (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
        return (uint8_t)(dow + 1);
    }

    void notifyRingPress_(bool stack_view, uint32_t node_id)
    {
        String msg = F("Звонок: веб-кнопка");
        if (stack_view)
        {
            msg += F(" (стек");
            if (node_id)
            {
                msg += F(" ");
                msg += stackNodeIdHex_(node_id);
            }
            msg += F(")");
        }
        else
        {
            msg += F(" (локально)");
        }
        if (_log)
            _log->info(F("RING"), F("%s"), msg.c_str());
        sendTelegramNotify_(msg);
    }

    void sendTelegramNotify_(const String &msg)
    {
        if (!_tgbot_bot || !_tgbot_menu)
            return;
        const auto users = _tgbot_menu->allowedUsers();
        if (users.empty())
            return;
        for (size_t i = 0; i < users.size; ++i)
        {
            const auto &user = users[i];
            if (!user.enabled || !user.is_notify || user.chat_id == 0)
                continue;
            _tgbot_bot->sendText(user.chat_id, msg);
        }
    }

    static void appendJsonEscaped_(String &out, const String &value)
    {
        for (size_t i = 0; i < (size_t)value.length(); ++i)
        {
            const char c = value[i];
            switch (c)
            {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if ((unsigned char)c < 0x20)
                    out += ' ';
                else
                    out += c;
                break;
            }
        }
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

    String fanStatusIcon_(bool on) const
    {
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

    void sendRedirect_(AsyncWebServerRequest *request, const String &path, bool set_cookie)
    {
        sendRedirect_(request, path.c_str(), set_cookie);
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
        hashAdd_(hash, _cloud_status);
        hashAdd_(hash, _stack_status);
        hashAdd_(hash, _device_status);
        hashAdd_(hash, _sockets_status);
        hashAdd_(hash, _controllers_status);
        hashAdd_(hash, _sockets_status);
        hashAdd_(hash, _lights_status);
        hashAdd_(hash, _meteo_status);
        hashAdd_(hash, _thermo_status);
        hashAdd_(hash, _tanks_status);
        hashAdd_(hash, _watering_status);
        hashAdd_(hash, _septic_status);
        hashAdd_(hash, _ring_status);
        hashAdd_(hash, _avr_status);
        hashAdd_(hash, _leak_status);
        hashAdd_(hash, _security_status);
        hashAdd_(hash, _rules_status);

        if (path == "/" || path == "/index")
        {
            hashAdd_(hash, deviceName_());
            hashAdd_(hash, stackRoleName_(stackRole_()));
            hashAdd_(hash, wifiIp_());
            hashAdd_(hash, formatTemp_(boardTemp_()));
            hashAdd_(hash, formatTemp_(cpuTemp_()));
            hashAdd_(hash, formatTemp_(rtcTemp_()));
            hashAdd_(hash, listStackNodesStatusHtml_());
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
        if (path == "/stack")
        {
            hashAdd_(hash, stackRoleName_(stackRole_()));
            hashAdd_(hash, stackMasterHost_());
            if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
            {
                hashAdd_(hash, _stack_slave->nodeConnected() ? 1u : 0u);
                hashAdd_(hash, _stack_slave->linkReadyAfterHello() ? 1u : 0u);
            }
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
                hashAdd_(hash, _controllers->watering().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->septic().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->ring().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->avr().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->leak().controllerEnabled() ? 1u : 0u);
                hashAdd_(hash, _controllers->security().controllerEnabled() ? 1u : 0u);
            }
            return hash;
        }
        if (path == "/rules")
        {
            if (_rules)
            {
                for (size_t i = 1; i <= RulesController::kRuleCount; ++i)
                {
                    const auto *r = _rules->rule(i);
                    if (!r)
                        continue;
                    hashAdd_(hash, (uint32_t)r->id);
                    hashAdd_(hash, r->enabled ? 1u : 0u);
                    hashAdd_(hash, r->name);
                    hashAdd_(hash, r->condition_enabled ? 1u : 0u);
                    hashAdd_(hash, r->condition_node_id);
                    hashAdd_(hash, r->condition_controller);
                    hashAdd_(hash, (uint32_t)r->condition_item_id);
                    hashAdd_(hash, r->condition_parameter);
                    hashAdd_(hash, r->condition_op);
                    hashAdd_(hash, r->condition_value);
                    for (size_t j = 1; j <= RulesController::kActionCount; ++j)
                    {
                        const auto *a = _rules->action(i, j);
                        if (!a)
                            continue;
                        hashAdd_(hash, (uint32_t)a->id);
                        hashAdd_(hash, a->enabled ? 1u : 0u);
                        hashAdd_(hash, (uint32_t)a->kind);
                        hashAdd_(hash, (uint32_t)a->delay_ms);
                        hashAdd_(hash, a->node_id);
                        hashAdd_(hash, a->controller);
                        hashAdd_(hash, a->parameter);
                        hashAdd_(hash, a->value);
                    }
                }
            }
            return hash;
        }
        if (path == "/users")
        {
            if (_users)
            {
                for (size_t i = 0; i < _users->size(); ++i)
                {
                    const auto &u = _users->user(i);
                    hashAdd_(hash, (uint32_t)u.id);
                    hashAdd_(hash, u.enabled ? 1u : 0u);
                    hashAdd_(hash, u.username);
                    hashAdd_(hash, u.tg_username);
                    hashAdd_(hash, String((long long)u.tg_chat_id));
                    hashAdd_(hash, u.tg_admin ? 1u : 0u);
                    hashAdd_(hash, u.tg_notify ? 1u : 0u);
                    hashAdd_(hash, u.gsm_phone);
                    hashAdd_(hash, u.gsm_sms ? 1u : 0u);
                    hashAdd_(hash, u.gsm_call ? 1u : 0u);
                    hashAdd_(hash, u.ibutton_key);
                    hashAdd_(hash, u.rfid_key);
                }
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
            const StackSocketsCache &c = _stack_sockets_cache;
            if (c.node_id != 0)
            {
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackSocketItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.enabled ? 1u : 0u);
                    hashAdd_(hash, it.state ? 1u : 0u);
                    hashAdd_(hash, it.name);
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
            const StackLightsCache &c = _stack_lights_cache;
            if (c.node_id != 0)
            {
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackLightItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.enabled ? 1u : 0u);
                    hashAdd_(hash, it.state ? 1u : 0u);
                    hashAdd_(hash, it.name);
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
            if (_stack_meteo_cache.node_id != 0)
            {
                const StackMeteoCache &c = _stack_meteo_cache;
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackMeteoItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.enabled ? 1u : 0u);
                    hashAdd_(hash, it.ok ? 1u : 0u);
                    hashAdd_(hash, it.has_temp ? 1u : 0u);
                    hashAdd_(hash, it.has_hum ? 1u : 0u);
                    hashAdd_(hash, scaled10_(it.temp_c));
                    hashAdd_(hash, scaled10_(it.hum));
                    hashAdd_(hash, it.name);
                    hashAdd_(hash, it.type);
                    hashAdd_(hash, it.addr);
                    hashAdd_(hash, it.pin);
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
            if (_stack_thermo_cache.node_id != 0)
            {
                const StackThermoCache &c = _stack_thermo_cache;
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackThermoItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.enabled ? 1u : 0u);
                    hashAdd_(hash, it.power_on ? 1u : 0u);
                    hashAdd_(hash, it.heat_on ? 1u : 0u);
                    hashAdd_(hash, it.cool_on ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)it.sensor);
                    hashAdd_(hash, scaled10_(it.target));
                    hashAdd_(hash, scaled10_(it.hyst));
                    hashAdd_(hash, (uint32_t)it.heat);
                    hashAdd_(hash, (uint32_t)it.cool);
                    hashAdd_(hash, (uint32_t)it.button);
                    hashAdd_(hash, it.name);
                    hashAdd_(hash, it.mode);
                }
            }
            if (_stack_meteo_cache.node_id != 0)
            {
                const StackMeteoCache &c = _stack_meteo_cache;
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackMeteoItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.has_temp ? 1u : 0u);
                    hashAdd_(hash, scaled10_(it.temp_c));
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
            if (_stack_tanks_cache.node_id != 0)
            {
                const StackTankCache &c = _stack_tanks_cache;
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackTankItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.enabled ? 1u : 0u);
                    hashAdd_(hash, it.power_on ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)it.low);
                    hashAdd_(hash, (uint32_t)it.mid);
                    hashAdd_(hash, (uint32_t)it.full);
                    hashAdd_(hash, (uint32_t)it.valve);
                    hashAdd_(hash, (uint32_t)it.pump);
                    hashAdd_(hash, (uint32_t)it.alarm);
                    hashAdd_(hash, it.level_low ? 1u : 0u);
                    hashAdd_(hash, it.level_mid ? 1u : 0u);
                    hashAdd_(hash, it.level_full ? 1u : 0u);
                    hashAdd_(hash, it.levels_ok ? 1u : 0u);
                    hashAdd_(hash, it.valve_on ? 1u : 0u);
                    hashAdd_(hash, it.pump_on ? 1u : 0u);
                    hashAdd_(hash, it.alarm_on ? 1u : 0u);
                    hashAdd_(hash, it.name);
                }
            }
            return hash;
        }
        if (path == "/watering")
        {
            if (_controllers)
            {
                WateringController &watering = _controllers->watering();
                for (size_t i = 0; i < WateringController::kRuleCount; ++i)
                {
                    const auto *cfg = watering.configByIndex(i);
                    const auto *st = watering.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->port);
                    hashAdd_(hash, (uint32_t)cfg->tank_id);
                    hashAdd_(hash, (uint32_t)cfg->weekdays_mask);
                    hashAdd_(hash, (uint32_t)cfg->hour);
                    hashAdd_(hash, (uint32_t)cfg->minute);
                    hashAdd_(hash, (uint32_t)cfg->duration_sec);
                    hashAdd_(hash, (uint32_t)cfg->hour2);
                    hashAdd_(hash, (uint32_t)cfg->minute2);
                    hashAdd_(hash, (uint32_t)cfg->duration2_sec);
                    hashAdd_(hash, (uint32_t)cfg->hour3);
                    hashAdd_(hash, (uint32_t)cfg->minute3);
                    hashAdd_(hash, (uint32_t)cfg->duration3_sec);
                    hashAdd_(hash, cfg->resume_after_refill ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->resume_level);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->status ? 1u : 0u);
                    hashAdd_(hash, st->active ? 1u : 0u);
                    hashAdd_(hash, st->paused ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)st->remaining_ms);
                }
            }
            if (_stack_watering_cache.node_id != 0)
            {
                const StackWateringCache &c = _stack_watering_cache;
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                hashAdd_(hash, (uint32_t)c.total);
                hashAdd_(hash, (uint32_t)c.offset);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackWateringItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.enabled ? 1u : 0u);
                    hashAdd_(hash, it.status ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)it.port);
                    hashAdd_(hash, (uint32_t)it.tank_id);
                    hashAdd_(hash, (uint32_t)it.weekdays_mask);
                    hashAdd_(hash, (uint32_t)it.hour);
                    hashAdd_(hash, (uint32_t)it.minute);
                    hashAdd_(hash, (uint32_t)it.duration_sec);
                    hashAdd_(hash, (uint32_t)it.hour2);
                    hashAdd_(hash, (uint32_t)it.minute2);
                    hashAdd_(hash, (uint32_t)it.duration2_sec);
                    hashAdd_(hash, (uint32_t)it.hour3);
                    hashAdd_(hash, (uint32_t)it.minute3);
                    hashAdd_(hash, (uint32_t)it.duration3_sec);
                    hashAdd_(hash, it.resume_after_refill ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)it.resume_level);
                    hashAdd_(hash, it.active ? 1u : 0u);
                    hashAdd_(hash, it.paused ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)it.remaining_ms);
                    hashAdd_(hash, it.name);
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
            if (_stack_septic_cache.node_id != 0)
            {
                const StackSepticCache &c = _stack_septic_cache;
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackSepticItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.enabled ? 1u : 0u);
                    hashAdd_(hash, it.monitor ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)it.warning_port);
                    hashAdd_(hash, (uint32_t)it.alarm_port);
                    hashAdd_(hash, (uint32_t)it.relay_warning);
                    hashAdd_(hash, (uint32_t)it.relay_alarm);
                    hashAdd_(hash, it.warning ? 1u : 0u);
                    hashAdd_(hash, it.alarm ? 1u : 0u);
                }
            }
            return hash;
        }
        if (path == "/ring")
        {
            if (_controllers)
            {
                const auto &cfg = _controllers->ring().config();
                hashAdd_(hash, cfg.enabled ? 1u : 0u);
                hashAdd_(hash, (uint32_t)cfg.button_port);
                hashAdd_(hash, (uint32_t)cfg.relay_port);
            }
            hashAdd_(hash, _ring_status);
            return hash;
        }
        if (path == "/avr")
        {
            if (_controllers)
            {
                const auto &cfg = _controllers->avr().config();
                const auto &st = _controllers->avr().state();
                hashAdd_(hash, cfg.enabled ? 1u : 0u);
                hashAdd_(hash, cfg.auto_mode ? 1u : 0u);
                hashAdd_(hash, cfg.prefer_main ? 1u : 0u);
                hashAdd_(hash, cfg.auto_return_main ? 1u : 0u);
                hashAdd_(hash, (uint32_t)cfg.main_ok_port);
                hashAdd_(hash, (uint32_t)cfg.reserve_ok_port);
                hashAdd_(hash, (uint32_t)cfg.relay_main_port);
                hashAdd_(hash, (uint32_t)cfg.relay_reserve_port);
                hashAdd_(hash, (uint32_t)cfg.feedback_main_port);
                hashAdd_(hash, (uint32_t)cfg.feedback_reserve_port);
                hashAdd_(hash, cfg.main_ok_active_low ? 1u : 0u);
                hashAdd_(hash, cfg.reserve_ok_active_low ? 1u : 0u);
                hashAdd_(hash, cfg.feedback_main_active_low ? 1u : 0u);
                hashAdd_(hash, cfg.feedback_reserve_active_low ? 1u : 0u);
                hashAdd_(hash, cfg.relay_main_invert ? 1u : 0u);
                hashAdd_(hash, cfg.relay_reserve_invert ? 1u : 0u);
                hashAdd_(hash, cfg.debounce_ms);
                hashAdd_(hash, cfg.loss_delay_ms);
                hashAdd_(hash, cfg.return_delay_ms);
                hashAdd_(hash, cfg.break_ms);
                hashAdd_(hash, cfg.warmup_ms);
                hashAdd_(hash, cfg.transfer_timeout_ms);
                hashAdd_(hash, (uint32_t)st.active_source);
                hashAdd_(hash, (uint32_t)st.target_source);
                hashAdd_(hash, (uint32_t)st.manual_source);
                hashAdd_(hash, (uint32_t)st.fault);
                hashAdd_(hash, st.transfer_in_progress ? 1u : 0u);
                hashAdd_(hash, st.main_ok ? 1u : 0u);
                hashAdd_(hash, st.reserve_ok ? 1u : 0u);
            }
            hashAdd_(hash, _avr_status);
            return hash;
        }
        if (path == "/leak")
        {
            if (_controllers)
            {
                LeakController &leak = _controllers->leak();
                hashAdd_(hash, leak.controllerEnabled() ? 1u : 0u);
                for (size_t i = 0; i < LeakController::kZoneCount; ++i)
                {
                    const auto *cfg = leak.configByIndex(i);
                    const auto *st = leak.stateByIndex(i);
                    if (!cfg || !st)
                        continue;
                    hashAdd_(hash, (uint32_t)cfg->id);
                    hashAdd_(hash, cfg->enabled ? 1u : 0u);
                    hashAdd_(hash, cfg->power_on ? 1u : 0u);
                    hashAdd_(hash, cfg->sensor_active_low ? 1u : 0u);
                    hashAdd_(hash, cfg->valve_open_on_power ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)cfg->sensor_port);
                    hashAdd_(hash, (uint32_t)cfg->valve_port);
                    hashAdd_(hash, (uint32_t)cfg->alarm_port);
                    hashAdd_(hash, cfg->name);
                    hashAdd_(hash, st->wet ? 1u : 0u);
                    hashAdd_(hash, st->alarm_latched ? 1u : 0u);
                }
            }
            hashAdd_(hash, _leak_status);
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
            if (_stack_security_cache.node_id != 0)
            {
                const StackSecurityCache &c = _stack_security_cache;
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackSecuritySensorItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.enabled ? 1u : 0u);
                    hashAdd_(hash, it.detect ? 1u : 0u);
                    hashAdd_(hash, it.silent ? 1u : 0u);
                    hashAdd_(hash, (uint32_t)it.port);
                    hashAdd_(hash, it.type ? it.type : "");
                    hashAdd_(hash, it.name);
                }
            }
            return hash;
        }
        if (path == "/ports")
        {
            hashAdd_(hash, listPortsHtml_());
            hashAdd_(hash, listExtendersHtml_());
            if (_stack_ports_cache.node_id != 0)
            {
                const StackPortsCache &c = _stack_ports_cache;
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackPortItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, it.ctrl ? 1u : 0u);
                    hashAdd_(hash, it.is_extender ? 1u : 0u);
                    hashAdd_(hash, (int32_t)it.dev);
                    hashAdd_(hash, (int32_t)it.pin);
                    hashAdd_(hash, it.backend);
                    hashAdd_(hash, it.loc);
                    hashAdd_(hash, it.type);
                    hashAdd_(hash, it.hw);
                }
            }
            if (_stack_ext_cache.node_id != 0)
            {
                const StackExtendersCache &c = _stack_ext_cache;
                hashAdd_(hash, (uint32_t)c.node_id);
                hashAdd_(hash, (uint32_t)c.item_count);
                hashAdd_(hash, c.pending ? 1u : 0u);
                hashAdd_(hash, c.last_ok ? 1u : 0u);
                hashAdd_(hash, c.last_error);
                for (size_t j = 0; j < c.item_count; ++j)
                {
                    const StackExtenderItem &it = c.items[j];
                    hashAdd_(hash, (uint32_t)it.id);
                    hashAdd_(hash, (uint32_t)it.bus);
                    hashAdd_(hash, it.present ? 1u : 0u);
                    hashAdd_(hash, it.addr);
                    hashAdd_(hash, it.type);
                }
            }
            return hash;
        }
        if (path == "/buses")
        {
            hashAdd_(hash, listI2cHtml_());
            hashAdd_(hash, listOwHtml_());
            hashAdd_(hash, (uint32_t)_stack_i2c_cache.node_id);
            hashAdd_(hash, (uint32_t)_stack_i2c_cache.item_count);
            hashAdd_(hash, _stack_i2c_cache.pending ? 1u : 0u);
            hashAdd_(hash, _stack_i2c_cache.last_ok ? 1u : 0u);
            hashAdd_(hash, _stack_i2c_cache.last_error);
            for (size_t i = 0; i < _stack_i2c_cache.item_count; ++i)
            {
                hashAdd_(hash, (uint32_t)_stack_i2c_cache.items[i].bus);
                hashAdd_(hash, (uint32_t)_stack_i2c_cache.items[i].addr);
            }
            hashAdd_(hash, (uint32_t)_stack_ow_cache.node_id);
            hashAdd_(hash, (uint32_t)_stack_ow_cache.item_count);
            hashAdd_(hash, _stack_ow_cache.pending ? 1u : 0u);
            hashAdd_(hash, _stack_ow_cache.last_ok ? 1u : 0u);
            hashAdd_(hash, _stack_ow_cache.last_error);
            for (size_t i = 0; i < _stack_ow_cache.item_count; ++i)
            {
                hashAdd_(hash, (uint32_t)_stack_ow_cache.items[i].bus);
                hashAdd_(hash, _stack_ow_cache.items[i].addr);
                hashAdd_(hash, _stack_ow_cache.items[i].type);
            }
            return hash;
        }
        if (path == "/stack")
        {
            hashAdd_(hash, stackRoleName_(stackRole_()));
            hashAdd_(hash, stackMasterHost_());
            hashAdd_(hash, stackFallbackEnabled_() ? 1u : 0u);
            hashAdd_(hash, stackFallbackHost_());
            hashAdd_(hash, stackSlaveController_() ? 1u : 0u);
            hashAdd_(hash, stackApiKey_());
            hashAdd_(hash, listStackNodesHtml_());
            return hash;
        }
        if (path == "/admin")
        {
            if (_cli_auth)
                hashAdd_(hash, _cli_auth->adminPasswordSet() ? 1u : 0u);
            hashAdd_(hash, rtcDateStr_());
            hashAdd_(hash, rtcTimeOnlyStr_());
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
            return hash;
        }
        if (path == "/cloud")
        {
            hashAdd_(hash, cloudEnabled_() ? 1u : 0u);
            hashAdd_(hash, cloudHost_());
            hashAdd_(hash, (uint32_t)cloudPort_());
            hashAdd_(hash, cloudPath_());
            hashAdd_(hash, cloudUseSsl_() ? 1u : 0u);
            hashAdd_(hash, cloudReconnectMs_());
            hashAdd_(hash, cloudEventMs_());
            hashAdd_(hash, cloudApiKey_());
            hashAdd_(hash, cloudFwVersion_());
            hashAdd_(hash, cloudDeviceId_());
            hashAdd_(hash, cloudConnected_() ? 1u : 0u);
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
    TelegramBot *_tgbot_bot = nullptr;
    TelegramMenu *_tgbot_menu = nullptr;
    Controllers *_controllers = nullptr;
    RulesController *_rules = nullptr;
    GsmModem *_gsm = nullptr;
    I2CManager *_i2c = nullptr;
    OneWireManager *_ow = nullptr;
    struct StackSocketItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool state = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackSocketsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackSocketItem items[SocketController::kSocketCount] = {};
        size_t item_count = 0;
    };
    StackSocketsCache _stack_sockets_cache = {};
    struct StackLightItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool state = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackLightsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackLightItem items[SocketController::kLightCount] = {};
        size_t item_count = 0;
    };
    StackLightsCache _stack_lights_cache = {};
    struct StackPortItem
    {
        uint8_t id = 0;
        bool ctrl = false;
        bool is_extender = false;
        int16_t dev = -1;
        int16_t pin = -1;
        static constexpr size_t kBackendLen = 32;
        static constexpr size_t kLocLen = 32;
        static constexpr size_t kTypeLen = 32;
        static constexpr size_t kHwLen = 32;
        char backend[kBackendLen] = {};
        char loc[kLocLen] = {};
        char type[kTypeLen] = {};
        char hw[kHwLen] = {};
    };
    struct StackPortsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackPortItem items[PortIO::PORT_COUNT] = {};
        size_t item_count = 0;
    };
    StackPortsCache _stack_ports_cache = {};
    struct StackExtenderItem
    {
        uint8_t id = 0;
        uint8_t bus = 0;
        bool present = false;
        static constexpr size_t kAddrLen = 24;
        static constexpr size_t kTypeLen = 24;
        char addr[kAddrLen] = {};
        char type[kTypeLen] = {};
    };
    struct StackExtendersCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackExtenderItem items[Extender::MAX_DEVS] = {};
        size_t item_count = 0;
    };
    StackExtendersCache _stack_ext_cache = {};
    struct StackI2cItem
    {
        uint8_t bus = 0;
        uint8_t addr = 0;
    };
    struct StackI2cCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackI2cItem items[127] = {};
        size_t item_count = 0;
    };
    StackI2cCache _stack_i2c_cache = {};
    struct StackOwItem
    {
        uint8_t bus = 0;
        char addr[17] = {};
        char type[8] = {};
    };
    struct StackOwCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackOwItem items[64] = {};
        size_t item_count = 0;
    };
    StackOwCache _stack_ow_cache = {};
    struct StackSecuritySensorItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool detect = false;
        bool silent = false;
        uint8_t port = SecurityController::kInvalidPort;
        static constexpr size_t kTypeLen = 24;
        static constexpr size_t kNameLen = 48;
        char type[kTypeLen] = {};
        char name[kNameLen] = {};
    };
    struct StackSecurityCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackSecuritySensorItem items[SecurityController::kSensorCount] = {};
        size_t item_count = 0;
    };
    StackSecurityCache _stack_security_cache = {};
    struct StackMeteoItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool ok = false;
        bool has_temp = false;
        bool has_hum = false;
        float temp_c = 0.0f;
        float hum = 0.0f;
        static constexpr size_t kNameLen = 48;
        static constexpr size_t kTypeLen = 24;
        static constexpr size_t kAddrLen = 24;
        char name[kNameLen] = {};
        char type[kTypeLen] = {};
        char addr[kAddrLen] = {};
        int pin = -1;
    };
    struct StackMeteoCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackMeteoItem items[MeteoController::kSensorCount] = {};
        size_t item_count = 0;
    };
    StackMeteoCache _stack_meteo_cache = {};
    struct StackThermoItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool power_on = false;
        bool heat_on = false;
        bool cool_on = false;
        uint8_t sensor = 0;
        float target = 0.0f;
        float hyst = 0.0f;
        uint8_t heat = ThermoController::kInvalidPort;
        uint8_t cool = ThermoController::kInvalidPort;
        uint8_t button = ThermoController::kInvalidPort;
        static constexpr size_t kNameLen = 48;
        static constexpr size_t kModeLen = 24;
        char name[kNameLen] = {};
        char mode[kModeLen] = {};
    };
    struct StackThermoCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackThermoItem items[ThermoController::kDeviceCount] = {};
        size_t item_count = 0;
    };
    StackThermoCache _stack_thermo_cache = {};
    struct StackSepticItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool monitor = false;
        uint8_t warning_port = SepticController::kInvalidPort;
        uint8_t alarm_port = SepticController::kInvalidPort;
        uint8_t relay_warning = SepticController::kInvalidPort;
        uint8_t relay_alarm = SepticController::kInvalidPort;
        bool warning = false;
        bool alarm = false;
    };
    struct StackSepticCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackSepticItem items[SepticController::kSepticCount] = {};
        size_t item_count = 0;
    };
    StackSepticCache _stack_septic_cache = {};
    struct StackTankItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool power_on = false;
        uint8_t low = TankController::kInvalidPort;
        uint8_t mid = TankController::kInvalidPort;
        uint8_t full = TankController::kInvalidPort;
        uint8_t valve = TankController::kInvalidPort;
        uint8_t pump = TankController::kInvalidPort;
        uint8_t alarm = TankController::kInvalidPort;
        bool level_low = false;
        bool level_mid = false;
        bool level_full = false;
        bool levels_ok = false;
        bool valve_on = false;
        bool pump_on = false;
        bool alarm_on = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackTankCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackTankItem items[TankController::kTankCount] = {};
        size_t item_count = 0;
    };
    StackTankCache _stack_tanks_cache = {};
    struct StackWateringItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool status = false;
        uint8_t port = WateringController::kInvalidPort;
        uint8_t tank_id = 0;
        uint8_t weekdays_mask = 0;
        uint8_t hour = 0;
        uint8_t minute = 0;
        uint32_t duration_sec = 0;
        uint8_t hour2 = 0;
        uint8_t minute2 = 0;
        uint32_t duration2_sec = 0;
        uint8_t hour3 = 0;
        uint8_t minute3 = 0;
        uint32_t duration3_sec = 0;
        bool resume_after_refill = false;
        uint8_t resume_level = 0;
        bool active = false;
        bool paused = false;
        uint32_t remaining_ms = 0;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackWateringCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        uint16_t total = 0;
        uint16_t offset = 0;
        StackWateringItem items[WateringController::kRuleCount] = {};
        size_t item_count = 0;
    };
    StackWateringCache _stack_watering_cache = {};
    struct StackNodeStatusCache
    {
        uint32_t node_id = 0;
        uint32_t plc_updated_ms = 0;
        uint32_t rtc_updated_ms = 0;
        uint16_t pending_plc_cmd_id = 0;
        uint16_t pending_rtc_cmd_id = 0;
        bool pending_plc = false;
        bool pending_rtc = false;
        bool has_plc = false;
        bool has_rtc = false;
        bool last_plc_ok = false;
        bool last_rtc_ok = false;
        String last_plc_error;
        String last_rtc_error;
        float board_temp = 0.0f;
        float cpu_temp = 0.0f;
        bool fan_on = false;
        float fan_on_c = 0.0f;
        float fan_hyst_c = 0.0f;
        String rtc_date;
        String rtc_time;
        float rtc_temp = 0.0f;
        uint8_t rtc_weekday = 0;
    };
    StackNodeStatusCache _stack_status_cache[StackMaster::MAX_SESSIONS] = {};
    uint16_t _stack_cmd_id = 0;
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
    String _cloud_status;
    String _stack_status;
    String _device_status;
    String _sockets_status;
    String _lights_status;
    String _controllers_status;
    String _meteo_status;
    String _thermo_status;
    String _tanks_status;
    String _watering_status;
    String _septic_status;
    String _ring_status;
    String _avr_status;
    String _leak_status;
    String _security_status;
    String _rules_status;
    String _users_status;
    String _display_status;
    bool _auth_enabled = false;
    String _auth_user;
    String _auth_pass;
    CliConsole *_cli_auth = nullptr;
    Extender *_ext = nullptr;
    Logger *_log = nullptr;
    StackMaster *_stack_master = nullptr;
    StackSlaveHandler *_stack_slave = nullptr;
    StackCache *_stack_cache = nullptr;
    CloudClient *_cloud = nullptr;
    UsersRegistry *_users = nullptr;
    String _session_token;
    uint32_t _session_expire_ms = 0;
    uint32_t _session_ttl_ms = 10u * 60u * 1000u;
    bool _upload_set_cookie = false;
    bool _ota_set_cookie = false;
    bool _ota_in_progress = false;
};

#include "core/network/web/interfaces/web_interface_handlers.hpp"
#include "core/network/web/interfaces/web_interface_routes.hpp"





