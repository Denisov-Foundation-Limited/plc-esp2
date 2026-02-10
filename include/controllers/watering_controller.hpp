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

class WateringController
{
public:
    static constexpr size_t kRuleCount = 30;
    static constexpr uint8_t kInvalidPort = 0xFF;
    static constexpr uint8_t kTimeSlotCount = 3;

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

    WateringController(Gpio &gpio, TankController &tanks, RTC &rtc, Logger &logs)
        : _gpio(gpio), _tanks(tanks), _rtc(rtc), _logs(logs)
    {
        reset_();
    }

    bool begin()
    {
        _logs.info(F("WATER"), F("Controller init"));
        return true;
    }

    void setEventHandler(EventHandler cb, void *ctx)
    {
        _event_cb = cb;
        _event_ctx = ctx;
    }

    void task()
    {
        if (!_controller_enabled)
            return;
        Ds3231Mz::DateTime now{};
        bool have_time = false;

        for (size_t i = 0; i < kRuleCount; ++i)
        {
            RuleConfig &cfg = _cfg[i];
            RuleState &st = _state[i];

            if (!cfg.enabled || !st.status)
            {
                stopIfActive_(cfg, st, Event::Stop);
                st.paused = false;
                st.remaining_ms = 0;
                continue;
            }

            const bool tank_empty = isTankEmpty_(cfg);
            if (tank_empty && st.active)
            {
                stopForEmpty_(cfg, st);
                continue;
            }

            if (st.paused)
            {
                if (!tank_empty && cfg.resume_after_refill && isResumeLevelReached_(cfg))
                    resumeAfterRefill_(cfg, st);
                continue;
            }

            if (st.active)
            {
                if (timeAfterOrEqual_(millis(), st.end_ms))
                {
                    stopIfActive_(cfg, st, Event::StopDone);
                }
                continue;
            }

            if (!isStartValid_(cfg) || cfg.port == kInvalidPort)
                continue;
            if (tank_empty)
                continue;

            if (!have_time)
            {
                if (!_rtc.Time(now))
                    return;
                have_time = true;
            }

            if (!isWeekdayAllowed_(cfg, now.day_of_week))
                continue;
            for (uint8_t slot = 0; slot < kTimeSlotCount; ++slot)
            {
                uint8_t slot_hour = 0;
                uint8_t slot_minute = 0;
                uint32_t slot_duration_sec = 0;
                getSlot_(cfg, slot, slot_hour, slot_minute, slot_duration_sec);
                if (slot_duration_sec == 0)
                    continue;
                if (slot_hour > 23 || slot_minute > 59)
                    continue;
                if (now.hour != slot_hour || now.minute != slot_minute)
                    continue;

                const uint32_t key = makeStartKey_(now.year, now.month, now.day, slot_hour, slot_minute, slot);
                if (st.last_start_key == key)
                    continue;

                st.last_start_key = key;
                st.active = true;
                st.paused = false;
                st.remaining_ms = 0;
                st.end_ms = millis() + slot_duration_sec * 1000u;
                writePort_(cfg.port, true);
                _logs.info(F("WATER"), F("start: rule: %u slot: %u port: %u tank: %u duration_s: %lu"),
                           (unsigned)cfg.id, (unsigned)(slot + 1u), (unsigned)cfg.port, (unsigned)cfg.tank_id,
                           (unsigned long)slot_duration_sec);
                notifyEvent_(Event::Start, cfg, st);
                _runtime_dirty = true;
                break;
            }
        }
    }

    void applyConfig(JsonArrayConst rules)
    {
        reset_();
        size_t idx = 0;
        for (JsonVariantConst v : rules)
        {
            if (idx >= kRuleCount)
                break;
            if (!v.is<JsonObjectConst>())
            {
                ++idx;
                continue;
            }
            JsonObjectConst obj = v.as<JsonObjectConst>();
            uint8_t id = (uint8_t)(idx + 1);
            if (obj["id"].is<unsigned>())
            {
                const unsigned raw = obj["id"].as<unsigned>();
                if (raw <= 0xFFu)
                    id = (uint8_t)raw;
            }
            size_t dst = 0;
            if (!indexById_(id, dst))
            {
                ++idx;
                continue;
            }
            RuleConfig &cfg = _cfg[dst];
            cfg.id = id;
            bool enabled_set = false;
            if (obj["enabled"].is<bool>())
            {
                cfg.enabled = obj["enabled"].as<bool>();
                enabled_set = true;
            }
            if (obj["name"].is<const char *>())
                cfg.name = obj["name"].as<const char *>();
            if (obj["port"].is<unsigned>())
            {
                const unsigned raw = obj["port"].as<unsigned>();
                if (raw <= 0xFFu)
                    cfg.port = (uint8_t)raw;
            }
            if (obj["tank_id"].is<unsigned>())
            {
                const unsigned raw = obj["tank_id"].as<unsigned>();
                if (raw <= 0xFFu)
                    cfg.tank_id = (uint8_t)raw;
            }
            if (obj["weekdays_mask"].is<unsigned>())
            {
                const unsigned raw = obj["weekdays_mask"].as<unsigned>();
                cfg.weekdays_mask = (uint8_t)(raw & 0x7Fu);
            }
            else if (obj["weekdays"].is<JsonArrayConst>())
            {
                uint8_t mask = 0;
                JsonArrayConst days = obj["weekdays"].as<JsonArrayConst>();
                for (JsonVariantConst dv : days)
                {
                    if (!dv.is<unsigned>())
                        continue;
                    const unsigned raw = dv.as<unsigned>();
                    if (raw >= 1 && raw <= 7)
                        mask |= (uint8_t)(1u << (raw - 1u));
                }
                cfg.weekdays_mask = mask;
            }
            else if (obj["year"].is<unsigned>() && obj["month"].is<unsigned>() && obj["day"].is<unsigned>())
            {
                const unsigned raw_y = obj["year"].as<unsigned>();
                const unsigned raw_m = obj["month"].as<unsigned>();
                const unsigned raw_d = obj["day"].as<unsigned>();
                if (raw_y >= 2000 && raw_y <= 2099 && raw_m >= 1 && raw_m <= 12 && raw_d >= 1 && raw_d <= 31)
                {
                    const uint8_t dow = calcDow_((uint16_t)raw_y, (uint8_t)raw_m, (uint8_t)raw_d);
                    if (dow >= 1 && dow <= 7)
                        cfg.weekdays_mask = (uint8_t)(1u << (dow - 1u));
                }
            }
            if (obj["hour"].is<unsigned>())
            {
                const unsigned raw = obj["hour"].as<unsigned>();
                if (raw <= 0xFFu)
                    cfg.hour = (uint8_t)raw;
            }
            if (obj["minute"].is<unsigned>())
            {
                const unsigned raw = obj["minute"].as<unsigned>();
                if (raw <= 0xFFu)
                    cfg.minute = (uint8_t)raw;
            }
            if (obj["duration_s"].is<unsigned long>())
                cfg.duration_sec = (uint32_t)obj["duration_s"].as<unsigned long>();
            if (obj["hour2"].is<unsigned>())
            {
                const unsigned raw = obj["hour2"].as<unsigned>();
                if (raw <= 0xFFu)
                    cfg.hour2 = (uint8_t)raw;
            }
            if (obj["minute2"].is<unsigned>())
            {
                const unsigned raw = obj["minute2"].as<unsigned>();
                if (raw <= 0xFFu)
                    cfg.minute2 = (uint8_t)raw;
            }
            if (obj["duration2_s"].is<unsigned long>())
                cfg.duration2_sec = (uint32_t)obj["duration2_s"].as<unsigned long>();
            if (obj["hour3"].is<unsigned>())
            {
                const unsigned raw = obj["hour3"].as<unsigned>();
                if (raw <= 0xFFu)
                    cfg.hour3 = (uint8_t)raw;
            }
            if (obj["minute3"].is<unsigned>())
            {
                const unsigned raw = obj["minute3"].as<unsigned>();
                if (raw <= 0xFFu)
                    cfg.minute3 = (uint8_t)raw;
            }
            if (obj["duration3_s"].is<unsigned long>())
                cfg.duration3_sec = (uint32_t)obj["duration3_s"].as<unsigned long>();
            if (obj["resume_after_refill"].is<bool>())
                cfg.resume_after_refill = obj["resume_after_refill"].as<bool>();
            if (obj["resume_level"].is<const char *>())
            {
                const char *lvl = obj["resume_level"].as<const char *>();
                if (lvl)
                {
                    String t = lvl;
                    t.toLowerCase();
                    if (t == "mid")
                        cfg.resume_level = 1;
                    else if (t == "full")
                        cfg.resume_level = 2;
                    else
                        cfg.resume_level = 0;
                }
            }
            else if (obj["resume_level"].is<unsigned>())
            {
                const unsigned raw = obj["resume_level"].as<unsigned>();
                if (raw <= 2u)
                    cfg.resume_level = (uint8_t)raw;
            }
            if (!enabled_set)
                cfg.enabled = true;
            ++idx;
        }
    }

    void serialize(JsonArray out) const
    {
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            const RuleConfig &cfg = _cfg[i];
            if (!cfg.enabled)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = cfg.id;
            obj["enabled"] = cfg.enabled;
            if (cfg.name.length())
                obj["name"] = cfg.name;
            if (cfg.port != kInvalidPort)
                obj["port"] = cfg.port;
            if (cfg.tank_id != 0)
                obj["tank_id"] = cfg.tank_id;
            if (cfg.weekdays_mask)
                obj["weekdays_mask"] = cfg.weekdays_mask;
            if (cfg.duration_sec && cfg.hour <= 23 && cfg.minute <= 59)
            {
                obj["hour"] = cfg.hour;
                obj["minute"] = cfg.minute;
                obj["duration_s"] = cfg.duration_sec;
            }
            if (cfg.duration2_sec && cfg.hour2 <= 23 && cfg.minute2 <= 59)
            {
                obj["hour2"] = cfg.hour2;
                obj["minute2"] = cfg.minute2;
                obj["duration2_s"] = cfg.duration2_sec;
            }
            if (cfg.duration3_sec && cfg.hour3 <= 23 && cfg.minute3 <= 59)
            {
                obj["hour3"] = cfg.hour3;
                obj["minute3"] = cfg.minute3;
                obj["duration3_s"] = cfg.duration3_sec;
            }
            obj["resume_after_refill"] = cfg.resume_after_refill;
            if (cfg.resume_level == 1)
                obj["resume_level"] = "mid";
            else if (cfg.resume_level == 2)
                obj["resume_level"] = "full";
            else
                obj["resume_level"] = "low";
        }
    }

    void applySnapshot(const uint8_t *status_mask, size_t bytes)
    {
        if (!status_mask || bytes == 0)
            return;
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            const size_t byte = i / 8u;
            const uint8_t bit = (uint8_t)(1u << (i % 8u));
            if (byte >= bytes)
                break;
            _state[i].status = (status_mask[byte] & bit) != 0;
        }
        _dirty = false;
    }

    void buildSnapshot(uint8_t *status_mask, size_t bytes) const
    {
        if (!status_mask || bytes == 0)
            return;
        memset(status_mask, 0, bytes);
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            if (!_state[i].status)
                continue;
            const size_t byte = i / 8u;
            const uint8_t bit = (uint8_t)(1u << (i % 8u));
            if (byte >= bytes)
                break;
            status_mask[byte] |= bit;
        }
    }

    void applyRuntimeSnapshot(const uint8_t *active_mask, const uint8_t *paused_mask, const uint32_t *remaining_ms,
                              const uint32_t *last_start_key, size_t bytes)
    {
        if (!active_mask || !paused_mask || !remaining_ms || !last_start_key || bytes == 0)
            return;
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            RuleConfig &cfg = _cfg[i];
            RuleState &st = _state[i];
            const size_t byte = i / 8u;
            const uint8_t bit = (uint8_t)(1u << (i % 8u));
            if (byte >= bytes)
                break;
            st.last_start_key = last_start_key[i];
            st.remaining_ms = remaining_ms[i];
            st.active = (active_mask[byte] & bit) != 0;
            st.paused = (paused_mask[byte] & bit) != 0;
            if (!cfg.enabled || !st.status || st.remaining_ms == 0)
            {
                st.active = false;
                st.paused = false;
                st.remaining_ms = 0;
                continue;
            }
            if (st.active)
            {
                st.end_ms = millis() + st.remaining_ms;
                writePort_(cfg.port, true);
            }
        }
        _runtime_dirty = false;
    }

    void buildRuntimeSnapshot(uint8_t *active_mask, uint8_t *paused_mask, uint32_t *remaining_ms,
                              uint32_t *last_start_key, size_t bytes) const
    {
        if (!active_mask || !paused_mask || !remaining_ms || !last_start_key || bytes == 0)
            return;
        memset(active_mask, 0, bytes);
        memset(paused_mask, 0, bytes);
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            const RuleState &st = _state[i];
            const size_t byte = i / 8u;
            const uint8_t bit = (uint8_t)(1u << (i % 8u));
            if (byte >= bytes)
                break;
            if (st.active)
                active_mask[byte] |= bit;
            if (st.paused)
                paused_mask[byte] |= bit;
            last_start_key[i] = st.last_start_key;
            if (st.active)
            {
                const uint32_t now = millis();
                remaining_ms[i] = timeAfterOrEqual_(st.end_ms, now) ? (st.end_ms - now) : 0;
            }
            else
            {
                remaining_ms[i] = st.remaining_ms;
            }
        }
    }

    bool setEnabled(size_t id, bool enabled)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].enabled = enabled;
        if (!enabled)
            stopIfActive_(_cfg[idx], _state[idx], Event::Stop);
        return true;
    }

    bool setName(size_t id, const String &name)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].name = name;
        return true;
    }

    bool setPort(size_t id, uint8_t port)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].port = port;
        if (port == kInvalidPort)
            stopIfActive_(_cfg[idx], _state[idx], Event::Stop);
        return true;
    }

    bool setStartDate(size_t id, uint16_t year, uint8_t month, uint8_t day)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31)
            return false;
        const uint8_t dow = calcDow_(year, month, day);
        if (dow < 1 || dow > 7)
            return false;
        _cfg[idx].weekdays_mask = (uint8_t)(1u << (dow - 1u));
        return true;
    }

    bool setStartTime(size_t id, uint8_t hour, uint8_t minute)
    {
        return setStartTimeSlot(id, 0, hour, minute);
    }

    bool setStartTimeSlot(size_t id, uint8_t slot, uint8_t hour, uint8_t minute)
    {
        if (slot >= kTimeSlotCount)
            return false;
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        setSlotTime_(slot, _cfg[idx], hour, minute);
        return true;
    }

    bool setWeekdaysMask(size_t id, uint8_t mask)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].weekdays_mask = (uint8_t)(mask & 0x7Fu);
        return true;
    }

    bool setDuration(size_t id, uint32_t duration_sec)
    {
        return setDurationSlot(id, 0, duration_sec);
    }

    bool setDurationSlot(size_t id, uint8_t slot, uint32_t duration_sec)
    {
        if (slot >= kTimeSlotCount)
            return false;
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        setSlotDuration_(slot, _cfg[idx], duration_sec);
        return true;
    }

    bool setStatus(size_t id, bool status)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        if (_state[idx].status == status)
            return true;
        _state[idx].status = status;
        if (!status)
            stopIfActive_(_cfg[idx], _state[idx], Event::Stop);
        _state[idx].paused = false;
        _state[idx].remaining_ms = 0;
        _runtime_dirty = true;
        _dirty = true;
        return true;
    }

    bool controllerEnabled() const { return _controller_enabled; }
    void setControllerEnabled(bool enabled)
    {
        if (_controller_enabled == enabled)
            return;
        _controller_enabled = enabled;
        if (!_controller_enabled)
        {
            for (size_t i = 0; i < kRuleCount; ++i)
                stopIfActive_(_cfg[i], _state[i], Event::Stop);
            reset_();
        }
    }

    bool setTankId(size_t id, uint8_t tank_id)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].tank_id = tank_id;
        if (tank_id == 0)
            return true;
        if (isTankEmpty_(_cfg[idx]))
            stopForEmpty_(_cfg[idx], _state[idx]);
        return true;
    }

    bool setResumeAfterRefill(size_t id, bool enable)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].resume_after_refill = enable;
        if (!enable && _state[idx].paused)
        {
            _state[idx].paused = false;
            _state[idx].remaining_ms = 0;
            _runtime_dirty = true;
        }
        return true;
    }

    bool setResumeLevel(size_t id, uint8_t level)
    {
        if (level > 2)
            return false;
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].resume_level = level;
        return true;
    }

    bool takeDirty()
    {
        const bool v = _dirty;
        _dirty = false;
        return v;
    }

    bool takeRuntimeDirty()
    {
        const bool v = _runtime_dirty;
        _runtime_dirty = false;
        return v;
    }

    const RuleConfig *config(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_cfg[idx];
    }

    const RuleState *state(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_state[idx];
    }

    const RuleConfig *configByIndex(size_t idx) const
    {
        if (idx >= kRuleCount)
            return nullptr;
        return &_cfg[idx];
    }

    const RuleState *stateByIndex(size_t idx) const
    {
        if (idx >= kRuleCount)
            return nullptr;
        return &_state[idx];
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

    void reset_()
    {
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            _cfg[i] = RuleConfig{};
            _cfg[i].id = (uint8_t)(i + 1);
            _cfg[i].duration_sec = 0;
            _state[i] = RuleState{};
        }
    }

    static bool isStartValid_(const RuleConfig &cfg)
    {
        if (cfg.weekdays_mask == 0)
            return false;
        for (uint8_t slot = 0; slot < kTimeSlotCount; ++slot)
        {
            uint8_t h = 0;
            uint8_t m = 0;
            uint32_t d = 0;
            getSlot_(cfg, slot, h, m, d);
            if (d == 0)
                continue;
            if (h <= 23 && m <= 59)
                return true;
        }
        return false;
    }

    static uint32_t makeStartKey_(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t slot)
    {
        const uint32_t base = (uint32_t)year * 100000000u + (uint32_t)month * 1000000u + (uint32_t)day * 10000u +
                              (uint32_t)hour * 100u + (uint32_t)minute;
        return base * 10u + (uint32_t)(slot % kTimeSlotCount);
    }

    static void getSlot_(const RuleConfig &cfg, uint8_t slot, uint8_t &hour, uint8_t &minute, uint32_t &duration_sec)
    {
        if (slot == 0)
        {
            hour = cfg.hour;
            minute = cfg.minute;
            duration_sec = cfg.duration_sec;
            return;
        }
        if (slot == 1)
        {
            hour = cfg.hour2;
            minute = cfg.minute2;
            duration_sec = cfg.duration2_sec;
            return;
        }
        hour = cfg.hour3;
        minute = cfg.minute3;
        duration_sec = cfg.duration3_sec;
    }

    static void setSlotTime_(uint8_t slot, RuleConfig &cfg, uint8_t hour, uint8_t minute)
    {
        if (slot == 0)
        {
            cfg.hour = hour;
            cfg.minute = minute;
            return;
        }
        if (slot == 1)
        {
            cfg.hour2 = hour;
            cfg.minute2 = minute;
            return;
        }
        cfg.hour3 = hour;
        cfg.minute3 = minute;
    }

    static void setSlotDuration_(uint8_t slot, RuleConfig &cfg, uint32_t duration_sec)
    {
        if (slot == 0)
        {
            cfg.duration_sec = duration_sec;
            return;
        }
        if (slot == 1)
        {
            cfg.duration2_sec = duration_sec;
            return;
        }
        cfg.duration3_sec = duration_sec;
    }

    static uint8_t calcDow_(uint16_t y, uint8_t m, uint8_t d)
    {
        static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (m < 3)
            y -= 1;
        const uint8_t dow = (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
        return (uint8_t)(dow + 1);
    }

    static bool isWeekdayAllowed_(const RuleConfig &cfg, uint8_t day_of_week)
    {
        if (day_of_week < 1 || day_of_week > 7)
            return false;
        const uint8_t bit = (uint8_t)(1u << (day_of_week - 1u));
        return (cfg.weekdays_mask & bit) != 0;
    }

    static bool timeAfterOrEqual_(uint32_t now, uint32_t target)
    {
        return (uint32_t)(now - target) < 0x80000000u;
    }

    void stopIfActive_(const RuleConfig &cfg, RuleState &st, Event reason)
    {
        if (!st.active)
            return;
        st.active = false;
        st.end_ms = 0;
        st.remaining_ms = 0;
        writePort_(cfg.port, false);
        _logs.info(F("WATER"), F("stop: rule: %u port: %u tank: %u"),
                   (unsigned)cfg.id, (unsigned)cfg.port, (unsigned)cfg.tank_id);
        notifyEvent_(reason, cfg, st);
        _runtime_dirty = true;
    }

    void stopForEmpty_(const RuleConfig &cfg, RuleState &st)
    {
        if (!st.active)
            return;
        Event ev = Event::StopEmpty;
        const uint32_t now = millis();
        if (cfg.resume_after_refill && timeAfterOrEqual_(st.end_ms, now))
        {
            st.remaining_ms = st.end_ms - now;
            st.paused = true;
            _logs.warn(F("WATER"),
                       F("pause: empty tank: rule: %u tank: %u remaining_ms: %lu resume_level: %s"),
                       (unsigned)cfg.id, (unsigned)cfg.tank_id, (unsigned long)st.remaining_ms,
                       cfg.resume_level == 2 ? "full" : (cfg.resume_level == 1 ? "mid" : "low"));
            ev = Event::PauseEmpty;
        }
        else
        {
            st.remaining_ms = 0;
            st.paused = false;
            _logs.warn(F("WATER"), F("stop: empty tank: rule: %u tank: %u"),
                       (unsigned)cfg.id, (unsigned)cfg.tank_id);
        }
        st.active = false;
        st.end_ms = 0;
        writePort_(cfg.port, false);
        notifyEvent_(ev, cfg, st);
        _runtime_dirty = true;
    }

    void resumeAfterRefill_(const RuleConfig &cfg, RuleState &st)
    {
        if (!st.paused || st.remaining_ms == 0)
            return;
        st.paused = false;
        st.active = true;
        st.end_ms = millis() + st.remaining_ms;
        st.remaining_ms = 0;
        writePort_(cfg.port, true);
        _logs.info(F("WATER"), F("resume: rule: %u port: %u tank: %u"),
                   (unsigned)cfg.id, (unsigned)cfg.port, (unsigned)cfg.tank_id);
        notifyEvent_(Event::Resume, cfg, st);
        _runtime_dirty = true;
    }

    void writePort_(uint8_t port, bool on)
    {
        if (port == kInvalidPort)
            return;
        _gpio.writeDyn(port, on);
    }

    bool isTankEmpty_(const RuleConfig &cfg) const
    {
        if (cfg.tank_id == 0)
            return false;
        const TankController::TankState *st = _tanks.state(cfg.tank_id);
        if (!st)
            return true;
        const bool empty = !(st->level_low || st->level_mid || st->level_full);
        return !st->levels_ok || empty;
    }

    bool isResumeLevelReached_(const RuleConfig &cfg) const
    {
        if (cfg.tank_id == 0)
            return true;
        const TankController::TankState *st = _tanks.state(cfg.tank_id);
        if (!st || !st->levels_ok)
            return false;
        if (cfg.resume_level == 2)
            return st->level_full;
        if (cfg.resume_level == 1)
            return st->level_mid || st->level_full;
        return st->level_low || st->level_mid || st->level_full;
    }

    bool indexById_(size_t id, size_t &out) const
    {
        if (id == 0)
            return false;
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            if (_cfg[i].id == id)
            {
                out = i;
                return true;
            }
        }
        return false;
    }

    void notifyEvent_(Event ev, const RuleConfig &cfg, const RuleState &st)
    {
        if (_event_cb)
            _event_cb(_event_ctx, ev, cfg, st);
    }
};
