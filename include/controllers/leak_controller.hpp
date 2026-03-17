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
#include <ArduinoJson.h>

#include "core/network/telegram/telegram_allowed_users.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "hal/gpio/gpio.hpp"
#include "hal/gpio/portio.hpp"
#include "utils/logger.hpp"
#include "utils/rtos_lock.hpp"

class LeakController
{
public:
    static constexpr size_t kZoneCount = 16;
    static constexpr uint8_t kInvalidPort = 0xFF;
    using LockGuard = RtosRecursiveLock::Guard;

    struct ZoneConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        bool power_on = false;
        bool sensor_active_low = true;
        bool valve_open_on_power = true;
        uint8_t sensor_port = kInvalidPort;
        uint8_t valve_port = kInvalidPort;
        uint8_t alarm_port = kInvalidPort;
        String name;
    };

    struct ZoneState
    {
        bool wet = false;
        bool last_wet = false;
        bool alarm_latched = false;
        bool valve_closed = false;
        bool alarm_on = false;
        uint32_t last_detect_event_ms = 0;
    };

    LeakController(Gpio &gpio, Logger &logs, TelegramBot &bot, TelegramAllowedUsersProvider &users)
        ;bool begin();void task();void applyConfig(JsonArrayConst zones);void serialize(JsonArray out) const;bool controllerEnabled() const;bool setControllerEnabled(bool enabled);bool takeDirty();bool setEnabled(size_t id, bool enabled);bool setPower(size_t id, bool on);bool setSensorActiveLow(size_t id, bool active_low);bool setValveOpenOnPower(size_t id, bool open_on_power);bool setName(size_t id, const String &name);bool setSensorPort(size_t id, uint8_t port);bool setValvePort(size_t id, uint8_t port);bool setAlarmPort(size_t id, uint8_t port);bool ack(size_t id);bool ackAll();const ZoneConfig *config(size_t id) const;const ZoneState *state(size_t id) const;const ZoneConfig *configByIndex(size_t idx) const;const ZoneState *stateByIndex(size_t idx) const;LockGuard lockGuard(uint32_t timeout_ms = 0xFFFFFFFFu) const { return _lock.guard(timeout_ms); }
private:
    Gpio &_gpio;
    Logger &_logs;
    TelegramBot &_tgbot;
    TelegramAllowedUsersProvider &_tgusers;
    ZoneConfig _cfg[kZoneCount]{};
    ZoneState _state[kZoneCount]{};
    bool _controller_enabled = false;
    bool _dirty = false;
    mutable RtosRecursiveLock _lock;

    void reset_();static bool parsePort_(JsonVariantConst v, uint8_t &out);static bool indexById_(size_t id, size_t &out);bool setPort_(size_t id, uint8_t port, uint8_t ZoneConfig::*field, bool input);void setupZone_(const ZoneConfig &cfg, ZoneState &st);void setupInput_(uint8_t port);void setupOutput_(uint8_t port);bool readSensor_(const ZoneConfig &cfg, bool &wet);void writeOutputs_(const ZoneConfig &cfg, ZoneState &st, bool alarm);void notifyLeak_(const ZoneConfig &cfg);void sendTgNotify_(const String &msg);static constexpr uint32_t kDetectEventDebounceMs = 10000;
};
