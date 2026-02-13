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
#include "core/network/gsm_modem.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/gpio/gpio.hpp"
#include "hal/gpio/portio.hpp"
#include "hal/ibutton.hpp"
#include "hal/pn532.hpp"
#include "core/eeprom_storage.hpp"
#include "utils/logger.hpp"
#include "utils/users_registry.hpp"

class SecurityController
{
public:
    static constexpr size_t kSensorCount = 72;
    static constexpr size_t kKeyCount = UsersRegistry::kMaxUsers;
    static constexpr size_t kRfidKeyCount = UsersRegistry::kMaxUsers;
    static constexpr size_t kPhoneCount = UsersRegistry::kMaxUsers;
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

    using ArmStateHandler = void (*)(void *ctx, bool armed);
    using PreArmCheckHandler = bool (*)(void *ctx, String &out, String *plain_out);
    using AlarmStateHandler = void (*)(void *ctx, bool alarm_on);
    using ClearDetectHandler = void (*)(void *ctx);
    using DetectHandler = void (*)(void *ctx, uint8_t sensor_id, const String &name, bool silent);
    using RfidUidHandler = bool (*)(void *ctx, const String &uid);
    using IButtonSerialHandler = bool (*)(void *ctx, const String &serial);

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
        _rfid_disabled_startup_missing = false;
        setupOutputs_();
        initIButton_();
        initRfid_(true);
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
        _logs.info(F("SECURITY"), F("Init done"));
        return true;
    }

    void task()
    {
        handleGsm_();
        processTgNotifyQueue_();
        if (!_controller_enabled)
            return;
        handleIButton_();
        handleRfid_();
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
                    const bool was_alarm = _alarm_on;
                    _alarm_on = true;
                    if (!was_alarm)
                    {
                        _dirty = true;
                        _force_save = true;
                    }
                    resetAlarmBuzzer_();
                    updateSiren_();
                    if (!was_alarm)
                        notifyAlarmState_(true);
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
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
            _users->user(i).ibutton_key = "";
        size_t idx = 0;
        for (JsonVariantConst v : keys)
        {
            if (idx >= kKeyCount)
                break;
            UsersRegistry::User &u = _users->user(idx);
            if (v.is<const char *>())
            {
                u.ibutton_key = UsersRegistry::normalizeHex(v.as<const char *>(), 16);
            }
            else if (v.is<JsonObjectConst>())
            {
                JsonObjectConst obj = v.as<JsonObjectConst>();
                uint16_t id = (uint16_t)(idx + 1);
                bool enabled = true;
                const char *serial = nullptr;
                if (obj["id"].is<unsigned>())
                    id = (uint16_t)obj["id"].as<unsigned>();
                if (obj["enabled"].is<bool>())
                    enabled = obj["enabled"].as<bool>();
                if (obj["serial"].is<const char *>())
                    serial = obj["serial"].as<const char *>();
                else if (obj["addr"].is<const char *>())
                    serial = obj["addr"].as<const char *>();
                if (id < 1 || id > kKeyCount)
                {
                    ++idx;
                    continue;
                }
                const size_t dst = (size_t)(id - 1);
                if (serial && enabled)
                    _users->user(dst).ibutton_key = UsersRegistry::normalizeHex(serial, 16);
                else if (serial && !enabled)
                    _users->user(dst).ibutton_key = "";
            }
            ++idx;
        }
    }

    void applyRfidKeys(JsonArrayConst keys)
    {
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
            _users->user(i).rfid_key = "";
        size_t idx = 0;
        for (JsonVariantConst v : keys)
        {
            if (idx >= kRfidKeyCount)
                break;
            UsersRegistry::User &u = _users->user(idx);
            if (v.is<const char *>())
            {
                u.rfid_key = UsersRegistry::normalizeHex(v.as<const char *>(), 20);
            }
            else if (v.is<JsonObjectConst>())
            {
                JsonObjectConst obj = v.as<JsonObjectConst>();
                uint16_t id = (uint16_t)(idx + 1);
                bool enabled = true;
                const char *uid_str = nullptr;
                if (obj["id"].is<unsigned>())
                    id = (uint16_t)obj["id"].as<unsigned>();
                if (obj["enabled"].is<bool>())
                    enabled = obj["enabled"].as<bool>();
                if (obj["serial"].is<const char *>())
                    uid_str = obj["serial"].as<const char *>();
                else if (obj["uid"].is<const char *>())
                    uid_str = obj["uid"].as<const char *>();
                if (id < 1 || id > kRfidKeyCount)
                {
                    ++idx;
                    continue;
                }
                const size_t dst = (size_t)(id - 1);
                if (uid_str && enabled)
                    _users->user(dst).rfid_key = UsersRegistry::normalizeHex(uid_str, 20);
                else if (uid_str && !enabled)
                    _users->user(dst).rfid_key = "";
            }
            ++idx;
        }
    }

    void applyPhones(JsonArrayConst phones)
    {
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            _users->user(i).gsm_phone = "";
            _users->user(i).gsm_sms = false;
            _users->user(i).gsm_call = false;
        }
        size_t idx = 0;
        for (JsonVariantConst v : phones)
        {
            if (idx >= kPhoneCount)
                break;
            bool enabled = true;
            bool notify = false;
            bool call = false;
            uint16_t id = (uint16_t)(idx + 1);
            String number;
            String name;
            if (v.is<const char *>())
            {
                number = v.as<const char *>();
            }
            if (v.is<JsonObjectConst>())
            {
                JsonObjectConst obj = v.as<JsonObjectConst>();
                if (obj["id"].is<unsigned>())
                    id = (uint16_t)obj["id"].as<unsigned>();
                if (obj["enabled"].is<bool>())
                    enabled = obj["enabled"].as<bool>();
                if (obj["notify"].is<bool>())
                    notify = obj["notify"].as<bool>();
                if (obj["call"].is<bool>())
                    call = obj["call"].as<bool>();
                if (obj["number"].is<const char *>())
                    number = obj["number"].as<const char *>();
                if (obj["name"].is<const char *>())
                    name = obj["name"].as<const char *>();
            }
            if (id < 1 || id > kPhoneCount)
            {
                ++idx;
                continue;
            }
            const size_t dst = (size_t)(id - 1);
            UsersRegistry::User &u = _users->user(dst);
            u.gsm_phone = enabled ? UsersRegistry::normalizePhone(number) : String();
            if (name.length())
                u.username = name;
            u.gsm_sms = notify;
            u.gsm_call = call;
            u.enabled = enabled;
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
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.ibutton_key.length() == 0)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = (unsigned)(i + 1);
            obj["enabled"] = true;
            obj["serial"] = u.ibutton_key;
            if (u.username.length())
                obj["name"] = u.username;
        }
    }

    void serializeRfidKeys(JsonArray out) const
    {
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.rfid_key.length() == 0)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = (unsigned)(i + 1);
            obj["enabled"] = true;
            obj["serial"] = u.rfid_key;
            if (u.username.length())
                obj["name"] = u.username;
        }
    }

    void serializePhones(JsonArray out) const
    {
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = (unsigned)(i + 1);
            obj["enabled"] = u.enabled;
            obj["notify"] = u.gsm_sms;
            obj["call"] = u.gsm_call;
            if (u.gsm_phone.length())
                obj["number"] = u.gsm_phone;
            if (u.username.length())
                obj["name"] = u.username;
        }
    }

    void buildSnapshot(uint8_t &flags) const
    {
        flags = 0;
        if (_armed)
            flags |= EepromStorage::kSecurityArmedMask;
        if (_alarm_on)
            flags |= EepromStorage::kSecurityAlarmMask;
    }

    void applySnapshot(uint8_t flags)
    {
        const bool armed = (flags & EepromStorage::kSecurityArmedMask) != 0;
        const bool alarm = armed && ((flags & EepromStorage::kSecurityAlarmMask) != 0);
        applySnapshot_(armed, alarm);
    }

    void setArmStateHandler(ArmStateHandler cb, void *ctx)
    {
        _arm_state_cb = cb;
        _arm_state_ctx = ctx;
    }
    void setPreArmCheckHandler(PreArmCheckHandler cb, void *ctx)
    {
        _pre_arm_cb = cb;
        _pre_arm_ctx = ctx;
    }

    void setAlarmStateHandler(AlarmStateHandler cb, void *ctx)
    {
        _alarm_state_cb = cb;
        _alarm_state_ctx = ctx;
    }

    void setClearDetectHandler(ClearDetectHandler cb, void *ctx)
    {
        _clear_detect_cb = cb;
        _clear_detect_ctx = ctx;
    }

    void setDetectHandler(DetectHandler cb, void *ctx)
    {
        _detect_cb = cb;
        _detect_ctx = ctx;
    }

    void setRfidUidHandler(RfidUidHandler cb, void *ctx)
    {
        _rfid_uid_cb = cb;
        _rfid_uid_ctx = ctx;
    }
    void setIButtonSerialHandler(IButtonSerialHandler cb, void *ctx)
    {
        _ibutton_serial_cb = cb;
        _ibutton_serial_ctx = ctx;
    }

    void setRfidI2c(I2CManager *i2c)
    {
        _rfid_i2c = i2c;
    }
    void setUsersRegistry(UsersRegistry &users)
    {
        _users = &users;
    }

    bool processRfidUid(const PN532::UID &uid, const char *src = "rfid")
    {
        if (!_controller_enabled)
            return false;
        if (uid.len == 0 || uid.len > sizeof(_last_rfid))
            return false;
        if (isRfidRepeat_(uid))
            return false;
        const String uid_str = rfidUidToString_(uid.bytes, uid.len);
        _logs.info(F("SECURITY"), F("Detected RFID UID: %s"), uid_str.c_str());
        String user;
        if (!matchRfidKey_(uid, user))
        {
            _logs.warn(F("SECURITY"), F("RFID tag is not valid"));
            startBeep_(kBeepRejectCount, kBeepRejectOnMs, kBeepRejectOffMs);
            return false;
        }
        if (!_armed)
        {
            const char *who = user.length() ? user.c_str() : "unknown";
            _logs.info(F("SECURITY"), F("RFID match while disarmed: owner: %s uid: %s"), who, uid_str.c_str());
            startBeep_(1, kBeepShortMs, 0);
            // Keep stack units in sync: a valid RFID key always enforces disarmed state cluster-wide.
            notifyArmState_(false);
            return true;
        }
        const char *who = user.length() ? user.c_str() : "unknown";
        _logs.info(F("SECURITY"), F("disarmed by RFID key: owner: %s uid: %s"), who, uid_str.c_str());
        disarm_(false, src ? src : "rfid", user);
        return true;
    }

    bool processRfidUidString(const char *uid_str, const char *src = "rfid")
    {
        PN532::UID uid;
        if (!parseRfidUid_(uid_str, uid))
            return false;
        return processRfidUid(uid, src);
    }

    bool processIButtonAddr(const uint8_t addr[8], const char *src = "ibutton")
    {
        if (!_controller_enabled || !addr)
            return false;
        if (isKeyRepeat_(addr))
            return false;
        char serial[17] = {};
        IButton::toHex(addr, serial);
        _logs.info(F("SECURITY"), F("Detected iButton key: %s"), serial);
        String user;
        if (!matchKey_(addr, user))
        {
            _logs.warn(F("SECURITY"), F("iButton key is not valid"));
            startBeep_(kBeepRejectCount, kBeepRejectOnMs, kBeepRejectOffMs);
            return false;
        }
        if (!_armed)
        {
            const char *who = user.length() ? user.c_str() : "unknown";
            _logs.info(F("SECURITY"), F("iButton match while disarmed: owner: %s serial: %s"), who, serial);
            startBeep_(1, kBeepShortMs, 0);
            notifyArmState_(false);
            return true;
        }
        const char *who = user.length() ? user.c_str() : "unknown";
        _logs.info(F("SECURITY"), F("disarmed by iButton key: owner: %s serial: %s"), who, serial);
        disarm_(false, src ? src : "ibutton", user);
        return true;
    }

    bool processIButtonSerialString(const char *serial, const char *src = "ibutton")
    {
        uint8_t addr[8] = {};
        if (!parseHexAddr_(serial, addr))
            return false;
        return processIButtonAddr(addr, src);
    }

    void setNotifyEnabled(bool enabled)
    {
        _notify_enabled = enabled;
    }

    bool takeDirty()
    {
        if (!_dirty)
            return false;
        _dirty = false;
        return true;
    }

    bool takeForceSave()
    {
        if (!_force_save)
            return false;
        _force_save = false;
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
            _rfid_ready = false;
            reset_();
            return;
        }
        _logs.info(F("SECURITY"), F("controller: enabled"));
        setupOutputs_();
        initIButton_();
        initRfid_();
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
    void setGsmModem(GsmModem &modem) { _gsm = &modem; }
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
    bool armFrom(const char *src, const String &user)
    {
        if (_armed)
            return true;
        if (!_controller_enabled)
            return false;
        arm_(src, user);
        return true;
    }
    bool armForcedFrom(const char *src, const String &user)
    {
        if (_armed)
            return true;
        if (!_controller_enabled)
            return false;
        armForce_(src, user);
        return true;
    }
    bool disarmFrom(const char *src, const String &user, bool silent = false)
    {
        if (!_armed)
            return true;
        disarm_(silent, src, user);
        return true;
    }
    void toggleFrom(const char *src, const String &user)
    {
        if (_armed)
            disarm_(false, src, user);
        else
            arm_(src, user);
    }
    void clearDetect()
    {
        clearDetect_();
        notifyClearDetect_();
    }

    bool fillPrearmItems(JsonArray &arr, String *plain_out = nullptr)
    {
        bool any = false;
        if (plain_out)
            *plain_out = "";
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            SensorConfig &cfg = _cfg[i];
            SensorState &st = _state[i];
            if (!cfg.enabled)
                continue;
            const bool raw = readRaw_(cfg);
            st.raw = raw;
            if (!isTriggered_(cfg, raw))
                continue;
            JsonObject o = arr.add<JsonObject>();
            o["id"] = (unsigned)cfg.id;
            if (cfg.name.length())
                o["name"] = cfg.name;
            if (plain_out)
            {
                if (plain_out->length())
                    *plain_out += F(", ");
                *plain_out += String((unsigned)cfg.id);
                if (cfg.name.length())
                {
                    *plain_out += F(" (");
                    *plain_out += cfg.name;
                    *plain_out += F(")");
                }
            }
            any = true;
        }
        return any;
    }

    void notifyRemoteDetect(const String &source, uint8_t sensor_id, const String &name, bool silent)
    {
        if (!_notify_enabled)
            return;
        String msg = F("Охрана: тревога ");
        if (source.length())
        {
            msg += F("<b>");
            msg += escapeHtml_(source);
            msg += F("</b>");
            msg += F(", ");
        }
        msg += F("датчик ");
        msg += String((unsigned)sensor_id);
        if (name.length())
        {
            msg += F(" (");
            msg += F("<b>");
            msg += escapeHtml_(name);
            msg += F("</b>");
            msg += F(")");
        }
        if (silent)
            msg += F(" [silent]");
        sendTgNotify_(msg, F("HTML"));
        sendSmsNotify_(sensor_id, name);
    }

    void setAlarmState(bool on)
    {
        if (_alarm_on == on)
            return;
        _alarm_on = on;
        _dirty = true;
        _force_save = true;
        resetAlarmBuzzer_();
        updateSiren_();
        notifyAlarmState_(on);
    }
    bool setEnabled(size_t id, bool enabled)
    {
        size_t idx = 0;
        if (!indexById_((uint8_t)id, idx))
            return false;
        SensorConfig &cfg = _cfg[idx];
        SensorState &st = _state[idx];
        if (!enabled)
        {
            const uint8_t saved_id = cfg.id;
            String saved_name = cfg.name;
            cfg = SensorConfig{};
            cfg.id = saved_id;
            cfg.enabled = false;
            st = SensorState{};
            const char *name = saved_name.length() ? saved_name.c_str() : "-";
            _logs.info(F("SECURITY"), F("id: %u name: %s enabled: false"), (unsigned)cfg.id, name);
            return true;
        }
        cfg.enabled = true;
        st = SensorState{};
        if (_controller_enabled && cfg.enabled)
        {
            setupSensorInput_(cfg);
            st.raw = readRaw_(cfg);
        }
        _logs.info(F("SECURITY"), F("id: %u enabled: true"), (unsigned)cfg.id);
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
        return addKey(addr, "");
    }
    bool addKey(const uint8_t addr[8], const String &name)
    {
        if (!addr || !_users)
            return false;
        char hex[17] = {};
        IButton::toHex(addr, hex);
        const String serial = UsersRegistry::normalizeHex(hex, 16);
        for (size_t i = 0; i < _users->size(); ++i)
        {
            if (_users->user(i).ibutton_key == serial)
                return true;
        }
        for (size_t i = 0; i < _users->size(); ++i)
        {
            UsersRegistry::User &u = _users->user(i);
            if (u.ibutton_key.length())
                continue;
            u.ibutton_key = serial;
            if (name.length())
                u.username = name;
            return true;
        }
        return false;
    }
    bool removeKey(const uint8_t addr[8])
    {
        if (!addr || !_users)
            return false;
        char hex[17] = {};
        IButton::toHex(addr, hex);
        const String serial = UsersRegistry::normalizeHex(hex, 16);
        for (size_t i = 0; i < _users->size(); ++i)
        {
            UsersRegistry::User &u = _users->user(i);
            if (u.ibutton_key != serial)
                continue;
            u.ibutton_key = "";
            return true;
        }
        return false;
    }
    void clearKeys()
    {
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
            _users->user(i).ibutton_key = "";
    }
    void clearPhones()
    {
        clearPhones_();
    }
    bool setPhone(size_t idx, const String &number)
    {
        if (!_users || idx >= _users->size())
            return false;
        _users->user(idx).gsm_phone = UsersRegistry::normalizePhone(number);
        return true;
    }
    bool setPhoneName(size_t idx, const String &name)
    {
        if (!_users || idx >= _users->size())
            return false;
        _users->user(idx).username = name;
        return true;
    }
    bool setPhoneNotify(size_t idx, bool notify)
    {
        if (!_users || idx >= _users->size())
            return false;
        _users->user(idx).gsm_sms = notify;
        return true;
    }
    bool setPhoneCall(size_t idx, bool call)
    {
        if (!_users || idx >= _users->size())
            return false;
        _users->user(idx).gsm_call = call;
        return true;
    }
    bool setPhoneEnabled(size_t idx, bool enabled)
    {
        if (!_users || idx >= _users->size())
            return false;
        _users->user(idx).enabled = enabled;
        return true;
    }
    bool phoneSlot(size_t idx, String &number, bool &enabled) const
    {
        if (!_users || idx >= _users->size())
            return false;
        const auto &u = _users->user(idx);
        number = u.gsm_phone;
        enabled = u.enabled;
        return true;
    }
    const String &phoneByIndex(size_t idx) const
    {
        static const String empty;
        if (!_users || idx >= _users->size())
            return empty;
        return _users->user(idx).gsm_phone;
    }
    const String &phoneNameByIndex(size_t idx) const
    {
        static const String empty;
        if (!_users || idx >= _users->size())
            return empty;
        return _users->user(idx).username;
    }
    bool phoneNotifyByIndex(size_t idx) const
    {
        if (!_users || idx >= _users->size())
            return false;
        return _users->user(idx).gsm_sms;
    }
    bool phoneCallByIndex(size_t idx) const
    {
        if (!_users || idx >= _users->size())
            return false;
        return _users->user(idx).gsm_call;
    }
    size_t keyCount() const
    {
        size_t count = 0;
        if (!_users)
            return 0;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.ibutton_key.length())
                ++count;
        }
        return count;
    }
    bool keyByIndex(size_t idx, uint8_t out[8]) const
    {
        if (!out || !_users)
            return false;
        size_t seen = 0;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            const String serial = u.ibutton_key;
            if (serial.length() == 0)
                continue;
            if (seen == idx)
            {
                return parseHexAddr_(serial.c_str(), out);
            }
            ++seen;
        }
        return false;
    }
    const String &keyNameByIndex(size_t idx) const
    {
        static const String empty;
        if (!_users)
            return empty;
        size_t seen = 0;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled || u.ibutton_key.length() == 0)
                continue;
            if (seen == idx)
            {
                const String &name = u.username;
                return name.length() ? name : empty;
            }
            ++seen;
        }
        return empty;
    }
    bool setKeyNameByAddr(const uint8_t addr[8], const String &name)
    {
        if (!addr || !_users)
            return false;
        char hex[17] = {};
        IButton::toHex(addr, hex);
        const String serial = UsersRegistry::normalizeHex(hex, 16);
        for (size_t i = 0; i < _users->size(); ++i)
        {
            UsersRegistry::User &u = _users->user(i);
            if (u.ibutton_key != serial)
                continue;
            u.username = name;
            return true;
        }
        return false;
    }
    bool keySlot(size_t idx, uint8_t out[8], bool &enabled) const
    {
        if (!_users || idx >= _users->size())
            return false;
        const String serial = _users->user(idx).ibutton_key;
        enabled = serial.length() > 0;
        if (out)
        {
            if (!enabled || !parseHexAddr_(serial.c_str(), out))
                memset(out, 0, 8);
        }
        return true;
    }
    bool lastKeyHex(char out[17]) const
    {
        if (!out || _last_key_ms == 0)
            return false;
        IButton::toHex(_last_key, out);
        return true;
    }
    bool lastRfidSerial(String &out) const
    {
        if (_last_rfid_ms == 0 || _last_rfid_len == 0)
            return false;
        out = rfidUidToString_(_last_rfid, _last_rfid_len);
        return out.length() > 0;
    }
    bool setKeySlot(size_t idx, const uint8_t addr[8], bool enabled, const String &name)
    {
        if (!_users || idx >= _users->size())
            return false;
        UsersRegistry::User &u = _users->user(idx);
        if (enabled)
        {
            char hex[17] = {};
            IButton::toHex(addr, hex);
            u.ibutton_key = UsersRegistry::normalizeHex(hex, 16);
        }
        else
        {
            u.ibutton_key = "";
        }
        u.username = name;
        return true;
    }
    bool rfidKeySlot(size_t idx, uint8_t out[10], uint8_t &len, bool &enabled) const
    {
        if (!_users || idx >= _users->size())
            return false;
        const String serial = _users->user(idx).rfid_key;
        enabled = serial.length() > 0;
        len = 0;
        if (out)
        {
            memset(out, 0, 10);
        }
        if (enabled)
        {
            PN532::UID uid;
            if (parseRfidUid_(serial.c_str(), uid))
            {
                len = uid.len;
                if (out && len > 0)
                    memcpy(out, uid.bytes, len);
            }
        }
        return true;
    }
    const String &rfidKeyNameByIndex(size_t idx) const
    {
        static const String empty;
        if (!_users)
            return empty;
        size_t seen = 0;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled || u.rfid_key.length() == 0)
                continue;
            if (seen == idx)
            {
                const String &name = u.username;
                return name.length() ? name : empty;
            }
            ++seen;
        }
        return empty;
    }
    bool setRfidKeySlot(size_t idx, const uint8_t *bytes, uint8_t len, bool enabled, const String &name)
    {
        if (!_users || idx >= _users->size())
            return false;
        UsersRegistry::User &u = _users->user(idx);
        if (enabled && bytes && len > 0)
            u.rfid_key = UsersRegistry::normalizeHex(rfidUidToString_(bytes, len), 20);
        else
            u.rfid_key = "";
        u.username = name;
        return true;
    }

    static bool parseRfidSerial(const char *s, uint8_t out[10], uint8_t &len)
    {
        PN532::UID uid;
        if (!parseRfidUid_(s, uid))
            return false;
        len = uid.len;
        memcpy(out, uid.bytes, uid.len);
        return true;
    }
    static String rfidSerialToString(const uint8_t *bytes, uint8_t len)
    {
        return rfidUidToString_(bytes, len);
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
    struct TgNotifyItem
    {
        String msg;
        String parse_mode;
        uint16_t next_user = 0;
    };

    Gpio &_gpio;
    OneWireManager &_ow;
    Logger &_logs;
    TelegramBot &_tgbot;
    TelegramAllowedUsersProvider &_tgusers;
    uint32_t _tg_last_send_ms = 0;
    static constexpr uint8_t kTgQueueDepth = 8;
    TgNotifyItem _tg_queue[kTgQueueDepth]{};
    uint8_t _tg_q_head = 0;
    uint8_t _tg_q_tail = 0;
    uint8_t _tg_q_size = 0;
    GsmModem *_gsm = nullptr;
    IButton _ibutton;
    bool _ibutton_ready = false;
    I2CManager *_rfid_i2c = nullptr;
    PN532 _rfid;
    bool _rfid_ready = false;
    bool _rfid_disabled_startup_missing = false;
    UsersRegistry *_users = nullptr;
    RfidUidHandler _rfid_uid_cb = nullptr;
    void *_rfid_uid_ctx = nullptr;
    IButtonSerialHandler _ibutton_serial_cb = nullptr;
    void *_ibutton_serial_ctx = nullptr;

    SensorConfig _cfg[kSensorCount]{};
    SensorState _state[kSensorCount]{};
    uint8_t _last_key[8]{};
    uint32_t _last_key_ms = 0;
    uint8_t _last_rfid[10]{};
    uint8_t _last_rfid_len = 0;
    uint32_t _last_rfid_ms = 0;
    uint32_t _last_rfid_poll_ms = 0;
    bool _controller_enabled = false;
    bool _armed = false;
    bool _alarm_on = false;
    uint8_t _siren_port = kInvalidPort;
    bool _dirty = false;
    bool _force_save = false;

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
        clearRfidKeys_();
        clearPhones_();
        _siren_port = kInvalidPort;
        _armed = false;
        _alarm_on = false;
        _beep_remaining = 0;
        _dirty = false;
        _force_save = false;
    }

    void clearKeys_()
    {
        if (_users)
        {
            for (size_t i = 0; i < _users->size(); ++i)
                _users->user(i).ibutton_key = "";
        }
        memset(_last_key, 0, sizeof(_last_key));
        _last_key_ms = 0;
    }

    void clearRfidKeys_()
    {
        if (_users)
        {
            for (size_t i = 0; i < _users->size(); ++i)
                _users->user(i).rfid_key = "";
        }
        memset(_last_rfid, 0, sizeof(_last_rfid));
        _last_rfid_len = 0;
        _last_rfid_ms = 0;
    }

    bool setRfidKeySlot_(size_t idx, const PN532::UID &uid, bool enabled, const String &name)
    {
        if (!_users || idx >= _users->size())
            return false;
        UsersRegistry::User &u = _users->user(idx);
        if (enabled)
        {
            if (uid.len == 0 || uid.len > 10)
                return false;
            u.rfid_key = UsersRegistry::normalizeHex(rfidUidToString_(uid.bytes, uid.len), 20);
        }
        else
        {
            u.rfid_key = "";
        }
        u.username = name;
        return true;
    }

    void clearPhones_()
    {
        if (!_users)
            return;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            auto &u = _users->user(i);
            u.gsm_phone = "";
            u.gsm_sms = false;
            u.gsm_call = false;
        }
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
            _logs.warn(F("SECURITY"), F("iButton bus missing"));
            return;
        }
        _ibutton.begin(*bus);
        _ibutton_ready = true;
    }

    void initRfid_(bool startup = false)
    {
        _rfid_ready = false;
        if (_rfid_disabled_startup_missing)
            return;
        if (!_rfid_i2c)
        {
            _logs.warn(F("SECURITY"), F("RFID I2C not set"));
            return;
        }
        const uint8_t i2c_index = ActiveBoardProfile::RFID_I2C_INDEX;
        if (i2c_index >= ActiveBoardProfile::I2C_COUNT)
        {
            _logs.warn(F("SECURITY"), F("RFID I2C index invalid"));
            return;
        }
        const auto cfg = ActiveBoardProfile::I2CS[i2c_index];
        TwoWire *wire = _rfid_i2c->wirePtr(cfg.bus_num);
        if (!wire)
        {
            _logs.warn(F("SECURITY"), F("RFID I2C bus unavailable"));
            return;
        }
        if (!_rfid_i2c->probeAddress(cfg.bus_num, kRfidI2cAddr))
        {
            if (startup)
                _rfid_disabled_startup_missing = true;
            _logs.warn(F("SECURITY"), F("RFID not found on I2C"));
            return;
        }
        uint8_t sda_gpio = 0;
        uint8_t scl_gpio = 0;
        if (!portToGpio_(cfg.sda, sda_gpio) || !portToGpio_(cfg.scl, scl_gpio))
        {
            _logs.warn(F("SECURITY"), F("RFID SDA/SCL port mapping failed"));
            return;
        }
        _rfid_ready = _rfid.begin(*wire, sda_gpio, scl_gpio, cfg.freq, kRfidI2cAddr, -1, -1);
        if (!_rfid_ready)
            _logs.warn(F("SECURITY"), F("RFID init failed"));
    }

    void handleRfid_()
    {
        if (_rfid_disabled_startup_missing)
            return;
        if (!_rfid_i2c)
            return;
        const uint32_t now = millis();
        if ((uint32_t)(now - _last_rfid_poll_ms) < kRfidPollMs)
            return;
        _last_rfid_poll_ms = now;
        const uint8_t i2c_index = ActiveBoardProfile::RFID_I2C_INDEX;
        if (i2c_index >= ActiveBoardProfile::I2C_COUNT)
            return;
        const auto cfg = ActiveBoardProfile::I2CS[i2c_index];
        if (!_rfid_i2c->probeAddress(cfg.bus_num, kRfidI2cAddr))
        {
            if (_rfid_ready)
            {
                _rfid_ready = false;
                _logs.warn(F("SECURITY"), F("RFID I2C lost"));
            }
            return;
        }
        if (!_rfid_ready)
        {
            initRfid_();
            if (!_rfid_ready)
                return;
        }
        PN532::UID uid{};
        const PN532::Status st = _rfid.readPassiveTargetID(uid, kRfidReadTimeoutMs);
        if (st == PN532::Status::I2cError)
        {
            _rfid_ready = false;
            return;
        }
        if (st != PN532::Status::Ok)
            return;
        if (isRfidRepeat_(uid))
            return;
        const String uid_str = rfidUidToString_(uid.bytes, uid.len);
        if (_rfid_uid_cb && _rfid_uid_cb(_rfid_uid_ctx, uid_str))
            return;
        processRfidUid(uid, "rfid");
    }

    void handleIButton_()
    {
        if (!_ibutton_ready)
            return;
        uint8_t addr[8] = {};
        if (!_ibutton.readSerial(addr))
            return;
        char hex[17] = {};
        IButton::toHex(addr, hex);
        if (_ibutton_serial_cb && _ibutton_serial_cb(_ibutton_serial_ctx, String(hex)))
            return;
        processIButtonAddr(addr, "ibutton");
    }

    bool isAllowedKey_(const uint8_t addr[8]) const
    {
        if (!_users)
            return false;
        char hex[17] = {};
        IButton::toHex(addr, hex);
        const String serial = UsersRegistry::normalizeHex(hex, 16);
        for (size_t i = 0; i < _users->size(); ++i)
        {
            if (_users->user(i).ibutton_key == serial)
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

    bool isRfidRepeat_(const PN532::UID &uid)
    {
        const uint32_t now = millis();
        if (uid.len == _last_rfid_len &&
            memcmp(_last_rfid, uid.bytes, uid.len) == 0)
        {
            if ((uint32_t)(now - _last_rfid_ms) < kKeyRepeatMs)
                return true;
        }
        _last_rfid_len = uid.len;
        memcpy(_last_rfid, uid.bytes, uid.len);
        _last_rfid_ms = now;
        return false;
    }

    static bool portToGpio_(uint8_t port, uint8_t &out_gpio)
    {
        if (port >= PortIO::PORT_COUNT)
            return false;
        const auto &desc = ActiveBoardProfile::PORTS[port];
        if (desc.backend != PortIO::Backend::Esp32)
            return false;
        if (desc.u.esp.gpio == 0xFF)
            return false;
        out_gpio = desc.u.esp.gpio;
        return true;
    }

    void toggleArm_()
    {
        if (_armed)
            disarm_(false);
        else
            arm_();
    }

    void toggleArm_(const char *src, const String &user)
    {
        if (_armed)
            disarm_(false, src, user);
        else
            arm_(src, user);
    }

    void handleGsm_()
    {
        if (!_gsm)
            return;
        String number;
        if (!_gsm->takeLastCall(number))
            return;
        _gsm->driver().hangup();
        String user;
        if (!matchPhone_(number, user))
            return;
        if (_armed)
        {
            disarm_(false, "gsm", user);
        }
        else
        {
            arm_("gsm", user);
        }
    }

    void arm_()
    {
        String blocked;
        String blocked_log;
        if (hasTriggeredBeforeArm_(blocked, &blocked_log))
        {
            _logs.warn(F("SECURITY"), F("arm blocked, triggered: %s"), blocked_log.c_str());
            startBeep_(kBeepRejectCount, kBeepRejectOnMs, kBeepRejectOffMs);
            const char *src = "local";
            const char *who = "unknown";
            String msg = F("Охрана: невозможно поставить, источник: <b>");
            msg += escapeHtml_(src);
            msg += F("</b>, кто: <b>");
            msg += escapeHtml_(who);
            msg += F("</b>\nСработали датчики:\n<pre>");
            msg += blocked;
            msg += F("</pre>");
            sendTgNotify_(msg, F("HTML"));
            return;
        }
        _armed = true;
        const bool was_alarm = _alarm_on;
        _alarm_on = false;
        clearDetect_();
        resetAlarmBuzzer_();
        updateAlarmLed_();
        updateSiren_();
        startBeep_(2, kBeepShortMs, kBeepGapMs);
        _dirty = true;
        if (was_alarm)
            _force_save = true;
        logArmAction_(true, nullptr, String());
        notifyArmState_(true);
        if (was_alarm)
            notifyAlarmState_(false);
    }

    void disarm_(bool silent)
    {
        _armed = false;
        const bool was_alarm = _alarm_on;
        _alarm_on = false;
        clearDetect_();
        resetAlarmBuzzer_();
        updateAlarmLed_();
        updateSiren_();
        if (!silent)
            startBeep_(1, kBeepLongMs, 0);
        _dirty = true;
        if (was_alarm)
            _force_save = true;
        logArmAction_(false, nullptr, String());
        notifyArmState_(false);
        if (was_alarm)
            notifyAlarmState_(false);
    }

    void arm_(const char *src, const String &user)
    {
        String blocked;
        String blocked_log;
        if (hasTriggeredBeforeArm_(blocked, &blocked_log))
        {
            _logs.warn(F("SECURITY"), F("arm blocked, triggered: %s"), blocked_log.c_str());
            startBeep_(kBeepRejectCount, kBeepRejectOnMs, kBeepRejectOffMs);
            const char *who = user.length() ? user.c_str() : "unknown";
            const char *from = src ? src : "local";
            String msg = F("Охрана: невозможно поставить, источник: <b>");
            msg += escapeHtml_(from);
            msg += F("</b>, кто: <b>");
            msg += escapeHtml_(who);
            msg += F("</b>\nСработали датчики:\n<pre>");
            msg += blocked;
            msg += F("</pre>");
            sendTgNotify_(msg, F("HTML"));
            return;
        }
        _armed = true;
        const bool was_alarm = _alarm_on;
        _alarm_on = false;
        clearDetect_();
        resetAlarmBuzzer_();
        updateAlarmLed_();
        updateSiren_();
        startBeep_(2, kBeepShortMs, kBeepGapMs);
        _dirty = true;
        if (was_alarm)
            _force_save = true;
        logArmAction_(true, src, user);
        notifyArmState_(true);
        if (was_alarm)
            notifyAlarmState_(false);
    }

    void armForce_(const char *src, const String &user)
    {
        _armed = true;
        const bool was_alarm = _alarm_on;
        _alarm_on = false;
        clearDetect_();
        resetAlarmBuzzer_();
        updateAlarmLed_();
        updateSiren_();
        startBeep_(2, kBeepShortMs, kBeepGapMs);
        _dirty = true;
        if (was_alarm)
            _force_save = true;
        logArmAction_(true, src, user);
        notifyArmState_(true);
        if (was_alarm)
            notifyAlarmState_(false);
    }

    void disarm_(bool silent, const char *src, const String &user)
    {
        _armed = false;
        const bool was_alarm = _alarm_on;
        _alarm_on = false;
        clearDetect_();
        resetAlarmBuzzer_();
        updateAlarmLed_();
        updateSiren_();
        if (!silent)
            startBeep_(1, kBeepLongMs, 0);
        _dirty = true;
        if (was_alarm)
            _force_save = true;
        logArmAction_(false, src, user);
        notifyArmState_(false);
        if (was_alarm)
            notifyAlarmState_(false);
    }

    void applySnapshot_(bool armed, bool alarm)
    {
        _armed = armed;
        _alarm_on = alarm;
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
        _force_save = false;
    }

    bool hasTriggeredBeforeArm_(String &out, String *plain_out = nullptr)
    {
        out = "";
        if (plain_out)
            *plain_out = "";
        bool any = false;
        bool local_any = false;
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            SensorConfig &cfg = _cfg[i];
            SensorState &st = _state[i];
            if (!cfg.enabled)
                continue;
            const bool raw = readRaw_(cfg);
            st.raw = raw;
            if (!isTriggered_(cfg, raw))
                continue;
            _logs.warn(F("SECURITY"), F("prearm blocked sensor %u (%s)"),
                       (unsigned)cfg.id, cfg.name.length() ? cfg.name.c_str() : "-");
            if (local_any)
                out += F(", ");
            else
                out += F("локально: ");
            out += String((unsigned)cfg.id);
            if (cfg.name.length())
            {
                out += F(" (");
                out += F("<b>");
                out += escapeHtml_(cfg.name);
                out += F("</b>");
                out += F(")");
            }
            if (plain_out)
            {
                if (local_any)
                    *plain_out += F(", ");
                else
                    *plain_out += F("локально: ");
                *plain_out += String((unsigned)cfg.id);
                if (cfg.name.length())
                {
                    *plain_out += F(" (");
                    *plain_out += cfg.name;
                    *plain_out += F(")");
                }
            }
            local_any = true;
            any = true;
        }
        if (_pre_arm_cb)
        {
            String extra;
            String extra_plain;
            const bool extra_any = _pre_arm_cb(_pre_arm_ctx, extra, plain_out ? &extra_plain : nullptr);
            if (extra_any)
            {
                if (out.length())
                    out += F("\n");
                out += extra;
                if (plain_out && extra_plain.length())
                {
                    if (plain_out->length())
                        *plain_out += F(", ");
                    *plain_out += extra_plain;
                }
                any = true;
            }
        }
        return any;
    }

    static String escapeHtml_(const char *text)
    {
        if (!text)
            return String();
        return escapeHtml_(String(text));
    }

    static String escapeHtml_(const String &text)
    {
        String out = text;
        out.replace("&", "&amp;");
        out.replace("<", "&lt;");
        out.replace(">", "&gt;");
        return out;
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
        _logs.warn(F("SECURITY"), F("detect id: %u type: %s"),
                   (unsigned)cfg.id, typeName_(cfg.type));
    }

    void notifyDetect_(const SensorConfig &cfg)
    {
        notifyDetectEvent_(cfg);
        if (!_notify_enabled)
            return;
        String msg = F("Охрана: тревога датчик ");
        msg += String((unsigned)cfg.id);
        if (cfg.name.length())
        {
            msg += F(" (");
            msg += F("<b>");
            msg += escapeHtml_(cfg.name);
            msg += F("</b>");
            msg += F(")");
        }
        sendTgNotify_(msg, F("HTML"));
        sendSmsNotify_(cfg);
    }

    void notifyArmAction_(bool armed, const char *src, const String &user)
    {
        if (!_notify_enabled)
            return;
        String msg = armed ? F("Охрана: постановка") : F("Охрана: снятие");
        const char *who = user.length() ? user.c_str() : "неизвестен";
        const char *from = src ? src : "локально";
        String who_txt = who;
        String from_txt = from;
        who_txt.replace("&", "&amp;");
        who_txt.replace("<", "&lt;");
        who_txt.replace(">", "&gt;");
        from_txt.replace("&", "&amp;");
        from_txt.replace("<", "&lt;");
        from_txt.replace(">", "&gt;");
        msg += F(", кто: <b>");
        msg += who_txt;
        msg += F("</b>, способ: <b>");
        msg += from_txt;
        msg += F("</b>");
        sendTgNotify_(msg, F("HTML"));
    }

    void sendTgNotify_(const String &msg, const String &parse_mode = "")
    {
        enqueueTgNotify_(msg, parse_mode);
    }

    bool enqueueTgNotify_(const String &msg, const String &parse_mode)
    {
        if (_tg_q_size > 0)
        {
            const uint8_t last = (uint8_t)((_tg_q_tail + kTgQueueDepth - 1) % kTgQueueDepth);
            const TgNotifyItem &prev = _tg_queue[last];
            if (prev.msg == msg && prev.parse_mode == parse_mode)
                return true;
        }
        if (_tg_q_size >= kTgQueueDepth)
        {
            _logs.warn(F("SECURITY"), F("notify queue full, drop oldest"));
            popTgNotify_();
        }
        TgNotifyItem &item = _tg_queue[_tg_q_tail];
        item.msg = msg;
        item.parse_mode = parse_mode;
        item.next_user = 0;
        _tg_q_tail = (uint8_t)((_tg_q_tail + 1) % kTgQueueDepth);
        ++_tg_q_size;
        return true;
    }

    void popTgNotify_()
    {
        if (_tg_q_size == 0)
            return;
        TgNotifyItem &item = _tg_queue[_tg_q_head];
        item.msg = "";
        item.parse_mode = "";
        item.next_user = 0;
        _tg_q_head = (uint8_t)((_tg_q_head + 1) % kTgQueueDepth);
        --_tg_q_size;
    }

    void processTgNotifyQueue_()
    {
        if (_tg_q_size == 0)
            return;
        const uint32_t now = millis();
        const int32_t delta = (int32_t)(now - _tg_last_send_ms);
        if (_tg_last_send_ms != 0 && delta < (int32_t)kTgSendGapMs)
            return;

        const auto users = _tgusers.allowedUsers();
        while (_tg_q_size > 0)
        {
            TgNotifyItem &item = _tg_queue[_tg_q_head];
            while (item.next_user < users.size)
            {
                const auto &u = users[item.next_user++];
                if (!u.enabled || !u.is_notify || u.chat_id == 0)
                    continue;
                if (item.parse_mode.length())
                    _tgbot.sendText(u.chat_id, item.msg, "", item.parse_mode);
                else
                    _tgbot.sendText(u.chat_id, item.msg);
                _tg_last_send_ms = millis();
                return;
            }
            popTgNotify_();
        }
    }


    bool isAllowedPhone_(const String &number) const
    {
        if (!_users)
            return false;
        const String norm = UsersRegistry::normalizePhone(number);
        if (norm.length() == 0)
            return false;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.gsm_phone.length() == 0)
                continue;
            if (u.gsm_phone == norm)
                return true;
        }
        return false;
    }

    bool matchPhone_(const String &number, String &user) const
    {
        if (!_users)
            return false;
        const String norm = UsersRegistry::normalizePhone(number);
        if (norm.length() == 0)
            return false;
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.gsm_phone.length() == 0)
                continue;
            if (u.gsm_phone == norm)
            {
                user = u.username;
                return true;
            }
        }
        return false;
    }

    void sendSmsNotify_(const SensorConfig &cfg)
    {
        if (!_gsm || !_users)
            return;
        String msg = F("ALARM sensor ");
        msg += String((unsigned)cfg.id);
        if (cfg.name.length())
        {
            msg += F(" (");
            msg += cfg.name;
            msg += F(")");
        }
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled || !u.gsm_sms)
                continue;
            if (u.gsm_phone.length() == 0)
                continue;
            _gsm->sendSms(u.gsm_phone, msg);
        }
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled || !u.gsm_call)
                continue;
            if (u.gsm_phone.length() == 0)
                continue;
            _gsm->driver().dial(u.gsm_phone);
        }
    }

    void sendSmsNotify_(uint8_t sensor_id, const String &name)
    {
        if (!_gsm || !_users)
            return;
        String msg = F("ALARM sensor ");
        msg += String((unsigned)sensor_id);
        if (name.length())
        {
            msg += F(" (");
            msg += name;
            msg += F(")");
        }
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled || !u.gsm_sms)
                continue;
            if (u.gsm_phone.length() == 0)
                continue;
            _gsm->sendSms(u.gsm_phone, msg);
        }
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled || !u.gsm_call)
                continue;
            if (u.gsm_phone.length() == 0)
                continue;
            _gsm->driver().dial(u.gsm_phone);
        }
    }

    bool matchKey_(const uint8_t addr[8], String &user) const
    {
        if (!_users)
            return false;
        char hex[17] = {};
        IButton::toHex(addr, hex);
        const String serial = UsersRegistry::normalizeHex(hex, 16);
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.ibutton_key.length() == 0)
                continue;
            if (u.ibutton_key != serial)
                continue;
            user = u.username.length() ? u.username : u.tg_username;
            return true;
        }
        return false;
    }

    bool matchRfidKey_(const PN532::UID &uid, String &user) const
    {
        if (uid.len == 0 || uid.len > 10 || !_users)
            return false;
        const String serial = UsersRegistry::normalizeHex(rfidUidToString_(uid.bytes, uid.len), 20);
        for (size_t i = 0; i < _users->size(); ++i)
        {
            const auto &u = _users->user(i);
            if (!u.enabled)
                continue;
            if (u.rfid_key.length() == 0)
                continue;
            if (u.rfid_key != serial)
                continue;
            user = u.username.length() ? u.username : u.tg_username;
            return true;
        }
        return false;
    }

    void logArmAction_(bool armed, const char *src, const String &user)
    {
        if (!src && user.length() == 0)
        {
            _logs.info(F("SECURITY"), F("%s"), armed ? "armed" : "disarmed");
            notifyArmAction_(armed, "local", String());
            return;
        }
        const char *who = user.length() ? user.c_str() : "unknown";
        const char *from = src ? src : "unknown";
        _logs.info(F("SECURITY"), F("%s by %s (%s)"), armed ? "armed" : "disarmed", who, from);
        notifyArmAction_(armed, src, user);
    }

    void notifyArmState_(bool armed)
    {
        if (_arm_state_cb)
            _arm_state_cb(_arm_state_ctx, armed);
    }

    void notifyAlarmState_(bool alarm_on)
    {
        if (_alarm_state_cb)
            _alarm_state_cb(_alarm_state_ctx, alarm_on);
    }

    void notifyClearDetect_()
    {
        if (_clear_detect_cb)
            _clear_detect_cb(_clear_detect_ctx);
    }

    void notifyDetectEvent_(const SensorConfig &cfg)
    {
        if (_detect_cb)
            _detect_cb(_detect_ctx, cfg.id, cfg.name, cfg.silent);
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

    static bool parseRfidUid_(const char *s, PN532::UID &out)
    {
        if (!s)
            return false;
        uint8_t bytes[10] = {};
        uint8_t len = 0;
        int hi = -1;
        for (size_t i = 0; s[i]; ++i)
        {
            const char c = s[i];
            if (c == ':' || c == '-' || c == ' ')
                continue;
            const int n = hexNibble_(c);
            if (n < 0)
                return false;
            if (hi < 0)
            {
                hi = n;
                continue;
            }
            if (len >= sizeof(bytes))
                return false;
            bytes[len++] = (uint8_t)((hi << 4) | n);
            hi = -1;
        }
        if (hi >= 0 || len == 0)
            return false;
        out.len = len;
        memcpy(out.bytes, bytes, len);
        return true;
    }

    static String rfidUidToString_(const uint8_t *bytes, uint8_t len)
    {
        static const char kHex[] = "0123456789ABCDEF";
        if (!bytes || len == 0)
            return String();
        String out;
        out.reserve(len * 2);
        for (uint8_t i = 0; i < len; ++i)
        {
            out += kHex[(bytes[i] >> 4) & 0x0F];
            out += kHex[bytes[i] & 0x0F];
        }
        return out;
    }

    static constexpr uint32_t kKeyRepeatMs = 2000;
    static constexpr uint32_t kTgSendGapMs = 800;
    static constexpr uint16_t kBeepShortMs = 120;
    static constexpr uint16_t kBeepGapMs = 120;
    static constexpr uint16_t kBeepLongMs = 500;
    static constexpr uint8_t kBeepRejectCount = 3;
    static constexpr uint16_t kBeepRejectOnMs = 60;
    static constexpr uint16_t kBeepRejectOffMs = 80;
    static constexpr uint16_t kAlarmBuzzMs = 500;
    static constexpr uint8_t kRfidI2cAddr = 0x24;
    static constexpr uint16_t kRfidReadTimeoutMs = 50;
    static constexpr uint16_t kRfidPollMs = 250;

    bool _alarm_buzz_state = false;
    uint32_t _alarm_buzz_next_ms = 0;
    ArmStateHandler _arm_state_cb = nullptr;
    void *_arm_state_ctx = nullptr;
    PreArmCheckHandler _pre_arm_cb = nullptr;
    void *_pre_arm_ctx = nullptr;
    AlarmStateHandler _alarm_state_cb = nullptr;
    void *_alarm_state_ctx = nullptr;
    ClearDetectHandler _clear_detect_cb = nullptr;
    void *_clear_detect_ctx = nullptr;
    DetectHandler _detect_cb = nullptr;
    void *_detect_ctx = nullptr;
    bool _notify_enabled = true;
};


