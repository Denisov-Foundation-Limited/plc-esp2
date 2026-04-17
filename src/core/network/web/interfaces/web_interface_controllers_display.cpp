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
void loadLocalSocketOptionItems_(const SocketController &sockets, WebInterface::ScratchBuffer &scratch,
                                 size_t count, bool lights)
{
    if (count == 0)
        return;
    auto guard = sockets.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        if (lights)
            scratch.light_valid[i] = false;
        else
            scratch.socket_valid[i] = false;
        const auto *cfg = lights ? sockets.lightConfigByIndex(i) : sockets.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        if (lights)
        {
            scratch.light_valid[i] = true;
            scratch.light_cfg[i] = *static_cast<const SocketController::LightConfig *>(cfg);
        }
        else
        {
            scratch.socket_valid[i] = true;
            scratch.socket_cfg[i] = *static_cast<const SocketController::SocketConfig *>(cfg);
        }
    }
}

void loadLocalMeteoOptionItems_(const MeteoController &meteo, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = meteo.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        scratch.meteo_valid[i] = false;
        const auto *cfg = meteo.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        scratch.meteo_valid[i] = true;
        scratch.meteo_cfg[i] = *cfg;
    }
}

void loadLocalThermoOptionItems_(const ThermoController &thermo, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = thermo.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        scratch.thermo_valid[i] = false;
        const auto *cfg = thermo.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        scratch.thermo_valid[i] = true;
        scratch.thermo_cfg[i] = *cfg;
    }
}

void loadLocalTankOptionItems_(const TankController &tanks, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = tanks.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        scratch.tank_valid[i] = false;
        const auto *cfg = tanks.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        scratch.tank_valid[i] = true;
        scratch.tank_cfg[i] = *cfg;
    }
}

void loadLocalSepticOptionItems_(const SepticController &septic, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = septic.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        scratch.septic_valid[i] = false;
        const auto *cfg = septic.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        scratch.septic_valid[i] = true;
        scratch.septic_cfg[i] = *cfg;
    }
}

void loadLocalLeakOptionItems_(const LeakController &leak, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = leak.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        scratch.leak_valid[i] = false;
        const auto *cfg = leak.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        scratch.leak_valid[i] = true;
        scratch.leak_cfg[i] = *cfg;
    }
}

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

bool waitForDisplayStackMeteoCache_(WebInterface &web, uint32_t node_id, uint32_t timeout_ms = 700u)
{
    if (!web.network() || node_id == 0)
        return false;
    const uint32_t started_ms = millis();
    while ((uint32_t)(millis() - started_ms) < timeout_ms)
    {
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        if (web.network()->stackIndexState(node_id, snapshot) && web.network()->stackIndexCacheState(node_id, cache) &&
            cache.meteo_count > 0)
            return true;
        delay(25);
    }
    return false;
}

bool waitForDisplayStackThermoCache_(WebInterface &web, uint32_t node_id, uint32_t timeout_ms = 700u)
{
    if (!web.network() || node_id == 0)
        return false;
    const uint32_t started_ms = millis();
    while ((uint32_t)(millis() - started_ms) < timeout_ms)
    {
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        if (web.network()->stackIndexState(node_id, snapshot) && web.network()->stackIndexCacheState(node_id, cache) &&
            cache.thermo_count > 0)
            return true;
        delay(25);
    }
    return false;
}

bool waitForDisplayStackTankCache_(WebInterface &web, uint32_t node_id, uint32_t timeout_ms = 700u)
{
    if (!web.network() || node_id == 0)
        return false;
    const uint32_t started_ms = millis();
    while ((uint32_t)(millis() - started_ms) < timeout_ms)
    {
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        if (web.network()->stackIndexState(node_id, snapshot) && web.network()->stackIndexCacheState(node_id, cache) &&
            cache.tank_count > 0)
            return true;
        delay(25);
    }
    return false;
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
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "{}";
            const SocketController &sockets = web._controllers->sockets();
            loadLocalSocketOptionItems_(sockets, *scratch, SocketController::kSocketCount, false);
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                if (!scratch->socket_valid[i])
                    continue;
                const auto &cfg = scratch->socket_cfg[i];
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg.id);
                local += ",\"l\":\"";
                if (cfg.name.length())
                    web.appendJsonEscaped_(local, cfg.name);
                else
                    local += String("Socket #") + String((unsigned)cfg.id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
                const uint32_t node_id = device.node_id;
                StackUnitSnapshot::State snapshot{};
                StackUnitSnapshot::CacheState cache{};
                if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                    snapshot.updated_ms == 0)
                {
                    const_cast<WebInterface &>(web).requestStackSockets_(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    return;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                web.network()->forEachStackSocket(node_id, cache.socket_count, [&](uint8_t, const StackUnitSnapshot::SocketItem &it) {
                    if (!it.enabled)
                        return;
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
                });
                list += "]";
                if (snapshot.sockets_enabled > cache.socket_count)
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
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "{}";
            const SocketController &sockets = web._controllers->sockets();
            loadLocalSocketOptionItems_(sockets, *scratch, SocketController::kLightCount, true);
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                if (!scratch->light_valid[i])
                    continue;
                const auto &cfg = scratch->light_cfg[i];
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg.id);
                local += ",\"l\":\"";
                if (cfg.name.length())
                    web.appendJsonEscaped_(local, cfg.name);
                else
                    local += String("Light #") + String((unsigned)cfg.id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
                const uint32_t node_id = device.node_id;
                StackUnitSnapshot::State snapshot{};
                StackUnitSnapshot::CacheState cache{};
                if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                    snapshot.updated_ms == 0)
                {
                    const_cast<WebInterface &>(web).requestStackLights_(node_id);
                    append_node(String((unsigned long)node_id), "[]");
                    return;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                web.network()->forEachStackLight(node_id, cache.light_count, [&](uint8_t, const StackUnitSnapshot::SocketItem &it) {
                    if (!it.enabled)
                        return;
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
                });
                list += "]";
                if (snapshot.lights_enabled > cache.light_count)
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
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "{}";
            const MeteoController &meteo = web._controllers->meteo();
            loadLocalMeteoOptionItems_(meteo, *scratch, MeteoController::kSensorCount);
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                if (!scratch->meteo_valid[i])
                    continue;
                const auto &cfg = scratch->meteo_cfg[i];
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg.id);
                local += ",\"l\":\"";
                if (cfg.name.length())
                    web.appendJsonEscaped_(local, cfg.name);
                else
                    local += String("Sensor #") + String((unsigned)cfg.id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
                const uint32_t node_id = device.node_id;
                StackUnitSnapshot::State snapshot{};
                StackUnitSnapshot::CacheState cache{};
                if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                    snapshot.updated_ms == 0)
                {
                    const_cast<WebInterface &>(web).requestStackMeteo_(node_id);
                    waitForDisplayStackMeteoCache_(const_cast<WebInterface &>(web), node_id);
                    if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                        snapshot.updated_ms == 0)
                    {
                        append_node(String((unsigned long)node_id), "[]");
                        return;
                    }
                }
                if (cache.meteo_count == 0 && snapshot.meteo_enabled > 0)
                {
                    const_cast<WebInterface &>(web).requestStackMeteo_(node_id);
                    waitForDisplayStackMeteoCache_(const_cast<WebInterface &>(web), node_id);
                    web.network()->stackIndexState(node_id, snapshot);
                    web.network()->stackIndexCacheState(node_id, cache);
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                web.network()->forEachStackMeteo(node_id, cache.meteo_count, [&](uint8_t, const StackUnitSnapshot::MeteoItem &it) {
                    if (!it.enabled)
                        return;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        web.appendJsonEscaped_(list, it.name);
                    else
                        list += String("Sensor #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                });
                list += "]";
                if (snapshot.meteo_enabled > cache.meteo_count)
                    const_cast<WebInterface &>(web).requestStackMeteo_(node_id);
                append_node(String((unsigned long)node_id), list);
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
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "{}";
            const ThermoController &thermo = web._controllers->thermo();
            loadLocalThermoOptionItems_(thermo, *scratch, ThermoController::kDeviceCount);
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                if (!scratch->thermo_valid[i])
                    continue;
                const auto &cfg = scratch->thermo_cfg[i];
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg.id);
                local += ",\"l\":\"";
                if (cfg.name.length())
                    web.appendJsonEscaped_(local, cfg.name);
                else
                    local += String("Thermo #") + String((unsigned)cfg.id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
                const uint32_t node_id = device.node_id;
                StackUnitSnapshot::State snapshot{};
                StackUnitSnapshot::CacheState cache{};
                if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                    snapshot.updated_ms == 0)
                {
                    const_cast<WebInterface &>(web).requestStackThermo_(node_id);
                    waitForDisplayStackThermoCache_(const_cast<WebInterface &>(web), node_id);
                    web.network()->stackIndexState(node_id, snapshot);
                    web.network()->stackIndexCacheState(node_id, cache);
                }
                if (cache.thermo_count == 0 && snapshot.thermo_enabled > 0)
                {
                    const_cast<WebInterface &>(web).requestStackThermo_(node_id);
                    waitForDisplayStackThermoCache_(const_cast<WebInterface &>(web), node_id);
                    web.network()->stackIndexState(node_id, snapshot);
                    web.network()->stackIndexCacheState(node_id, cache);
                }
                if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                    snapshot.updated_ms == 0)
                {
                    append_node(String((unsigned long)node_id), "[]");
                    return;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                web.network()->forEachStackThermo(node_id, cache.thermo_count, [&](uint8_t, const StackUnitSnapshot::ThermoItem &it) {
                    if (!it.enabled)
                        return;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        web.appendJsonEscaped_(list, it.name);
                    else
                        list += String("Thermo #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                });
                list += "]";
                if (snapshot.thermo_enabled > cache.thermo_count)
                    const_cast<WebInterface &>(web).requestStackThermo_(node_id);
                append_node(String((unsigned long)node_id), list);
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
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "{}";
            const TankController &tanks = web._controllers->tanks();
            loadLocalTankOptionItems_(tanks, *scratch, TankController::kTankCount);
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                if (!scratch->tank_valid[i])
                    continue;
                const auto &cfg = scratch->tank_cfg[i];
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg.id);
                local += ",\"l\":\"";
                if (cfg.name.length())
                    web.appendJsonEscaped_(local, cfg.name);
                else
                    local += String("Tank #") + String((unsigned)cfg.id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
                const uint32_t node_id = device.node_id;
                StackUnitSnapshot::State snapshot{};
                StackUnitSnapshot::CacheState cache{};
                if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                    snapshot.updated_ms == 0)
                {
                    const_cast<WebInterface &>(web).requestStackTanks_(node_id);
                    waitForDisplayStackTankCache_(const_cast<WebInterface &>(web), node_id);
                    web.network()->stackIndexState(node_id, snapshot);
                    web.network()->stackIndexCacheState(node_id, cache);
                }
                if (cache.tank_count == 0 && snapshot.tanks_enabled > 0)
                {
                    const_cast<WebInterface &>(web).requestStackTanks_(node_id);
                    waitForDisplayStackTankCache_(const_cast<WebInterface &>(web), node_id);
                    web.network()->stackIndexState(node_id, snapshot);
                    web.network()->stackIndexCacheState(node_id, cache);
                }
                if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                    snapshot.updated_ms == 0)
                {
                    append_node(String((unsigned long)node_id), "[]");
                    return;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                web.network()->forEachStackTank(node_id, cache.tank_count, [&](uint8_t, const StackUnitSnapshot::TankItem &it) {
                    if (!it.enabled)
                        return;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        web.appendJsonEscaped_(list, it.name);
                    else
                        list += String("Tank #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                });
                list += "]";
                if (snapshot.tanks_enabled > cache.tank_count)
                    const_cast<WebInterface &>(web).requestStackTanks_(node_id);
                append_node(String((unsigned long)node_id), list);
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
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "{}";
            const SepticController &septic = web._controllers->septic();
            loadLocalSepticOptionItems_(septic, *scratch, SepticController::kSepticCount);
            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                if (!scratch->septic_valid[i])
                    continue;
                const auto &cfg = scratch->septic_cfg[i];
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg.id);
                local += ",\"l\":\"";
                if (cfg.name.length())
                    web.appendJsonEscaped_(local, cfg.name);
                else
                    local += String("Septic #") + String((unsigned)cfg.id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
            StackUnitSnapshot::State snapshot{};
            if (!web.network()->stackIndexState(device.node_id, snapshot) || snapshot.updated_ms == 0 || snapshot.septic_enabled == 0)
            {
                append_node(String((unsigned long)device.node_id), "[]");
                return;
            }
            append_node(String((unsigned long)device.node_id), "[{\"v\":1,\"l\":\"Septic\"}]");
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
            StackUnitSnapshot::State snapshot{};
            if (!web.network()->stackIndexState(device.node_id, snapshot) || snapshot.updated_ms == 0 || !snapshot.avr_enabled)
            {
                append_node(String((unsigned long)device.node_id), "[]");
                return;
            }
            append_node(String((unsigned long)device.node_id), "[{\"v\":1,\"l\":\"AVR\"}]");
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
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "{}";
            const LeakController &leak = web._controllers->leak();
            loadLocalLeakOptionItems_(leak, *scratch, LeakController::kZoneCount);
            for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            {
                if (!scratch->leak_valid[i])
                    continue;
                const auto &cfg = scratch->leak_cfg[i];
                if (!first)
                    local += ",";
                local += "{\"v\":";
                local += String((unsigned)cfg.id);
                local += ",\"l\":\"";
                if (cfg.name.length())
                    web.appendJsonEscaped_(local, cfg.name);
                else
                    local += String("Leak #") + String((unsigned)cfg.id);
                local += "\"}";
                first = false;
            }
        }
        local += "]";
        append_node("0", local);
    
        forEachDisplayStackDevice_(web, [&](const StackDeviceRegistry::DeviceInfo &device) {
                const uint32_t node_id = device.node_id;
                StackUnitSnapshot::State snapshot{};
                StackUnitSnapshot::CacheState cache{};
                if (!web.network()->stackIndexState(node_id, snapshot) || !web.network()->stackIndexCacheState(node_id, cache) ||
                    snapshot.updated_ms == 0)
                {
                    const uint32_t now = millis();
                    if (web.network()->prepareStackPageRequest(StackUnitSnapshot::PageKind::Leak, node_id, now, 0, 4000u))
                    {
                        DynamicJsonDocument req(64);
                        req["offset"] = 0;
                        req["limit"] = StackUnitSnapshot::kPageSize;
                        web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "leak",
                                                                        "snapshot_req", &req, true);
                    }
                    append_node(String((unsigned long)node_id), "[]");
                    return;
                }
                String list;
                list.reserve(256);
                list += "[";
                bool first_item = true;
                web.network()->forEachStackLeak(node_id, cache.leak_count, [&](uint8_t, const StackUnitSnapshot::LeakItem &it) {
                    if (!it.enabled)
                        return;
                    if (!first_item)
                        list += ",";
                    list += "{\"v\":";
                    list += String((unsigned)it.id);
                    list += ",\"l\":\"";
                    if (it.name[0])
                        web.appendJsonEscaped_(list, it.name);
                    else
                        list += String("Leak #") + String((unsigned)it.id);
                    list += "\"}";
                    first_item = false;
                });
                list += "]";
                if (snapshot.leak_enabled > cache.leak_count)
                {
                    const uint32_t now = millis();
                    if (web.network()->prepareStackPageRequest(StackUnitSnapshot::PageKind::Leak, node_id, now,
                                                               cache.leak_count, 4000u))
                    {
                        DynamicJsonDocument req(64);
                        req["offset"] = cache.leak_count;
                        req["limit"] = StackUnitSnapshot::kPageSize;
                        web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "leak",
                                                                        "snapshot_req", &req, true);
                    }
                }
                append_node(String((unsigned long)node_id), list);
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


