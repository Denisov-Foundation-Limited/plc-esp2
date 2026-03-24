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
    copyState_(out, *entry);
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
    copyState_(out, *entry);
    memcpy(out.sockets, entry->sockets, sizeof(out.sockets));
    memcpy(out.lights, entry->lights, sizeof(out.lights));
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
    return entry && copyItemById_(entry->sockets, entry->socket_count, id, out);
}

bool StackUnitSnapshot::socketAt(uint32_t node_id, uint8_t index, SocketItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_(entry->sockets, entry->socket_count, index, out);
}

bool StackUnitSnapshot::lightById(uint32_t node_id, uint8_t id, SocketItem &out) const
{
    if (node_id == 0 || id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemById_(entry->lights, entry->light_count, id, out);
}

bool StackUnitSnapshot::lightAt(uint32_t node_id, uint8_t index, SocketItem &out) const
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    const Entry *entry = findEntry_(node_id);
    return entry && copyItemAt_(entry->lights, entry->light_count, index, out);
}

bool StackUnitSnapshot::prepareSocketsPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms)
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return false;
    if (entry->sockets_page_pending &&
        entry->sockets_page_request_offset == offset &&
        (uint32_t)(now_ms - entry->sockets_page_request_started_ms) < pending_ms)
        return false;
    entry->used = true;
    entry->node_id = node_id;
    entry->sockets_page_pending = true;
    entry->sockets_page_request_offset = offset;
    entry->sockets_page_request_started_ms = now_ms;
    return true;
}

bool StackUnitSnapshot::prepareLightsPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms)
{
    if (node_id == 0)
        return false;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return false;
    Entry *entry = allocEntry_(node_id);
    if (!entry)
        return false;
    if (entry->lights_page_pending &&
        entry->lights_page_request_offset == offset &&
        (uint32_t)(now_ms - entry->lights_page_request_started_ms) < pending_ms)
        return false;
    entry->used = true;
    entry->node_id = node_id;
    entry->lights_page_pending = true;
    entry->lights_page_request_offset = offset;
    entry->lights_page_request_started_ms = now_ms;
    return true;
}

void StackUnitSnapshot::completeSocketsPageRequest(uint32_t node_id, uint16_t offset)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = findEntry_(node_id);
    if (!entry)
        return;
    if (!entry->sockets_page_pending || entry->sockets_page_request_offset != offset)
        return;
    entry->sockets_page_pending = false;
    entry->sockets_page_request_offset = 0;
    entry->sockets_page_request_started_ms = 0;
}

void StackUnitSnapshot::completeLightsPageRequest(uint32_t node_id, uint16_t offset)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = findEntry_(node_id);
    if (!entry)
        return;
    if (!entry->lights_page_pending || entry->lights_page_request_offset != offset)
        return;
    entry->lights_page_pending = false;
    entry->lights_page_request_offset = 0;
    entry->lights_page_request_started_ms = 0;
}

void StackUnitSnapshot::clearSocketsPageRequest(uint32_t node_id)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = findEntry_(node_id);
    if (!entry)
        return;
    entry->sockets_page_pending = false;
    entry->sockets_page_request_offset = 0;
    entry->sockets_page_request_started_ms = 0;
}

void StackUnitSnapshot::clearLightsPageRequest(uint32_t node_id)
{
    if (node_id == 0)
        return;
    const auto guard = _lock.guard();
    if (!ensureStorage_())
        return;
    Entry *entry = findEntry_(node_id);
    if (!entry)
        return;
    entry->lights_page_pending = false;
    entry->lights_page_request_offset = 0;
    entry->lights_page_request_started_ms = 0;
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
    const uint32_t sockets_page_request_started_ms = entry->sockets_page_request_started_ms;
    const uint32_t lights_page_request_started_ms = entry->lights_page_request_started_ms;
    const uint16_t sockets_page_request_offset = entry->sockets_page_request_offset;
    const uint16_t lights_page_request_offset = entry->lights_page_request_offset;
    const bool sockets_page_pending = entry->sockets_page_pending;
    const bool lights_page_pending = entry->lights_page_pending;
    entry->used = true;
    copyState_(*entry, state);
    entry->updated_ms = state.updated_ms ? state.updated_ms : millis();
    entry->sockets_page_request_started_ms = sockets_page_request_started_ms;
    entry->lights_page_request_started_ms = lights_page_request_started_ms;
    entry->sockets_page_request_offset = sockets_page_request_offset;
    entry->lights_page_request_offset = lights_page_request_offset;
    entry->sockets_page_pending = sockets_page_pending;
    entry->lights_page_pending = lights_page_pending;
    memcpy(entry->sockets, state.sockets, sizeof(entry->sockets));
    memcpy(entry->lights, state.lights, sizeof(entry->lights));
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
            memset(&entry, 0, sizeof(entry));
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

void StackUnitSnapshot::copyState_(State &dst, const State &src)
{
    dst.node_id = src.node_id;
    dst.updated_ms = src.updated_ms;
    dst.request_started_ms = src.request_started_ms;
    dst.pending = src.pending;
    dst.has_plc = src.has_plc;
    dst.has_rtc = src.has_rtc;
    dst.rtc_temp_ok = src.rtc_temp_ok;
    dst.board_temp = src.board_temp;
    dst.fan_on = src.fan_on;
    dst.rtc_temp = src.rtc_temp;
    memcpy(dst.rtc_date, src.rtc_date, sizeof(dst.rtc_date));
    memcpy(dst.rtc_time, src.rtc_time, sizeof(dst.rtc_time));
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
    dst.septic_alert = src.septic_alert;
    dst.watering_enabled = src.watering_enabled;
    dst.watering_active = src.watering_active;
    dst.security_sensors_enabled = src.security_sensors_enabled;
    dst.leak_enabled = src.leak_enabled;
    dst.leak_alert = src.leak_alert;
    dst.security_enabled = src.security_enabled;
    dst.security_armed = src.security_armed;
    dst.security_alarm = src.security_alarm;
    dst.ring_enabled = src.ring_enabled;
    dst.ring_on = src.ring_on;
    dst.avr_enabled = src.avr_enabled;
    dst.avr_fault = src.avr_fault;
    dst.avr_active_source = src.avr_active_source;
    dst.socket_count = src.socket_count;
    dst.light_count = src.light_count;
    dst.sockets_page_request_started_ms = src.sockets_page_request_started_ms;
    dst.lights_page_request_started_ms = src.lights_page_request_started_ms;
    dst.sockets_page_request_offset = src.sockets_page_request_offset;
    dst.lights_page_request_offset = src.lights_page_request_offset;
    dst.sockets_page_pending = src.sockets_page_pending;
    dst.lights_page_pending = src.lights_page_pending;
}

bool StackUnitSnapshot::copyItemById_(const SocketItem *items, uint8_t count, uint8_t id, SocketItem &out)
{
    if (!items || id == 0)
        return false;
    for (uint8_t i = 0; i < count && i < kSocketCount; ++i)
    {
        if (items[i].id != id)
            continue;
        out = items[i];
        return true;
    }
    return false;
}

bool StackUnitSnapshot::copyItemAt_(const SocketItem *items, uint8_t count, uint8_t index, SocketItem &out)
{
    if (!items || index >= count || index >= kSocketCount)
        return false;
    out = items[index];
    return true;
}
