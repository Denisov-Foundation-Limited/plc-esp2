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
#include <stdint.h>

#include "core/rtc.hpp"
#include "hal/gpio/gpio.hpp"
#include "controllers/tank_controller.hpp"
#include "utils/logger.hpp"
#include "utils/rtos_lock.hpp"

class WateringController
{
public:
    static constexpr size_t kRuleCount = 30;
    static constexpr uint8_t kInvalidPort = 0xFF;
    static constexpr uint8_t kTimeSlotCount = 3;
    using LockGuard = RtosRecursiveLock::Guard;

    struct RuleConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        uint8_t port = kInvalidPort;
        uint8_t tank_id = 0;
        uint8_t weekdays_mask = 0; // bit0=Sun ... bit6=Sat (RTC day_of_week 1..7)
        uint8_t hour = 0xFF;
        uint8_t minute = 0xFF;
        uint32_t duration_sec = 0;
        uint8_t hour2 = 0xFF;
        uint8_t minute2 = 0xFF;
        uint32_t duration2_sec = 0;
        uint8_t hour3 = 0xFF;
        uint8_t minute3 = 0xFF;
        uint32_t duration3_sec = 0;
        uint8_t resume_level = 0; // 0=low,1=mid,2=full
        bool resume_after_refill = false;
        String name;
    };

    struct RuleState
    {
        bool status = false; // monitor time
        bool active = false;
        bool paused = false;
        uint32_t end_ms = 0;
        uint32_t remaining_ms = 0;
        uint32_t last_start_key = 0;
    };

    enum class Event
    {
        Start,
        PauseEmpty,
        Resume,
        Stop,
        StopEmpty,
        StopDone
    };
    using EventHandler = void (*)(void *ctx, Event ev, const RuleConfig &cfg, const RuleState &st);

    WateringController(Gpio &gpio, TankController &tanks, RTC &rtc, Logger &logs);

    bool begin();
    void setEventHandler(EventHandler cb, void *ctx);
    void setEventHandlerSecondary(EventHandler cb, void *ctx);
    void task();
    void applyConfig(JsonArrayConst rules);
    void serialize(JsonArray out) const;
    void applySnapshot(const uint8_t *status_mask, size_t bytes);
    void buildSnapshot(uint8_t *status_mask, size_t bytes) const;
    void applyRuntimeSnapshot(const uint8_t *active_mask, const uint8_t *paused_mask, const uint32_t *remaining_ms,
                              const uint32_t *last_start_key, size_t bytes);
    void buildRuntimeSnapshot(uint8_t *active_mask, uint8_t *paused_mask, uint32_t *remaining_ms,
                              uint32_t *last_start_key, size_t bytes) const;
    bool setEnabled(size_t id, bool enabled);
    bool setName(size_t id, const String &name);
    bool setPort(size_t id, uint8_t port);
    bool setStartDate(size_t id, uint16_t year, uint8_t month, uint8_t day);
    bool setStartTime(size_t id, uint8_t hour, uint8_t minute);
    bool setStartTimeSlot(size_t id, uint8_t slot, uint8_t hour, uint8_t minute);
    bool setWeekdaysMask(size_t id, uint8_t mask);
    bool setDuration(size_t id, uint32_t duration_sec);
    bool setDurationSlot(size_t id, uint8_t slot, uint32_t duration_sec);
    bool setStatus(size_t id, bool status);
    bool controllerEnabled() const;
    void setControllerEnabled(bool enabled);
    bool setTankId(size_t id, uint8_t tank_id);
    bool setResumeAfterRefill(size_t id, bool enable);
    bool setResumeLevel(size_t id, uint8_t level);
    bool takeDirty();
    bool takeRuntimeDirty();
    const RuleConfig *config(size_t id) const;
    const RuleState *state(size_t id) const;
    const RuleConfig *configByIndex(size_t idx) const;
    const RuleState *stateByIndex(size_t idx) const;
    LockGuard lockGuard(uint32_t timeout_ms = 0xFFFFFFFFu) const
    {
        return _lock.guard(timeout_ms);
    }

private:
    Gpio &_gpio;
    TankController &_tanks;
    RTC &_rtc;
    Logger &_logs;
    RuleConfig _cfg[kRuleCount]{};
    RuleState _state[kRuleCount]{};
    bool _dirty = false;
    bool _runtime_dirty = false;
    bool _controller_enabled = false;
    EventHandler _event_cb = nullptr;
    void *_event_ctx = nullptr;
    EventHandler _event_cb_secondary = nullptr;
    void *_event_ctx_secondary = nullptr;
    mutable RtosRecursiveLock _lock;

    void reset_();
    static bool isStartValid_(const RuleConfig &cfg);
    static uint32_t makeStartKey_(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t slot);
    static void getSlot_(const RuleConfig &cfg, uint8_t slot, uint8_t &hour, uint8_t &minute, uint32_t &duration_sec);
    static void setSlotTime_(uint8_t slot, RuleConfig &cfg, uint8_t hour, uint8_t minute);
    static void setSlotDuration_(uint8_t slot, RuleConfig &cfg, uint32_t duration_sec);
    static uint8_t calcDow_(uint16_t y, uint8_t m, uint8_t d);
    static bool isWeekdayAllowed_(const RuleConfig &cfg, uint8_t day_of_week);
    static bool timeAfterOrEqual_(uint32_t now, uint32_t target);
    void stopIfActive_(const RuleConfig &cfg, RuleState &st, Event reason, bool notify = true);
    void stopForEmpty_(const RuleConfig &cfg, RuleState &st, bool notify = true);
    void resumeAfterRefill_(const RuleConfig &cfg, RuleState &st, bool notify = true);
    void writePort_(uint8_t port, bool on);
    bool isTankEmpty_(const RuleConfig &cfg) const;
    bool isResumeLevelReached_(const RuleConfig &cfg) const;
    bool indexById_(size_t id, size_t &out) const;
    void notifyEvent_(Event ev, const RuleConfig &cfg, const RuleState &st);
};
