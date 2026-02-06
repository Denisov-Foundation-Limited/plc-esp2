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
#include "core/network/web/pages/web_interface_page.hpp"
#include "core/network/web/pages/web_interface_manage.hpp"
#include "core/network/web/pages/web_interface_ports.hpp"
#include "core/network/web/pages/web_interface_buses.hpp"
#include "core/network/web/pages/web_interface_stack.hpp"
#include "core/network/web/pages/web_interface_wifi.hpp"
#include "core/network/web/pages/web_interface_controllers.hpp"
#include "core/network/web/pages/web_interface_clients.hpp"
#include "core/network/web/pages/web_interface_rfid.hpp"
#include "core/network/web/pages/web_interface_display.hpp"
#include "core/network/web/pages/web_interface_client_ring.hpp"
#include "core/network/web/pages/web_interface_security.hpp"
#include "core/network/web/pages/web_interface_ring.hpp"
#include "core/network/web/pages/web_interface_septic.hpp"
#include "core/network/web/pages/web_interface_meteo.hpp"
#include "core/network/web/pages/web_interface_thermo.hpp"
#include "core/network/web/pages/web_interface_tanks.hpp"
#include "core/network/web/pages/web_interface_admin.hpp"
#include "core/network/web/pages/web_interface_logs.hpp"
#include "core/network/web/pages/web_interface_telegram.hpp"
#include "core/network/web/pages/web_interface_cloud.hpp"
#include "core/network/web/pages/web_interface_status.hpp"
#include "core/network/web/pages/web_interface_sockets.hpp"
#include "core/network/web/pages/web_interface_lights.hpp"
class ControllersHandler;
class SocketsHandler;
class LightsHandler;
class ThermoHandler;
class IndexHandler;
class WifiHandler;
class ManageHandler;
class PortsHandler;
class BusesHandler;
class StackHandler;
class ClientsHandler;
class DisplayHandler;
class AdminHandler;
class LogsHandler;
class StatusHandler;
class SepticHandler;
class RingHandler;
class SecurityHandler;
class TelegramHandler;
class CloudHandler;
class MeteoHandler;
class TankHandler;
class RfidHandler;
class RingClientHandler;
class StackSlaveHandler;
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "utils/logger.hpp"
#include "utils/configs.hpp"
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
#include "clients/rfid_reader.hpp"
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
                 RTC &rtc, TelegramClient &tgbot, TelegramBot &tgbot_bot, TelegramMenu &tgbot_menu, Logger &logs,
                 Extender &ext,
                 I2CManager &i2c, OneWireManager &ow, Controllers &controllers, RfidReader &rfid)
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
          _rfid(&rfid),
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
    void setStackMaster(StackMaster &master)
    {
        _stack_master = &master;
        if (_stack_cache)
            _stack_cache->setStackMaster(&master);
        _stack_master->setFrameHandlerSecondary(&WebInterface::onStackFrame_, this);
    }
    void setStackSlave(StackSlaveHandler *slave) { _stack_slave = slave; }
    void setCloudClient(CloudClient &client) { _cloud = &client; }

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
    friend class ClientsHandler;
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
    friend class RfidHandler;
    friend class RingClientHandler;
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
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
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

    String listStackSocketsHtml_(uint32_t node_id)
    {
        StackSocketsCache *cache = findStackSocketsCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Розетки отсутствуют</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 420u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackSocketItem &cfg = cache->items[i];
            const bool on = cfg.state;
            items += "<div class=\"tile\">";
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
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Розетка";
            items += "</strong></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += on ? "status-on" : "status-off";
            items += "\"></span>";
            items += "<span class=\"status-text\">";
            items += on ? "Включена" : "Выключена";
            items += "</span></div>";
            items += "<div class=\"form-row\"><label>Перекл.</label>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-toggle\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\"";
            if (on)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div></div>";
        }
        return items;
    }

    String socketsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"sockets-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String indexDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\" style=\"margin: 6px 0 10px;\">";
        html += "<span class=\"status\">Устройство</span>";
        html += "<select id=\"index-device\" class=\"mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String lightsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"lights-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String securityDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"security-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String meteoDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"meteo-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String thermoDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"thermo-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String septicDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"septic-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String tanksDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"tanks-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String ringDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"ring-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String busesDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\" style=\"margin-bottom:10px;\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"buses-device\" class=\"field mini\" onchange=\"location.href='/buses?node=' + this.value;\">";
        html += "<option value=\"0\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String portsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\" style=\"margin-bottom:10px;\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"ports-device\" class=\"field mini\" onchange=\"location.href='/ports?node=' + this.value;\">";
        html += "<option value=\"0\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

    String stackSocketsStatusText_(uint32_t node_id) const
    {
        const StackSocketsCache *cache = findStackSocketsCache_(node_id, false);
        if (!cache)
            return "Нет данных со слейва";
        if (cache->pending)
            return "Запрос данных со слейва...";
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = "Ошибка: ";
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return "Нет данных со слейва";
        return "OK";
    }

    String stackMeteoStatusText_(uint32_t node_id) const
    {
        const StackMeteoCache *cache = findStackMeteoCache_(node_id, false);
        if (!cache)
            return "Нет данных со слейва";
        if (cache->pending)
            return "Запрос данных со слейва...";
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = "Ошибка: ";
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return "Нет данных со слейва";
        return "OK";
    }

    String stackThermoStatusText_(uint32_t node_id) const
    {
        const StackThermoCache *cache = findStackThermoCache_(node_id, false);
        if (!cache)
            return "Нет данных со слейва";
        if (cache->pending)
            return "Запрос данных со слейва...";
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = "Ошибка: ";
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return "Нет данных со слейва";
        return "OK";
    }

    String stackSepticStatusText_(uint32_t node_id) const
    {
        const StackSepticCache *cache = findStackSepticCache_(node_id, false);
        if (!cache)
            return "Нет данных со слейва";
        if (cache->pending)
            return "Запрос данных со слейва...";
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = "Ошибка: ";
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return "Нет данных со слейва";
        return "OK";
    }

    String stackTanksStatusText_(uint32_t node_id) const
    {
        const StackTankCache *cache = findStackTanksCache_(node_id, false);
        if (!cache)
            return "Нет данных со слейва";
        if (cache->pending)
            return "Запрос данных со слейва...";
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = "Ошибка: ";
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return "Нет данных со слейва";
        return "OK";
    }

    String stackLightsStatusText_(uint32_t node_id) const
    {
        const StackLightsCache *cache = findStackLightsCache_(node_id, false);
        if (!cache)
            return "Нет данных со слейва";
        if (cache->pending)
            return "Запрос данных со слейва...";
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = "Ошибка: ";
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return "Нет данных со слейва";
        return "OK";
    }

    String stackSecurityStatusText_(uint32_t node_id) const
    {
        const StackSecurityCache *cache = findStackSecurityCache_(node_id, false);
        if (!cache)
            return "Нет данных со слейва";
        if (cache->pending)
            return "Запрос данных со слейва...";
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = "Ошибка: ";
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return "Нет данных со слейва";
        return "OK";
    }

    String stackBusesStatusText_(uint32_t node_id) const
    {
        const StackI2cCache *i2c = findStackI2cCache_(node_id, false);
        const StackOwCache *ow = findStackOwCache_(node_id, false);
        if (!i2c && !ow)
            return "Нет данных со слейва";
        if ((i2c && i2c->pending) || (ow && ow->pending))
            return "Запрос данных со слейва...";
        if (i2c && !i2c->last_ok && i2c->last_error.length())
        {
            String msg = "I2C ошибка: ";
            msg += i2c->last_error;
            return msg;
        }
        if (ow && !ow->last_ok && ow->last_error.length())
        {
            String msg = "OW ошибка: ";
            msg += ow->last_error;
            return msg;
        }
        const bool i2c_ok = i2c && i2c->has_data;
        const bool ow_ok = ow && ow->has_data;
        if (!i2c_ok && !ow_ok)
            return "Нет данных со слейва";
        return "OK";
    }

    String stackPortsStatusText_(uint32_t node_id) const
    {
        const StackPortsCache *ports = findStackPortsCache_(node_id, false);
        const StackExtendersCache *exts = findStackExtendersCache_(node_id, false);
        if (!ports && !exts)
            return "Нет данных со слейва";
        if ((ports && ports->pending) || (exts && exts->pending))
            return "Запрос данных со слейва...";
        if (ports && !ports->last_ok && ports->last_error.length())
        {
            String msg = "Ports ошибка: ";
            msg += ports->last_error;
            return msg;
        }
        if (exts && !exts->last_ok && exts->last_error.length())
        {
            String msg = "Extenders ошибка: ";
            msg += exts->last_error;
            return msg;
        }
        const bool ports_ok = ports && ports->has_data;
        const bool exts_ok = exts && exts->has_data;
        if (!ports_ok && !exts_ok)
            return "Нет данных со слейва";
        return "OK";
    }

    String stackSecurityTitle_(uint32_t node_id) const
    {
        String title = "Датчики";
        if (!_stack_master || node_id == 0)
            return title;
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            if (_stack_master->nodeIdAt(i) == node_id)
            {
                String name = _stack_master->nodeNameAt(i);
                if (name.length() > 0)
                {
                    title += " (";
                    title += name;
                    title += ")";
                }
                return title;
            }
        }
        return title;
    }

    bool isStackSocketsView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    bool isStackLightsView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    bool isStackSecurityView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    bool isStackMeteoView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    bool isStackThermoView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    bool isStackSepticView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    bool isStackTanksView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    bool isStackRingView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    bool isStackBusesView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    bool isStackPortsView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    uint32_t parseStackNodeIdParam_(AsyncWebServerRequest *request) const
    {
        String node = paramValueAny_(request, "node_id");
        if (node.length() == 0)
            node = paramValueAny_(request, "node");
        if (node.length() == 0)
            return 0;
        char *end = nullptr;
        const unsigned long value = strtoul(node.c_str(), &end, 0);
        if (!end || end == node.c_str())
            return 0;
        return (uint32_t)value;
    }

    void handleStackSocketsToggle_(AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie)
    {
        if (!_stack_master)
        {
            sendText_(request, 400, "text/plain", "Stack master missing", set_cookie);
            return;
        }
        const String id_str = paramValueAny_(request, "id");
        if (!id_str.length())
        {
            sendText_(request, 400, "text/plain", "Missing id", set_cookie);
            return;
        }
        const uint8_t id = (uint8_t)id_str.toInt();
        if (id == 0)
        {
            sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        String action = paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        StackSocketsCache *cache = findStackSocketsCache_(node_id, false);
        StackSocketItem *item = cache ? findStackSocketItem_(*cache, id) : nullptr;

        if (action == "state")
        {
            if (!cache || !cache->has_data ||
                (uint32_t)(millis() - cache->updated_ms) > 1500u)
            {
                requestStackSockets_(node_id);
            }
            if (!item)
            {
                sendText_(request, 200, "text/plain", "unknown", set_cookie);
                return;
            }
            sendText_(request, 200, "text/plain", item->state ? "on" : "off", set_cookie);
            return;
        }

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        JsonArray items = params["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = id;

        bool desired_known = false;
        bool desired = false;
        if (action == "on" || action == "off")
        {
            desired = (action == "on");
            desired_known = true;
            o["state"] = desired;
        }
        else
        {
            o["toggle"] = true;
            if (item)
            {
                desired = !item->state;
                desired_known = true;
            }
        }

        char payload[160] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0 || !_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                               (const uint8_t *)payload, len))
        {
            sendText_(request, 400, "text/plain", "Send failed", set_cookie);
            return;
        }

        if (desired_known && item)
            item->state = desired;
        sendText_(request, 200, "text/plain", desired_known ? (desired ? "on" : "off") : "pending", set_cookie);
    }

    void handleStackLightsToggle_(AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie)
    {
        if (!_stack_master)
        {
            sendText_(request, 400, "text/plain", "Stack master missing", set_cookie);
            return;
        }
        const String id_str = paramValueAny_(request, "id");
        if (!id_str.length())
        {
            sendText_(request, 400, "text/plain", "Missing id", set_cookie);
            return;
        }
        const uint8_t id = (uint8_t)id_str.toInt();
        if (id == 0)
        {
            sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        String action = paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        StackLightsCache *cache = findStackLightsCache_(node_id, false);
        StackLightItem *item = cache ? findStackLightItem_(*cache, id) : nullptr;

        if (action == "state")
        {
            if (!cache || !cache->has_data ||
                (uint32_t)(millis() - cache->updated_ms) > 1500u)
            {
                requestStackLights_(node_id);
            }
            if (!item)
            {
                sendText_(request, 200, "text/plain", "unknown", set_cookie);
                return;
            }
            sendText_(request, 200, "text/plain", item->state ? "on" : "off", set_cookie);
            return;
        }

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "set_lights";
        JsonObject params = doc["params"].to<JsonObject>();
        JsonArray items = params["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = id;

        bool desired_known = false;
        bool desired = false;
        if (action == "on" || action == "off")
        {
            desired = (action == "on");
            desired_known = true;
            o["state"] = desired;
        }
        else
        {
            o["toggle"] = true;
            if (item)
            {
                desired = !item->state;
                desired_known = true;
            }
        }

        char payload[160] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0 || !_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                               (const uint8_t *)payload, len))
        {
            sendText_(request, 400, "text/plain", "Send failed", set_cookie);
            return;
        }

        if (desired_known && item)
            item->state = desired;
        sendText_(request, 200, "text/plain", desired_known ? (desired ? "on" : "off") : "pending", set_cookie);
    }

    bool sendStackRingCmd_(uint32_t node_id, bool set_state, bool state)
    {
        if (!_stack_master)
            return false;
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Ring;
        if (set_state)
        {
            doc["action"] = "set";
            JsonObject params = doc["params"].to<JsonObject>();
            params["state"] = state;
        }
        else
        {
            doc["action"] = "trigger";
        }
        const String key = stackApiKey_();
        if (key.length())
            doc["api_key"] = key;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        return _stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                     (const uint8_t *)payload, len);
    }

    bool sendStackRingCmdAll_(bool set_state, bool state)
    {
        if (!_stack_master || stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return false;
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Ring;
        if (set_state)
        {
            doc["action"] = "set";
            JsonObject params = doc["params"].to<JsonObject>();
            params["state"] = state;
        }
        else
        {
            doc["action"] = "trigger";
        }
        const String key = stackApiKey_();
        if (key.length())
            doc["api_key"] = key;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        bool ok = true;
        for (size_t i = 0; i < count; ++i)
        {
            if (!_stack_master->sendTo(_stack_master->nodeIdAt(i), (uint8_t)StackMsgType::CmdSet,
                                       (const uint8_t *)payload, len))
                ok = false;
        }
        return ok;
    }

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        if (!ctx)
            return;
        WebInterface *self = static_cast<WebInterface *>(ctx);
        self->handleStackFrame_(node_id, frame);
        if (self->_stack_cache)
            StackCache::onStackFrame_(self->_stack_cache, node_id, frame);
    }

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame)
    {
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return;
        DynamicJsonDocument doc(4096);
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        StackSocketsCache *sock_cache = findStackSocketsCacheByCmd_(cmd_id);
        StackLightsCache *light_cache = findStackLightsCacheByCmd_(cmd_id);
        StackPortsCache *ports_cache = findStackPortsCacheByCmd_(cmd_id);
        StackExtendersCache *ext_cache = findStackExtendersCacheByCmd_(cmd_id);
        StackSecurityCache *sec_cache = findStackSecurityCacheByCmd_(cmd_id);
        StackMeteoCache *meteo_cache = findStackMeteoCacheByCmd_(cmd_id);
        StackThermoCache *thermo_cache = findStackThermoCacheByCmd_(cmd_id);
        StackSepticCache *septic_cache = findStackSepticCacheByCmd_(cmd_id);
        StackTankCache *tanks_cache = findStackTanksCacheByCmd_(cmd_id);
        StackI2cCache *i2c_cache = findStackI2cCacheByCmd_(cmd_id);
        StackOwCache *ow_cache = findStackOwCacheByCmd_(cmd_id);
        bool status_is_plc = false;
        bool status_is_rtc = false;
        StackNodeStatusCache *status_cache = findStackNodeStatusCacheByCmd_(cmd_id, status_is_plc, status_is_rtc);
        if (!sock_cache && !light_cache && !ports_cache && !ext_cache && !sec_cache && !meteo_cache &&
            !thermo_cache && !septic_cache && !tanks_cache && !i2c_cache && !ow_cache && !status_cache)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (doc["ok"] | false);
        JsonArrayConst items = doc["data"]["items"].as<JsonArrayConst>();

        if (sock_cache)
        {
            sock_cache->pending = false;
            sock_cache->updated_ms = millis();
            sock_cache->last_ok = false;
            sock_cache->last_error = "";
            if (!ok)
            {
                sock_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                sock_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (sock_cache->item_count >= SocketController::kSocketCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackSocketItem &dst = sock_cache->items[sock_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.state = item["state"] | false;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                }
                sock_cache->has_data = true;
                sock_cache->last_ok = true;
                sock_cache->node_id = node_id;
            }
        }

        if (light_cache)
        {
            light_cache->pending = false;
            light_cache->updated_ms = millis();
            light_cache->last_ok = false;
            light_cache->last_error = "";
            if (!ok)
            {
                light_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                light_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (light_cache->item_count >= SocketController::kLightCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackLightItem &dst = light_cache->items[light_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.state = item["state"] | false;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                }
                light_cache->has_data = true;
                light_cache->last_ok = true;
                light_cache->node_id = node_id;
            }
        }

        if (ports_cache)
        {
            ports_cache->pending = false;
            ports_cache->updated_ms = millis();
            ports_cache->last_ok = false;
            ports_cache->last_error = "";
            if (!ok)
            {
                ports_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonArrayConst ports = doc["data"]["ports"].as<JsonArrayConst>();
                if (!ports.isNull())
                {
                    ports_cache->item_count = 0;
                    for (JsonObjectConst item : ports)
                    {
                        if (ports_cache->item_count >= PortIO::PORT_COUNT)
                            break;
                        if (!item["id"].is<unsigned>())
                            continue;
                        StackPortItem &dst = ports_cache->items[ports_cache->item_count++];
                        dst.id = (uint8_t)item["id"].as<unsigned>();
                        dst.ctrl = item["ctrl"] | false;
                        copyStr_(dst.backend, sizeof(dst.backend), item["backend"].as<const char *>());
                        copyStr_(dst.loc, sizeof(dst.loc), item["loc"].as<const char *>());
                        copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                        copyStr_(dst.hw, sizeof(dst.hw), item["hw"].as<const char *>());
                        dst.is_extender = (strcmp(dst.backend, "Extender") == 0);
                        if (item["dev"].is<int>() || item["dev"].is<unsigned>())
                            dst.dev = (int16_t)item["dev"].as<int>();
                        else
                            dst.dev = -1;
                        if (item["pin"].is<int>() || item["pin"].is<unsigned>())
                            dst.pin = (int16_t)item["pin"].as<int>();
                        else
                            dst.pin = -1;
                    }
                    ports_cache->has_data = true;
                    ports_cache->last_ok = true;
                    ports_cache->node_id = node_id;
                }
            }
        }

        if (ext_cache)
        {
            ext_cache->pending = false;
            ext_cache->updated_ms = millis();
            ext_cache->last_ok = false;
            ext_cache->last_error = "";
            if (!ok)
            {
                ext_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                ext_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (ext_cache->item_count >= Extender::MAX_DEVS)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackExtenderItem &dst = ext_cache->items[ext_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.bus = (uint8_t)(item["bus"] | 0u);
                    copyStr_(dst.addr, sizeof(dst.addr), item["addr"].as<const char *>());
                    copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                    dst.present = item["present"] | false;
                }
                ext_cache->has_data = true;
                ext_cache->last_ok = true;
                ext_cache->node_id = node_id;
            }
        }

        if (sec_cache)
        {
            sec_cache->pending = false;
            sec_cache->updated_ms = millis();
            sec_cache->last_ok = false;
            sec_cache->last_error = "";
            if (!ok)
            {
                sec_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                sec_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (sec_cache->item_count >= SecurityController::kSensorCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackSecuritySensorItem &dst = sec_cache->items[sec_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.detect = item["detect"] | false;
                    dst.silent = item["silent"] | false;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                    dst.port = (uint8_t)(item["port"] | SecurityController::kInvalidPort);
                }
                sec_cache->has_data = true;
                sec_cache->last_ok = true;
                sec_cache->node_id = node_id;
            }
        }

        if (meteo_cache)
        {
            meteo_cache->pending = false;
            meteo_cache->updated_ms = millis();
            meteo_cache->last_ok = false;
            meteo_cache->last_error = "";
            if (!ok)
            {
                meteo_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                meteo_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (meteo_cache->item_count >= MeteoController::kSensorCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackMeteoItem &dst = meteo_cache->items[meteo_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.ok = item["ok"] | false;
                    dst.has_temp = item["has_temp"] | false;
                    dst.has_hum = item["has_hum"] | false;
                    dst.temp_c = item["temp_c"] | 0.0f;
                    dst.hum = item["hum"] | 0.0f;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                    copyStr_(dst.addr, sizeof(dst.addr), item["addr"].as<const char *>());
                    if (item["pin"].is<int>() || item["pin"].is<unsigned>())
                        dst.pin = item["pin"].as<int>();
                    else
                        dst.pin = -1;
                }
                meteo_cache->has_data = true;
                meteo_cache->last_ok = true;
                meteo_cache->node_id = node_id;
            }
        }

        if (thermo_cache)
        {
            thermo_cache->pending = false;
            thermo_cache->updated_ms = millis();
            thermo_cache->last_ok = false;
            thermo_cache->last_error = "";
            if (!ok)
            {
                thermo_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                thermo_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (thermo_cache->item_count >= ThermoController::kDeviceCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackThermoItem &dst = thermo_cache->items[thermo_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.power_on = item["power_on"] | false;
                    dst.heat_on = item["heat_on"] | false;
                    dst.cool_on = item["cool_on"] | false;
                    dst.sensor = (uint8_t)(item["sensor"] | 0u);
                    dst.target = item["target"] | 0.0f;
                    dst.hyst = item["hyst"] | 0.0f;
                    dst.heat = (uint8_t)(item["heat"] | ThermoController::kInvalidPort);
                    dst.cool = (uint8_t)(item["cool"] | ThermoController::kInvalidPort);
                    dst.button = (uint8_t)(item["button"] | ThermoController::kInvalidPort);
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    copyStr_(dst.mode, sizeof(dst.mode), item["mode"].as<const char *>());
                }
                thermo_cache->has_data = true;
                thermo_cache->last_ok = true;
                thermo_cache->node_id = node_id;
            }
        }

        if (septic_cache)
        {
            septic_cache->pending = false;
            septic_cache->updated_ms = millis();
            septic_cache->last_ok = false;
            septic_cache->last_error = "";
            if (!ok)
            {
                septic_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                septic_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (septic_cache->item_count >= SepticController::kSepticCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackSepticItem &dst = septic_cache->items[septic_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.monitor = item["monitor"] | false;
                    dst.warning_port = (uint8_t)(item["warning_port"] | SepticController::kInvalidPort);
                    dst.alarm_port = (uint8_t)(item["alarm_port"] | SepticController::kInvalidPort);
                    dst.relay_warning = (uint8_t)(item["relay_warning"] | SepticController::kInvalidPort);
                    dst.relay_alarm = (uint8_t)(item["relay_alarm"] | SepticController::kInvalidPort);
                    dst.warning = item["warning"] | false;
                    dst.alarm = item["alarm"] | false;
                }
                septic_cache->has_data = true;
                septic_cache->last_ok = true;
                septic_cache->node_id = node_id;
            }
        }

        if (tanks_cache)
        {
            tanks_cache->pending = false;
            tanks_cache->updated_ms = millis();
            tanks_cache->last_ok = false;
            tanks_cache->last_error = "";
            if (!ok)
            {
                tanks_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                tanks_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (tanks_cache->item_count >= TankController::kTankCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackTankItem &dst = tanks_cache->items[tanks_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.power_on = item["power_on"] | false;
                    dst.low = (uint8_t)(item["low"] | TankController::kInvalidPort);
                    dst.mid = (uint8_t)(item["mid"] | TankController::kInvalidPort);
                    dst.full = (uint8_t)(item["full"] | TankController::kInvalidPort);
                    dst.valve = (uint8_t)(item["valve"] | TankController::kInvalidPort);
                    dst.pump = (uint8_t)(item["pump"] | TankController::kInvalidPort);
                    dst.alarm = (uint8_t)(item["alarm"] | TankController::kInvalidPort);
                    dst.level_low = item["level_low"] | false;
                    dst.level_mid = item["level_mid"] | false;
                    dst.level_full = item["level_full"] | false;
                    dst.levels_ok = item["levels_ok"] | false;
                    dst.valve_on = item["valve_on"] | false;
                    dst.pump_on = item["pump_on"] | false;
                    dst.alarm_on = item["alarm_on"] | false;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                }
                tanks_cache->has_data = true;
                tanks_cache->last_ok = true;
                tanks_cache->node_id = node_id;
            }
        }

        if (i2c_cache)
        {
            i2c_cache->pending = false;
            i2c_cache->updated_ms = millis();
            i2c_cache->last_ok = false;
            i2c_cache->last_error = "";
            if (!ok)
            {
                i2c_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                i2c_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (i2c_cache->item_count >= 127)
                        break;
                    if (!item["bus"].is<unsigned>())
                        continue;
                    const char *addr = item["addr"] | "";
                    uint8_t addr_val = 0;
                    if (addr && addr[0])
                        addr_val = (uint8_t)strtoul(addr, nullptr, 0);
                    StackI2cItem &dst = i2c_cache->items[i2c_cache->item_count++];
                    dst.bus = (uint8_t)item["bus"].as<unsigned>();
                    dst.addr = addr_val;
                }
                i2c_cache->has_data = true;
                i2c_cache->last_ok = true;
                i2c_cache->node_id = node_id;
            }
        }

        if (ow_cache)
        {
            ow_cache->pending = false;
            ow_cache->updated_ms = millis();
            ow_cache->last_ok = false;
            ow_cache->last_error = "";
            if (!ok)
            {
                ow_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                ow_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (ow_cache->item_count >= 64)
                        break;
                    if (!item["bus"].is<unsigned>())
                        continue;
                    StackOwItem &dst = ow_cache->items[ow_cache->item_count++];
                    dst.bus = (uint8_t)item["bus"].as<unsigned>();
                    const char *addr = item["addr"] | "";
                    const char *type = item["type"] | "";
                    strncpy(dst.addr, addr ? addr : "", sizeof(dst.addr) - 1);
                    strncpy(dst.type, type ? type : "", sizeof(dst.type) - 1);
                    dst.addr[sizeof(dst.addr) - 1] = '\0';
                    dst.type[sizeof(dst.type) - 1] = '\0';
                }
                ow_cache->has_data = true;
                ow_cache->last_ok = true;
                ow_cache->node_id = node_id;
            }
        }

        if (status_cache)
        {
            JsonObjectConst data = doc["data"].as<JsonObjectConst>();
            if (status_is_plc)
            {
                status_cache->pending_plc = false;
                status_cache->plc_updated_ms = millis();
                status_cache->last_plc_ok = false;
                status_cache->last_plc_error = "";
                if (!ok)
                {
                    status_cache->last_plc_error = doc["error"] | "error";
                }
                else if (!data.isNull())
                {
                    status_cache->board_temp = data["board_temp"] | status_cache->board_temp;
                    status_cache->cpu_temp = data["cpu_temp"] | status_cache->cpu_temp;
                    status_cache->fan_on = data["fan_on"] | false;
                    status_cache->fan_on_c = data["on_c"] | status_cache->fan_on_c;
                    status_cache->fan_hyst_c = data["hyst_c"] | status_cache->fan_hyst_c;
                    status_cache->has_plc = true;
                    status_cache->last_plc_ok = true;
                    status_cache->node_id = node_id;
                }
            }
            if (status_is_rtc)
            {
                status_cache->pending_rtc = false;
                status_cache->rtc_updated_ms = millis();
                status_cache->last_rtc_ok = false;
                status_cache->last_rtc_error = "";
                if (!ok)
                {
                    status_cache->last_rtc_error = doc["error"] | "error";
                }
                else if (!data.isNull())
                {
                    status_cache->rtc_date = data["date"] | status_cache->rtc_date;
                    status_cache->rtc_time = data["time"] | status_cache->rtc_time;
                    status_cache->rtc_weekday = (uint8_t)(data["weekday"] | status_cache->rtc_weekday);
                    status_cache->rtc_temp = data["temp_c"] | status_cache->rtc_temp;
                    status_cache->has_rtc = true;
                    status_cache->last_rtc_ok = true;
                    status_cache->node_id = node_id;
                }
            }
        }
    }

    bool requestStackSockets_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSocketsCache *cache = findStackSocketsCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackLights_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackLightsCache *cache = findStackLightsCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "get_lights";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackSecurity_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSecurityCache *cache = findStackSecurityCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackMeteo_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackMeteoCache *cache = findStackMeteoCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Meteo;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackThermo_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackThermoCache *cache = findStackThermoCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Thermo;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackSeptic_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSepticCache *cache = findStackSepticCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Septic;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackTanks_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackTankCache *cache = findStackTanksCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Tanks;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackPorts_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackPortsCache *cache = findStackPortsCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Ports;
        doc["action"] = "get_state";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackExtenders_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackExtendersCache *cache = findStackExtendersCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Extenders;
        doc["action"] = "get_list";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackI2c_(uint32_t node_id, bool run)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackI2cCache *cache = findStackI2cCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (!run && cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<128> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::I2cScan;
        doc["action"] = run ? "run" : "get_last";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackOw_(uint32_t node_id, bool run)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackOwCache *cache = findStackOwCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (!run && cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<128> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::OwScan;
        doc["action"] = run ? "run" : "get_last";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackPlcStatus_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackNodeStatusCache *cache = findStackNodeStatusCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending_plc)
            return false;
        if (cache->has_plc && (uint32_t)(now - cache->plc_updated_ms) < 3000u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<128> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::PlcStatus;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending_plc = true;
        cache->pending_plc_cmd_id = cmd_id;
        return true;
    }

    bool requestStackRtcStatus_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackNodeStatusCache *cache = findStackNodeStatusCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending_rtc)
            return false;
        if (cache->has_rtc && (uint32_t)(now - cache->rtc_updated_ms) < 3000u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<128> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Rtc;
        doc["action"] = "get_time";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending_rtc = true;
        cache->pending_rtc_cmd_id = cmd_id;
        return true;
    }

    StackSocketsCache *findStackSocketsCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_sockets_cache.node_id == node_id)
            return &_stack_sockets_cache;
        if (!create)
            return nullptr;
        _stack_sockets_cache = StackSocketsCache{};
        _stack_sockets_cache.node_id = node_id;
        return &_stack_sockets_cache;
    }

    const StackSocketsCache *findStackSocketsCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackSocketsCache_(node_id, create);
    }

    StackSocketsCache *findStackSocketsCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_sockets_cache.pending && _stack_sockets_cache.pending_cmd_id == cmd_id)
            return &_stack_sockets_cache;
        return nullptr;
    }

    StackSocketItem *findStackSocketItem_(StackSocketsCache &cache, uint8_t id)
    {
        for (size_t i = 0; i < cache.item_count; ++i)
            if (cache.items[i].id == id)
                return &cache.items[i];
        return nullptr;
    }

    StackLightsCache *findStackLightsCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_lights_cache.node_id == node_id)
            return &_stack_lights_cache;
        if (!create)
            return nullptr;
        _stack_lights_cache = StackLightsCache{};
        _stack_lights_cache.node_id = node_id;
        return &_stack_lights_cache;
    }

    const StackLightsCache *findStackLightsCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackLightsCache_(node_id, create);
    }

    StackLightsCache *findStackLightsCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_lights_cache.pending && _stack_lights_cache.pending_cmd_id == cmd_id)
            return &_stack_lights_cache;
        return nullptr;
    }

    StackPortsCache *findStackPortsCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_ports_cache.node_id == node_id)
            return &_stack_ports_cache;
        if (!create)
            return nullptr;
        _stack_ports_cache = StackPortsCache{};
        _stack_ports_cache.node_id = node_id;
        return &_stack_ports_cache;
    }

    const StackPortsCache *findStackPortsCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackPortsCache_(node_id, create);
    }

    StackPortsCache *findStackPortsCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_ports_cache.pending && _stack_ports_cache.pending_cmd_id == cmd_id)
            return &_stack_ports_cache;
        return nullptr;
    }

    StackExtendersCache *findStackExtendersCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_ext_cache.node_id == node_id)
            return &_stack_ext_cache;
        if (!create)
            return nullptr;
        _stack_ext_cache = StackExtendersCache{};
        _stack_ext_cache.node_id = node_id;
        return &_stack_ext_cache;
    }

    const StackExtendersCache *findStackExtendersCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackExtendersCache_(node_id, create);
    }

    StackExtendersCache *findStackExtendersCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_ext_cache.pending && _stack_ext_cache.pending_cmd_id == cmd_id)
            return &_stack_ext_cache;
        return nullptr;
    }

    StackI2cCache *findStackI2cCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_i2c_cache.node_id == node_id)
            return &_stack_i2c_cache;
        if (!create)
            return nullptr;
        _stack_i2c_cache = StackI2cCache{};
        _stack_i2c_cache.node_id = node_id;
        return &_stack_i2c_cache;
    }

    const StackI2cCache *findStackI2cCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackI2cCache_(node_id, create);
    }

    StackI2cCache *findStackI2cCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_i2c_cache.pending && _stack_i2c_cache.pending_cmd_id == cmd_id)
            return &_stack_i2c_cache;
        return nullptr;
    }

    StackOwCache *findStackOwCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_ow_cache.node_id == node_id)
            return &_stack_ow_cache;
        if (!create)
            return nullptr;
        _stack_ow_cache = StackOwCache{};
        _stack_ow_cache.node_id = node_id;
        return &_stack_ow_cache;
    }

    const StackOwCache *findStackOwCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackOwCache_(node_id, create);
    }

    StackOwCache *findStackOwCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_ow_cache.pending && _stack_ow_cache.pending_cmd_id == cmd_id)
            return &_stack_ow_cache;
        return nullptr;
    }

    StackLightItem *findStackLightItem_(StackLightsCache &cache, uint8_t id)
    {
        for (size_t i = 0; i < cache.item_count; ++i)
            if (cache.items[i].id == id)
                return &cache.items[i];
        return nullptr;
    }

    StackSecurityCache *findStackSecurityCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_security_cache.node_id == node_id)
            return &_stack_security_cache;
        if (!create)
            return nullptr;
        _stack_security_cache = StackSecurityCache{};
        _stack_security_cache.node_id = node_id;
        return &_stack_security_cache;
    }

    const StackSecurityCache *findStackSecurityCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackSecurityCache_(node_id, create);
    }

    StackSecurityCache *findStackSecurityCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_security_cache.pending && _stack_security_cache.pending_cmd_id == cmd_id)
            return &_stack_security_cache;
        return nullptr;
    }

    StackMeteoCache *findStackMeteoCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_meteo_cache.node_id == node_id)
            return &_stack_meteo_cache;
        if (!create)
            return nullptr;
        _stack_meteo_cache = StackMeteoCache{};
        _stack_meteo_cache.node_id = node_id;
        return &_stack_meteo_cache;
    }

    const StackMeteoCache *findStackMeteoCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackMeteoCache_(node_id, create);
    }

    StackMeteoCache *findStackMeteoCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_meteo_cache.pending && _stack_meteo_cache.pending_cmd_id == cmd_id)
            return &_stack_meteo_cache;
        return nullptr;
    }

    StackThermoCache *findStackThermoCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_thermo_cache.node_id == node_id)
            return &_stack_thermo_cache;
        if (!create)
            return nullptr;
        _stack_thermo_cache = StackThermoCache{};
        _stack_thermo_cache.node_id = node_id;
        return &_stack_thermo_cache;
    }

    const StackThermoCache *findStackThermoCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackThermoCache_(node_id, create);
    }

    StackThermoCache *findStackThermoCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_thermo_cache.pending && _stack_thermo_cache.pending_cmd_id == cmd_id)
            return &_stack_thermo_cache;
        return nullptr;
    }

    StackSepticCache *findStackSepticCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_septic_cache.node_id == node_id)
            return &_stack_septic_cache;
        if (!create)
            return nullptr;
        _stack_septic_cache = StackSepticCache{};
        _stack_septic_cache.node_id = node_id;
        return &_stack_septic_cache;
    }

    const StackSepticCache *findStackSepticCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackSepticCache_(node_id, create);
    }

    StackSepticCache *findStackSepticCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_septic_cache.pending && _stack_septic_cache.pending_cmd_id == cmd_id)
            return &_stack_septic_cache;
        return nullptr;
    }

    StackTankCache *findStackTanksCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_tanks_cache.node_id == node_id)
            return &_stack_tanks_cache;
        if (!create)
            return nullptr;
        _stack_tanks_cache = StackTankCache{};
        _stack_tanks_cache.node_id = node_id;
        return &_stack_tanks_cache;
    }

    const StackTankCache *findStackTanksCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackTanksCache_(node_id, create);
    }

    StackTankCache *findStackTanksCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_tanks_cache.pending && _stack_tanks_cache.pending_cmd_id == cmd_id)
            return &_stack_tanks_cache;
        return nullptr;
    }

    StackNodeStatusCache *findStackNodeStatusCache_(uint32_t node_id, bool create)
    {
        for (auto &c : _stack_status_cache)
        {
            if (c.node_id == node_id)
                return &c;
        }
        if (!create)
            return nullptr;
        for (auto &c : _stack_status_cache)
        {
            if (c.node_id == 0)
            {
                c = StackNodeStatusCache{};
                c.node_id = node_id;
                return &c;
            }
        }
        return nullptr;
    }

    const StackNodeStatusCache *findStackNodeStatusCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackNodeStatusCache_(node_id, create);
    }

    StackNodeStatusCache *findStackNodeStatusCacheByCmd_(uint16_t cmd_id, bool &is_plc, bool &is_rtc)
    {
        is_plc = false;
        is_rtc = false;
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_status_cache)
        {
            if (c.pending_plc && c.pending_plc_cmd_id == cmd_id)
            {
                is_plc = true;
                return &c;
            }
            if (c.pending_rtc && c.pending_rtc_cmd_id == cmd_id)
            {
                is_rtc = true;
                return &c;
            }
        }
        return nullptr;
    }

    uint16_t nextStackCmdId_()
    {
        ++_stack_cmd_id;
        if (_stack_cmd_id == 0)
            _stack_cmd_id = 1;
        return _stack_cmd_id;
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
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
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

    String listStackLightsHtml_(uint32_t node_id)
    {
        StackLightsCache *cache = findStackLightsCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Свет отсутствует</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 420u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackLightItem &cfg = cache->items[i];
            const bool on = cfg.state;
            items += "<div class=\"tile\">";
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
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Свет";
            items += "</strong></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += on ? "status-on" : "status-off";
            items += "\"></span>";
            items += "<span class=\"status-text\">";
            items += on ? "Включена" : "Выключена";
            items += "</span></div>";
            items += "<div class=\"form-row\"><label>Перекл.</label>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-toggle\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\"";
            if (on)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div></div>";
        }
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

    String listSecurityRfidKeysHtml_()
    {
        if (!_controllers)
            return "<tr><td colspan=\"4\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></td></tr>";
        String items;
        items.reserve(1400);
        SecurityController &sec = _controllers->security();
        String last_serial;
        const bool has_last = sec.lastRfidSerial(last_serial);
        auto appendRow = [&](size_t idx, bool enabled, const char *serial, const String &name) {
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)(idx + 1));
            items += "</strong></td><td><input type=\"checkbox\" name=\"rk";
            items += String((unsigned)(idx + 1));
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"rk";
            items += String((unsigned)(idx + 1));
            items += "_name\" value=\"";
            if (name.length())
                appendHtmlEscaped_(items, name.c_str());
            items += "\"></td><td><input class=\"field serial\" type=\"text\" name=\"rk";
            items += String((unsigned)(idx + 1));
            items += "_serial\" list=\"rk";
            items += String((unsigned)(idx + 1));
            items += "_serial_list\" value=\"";
            if (enabled && serial)
                appendHtmlEscaped_(items, serial);
            items += "\">";
            if (has_last)
            {
                items += "<datalist id=\"rk";
                items += String((unsigned)(idx + 1));
                items += "_serial_list\"><option value=\"";
                appendHtmlEscaped_(items, last_serial.c_str());
                items += "\"></option></datalist>";
            }
            items += "</td></tr>";
        };

        int first_disabled = -1;
        for (size_t i = 0; i < SecurityController::kRfidKeyCount; ++i)
        {
            bool enabled = false;
            uint8_t bytes[10] = {};
            uint8_t len = 0;
            sec.rfidKeySlot(i, bytes, len, enabled);
            if (enabled)
            {
                const String serial = SecurityController::rfidSerialToString(bytes, len);
                appendRow(i, true, serial.c_str(), sec.rfidKeyNameByIndex(i));
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

    String listClientsTilesHtml_(uint8_t page_idx, uint8_t &out_pages)
    {
        const uint8_t page_size = 6;
        const uint8_t total = 2;
        out_pages = (uint8_t)((total + page_size - 1) / page_size);
        if (out_pages == 0)
            out_pages = 1;
        if (page_idx >= out_pages)
            page_idx = (uint8_t)(out_pages - 1);

        String items;
        items.reserve(512);
        const uint8_t start = (uint8_t)(page_idx * page_size);
        const uint8_t end = (uint8_t)(start + page_size);
        if (start >= total)
        {
            items = "<div class=\"tile\"><strong>Клиенты отсутствуют</strong></div>";
            return items;
        }

        if (start <= 0 && 0 < end)
        {
            const bool enabled = _configs_manager ? _configs_manager->rfidEnabled() : false;
            const bool active = enabled && (stackRole_() == ConfigsManagerIface::StackRole::Slave);
            items += "<div class=\"tile\"><form method=\"POST\" action=\"/clients\" id=\"rfid-form\">";
            items += "<input type=\"hidden\" name=\"client\" value=\"rfid\">";
            items += "<div class=\"tile-head\"><a href=\"/rfid\">RFID</a>";
            items += "<label class=\"switch\"><input type=\"checkbox\" id=\"rfid-enabled\" name=\"rfid_enabled\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div></form>";
            items += "<span>Тонкий клиент: RFID reader</span>";
            items += "<span class=\"status\">RFID: <strong>";
            items += enabled ? "включен" : "выключен";
            items += "</strong></span>";
            items += "<span class=\"status\">Состояние: <strong>";
            items += active ? "active" : "inactive";
            items += "</strong></span></div>";
        }
        if (start <= 1 && 1 < end)
        {
            const bool enabled = _configs_manager ? _configs_manager->ringClientEnabled() : false;
            const bool active = enabled && (stackRole_() == ConfigsManagerIface::StackRole::Slave);
            items += "<div class=\"tile\"><form method=\"POST\" action=\"/clients\" id=\"ring-client-form\">";
            items += "<input type=\"hidden\" name=\"client\" value=\"ring\">";
            items += "<div class=\"tile-head\"><a href=\"/client/ring\">Ring</a>";
            items += "<label class=\"switch\"><input type=\"checkbox\" id=\"ring-client-enabled\" name=\"ring_client_enabled\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div></form>";
            items += "<span>Тонкий клиент: Ring button</span>";
            items += "<span class=\"status\">Ring: <strong>";
            items += enabled ? "включен" : "выключен";
            items += "</strong></span>";
            items += "<span class=\"status\">Состояние: <strong>";
            items += active ? "active" : "inactive";
            items += "</strong></span></div>";
        }
        return items;
    }

    String displaySlotsHtml_() const
    {
        String html;
        html.reserve(2048);
        const size_t count = _configs_manager ? _configs_manager->displaySlotCount() : 8;
        const size_t total = (count > 0) ? count : 8;
        for (size_t i = 0; i < total && i < 8; ++i)
        {
            DisplaySlotConfig slot{};
            if (_configs_manager)
                _configs_manager->displaySlot(i, slot);
            const uint8_t row = (uint8_t)(i / 4);
            const uint8_t col = (uint8_t)(i % 4);
            const String idx = String((unsigned)i);
            html += "<div class=\"slot display-slot\" data-kind=\"";
            html += displaySlotKindName_(slot.kind);
            html += "\" data-index=\"";
            html += String((unsigned)slot.index);
            html += "\" data-field=\"";
            html += displaySlotFieldName_(slot.field);
            html += "\" data-node=\"";
            html += String((unsigned long)slot.node_id);
            html += "\">";
            html += "<div class=\"slot-head\">L";
            html += String((unsigned)(row + 1));
            html += "-";
            html += String((unsigned)(col + 1));
            html += "</div>";
            html += "<label>Источник</label><select class=\"field slot-kind\" name=\"ds";
            html += idx;
            html += "_kind\"></select>";
            html += "<label>Устройство</label><select class=\"field slot-node\" name=\"ds";
            html += idx;
            html += "_node\"></select>";
            html += "<label>Объект</label><select class=\"field slot-index\" name=\"ds";
            html += idx;
            html += "_index\"></select>";
            html += "<label>Параметр</label><select class=\"field slot-field\" name=\"ds";
            html += idx;
            html += "_field\"></select>";
            html += "<label>Текст</label><input class=\"field slot-text\" type=\"text\" name=\"ds";
            html += idx;
            html += "_text\" maxlength=\"4\" value=\"";
            if (slot.text[0])
                appendHtmlEscaped_(html, slot.text);
            html += "\"></div>";
        }
        return html;
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
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
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

    String listStackSecuritySensorsTiles_(uint32_t node_id)
    {
        StackSecurityCache *cache = findStackSecurityCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 420u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackSecuritySensorItem &cfg = cache->items[i];
            items += "<div class=\"tile\">";
            items += "<div class=\"sock-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sock-icon ";
            if (cfg.detect)
                items += "alert";
            else
                items += "off";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            if (strcmp(cfg.type, "reed") == 0)
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
            items += "</svg>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Датчик";
            items += "</strong></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += cfg.detect ? "status-bad" : "status-off";
            items += "\"></span>";
            items += "<span class=\"status-text\">";
            items += cfg.detect ? "Сработал" : "ОК";
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Тип</label><input class=\"field mini\" type=\"text\" value=\"";
            items += cfg.type;
            items += "\" readonly></div>";
            items += "<div class=\"form-row\"><label>Порт</label><input class=\"field mini\" type=\"text\" value=\"";
            if (cfg.port != SecurityController::kInvalidPort)
                items += String((unsigned)cfg.port);
            else
                items += "--";
            items += "\" readonly></div>";
            items += "<div class=\"form-row\"><label>Тихий</label><input class=\"field mini\" type=\"text\" value=\"";
            items += cfg.silent ? "yes" : "no";
            items += "\" readonly></div>";
            items += "</div>";
            items += "</div></div>";
        }
        return items;
    }

    String listStackMeteoHtml_(uint32_t node_id)
    {
        StackMeteoCache *cache = findStackMeteoCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 520u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackMeteoItem &cfg = cache->items[i];
            char temp_buf[12] = {};
            char hum_buf[12] = {};
            const char *temp = "--";
            const char *hum = "--";
            if (cfg.has_temp)
            {
                dtostrf(cfg.temp_c, 0, 1, temp_buf);
                temp = temp_buf;
            }
            if (cfg.has_hum)
            {
                dtostrf(cfg.hum, 0, 1, hum_buf);
                hum = hum_buf;
            }
            const bool has_data = cfg.has_temp || cfg.has_hum;
            const bool ok_on = has_data && cfg.ok;
            const char *status_class = "status-na";
            if (has_data)
                status_class = cfg.ok ? "status-ok" : "status-err";

            String type_label = cfg.type;
            type_label.toLowerCase();
            if (type_label == "dht22")
                type_label = "DHT22";
            else if (type_label == "ds18b20")
                type_label = "DS18B20";
            else if (type_label.length() == 0)
                type_label = "none";

            items += "<div class=\"tile";
            if (!cfg.enabled)
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
            if (cfg.has_hum)
            {
                items += "<div class=\"sensor-hum\"><svg class=\"sensor-hum-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<path fill=\"currentColor\" d=\"M32 6c7 12 16 22 16 34 0 8.8-7.2 16-16 16S16 48.8 16 40c0-12 9-22 16-34z\"/>";
                items += "</svg><div class=\"sensor-value\">";
                items += hum;
                items += "</div><div class=\"sensor-unit\">%</div></div>";
            }
            items += "</div></div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Датчик";
            items += "</strong></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += status_class;
            items += "\"></span><span class=\"status-text\">";
            if (!cfg.enabled)
                items += "Отключен";
            else if (!has_data)
                items += "Нет данных";
            else
                items += cfg.ok ? "ОК" : "Ошибка";
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Тип</label><div class=\"field mini\">";
            appendHtmlEscaped_(items, type_label.c_str());
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Пин</label><div class=\"field mini\">";
            if (cfg.pin >= 0)
                items += String(cfg.pin);
            else
                items += "--";
            items += "</div></div>";
            items += "<div class=\"form-row full\"><label>Адрес</label><div class=\"field\">";
            if (cfg.addr[0])
                appendHtmlEscaped_(items, cfg.addr);
            else
                items += "--";
            items += "</div></div>";
            items += "</div>";
            items += "</div></div>";
        }
        return items;
    }

    String listStackThermoHtml_(uint32_t node_id)
    {
        StackThermoCache *cache = findStackThermoCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Термо отсутствует</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 620u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
        const StackMeteoCache *meteo_cache = findStackMeteoCache_(node_id, false);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackThermoItem &cfg = cache->items[i];
            const bool mode_off = strcmp(cfg.mode, "off") == 0;
            const bool mode_heat = strcmp(cfg.mode, "heat") == 0;
            const bool mode_cool = strcmp(cfg.mode, "cool") == 0;
            const char *mode_label = "авто";
            if (mode_off)
                mode_label = "выкл";
            else if (mode_heat)
                mode_label = "нагрев";
            else if (mode_cool)
                mode_label = "охлаждение";

            const char *state_label = "ожидание";
            const char *state_class = "status-idle";
            if (cfg.heat_on)
            {
                state_label = "нагрев";
                state_class = "status-heat";
            }
            else if (cfg.cool_on)
            {
                state_label = "охлаждение";
                state_class = "status-cool";
            }

            bool show_heat = true;
            bool show_cool = true;
            String heat_class = "icon heat ";
            String cool_class = "icon cool ";
            if (mode_off)
            {
                show_heat = false;
                show_cool = false;
            }
            else if (mode_heat)
            {
                show_cool = false;
                heat_class += cfg.heat_on ? "active" : "inactive";
            }
            else if (mode_cool)
            {
                show_heat = false;
                cool_class += cfg.cool_on ? "active" : "inactive";
            }
            else
            {
                heat_class += cfg.heat_on ? "active" : "inactive";
                cool_class += cfg.cool_on ? "active" : "inactive";
            }

            const char *sensor_label = "нет";
            const char *sensor_suffix = "";
            char sensor_buf[16] = {};
            if (cfg.sensor != 0)
            {
                bool found = false;
                if (meteo_cache && meteo_cache->has_data)
                {
                    for (size_t s = 0; s < meteo_cache->item_count; ++s)
                    {
                        const StackMeteoItem &ms = meteo_cache->items[s];
                        if (ms.id == cfg.sensor)
                        {
                            found = true;
                            if (ms.has_temp)
                            {
                                dtostrf(ms.temp_c, 0, 1, sensor_buf);
                                sensor_label = sensor_buf;
                                sensor_suffix = "°C";
                            }
                            else
                            {
                                sensor_label = "--";
                            }
                            break;
                        }
                    }
                }
                if (!found && cfg.sensor != 0)
                    sensor_label = "--";
            }

            items += "<div class=\"tile";
            if (!cfg.enabled)
                items += " disabled";
            items += "\"><div class=\"thermo-left\"><div class=\"thermo-visual\"><div class=\"temp-pill sensor\">Текущая: <span class=\"temp-value\">";
            items += sensor_label;
            items += sensor_suffix;
            items += "</span></div><div class=\"temp-pill target\">Цель: <span class=\"temp-value\">";
            items += String(cfg.target, 1);
            items += "&deg;C</span></div>";
            if (show_heat)
            {
                items += "<svg class=\"";
                items += heat_class;
                items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"22\" y=\"30\" width=\"76\" height=\"60\" rx=\"10\"/><line x1=\"36\" y1=\"40\" x2=\"36\" y2=\"80\"/><line x1=\"52\" y1=\"40\" x2=\"52\" y2=\"80\"/><line x1=\"68\" y1=\"40\" x2=\"68\" y2=\"80\"/><line x1=\"84\" y1=\"40\" x2=\"84\" y2=\"80\"/></svg>";
            }
            if (show_cool)
            {
                items += "<svg class=\"";
                items += cool_class;
                items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"18\" y=\"28\" width=\"84\" height=\"46\" rx=\"10\"/><line x1=\"28\" y1=\"44\" x2=\"92\" y2=\"44\"/><line x1=\"28\" y1=\"56\" x2=\"92\" y2=\"56\"/><line x1=\"40\" y1=\"78\" x2=\"34\" y2=\"92\"/><line x1=\"60\" y1=\"78\" x2=\"60\" y2=\"94\"/><line x1=\"80\" y1=\"78\" x2=\"86\" y2=\"92\"/></svg>";
            }
            items += "</div>";
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
            items += "</span></span></div></div>";
            items += "<div><div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Термо";
            items += "</strong><span class=\"badge\">ID ";
            items += String((unsigned)cfg.id);
            items += "</span></div>";
            items += "<div class=\"status-line\"><span class=\"badge\">Питание: ";
            items += cfg.power_on ? "on" : "off";
            items += "</span><span class=\"badge\">Режим: ";
            items += mode_label;
            items += "</span></div>";
            items += "<div class=\"status-line\"><span class=\"badge\">Датчик: ";
            if (cfg.sensor != 0)
                items += String((unsigned)cfg.sensor);
            else
                items += "--";
            items += "</span></div>";
            items += "</div></div>";
        }
        return items;
    }

    String listStackSepticHtml_(uint32_t node_id)
    {
        StackSepticCache *cache = findStackSepticCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Септик отсутствует</strong></div>";
        String items;
        size_t reserve = 1024u + cache->item_count * 480u;
        if (reserve < 4096u)
            reserve = 4096u;
        items.reserve(reserve);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackSepticItem &cfg = cache->items[i];
            const bool warn = cfg.warning;
            const bool alarm = cfg.alarm;
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
            if (!cfg.enabled)
                items += " disabled";
            items += "\"><div class=\"septic-visual\"><div class=\"liquid ";
            items += water_class;
            items += "\" style=\"height:";
            items += water_level;
            items += ";\"></div><div class=\"level-label\">";
            items += water_label;
            items += "</div></div><div><div class=\"tile-head\"><div><strong>Септик #";
            items += String((unsigned)cfg.id);
            items += "</strong>";
            if (!cfg.enabled)
                items += " <span class=\"badge\">выкл</span>";
            items += "</div></div>";
            items += "<div class=\"status-grid\">";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += warn ? "status-on" : "status-off";
            items += "\"></span><span>Датчик предупреждения</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += alarm ? "status-on" : "status-off";
            items += "\"></span><span>Датчик тревоги</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot status-off\"></span><span>Реле предупреждения: н/д</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot status-off\"></span><span>Реле тревоги: н/д</span></div>";
            items += "</div></div></div>";
        }
        return items;
    }

    String listStackTanksHtml_(uint32_t node_id)
    {
        StackTankCache *cache = findStackTanksCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Баки отсутствуют</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 520u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackTankItem &cfg = cache->items[i];
            const char *level = "пусто";
            const char *level_class = "level-empty";
            unsigned level_pct = 10;
            if (cfg.level_full)
            {
                level = "полный";
                level_class = "level-full";
                level_pct = 90;
            }
            else if (cfg.level_mid)
            {
                level = "средний";
                level_class = "level-mid";
                level_pct = 60;
            }
            else if (cfg.level_low)
            {
                level = "низкий";
                level_class = "level-low";
                level_pct = 30;
            }

            items += "<div class=\"tile";
            if (!cfg.enabled)
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
            items += cfg.power_on ? "питание on" : "питание off";
            items += "</span>";
            items += "</div></div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Бак";
            items += "</strong></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += cfg.valve_on ? "status-on" : "status-off";
            items += "\"></span><span>Клапан</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += cfg.pump_on ? "status-on" : "status-off";
            items += "\"></span><span>Насос</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += cfg.alarm_on ? "status-on" : "status-off";
            items += "\"></span><span>Авария</span></div>";
            items += "</div></div>";
        }
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
                dtostrf(st.temp_c, 0, 1, temp_buf);
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

            const bool has_remote = (cfg.source_node_id != 0 && cfg.source_sensor_id != 0);
            MeteoController::SensorType ui_type = cfg.type;
            if (has_remote)
            {
                MeteoController::SensorType remote_type = MeteoController::SensorType::None;
                if (meteoRemoteType_(cfg.source_node_id, cfg.source_sensor_id, remote_type) &&
                    remote_type != MeteoController::SensorType::None)
                    ui_type = remote_type;
            }
            const bool show_hum = (ui_type == MeteoController::SensorType::Dht22);
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
            String remote_label;
            if (has_remote)
                remote_label = meteoRemoteLabel_(cfg.source_node_id, cfg.source_sensor_id);
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Датчик";
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"meteo-enable\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<div class=\"form-row full\" style=\"margin-bottom:8px;\"><label>Устройство</label><select class=\"field meteo-device\">";
            items += meteoRemoteNodeOptionsHtml_(cfg.source_node_id);
            items += "</select></div>";
            items += "<div class=\"form-row name-local\"><label>Имя</label><input class=\"field name meteo-name\" type=\"text\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"></div>";
            items += "<div class=\"form-row name-remote\" style=\"display:none;\"><label>Имя</label><select class=\"field name meteo-source\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_src\">";
            items += meteoRemoteSensorOptionsHtml_(cfg.source_sensor_id, cfg.source_node_id);
            items += "</select></div>";
            if (remote_label.length())
            {
                items += "<div class=\"muted\">";
                appendHtmlEscaped_(items, remote_label.c_str());
                items += "</div>";
            }
            items += "<div class=\"status-line\">";
            items += ok;
            items += "<span>Давность: ";
            items += age;
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Тип</label><select class=\"field meteo-type\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_type\">";
            appendTypeOption("none", "none", ui_type == MeteoController::SensorType::None);
            appendTypeOption("ds18b20", "ds18b20", ui_type == MeteoController::SensorType::Ds18b20);
            appendTypeOption("dht22", "dht22", ui_type == MeteoController::SensorType::Dht22);
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
        uint32_t remote_used[ThermoController::kDeviceCount] = {};
        size_t remote_used_count = 0;

        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            if (cfg->sensor_id == 0 || cfg->sensor_id > MeteoController::kSensorCount)
                continue;
            if (cfg->sensor_node_id == 0)
            {
                sensor_used[cfg->sensor_id]++;
            }
            else if (remote_used_count < ThermoController::kDeviceCount)
            {
                remote_used[remote_used_count++] = (cfg->sensor_node_id << 8) | cfg->sensor_id;
            }
        }

        auto appendTile = [&](const ThermoController::DeviceConfig &cfg, const ThermoController::DeviceState &st,
                              bool enabled) {
            const MeteoController::SensorState *sensor_st = nullptr;
            bool remote_has_temp = false;
            float remote_temp_c = 0.0f;
            if (cfg.sensor_id != ThermoController::kInvalidSensor)
            {
                if (cfg.sensor_node_id != 0)
                {
                    if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
                    {
                        const auto *sc = _stack_slave->remoteMeteoCache(cfg.sensor_node_id);
                        if (sc && sc->has_data)
                        {
                            for (size_t s = 0; s < sc->item_count; ++s)
                            {
                                const auto &it = sc->items[s];
                                if (it.id == cfg.sensor_id)
                                {
                                    remote_has_temp = it.has_temp;
                                    remote_temp_c = it.temp_c;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        const auto *remote_cache = stackCache().meteoCache(cfg.sensor_node_id);
                        if (remote_cache && remote_cache->has_data)
                        {
                            for (size_t s = 0; s < remote_cache->item_count; ++s)
                            {
                                const auto &it = remote_cache->items[s];
                                if (it.id == cfg.sensor_id)
                                {
                                    remote_has_temp = it.has_temp;
                                    remote_temp_c = it.temp_c;
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    for (size_t s = 0; s < MeteoController::kSensorCount; ++s)
                    {
                        const auto *scfg = meteo.configByIndex(s);
                        if (scfg && scfg->id == cfg.sensor_id)
                        {
                            sensor_st = meteo.stateByIndex(s);
                            break;
                        }
                    }
                }
            }

            const char *sensor_label = "нет";
            const char *sensor_suffix = "";
            char sensor_buf[16] = {};
            if (cfg.sensor_id != ThermoController::kInvalidSensor)
            {
                if (cfg.sensor_node_id != 0)
                {
                    if (remote_has_temp)
                    {
                        dtostrf(remote_temp_c, 0, 1, sensor_buf);
                        sensor_label = sensor_buf;
                        sensor_suffix = "&deg;C";
                    }
                    else
                    {
                        sensor_label = "--";
                    }
                }
                else
                {
                    if (sensor_st && sensor_st->has_temp)
                    {
                        dtostrf(sensor_st->temp_c, 0, 1, sensor_buf);
                        sensor_label = sensor_buf;
                        sensor_suffix = "&deg;C";
                    }
                    else
                    {
                        sensor_label = "--";
                    }
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

            bool show_heat = true;
            bool show_cool = true;
            String heat_class = "icon heat ";
            String cool_class = "icon cool ";
            if (cfg.mode == ThermoController::Mode::Off)
            {
                show_heat = false;
                show_cool = false;
            }
            else if (cfg.mode == ThermoController::Mode::Heat)
            {
                show_cool = false;
                heat_class += st.heat_on ? "active" : "inactive";
            }
            else if (cfg.mode == ThermoController::Mode::Cool)
            {
                show_heat = false;
                cool_class += st.cool_on ? "active" : "inactive";
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
            items += String((int)(cfg.target_c + 0.5f));
            items += "&deg;C</span></div>";
            if (show_heat)
            {
                items += "<svg class=\"";
                items += heat_class;
                items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"22\" y=\"30\" width=\"76\" height=\"60\" rx=\"10\"/><line x1=\"36\" y1=\"40\" x2=\"36\" y2=\"80\"/><line x1=\"52\" y1=\"40\" x2=\"52\" y2=\"80\"/><line x1=\"68\" y1=\"40\" x2=\"68\" y2=\"80\"/><line x1=\"84\" y1=\"40\" x2=\"84\" y2=\"80\"/></svg>";
            }
            if (show_cool)
            {
                items += "<svg class=\"";
                items += cool_class;
                items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"18\" y=\"28\" width=\"84\" height=\"46\" rx=\"10\"/><line x1=\"28\" y1=\"44\" x2=\"92\" y2=\"44\"/><line x1=\"28\" y1=\"56\" x2=\"92\" y2=\"56\"/><line x1=\"40\" y1=\"78\" x2=\"34\" y2=\"92\"/><line x1=\"60\" y1=\"78\" x2=\"60\" y2=\"94\"/><line x1=\"80\" y1=\"78\" x2=\"86\" y2=\"92\"/></svg>";
            }
            items += "</div>";
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
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "<span class=\"status-dot ";
            items += state_class;
            items += "\"></span><span class=\"status-value ";
            if (strcmp(state_class, "status-heat") == 0)
                items += "status-text-heat";
            else if (strcmp(state_class, "status-cool") == 0)
                items += "status-text-cool";
            else
                items += "status-text-idle";
            items += "\">";
            items += state_label;
            items += "</span></div><input type=\"hidden\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_en_force\" value=\"\">";
            items += "<div class=\"form-row full\"><label>Датчик</label><select class=\"field mini\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_sensor\">";
            items += meteoSensorOptionsHtml_(cfg.sensor_id, cfg.sensor_node_id, sensor_used, remote_used, remote_used_count);
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
            items += ">auto</option></select></div><div class=\"form-row\"><label>Цель</label><input class=\"field temp\" type=\"number\" step=\"1\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_target\" value=\"";
            items += String((int)(cfg.target_c + 0.5f));
            items += "\"></div><div class=\"form-row\"><label>Гист.</label><input class=\"field temp\" type=\"number\" step=\"1\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_hyst\" value=\"";
            items += String((int)(cfg.hysteresis + 0.5f));
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
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
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
            items += "<div class=\"form-row\"><label>Клапан</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
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
            items += "<div class=\"form-row\"><label>Индикатор</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
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
            items += "</div></div></div>";
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

    String listStackI2cHtml_(uint32_t node_id) const
    {
        const StackI2cCache *cache = findStackI2cCache_(node_id, false);
        if (!cache)
            return "<tr><td colspan=\"2\" style=\"color:#94a3b8\"><strong>n/a</strong></td></tr>";
        if (cache->pending)
            return "<tr><td colspan=\"2\" style=\"color:#94a3b8\"><strong>pending</strong></td></tr>";
        if (!cache->has_data)
            return "<tr><td colspan=\"2\" style=\"color:#94a3b8\"><strong>no data</strong></td></tr>";
        String items;
        items.reserve(cache->item_count * 32 + 64);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            char addr_buf[8] = {};
            snprintf(addr_buf, sizeof(addr_buf), "0x%02X", (unsigned)it.addr);
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)it.bus);
            items += "</strong></td><td><strong>";
            items += addr_buf;
            items += "</strong></td></tr>";
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
        out += "<th>Unit</th><th>DeviceName</th><th>NodeID</th><th>IP</th><th>Тип</th>";
        out += "</tr></thead><tbody>";
        out += listStackNodesHtml_();
        out += "</tbody></table>";
        out += "</div>";
        return out;
    }

    String listStackNodesStatusHtml_() const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return "<p class=\"status\">Слейвы не подключены</p>";
        String html;
        html.reserve(256 + count * 160);
        html += "<table><thead><tr>";
        html += "<th>Узел</th><th>IP</th><th>Дата</th><th>Время</th><th>RTC</th><th>Плата</th><th>CPU</th><th>Вент.</th>";
        html += "</tr></thead><tbody>";
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            if (id == 0)
                continue;
            const String name = _stack_master->nodeNameAt(i);
            const String ip = _stack_master->nodeIpAt(i);
            const StackNodeStatusCache *cache = findStackNodeStatusCache_(id, false);
            const bool has_rtc = cache && cache->has_rtc && cache->last_rtc_ok;
            const bool has_plc = cache && cache->has_plc && cache->last_plc_ok;
            const String label = name.length() ? safeHtmlValue_(name, "") : stackNodeIdHex_(id);
            html += "<tr><td><strong>";
            html += label;
            html += "</strong></td><td>";
            html += safeHtmlValue_(ip, "n/a");
            html += "</td><td>";
            html += has_rtc ? safeHtmlValue_(cache->rtc_date, "n/a") : "n/a";
            html += "</td><td>";
            html += has_rtc ? safeHtmlValue_(cache->rtc_time, "n/a") : "n/a";
            html += "</td><td>";
            html += has_rtc ? formatTemp_(cache->rtc_temp) : "n/a";
            html += "</td><td>";
            html += has_plc ? formatTemp_(cache->board_temp) : "n/a";
            html += "</td><td>";
            html += has_plc ? formatTemp_(cache->cpu_temp) : "n/a";
            html += "</td><td>";
            if (has_plc)
                html += fanStatusIcon_(cache->fan_on);
            else
                html += "n/a";
            html += "</td></tr>";
        }
        html += "</tbody></table>";
        return html;
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
            items += "</td><td>";
            items += _stack_master->nodeIsControllerAt(i) ? "Контроллер" : "Модуль";
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

    String displayDeviceOptionsJson_() const
    {
        String out;
        out.reserve(256);
        out += "[";
        bool first = true;
        auto append = [&](const String &value, const String &label) {
            if (!first)
                out += ",";
            out += "{\"v\":";
            out += value;
            out += ",\"l\":\"";
            appendJsonEscaped_(out, label);
            out += "\"}";
            first = false;
        };
        append("0", "local");
        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t id = _stack_master->nodeIdAt(i);
                String name = _stack_master->nodeNameAt(i);
                const String label = name.length() ? name : stackNodeIdHex_(id);
                append(String((unsigned long)id), label);
            }
        }
        out += "]";
        return out;
    }

    String displaySocketOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const SocketController &sockets = _controllers->sockets();
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = sockets.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Socket #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackSocketsCache *cache = stackCache().socketsCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestSockets(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Socket #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }

    String displayLightOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const SocketController &sockets = _controllers->sockets();
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = sockets.lightConfigByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Light #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackLightsCache *cache = stackCache().lightsCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestLights(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Light #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }

    String displayMeteoOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const MeteoController &meteo = _controllers->meteo();
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = meteo.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Sensor #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackMeteoCache *cache = stackCache().meteoCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestMeteo(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Sensor #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }

    String displayThermoOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const ThermoController &thermo = _controllers->thermo();
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = thermo.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Thermo #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackThermoCache *cache = stackCache().thermoCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestThermo(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Thermo #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }

    String displayTankOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const TankController &tanks = _controllers->tanks();
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = tanks.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Tank #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackTankCache *cache = stackCache().tanksCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestTanks(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        appendJsonEscaped_(list, it.name);
                    else
                        list += String("Tank #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
        return out;
    }

    String displaySepticOptionsJson_() const
    {
        String out;
        out.reserve(512);
        out += "{";
        bool first_node = true;
        auto append_node = [&](const String &key, const String &list) {
            if (!first_node)
                out += ",";
            out += "\"";
            out += key;
            out += "\":";
            out += list;
            first_node = false;
        };

        String local;
        local.reserve(256);
        local += "[";
        bool first = true;
        if (_controllers)
        {
            const SepticController &septic = _controllers->septic();
            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = septic.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Septic #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master && _stack_cache)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                const StackCache::StackSepticCache *cache = stackCache().septicCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    const_cast<StackCache *>(_stack_cache)->requestSeptic(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    continue;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (size_t k = 0; k < cache->item_count; ++k)
                {
                    const auto &it = cache->items[k];
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.id)
                        list += String("Septic #") + String((unsigned)it.id);
                    else
                        list += String("Septic");
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                append_node(String((unsigned long)node_id), list);
            }
        }
        out += "}";
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

    String ringUsedPortsJson_(PortIO::PinType type) const { return globalUsedPortsJson_(type); }

    String globalUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        auto mark_used = [](bool used[], uint8_t port)
        {
            if (port < PortIO::PORT_COUNT)
                used[port] = true;
        };
        bool used[PortIO::PORT_COUNT] = {};
        if (_controllers)
        {
            SocketController &sockets = _controllers->sockets();
            MeteoController &meteo = _controllers->meteo();
            ThermoController &thermo = _controllers->thermo();
            TankController &tanks = _controllers->tanks();
            SepticController &septic = _controllers->septic();
            SecurityController &security = _controllers->security();
            RingController &ring = _controllers->ring();

            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = sockets.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->button_port);
                mark_used(used, cfg->relay_port);
            }
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = sockets.lightConfigByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->button_port);
                mark_used(used, cfg->relay_port);
            }
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = meteo.configByIndex(i);
                if (!cfg)
                    continue;
                if (cfg->type != MeteoController::SensorType::Dht22)
                    continue;
                mark_used(used, cfg->dht_pin);
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
            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = septic.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->warning_port);
                mark_used(used, cfg->alarm_port);
                mark_used(used, cfg->relay_warning);
                mark_used(used, cfg->relay_alarm);
            }
            if (security.sirenPort() != SecurityController::kInvalidPort)
                mark_used(used, security.sirenPort());
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                const auto *cfg = security.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->port);
            }
            const auto &rcfg = ring.config();
            mark_used(used, rcfg.button_port);
            mark_used(used, rcfg.relay_port);
        }
        if (_configs_manager)
            mark_used(used, _configs_manager->ringClientButtonPort());

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

    String listStackOwHtml_(uint32_t node_id) const
    {
        const StackOwCache *cache = findStackOwCache_(node_id, false);
        if (!cache)
            return "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>n/a</strong></td></tr>";
        if (cache->pending)
            return "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>pending</strong></td></tr>";
        if (!cache->has_data)
            return "<tr><td colspan=\"3\" style=\"color:#94a3b8\"><strong>no data</strong></td></tr>";
        String items;
        items.reserve(cache->item_count * 48 + 64);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)it.bus);
            items += "</strong></td><td><strong>";
            items += it.type[0] ? it.type : "n/a";
            items += "</strong></td><td><strong>";
            items += it.addr[0] ? it.addr : "n/a";
            items += "</strong></td></tr>";
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

        const bool fallback_enabled = request->hasParam("fallback_enabled", true);
        if (fallback_enabled != _configs_manager->stackFallbackEnabled())
        {
            _configs_manager->setStackFallbackEnabled(fallback_enabled);
            changed = true;
        }

        String fallback_host = request->hasParam("fallback_host", true)
                                   ? request->getParam("fallback_host", true)->value()
                                   : String("");
        fallback_host.trim();
        if (fallback_host != _configs_manager->stackFallbackHost())
        {
            _configs_manager->setStackFallbackHost(fallback_host);
            changed = true;
        }

        const bool slave_controller = request->hasParam("slave_controller", true);
        if (slave_controller != _configs_manager->stackSlaveController())
        {
            _configs_manager->setStackSlaveController(slave_controller);
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
        nav += F("<a href=\"/stack\">Стек</a> | <a href=\"/controllers\">Контроллеры</a> | <a href=\"/clients\">Клиенты</a> | <a href=\"/display\">Дисплей</a> | ");
        nav += F("<a href=\"/telegram\">Telegram</a> | <a href=\"/cloud\">Облако</a> | ");
        nav += F("<a href=\"/admin\">Система</a> | <a href=\"/logs\">Logs</a>");
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

    bool stackFallbackEnabled_() const
    {
        if (_configs_manager)
            return _configs_manager->stackFallbackEnabled();
        return false;
    }

    String stackFallbackHost_() const
    {
        if (_configs_manager)
            return _configs_manager->stackFallbackHost();
        return "";
    }

    bool stackSlaveController_() const
    {
        if (_configs_manager)
            return _configs_manager->stackSlaveController();
        return true;
    }

    String stackApiKey_() const
    {
        if (_configs_manager)
            return _configs_manager->stackApiKey();
        return "";
    }

    bool cloudEnabled_() const
    {
        if (_configs_manager)
            return _configs_manager->cloudEnabled();
        return false;
    }

    String cloudHost_() const
    {
        if (_configs_manager)
            return _configs_manager->cloudHost();
        return "";
    }

    uint16_t cloudPort_() const
    {
        if (_configs_manager)
            return _configs_manager->cloudPort();
        return 0;
    }

    String cloudPath_() const
    {
        if (_configs_manager)
            return _configs_manager->cloudPath();
        return "/";
    }

    bool cloudUseSsl_() const
    {
        if (_configs_manager)
            return _configs_manager->cloudUseSsl();
        return false;
    }

    uint32_t cloudReconnectMs_() const
    {
        if (_configs_manager)
            return _configs_manager->cloudReconnectMs();
        return 0;
    }

    uint32_t cloudEventMs_() const
    {
        if (_configs_manager)
            return _configs_manager->cloudEventIntervalMs();
        return 0;
    }

    String cloudApiKey_() const
    {
        if (_configs_manager)
            return _configs_manager->cloudApiKey();
        return "";
    }

    String cloudFwVersion_() const
    {
        if (_configs_manager)
            return _configs_manager->cloudFirmwareVersion();
        return "";
    }
    uint32_t cloudDeviceId_() const
    {
#if defined(ESP32)
        return (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFu);
#else
        return 0;
#endif
    }

    bool cloudConnected_() const
    {
        if (_cloud)
            return _cloud->isConnected();
        return false;
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

    static const char *displaySlotKindName_(DisplaySlotKind kind)
    {
        switch (kind)
        {
        case DisplaySlotKind::Time:
            return "time";
        case DisplaySlotKind::Socket:
            return "socket";
        case DisplaySlotKind::Light:
            return "light";
        case DisplaySlotKind::Meteo:
            return "meteo";
        case DisplaySlotKind::Thermo:
            return "thermo";
        case DisplaySlotKind::Tank:
            return "tank";
        case DisplaySlotKind::Septic:
            return "septic";
        case DisplaySlotKind::Security:
            return "security";
        case DisplaySlotKind::Text:
            return "text";
        case DisplaySlotKind::None:
        default:
            return "none";
        }
    }

    static const char *displaySlotFieldName_(DisplaySlotField field)
    {
        switch (field)
        {
        case DisplaySlotField::TimeHm:
            return "hm";
        case DisplaySlotField::TimeMin:
            return "min";
        case DisplaySlotField::SocketState:
            return "state";
        case DisplaySlotField::LightState:
            return "state";
        case DisplaySlotField::MeteoTemp:
            return "temp";
        case DisplaySlotField::MeteoHum:
            return "hum";
        case DisplaySlotField::ThermoState:
            return "state";
        case DisplaySlotField::TankLevel:
            return "level";
        case DisplaySlotField::SepticLevel:
            return "level";
        case DisplaySlotField::SecurityArmed:
            return "armed";
        case DisplaySlotField::Text:
            return "text";
        case DisplaySlotField::None:
        default:
            return "none";
        }
    }

    static bool parseDisplaySlotKind_(const String &input, DisplaySlotKind &out)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "none")
        {
            out = DisplaySlotKind::None;
            return true;
        }
        if (t == "time")
            out = DisplaySlotKind::Time;
        else if (t == "socket")
            out = DisplaySlotKind::Socket;
        else if (t == "light")
            out = DisplaySlotKind::Light;
        else if (t == "meteo")
            out = DisplaySlotKind::Meteo;
        else if (t == "thermo")
            out = DisplaySlotKind::Thermo;
        else if (t == "tank")
            out = DisplaySlotKind::Tank;
        else if (t == "septic")
            out = DisplaySlotKind::Septic;
        else if (t == "security")
            out = DisplaySlotKind::Security;
        else if (t == "text")
            out = DisplaySlotKind::Text;
        else
            return false;
        return true;
    }

    static bool parseDisplaySlotField_(const String &input, DisplaySlotField &out)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0 || t == "none")
        {
            out = DisplaySlotField::None;
            return true;
        }
        if (t == "hm")
            out = DisplaySlotField::TimeHm;
        else if (t == "min")
            out = DisplaySlotField::TimeMin;
        else if (t == "state")
            out = DisplaySlotField::SocketState;
        else if (t == "temp")
            out = DisplaySlotField::MeteoTemp;
        else if (t == "hum")
            out = DisplaySlotField::MeteoHum;
        else if (t == "level")
            out = DisplaySlotField::TankLevel;
        else if (t == "armed")
            out = DisplaySlotField::SecurityArmed;
        else if (t == "text")
            out = DisplaySlotField::Text;
        else
            return false;
        return true;
    }

    static bool parseUint_(const String &input, uint16_t &out)
    {
        String t = input;
        t.trim();
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const unsigned long v = strtoul(t.c_str(), nullptr, 10);
        if (v > 0xFFFFu)
            return false;
        out = (uint16_t)v;
        return true;
    }

    static bool parseUint_(const String &input, uint32_t &out)
    {
        String t = input;
        t.trim();
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const unsigned long v = strtoul(t.c_str(), nullptr, 10);
        out = (uint32_t)v;
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

    static MeteoController::SensorType parseMeteoTypeName_(const char *input)
    {
        if (!input || !input[0])
            return MeteoController::SensorType::None;
        String t = input;
        t.toLowerCase();
        if (t == "dht22")
            return MeteoController::SensorType::Dht22;
        if (t == "ds18b20")
            return MeteoController::SensorType::Ds18b20;
        return MeteoController::SensorType::None;
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

    static bool parseThermoSensor_(const String &input, uint8_t &out, uint32_t &out_node)
    {
        String t = input;
        t.trim();
        t.toLowerCase();
        out_node = 0;
        if (t.length() == 0 || t == "-" || t == "none")
        {
            out = ThermoController::kInvalidSensor;
            return true;
        }
        int sep = t.indexOf(':');
        String node_str;
        String sensor_str;
        if (sep >= 0)
        {
            node_str = t.substring(0, sep);
            sensor_str = t.substring(sep + 1);
        }
        else
        {
            sensor_str = t;
        }
        if (node_str.length())
        {
            for (size_t i = 0; i < (size_t)node_str.length(); ++i)
                if (node_str[i] < '0' || node_str[i] > '9')
                    return false;
            const unsigned long node_v = strtoul(node_str.c_str(), nullptr, 10);
            out_node = (uint32_t)node_v;
        }
        for (size_t i = 0; i < (size_t)sensor_str.length(); ++i)
            if (sensor_str[i] < '0' || sensor_str[i] > '9')
                return false;
        const unsigned long v = strtoul(sensor_str.c_str(), nullptr, 10);
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

    String meteoSensorOptionsHtml_(uint8_t selected_id, uint32_t selected_node_id,
                                   const uint8_t used_local[MeteoController::kSensorCount + 1],
                                   const uint32_t *used_remote, size_t used_remote_count) const
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
            const bool is_selected = (selected_node_id == 0 && id == selected_id);
            const bool is_used = (id <= MeteoController::kSensorCount) && (used_local[id] > 0) && !is_selected;
            if (is_used)
                continue;
            out += "<option value=\"";
            out += String((unsigned)id);
            out += "\"";
            if (is_selected)
                out += " selected";
            out += ">";
            String label_name;
            if (cfg->name.length())
            {
                label_name = cfg->name;
            }
            else if (cfg->source_node_id && cfg->source_sensor_id)
            {
                label_name = meteoRemoteSensorName_(cfg->source_node_id, cfg->source_sensor_id);
            }
            out += String((unsigned)id);
            if (label_name.length())
            {
                out += ": ";
                appendHtmlEscaped_(out, label_name.c_str());
            }
            out += "</option>";
        }
        if (selected_node_id != 0 && selected_id != 0)
        {
            const String remote_name = meteoRemoteSensorName_(selected_node_id, selected_id);
            out += "<option value=\"";
            out += String((unsigned long)selected_node_id);
            out += ":";
            out += String((unsigned)selected_id);
            out += "\" selected>";
            if (remote_name.length())
                appendHtmlEscaped_(out, remote_name.c_str());
            else
                out += String((unsigned)selected_id);
            out += "</option>";
        }
        return out;
    }

    String meteoRemoteSensorOptionsHtml_(uint8_t selected_id, uint32_t selected_node_id) const
    {
        String out;
        out += "<option value=\"\">-</option>";
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const size_t slots = _stack_slave->remoteMeteoCacheSlots();
            for (size_t i = 0; i < slots; ++i)
            {
                const auto &cache = _stack_slave->remoteMeteoCacheAt(i);
                if (cache.node_id == 0 || !cache.has_data || !cache.items)
                    continue;
                const String node_label = cache.node_name.length()
                                              ? cache.node_name
                                              : String("Node ") + String((unsigned long)cache.node_id);
                for (size_t s = 0; s < cache.item_count; ++s)
                {
                    const auto &it = cache.items[s];
                    if (!it.enabled)
                        continue;
                    const bool is_selected = (selected_node_id == cache.node_id && it.id == selected_id);
                    out += "<option value=\"";
                    out += String((unsigned long)cache.node_id);
                    out += ":";
                    out += String((unsigned)it.id);
                    out += "\"";
                    out += " data-node=\"";
                    out += String((unsigned long)cache.node_id);
                    out += "\"";
                    if (is_selected)
                        out += " selected";
                    out += ">";
                    appendHtmlEscaped_(out, node_label.c_str());
                    out += ": ";
                    if (it.name[0])
                        appendHtmlEscaped_(out, it.name);
                    else
                        out += String((unsigned)it.id);
                    out += "</option>";
                }
            }
            return out;
        }

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                if (node_id == 0)
                    continue;
                const StackCache::StackMeteoCache *cache = stackCache().meteoCache(node_id);
                if (!cache || !cache->has_data)
                    continue;
                String node_label;
                if (_stack_master)
                {
                    String ip;
                    uint16_t fw_ver = 0;
                    _stack_master->nodeInfo(node_id, node_label, ip, fw_ver);
                }
                if (!node_label.length())
                    node_label = String("Node ") + String((unsigned long)node_id);
                for (size_t s = 0; s < cache->item_count; ++s)
                {
                    const auto &it = cache->items[s];
                    if (!it.enabled)
                        continue;
                    const bool is_selected = (selected_node_id == node_id && it.id == selected_id);
                    out += "<option value=\"";
                    out += String((unsigned long)node_id);
                    out += ":";
                    out += String((unsigned)it.id);
                    out += "\"";
                    out += " data-node=\"";
                    out += String((unsigned long)node_id);
                    out += "\"";
                    if (is_selected)
                        out += " selected";
                    out += ">";
                    appendHtmlEscaped_(out, node_label.c_str());
                    out += ": ";
                    if (it.name[0])
                        appendHtmlEscaped_(out, it.name);
                    else
                        out += String((unsigned)it.id);
                    out += "</option>";
                }
            }
        }
        return out;
    }

    String meteoRemoteNodeOptionsHtml_(uint32_t selected_node_id) const
    {
        String out;
        out += "<option value=\"local\"";
        if (selected_node_id == 0)
            out += " selected";
        out += ">local</option>";
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const size_t slots = _stack_slave->remoteMeteoCacheSlots();
            for (size_t i = 0; i < slots; ++i)
            {
                const auto &cache = _stack_slave->remoteMeteoCacheAt(i);
                if (cache.node_id == 0 || !cache.has_data || !cache.items)
                    continue;
                const String node_label = cache.node_name.length()
                                              ? cache.node_name
                                              : String("Node ") + String((unsigned long)cache.node_id);
                out += "<option value=\"";
                out += String((unsigned long)cache.node_id);
                out += "\"";
                if (selected_node_id == cache.node_id)
                    out += " selected";
                out += ">";
                appendHtmlEscaped_(out, node_label.c_str());
                out += "</option>";
            }
            return out;
        }
        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                if (node_id == 0)
                    continue;
                String node_label;
                String ip;
                uint16_t fw_ver = 0;
                _stack_master->nodeInfo(node_id, node_label, ip, fw_ver);
                if (!node_label.length())
                    node_label = String("Node ") + String((unsigned long)node_id);
                out += "<option value=\"";
                out += String((unsigned long)node_id);
                out += "\"";
                if (selected_node_id == node_id)
                    out += " selected";
                out += ">";
                appendHtmlEscaped_(out, node_label.c_str());
                out += "</option>";
            }
        }
        return out;
    }

    String meteoRemoteLabel_(uint32_t node_id, uint8_t sensor_id) const
    {
        if (node_id == 0 || sensor_id == 0)
            return "";
        String node_label;
        const char *sensor_name = nullptr;
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const auto *cache = _stack_slave->remoteMeteoCache(node_id);
            if (cache && cache->has_data && cache->items)
            {
                if (cache->node_name.length())
                    node_label = cache->node_name;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id == sensor_id)
                    {
                        if (it.name[0])
                            sensor_name = it.name;
                        break;
                    }
                }
            }
        }
        else if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = stackCache().meteoCache(node_id);
            if (cache && cache->has_data)
            {
                String ip;
                uint16_t fw_ver = 0;
                _stack_master->nodeInfo(node_id, node_label, ip, fw_ver);
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id == sensor_id)
                    {
                        if (it.name[0])
                            sensor_name = it.name;
                        break;
                    }
                }
            }
        }
        if (!node_label.length())
            node_label = String("Node ") + String((unsigned long)node_id);
        String out = node_label;
        out += ": ";
        if (sensor_name && sensor_name[0])
            out += sensor_name;
        else
            out += String((unsigned)sensor_id);
        return out;
    }

    String meteoRemoteSensorName_(uint32_t node_id, uint8_t sensor_id) const
    {
        if (node_id == 0 || sensor_id == 0)
            return "";
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const auto *cache = _stack_slave->remoteMeteoCache(node_id);
            if (cache && cache->has_data && cache->items)
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != sensor_id)
                        continue;
                    if (it.name[0])
                        return String(it.name);
                    return String((unsigned)sensor_id);
                }
            }
            return "";
        }
        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = stackCache().meteoCache(node_id);
            if (cache && cache->has_data)
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != sensor_id)
                        continue;
                    if (it.name[0])
                        return String(it.name);
                    return String((unsigned)sensor_id);
                }
            }
        }
        return "";
    }

    bool meteoRemoteType_(uint32_t node_id, uint8_t sensor_id, MeteoController::SensorType &out) const
    {
        out = MeteoController::SensorType::None;
        if (node_id == 0 || sensor_id == 0)
            return false;
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const auto *cache = _stack_slave->remoteMeteoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
                return false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != sensor_id)
                    continue;
                out = parseMeteoTypeName_(it.type);
                return true;
            }
            return false;
        }
        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = stackCache().meteoCache(node_id);
            if (!cache || !cache->has_data)
                return false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != sensor_id)
                    continue;
                out = parseMeteoTypeName_(it.type);
                return true;
            }
        }
        return false;
    }

    bool isMeteoSensorActive_(uint8_t id) const
    {
        if (!_controllers)
            return false;
        const auto *cfg = _controllers->meteo().config(id);
        return cfg && cfg->enabled;
    }

    bool isRemoteMeteoSensorActive_(uint32_t node_id, uint8_t id) const
    {
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave)
        {
            if (!_stack_slave)
                return false;
            const auto *cache = _stack_slave->remoteMeteoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
                return false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id == id)
                    return it.enabled;
            }
            return false;
        }
        if (!_stack_master || stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        const StackCache::StackMeteoCache *cache = stackCache().meteoCache(node_id);
        if (!cache || !cache->has_data)
            return false;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (it.id == id)
                return it.enabled;
        }
        return false;
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
        hashAdd_(hash, _septic_status);
        hashAdd_(hash, _ring_status);
        hashAdd_(hash, _security_status);

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
                hashAdd_(hash, _controllers->ring().controllerEnabled() ? 1u : 0u);
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
            hashAdd_(hash, allowedUsersRowsHtml_());
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
    RfidReader *_rfid = nullptr;
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
    String _septic_status;
    String _ring_status;
    String _security_status;
    String _rfid_status;
    String _display_status;
    String _ring_client_status;
    String _clients_status;
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
    String _session_token;
    uint32_t _session_expire_ms = 0;
    uint32_t _session_ttl_ms = 10u * 60u * 1000u;
    bool _upload_set_cookie = false;
    bool _ota_set_cookie = false;
    bool _ota_in_progress = false;
};

#include "core/network/web/handlers/controllers_handler.hpp"
#include "core/network/web/handlers/sockets_handler.hpp"
#include "core/network/web/handlers/lights_handler.hpp"
#include "core/network/web/handlers/thermo_handler.hpp"
#include "core/network/web/handlers/index_handler.hpp"
#include "core/network/web/handlers/wifi_handler.hpp"
#include "core/network/web/handlers/manage_handler.hpp"
#include "core/network/web/handlers/ports_handler.hpp"
#include "core/network/web/handlers/buses_handler.hpp"
#include "core/network/web/handlers/stack_handler.hpp"
#include "core/network/web/handlers/clients_handler.hpp"
#include "core/network/web/handlers/display_handler.hpp"
#include "core/network/web/handlers/admin_handler.hpp"
#include "core/network/web/handlers/logs_handler.hpp"
#include "core/network/web/handlers/status_handler.hpp"
#include "core/network/web/handlers/septic_handler.hpp"
#include "core/network/web/handlers/ring_handler.hpp"
#include "core/network/web/handlers/security_handler.hpp"
#include "core/network/web/handlers/telegram_handler.hpp"
#include "core/network/web/handlers/cloud_handler.hpp"
#include "core/network/web/handlers/meteo_handler.hpp"
#include "core/network/web/handlers/tank_handler.hpp"
#include "core/network/web/handlers/rfid_handler.hpp"
#include "core/network/web/handlers/ring_client_handler.hpp"

inline void WebInterface::registerRoutes()
{
    IndexHandler::registerRoutes(*this, _server);
    WifiHandler::registerRoutes(*this, _server);
    ManageHandler::registerRoutes(*this, _server);
    PortsHandler::registerRoutes(*this, _server);
    BusesHandler::registerRoutes(*this, _server);
    _server.on("/stack/gen_key", HTTP_POST, [this](AsyncWebServerRequest *request) { handleStackGenKey_(request); });
    StackHandler::registerRoutes(*this, _server);
    ControllersHandler::registerRoutes(*this, _server);
    ClientsHandler::registerRoutes(*this, _server);
    DisplayHandler::registerRoutes(*this, _server);
    RfidHandler::registerRoutes(*this, _server);
    RingClientHandler::registerRoutes(*this, _server);
    SocketsHandler::registerRoutes(*this, _server);
    LightsHandler::registerRoutes(*this, _server);
    ThermoHandler::registerRoutes(*this, _server);
    MeteoHandler::registerRoutes(*this, _server);
    TankHandler::registerRoutes(*this, _server);
    SepticHandler::registerRoutes(*this, _server);
    RingHandler::registerRoutes(*this, _server);
    SecurityHandler::registerRoutes(*this, _server);
    TelegramHandler::registerRoutes(*this, _server);
    CloudHandler::registerRoutes(*this, _server);
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
    AdminHandler::registerRoutes(*this, _server);
    _server.on("/admin", HTTP_POST, [this](AsyncWebServerRequest *request) { handleAdminSave_(request); });
    LogsHandler::registerRoutes(*this, _server);
    _server.on("/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) { handleReboot_(request); });
    _server.on("/files", HTTP_GET, [this](AsyncWebServerRequest *request) { handleFileDownload_(request); });
    _server.on("/delete", HTTP_GET, [this](AsyncWebServerRequest *request) { handleDelete_(request); });
    StatusHandler::registerRoutes(*this, _server);
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



