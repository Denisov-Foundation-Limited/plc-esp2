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

#include "hal/gpio/gpio.hpp"
#include "hal/gpio/portio.hpp"
#include "utils/logger.hpp"
#include "utils/rtos_lock.hpp"

class TankController
{
public:
    static constexpr size_t kTankCount = 20;
    static constexpr uint8_t kInvalidPort = 0xFF;
    using LockGuard = RtosRecursiveLock::Guard;

    struct TankConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        uint8_t group_id = 0;
        bool power_on = false;
        uint8_t level_low = kInvalidPort;
        uint8_t level_mid = kInvalidPort;
        uint8_t level_full = kInvalidPort;
        uint8_t relay_valve = kInvalidPort;
        uint8_t relay_pump = kInvalidPort;
        uint8_t relay_alarm = kInvalidPort;
        String name;
    };

    struct TankState
    {
        bool level_low = false;
        bool level_mid = false;
        bool level_full = false;
        bool level_low_candidate = false;
        bool level_mid_candidate = false;
        bool level_full_candidate = false;
        bool levels_initialized = false;
        uint32_t level_low_changed_ms = 0;
        uint32_t level_mid_changed_ms = 0;
        uint32_t level_full_changed_ms = 0;
        bool levels_ok = false;
        bool levels_ok_prev = true;
        uint32_t last_level_err_ms = 0;
        bool valve_on = false;
        bool pump_on = false;
        bool alarm_on = false;
        bool last_empty = false;
        uint32_t last_empty_event_ms = 0;
    };

    using DetectHandler = void (*)(void *ctx, uint8_t tank_id, const String &name, bool empty);

    TankController(Gpio &gpio, Logger &logs);

    bool begin();
    void task();
    void applyConfig(JsonArrayConst tanks);
    void serialize(JsonArray out) const;
    void buildSnapshot(uint8_t *power_mask, size_t bytes) const;
    void applySnapshot(const uint8_t *power_mask, size_t bytes);
    bool takeDirty();
    bool controllerEnabled() const;
    void setControllerEnabled(bool enabled);
    bool setEnabled(size_t id, bool enabled);
    bool setPower(size_t id, bool on);
    bool setName(size_t id, const String &name);
    bool setGroupId(size_t id, uint8_t group_id);
    bool setLevelLow(size_t id, uint8_t port);
    bool setLevelMid(size_t id, uint8_t port);
    bool setLevelFull(size_t id, uint8_t port);
    bool setValveRelay(size_t id, uint8_t port);
    bool setPumpRelay(size_t id, uint8_t port);
    bool setAlarmRelay(size_t id, uint8_t port);
    void setDetectHandler(DetectHandler cb, void *ctx);
    void setDetectHandlerSecondary(DetectHandler cb, void *ctx);
    void setNotifyEnabled(bool enabled);
    void notifyRemoteEmpty(const String &source, uint8_t tank_id, const String &name);
    const TankConfig *config(size_t id) const;
    const TankState *state(size_t id) const;
    const TankConfig *configByIndex(size_t idx) const;
    const TankState *stateByIndex(size_t idx) const;
    LockGuard lockGuard(uint32_t timeout_ms = 0xFFFFFFFFu) const
    {
        return _lock.guard(timeout_ms);
    }

private:
    Gpio &_gpio;
    Logger &_logs;
    TankConfig _cfg[kTankCount]{};
    TankState _state[kTankCount]{};
    bool _controller_enabled = false;
    bool _notify_enabled = true;
    bool _dirty = false;
    DetectHandler _detect_cb = nullptr;
    void *_detect_ctx = nullptr;
    DetectHandler _detect_cb_secondary = nullptr;
    void *_detect_ctx_secondary = nullptr;
    mutable RtosRecursiveLock _lock;

    void reset_();
    static bool parsePort_(JsonVariantConst v, uint8_t &out);
    static bool indexById_(uint8_t id, size_t &out);
    void setupInputs_(const TankConfig &cfg);
    void setupInput_(uint8_t port);
    bool setLevelPort_(size_t id, uint8_t port, uint8_t TankConfig::*field);
    bool setRelayPort_(size_t id, uint8_t port, uint8_t TankConfig::*field, uint8_t);
    void setupOutputs_(const TankConfig &cfg, TankState &st);
    void setupRelay_(uint8_t port, bool &state);
    void readLevels_(const TankConfig &cfg, TankState &st);
    bool readInput_(uint8_t port, bool &out);
    void updateControl_(const TankConfig &cfg, TankState &st);
    void logLevelChange_(const TankConfig &cfg, const TankState &prev, const TankState &curr);
    void logRelayChange_(const TankConfig &cfg, const TankState &prev, const TankState &curr);
    static bool isEmpty_(const TankState &st);
    void writeAllOff_(const TankConfig &cfg, TankState &st);
    void writeRelay_(uint8_t port, bool on);
    void notifyEmpty_(const TankConfig &cfg);
    void notifyDetectEvent_(const TankConfig &cfg, bool empty);
    static constexpr bool kLevelPullup = true;
    static constexpr uint32_t kLevelErrLogMs = 5000;
    static constexpr uint32_t kLevelDebounceMs = 1000;
    static constexpr uint32_t kEmptyEventDebounceMs = 10000;
};
