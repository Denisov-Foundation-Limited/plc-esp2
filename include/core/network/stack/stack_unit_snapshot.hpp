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
    static constexpr size_t kSocketNameLen = 24;
    static constexpr size_t kSocketCount = 72;

    struct SocketItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool state = false;
        uint8_t button_port = 0xFF;
        uint8_t relay_port = 0xFF;
        uint8_t group_id = 0;
        char name[kSocketNameLen] = {};
    };

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
        uint16_t sockets_enabled = 0;
        uint16_t sockets_on = 0;
        uint16_t lights_enabled = 0;
        uint16_t lights_on = 0;
        uint16_t meteo_enabled = 0;
        uint16_t meteo_ok = 0;
        uint16_t thermo_enabled = 0;
        uint16_t thermo_active = 0;
        uint16_t tanks_enabled = 0;
        uint16_t tanks_alert = 0;
        uint16_t septic_enabled = 0;
        uint16_t septic_alert = 0;
        uint16_t watering_enabled = 0;
        uint16_t watering_active = 0;
        uint16_t security_sensors_enabled = 0;
        uint16_t leak_enabled = 0;
        uint16_t leak_alert = 0;
        bool security_enabled = false;
        bool security_armed = false;
        bool security_alarm = false;
        bool ring_enabled = false;
        bool ring_on = false;
        bool avr_enabled = false;
        bool avr_fault = false;
        uint8_t avr_active_source = 0;
        uint8_t socket_count = 0;
        SocketItem sockets[kSocketCount]{};
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
        uint16_t sockets_enabled = 0;
        uint16_t sockets_on = 0;
        uint16_t lights_enabled = 0;
        uint16_t lights_on = 0;
        uint16_t meteo_enabled = 0;
        uint16_t meteo_ok = 0;
        uint16_t thermo_enabled = 0;
        uint16_t thermo_active = 0;
        uint16_t tanks_enabled = 0;
        uint16_t tanks_alert = 0;
        uint16_t septic_enabled = 0;
        uint16_t septic_alert = 0;
        uint16_t watering_enabled = 0;
        uint16_t watering_active = 0;
        uint16_t security_sensors_enabled = 0;
        uint16_t leak_enabled = 0;
        uint16_t leak_alert = 0;
        bool security_enabled = false;
        bool security_armed = false;
        bool security_alarm = false;
        bool ring_enabled = false;
        bool ring_on = false;
        bool avr_enabled = false;
        bool avr_fault = false;
        uint8_t avr_active_source = 0;
        uint8_t socket_count = 0;
        SocketItem sockets[kSocketCount]{};
    };

    Entry *findEntry_(uint32_t node_id);
    const Entry *findEntry_(uint32_t node_id) const;
    Entry *allocEntry_(uint32_t node_id);
    bool ensureStorage_() const;
    void releaseStorage_();

    mutable RtosRecursiveLock _lock;
    mutable Entry *_entries = nullptr;
};
