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
    const bool fresh_plc = entry->has_plc && (uint32_t)(now_ms - entry->updated_ms) < fresh_ms;
    const bool fresh_rtc = entry->has_rtc && (uint32_t)(now_ms - entry->updated_ms) < fresh_ms;
    if (entry->pending && (uint32_t)(now_ms - entry->request_started_ms) < pending_ms)
        return false;
    if (fresh_plc && fresh_rtc)
        return false;
    entry->used = true;
    entry->node_id = node_id;
    entry->pending = true;
    entry->request_started_ms = now_ms;
    return true;
}

bool StackUnitSnapshot::snapshot(uint32_t node_id, Snapshot &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    if (!entry)
        return false;
    out.node_id = entry->node_id;
    out.updated_ms = entry->updated_ms;
    out.request_started_ms = entry->request_started_ms;
    out.pending = entry->pending;
    out.has_plc = entry->has_plc;
    out.has_rtc = entry->has_rtc;
    out.rtc_temp_ok = entry->rtc_temp_ok;
    out.board_temp = entry->board_temp;
    out.fan_on = entry->fan_on;
    out.rtc_temp = entry->rtc_temp;
    memcpy(out.rtc_date, entry->rtc_date, sizeof(out.rtc_date));
    memcpy(out.rtc_time, entry->rtc_time, sizeof(out.rtc_time));
    out.sockets_enabled = entry->sockets_enabled;
    out.sockets_on = entry->sockets_on;
    out.lights_enabled = entry->lights_enabled;
    out.lights_on = entry->lights_on;
    out.meteo_enabled = entry->meteo_enabled;
    out.meteo_ok = entry->meteo_ok;
    out.thermo_enabled = entry->thermo_enabled;
    out.thermo_active = entry->thermo_active;
    out.tanks_enabled = entry->tanks_enabled;
    out.tanks_alert = entry->tanks_alert;
    out.septic_enabled = entry->septic_enabled;
    out.septic_alert = entry->septic_alert;
    out.watering_enabled = entry->watering_enabled;
    out.watering_active = entry->watering_active;
    out.security_sensors_enabled = entry->security_sensors_enabled;
    out.leak_enabled = entry->leak_enabled;
    out.leak_alert = entry->leak_alert;
    out.security_enabled = entry->security_enabled;
    out.security_armed = entry->security_armed;
    out.security_alarm = entry->security_alarm;
    out.ring_enabled = entry->ring_enabled;
    out.ring_on = entry->ring_on;
    out.avr_enabled = entry->avr_enabled;
    out.avr_fault = entry->avr_fault;
    out.avr_active_source = entry->avr_active_source;
    out.socket_count = entry->socket_count;
    memcpy(out.sockets, entry->sockets, sizeof(out.sockets));
    return true;
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
    entry->pending = false;
    entry->request_started_ms = 0;
}

void StackUnitSnapshot::update(uint32_t node_id, const Snapshot &state)
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
    entry->node_id = node_id;
    entry->updated_ms = state.updated_ms ? state.updated_ms : millis();
    entry->request_started_ms = state.request_started_ms;
    entry->pending = state.pending;
    entry->has_plc = state.has_plc;
    entry->has_rtc = state.has_rtc;
    entry->rtc_temp_ok = state.rtc_temp_ok;
    entry->board_temp = state.board_temp;
    entry->fan_on = state.fan_on;
    entry->rtc_temp = state.rtc_temp;
    memcpy(entry->rtc_date, state.rtc_date, sizeof(entry->rtc_date));
    memcpy(entry->rtc_time, state.rtc_time, sizeof(entry->rtc_time));
    entry->sockets_enabled = state.sockets_enabled;
    entry->sockets_on = state.sockets_on;
    entry->lights_enabled = state.lights_enabled;
    entry->lights_on = state.lights_on;
    entry->meteo_enabled = state.meteo_enabled;
    entry->meteo_ok = state.meteo_ok;
    entry->thermo_enabled = state.thermo_enabled;
    entry->thermo_active = state.thermo_active;
    entry->tanks_enabled = state.tanks_enabled;
    entry->tanks_alert = state.tanks_alert;
    entry->septic_enabled = state.septic_enabled;
    entry->septic_alert = state.septic_alert;
    entry->watering_enabled = state.watering_enabled;
    entry->watering_active = state.watering_active;
    entry->security_sensors_enabled = state.security_sensors_enabled;
    entry->leak_enabled = state.leak_enabled;
    entry->leak_alert = state.leak_alert;
    entry->security_enabled = state.security_enabled;
    entry->security_armed = state.security_armed;
    entry->security_alarm = state.security_alarm;
    entry->ring_enabled = state.ring_enabled;
    entry->ring_on = state.ring_on;
    entry->avr_enabled = state.avr_enabled;
    entry->avr_fault = state.avr_fault;
    entry->avr_active_source = state.avr_active_source;
    entry->socket_count = state.socket_count;
    memcpy(entry->sockets, state.sockets, sizeof(entry->sockets));
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
        *entry = Entry{};
}

StackUnitSnapshot::Entry *StackUnitSnapshot::findEntry_(uint32_t node_id)
{
    if (node_id == 0 || !_entries)
        return nullptr;
    for (size_t i = 0; i < StackDeviceRegistry::kMaxDevices; ++i)
    {
        auto &entry = _entries[i];
        if (entry.used && entry.node_id == node_id)
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
        if (entry.used && entry.node_id == node_id)
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
            entry = Entry{};
            entry.used = true;
            entry.node_id = node_id;
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
#if defined(ESP32)
    if (psramFound())
        mem = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
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
