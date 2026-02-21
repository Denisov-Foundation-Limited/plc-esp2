#pragma once

#ifndef WEB_INTERFACE_CLASS_CONTEXT
class WebInterface;
#else

#include "core/network/web/interfaces/web_interface_controllers_sockets.hpp"

    String indexDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\" style=\"margin: 6px 0 10px;\">";
        html += String("<span class=\"status\">") + WebUiRu::kDevice + "</span>";
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

#include "core/network/web/interfaces/web_interface_controllers_lights.hpp"

#include "core/network/web/interfaces/web_interface_controllers_security.hpp"

#include "core/network/web/interfaces/web_interface_controllers_meteo.hpp"

#include "core/network/web/interfaces/web_interface_controllers_thermo.hpp"

#include "core/network/web/interfaces/web_interface_controllers_septic.hpp"

#include "core/network/web/interfaces/web_interface_controllers_tanks.hpp"

#include "core/network/web/interfaces/web_interface_controllers_watering.hpp"

#include "core/network/web/interfaces/web_interface_controllers_ring.hpp"

    String busesDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\" style=\"margin-bottom:10px;\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"buses-device\" class=\"mini\">";
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
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"ports-device\" class=\"mini\">";
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

    String stackBusesStatusText_(uint32_t node_id) const
    {
        const auto *i2c = _stack_cache ? _stack_cache->i2cCache(node_id) : nullptr;
        const auto *ow = _stack_cache ? _stack_cache->owCache(node_id) : nullptr;
        if (!i2c && !ow)
            return WebUiRu::kNoDataFromSlave;
        if ((i2c && i2c->pending) || (ow && ow->pending))
            return "";
        if (i2c && !i2c->last_ok && i2c->last_error.length())
        {
            String msg = WebUiRu::Controllers::kI2c;
            msg += i2c->last_error;
            return msg;
        }
        if (ow && !ow->last_ok && ow->last_error.length())
        {
            String msg = WebUiRu::Controllers::kOw;
            msg += ow->last_error;
            return msg;
        }
        const bool i2c_ok = i2c && i2c->has_data;
        const bool ow_ok = ow && ow->has_data;
        if (!i2c_ok && !ow_ok)
            return WebUiRu::kNoDataFromSlave;
        return WebUiRu::kStatusOk;
    }

    String stackPortsStatusText_(uint32_t node_id) const
    {
        const auto *ports = _stack_cache ? _stack_cache->portsCache(node_id) : nullptr;
        const auto *exts = _stack_cache ? _stack_cache->extendersCache(node_id) : nullptr;
        if (!ports && !exts)
            return WebUiRu::kNoDataFromSlave;
        if ((ports && ports->pending) || (exts && exts->pending))
            return "";
        if (ports && !ports->last_ok && ports->last_error.length())
        {
            String msg = WebUiRu::Controllers::kPorts;
            msg += ports->last_error;
            return msg;
        }
        if (exts && !exts->last_ok && exts->last_error.length())
        {
            String msg = WebUiRu::Controllers::kExtenders;
            msg += exts->last_error;
            return msg;
        }
        const bool ports_ok = ports && ports->has_data;
        const bool exts_ok = exts && exts->has_data;
        if (!ports_ok && !exts_ok)
            return WebUiRu::kNoDataFromSlave;
        return WebUiRu::kStatusOk;
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

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame)
    {
        // Fallback local stack caches removed: StackCache is the single source of truth.
        (void)node_id;
        (void)frame;
    }

    bool requestStackPorts_(uint32_t node_id)
    {
        return _stack_cache && _stack_cache->requestPorts(node_id);
    }

    bool requestStackExtenders_(uint32_t node_id)
    {
        return _stack_cache && _stack_cache->requestExtenders(node_id);
    }

    bool requestStackI2c_(uint32_t node_id, bool run)
    {
        return _stack_cache && _stack_cache->requestI2c(node_id, run);
    }

    bool requestStackOw_(uint32_t node_id, bool run)
    {
        return _stack_cache && _stack_cache->requestOw(node_id, run);
    }

    bool requestStackPlcStatus_(uint32_t node_id)
    {
        return _stack_cache && _stack_cache->requestPlcStatus(node_id);
    }

    bool requestStackRtcStatus_(uint32_t node_id)
    {
        return _stack_cache && _stack_cache->requestRtcStatus(node_id);
    }

    uint16_t nextStackCmdId_()
    {
        ++_stack_cmd_id;
        if (_stack_cmd_id == 0)
            _stack_cmd_id = 1;
        return _stack_cmd_id;
    }

#include "core/network/web/interfaces/web_interface_controllers_display.hpp"

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
        const auto *cache = _stack_cache ? _stack_cache->i2cCache(node_id) : nullptr;
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
        out += WebUiRu::Controllers::kText;
        out += "<table><thead><tr>";
        out += WebUiRu::Controllers::kUnitDevicenameNodeidIp;
        out += "</tr></thead><tbody id=\"stack-nodes-tbody\">";
        out += listStackNodesHtml_();
        out += "</tbody></table>";
        out += "</div>";
        return out;
    }

    String listStackNodesStatusHtml_() const
    {
        String html;
        html.reserve(4096);
        html += "<tr><th>";
        html += WebUiRu::Controllers::kIpRtcCpu;
        html += "</th></tr>";
        if (!_stack_master)
            return WebUiRu::Controllers::kText3;
        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return WebUiRu::Controllers::kText4;
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            const auto *cache = _stack_cache ? _stack_cache->statusCache(id) : nullptr;
            const bool has_rtc = cache && cache->has_rtc && cache->last_rtc_ok;
            const bool has_plc = cache && cache->has_plc && cache->last_plc_ok;
            html += "<tr><td><strong>";
            html += _stack_master->nodeNameAt(i);
            html += "</strong></td><td><strong>";
            html += _stack_master->nodeIpAt(i);
            html += "</strong></td><td><strong>";
            html += has_rtc ? safeHtmlValue_(cache->rtc_date, "n/a") : "n/a";
            html += "</strong></td><td><strong>";
            html += has_rtc ? safeHtmlValue_(cache->rtc_time, "n/a") : "n/a";
            html += "</strong></td><td><strong>";
            html += has_rtc ? formatTemp_(cache->rtc_temp) : "n/a";
            html += "</strong></td><td><strong>";
            html += has_plc ? formatTemp_(cache->board_temp) : "n/a";
            html += "</strong></td><td><strong>";
            html += has_plc ? formatTemp_(cache->cpu_temp) : "n/a";
            html += "</strong></td><td class=\"center\"><strong>";
            if (has_plc)
                html += fanStatusIcon_(cache->fan_on);
            else
                html += "n/a";
            html += "</strong></td></tr>";
        }
        return html;
    }

    String listStackNodesHtml_() const
    {
        if (!_stack_master)
            return WebUiRu::Controllers::kText3;
        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return WebUiRu::Controllers::kText4;
        String items;
        items.reserve(1024);
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            const String name = _stack_master->nodeNameAt(i);
            const String ip = _stack_master->nodeIpAt(i);
            items += "<tr data-node=\"";
            items += String((unsigned long)id);
            items += "\"><td><strong>";
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
            items += _stack_master->nodeIsControllerAt(i) ? WebUiRu::Controllers::kText5 : WebUiRu::Controllers::kText6;
            items += "</td></tr>";
        }
        return items;
    }

    String globalUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (!_controllers)
            return "[]";
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            if (!_controllers->gpioPortUsedByType(i, type))
                continue;
            if (!first)
                out += ",";
            out += String((unsigned)i);
            first = false;
        }
        out += "]";
        return out;
    }

    static bool stackPortTypeMatch_(const StackCache::StackPortItem &it, PortIO::PinType type)
    {
        const bool has_pin_type = (it.pin_type != 0xFFu);
        if (has_pin_type && ((uint8_t)type == it.pin_type))
            return true;

        String t = it.type;
        t.toLowerCase();
        switch (type)
        {
        case PortIO::PinType::Relay:
            return t == "relay" || t == "output" || t == "out" || t == "rly";
        case PortIO::PinType::DInput:
            return t == "dinput" || t == "din" || t == "input" || t == "button" || t == "btn" || t == "switch";
        case PortIO::PinType::Sensor:
            return t == "sensor";
        case PortIO::PinType::Button:
            return t == "button" || t == "btn" || t == "dinput" || t == "din" || t == "input" || t == "switch";
        case PortIO::PinType::Led:
            return t == "led";
        case PortIO::PinType::System:
            return t == "system";
        case PortIO::PinType::Buzzer:
            return t == "buzzer";
        case PortIO::PinType::Fan:
            return t == "fan";
        default:
            break;
        }
        return false;
    }

    String stackPortOptionsJson_(uint32_t node_id, PortIO::PinType type) const
    {
        String out;
        out.reserve(256);
        out += "[";
        const auto *cache = _stack_cache ? _stack_cache->portsCache(node_id) : nullptr;
        if (!cache || !cache->has_data || !cache->items)
            return "[]";

        bool first = true;
        size_t matched = 0;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (!stackPortTypeMatch_(it, type))
                continue;
            ++matched;
            if (!first)
                out += ",";
            out += "{\"v\":";
            out += String((unsigned)it.id);
            out += ",\"l\":\"";
            out += "#";
            out += String((unsigned)it.id);
            out += " ";
            if (it.type[0])
                appendJsonEscaped_(out, it.type);
            else
                out += "Port";
            if (it.loc[0])
            {
                out += " @";
                appendJsonEscaped_(out, it.loc);
            }
            if (it.pin >= 0)
            {
                out += " GPIO";
                out += String((int)it.pin);
            }
            out += "\"}";
            first = false;
        }
        // Fallback: old/incompatible slave may send unknown type/ptype values.
        // Keep dropdown usable by exposing all controllable ports.
        if (matched == 0)
        {
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (!it.ctrl)
                    continue;
                if (!first)
                    out += ",";
                out += "{\"v\":";
                out += String((unsigned)it.id);
                out += ",\"l\":\"";
                out += "#";
                out += String((unsigned)it.id);
                if (it.type[0])
                {
                    out += " ";
                    appendJsonEscaped_(out, it.type);
                }
                if (it.loc[0])
                {
                    out += " @";
                    appendJsonEscaped_(out, it.loc);
                }
                if (it.pin >= 0)
                {
                    out += " GPIO";
                    out += String((int)it.pin);
                }
                out += "\"}";
                first = false;
            }
        }
        out += "]";
        return out;
    }

    String stackUsedPortsJson_(uint32_t node_id, PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        const auto *cache = _stack_cache ? _stack_cache->portsCache(node_id) : nullptr;
        if (!cache || !cache->has_data || !cache->items)
            return "[]";

        bool first = true;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (!stackPortTypeMatch_(it, type))
                continue;
            if (!it.used)
                continue;
            if (!first)
                out += ",";
            out += String((unsigned)it.id);
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
        const auto *cache = _stack_cache ? _stack_cache->owCache(node_id) : nullptr;
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
            if (!requireWebAdmin_(request, &set_cookie))
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
            if (!requireWebAdmin_(request, &set_cookie))
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
        if (!requireWebAdmin_(request, &set_cookie))
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
        if (!requireWebAdmin_(request, &set_cookie))
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
        if (!requireWebAdmin_(request, &set_cookie))
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
        if (!requireWebAdmin_(request, &set_cookie))
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
        if (!requireWebAdmin_(request, &set_cookie))
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
        if (!requireWebAdmin_(request, &set_cookie))
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
        if (!requireWebAdmin_(request, &set_cookie))
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

    bool hasUsersRegistryWebAuth_() const
    {
        if (!_users)
            return false;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.username.length() == 0)
                continue;
            if (!u.hasWebPassword())
                continue;
            return true;
        }
        return false;
    }

    bool checkLegacyAdminAuth_(const String &user, const String &pass) const
    {
        if (_cli_auth)
        {
            String ulow = user;
            ulow.toLowerCase();
            if (ulow == CliConsole::kAdminUser && _cli_auth->adminPasswordSet() && _cli_auth->checkAdminPassword(pass))
                return true;
        }
        return false;
    }

    bool findUsersRegistryAuth_(const String &user, const String &pass, size_t &user_idx) const
    {
        user_idx = 0;
        if (!_users)
            return false;
        const String login = UsersRegistry::normalizeUsername(user);
        if (login.length() == 0)
            return false;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.username.length() == 0 || !u.hasWebPassword())
                continue;
            if (UsersRegistry::normalizeUsername(u.username) != login)
                continue;
            if (!u.checkWebPassword(pass))
                continue;
            user_idx = i;
            return true;
        }
        return false;
    }

    bool checkUsersRegistryAuth_(const String &user, const String &pass) const
    {
        size_t user_idx = 0;
        return findUsersRegistryAuth_(user, pass, user_idx);
    }

    bool checkAuth_(AsyncWebServerRequest *request, bool *set_cookie, bool require_session = false)
    {
        if (set_cookie)
            *set_cookie = false;
        const bool users_web_auth = hasUsersRegistryWebAuth_();

        String token;
        if (extractSessionToken_(request, token) && sessionValid_(token))
        {
            if (!sessionPrincipalValid_())
            {
                clearSession_();
            }
            else
            {
                refreshSession_();
                return true;
            }
        }
        if (require_session)
        {
            sendText_(request, 403, "text/plain", "Session required", false);
            return false;
        }

        String user;
        String pass;
        if (parseBasicAuth_(request, user, pass))
        {
            size_t user_idx = 0;
            if (findUsersRegistryAuth_(user, pass, user_idx))
            {
                issueSession_((int16_t)user_idx);
                if (set_cookie)
                    *set_cookie = true;
                return true;
            }
            if (!users_web_auth && checkLegacyAdminAuth_(user, pass))
            {
                issueSession_(-1);
                if (set_cookie)
                    *set_cookie = true;
                return true;
            }
        }
        if (users_web_auth)
        {
            requestBasicAuth_(request);
            return false;
        }
        if (_auth_enabled && request->authenticate(_auth_user.c_str(), _auth_pass.c_str()))
        {
            issueSession_(-1);
            if (set_cookie)
                *set_cookie = true;
            return true;
        }
        if (!_auth_enabled && !_cli_auth)
            return true;
        requestBasicAuth_(request);
        return false;
    }

    bool checkAuthApi_(AsyncWebServerRequest *request, bool *set_cookie)
    {
        if (set_cookie)
            *set_cookie = false;
        if (!request)
            return false;
        const bool users_web_auth = hasUsersRegistryWebAuth_();

        String token;
        if (extractSessionToken_(request, token) && sessionValid_(token))
        {
            if (!sessionPrincipalValid_())
            {
                clearSession_();
            }
            else
            {
                refreshSession_();
                return true;
            }
        }

        String user;
        String pass;
        if (parseBasicAuth_(request, user, pass))
        {
            size_t user_idx = 0;
            if (findUsersRegistryAuth_(user, pass, user_idx))
            {
                issueSession_((int16_t)user_idx);
                if (set_cookie)
                    *set_cookie = true;
                return true;
            }
            if (!users_web_auth && checkLegacyAdminAuth_(user, pass))
            {
                issueSession_(-1);
                if (set_cookie)
                    *set_cookie = true;
                return true;
            }
        }

        sendText_(request, 403, "application/json", "{\"ok\":false,\"err\":\"auth\"}", false);
        return false;
    }

    const UsersRegistry::User *sessionUser_() const
    {
        if (!_users || _session_user_idx < 0)
            return nullptr;
        const size_t idx = (size_t)_session_user_idx;
        if (idx >= _users->size())
            return nullptr;
        const auto &u = _users->user(idx);
        if (!u.enabled)
            return nullptr;
        return &u;
    }

    uint8_t aclUnitByNodeId_(uint32_t node_id) const
    {
        if (node_id == 0)
            return 0;
        if (!_stack_master)
            return UsersRegistry::kAclUnitCount;
        for (size_t i = 0; i < _stack_master->nodeCount(); ++i)
        {
            if (_stack_master->nodeIdAt(i) == node_id)
            {
                const size_t unit = i + 1u;
                if (unit >= (size_t)UsersRegistry::kAclUnitCount)
                    return UsersRegistry::kAclUnitCount;
                return (uint8_t)unit;
            }
        }
        return UsersRegistry::kAclUnitCount;
    }

    bool webAclControllerAllowed_(UsersRegistry::AclController ctrl, uint32_t node_id = 0) const
    {
        if (webSessionIsAdmin_())
            return true;
        const UsersRegistry::User *u = sessionUser_();
        if (!u)
            return false;
        const uint8_t unit = aclUnitByNodeId_(node_id);
        if (unit >= UsersRegistry::kAclUnitCount)
            return false;
        return u->controllerAllowed(unit, ctrl);
    }

    bool webAclCanViewItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id = 0) const
    {
        if (webSessionIsAdmin_())
            return true;
        const UsersRegistry::User *u = sessionUser_();
        if (!u)
            return false;
        const uint8_t unit = aclUnitByNodeId_(node_id);
        if (unit >= UsersRegistry::kAclUnitCount)
            return false;
        return u->canViewItem(unit, ctrl, item_id);
    }

    bool webAclCanControlItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id = 0) const
    {
        if (webSessionIsAdmin_())
            return true;
        const UsersRegistry::User *u = sessionUser_();
        if (!u)
            return false;
        const uint8_t unit = aclUnitByNodeId_(node_id);
        if (unit >= UsersRegistry::kAclUnitCount)
            return false;
        return u->canControlItem(unit, ctrl, item_id);
    }

    bool webSessionIsAdmin_() const
    {
        const UsersRegistry::User *u = sessionUser_();
        if (u)
            return u->tg_admin;
        if (!hasUsersRegistryWebAuth_() && _session_user_idx < 0)
            return sessionPrincipalValid_();
        return false;
    }

    bool requireWebAdmin_(AsyncWebServerRequest *request, bool *set_cookie)
    {
        if (webSessionIsAdmin_())
            return true;
        sendText_(request, 403, "text/plain", "Admin only", set_cookie ? *set_cookie : false);
        return false;
    }

    bool requireWebAclController_(AsyncWebServerRequest *request, bool *set_cookie,
                                  UsersRegistry::AclController ctrl, uint32_t node_id = 0)
    {
        if (webAclControllerAllowed_(ctrl, node_id))
            return true;
        sendText_(request, 403, "text/plain", "ACL deny", set_cookie ? *set_cookie : false);
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
        String nav = WebUiRu::Controllers::kFcplc;
        if (webSessionIsAdmin_())
        {
            nav += WebUiRu::Controllers::kText7;
            nav += WebUiRu::Controllers::kText8;
            nav += WebUiRu::Controllers::kText9;
            nav += WebUiRu::Controllers::kText10;
            nav += WebUiRu::Controllers::kTelegram;
            nav += WebUiRu::Controllers::kLogs;
        }
        nav += F("</div>");
        nav += F(R"HTML(
<div id="global-stack-toast-wrap" style="position:fixed;left:16px;top:16px;display:flex;flex-direction:column;gap:8px;z-index:9999;pointer-events:none"></div>
<script>
(function(){
  const wrap = document.getElementById('global-stack-toast-wrap');
  if (!wrap) return;
  function showToast(msg, kind){
    const el = document.createElement('div');
    el.textContent = msg;
    const ok = kind !== 'error';
    const border = ok ? 'rgba(34,197,94,0.45)' : 'rgba(239,68,68,0.50)';
    const bg = ok ? 'rgba(6,33,23,.94)' : 'rgba(46,12,12,.94)';
    const color = ok ? '#d1fae5' : '#fee2e2';
    el.style.cssText = 'min-width:220px;max-width:340px;padding:10px 12px;border-radius:10px;border:1px solid ' + border + ';background:' + bg + ';color:' + color + ';font-size:13px;box-shadow:0 8px 20px rgba(0,0,0,.35);opacity:0;transform:translateY(8px);transition:opacity .18s ease,transform .18s ease';
    wrap.appendChild(el);
    requestAnimationFrame(()=>{ el.style.opacity='1'; el.style.transform='translateY(0)'; });
    setTimeout(()=>{ el.style.opacity='0'; el.style.transform='translateY(8px)'; setTimeout(()=>{ if(el.parentNode) el.parentNode.removeChild(el); },220); },3200);
  }
  let known = null;
  async function poll(){
    try{
      const r = await fetch('/stack/online_snapshot', {cache:'no-store', credentials:'same-origin'});
      if(!r.ok) return;
      const data = await r.json();
      const next = new Map();
      if(Array.isArray(data)){
        data.forEach((n)=>{
          const id = String((n && n.id != null) ? n.id : '').trim();
          if(!id) return;
          const name = String((n && n.name) ? n.name : id);
          const sync = !!(n && (n.sync === 1 || n.sync === true || n.sync === '1'));
          next.set(id, { name, sync });
        });
      }
      if (known === null){
        known = next;
        return;
      }
      next.forEach((cur,id)=>{
        const prev = known.get(id);
        if(!prev) showToast('Подключен слейв: ' + cur.name + ' id: 0x' + Number(id).toString(16).toUpperCase().padStart(8, '0'), 'success');
        if((!prev || !prev.sync) && cur.sync){
          showToast('Sync slave unit complete: ' + cur.name + ' id: 0x' + Number(id).toString(16).toUpperCase().padStart(8, '0'), 'success');
        }
      });
      known.forEach((prev,id)=>{
        if(!next.has(id)) showToast('Отключен слейв: ' + prev.name + ' id: 0x' + Number(id).toString(16).toUpperCase().padStart(8, '0'), 'error');
      });
      known = next;
    }catch(e){}
  }
  poll();
  setInterval(poll, 3000);
})();
(function(){
  function isStackGpioVisiblePage(){
    try{
      if (!window.location) return false;
      const p0 = window.location.pathname || '';
      const p = p0.endsWith('/') && p0.length > 1 ? p0.slice(0, -1) : p0;
      return p === '/thermo' || p === '/tanks';
    }catch(e){
      return false;
    }
  }
  function isStackView(){
    try{
      const u = new URL(window.location.href);
      const unit = (u.searchParams.get('unit') || '').toLowerCase();
      if (unit === 'stack') return true;
      if (u.searchParams.get('node') || u.searchParams.get('node_id')) return true;
      return false;
    }catch(e){
      return false;
    }
  }
  function hideNode(el){
    if(!el) return;
    const row = el.closest('.form-row') || el.closest('td') || el.closest('th') || el.parentElement;
    if (row) row.style.display = 'none';
    else el.style.display = 'none';
  }
  function hideGpioFields(){
    if (isStackGpioVisiblePage()) return;
    if(!isStackView()) return;
    const sels = document.querySelectorAll(
      'select.socket-select,select.light-select,select.security-port,select.meteo-pin,select.thermo-select,' +
      'select.tank-select,select.septic-select,select.watering-select,select.ring-select,select.avr-select,' +
      'select.leak-select,select[data-type=\"relay\"],select[data-type=\"dinput\"],select[data-type=\"input\"],' +
      'select[data-type=\"button\"]'
    );
    sels.forEach(hideNode);
    const ins = document.querySelectorAll(
      'input[name$=\"_relay\"],input[name$=\"_btn\"],input[name$=\"_pin\"],input[name*=\"_port\"]'
    );
    ins.forEach(hideNode);
  }
  if (document.readyState === 'loading')
    document.addEventListener('DOMContentLoaded', hideGpioFields, { once: true });
  else
    hideGpioFields();
  let n = 0;
  const t = setInterval(()=>{
    hideGpioFields();
    n++;
    if(n >= 20) clearInterval(t);
  }, 500);
})();
</script>
)HTML");
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
        case DisplaySlotKind::Avr:
            return "avr";
        case DisplaySlotKind::Leak:
            return "leak";
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
        case DisplaySlotField::AvrSource:
            return "avr_source";
        case DisplaySlotField::AvrMainOk:
            return "avr_main_ok";
        case DisplaySlotField::AvrReserveOk:
            return "avr_reserve_ok";
        case DisplaySlotField::LeakState:
            return "leak_state";
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
        else if (t == "avr")
            out = DisplaySlotKind::Avr;
        else if (t == "leak")
            out = DisplaySlotKind::Leak;
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
        else if (t == "avr_source")
            out = DisplaySlotField::AvrSource;
        else if (t == "avr_main_ok")
            out = DisplaySlotField::AvrMainOk;
        else if (t == "avr_reserve_ok")
            out = DisplaySlotField::AvrReserveOk;
        else if (t == "leak_state")
            out = DisplaySlotField::LeakState;
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

#endif // WEB_INTERFACE_CLASS_CONTEXT


