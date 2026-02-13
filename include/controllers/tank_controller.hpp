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
#include <string.h>

#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/telegram/telegram_allowed_users.hpp"
#include "hal/gpio/gpio.hpp"
#include "hal/gpio/portio.hpp"
#include "utils/logger.hpp"

class TankController
{
public:
    static constexpr size_t kTankCount = 20;
    static constexpr uint8_t kInvalidPort = 0xFF;

    struct TankConfig
    {
        uint8_t id = 1;
        bool enabled = false;
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

    TankController(Gpio &gpio, Logger &logs, TelegramBot &bot, TelegramAllowedUsersProvider &users)
        : _gpio(gpio), _logs(logs), _tgbot(bot), _tgusers(users)
    {
        reset_();
    }

    bool begin()
    {
        if (!_controller_enabled)
            return true;
        for (size_t i = 0; i < kTankCount; ++i)
        {
            TankConfig &cfg = _cfg[i];
            TankState &st = _state[i];
            if (!cfg.enabled)
                continue;
            setupInputs_(cfg);
            setupOutputs_(cfg, st);
            readLevels_(cfg, st);
            if (st.levels_ok)
                updateControl_(cfg, st);
            else
                writeAllOff_(cfg, st);
            if (st.levels_ok)
                st.last_empty = isEmpty_(st);
        }
        _logs.info(F("TANK"), F("Init done"));
        return true;
    }

    void task()
    {
        if (!_controller_enabled)
            return;
        for (size_t i = 0; i < kTankCount; ++i)
        {
            TankConfig &cfg = _cfg[i];
            TankState &st = _state[i];
            if (!cfg.enabled)
                continue;
            if (!cfg.power_on)
            {
                const TankState prev = st;
                writeAllOff_(cfg, st);
                logRelayChange_(cfg, prev, st);
                continue;
            }
            const TankState prev = st;
            readLevels_(cfg, st);
            if (!st.levels_ok)
            {
                const TankState prev_relays = st;
                writeAllOff_(cfg, st);
                logRelayChange_(cfg, prev_relays, st);
                continue;
            }
            updateControl_(cfg, st);
            logLevelChange_(cfg, prev, st);
            logRelayChange_(cfg, prev, st);
            const bool empty = isEmpty_(st);
            if (empty && !st.last_empty)
            {
                const uint32_t now = millis();
                const bool allow_event = (st.last_empty_event_ms == 0) ||
                                         ((uint32_t)(now - st.last_empty_event_ms) >= kEmptyEventDebounceMs);
                if (allow_event)
                {
                    notifyDetectEvent_(cfg, true);
                    notifyEmpty_(cfg);
                    st.last_empty_event_ms = now;
                }
            }
            st.last_empty = empty;
        }
    }

    void applyConfig(JsonArrayConst tanks)
    {
        reset_();
        size_t idx = 0;
        for (JsonVariantConst v : tanks)
        {
            if (idx >= kTankCount)
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
            TankConfig &cfg = _cfg[dst];
            cfg.id = id;
            bool enabled_set = false;
            if (obj["enabled"].is<bool>())
            {
                cfg.enabled = obj["enabled"].as<bool>();
                enabled_set = true;
            }
            if (obj["power_on"].is<bool>())
                cfg.power_on = obj["power_on"].as<bool>();
            if (obj["name"].is<const char *>())
                cfg.name = obj["name"].as<const char *>();
            parsePort_(obj["low"], cfg.level_low);
            parsePort_(obj["mid"], cfg.level_mid);
            parsePort_(obj["full"], cfg.level_full);
            parsePort_(obj["valve"], cfg.relay_valve);
            parsePort_(obj["pump"], cfg.relay_pump);
            parsePort_(obj["alarm"], cfg.relay_alarm);
            if (!enabled_set)
                cfg.enabled = true;
            ++idx;
        }
    }

    void serialize(JsonArray out) const
    {
        for (size_t i = 0; i < kTankCount; ++i)
        {
            const TankConfig &cfg = _cfg[i];
            if (!cfg.enabled)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = cfg.id;
            obj["enabled"] = cfg.enabled;
            obj["power_on"] = cfg.power_on;
            if (cfg.name.length())
                obj["name"] = cfg.name;
            if (cfg.level_low != kInvalidPort)
                obj["low"] = cfg.level_low;
            if (cfg.level_mid != kInvalidPort)
                obj["mid"] = cfg.level_mid;
            if (cfg.level_full != kInvalidPort)
                obj["full"] = cfg.level_full;
            if (cfg.relay_valve != kInvalidPort)
                obj["valve"] = cfg.relay_valve;
            if (cfg.relay_pump != kInvalidPort)
                obj["pump"] = cfg.relay_pump;
            if (cfg.relay_alarm != kInvalidPort)
                obj["alarm"] = cfg.relay_alarm;
        }
    }

    void buildSnapshot(uint8_t *power_mask, size_t bytes) const
    {
        if (!power_mask)
            return;
        memset(power_mask, 0, bytes);
        for (size_t i = 0; i < kTankCount; ++i)
        {
            const TankConfig &cfg = _cfg[i];
            if (!cfg.enabled)
                continue;
            const size_t byte = i / 8;
            const uint8_t bit = (uint8_t)(1u << (i % 8));
            if (byte >= bytes)
                break;
            if (cfg.power_on)
                power_mask[byte] |= bit;
        }
    }

    void applySnapshot(const uint8_t *power_mask, size_t bytes)
    {
        if (!power_mask)
            return;
        for (size_t i = 0; i < kTankCount; ++i)
        {
            TankConfig &cfg = _cfg[i];
            TankState &st = _state[i];
            if (!cfg.enabled)
                continue;
            const size_t byte = i / 8;
            const uint8_t bit = (uint8_t)(1u << (i % 8));
            if (byte >= bytes)
                break;
            const bool on = (power_mask[byte] & bit) != 0;
            if (cfg.power_on == on)
                continue;
            cfg.power_on = on;
            _logs.info(F("TANK"), F("id: %u power: %s (restore)"),
                       (unsigned)cfg.id, on ? "on" : "off");
            if (!_controller_enabled)
                continue;
            if (!cfg.power_on)
            {
                const TankState prev = st;
                writeAllOff_(cfg, st);
                logRelayChange_(cfg, prev, st);
                continue;
            }
            const TankState prev = st;
            readLevels_(cfg, st);
            if (!st.levels_ok)
            {
                const TankState prev_relays = st;
                writeAllOff_(cfg, st);
                logRelayChange_(cfg, prev_relays, st);
                continue;
            }
            updateControl_(cfg, st);
            st.last_empty = isEmpty_(st);
            logLevelChange_(cfg, prev, st);
            logRelayChange_(cfg, prev, st);
        }
    }

    bool takeDirty()
    {
        if (!_dirty)
            return false;
        _dirty = false;
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
            for (size_t i = 0; i < kTankCount; ++i)
            {
                TankConfig &cfg = _cfg[i];
                TankState &st = _state[i];
                const TankState prev = st;
                writeAllOff_(cfg, st);
                logRelayChange_(cfg, prev, st);
            }
            reset_();
            return;
        }
        _logs.info(F("TANK"), F("controller: enabled"));
        for (size_t i = 0; i < kTankCount; ++i)
        {
            TankConfig &cfg = _cfg[i];
            TankState &st = _state[i];
            if (!cfg.enabled)
                continue;
            st = TankState{};
            setupInputs_(cfg);
            setupOutputs_(cfg, st);
            readLevels_(cfg, st);
            if (cfg.power_on && st.levels_ok)
                updateControl_(cfg, st);
            else if (!st.levels_ok)
                writeAllOff_(cfg, st);
            if (st.levels_ok)
                st.last_empty = isEmpty_(st);
        }
    }

    bool setEnabled(size_t id, bool enabled)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        TankConfig &cfg = _cfg[idx];
        TankState &st = _state[idx];
        if (!enabled)
        {
            const TankState prev = st;
            writeAllOff_(cfg, st);
            logRelayChange_(cfg, prev, st);
            const uint8_t saved_id = cfg.id;
            cfg = TankConfig{};
            cfg.id = saved_id;
            cfg.enabled = false;
            cfg.power_on = false;
            st = TankState{};
            _logs.info(F("TANK"), F("id: %u enabled: false"), (unsigned)cfg.id);
            return true;
        }
        cfg.enabled = true;
        if (!_controller_enabled)
            return true;
        st = TankState{};
        setupInputs_(cfg);
        setupOutputs_(cfg, st);
        readLevels_(cfg, st);
        if (cfg.power_on && st.levels_ok)
            updateControl_(cfg, st);
        else if (!st.levels_ok)
            writeAllOff_(cfg, st);
        if (st.levels_ok)
            st.last_empty = isEmpty_(st);
        _logs.info(F("TANK"), F("id: %u enabled: true"), (unsigned)cfg.id);
        return true;
    }

    bool setPower(size_t id, bool on)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        TankConfig &cfg = _cfg[idx];
        TankState &st = _state[idx];
        if (cfg.power_on == on)
            return true;
        cfg.power_on = on;
        _dirty = true;
        _logs.info(F("TANK"), F("id: %u power: %s"), (unsigned)cfg.id, on ? "on" : "off");
        if (!_controller_enabled || !cfg.enabled)
            return true;
        if (!on)
        {
            const TankState prev = st;
            writeAllOff_(cfg, st);
            logRelayChange_(cfg, prev, st);
            return true;
        }
        readLevels_(cfg, st);
        if (!st.levels_ok)
        {
            const TankState prev = st;
            writeAllOff_(cfg, st);
            logRelayChange_(cfg, prev, st);
            return true;
        }
        updateControl_(cfg, st);
        st.last_empty = isEmpty_(st);
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

    bool setLevelLow(size_t id, uint8_t port)
    {
        return setLevelPort_(id, port, &TankConfig::level_low);
    }

    bool setLevelMid(size_t id, uint8_t port)
    {
        return setLevelPort_(id, port, &TankConfig::level_mid);
    }

    bool setLevelFull(size_t id, uint8_t port)
    {
        return setLevelPort_(id, port, &TankConfig::level_full);
    }

    bool setValveRelay(size_t id, uint8_t port)
    {
        return setRelayPort_(id, port, &TankConfig::relay_valve, 0);
    }

    bool setPumpRelay(size_t id, uint8_t port)
    {
        return setRelayPort_(id, port, &TankConfig::relay_pump, 1);
    }

    bool setAlarmRelay(size_t id, uint8_t port)
    {
        return setRelayPort_(id, port, &TankConfig::relay_alarm, 2);
    }

    void setDetectHandler(DetectHandler cb, void *ctx)
    {
        _detect_cb = cb;
        _detect_ctx = ctx;
    }

    void setNotifyEnabled(bool enabled)
    {
        _notify_enabled = enabled;
    }

    void notifyRemoteEmpty(const String &source, uint8_t tank_id, const String &name)
    {
        if (!_notify_enabled)
            return;
        String msg = F("Бак пустой");
        if (source.length())
        {
            msg += F(" [");
            msg += source;
            msg += F("]");
        }
        if (tank_id > 0)
        {
            msg += F(" #");
            msg += String((unsigned)tank_id);
        }
        if (name.length())
        {
            msg += F(" (");
            msg += name;
            msg += F(")");
        }
        sendTgNotify_(msg);
    }


    const TankConfig *config(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_cfg[idx];
    }

    const TankState *state(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_state[idx];
    }

    const TankConfig *configByIndex(size_t idx) const
    {
        if (idx >= kTankCount)
            return nullptr;
        return &_cfg[idx];
    }

    const TankState *stateByIndex(size_t idx) const
    {
        if (idx >= kTankCount)
            return nullptr;
        return &_state[idx];
    }

private:
    Gpio &_gpio;
    Logger &_logs;
    TelegramBot &_tgbot;
    TelegramAllowedUsersProvider &_tgusers;
    TankConfig _cfg[kTankCount]{};
    TankState _state[kTankCount]{};
    bool _controller_enabled = false;
    bool _notify_enabled = true;
    bool _dirty = false;
    DetectHandler _detect_cb = nullptr;
    void *_detect_ctx = nullptr;

    void reset_()
    {
        for (size_t i = 0; i < kTankCount; ++i)
        {
            TankConfig &cfg = _cfg[i];
            cfg = TankConfig{};
            cfg.id = (uint8_t)(i + 1);
            cfg.enabled = false;
            cfg.power_on = false;
            TankState &st = _state[i];
            st = TankState{};
        }
    }

    static bool parsePort_(JsonVariantConst v, uint8_t &out)
    {
        if (v.is<unsigned>())
        {
            const unsigned val = v.as<unsigned>();
            if (val <= 0xFFu)
            {
                out = static_cast<uint8_t>(val);
                return true;
            }
        }
        return false;
    }

    static bool indexById_(uint8_t id, size_t &out)
    {
        if (id == 0 || id > kTankCount)
            return false;
        out = (size_t)(id - 1);
        return true;
    }

    void setupInputs_(const TankConfig &cfg)
    {
        setupInput_(cfg.level_low);
        setupInput_(cfg.level_mid);
        setupInput_(cfg.level_full);
    }

    void setupInput_(uint8_t port)
    {
        if (port == kInvalidPort)
            return;
        const PortIO::PortMode mode = kLevelPullup ? PortIO::PortMode::InputPullUp : PortIO::PortMode::Input;
        _gpio.pinModeDyn(port, mode);
    }

    bool setLevelPort_(size_t id, uint8_t port, uint8_t TankConfig::*field)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        TankConfig &cfg = _cfg[idx];
        TankState &st = _state[idx];
        cfg.*field = port;
        if (_controller_enabled && cfg.enabled)
        {
            setupInputs_(cfg);
            readLevels_(cfg, st);
            if (st.levels_ok)
                updateControl_(cfg, st);
            else
                writeAllOff_(cfg, st);
        }
        return true;
    }

    bool setRelayPort_(size_t id, uint8_t port, uint8_t TankConfig::*field, uint8_t)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        TankConfig &cfg = _cfg[idx];
        TankState &st = _state[idx];
        cfg.*field = port;
        if (_controller_enabled && cfg.enabled)
        {
            setupOutputs_(cfg, st);
            if (st.levels_ok)
                updateControl_(cfg, st);
            else
                writeAllOff_(cfg, st);
        }
        return true;
    }

    void setupOutputs_(const TankConfig &cfg, TankState &st)
    {
        setupRelay_(cfg.relay_valve, st.valve_on);
        setupRelay_(cfg.relay_pump, st.pump_on);
        setupRelay_(cfg.relay_alarm, st.alarm_on);
    }

    void setupRelay_(uint8_t port, bool &state)
    {
        if (port == kInvalidPort)
            return;
        if (!_gpio.pinModeDyn(port, PortIO::PortMode::Output))
            return;
        state = false;
        writeRelay_(port, state);
    }

    void readLevels_(const TankConfig &cfg, TankState &st)
    {
        bool low = false;
        bool mid = false;
        bool full = false;
        const bool ok_low = readInput_(cfg.level_low, low);
        const bool ok_mid = readInput_(cfg.level_mid, mid);
        const bool ok_full = readInput_(cfg.level_full, full);
        if (ok_low)
            st.level_low = low;
        if (ok_mid)
            st.level_mid = mid;
        if (ok_full)
            st.level_full = full;
        st.levels_ok = ok_low && ok_mid && ok_full;
        const uint32_t now = millis();
        if (!st.levels_ok)
        {
            if (st.levels_ok_prev || (uint32_t)(now - st.last_level_err_ms) >= kLevelErrLogMs)
            {
                _logs.warn(F("TANK"),
                           F("id: %u level read failed (low:%u mid:%u full:%u)"),
                           (unsigned)cfg.id,
                           ok_low ? 1u : 0u,
                           ok_mid ? 1u : 0u,
                           ok_full ? 1u : 0u);
                st.last_level_err_ms = now;
            }
        }
        else if (!st.levels_ok_prev)
        {
            _logs.info(F("TANK"), F("id: %u level read ok"), (unsigned)cfg.id);
        }
        st.levels_ok_prev = st.levels_ok;
    }

    bool readInput_(uint8_t port, bool &out)
    {
        if (port == kInvalidPort)
            return false;
        bool raw = false;
        if (!_gpio.readDyn(port, raw))
            return false;
        out = raw;
        return true;
    }

    void updateControl_(const TankConfig &cfg, TankState &st)
    {
        const bool empty = isEmpty_(st);
        const bool full = st.level_full;
        st.pump_on = !empty;
        st.valve_on = !full;
        st.alarm_on = empty;
        writeRelay_(cfg.relay_pump, st.pump_on);
        writeRelay_(cfg.relay_valve, st.valve_on);
        writeRelay_(cfg.relay_alarm, st.alarm_on);
    }

    void logLevelChange_(const TankConfig &cfg, const TankState &prev, const TankState &curr)
    {
        if (prev.level_low == curr.level_low &&
            prev.level_mid == curr.level_mid &&
            prev.level_full == curr.level_full)
            return;
        _logs.info(F("TANK"), F("id: %u low: %u mid: %u full: %u"),
                   (unsigned)cfg.id,
                   curr.level_low ? 1u : 0u,
                   curr.level_mid ? 1u : 0u,
                   curr.level_full ? 1u : 0u);
    }

    void logRelayChange_(const TankConfig &cfg, const TankState &prev, const TankState &curr)
    {
        if (prev.pump_on != curr.pump_on)
            _logs.info(F("TANK"), F("id: %u pump: %s"),
                       (unsigned)cfg.id, curr.pump_on ? "on" : "off");
        if (prev.valve_on != curr.valve_on)
            _logs.info(F("TANK"), F("id: %u valve: %s"),
                       (unsigned)cfg.id, curr.valve_on ? "on" : "off");
    }

    static bool isEmpty_(const TankState &st)
    {
        return !(st.level_low || st.level_mid || st.level_full);
    }

    void writeAllOff_(const TankConfig &cfg, TankState &st)
    {
        st.valve_on = false;
        st.pump_on = false;
        st.alarm_on = false;
        writeRelay_(cfg.relay_pump, st.pump_on);
        writeRelay_(cfg.relay_valve, st.valve_on);
        writeRelay_(cfg.relay_alarm, st.alarm_on);
    }

    void writeRelay_(uint8_t port, bool on)
    {
        if (port == kInvalidPort)
            return;
        _gpio.writeDyn(port, on);
    }

    void notifyEmpty_(const TankConfig &cfg)
    {
        if (!_notify_enabled)
            return;
        String msg = F("Бак пустой: ");
        msg += String((unsigned)cfg.id);
        if (cfg.name.length())
        {
            msg += F(" (");
            msg += cfg.name;
            msg += F(")");
        }
        sendTgNotify_(msg);
    }

    void sendTgNotify_(const String &msg)
    {
        const auto users = _tgusers.allowedUsers();
        if (users.empty())
            return;
        for (size_t i = 0; i < users.size; ++i)
        {
            const auto &user = users[i];
            if (!user.enabled || !user.is_notify || user.chat_id == 0)
                continue;
            _tgbot.sendText(user.chat_id, msg);
        }
    }

    void notifyDetectEvent_(const TankConfig &cfg, bool empty)
    {
        if (_detect_cb)
            _detect_cb(_detect_ctx, cfg.id, cfg.name, empty);
    }

    static constexpr bool kLevelPullup = true;
    static constexpr uint32_t kLevelErrLogMs = 5000;
    static constexpr uint32_t kEmptyEventDebounceMs = 10000;
};
