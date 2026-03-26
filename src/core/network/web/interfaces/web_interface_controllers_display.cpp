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

#include "core/network/web/web_interface.hpp"

namespace
{
template <typename Fn>
void forEachDisplayStackDevice_(const WebInterface &web, Fn &&fn)
{
    if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
        return;
    const size_t count = web.network()->stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!web.network()->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
            continue;
        fn(device);
    }
}
}

String WebInterfaceControllersDisplayHelper::displaySlotsHtml_(const WebInterface &web) {
        String html;
        html.reserve(2048);
        const size_t count = web._configs_manager ? web._configs_manager->displaySlotCount() : 8;
        const size_t total = (count > 0) ? count : 8;
        for (size_t i = 0; i < total && i < 8; ++i)
        {
            DisplaySlotConfig slot{};
            if (web._configs_manager)
                web._configs_manager->displaySlot(i, slot);
            const uint8_t row = (uint8_t)(i / 4);
            const uint8_t col = (uint8_t)(i % 4);
            const String idx = String((unsigned)i);
            html += "<div class=\"slot display-slot\" data-kind=\"";
            html += web.displaySlotKindName_(slot.kind);
            html += "\" data-index=\"";
            html += String((unsigned)slot.index);
            html += "\" data-field=\"";
            html += web.displaySlotFieldName_(slot.field);
            html += "\" data-node=\"";
            html += String((unsigned long)slot.node_id);
            html += "\">";
            html += "<div class=\"slot-head\">L";
            html += String((unsigned)(row + 1));
            html += "-";
            html += String((unsigned)(col + 1));
            html += "</div>";
            html += WebUiRu::Display::kSelectClassFieldSlotKindNameDs;
            html += idx;
            html += "_kind\"></select>";
            html += WebUiRu::Display::kSelectClassFieldSlotNodeNameDs;
            html += idx;
            html += "_node\"></select>";
            html += WebUiRu::Display::kSelectClassFieldSlotIndexNameDs;
            html += idx;
            html += "_index\"></select>";
            html += WebUiRu::Display::kSelectClassFieldSlotFieldNameDs;
            html += idx;
            html += "_field\"></select>";
            html += WebUiRu::Display::kInputClassFieldSlotTextTypeText;
            html += idx;
            html += "_text\" maxlength=\"4\" value=\"";
            if (slot.text[0])
                web.appendHtmlEscaped_(html, slot.text);
            html += "\"></div>";
        }
        return html;
    }

String WebInterfaceControllersDisplayHelper::displayDeviceOptionsJson_(const WebInterface &web) {
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
            web.appendJsonEscaped_(out, label);
            out += "\"}";
            first = false;
        };
        append("0", "local");
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
            const String label = device.name[0] ? String(device.name) : web.stackNodeIdHex_(device.node_id);
            append(String((unsigned long)device.node_id), label);
        });
        out += "]";
        return out;
    }

String WebInterfaceControllersDisplayHelper::displaySocketOptionsJson_(const WebInterface &web) {
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
        if (web._controllers)
        {
            const SocketController &sockets = web._controllers->sockets();
            auto guard = sockets.lockGuard();
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
                    web.appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Socket #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
                const uint32_t node_id = device.node_id;
                StackUnitSnapshot::State snapshot{};
                if (!web.network()->stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
                {
                    const_cast<WebInterface &>(web).requestStackSockets_(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    return;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (uint8_t k = 0; k < snapshot.socket_count && k < StackUnitSnapshot::kSocketCount; ++k)
                {
                    StackUnitSnapshot::SocketItem it{};
                    if (!web.network()->stackIndexSocketAt(node_id, k, it))
                        continue;
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        web.appendJsonEscaped_(list, it.name);
                    else
                        list += String("Socket #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                if (snapshot.sockets_enabled > snapshot.socket_count)
                    const_cast<WebInterface &>(web).requestStackSockets_(node_id);
                append_node(String((unsigned long)node_id), list);
        });
        out += "}";
        return out;
    }

String WebInterfaceControllersDisplayHelper::displayLightOptionsJson_(const WebInterface &web) {
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
        if (web._controllers)
        {
            const SocketController &sockets = web._controllers->sockets();
            auto guard = sockets.lockGuard();
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
                    web.appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Light #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
                const uint32_t node_id = device.node_id;
                StackUnitSnapshot::State snapshot{};
                if (!web.network()->stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
                {
                    const_cast<WebInterface &>(web).requestStackLights_(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    return;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                for (uint8_t k = 0; k < snapshot.light_count && k < StackUnitSnapshot::kSocketCount; ++k)
                {
                    StackUnitSnapshot::SocketItem it{};
                    if (!web.network()->stackIndexLightAt(node_id, k, it))
                        continue;
                    if (!it.enabled)
                        continue;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        web.appendJsonEscaped_(list, it.name);
                    else
                        list += String("Light #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                }
                list += "]";
                if (snapshot.lights_enabled > snapshot.light_count)
                    const_cast<WebInterface &>(web).requestStackLights_(node_id);
                append_node(String((unsigned long)node_id), list);
        });
        out += "}";
        return out;
    }

String WebInterfaceControllersDisplayHelper::displayMeteoOptionsJson_(const WebInterface &web) {
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
        if (web._controllers)
        {
            const MeteoController &meteo = web._controllers->meteo();
            auto guard = meteo.lockGuard();
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
                    web.appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Sensor #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
            append_node(String((unsigned long)device.node_id), "[]");
        });
        out += "}";
        return out;
    }

String WebInterfaceControllersDisplayHelper::displayThermoOptionsJson_(const WebInterface &web) {
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
        if (web._controllers)
        {
            const ThermoController &thermo = web._controllers->thermo();
            auto guard = thermo.lockGuard();
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
                    web.appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Thermo #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
            append_node(String((unsigned long)device.node_id), "[]");
        });
        out += "}";
        return out;
    }

String WebInterfaceControllersDisplayHelper::displayTankOptionsJson_(const WebInterface &web) {
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
        if (web._controllers)
        {
            const TankController &tanks = web._controllers->tanks();
            auto guard = tanks.lockGuard();
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
                    web.appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Tank #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
            append_node(String((unsigned long)device.node_id), "[]");
        });
        out += "}";
        return out;
    }

String WebInterfaceControllersDisplayHelper::displaySepticOptionsJson_(const WebInterface &web) {
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
        if (web._controllers)
        {
            const SepticController &septic = web._controllers->septic();
            auto guard = septic.lockGuard();
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
                    web.appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Septic #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
            append_node(String((unsigned long)device.node_id), "[]");
        });
        out += "}";
        return out;
    }

String WebInterfaceControllersDisplayHelper::displayAvrOptionsJson_(const WebInterface &web) {
        String out;
        out.reserve(256);
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
    
        append_node("0", "[{\"v\":1,\"l\":\"AVR\"}]");
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
            append_node(String((unsigned long)device.node_id), "[]");
        });
        out += "}";
        return out;
    }

String WebInterfaceControllersDisplayHelper::displayLeakOptionsJson_(const WebInterface &web) {
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
        if (web._controllers)
        {
            const LeakController &leak = web._controllers->leak();
            auto guard = leak.lockGuard();
            for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            {
                const auto *cfg = leak.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg->id);
                local += ",\"l\":\"";
                if (cfg->name.length())
                    web.appendJsonEscaped_(local, cfg->name);
                else
                    local += String("Leak #") + String((unsigned)cfg->id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
            append_node(String((unsigned long)device.node_id), "[]");
        });
        out += "}";
        return out;
    }

String WebInterface::displaySlotsHtml_() const {
        return WebInterfaceControllersDisplayHelper::displaySlotsHtml_(*this);
    }

String WebInterface::displayDeviceOptionsJson_() const {
        return WebInterfaceControllersDisplayHelper::displayDeviceOptionsJson_(*this);
    }

String WebInterface::displaySocketOptionsJson_() const {
        return WebInterfaceControllersDisplayHelper::displaySocketOptionsJson_(*this);
    }

String WebInterface::displayLightOptionsJson_() const {
        return WebInterfaceControllersDisplayHelper::displayLightOptionsJson_(*this);
    }

String WebInterface::displayMeteoOptionsJson_() const {
        return WebInterfaceControllersDisplayHelper::displayMeteoOptionsJson_(*this);
    }

String WebInterface::displayThermoOptionsJson_() const {
        return WebInterfaceControllersDisplayHelper::displayThermoOptionsJson_(*this);
    }

String WebInterface::displayTankOptionsJson_() const {
        return WebInterfaceControllersDisplayHelper::displayTankOptionsJson_(*this);
    }

String WebInterface::displaySepticOptionsJson_() const {
        return WebInterfaceControllersDisplayHelper::displaySepticOptionsJson_(*this);
    }

String WebInterface::displayAvrOptionsJson_() const {
        return WebInterfaceControllersDisplayHelper::displayAvrOptionsJson_(*this);
    }

String WebInterface::displayLeakOptionsJson_() const {
        return WebInterfaceControllersDisplayHelper::displayLeakOptionsJson_(*this);
    }


