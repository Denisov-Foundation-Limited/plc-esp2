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

#include "core/network/stack/stack_unit_snapshot.hpp"

#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"

StackUnitSnapshot::StackUnitSnapshot() = default;

StackUnitSnapshot::~StackUnitSnapshot()
{
    releaseStorage_();
}

bool StackUnitSnapshot::prepareRequest(uint32_t node_id, uint32_t now_ms, uint32_t fresh_ms, uint32_t pending_ms)
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return false;
    const bool fresh_plc = entry->state.has_plc && (uint32_t)(now_ms - entry->state.updated_ms) < fresh_ms;
    const bool fresh_rtc = entry->state.has_rtc && (uint32_t)(now_ms - entry->state.updated_ms) < fresh_ms;
    if (entry->request.pending && (uint32_t)(now_ms - entry->request.started_ms) < pending_ms)
        return false;
    if (fresh_plc && fresh_rtc)
        return false;
    entry->used = true;
    entry->state.node_id = node_id;
    entry->request.pending = true;
    entry->request.started_ms = now_ms;
    return true;
}

bool StackUnitSnapshot::state(uint32_t node_id, State &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    if (!entry)
        return false;
    copyState_(out, entry->state);
    return true;
}

bool StackUnitSnapshot::cacheState(uint32_t node_id, CacheState &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    if (!entry)
        return false;
    copyCacheState_(out, entry->cache);
    return true;
}

bool StackUnitSnapshot::requestState(uint32_t node_id, RequestState &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    if (!entry)
        return false;
    copyRequestState_(out, entry->request);
    return true;
}

bool StackUnitSnapshot::pageRequestState(uint32_t node_id, PageKind kind, PageRequestState &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    if (!entry)
        return false;
    copyPageRequestState_(out, pageRequestState_(*entry, kind));
    return true;
}

bool StackUnitSnapshot::socketById(uint32_t node_id, uint8_t id, SocketItem &out) const
{
    if (node_id == 0 || id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemById_<SocketItem, kSocketCount>(entry->sockets.items, entry->cache.socket_count, id, out);
}

bool StackUnitSnapshot::socketAt(uint32_t node_id, uint8_t index, SocketItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_<SocketItem, kSocketCount>(entry->sockets.items, entry->cache.socket_count, index, out);
}

bool StackUnitSnapshot::socketsPage(uint32_t node_id, uint8_t offset, SocketItem *out, uint8_t capacity, uint8_t &out_count) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyPage_<SocketItem, kSocketCount>(entry->sockets.items, entry->cache.socket_count, offset, out, capacity, out_count);
}

bool StackUnitSnapshot::lightById(uint32_t node_id, uint8_t id, SocketItem &out) const
{
    if (node_id == 0 || id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemById_<SocketItem, kSocketCount>(entry->lights.items, entry->cache.light_count, id, out);
}

bool StackUnitSnapshot::lightAt(uint32_t node_id, uint8_t index, SocketItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_<SocketItem, kSocketCount>(entry->lights.items, entry->cache.light_count, index, out);
}

bool StackUnitSnapshot::lightsPage(uint32_t node_id, uint8_t offset, SocketItem *out, uint8_t capacity, uint8_t &out_count) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyPage_<SocketItem, kSocketCount>(entry->lights.items, entry->cache.light_count, offset, out, capacity, out_count);
}

bool StackUnitSnapshot::meteoById(uint32_t node_id, uint8_t id, MeteoItem &out) const
{
    if (node_id == 0 || id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemById_<MeteoItem, kMeteoCount>(entry->meteo.items, entry->cache.meteo_count, id, out);
}

bool StackUnitSnapshot::meteoAt(uint32_t node_id, uint8_t index, MeteoItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_<MeteoItem, kMeteoCount>(entry->meteo.items, entry->cache.meteo_count, index, out);
}

bool StackUnitSnapshot::meteoPage(uint32_t node_id, uint8_t offset, MeteoItem *out, uint8_t capacity, uint8_t &out_count) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyPage_<MeteoItem, kMeteoCount>(entry->meteo.items, entry->cache.meteo_count, offset, out, capacity, out_count);
}

bool StackUnitSnapshot::thermoById(uint32_t node_id, uint8_t id, ThermoItem &out) const
{
    if (node_id == 0 || id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemById_<ThermoItem, kThermoCount>(entry->thermo.items, entry->cache.thermo_count, id, out);
}

bool StackUnitSnapshot::thermoAt(uint32_t node_id, uint8_t index, ThermoItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_<ThermoItem, kThermoCount>(entry->thermo.items, entry->cache.thermo_count, index, out);
}

bool StackUnitSnapshot::thermoPage(uint32_t node_id, uint8_t offset, ThermoItem *out, uint8_t capacity, uint8_t &out_count) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyPage_<ThermoItem, kThermoCount>(entry->thermo.items, entry->cache.thermo_count, offset, out, capacity, out_count);
}

bool StackUnitSnapshot::tankById(uint32_t node_id, uint8_t id, TankItem &out) const
{
    if (node_id == 0 || id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemById_<TankItem, kTankCount>(entry->tanks.items, entry->cache.tank_count, id, out);
}

bool StackUnitSnapshot::tankAt(uint32_t node_id, uint8_t index, TankItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_<TankItem, kTankCount>(entry->tanks.items, entry->cache.tank_count, index, out);
}

bool StackUnitSnapshot::tanksPage(uint32_t node_id, uint8_t offset, TankItem *out, uint8_t capacity, uint8_t &out_count) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyPage_<TankItem, kTankCount>(entry->tanks.items, entry->cache.tank_count, offset, out, capacity, out_count);
}

bool StackUnitSnapshot::wateringById(uint32_t node_id, uint8_t id, WateringItem &out) const
{
    if (node_id == 0 || id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemById_<WateringItem, kWateringCount>(entry->watering.items, entry->cache.watering_count, id, out);
}

bool StackUnitSnapshot::wateringAt(uint32_t node_id, uint8_t index, WateringItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_<WateringItem, kWateringCount>(entry->watering.items, entry->cache.watering_count, index, out);
}

bool StackUnitSnapshot::wateringPage(uint32_t node_id, uint8_t offset, WateringItem *out, uint8_t capacity,
                                     uint8_t &out_count) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyPage_<WateringItem, kWateringCount>(entry->watering.items, entry->cache.watering_count, offset,
                                                            out, capacity, out_count);
}

bool StackUnitSnapshot::ruleAt(uint32_t node_id, uint8_t index, RuleItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_<RuleItem, kRuleCount>(entry->rules.items, entry->cache.rule_count, index, out);
}

bool StackUnitSnapshot::rulesPage(uint32_t node_id, uint8_t offset, RuleItem *out, uint8_t capacity, uint8_t &out_count) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyPage_<RuleItem, kRuleCount>(entry->rules.items, entry->cache.rule_count, offset, out, capacity,
                                                    out_count);
}

bool StackUnitSnapshot::leakById(uint32_t node_id, uint8_t id, LeakItem &out) const
{
    if (node_id == 0 || id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemById_<LeakItem, kLeakCount>(entry->leaks.items, entry->cache.leak_count, id, out);
}

bool StackUnitSnapshot::leakAt(uint32_t node_id, uint8_t index, LeakItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_<LeakItem, kLeakCount>(entry->leaks.items, entry->cache.leak_count, index, out);
}

bool StackUnitSnapshot::leaksPage(uint32_t node_id, uint8_t offset, LeakItem *out, uint8_t capacity, uint8_t &out_count) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyPage_<LeakItem, kLeakCount>(entry->leaks.items, entry->cache.leak_count, offset, out, capacity, out_count);
}

bool StackUnitSnapshot::prepareSocketsPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms)
{
    return preparePageRequest_(node_id, PageKind::Sockets, now_ms, offset, pending_ms);
}

bool StackUnitSnapshot::prepareLightsPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms)
{
    return preparePageRequest_(node_id, PageKind::Lights, now_ms, offset, pending_ms);
}

bool StackUnitSnapshot::prepareMeteoPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms)
{
    return preparePageRequest_(node_id, PageKind::Meteo, now_ms, offset, pending_ms);
}

bool StackUnitSnapshot::prepareThermoPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms)
{
    return preparePageRequest_(node_id, PageKind::Thermo, now_ms, offset, pending_ms);
}

bool StackUnitSnapshot::prepareTanksPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms)
{
    return preparePageRequest_(node_id, PageKind::Tanks, now_ms, offset, pending_ms);
}

bool StackUnitSnapshot::prepareWateringPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset,
                                                   uint32_t pending_ms)
{
    return preparePageRequest_(node_id, PageKind::Watering, now_ms, offset, pending_ms);
}

bool StackUnitSnapshot::prepareLeakPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms)
{
    return preparePageRequest_(node_id, PageKind::Leak, now_ms, offset, pending_ms);
}

void StackUnitSnapshot::completeSocketsPageRequest(uint32_t node_id, uint16_t offset)
{
    completePageRequest_(node_id, PageKind::Sockets, offset);
}

void StackUnitSnapshot::completeLightsPageRequest(uint32_t node_id, uint16_t offset)
{
    completePageRequest_(node_id, PageKind::Lights, offset);
}

void StackUnitSnapshot::completeMeteoPageRequest(uint32_t node_id, uint16_t offset)
{
    completePageRequest_(node_id, PageKind::Meteo, offset);
}

void StackUnitSnapshot::completeThermoPageRequest(uint32_t node_id, uint16_t offset)
{
    completePageRequest_(node_id, PageKind::Thermo, offset);
}

void StackUnitSnapshot::completeTanksPageRequest(uint32_t node_id, uint16_t offset)
{
    completePageRequest_(node_id, PageKind::Tanks, offset);
}

void StackUnitSnapshot::completeWateringPageRequest(uint32_t node_id, uint16_t offset)
{
    completePageRequest_(node_id, PageKind::Watering, offset);
}

void StackUnitSnapshot::completeLeakPageRequest(uint32_t node_id, uint16_t offset)
{
    completePageRequest_(node_id, PageKind::Leak, offset);
}

void StackUnitSnapshot::clearSocketsPageRequest(uint32_t node_id)
{
    clearPageRequest_(node_id, PageKind::Sockets);
}

void StackUnitSnapshot::clearLightsPageRequest(uint32_t node_id)
{
    clearPageRequest_(node_id, PageKind::Lights);
}

void StackUnitSnapshot::clearMeteoPageRequest(uint32_t node_id)
{
    clearPageRequest_(node_id, PageKind::Meteo);
}

void StackUnitSnapshot::clearThermoPageRequest(uint32_t node_id)
{
    clearPageRequest_(node_id, PageKind::Thermo);
}

void StackUnitSnapshot::clearTanksPageRequest(uint32_t node_id)
{
    clearPageRequest_(node_id, PageKind::Tanks);
}

void StackUnitSnapshot::clearWateringPageRequest(uint32_t node_id)
{
    clearPageRequest_(node_id, PageKind::Watering);
}

void StackUnitSnapshot::clearLeakPageRequest(uint32_t node_id)
{
    clearPageRequest_(node_id, PageKind::Leak);
}

void StackUnitSnapshot::clearPending(uint32_t node_id)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = findEntry_(node_id);
    if (!entry)
        return;
    entry->request.pending = false;
    entry->request.started_ms = 0;
}

void StackUnitSnapshot::applySystemState(uint32_t node_id, const State &state)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    mergeSystemState_(entry->state, state);
    entry->state.updated_ms = state.updated_ms ? state.updated_ms : millis();
}

void StackUnitSnapshot::applyControllerSummary(uint32_t node_id, const State &state)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    mergeControllerSummary_(entry->state, state);
    entry->state.updated_ms = state.updated_ms ? state.updated_ms : millis();
}

void StackUnitSnapshot::applySocketsPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t on_total,
                                         const SocketItem *items, uint8_t item_count, uint32_t updated_ms)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    entry->state.updated_ms = updated_ms ? updated_ms : millis();
    entry->state.sockets_enabled = enabled_total;
    entry->state.sockets_on = on_total;
    entry->cache.socket_count = applyPage_(entry->sockets, offset, items, item_count);
}

void StackUnitSnapshot::applyLightsPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t on_total,
                                        const SocketItem *items, uint8_t item_count, uint32_t updated_ms)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    entry->state.updated_ms = updated_ms ? updated_ms : millis();
    entry->state.lights_enabled = enabled_total;
    entry->state.lights_on = on_total;
    entry->cache.light_count = applyPage_(entry->lights, offset, items, item_count);
}

void StackUnitSnapshot::applyMeteoPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t ok_total,
                                       const MeteoItem *items, uint8_t item_count, uint32_t updated_ms)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    entry->state.updated_ms = updated_ms ? updated_ms : millis();
    entry->state.meteo_enabled = enabled_total;
    entry->state.meteo_ok = ok_total;
    entry->cache.meteo_count = applyPage_(entry->meteo, offset, items, item_count);
}

void StackUnitSnapshot::applyThermoPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t active_total,
                                        const ThermoItem *items, uint8_t item_count, uint32_t updated_ms)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    entry->state.updated_ms = updated_ms ? updated_ms : millis();
    entry->state.thermo_enabled = enabled_total;
    entry->state.thermo_active = active_total;
    entry->cache.thermo_count = applyPage_(entry->thermo, offset, items, item_count);
}

void StackUnitSnapshot::applyTanksPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t alert_total,
                                       const TankItem *items, uint8_t item_count, uint32_t updated_ms)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    entry->state.updated_ms = updated_ms ? updated_ms : millis();
    entry->state.tanks_enabled = enabled_total;
    entry->state.tanks_alert = alert_total;
    entry->cache.tank_count = applyPage_(entry->tanks, offset, items, item_count);
}

void StackUnitSnapshot::applyWateringPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total,
                                          uint16_t active_total, const WateringItem *items, uint8_t item_count,
                                          uint32_t updated_ms)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    entry->state.updated_ms = updated_ms ? updated_ms : millis();
    entry->state.watering_enabled = enabled_total;
    entry->state.watering_active = active_total;
    entry->cache.watering_count = applyPage_(entry->watering, offset, items, item_count);
}

void StackUnitSnapshot::applyRulesSummary(uint32_t node_id, uint16_t enabled_total, const RuleItem *items, uint8_t item_count,
                                          uint32_t updated_ms)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    entry->state.updated_ms = updated_ms ? updated_ms : millis();
    entry->state.rules_enabled = enabled_total;
    clearPageCache_(entry->rules);
    entry->cache.rule_count = 0;
    if (!items || item_count == 0)
        return;
    const uint8_t safe_count = clampCount_(item_count, kRuleCount);
    memcpy(entry->rules.items, items, sizeof(RuleItem) * safe_count);
    entry->cache.rule_count = safe_count;
}

void StackUnitSnapshot::applyLeaksPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t alert_total,
                                       const LeakItem *items, uint8_t item_count, uint32_t updated_ms)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return;
    entry->used = true;
    entry->state.node_id = node_id;
    entry->state.updated_ms = updated_ms ? updated_ms : millis();
    entry->state.leak_enabled = enabled_total;
    entry->state.leak_alert = alert_total;
    entry->cache.leak_count = applyPage_(entry->leaks, offset, items, item_count);
}

void StackUnitSnapshot::invalidate(uint32_t node_id)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = findEntry_(node_id);
    if (entry)
        memset(entry, 0, sizeof(*entry));
}

StackUnitSnapshot::Entry *StackUnitSnapshot::findEntry_(uint32_t node_id)
{
    if (node_id == 0 || !_entries)
        return nullptr;
    for (size_t i = 0; i < StackDeviceRegistry::kMaxDevices; ++i)
    {
        auto &entry = _entries[i];
        if (entry.used && entry.state.node_id == node_id)
            return &entry;
    }
    return nullptr;
}

const StackUnitSnapshot::Entry *StackUnitSnapshot::findEntry_(uint32_t node_id) const
{
    if (node_id == 0 || !_entries)
        return nullptr;
    for (size_t i = 0; i < StackDeviceRegistry::kMaxDevices; ++i)
    {
        const auto &entry = _entries[i];
        if (entry.used && entry.state.node_id == node_id)
            return &entry;
    }
    return nullptr;
}

StackUnitSnapshot::Entry *StackUnitSnapshot::allocEntry_(uint32_t node_id)
{
    if (Entry *entry = findEntry_(node_id))
        return entry;
    if (!_entries)
        return nullptr;
    for (size_t i = 0; i < StackDeviceRegistry::kMaxDevices; ++i)
    {
        auto &entry = _entries[i];
        if (!entry.used)
        {
            memset(&entry, 0, sizeof(entry));
            entry.used = true;
            entry.state.node_id = node_id;
            return &entry;
        }
    }
    return nullptr;
}

bool StackUnitSnapshot::ensureStorage_() const
{
    if (_entries)
        return true;
    const size_t bytes = sizeof(Entry) * StackDeviceRegistry::kMaxDevices;
    void *mem = nullptr;
    if (psramFound())
        mem = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mem)
        mem = malloc(bytes);
    if (!mem)
        return false;
    memset(mem, 0, bytes);
    _entries = static_cast<Entry *>(mem);
    return true;
}

void StackUnitSnapshot::releaseStorage_()
{
    const auto guard = _lock.guard();
    if (!_entries)
        return;
    free(_entries);
    _entries = nullptr;
}

void StackUnitSnapshot::copyState_(State &dst, const State &src)
{
    memcpy(&dst, &src, sizeof(dst));
}

void StackUnitSnapshot::copyCacheState_(CacheState &dst, const CacheState &src)
{
    memcpy(&dst, &src, sizeof(dst));
}

void StackUnitSnapshot::copyRequestState_(RequestState &dst, const RequestState &src)
{
    dst.started_ms = src.started_ms;
    dst.pending = src.pending;
}

void StackUnitSnapshot::copyPageRequestState_(PageRequestState &dst, const PageRequestState &src)
{
    dst.started_ms = src.started_ms;
    dst.offset = src.offset;
    dst.pending = src.pending;
}

template <typename ItemT, size_t N>
bool StackUnitSnapshot::copyItemById_(const ItemT *items, uint8_t count, uint8_t id, ItemT &out)
{
    if (!items || id == 0)
        return false;
    const uint8_t limit = clampCount_(count, N);
    for (uint8_t i = 0; i < limit; ++i)
    {
        if (items[i].id != id)
            continue;
        out = items[i];
        return true;
    }
    return false;
}

template <typename ItemT, size_t N>
bool StackUnitSnapshot::copyItemAt_(const ItemT *items, uint8_t count, uint8_t index, ItemT &out)
{
    const uint8_t limit = clampCount_(count, N);
    if (!items || index >= limit)
        return false;
    out = items[index];
    return true;
}

template <typename ItemT, size_t N>
bool StackUnitSnapshot::copyPage_(const ItemT *items, uint8_t count, uint8_t offset, ItemT *out, uint8_t capacity, uint8_t &out_count)
{
    out_count = 0;
    const uint8_t limit = clampCount_(count, N);
    if (!items || !out || capacity == 0 || offset >= limit)
        return false;
    const uint8_t available = (uint8_t)(limit - offset);
    out_count = (capacity < available) ? capacity : available;
    memcpy(out, items + offset, sizeof(ItemT) * out_count);
    return true;
}

void StackUnitSnapshot::mergeSystemState_(State &dst, const State &src)
{
    dst.node_id = src.node_id ? src.node_id : dst.node_id;
    dst.updated_ms = src.updated_ms;
    dst.has_plc = src.has_plc;
    dst.has_rtc = src.has_rtc;
    dst.has_wifi = src.has_wifi;
    dst.has_gsm = src.has_gsm;
    dst.rtc_temp_ok = src.rtc_temp_ok;
    dst.board_temp = src.board_temp;
    dst.fan_on = src.fan_on;
    dst.rtc_temp = src.rtc_temp;
    memcpy(dst.rtc_date, src.rtc_date, sizeof(dst.rtc_date));
    memcpy(dst.rtc_time, src.rtc_time, sizeof(dst.rtc_time));
    memcpy(dst.wifi_mode, src.wifi_mode, sizeof(dst.wifi_mode));
    memcpy(dst.wifi_ssid, src.wifi_ssid, sizeof(dst.wifi_ssid));
    memcpy(dst.wifi_ap_ssid, src.wifi_ap_ssid, sizeof(dst.wifi_ap_ssid));
    memcpy(dst.wifi_ip, src.wifi_ip, sizeof(dst.wifi_ip));
    memcpy(dst.wifi_mac, src.wifi_mac, sizeof(dst.wifi_mac));
    dst.gsm_enabled = src.gsm_enabled;
    dst.gsm_started = src.gsm_started;
    memcpy(dst.gsm_imei, src.gsm_imei, sizeof(dst.gsm_imei));
    memcpy(dst.gsm_imsi, src.gsm_imsi, sizeof(dst.gsm_imsi));
    memcpy(dst.gsm_operator, src.gsm_operator, sizeof(dst.gsm_operator));
    memcpy(dst.gsm_signal, src.gsm_signal, sizeof(dst.gsm_signal));
    memcpy(dst.gsm_reg_status, src.gsm_reg_status, sizeof(dst.gsm_reg_status));
    memcpy(dst.gsm_last_error, src.gsm_last_error, sizeof(dst.gsm_last_error));
    memcpy(dst.gsm_last_urc, src.gsm_last_urc, sizeof(dst.gsm_last_urc));
    memcpy(dst.gsm_last_call, src.gsm_last_call, sizeof(dst.gsm_last_call));
    memcpy(dst.gsm_last_ussd, src.gsm_last_ussd, sizeof(dst.gsm_last_ussd));
    dst.gsm_last_http_status = src.gsm_last_http_status;
    dst.gsm_last_http_len = src.gsm_last_http_len;
}

void StackUnitSnapshot::mergeControllerSummary_(State &dst, const State &src)
{
    dst.node_id = src.node_id ? src.node_id : dst.node_id;
    dst.updated_ms = src.updated_ms;
    dst.sockets_enabled = src.sockets_enabled;
    dst.sockets_on = src.sockets_on;
    dst.lights_enabled = src.lights_enabled;
    dst.lights_on = src.lights_on;
    dst.meteo_enabled = src.meteo_enabled;
    dst.meteo_ok = src.meteo_ok;
    dst.thermo_enabled = src.thermo_enabled;
    dst.thermo_active = src.thermo_active;
    dst.tanks_enabled = src.tanks_enabled;
    dst.tanks_alert = src.tanks_alert;
    dst.septic_enabled = src.septic_enabled;
    dst.septic_warning = src.septic_warning;
    dst.septic_alert = src.septic_alert;
    dst.septic_monitoring_on = src.septic_monitoring_on;
    dst.septic_relay_warning_on = src.septic_relay_warning_on;
    dst.septic_relay_alarm_on = src.septic_relay_alarm_on;
    dst.septic_group_id = src.septic_group_id;
    dst.septic_warning_port = src.septic_warning_port;
    dst.septic_alarm_port = src.septic_alarm_port;
    dst.septic_relay_warning_port = src.septic_relay_warning_port;
    dst.septic_relay_alarm_port = src.septic_relay_alarm_port;
    memcpy(dst.septic_name, src.septic_name, sizeof(dst.septic_name));
    dst.watering_enabled = src.watering_enabled;
    dst.watering_active = src.watering_active;
    dst.rules_enabled = src.rules_enabled;
    dst.security_sensors_enabled = src.security_sensors_enabled;
    dst.security_detected = src.security_detected;
    dst.security_detect_preview_count = src.security_detect_preview_count;
    memcpy(dst.security_detect_preview, src.security_detect_preview, sizeof(dst.security_detect_preview));
    dst.leak_enabled = src.leak_enabled;
    dst.leak_alert = src.leak_alert;
    dst.security_enabled = src.security_enabled;
    dst.security_armed = src.security_armed;
    dst.security_alarm = src.security_alarm;
    dst.ring_enabled = src.ring_enabled;
    dst.ring_on = src.ring_on;
    dst.avr_enabled = src.avr_enabled;
    dst.avr_main_ok = src.avr_main_ok;
    dst.avr_reserve_ok = src.avr_reserve_ok;
    dst.avr_fault = src.avr_fault;
    dst.avr_active_source = src.avr_active_source;
    if (src.ports_state_valid)
    {
        dst.ports_state_valid = true;
        memcpy(dst.relay_used_bits, src.relay_used_bits, sizeof(dst.relay_used_bits));
        memcpy(dst.dinput_used_bits, src.dinput_used_bits, sizeof(dst.dinput_used_bits));
        memcpy(dst.sensor_used_bits, src.sensor_used_bits, sizeof(dst.sensor_used_bits));
    }
}

uint8_t StackUnitSnapshot::clampCount_(uint16_t count, size_t max_count)
{
    return (count > max_count) ? (uint8_t)max_count : (uint8_t)count;
}

StackUnitSnapshot::PageRequestState &StackUnitSnapshot::pageRequestState_(Entry &entry, PageKind kind)
{
    switch (kind)
    {
        case PageKind::Sockets:
            return entry.sockets_request;
        case PageKind::Lights:
            return entry.lights_request;
        case PageKind::Meteo:
            return entry.meteo_request;
        case PageKind::Thermo:
            return entry.thermo_request;
        case PageKind::Tanks:
            return entry.tanks_request;
        case PageKind::Watering:
            return entry.watering_request;
        case PageKind::Leak:
        default:
            return entry.leak_request;
    }
}

const StackUnitSnapshot::PageRequestState &StackUnitSnapshot::pageRequestState_(const Entry &entry, PageKind kind)
{
    switch (kind)
    {
        case PageKind::Sockets:
            return entry.sockets_request;
        case PageKind::Lights:
            return entry.lights_request;
        case PageKind::Meteo:
            return entry.meteo_request;
        case PageKind::Thermo:
            return entry.thermo_request;
        case PageKind::Tanks:
            return entry.tanks_request;
        case PageKind::Watering:
            return entry.watering_request;
        case PageKind::Leak:
        default:
            return entry.leak_request;
    }
}

template <typename ItemT, size_t N>
void StackUnitSnapshot::clearPageCache_(PageCache<ItemT, N> &cache)
{
    memset(cache.items, 0, sizeof(cache.items));
}

template <typename ItemT, size_t N>
uint8_t StackUnitSnapshot::applyPage_(PageCache<ItemT, N> &cache, uint16_t offset, const ItemT *items, uint8_t item_count)
{
    if (offset == 0)
        clearPageCache_(cache);
    const uint16_t safe_offset = (offset > N) ? (uint16_t)N : offset;
    uint16_t loaded = safe_offset;
    if (items && item_count > 0 && safe_offset < N)
    {
        for (uint8_t i = 0; i < item_count && (safe_offset + i) < N; ++i)
            cache.items[safe_offset + i] = items[i];
        loaded = safe_offset + item_count;
        if (loaded > N)
            loaded = (uint16_t)N;
    }
    return clampCount_(loaded, N);
}

bool StackUnitSnapshot::preparePageRequest_(uint32_t node_id, PageKind kind, uint32_t now_ms, uint16_t offset,
                                            uint32_t pending_ms)
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return false;
    PageRequestState &state = pageRequestState_(*entry, kind);
    if (state.pending && state.offset == offset && (uint32_t)(now_ms - state.started_ms) < pending_ms)
        return false;
    entry->used = true;
    entry->state.node_id = node_id;
    state.pending = true;
    state.offset = offset;
    state.started_ms = now_ms;
    return true;
}

void StackUnitSnapshot::completePageRequest_(uint32_t node_id, PageKind kind, uint16_t offset)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = findEntry_(node_id);
    if (!entry)
        return;
    PageRequestState &state = pageRequestState_(*entry, kind);
    if (!state.pending || state.offset != offset)
        return;
    state.pending = false;
    state.offset = 0;
    state.started_ms = 0;
}

void StackUnitSnapshot::clearPageRequest_(uint32_t node_id, PageKind kind)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = findEntry_(node_id);
    if (!entry)
        return;
    PageRequestState &state = pageRequestState_(*entry, kind);
    state.pending = false;
    state.offset = 0;
    state.started_ms = 0;
}
