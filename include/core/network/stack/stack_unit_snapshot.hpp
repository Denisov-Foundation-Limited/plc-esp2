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
#include <stdint.h>

#include "core/network/stack/stack_device_registry.hpp"
#include "utils/rtos_lock.hpp"

class StackUnitSnapshot
{
public:
    static constexpr size_t kDateLen = 16;
    static constexpr size_t kTimeLen = 16;

    struct Snapshot
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint32_t request_started_ms = 0;
        bool pending = false;
        bool has_plc = false;
        bool has_rtc = false;
        bool rtc_temp_ok = false;
        float board_temp = 0.0f;
        bool fan_on = false;
        float rtc_temp = 0.0f;
        char rtc_date[kDateLen] = {};
        char rtc_time[kTimeLen] = {};
    };

    StackUnitSnapshot();
    ~StackUnitSnapshot();

    bool prepareRequest(uint32_t node_id, uint32_t now_ms, uint32_t fresh_ms, uint32_t pending_ms);
    bool snapshot(uint32_t node_id, Snapshot &out) const;
    void clearPending(uint32_t node_id);
    void update(uint32_t node_id, const Snapshot &state);
    void invalidate(uint32_t node_id);

private:
    struct Entry
    {
        bool used = false;
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint32_t request_started_ms = 0;
        bool pending = false;
        bool has_plc = false;
        bool has_rtc = false;
        bool rtc_temp_ok = false;
        float board_temp = 0.0f;
        bool fan_on = false;
        float rtc_temp = 0.0f;
        char rtc_date[kDateLen] = {};
        char rtc_time[kTimeLen] = {};
    };

    Entry *findEntry_(uint32_t node_id);
    const Entry *findEntry_(uint32_t node_id) const;
    Entry *allocEntry_(uint32_t node_id);
    bool ensureStorage_() const;
    void releaseStorage_();

    mutable RtosRecursiveLock _lock;
    mutable Entry *_entries = nullptr;
};
