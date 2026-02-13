#pragma once

#include "core/network/web/interfaces/web_interface_controllers_sockets.hpp"

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
        StackWateringCache *watering_cache = findStackWateringCacheByCmd_(cmd_id);
        StackI2cCache *i2c_cache = findStackI2cCacheByCmd_(cmd_id);
        StackOwCache *ow_cache = findStackOwCacheByCmd_(cmd_id);
        bool status_is_plc = false;
        bool status_is_rtc = false;
        StackNodeStatusCache *status_cache = findStackNodeStatusCacheByCmd_(cmd_id, status_is_plc, status_is_rtc);
        if (!sock_cache && !light_cache && !ports_cache && !ext_cache && !sec_cache && !meteo_cache &&
            !thermo_cache && !septic_cache && !tanks_cache && !watering_cache && !i2c_cache && !ow_cache &&
            !status_cache)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (doc["ok"] | false);
        JsonArrayConst items = doc["data"]["items"].as<JsonArrayConst>();

        if (sock_cache)
        {
            sock_cache->updated_ms = millis();
            if (!ok)
            {
                sock_cache->pending = false;
                sock_cache->last_ok = false;
                sock_cache->last_error = "";
                sock_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"].as<JsonObjectConst>();
                const uint16_t part = data["part"] | 1;
                const uint16_t parts = data["parts"] | 1;
                const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    sock_cache->item_count = 0;
                    sock_cache->has_data = false;
                    sock_cache->last_ok = false;
                    sock_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                }
                if (done)
                {
                    sock_cache->pending = false;
                    sock_cache->has_data = true;
                    sock_cache->last_ok = true;
                    sock_cache->node_id = node_id;
                }
                else
                {
                    sock_cache->pending = true;
                }
            }
        }

        if (light_cache)
        {
            light_cache->updated_ms = millis();
            if (!ok)
            {
                light_cache->pending = false;
                light_cache->last_ok = false;
                light_cache->last_error = "";
                light_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"].as<JsonObjectConst>();
                const uint16_t part = data["part"] | 1;
                const uint16_t parts = data["parts"] | 1;
                const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    light_cache->item_count = 0;
                    light_cache->has_data = false;
                    light_cache->last_ok = false;
                    light_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                }
                if (done)
                {
                    light_cache->pending = false;
                    light_cache->has_data = true;
                    light_cache->last_ok = true;
                    light_cache->node_id = node_id;
                }
                else
                {
                    light_cache->pending = true;
                }
            }
        }

        if (ports_cache)
        {
            ports_cache->updated_ms = millis();
            if (!ok)
            {
                ports_cache->pending = false;
                ports_cache->last_ok = false;
                ports_cache->last_error = "";
                ports_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"].as<JsonObjectConst>();
                const uint16_t part = data["part"] | 1;
                const uint16_t parts = data["parts"] | 1;
                const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
                JsonArrayConst ports = data["ports"].as<JsonArrayConst>();
                if (!ports.isNull())
                {
                    if (part <= 1)
                    {
                        ports_cache->item_count = 0;
                        ports_cache->has_data = false;
                        ports_cache->last_ok = false;
                        ports_cache->last_error = "";
                    }
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
                    if (done)
                    {
                        ports_cache->pending = false;
                        ports_cache->has_data = true;
                        ports_cache->last_ok = true;
                        ports_cache->node_id = node_id;
                    }
                    else
                    {
                        ports_cache->pending = true;
                    }
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
            sec_cache->updated_ms = millis();
            if (!ok)
            {
                sec_cache->pending = false;
                sec_cache->last_ok = false;
                sec_cache->last_error = "";
                sec_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"].as<JsonObjectConst>();
                const uint16_t part = data["part"] | 1;
                const uint16_t parts = data["parts"] | 1;
                const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    sec_cache->item_count = 0;
                    sec_cache->has_data = false;
                    sec_cache->last_ok = false;
                    sec_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                }
                if (done)
                {
                    sec_cache->pending = false;
                    sec_cache->has_data = true;
                    sec_cache->last_ok = true;
                    sec_cache->node_id = node_id;
                }
                else
                {
                    sec_cache->pending = true;
                }
            }
        }

        if (meteo_cache)
        {
            meteo_cache->updated_ms = millis();
            if (!ok)
            {
                meteo_cache->pending = false;
                meteo_cache->last_ok = false;
                meteo_cache->last_error = "";
                meteo_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"].as<JsonObjectConst>();
                const uint16_t part = data["part"] | 1;
                const uint16_t parts = data["parts"] | 1;
                const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    meteo_cache->item_count = 0;
                    meteo_cache->has_data = false;
                    meteo_cache->last_ok = false;
                    meteo_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                }
                if (done)
                {
                    meteo_cache->pending = false;
                    meteo_cache->has_data = true;
                    meteo_cache->last_ok = true;
                    meteo_cache->node_id = node_id;
                }
                else
                {
                    meteo_cache->pending = true;
                }
            }
        }

        if (thermo_cache)
        {
            thermo_cache->updated_ms = millis();
            if (!ok)
            {
                thermo_cache->pending = false;
                thermo_cache->last_ok = false;
                thermo_cache->last_error = "";
                thermo_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"].as<JsonObjectConst>();
                const uint16_t part = data["part"] | 1;
                const uint16_t parts = data["parts"] | 1;
                const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    thermo_cache->item_count = 0;
                    thermo_cache->has_data = false;
                    thermo_cache->last_ok = false;
                    thermo_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                }
                if (done)
                {
                    thermo_cache->pending = false;
                    thermo_cache->has_data = true;
                    thermo_cache->last_ok = true;
                    thermo_cache->node_id = node_id;
                }
                else
                {
                    thermo_cache->pending = true;
                }
            }
        }

        if (septic_cache)
        {
            septic_cache->updated_ms = millis();
            if (!ok)
            {
                septic_cache->pending = false;
                septic_cache->last_ok = false;
                septic_cache->last_error = "";
                septic_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"].as<JsonObjectConst>();
                const uint16_t part = data["part"] | 1;
                const uint16_t parts = data["parts"] | 1;
                const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    septic_cache->item_count = 0;
                    septic_cache->has_data = false;
                    septic_cache->last_ok = false;
                    septic_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                }
                if (done)
                {
                    septic_cache->pending = false;
                    septic_cache->has_data = true;
                    septic_cache->last_ok = true;
                    septic_cache->node_id = node_id;
                }
                else
                {
                    septic_cache->pending = true;
                }
            }
        }

        if (tanks_cache)
        {
            tanks_cache->updated_ms = millis();
            if (!ok)
            {
                tanks_cache->pending = false;
                tanks_cache->last_ok = false;
                tanks_cache->last_error = "";
                tanks_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"].as<JsonObjectConst>();
                const uint16_t part = data["part"] | 1;
                const uint16_t parts = data["parts"] | 1;
                const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    tanks_cache->item_count = 0;
                    tanks_cache->has_data = false;
                    tanks_cache->last_ok = false;
                    tanks_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                }
                if (done)
                {
                    tanks_cache->pending = false;
                    tanks_cache->has_data = true;
                    tanks_cache->last_ok = true;
                    tanks_cache->node_id = node_id;
                }
                else
                {
                    tanks_cache->pending = true;
                }
            }
        }

        if (watering_cache)
        {
            watering_cache->pending = false;
            watering_cache->updated_ms = millis();
            watering_cache->last_ok = false;
            watering_cache->last_error = "";
            if (!ok)
            {
                watering_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                watering_cache->item_count = 0;
                watering_cache->total = (uint16_t)(doc["data"]["total"] | 0u);
                watering_cache->offset = (uint16_t)(doc["data"]["offset"] | 0u);
                for (JsonObjectConst item : items)
                {
                    if (watering_cache->item_count >= WateringController::kRuleCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackWateringItem &dst = watering_cache->items[watering_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.status = item["status"] | false;
                    if (item["port"].is<int>() || item["port"].is<unsigned>())
                        dst.port = (uint8_t)(item["port"] | WateringController::kInvalidPort);
                    else
                    dst.port = WateringController::kInvalidPort;
                    dst.tank_id = (uint8_t)(item["tank"] | 0u);
                    dst.weekdays_mask = (uint8_t)(item["weekdays_mask"] | 0u);
                    dst.hour = (uint8_t)(item["hour"] | 0u);
                    dst.minute = (uint8_t)(item["minute"] | 0u);
                    dst.duration_sec = (uint32_t)(item["duration_s"] | 0u);
                    dst.hour2 = (uint8_t)(item["hour2"] | 0u);
                    dst.minute2 = (uint8_t)(item["minute2"] | 0u);
                    dst.duration2_sec = (uint32_t)(item["duration2_s"] | 0u);
                    dst.hour3 = (uint8_t)(item["hour3"] | 0u);
                    dst.minute3 = (uint8_t)(item["minute3"] | 0u);
                    dst.duration3_sec = (uint32_t)(item["duration3_s"] | 0u);
                    dst.resume_after_refill = item["resume"] | false;
                    dst.resume_level = (uint8_t)(item["resume_level"] | 0u);
                    dst.active = item["active"] | false;
                    dst.paused = item["paused"] | false;
                    dst.remaining_ms = (uint32_t)(item["remaining_ms"] | 0u);
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                }
                watering_cache->has_data = true;
                watering_cache->last_ok = true;
                watering_cache->node_id = node_id;
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

    StackWateringCache *findStackWateringCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        if (_stack_watering_cache.node_id == node_id)
            return &_stack_watering_cache;
        if (!create)
            return nullptr;
        _stack_watering_cache = StackWateringCache{};
        _stack_watering_cache.node_id = node_id;
        return &_stack_watering_cache;
    }

    const StackWateringCache *findStackWateringCache_(uint32_t node_id, bool create) const
    {
        return const_cast<WebInterface *>(this)->findStackWateringCache_(node_id, create);
    }

    StackWateringCache *findStackWateringCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        if (_stack_watering_cache.pending && _stack_watering_cache.pending_cmd_id == cmd_id)
            return &_stack_watering_cache;
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
        out += "</tr></thead><tbody id=\"stack-nodes-tbody\">";
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
            AvrController &avr = _controllers->avr();
            LeakController &leak = _controllers->leak();

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

            const auto &acfg = avr.config();
            mark_used(used, acfg.main_ok_port);
            mark_used(used, acfg.reserve_ok_port);
            mark_used(used, acfg.feedback_main_port);
            mark_used(used, acfg.feedback_reserve_port);
            mark_used(used, acfg.relay_main_port);
            mark_used(used, acfg.relay_reserve_port);

            for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            {
                const auto *cfg = leak.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->sensor_port);
                mark_used(used, cfg->valve_port);
                mark_used(used, cfg->alarm_port);
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
        nav += F("<a href=\"/stack\">Стек</a> | <a href=\"/controllers\">Контроллеры</a> | <a href=\"/users\">Пользователи</a> | <a href=\"/display\">Дисплей</a> | ");
        nav += F("<a href=\"/rules\">Правила</a> | ");
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

