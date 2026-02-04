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

class SepticController
{
public:
    static constexpr size_t kSepticCount = 1;
    static constexpr uint8_t kInvalidPort = 0xFF;

    struct SepticConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        bool monitoring_on = false;
        uint8_t warning_port = kInvalidPort;
        uint8_t alarm_port = kInvalidPort;
        uint8_t relay_warning = kInvalidPort;
        uint8_t relay_alarm = kInvalidPort;
        String name;
    };

    struct SepticState
    {
        bool warning = false;
        bool alarm = false;
        bool relay_warning = false;
        bool relay_alarm = false;
        bool last_warning = false;
        bool last_alarm = false;
    };

    using DetectHandler = void (*)(void *ctx, uint8_t septic_id, const String &name, bool is_alarm);

    SepticController(Gpio &gpio, Logger &logs, TelegramBot &bot, TelegramAllowedUsersProvider &users)
        : _gpio(gpio), _logs(logs), _tgbot(bot), _tgusers(users)
    {
        reset_();
    }

    bool begin()
    {
        if (!_controller_enabled)
        {
            _logs.info(F("SEPTIC"), F("Controller disabled"));
            return true;
        }
        for (size_t i = 0; i < kSepticCount; ++i)
        {
            SepticConfig &cfg = _cfg[i];
            SepticState &st = _state[i];
            if (!cfg.enabled)
                continue;
            setupInputs_(cfg);
            setupOutputs_(cfg, st);
            readLevels_(cfg, st);
            updateRelays_(cfg, st);
            st.last_warning = st.warning;
            st.last_alarm = st.alarm;
        }
        _logs.info(F("SEPTIC"), F("Init done"));
        return true;
    }

    void task()
    {
        if (!_controller_enabled)
            return;
        for (size_t i = 0; i < kSepticCount; ++i)
        {
            SepticConfig &cfg = _cfg[i];
            SepticState &st = _state[i];
            if (!cfg.enabled)
                continue;
            if (!cfg.monitoring_on)
            {
                writeRelay_(cfg.relay_warning, false);
                writeRelay_(cfg.relay_alarm, false);
                st = SepticState{};
                continue;
            }
            const SepticState prev = st;
            readLevels_(cfg, st);
            updateRelays_(cfg, st);
            logLevelChange_(cfg, prev, st);
            logRelayChange_(cfg, prev, st);
            if (!prev.warning && st.warning)
            {
                notifyDetectEvent_(cfg, false);
                notifyLevel_(cfg, false);
            }
            if (!prev.alarm && st.alarm)
            {
                notifyDetectEvent_(cfg, true);
                notifyLevel_(cfg, true);
            }
            st.last_warning = st.warning;
            st.last_alarm = st.alarm;
        }
    }

    void applyConfig(JsonArrayConst septic)
    {
        reset_();
        size_t idx = 0;
        for (JsonVariantConst v : septic)
        {
            if (idx >= kSepticCount)
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
            SepticConfig &cfg = _cfg[dst];
            cfg.id = id;
            bool enabled_set = false;
            if (obj["enabled"].is<bool>())
            {
                cfg.enabled = obj["enabled"].as<bool>();
                enabled_set = true;
            }
            if (obj["monitor"].is<bool>())
                cfg.monitoring_on = obj["monitor"].as<bool>();
            if (obj["name"].is<const char *>())
                cfg.name = obj["name"].as<const char *>();
            parsePort_(obj["warning"], cfg.warning_port);
            parsePort_(obj["alarm"], cfg.alarm_port);
            parsePort_(obj["relay_warning"], cfg.relay_warning);
            parsePort_(obj["relay_alarm"], cfg.relay_alarm);
            if (!enabled_set)
                cfg.enabled = true;
            ++idx;
        }
    }

    void serialize(JsonArray out) const
    {
        for (size_t i = 0; i < kSepticCount; ++i)
        {
            const SepticConfig &cfg = _cfg[i];
            if (!cfg.enabled)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = cfg.id;
            obj["enabled"] = cfg.enabled;
            if (!cfg.monitoring_on)
                obj["monitor"] = false;
            if (cfg.name.length())
                obj["name"] = cfg.name;
            if (cfg.warning_port != kInvalidPort)
                obj["warning"] = cfg.warning_port;
            if (cfg.alarm_port != kInvalidPort)
                obj["alarm"] = cfg.alarm_port;
            if (cfg.relay_warning != kInvalidPort)
                obj["relay_warning"] = cfg.relay_warning;
            if (cfg.relay_alarm != kInvalidPort)
                obj["relay_alarm"] = cfg.relay_alarm;
        }
    }

    bool controllerEnabled() const { return _controller_enabled; }
    void setControllerEnabled(bool enabled)
    {
        if (_controller_enabled == enabled)
            return;
        _controller_enabled = enabled;
        if (!_controller_enabled)
        {
            for (size_t i = 0; i < kSepticCount; ++i)
            {
                SepticConfig &cfg = _cfg[i];
                SepticState &st = _state[i];
                if (!cfg.enabled)
                    continue;
                st = SepticState{};
                writeRelay_(cfg.relay_warning, false);
                writeRelay_(cfg.relay_alarm, false);
            }
            return;
        }
        _logs.info(F("SEPTIC"), F("controller: enabled"));
        begin();
    }

    bool setEnabled(size_t id, bool enabled)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        SepticConfig &cfg = _cfg[idx];
        SepticState &st = _state[idx];
        cfg.enabled = enabled;
        st = SepticState{};
        if (_controller_enabled && cfg.enabled)
        {
            setupInputs_(cfg);
            setupOutputs_(cfg, st);
            readLevels_(cfg, st);
            updateRelays_(cfg, st);
        }
        return true;
    }

    bool setMonitoring(size_t id, bool on)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        SepticConfig &cfg = _cfg[idx];
        SepticState &st = _state[idx];
        cfg.monitoring_on = on;
        if (_controller_enabled && cfg.enabled)
        {
            if (!cfg.monitoring_on)
            {
                writeRelay_(cfg.relay_warning, false);
                writeRelay_(cfg.relay_alarm, false);
                st = SepticState{};
            }
            else
            {
                setupInputs_(cfg);
                readLevels_(cfg, st);
                updateRelays_(cfg, st);
            }
        }
        return true;
    }

    bool setName(size_t id, const String &name)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        _cfg[idx].name = name;
        return true;
    }

    bool setWarningPort(size_t id, uint8_t port)
    {
        return setLevelPort_(id, port, &SepticConfig::warning_port);
    }

    bool setAlarmPort(size_t id, uint8_t port)
    {
        return setLevelPort_(id, port, &SepticConfig::alarm_port);
    }

    bool setWarningRelay(size_t id, uint8_t port)
    {
        return setRelayPort_(id, port, &SepticConfig::relay_warning);
    }

    bool setAlarmRelay(size_t id, uint8_t port)
    {
        return setRelayPort_(id, port, &SepticConfig::relay_alarm);
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

    void notifyRemoteLevel(const String &source, uint8_t septic_id, const String &name, bool is_alarm)
    {
        if (!_notify_enabled)
            return;
        String msg = F("Септик: уровень ");
        msg += is_alarm ? F("ALARM") : F("WARNING");
        if (source.length())
        {
            msg += F(" [");
            msg += source;
            msg += F("]");
        }
        if (septic_id > 0)
        {
            msg += F(" #");
            msg += String((unsigned)septic_id);
        }
        if (name.length())
        {
            msg += F(" (");
            msg += name;
            msg += F(")");
        }
        sendTgNotify_(msg);
    }

    const SepticConfig *configByIndex(size_t idx) const
    {
        if (idx >= kSepticCount)
            return nullptr;
        return &_cfg[idx];
    }

    const SepticState *stateByIndex(size_t idx) const
    {
        if (idx >= kSepticCount)
            return nullptr;
        return &_state[idx];
    }

private:
    Gpio &_gpio;
    Logger &_logs;
    TelegramBot &_tgbot;
    TelegramAllowedUsersProvider &_tgusers;

    SepticConfig _cfg[kSepticCount]{};
    SepticState _state[kSepticCount]{};
    bool _controller_enabled = false;
    bool _notify_enabled = true;
    DetectHandler _detect_cb = nullptr;
    void *_detect_ctx = nullptr;

    void reset_()
    {
        for (size_t i = 0; i < kSepticCount; ++i)
        {
            _cfg[i] = SepticConfig{};
            _cfg[i].id = (uint8_t)(i + 1);
            _state[i] = SepticState{};
        }
    }

    static bool indexById_(uint8_t id, size_t &out)
    {
        if (id == 0 || id > kSepticCount)
            return false;
        out = (size_t)(id - 1);
        return true;
    }

    void setupInputs_(const SepticConfig &cfg)
    {
        setupInput_(cfg.warning_port);
        setupInput_(cfg.alarm_port);
    }

    void setupInput_(uint8_t port)
    {
        if (port == kInvalidPort)
            return;
        const PortIO::PortMode mode = kLevelPullup ? PortIO::PortMode::InputPullUp : PortIO::PortMode::Input;
        _gpio.pinModeDyn(port, mode);
    }

    void setupOutputs_(const SepticConfig &cfg, SepticState &st)
    {
        setupRelay_(cfg.relay_warning, st.relay_warning);
        setupRelay_(cfg.relay_alarm, st.relay_alarm);
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

    void readLevels_(const SepticConfig &cfg, SepticState &st)
    {
        st.warning = readInput_(cfg.warning_port);
        st.alarm = readInput_(cfg.alarm_port);
    }

    bool setLevelPort_(size_t id, uint8_t port, uint8_t SepticConfig::*field)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        SepticConfig &cfg = _cfg[idx];
        SepticState &st = _state[idx];
        cfg.*field = port;
        if (_controller_enabled && cfg.enabled)
        {
            setupInputs_(cfg);
            readLevels_(cfg, st);
            updateRelays_(cfg, st);
        }
        return true;
    }

    bool setRelayPort_(size_t id, uint8_t port, uint8_t SepticConfig::*field)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        SepticConfig &cfg = _cfg[idx];
        SepticState &st = _state[idx];
        cfg.*field = port;
        if (_controller_enabled && cfg.enabled)
        {
            setupOutputs_(cfg, st);
            updateRelays_(cfg, st);
        }
        return true;
    }

    bool readInput_(uint8_t port)
    {
        if (port == kInvalidPort)
            return false;
        bool raw = false;
        if (!_gpio.readDyn(port, raw))
            return false;
        return raw;
    }

    void updateRelays_(const SepticConfig &cfg, SepticState &st)
    {
        st.relay_warning = st.warning;
        st.relay_alarm = st.alarm;
        writeRelay_(cfg.relay_warning, st.relay_warning);
        writeRelay_(cfg.relay_alarm, st.relay_alarm);
    }

    void writeRelay_(uint8_t port, bool on)
    {
        if (port == kInvalidPort)
            return;
        const bool out = kRelayInvert ? !on : on;
        _gpio.writeDyn(port, out);
    }

    void logLevelChange_(const SepticConfig &cfg, const SepticState &prev, const SepticState &curr)
    {
        if (prev.warning == curr.warning && prev.alarm == curr.alarm)
            return;
        _logs.info(F("SEPTIC"), F("id: %u warning: %u alarm: %u"),
                   (unsigned)cfg.id,
                   curr.warning ? 1u : 0u,
                   curr.alarm ? 1u : 0u);
    }

    void logRelayChange_(const SepticConfig &cfg, const SepticState &prev, const SepticState &curr)
    {
        if (prev.relay_warning != curr.relay_warning)
            _logs.info(F("SEPTIC"), F("id: %u warning_lamp: %s"),
                       (unsigned)cfg.id, curr.relay_warning ? "on" : "off");
        if (prev.relay_alarm != curr.relay_alarm)
            _logs.info(F("SEPTIC"), F("id: %u alarm_lamp: %s"),
                       (unsigned)cfg.id, curr.relay_alarm ? "on" : "off");
    }

    void notifyLevel_(const SepticConfig &cfg, bool is_alarm)
    {
        if (!_notify_enabled)
            return;
        String msg = F("Септик: уровень ");
        msg += is_alarm ? F("ALARM") : F("WARNING");
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

    void notifyDetectEvent_(const SepticConfig &cfg, bool is_alarm)
    {
        if (_detect_cb)
            _detect_cb(_detect_ctx, cfg.id, cfg.name, is_alarm);
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

    static constexpr bool kRelayInvert = false;
    static constexpr bool kLevelPullup = true;
};


