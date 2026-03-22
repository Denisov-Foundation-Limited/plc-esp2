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
