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

#include "boards/board_profile.hpp"
#include "core/network/telegram/telegram_allowed_users.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/gpio/gpio.hpp"
#include "hal/gpio/portio.hpp"
#include "hal/ibutton.hpp"
#include "utils/logger.hpp"

class SecurityController
{
public:
    static constexpr size_t kSensorCount = 72;
    static constexpr size_t kKeyCount = 10;
    static constexpr uint8_t kInvalidPort = 0xFF;

    enum class SensorType : uint8_t
    {
        Pir = 0,
        Reed
    };

    struct SensorConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        SensorType type = SensorType::Pir;
        uint8_t port = kInvalidPort;
        bool silent = false;
        String name;
    };

    struct SensorState
    {
        bool raw = false;
        bool is_detect = false;
    };

    SecurityController(Gpio &gpio, OneWireManager &ow, Logger &logs,
                       TelegramBot &bot, TelegramAllowedUsersProvider &users)
        : _gpio(gpio), _ow(ow), _logs(logs), _tgbot(bot), _tgusers(users)
    {
        reset_();
    }

    bool begin()
    {
        if (!_controller_enabled)
            return true;
        setupOutputs_();
        initIButton_();
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            SensorConfig &cfg = _cfg[i];
            SensorState &st = _state[i];
            if (!cfg.enabled)
                continue;
            setupSensorInput_(cfg);
            st.raw = readRaw_(cfg);
            st.is_detect = false;
        }
        return true;
    }

    void task()
    {
        if (!_controller_enabled)
            return;
        handleIButton_();
        if (!_armed)
        {
            updateBuzzer_();
            return;
        }
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            SensorConfig &cfg = _cfg[i];
            SensorState &st = _state[i];
            if (!cfg.enabled)
                continue;
            const bool raw = readRaw_(cfg);
            st.raw = raw;
            const bool triggered = isTriggered_(cfg, raw);
            if (triggered && !st.is_detect)
            {
                st.is_detect = true;
                if (!cfg.silent)
                {
                    _alarm_on = true;
                    resetAlarmBuzzer_();
                    updateSiren_();
                }
                logDetect_(cfg);
                notifyDetect_(cfg);
            }
        }
        updateBuzzer_();
    }

    void applyConfig(JsonArrayConst sensors)
    {
        reset_();
        size_t idx = 0;
        for (JsonVariantConst v : sensors)
        {
            if (idx >= kSensorCount)
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
            SensorConfig &cfg = _cfg[dst];
            cfg.id = id;
            bool enabled_set = false;
            if (obj["enabled"].is<bool>())
            {
                cfg.enabled = obj["enabled"].as<bool>();
                enabled_set = true;
            }
            if (obj["port"].is<unsigned>())
            {
                const unsigned raw = obj["port"].as<unsigned>();
                if (raw <= 0xFFu)
                    cfg.port = (uint8_t)raw;
            }
            if (obj["type"].is<const char *>())
            {
                SensorType t;
                if (parseType_(obj["type"].as<const char *>(), t))
                    cfg.type = t;
            }
            if (obj["silent"].is<bool>())
                cfg.silent = obj["silent"].as<bool>();
            if (obj["name"].is<const char *>())
                cfg.name = obj["name"].as<const char *>();
            if (!enabled_set)
                cfg.enabled = true;
            ++idx;
        }
    }

    void applyKeys(JsonArrayConst keys)
    {
        clearKeys_();
        size_t idx = 0;
        for (JsonVariantConst v : keys)
        {
            if (idx >= kKeyCount)
                break;
            if (v.is<const char *>())
            {
                if (parseHexAddr_(v.as<const char *>(), _keys[idx]))
                    _key_set[idx] = true;
            }
            ++idx;
        }
    }

    void setSirenPort(uint8_t port)
    {
        _siren_port = port;
        if (_controller_enabled)
            setupSiren_();
    }

    void serialize(JsonArray out) const
    {
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            const SensorConfig &cfg = _cfg[i];
            if (!cfg.enabled)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = cfg.id;
            obj["enabled"] = cfg.enabled;
            obj["port"] = cfg.port;
            obj["type"] = typeName_(cfg.type);
            if (cfg.silent)
                obj["silent"] = true;
            if (cfg.name.length())
                obj["name"] = cfg.name;
        }
    }

    void serializeKeys(JsonArray out) const
    {
        for (size_t i = 0; i < kKeyCount; ++i)
        {
            if (!_key_set[i])
                continue;
            char hex[17] = {};
            IButton::toHex(_keys[i], hex);
            out.add(hex);
        }
    }

    void buildSnapshot(uint8_t &armed) const
    {
        armed = _armed ? 1 : 0;
    }

    void applySnapshot(uint8_t armed)
    {
        applySnapshot_(armed != 0);
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
            disarm_(true);
            for (size_t i = 0; i < kSensorCount; ++i)
                _state[i] = SensorState{};
            return;
        }
        _logs.info(F("SEC"), F("controller: enabled"));
        setupOutputs_();
        initIButton_();
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            SensorConfig &cfg = _cfg[i];
            SensorState &st = _state[i];
            st = SensorState{};
            if (!cfg.enabled)
                continue;
            setupSensorInput_(cfg);
            st.raw = readRaw_(cfg);
        }
    }

    bool armed() const { return _armed; }
    bool alarmOn() const { return _alarm_on; }
    uint8_t sirenPort() const { return _siren_port; }
    bool arm()
    {
        if (_armed)
            return true;
        if (!_controller_enabled)
            return false;
        arm_();
        return true;
    }
    bool disarm()
    {
        if (!_armed)
            return true;
        disarm_(false);
        return true;
    }
    void clearDetect()
    {
        clearDetect_();
    }
    bool setEnabled(size_t id, bool enabled)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        SensorConfig &cfg = _cfg[idx];
        SensorState &st = _state[idx];
        cfg.enabled = enabled;
        st = SensorState{};
        if (_controller_enabled && cfg.enabled)
        {
            setupSensorInput_(cfg);
            st.raw = readRaw_(cfg);
        }
        return true;
    }
    bool setType(size_t id, SensorType type)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        SensorConfig &cfg = _cfg[idx];
        SensorState &st = _state[idx];
        cfg.type = type;
        st = SensorState{};
        if (_controller_enabled && cfg.enabled)
        {
            setupSensorInput_(cfg);
            st.raw = readRaw_(cfg);
        }
        return true;
    }
    bool setPort(size_t id, uint8_t port)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        SensorConfig &cfg = _cfg[idx];
        SensorState &st = _state[idx];
        cfg.port = port;
        st = SensorState{};
        if (_controller_enabled && cfg.enabled)
        {
            setupSensorInput_(cfg);
            st.raw = readRaw_(cfg);
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
    bool setSilent(size_t id, bool silent)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        _cfg[idx].silent = silent;
        return true;
    }
    bool addKey(const uint8_t addr[8])
    {
        if (!addr)
            return false;
        for (size_t i = 0; i < kKeyCount; ++i)
        {
            if (_key_set[i] && memcmp(_keys[i], addr, 8) == 0)
                return true;
        }
        for (size_t i = 0; i < kKeyCount; ++i)
        {
            if (_key_set[i])
                continue;
            memcpy(_keys[i], addr, 8);
            _key_set[i] = true;
            return true;
        }
        return false;
    }
    bool removeKey(const uint8_t addr[8])
    {
        if (!addr)
            return false;
        for (size_t i = 0; i < kKeyCount; ++i)
        {
            if (!_key_set[i])
                continue;
            if (memcmp(_keys[i], addr, 8) != 0)
                continue;
            memset(_keys[i], 0, sizeof(_keys[i]));
            _key_set[i] = false;
            return true;
        }
        return false;
    }
    void clearKeys()
    {
        clearKeys_();
    }
    size_t keyCount() const
    {
        size_t count = 0;
        for (size_t i = 0; i < kKeyCount; ++i)
        {
            if (_key_set[i])
                ++count;
        }
        return count;
    }
    bool keyByIndex(size_t idx, uint8_t out[8]) const
    {
        if (!out)
            return false;
        size_t seen = 0;
        for (size_t i = 0; i < kKeyCount; ++i)
        {
            if (!_key_set[i])
                continue;
            if (seen == idx)
            {
                memcpy(out, _keys[i], 8);
                return true;
            }
            ++seen;
        }
        return false;
    }
    bool keySlot(size_t idx, uint8_t out[8], bool &enabled) const
    {
        if (idx >= kKeyCount)
            return false;
        enabled = _key_set[idx];
        if (out)
        {
            if (enabled)
                memcpy(out, _keys[idx], 8);
            else
                memset(out, 0, 8);
        }
        return true;
    }
    const SensorConfig *config(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return nullptr;
        return &_cfg[idx];
    }
    const SensorState *state(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return nullptr;
        return &_state[idx];
    }
    const SensorConfig *configByIndex(size_t idx) const
    {
        if (idx >= kSensorCount)
            return nullptr;
        return &_cfg[idx];
    }
    const SensorState *stateByIndex(size_t idx) const
    {
        if (idx >= kSensorCount)
            return nullptr;
        return &_state[idx];
    }

private:
    Gpio &_gpio;
    OneWireManager &_ow;
    Logger &_logs;
    TelegramBot &_tgbot;
    TelegramAllowedUsersProvider &_tgusers;
    IButton _ibutton;
    bool _ibutton_ready = false;

    SensorConfig _cfg[kSensorCount]{};
    SensorState _state[kSensorCount]{};
    uint8_t _keys[kKeyCount][8]{};
    bool _key_set[kKeyCount]{};
    uint8_t _last_key[8]{};
    uint32_t _last_key_ms = 0;

    bool _controller_enabled = false;
    bool _armed = false;
    bool _alarm_on = false;
    uint8_t _siren_port = kInvalidPort;
    bool _dirty = false;

    uint8_t _beep_remaining = 0;
    uint16_t _beep_on_ms = 0;
    uint16_t _beep_off_ms = 0;
    bool _beep_state_on = false;
    uint32_t _beep_next_ms = 0;

    void reset_()
    {
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            SensorConfig &cfg = _cfg[i];
            cfg = SensorConfig{};
            cfg.id = (uint8_t)(i + 1);
            cfg.enabled = false;
            cfg.type = SensorType::Pir;
            cfg.port = kInvalidPort;
            _state[i] = SensorState{};
        }
        clearKeys_();
        _siren_port = kInvalidPort;
        _armed = false;
        _alarm_on = false;
        _beep_remaining = 0;
    }

    void clearKeys_()
    {
        memset(_keys, 0, sizeof(_keys));
        memset(_key_set, 0, sizeof(_key_set));
        memset(_last_key, 0, sizeof(_last_key));
        _last_key_ms = 0;
    }

    static bool indexById_(uint8_t id, size_t &out)
    {
        if (id == 0 || id > kSensorCount)
            return false;
        out = (size_t)(id - 1);
        return true;
    }

    static const char *typeName_(SensorType t)
    {
        switch (t)
        {
        case SensorType::Pir:
            return "pir";
        case SensorType::Reed:
            return "reed";
        default:
            return "pir";
        }
    }

    static bool parseType_(const char *s, SensorType &out)
    {
        if (!s)
            return false;
        String t = s;
        t.toLowerCase();
        if (t == "pir")
        {
            out = SensorType::Pir;
            return true;
        }
        if (t == "reed")
        {
            out = SensorType::Reed;
            return true;
        }
        return false;
    }

    void setupOutputs_()
    {
        setupBuzzer_();
        setupAlarmLed_();
        setupSiren_();
        updateAlarmLed_();
        updateSiren_();
    }

    void setupBuzzer_()
    {
        _gpio.pinModeDyn(ActiveBoardProfile::BUZZER_PIN, PortIO::PortMode::Output);
        _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, false);
    }

    void setupAlarmLed_()
    {
        _gpio.pinModeDyn(ActiveBoardProfile::ALARM_LED_PIN, PortIO::PortMode::Output);
        _gpio.writeDyn(ActiveBoardProfile::ALARM_LED_PIN, false);
    }

    void setupSiren_()
    {
        if (_siren_port == kInvalidPort)
            return;
        _gpio.pinModeDyn(_siren_port, PortIO::PortMode::Output);
        _gpio.writeDyn(_siren_port, false);
    }

    void updateAlarmLed_()
    {
        _gpio.writeDyn(ActiveBoardProfile::ALARM_LED_PIN, _armed);
    }

    void updateSiren_()
    {
        if (_siren_port == kInvalidPort)
            return;
        _gpio.writeDyn(_siren_port, _alarm_on);
    }

    void setupSensorInput_(const SensorConfig &cfg)
    {
        if (cfg.port == kInvalidPort)
            return;
        const PortIO::PortMode mode = (cfg.type == SensorType::Reed)
                                          ? PortIO::PortMode::InputPullUp
                                          : PortIO::PortMode::Input;
        _gpio.pinModeDyn(cfg.port, mode);
    }

    bool readRaw_(const SensorConfig &cfg)
    {
        if (cfg.port == kInvalidPort)
            return false;
        bool raw = false;
        if (!_gpio.readDyn(cfg.port, raw))
            return false;
        return raw;
    }

    static bool isTriggered_(const SensorConfig &cfg, bool raw)
    {
        return (cfg.type == SensorType::Reed) ? !raw : raw;
    }

    void initIButton_()
    {
        _ibutton_ready = false;
        OneWireBus *bus = _ow.busPtrById(OneWireManager::OwBusType::iButton);
        if (!bus)
        {
            _logs.warn(F("SEC"), F("iButton bus missing"));
            return;
        }
        _ibutton.begin(*bus);
        _ibutton_ready = true;
    }

    void handleIButton_()
    {
        if (!_ibutton_ready)
            return;
        uint8_t addr[8] = {};
        if (!_ibutton.readSerial(addr))
            return;
        if (!isAllowedKey_(addr))
            return;
        if (isKeyRepeat_(addr))
            return;
        toggleArm_();
    }

    bool isAllowedKey_(const uint8_t addr[8]) const
    {
        for (size_t i = 0; i < kKeyCount; ++i)
        {
            if (!_key_set[i])
                continue;
            if (memcmp(_keys[i], addr, 8) == 0)
                return true;
        }
        return false;
    }

    bool isKeyRepeat_(const uint8_t addr[8])
    {
        const uint32_t now = millis();
        if (memcmp(_last_key, addr, 8) == 0)
        {
            if ((uint32_t)(now - _last_key_ms) < kKeyRepeatMs)
                return true;
        }
        memcpy(_last_key, addr, 8);
        _last_key_ms = now;
        return false;
    }

    void toggleArm_()
    {
        if (_armed)
            disarm_(false);
        else
            arm_();
    }

    void arm_()
    {
        _armed = true;
        _alarm_on = false;
        clearDetect_();
        resetAlarmBuzzer_();
        updateAlarmLed_();
        updateSiren_();
        startBeep_(2, kBeepShortMs, kBeepGapMs);
        _dirty = true;
        _logs.info(F("SEC"), F("armed"));
    }

    void disarm_(bool silent)
    {
        _armed = false;
        _alarm_on = false;
        clearDetect_();
        resetAlarmBuzzer_();
        updateAlarmLed_();
        updateSiren_();
        if (!silent)
            startBeep_(1, kBeepLongMs, 0);
        _dirty = true;
        _logs.info(F("SEC"), F("disarmed"));
    }

    void applySnapshot_(bool armed)
    {
        _armed = armed;
        _alarm_on = false;
        clearDetect_();
        _beep_remaining = 0;
        _beep_state_on = false;
        _beep_next_ms = 0;
        resetAlarmBuzzer_();
        if (_controller_enabled)
        {
            updateAlarmLed_();
            updateSiren_();
            _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, false);
        }
        _dirty = false;
    }

    void clearDetect_()
    {
        for (size_t i = 0; i < kSensorCount; ++i)
            _state[i].is_detect = false;
    }

    void startBeep_(uint8_t count, uint16_t on_ms, uint16_t off_ms)
    {
        _beep_remaining = count;
        _beep_on_ms = on_ms;
        _beep_off_ms = off_ms;
        _beep_state_on = true;
        _beep_next_ms = millis() + on_ms;
        _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, true);
    }

    void updateBuzzer_()
    {
        if (_alarm_on)
        {
            updateAlarmBuzzer_();
            return;
        }
        if (_alarm_buzz_state)
        {
            _alarm_buzz_state = false;
            _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, false);
        }
        if (_beep_remaining == 0)
            return;
        const uint32_t now = millis();
        if ((int32_t)(now - _beep_next_ms) < 0)
            return;

        if (_beep_state_on)
        {
            _beep_state_on = false;
            _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, false);
            if (_beep_off_ms == 0)
            {
                if (_beep_remaining > 0)
                    --_beep_remaining;
                if (_beep_remaining == 0)
                    return;
                _beep_state_on = true;
                _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, true);
                _beep_next_ms = now + _beep_on_ms;
                return;
            }
            _beep_next_ms = now + _beep_off_ms;
            return;
        }

        if (_beep_remaining > 0)
            --_beep_remaining;
        if (_beep_remaining == 0)
            return;
        _beep_state_on = true;
        _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, true);
        _beep_next_ms = now + _beep_on_ms;
    }

    void updateAlarmBuzzer_()
    {
        const uint32_t now = millis();
        if ((int32_t)(now - _alarm_buzz_next_ms) < 0)
            return;
        _alarm_buzz_state = !_alarm_buzz_state;
        _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, _alarm_buzz_state);
        _alarm_buzz_next_ms = now + kAlarmBuzzMs;
    }

    void resetAlarmBuzzer_()
    {
        _alarm_buzz_state = false;
        _alarm_buzz_next_ms = millis() + kAlarmBuzzMs;
        _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, false);
    }

    void logDetect_(const SensorConfig &cfg)
    {
        _logs.warn(F("SEC"), F("detect id: %u type: %s"),
                   (unsigned)cfg.id, typeName_(cfg.type));
    }

    void notifyDetect_(const SensorConfig &cfg)
    {
        const auto &users = _tgusers.allowedUsers();
        if (users.empty())
            return;
        String msg = F("Тревога: датчик ");
        msg += String((unsigned)cfg.id);
        if (cfg.name.length())
        {
            msg += F(" (");
            msg += cfg.name;
            msg += F(")");
        }
        for (const auto &user : users)
        {
            if (!user.enabled || !user.is_notify || user.chat_id == 0)
                continue;
            _tgbot.sendText(user.chat_id, msg);
        }
    }

    static int hexNibble_(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        return -1;
    }

    static bool parseHexAddr_(const char *s, uint8_t out[8])
    {
        if (!s || strlen(s) != 16)
            return false;
        for (uint8_t i = 0; i < 8; ++i)
        {
            const int hi = hexNibble_(s[i * 2]);
            const int lo = hexNibble_(s[i * 2 + 1]);
            if (hi < 0 || lo < 0)
                return false;
            out[i] = (uint8_t)((hi << 4) | lo);
        }
        return true;
    }

    static constexpr uint32_t kKeyRepeatMs = 2000;
    static constexpr uint16_t kBeepShortMs = 120;
    static constexpr uint16_t kBeepGapMs = 120;
    static constexpr uint16_t kBeepLongMs = 500;
    static constexpr uint16_t kAlarmBuzzMs = 500;

    bool _alarm_buzz_state = false;
    uint32_t _alarm_buzz_next_ms = 0;
};
