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

#include "controllers/security_controller.hpp"

#include <string.h>

#include "boards/board_profile.hpp"

namespace
{
constexpr uint32_t kSecurityPortDebounceMs = 1000u;

bool phonesMatch_(const String &lhs, const String &rhs)
{
    const String a = UsersRegistry::normalizePhone(lhs);
    const String b = UsersRegistry::normalizePhone(rhs);
    if (a.length() == 0 || b.length() == 0)
        return false;
    if (a == b)
        return true;
    if (a.length() < 10 || b.length() < 10)
        return false;
    return a.substring(a.length() - 10) == b.substring(b.length() - 10);
}

String formatOutgoingPhone_(const String &raw)
{
    const String norm = UsersRegistry::normalizePhone(raw);
    if (norm.length() == 0)
        return "";
    return String("+") + norm;
}
}

SecurityController::SecurityController(Gpio &gpio, OneWireManager &ow, Logger &logs)
 : _gpio(gpio), _ow(ow), _logs(logs){
    reset_();
}

bool SecurityController::begin(){
    auto guard = _lock.guard();
    _runtime_ready = true;
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
        bool raw = idleRawState_(cfg);
        if (readRaw_(cfg, raw))
            st.raw = raw;
        else
            st.raw = idleRawState_(cfg);
        st.filtered_raw = idleRawState_(cfg);
        st.active = false;
        st.raw_changed_ms = millis();
        st.is_detect = false;
    }
    _logs.info(F("SECURITY"), F("Init done"));
    return true;
}

void SecurityController::task(){
    handleGsm_();
    SensorConfig pending_detect[kSensorCount]{};
    size_t pending_detect_count = 0;
    bool notify_alarm_on = false;
    const uint32_t now = millis();
    {
        auto guard = _lock.guard();
        if (!_controller_enabled)
            return;
        handleIButton_();
        handleRfid_();
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            SensorConfig &cfg = _cfg[i];
            SensorState &st = _state[i];
            if (!cfg.enabled)
            {
                st.active = false;
                continue;
            }
            if (cfg.port == kInvalidPort)
            {
                st.active = false;
                continue;
            }
            const bool triggered = sampleTriggered_(cfg, st, now);
            st.active = triggered;
            if (!_armed)
                continue;
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
                        notify_alarm_on = true;
                }
                if (pending_detect_count < kSensorCount)
                    pending_detect[pending_detect_count++] = cfg;
            }
        }
        updateBuzzer_();
    }
    if (notify_alarm_on)
        notifyAlarmState_(true);
    for (size_t i = 0; i < pending_detect_count; ++i)
    {
        logDetect_(pending_detect[i]);
        notifyDetect_(pending_detect[i]);
    }
}

void SecurityController::applyConfig(JsonArrayConst sensors){
    auto guard = _lock.guard();
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
        if (obj["group_id"].is<unsigned>())
        {
            const unsigned raw = obj["group_id"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.group_id = (uint8_t)raw;
        }
        if (!enabled_set)
            cfg.enabled = true;
        ++idx;
    }
}

void SecurityController::applyKeys(JsonArrayConst keys){
    auto guard = _lock.guard();
    if (!_users)
        return;
    const auto users_guard = _users->guard();
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

void SecurityController::applyRfidKeys(JsonArrayConst keys){
    auto guard = _lock.guard();
    if (!_users)
        return;
    const auto users_guard = _users->guard();
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

void SecurityController::applyPhones(JsonArrayConst phones){
    auto guard = _lock.guard();
    if (!_users)
        return;
    const auto users_guard = _users->guard();
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

void SecurityController::setSirenPort(uint8_t port){
    auto guard = _lock.guard();
    _siren_port = port;
    if (_controller_enabled)
        setupSiren_();
}

void SecurityController::serialize(JsonArray out) const{
    auto guard = _lock.guard();
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
        if (cfg.group_id != 0)
            obj["group_id"] = cfg.group_id;
    }
}

void SecurityController::serializeKeys(JsonArray out) const{
    auto guard = _lock.guard();
    if (!_users)
        return;
    const auto users_guard = _users->guard();
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

void SecurityController::serializeRfidKeys(JsonArray out) const{
    auto guard = _lock.guard();
    if (!_users)
        return;
    const auto users_guard = _users->guard();
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

void SecurityController::serializePhones(JsonArray out) const{
    auto guard = _lock.guard();
    if (!_users)
        return;
    const auto users_guard = _users->guard();
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

void SecurityController::buildSnapshot(uint8_t &flags) const{
    auto guard = _lock.guard();
    flags = 0;
    if (_armed)
        flags |= EepromStorage::kSecurityArmedMask;
    if (_alarm_on)
        flags |= EepromStorage::kSecurityAlarmMask;
}

void SecurityController::applySnapshot(uint8_t flags){
    auto guard = _lock.guard();
    const bool armed = (flags & EepromStorage::kSecurityArmedMask) != 0;
    const bool alarm = armed && ((flags & EepromStorage::kSecurityAlarmMask) != 0);
    applySnapshot_(armed, alarm);
}

void SecurityController::setArmStateHandler(SecurityController::ArmStateHandler cb, void *ctx){
    auto guard = _lock.guard();
    _arm_state_cb = cb;
    _arm_state_ctx = ctx;
}

void SecurityController::setArmStateHandlerSecondary(SecurityController::ArmStateHandler cb, void *ctx){
    auto guard = _lock.guard();
    _arm_state_cb_secondary = cb;
    _arm_state_ctx_secondary = ctx;
}

void SecurityController::setPreArmCheckHandler(SecurityController::PreArmCheckHandler cb, void *ctx){
    auto guard = _lock.guard();
    _pre_arm_cb = cb;
    _pre_arm_ctx = ctx;
}

void SecurityController::setAlarmStateHandler(SecurityController::AlarmStateHandler cb, void *ctx){
    auto guard = _lock.guard();
    _alarm_state_cb = cb;
    _alarm_state_ctx = ctx;
}

void SecurityController::setAlarmStateHandlerSecondary(SecurityController::AlarmStateHandler cb, void *ctx){
    auto guard = _lock.guard();
    _alarm_state_cb_secondary = cb;
    _alarm_state_ctx_secondary = ctx;
}

void SecurityController::setClearDetectHandler(SecurityController::ClearDetectHandler cb, void *ctx){
    auto guard = _lock.guard();
    _clear_detect_cb = cb;
    _clear_detect_ctx = ctx;
}

void SecurityController::setClearDetectHandlerSecondary(SecurityController::ClearDetectHandler cb, void *ctx){
    auto guard = _lock.guard();
    _clear_detect_cb_secondary = cb;
    _clear_detect_ctx_secondary = ctx;
}

void SecurityController::setDetectHandler(SecurityController::DetectHandler cb, void *ctx){
    auto guard = _lock.guard();
    _detect_cb = cb;
    _detect_ctx = ctx;
}

void SecurityController::setDetectHandlerSecondary(SecurityController::DetectHandler cb, void *ctx){
    auto guard = _lock.guard();
    _detect_cb_secondary = cb;
    _detect_ctx_secondary = ctx;
}

void SecurityController::setRfidUidHandler(SecurityController::RfidUidHandler cb, void *ctx){
    auto guard = _lock.guard();
    _rfid_uid_cb = cb;
    _rfid_uid_ctx = ctx;
}

void SecurityController::setIButtonSerialHandler(SecurityController::IButtonSerialHandler cb, void *ctx){
    auto guard = _lock.guard();
    _ibutton_serial_cb = cb;
    _ibutton_serial_ctx = ctx;
}

void SecurityController::setRfidI2c(I2CManager *i2c){
    auto guard = _lock.guard();
    _rfid_i2c = i2c;
}

void SecurityController::setUsersRegistry(UsersRegistry &users){
    auto guard = _lock.guard();
    _users = &users;
}

void SecurityController::setPlcControl(PlcControl &plc){
    auto guard = _lock.guard();
    _plc = &plc;
}

bool SecurityController::processRfidUid(const PN532::UID &uid, const char *src ){
    auto guard = _lock.guard();
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

bool SecurityController::processRfidUidString(const char *uid_str, const char *src ){
    auto guard = _lock.guard();
    PN532::UID uid;
    if (!parseRfidUid_(uid_str, uid))
        return false;
    return processRfidUid(uid, src);
}

bool SecurityController::processIButtonAddr(const uint8_t addr[8], const char *src ){
    auto guard = _lock.guard();
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

bool SecurityController::processIButtonSerialString(const char *serial, const char *src ){
    auto guard = _lock.guard();
    uint8_t addr[8] = {};
    if (!parseHexAddr_(serial, addr))
        return false;
    return processIButtonAddr(addr, src);
}

void SecurityController::setNotifyEnabled(bool enabled){
    auto guard = _lock.guard();
    _notify_enabled = enabled;
}

bool SecurityController::takeDirty(){
    auto guard = _lock.guard();
    if (!_dirty)
        return false;
    _dirty = false;
    return true;
}

bool SecurityController::takeForceSave(){
    auto guard = _lock.guard();
    if (!_force_save)
        return false;
    _force_save = false;
    return true;
}

bool SecurityController::controllerEnabled() const{
    auto guard = _lock.guard();
    return _controller_enabled;
}

void SecurityController::setControllerEnabled(bool enabled){
    auto guard = _lock.guard();
    if (_controller_enabled == enabled)
        return;
    _controller_enabled = enabled;
    if (!_controller_enabled)
    {
        disarm_(true);
        for (size_t i = 0; i < kSensorCount; ++i)
            if (_cfg[i].enabled)
                setSensorPortDebounce_(_cfg[i].port, false);
        for (size_t i = 0; i < kSensorCount; ++i)
            _state[i] = SensorState{};
        _rfid_ready = false;
        reset_();
        return;
    }
    _logs.info(F("SECURITY"), F("controller: enabled"));
    // Config can enable security before HAL has initialized I2C/OneWire.
    // Defer all hardware touches until begin() marks runtime ready.
    if (!_runtime_ready)
        return;
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
        bool raw = idleRawState_(cfg);
        if (readRaw_(cfg, raw))
            st.raw = raw;
        else
            st.raw = idleRawState_(cfg);
        st.filtered_raw = idleRawState_(cfg);
        st.active = false;
        st.raw_changed_ms = millis();
    }
}

bool SecurityController::armed() const{
    auto guard = _lock.guard();
    return _armed;
}

bool SecurityController::alarmOn() const{
    auto guard = _lock.guard();
    return _alarm_on;
}

uint8_t SecurityController::sirenPort() const{
    auto guard = _lock.guard();
    return _siren_port;
}

void SecurityController::setGsmModem(GsmModem &modem){
    auto guard = _lock.guard();
    _gsm = &modem;
}

bool SecurityController::arm(){
    auto guard = _lock.guard();
    if (_armed)
        return true;
    if (!_controller_enabled)
        return false;
    const bool was_armed = _armed;
    arm_();
    return _armed && !was_armed;
}

bool SecurityController::disarm(){
    auto guard = _lock.guard();
    if (!_armed)
        return true;
    disarm_(false);
    return true;
}

bool SecurityController::armFrom(const char *src, const String &user){
    auto guard = _lock.guard();
    if (_armed)
        return true;
    if (!_controller_enabled)
        return false;
    const bool was_armed = _armed;
    arm_(src, user);
    return _armed && !was_armed;
}

bool SecurityController::armForcedFrom(const char *src, const String &user){
    auto guard = _lock.guard();
    if (_armed)
        return true;
    if (!_controller_enabled)
        return false;
    armForce_(src, user);
    return true;
}

bool SecurityController::disarmFrom(const char *src, const String &user, bool silent ){
    auto guard = _lock.guard();
    disarm_(silent, src, user);
    return true;
}

void SecurityController::toggleFrom(const char *src, const String &user){
    auto guard = _lock.guard();
    if (_armed)
        disarm_(false, src, user);
    else
        arm_(src, user);
}

void SecurityController::clearDetect(){
    auto guard = _lock.guard();
    clearDetect_();
    notifyClearDetect_();
}

bool SecurityController::fillPrearmItems(JsonArray &arr, String *plain_out ){
    auto guard = _lock.guard();
    bool any = false;
    if (plain_out)
        *plain_out = "";
    const uint32_t now = millis();
    for (size_t i = 0; i < kSensorCount; ++i)
    {
        SensorConfig &cfg = _cfg[i];
        SensorState &st = _state[i];
        if (!cfg.enabled)
            continue;
        if (cfg.port == kInvalidPort)
            continue;
        if (!sampleTriggered_(cfg, st, now))
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

size_t SecurityController::prearmTriggeredCount(){
    auto guard = _lock.guard();
    size_t count = 0;
    const uint32_t now = millis();
    for (size_t i = 0; i < kSensorCount; ++i)
    {
        const SensorConfig &cfg = _cfg[i];
        if (!cfg.enabled)
            continue;
        if (cfg.port == kInvalidPort)
            continue;
        if (sampleTriggered_(cfg, _state[i], now))
            ++count;
    }
    return count;
}

void SecurityController::notifyRemoteDetect(uint32_t node_id, const String &source, uint8_t sensor_id, const String &name, bool silent){
    bool notify_enabled = false;
    {
        auto guard = _lock.guard();
        const uint32_t now = millis();
        size_t slot = kRemoteDetectCapacity;
        size_t free_slot = kRemoteDetectCapacity;
        uint32_t oldest_ms = UINT32_MAX;
        size_t oldest_slot = 0;
        for (size_t i = 0; i < kRemoteDetectCapacity; ++i)
        {
            auto &item = _remote_detects[i];
            if (item.active && item.node_id == node_id && item.sensor_id == sensor_id)
            {
                slot = i;
                break;
            }
            if (!item.active && free_slot == kRemoteDetectCapacity)
                free_slot = i;
            if (item.updated_ms < oldest_ms)
            {
                oldest_ms = item.updated_ms;
                oldest_slot = i;
            }
        }
        if (slot == kRemoteDetectCapacity)
            slot = (free_slot != kRemoteDetectCapacity) ? free_slot : oldest_slot;
        auto &dst = _remote_detects[slot];
        dst.node_id = node_id;
        dst.sensor_id = sensor_id;
        dst.active = true;
        dst.silent = silent;
        dst.updated_ms = now;
        dst.unit_name = source;
        dst.sensor_name = name;
        notify_enabled = _notify_enabled;
    }
    if (!notify_enabled)
        return;
    SensorConfig cfg;
    cfg.id = sensor_id;
    cfg.name = name;
    cfg.silent = silent;
    notifyDetectEvent_(cfg);
    sendSmsNotify_(sensor_id, name);
}

size_t SecurityController::remoteDetectCount(uint32_t node_id) const{
    auto guard = _lock.guard();
    size_t count = 0;
    for (size_t i = 0; i < kRemoteDetectCapacity; ++i)
    {
        const auto &item = _remote_detects[i];
        if (!item.active)
            continue;
        if (node_id != 0 && item.node_id != node_id)
            continue;
        ++count;
    }
    return count;
}

bool SecurityController::remoteDetectAt(size_t idx, RemoteDetect &out, uint32_t node_id) const{
    auto guard = _lock.guard();
    size_t current = 0;
    for (size_t i = 0; i < kRemoteDetectCapacity; ++i)
    {
        const auto &item = _remote_detects[i];
        if (!item.active)
            continue;
        if (node_id != 0 && item.node_id != node_id)
            continue;
        if (current == idx)
        {
            out = item;
            return true;
        }
        ++current;
    }
    return false;
}

void SecurityController::clearRemoteDetects(uint32_t node_id){
    auto guard = _lock.guard();
    for (size_t i = 0; i < kRemoteDetectCapacity; ++i)
    {
        auto &item = _remote_detects[i];
        if (!item.active)
            continue;
        if (node_id != 0 && item.node_id != node_id)
            continue;
        item = RemoteDetect{};
    }
}

void SecurityController::setAlarmState(bool on){
    auto guard = _lock.guard();
    if (_alarm_on == on)
        return;
    _alarm_on = on;
    _dirty = true;
    _force_save = true;
    resetAlarmBuzzer_();
    updateSiren_();
    notifyAlarmState_(on);
}

bool SecurityController::setEnabled(size_t id, bool enabled){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    SensorConfig &cfg = _cfg[idx];
    SensorState &st = _state[idx];
    if (!enabled)
    {
        setSensorPortDebounce_(cfg.port, false);
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
        bool raw = idleRawState_(cfg);
        if (readRaw_(cfg, raw))
            st.raw = raw;
        else
            st.raw = idleRawState_(cfg);
        st.filtered_raw = idleRawState_(cfg);
        st.active = false;
        st.raw_changed_ms = millis();
    }
    _logs.info(F("SECURITY"), F("id: %u enabled: true"), (unsigned)cfg.id);
    return true;
}

bool SecurityController::setType(size_t id, SecurityController::SensorType type){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    SensorConfig &cfg = _cfg[idx];
    SensorState &st = _state[idx];
    setSensorPortDebounce_(cfg.port, false);
    cfg.type = type;
    st = SensorState{};
    if (_controller_enabled && cfg.enabled)
    {
        setupSensorInput_(cfg);
        bool raw = idleRawState_(cfg);
        if (readRaw_(cfg, raw))
            st.raw = raw;
        else
            st.raw = idleRawState_(cfg);
        st.filtered_raw = idleRawState_(cfg);
        st.active = false;
        st.raw_changed_ms = millis();
    }
    return true;
}

bool SecurityController::setPort(size_t id, uint8_t port){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    SensorConfig &cfg = _cfg[idx];
    SensorState &st = _state[idx];
    setSensorPortDebounce_(cfg.port, false);
    cfg.port = port;
    st = SensorState{};
    if (_controller_enabled && cfg.enabled)
    {
        setupSensorInput_(cfg);
        bool raw = idleRawState_(cfg);
        if (readRaw_(cfg, raw))
            st.raw = raw;
        else
            st.raw = idleRawState_(cfg);
        st.filtered_raw = idleRawState_(cfg);
        st.active = false;
        st.raw_changed_ms = millis();
    }
    return true;
}

bool SecurityController::setName(size_t id, const String &name){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    _cfg[idx].name = name;
    return true;
}

bool SecurityController::setGroupId(size_t id, uint8_t group_id){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    _cfg[idx].group_id = group_id;
    return true;
}

bool SecurityController::setSilent(size_t id, bool silent){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    _cfg[idx].silent = silent;
    return true;
}

bool SecurityController::addKey(const uint8_t addr[8]){
    auto guard = _lock.guard();
    return addKey(addr, "");
}

bool SecurityController::addKey(const uint8_t addr[8], const String &name){
    auto guard = _lock.guard();
    if (!addr || !_users)
        return false;
    const auto users_guard = _users->guard();
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

bool SecurityController::removeKey(const uint8_t addr[8]){
    auto guard = _lock.guard();
    if (!addr || !_users)
        return false;
    const auto users_guard = _users->guard();
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

void SecurityController::clearKeys(){
    auto guard = _lock.guard();
    if (!_users)
        return;
    const auto users_guard = _users->guard();
    for (size_t i = 0; i < _users->size(); ++i)
        _users->user(i).ibutton_key = "";
}

void SecurityController::clearPhones(){
    auto guard = _lock.guard();
    clearPhones_();
}

bool SecurityController::setPhone(size_t idx, const String &number){
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    _users->user(idx).gsm_phone = UsersRegistry::normalizePhone(number);
    return true;
}

bool SecurityController::setPhoneName(size_t idx, const String &name){
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    _users->user(idx).username = name;
    return true;
}

bool SecurityController::setPhoneNotify(size_t idx, bool notify){
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    _users->user(idx).gsm_sms = notify;
    return true;
}

bool SecurityController::setPhoneCall(size_t idx, bool call){
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    _users->user(idx).gsm_call = call;
    return true;
}

bool SecurityController::setPhoneEnabled(size_t idx, bool enabled){
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    _users->user(idx).enabled = enabled;
    return true;
}

bool SecurityController::phoneSlot(size_t idx, String &number, bool &enabled) const{
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    const auto &u = _users->user(idx);
    number = u.gsm_phone;
    enabled = u.enabled;
    return true;
}

const String &SecurityController::phoneByIndex(size_t idx) const{
    auto guard = _lock.guard();
    static const String empty;
    if (!_users || idx >= _users->size())
        return empty;
    const auto users_guard = _users->guard();
    _user_phone_cache = _users->user(idx).gsm_phone;
    return _user_phone_cache.length() ? _user_phone_cache : empty;
}

const String &SecurityController::phoneNameByIndex(size_t idx) const{
    auto guard = _lock.guard();
    static const String empty;
    if (!_users || idx >= _users->size())
        return empty;
    const auto users_guard = _users->guard();
    _user_phone_name_cache = _users->user(idx).username;
    return _user_phone_name_cache.length() ? _user_phone_name_cache : empty;
}

bool SecurityController::phoneNotifyByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    return _users->user(idx).gsm_sms;
}

bool SecurityController::phoneCallByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    return _users->user(idx).gsm_call;
}

size_t SecurityController::keyCount() const{
    auto guard = _lock.guard();
    size_t count = 0;
    if (!_users)
        return 0;
    const auto users_guard = _users->guard();
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

bool SecurityController::keyByIndex(size_t idx, uint8_t out[8]) const{
    auto guard = _lock.guard();
    if (!out || !_users)
        return false;
    const auto users_guard = _users->guard();
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

const String &SecurityController::keyNameByIndex(size_t idx) const{
    auto guard = _lock.guard();
    static const String empty;
    if (!_users)
        return empty;
    const auto users_guard = _users->guard();
    size_t seen = 0;
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled || u.ibutton_key.length() == 0)
            continue;
        if (seen == idx)
        {
            _user_key_name_cache = u.username;
            return _user_key_name_cache.length() ? _user_key_name_cache : empty;
        }
        ++seen;
    }
    return empty;
}

bool SecurityController::setKeyNameByAddr(const uint8_t addr[8], const String &name){
    auto guard = _lock.guard();
    if (!addr || !_users)
        return false;
    const auto users_guard = _users->guard();
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

bool SecurityController::keySlot(size_t idx, uint8_t out[8], bool &enabled) const{
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    const String serial = _users->user(idx).ibutton_key;
    enabled = serial.length() > 0;
    if (out)
    {
        if (!enabled || !parseHexAddr_(serial.c_str(), out))
            memset(out, 0, 8);
    }
    return true;
}

bool SecurityController::lastKeyHex(char out[17]) const{
    auto guard = _lock.guard();
    if (!out || _last_key_ms == 0)
        return false;
    IButton::toHex(_last_key, out);
    return true;
}

bool SecurityController::lastRfidSerial(String &out) const{
    auto guard = _lock.guard();
    if (_last_rfid_ms == 0 || _last_rfid_len == 0)
        return false;
    out = rfidUidToString_(_last_rfid, _last_rfid_len);
    return out.length() > 0;
}

bool SecurityController::setKeySlot(size_t idx, const uint8_t addr[8], bool enabled, const String &name){
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
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

bool SecurityController::rfidKeySlot(size_t idx, uint8_t out[10], uint8_t &len, bool &enabled) const{
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
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

const String &SecurityController::rfidKeyNameByIndex(size_t idx) const{
    auto guard = _lock.guard();
    static const String empty;
    if (!_users)
        return empty;
    const auto users_guard = _users->guard();
    size_t seen = 0;
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled || u.rfid_key.length() == 0)
            continue;
        if (seen == idx)
        {
            _user_rfid_name_cache = u.username;
            return _user_rfid_name_cache.length() ? _user_rfid_name_cache : empty;
        }
        ++seen;
    }
    return empty;
}

bool SecurityController::setRfidKeySlot(size_t idx, const uint8_t *bytes, uint8_t len, bool enabled, const String &name){
    auto guard = _lock.guard();
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
    UsersRegistry::User &u = _users->user(idx);
    if (enabled && bytes && len > 0)
        u.rfid_key = UsersRegistry::normalizeHex(rfidUidToString_(bytes, len), 20);
    else
        u.rfid_key = "";
    u.username = name;
    return true;
}

bool SecurityController::parseRfidSerial(const char *s, uint8_t out[10], uint8_t &len){
    PN532::UID uid;
    if (!parseRfidUid_(s, uid))
        return false;
    len = uid.len;
    memcpy(out, uid.bytes, uid.len);
    return true;
}

String SecurityController::rfidSerialToString(const uint8_t *bytes, uint8_t len){
    return rfidUidToString_(bytes, len);
}

const SecurityController::SensorConfig *SecurityController::config(size_t id) const{
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return nullptr;
    return &_cfg[idx];
}

const SecurityController::SensorState *SecurityController::state(size_t id) const{
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return nullptr;
    return &_state[idx];
}

const SecurityController::SensorConfig *SecurityController::configByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kSensorCount)
        return nullptr;
    return &_cfg[idx];
}

const SecurityController::SensorState *SecurityController::stateByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kSensorCount)
        return nullptr;
    return &_state[idx];
}

void SecurityController::reset_(){
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

void SecurityController::clearKeys_(){
    if (_users)
    {
        const auto users_guard = _users->guard();
        for (size_t i = 0; i < _users->size(); ++i)
            _users->user(i).ibutton_key = "";
    }
    memset(_last_key, 0, sizeof(_last_key));
    _last_key_ms = 0;
}

void SecurityController::clearRfidKeys_(){
    if (_users)
    {
        const auto users_guard = _users->guard();
        for (size_t i = 0; i < _users->size(); ++i)
            _users->user(i).rfid_key = "";
    }
    memset(_last_rfid, 0, sizeof(_last_rfid));
    _last_rfid_len = 0;
    _last_rfid_ms = 0;
}

bool SecurityController::setRfidKeySlot_(size_t idx, const PN532::UID &uid, bool enabled, const String &name){
    if (!_users || idx >= _users->size())
        return false;
    const auto users_guard = _users->guard();
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

void SecurityController::clearPhones_(){
    if (!_users)
        return;
    const auto users_guard = _users->guard();
    for (size_t i = 0; i < _users->size(); ++i)
    {
        auto &u = _users->user(i);
        u.gsm_phone = "";
        u.gsm_sms = false;
        u.gsm_call = false;
    }
}

bool SecurityController::indexById_(uint8_t id, size_t &out){
    if (id == 0 || id > kSensorCount)
        return false;
    out = (size_t)(id - 1);
    return true;
}

const char *SecurityController::typeName_(SecurityController::SensorType t){
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

bool SecurityController::parseType_(const char *s, SecurityController::SensorType &out){
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

void SecurityController::setupOutputs_(){
    setupBuzzer_();
    setupAlarmLed_();
    setupSiren_();
    updateAlarmLed_();
    updateSiren_();
}

void SecurityController::setupBuzzer_(){
    _gpio.pinModeDyn(ActiveBoardProfile::BUZZER_PIN, PortIO::PortMode::Output);
    writeBuzzer_(false);
}

void SecurityController::setupAlarmLed_(){
    _gpio.pinModeDyn(ActiveBoardProfile::ALARM_LED_PIN, PortIO::PortMode::Output);
    _gpio.writeDyn(ActiveBoardProfile::ALARM_LED_PIN, false);
}

void SecurityController::setupSiren_(){
    if (_siren_port == kInvalidPort)
        return;
    _gpio.pinModeDyn(_siren_port, PortIO::PortMode::Output);
    _gpio.writeDyn(_siren_port, false);
}

void SecurityController::updateAlarmLed_(){
    _gpio.writeDyn(ActiveBoardProfile::ALARM_LED_PIN, _armed);
}

void SecurityController::updateSiren_(){
    if (_siren_port == kInvalidPort)
        return;
    _gpio.writeDyn(_siren_port, _alarm_on);
}

void SecurityController::setupSensorInput_(const SecurityController::SensorConfig &cfg){
    if (cfg.port == kInvalidPort)
        return;
    const PortIO::PortMode mode = PortIO::PortMode::InputPullUp;
    _gpio.pinModeDyn(cfg.port, mode);
    setSensorPortDebounce_(cfg.port, true);
}

void SecurityController::setSensorPortDebounce_(uint8_t port, bool enabled){
    if (port == kInvalidPort)
        return;
    _gpio.setInputDebounceMsDyn(port, enabled ? kSecurityPortDebounceMs : 0u);
}

bool SecurityController::readRaw_(const SecurityController::SensorConfig &cfg, bool &out){
    if (cfg.port == kInvalidPort)
        return false;
    if (!_gpio.readDyn(cfg.port, out))
        return false;
    return true;
}

bool SecurityController::isTriggered_(const SecurityController::SensorConfig &cfg, bool raw){
    return (cfg.type == SensorType::Reed) ? !raw : raw;
}

bool SecurityController::idleRawState_(const SecurityController::SensorConfig &cfg){
    return (cfg.type == SensorType::Reed);
}

uint32_t SecurityController::debounceMs_(const SecurityController::SensorConfig &cfg){
    (void)cfg;
    return 1000u;
}

bool SecurityController::sampleTriggered_(const SecurityController::SensorConfig &cfg, SensorState &st, uint32_t now_ms){
    bool raw = st.raw;
    if (!readRaw_(cfg, raw))
        return isTriggered_(cfg, st.filtered_raw);
    const bool raw_detect = isTriggered_(cfg, raw);
    if (raw != st.raw)
    {
        st.raw = raw;
        st.raw_changed_ms = now_ms;
        st.debounce_try = 0;
    }
    const bool stable_triggered = isTriggered_(cfg, st.filtered_raw);
    const bool candidate_triggered = isTriggered_(cfg, st.raw);
    const uint32_t try_no = ++st.debounce_try;
    if (candidate_triggered == stable_triggered)
        return stable_triggered;
    if (!candidate_triggered)
    {
        // Clearing a confirmed detect should be fast; GPIO layer already debounced the raw input.
        st.filtered_raw = st.raw;
        return false;
    }
    const uint32_t debounce_ms = debounceMs_(cfg);
    const uint32_t age_ms = (uint32_t)(now_ms - st.raw_changed_ms);
    if (debounce_ms == 0 || age_ms >= debounce_ms)
    {
        st.filtered_raw = st.raw;
    }
    return isTriggered_(cfg, st.filtered_raw);
}

void SecurityController::initIButton_(){
    _ibutton_ready = false;
    OneWireBus *bus = _ow.busPtrById(OneWireManager::OwBusType::iButton);
    if (!bus)
    {
        _logs.warn(F("SECURITY"), F("iButton bus missing"));
        return;
    }
    _ibutton.setBusLockCallbacks(&SecurityController::owIButtonLockCb_, &SecurityController::owIButtonUnlockCb_, this);
    _ibutton.begin(*bus);
    _ibutton_ready = true;
}

void SecurityController::initRfid_(bool startup ){
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
    I2CManager::ScopedBusLock lk(*_rfid_i2c, cfg.bus_num);
    if (!lk.locked())
    {
        _logs.warn(F("SECURITY"), F("RFID lock timeout"));
        return;
    }
    _rfid_ready = _rfid.begin(*wire, sda_gpio, scl_gpio, cfg.freq, kRfidI2cAddr, -1, -1);
    if (!_rfid_ready)
        _logs.warn(F("SECURITY"), F("RFID init failed"));
}

void SecurityController::handleRfid_(){
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
    I2CManager::ScopedBusLock lk(*_rfid_i2c, cfg.bus_num);
    if (!lk.locked())
        return;
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

void SecurityController::handleIButton_(){
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

bool SecurityController::owIButtonLockCb_(void *ctx, uint32_t timeout_ms)
{
    auto *self = static_cast<SecurityController *>(ctx);
    return self ? self->_ow.lockBusById(OneWireManager::OwBusType::iButton, timeout_ms) : false;
}

void SecurityController::owIButtonUnlockCb_(void *ctx)
{
    auto *self = static_cast<SecurityController *>(ctx);
    if (self)
        self->_ow.unlockBusById(OneWireManager::OwBusType::iButton);
}

bool SecurityController::isAllowedKey_(const uint8_t addr[8]) const{
    if (!_users)
        return false;
    const auto users_guard = _users->guard();
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

bool SecurityController::isKeyRepeat_(const uint8_t addr[8]){
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

bool SecurityController::isRfidRepeat_(const PN532::UID &uid){
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

bool SecurityController::portToGpio_(uint8_t port, uint8_t &out_gpio){
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

void SecurityController::toggleArm_(){
    if (_armed)
        disarm_(false);
    else
        arm_();
}

void SecurityController::toggleArm_(const char *src, const String &user){
    if (_armed)
        disarm_(false, src, user);
    else
        arm_(src, user);
}

void SecurityController::handleGsm_(){
    if (!_gsm)
        return;
    String number;
    if (!_gsm->takeLastCall(number))
        return;
    _gsm->hangup();
    String user;
    auto guard = _lock.guard();
    if (!matchPhone_(number, user))
    {
        _logs.warn(F("SECURITY"), F("GSM call ignored, unknown number: %s"), number.c_str());
        return;
    }
    if (_armed)
    {
        disarm_(false, "gsm", user);
    }
    else
    {
        arm_("gsm", user);
    }
}

void SecurityController::arm_(){
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
        _logs.warn(F("SECURITY"), F("arm blocked: local sensors active"));
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

void SecurityController::disarm_(bool silent){
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
    _force_save = true;
    logArmAction_(false, nullptr, String());
    notifyArmState_(false);
    if (was_alarm)
        notifyAlarmState_(false);
}

void SecurityController::arm_(const char *src, const String &user){
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
        _logs.warn(F("SECURITY"), F("arm blocked: sensors active src: %s user: %s"),
                   from, who);
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

void SecurityController::armForce_(const char *src, const String &user){
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

void SecurityController::disarm_(bool silent, const char *src, const String &user){
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
    _force_save = true;
    logArmAction_(false, src, user);
    notifyArmState_(false);
    if (was_alarm)
        notifyAlarmState_(false);
}

void SecurityController::applySnapshot_(bool armed, bool alarm){
    const bool was_armed = _armed;
    const bool was_alarm = _alarm_on;
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
        writeBuzzer_(false);
        if (armed && (!was_armed || !was_alarm))
            startBeep_(2, kBeepShortMs, kBeepGapMs);
    }
    _dirty = false;
    _force_save = false;
    if (armed)
    {
        if (alarm)
            _logs.warn(F("SECURITY"), F("armed restored after restart, alarm: on"));
        else
            _logs.info(F("SECURITY"), F("armed restored after restart"));
    }
    else if (was_armed)
    {
        _logs.info(F("SECURITY"), F("disarmed restored after restart"));
    }
}

bool SecurityController::hasTriggeredBeforeArm_(String &out, String *plain_out ){
    out = "";
    if (plain_out)
        *plain_out = "";
    bool any = false;
    bool local_any = false;
    const uint32_t now = millis();
    for (size_t i = 0; i < kSensorCount; ++i)
    {
        SensorConfig &cfg = _cfg[i];
        SensorState &st = _state[i];
        if (!cfg.enabled)
            continue;
        if (cfg.port == kInvalidPort)
            continue;
        if (!sampleTriggered_(cfg, st, now))
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

String SecurityController::escapeHtml_(const char *text){
    if (!text)
        return String();
    return escapeHtml_(String(text));
}

String SecurityController::escapeHtml_(const String &text){
    String out = text;
    out.replace("&", "&amp;");
    out.replace("<", "&lt;");
    out.replace(">", "&gt;");
    return out;
}

void SecurityController::clearDetect_(){
    for (size_t i = 0; i < kSensorCount; ++i)
        _state[i].is_detect = false;
    for (size_t i = 0; i < kRemoteDetectCapacity; ++i)
        _remote_detects[i] = RemoteDetect{};
}

void SecurityController::startBeep_(uint8_t count, uint16_t on_ms, uint16_t off_ms){
    if (!buzzerEnabled_())
    {
        _beep_remaining = 0;
        _beep_state_on = false;
        writeBuzzer_(false);
        return;
    }
    _beep_remaining = count;
    _beep_on_ms = on_ms;
    _beep_off_ms = off_ms;
    _beep_state_on = true;
    _beep_next_ms = millis() + on_ms;
    writeBuzzer_(true);
}

void SecurityController::updateBuzzer_(){
    if (!buzzerEnabled_())
    {
        _beep_remaining = 0;
        _beep_state_on = false;
        _alarm_buzz_state = false;
        writeBuzzer_(false);
        return;
    }
    if (_alarm_on)
    {
        updateAlarmBuzzer_();
        return;
    }
    if (_alarm_buzz_state)
    {
        _alarm_buzz_state = false;
        writeBuzzer_(false);
    }
    if (_beep_remaining == 0)
        return;
    const uint32_t now = millis();
    if ((int32_t)(now - _beep_next_ms) < 0)
        return;

    if (_beep_state_on)
    {
        _beep_state_on = false;
        writeBuzzer_(false);
        if (_beep_off_ms == 0)
        {
            if (_beep_remaining > 0)
                --_beep_remaining;
            if (_beep_remaining == 0)
                return;
            _beep_state_on = true;
            writeBuzzer_(true);
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
    writeBuzzer_(true);
    _beep_next_ms = now + _beep_on_ms;
}

void SecurityController::updateAlarmBuzzer_(){
    const uint32_t now = millis();
    if ((int32_t)(now - _alarm_buzz_next_ms) < 0)
        return;
    _alarm_buzz_state = !_alarm_buzz_state;
    writeBuzzer_(_alarm_buzz_state);
    _alarm_buzz_next_ms = now + kAlarmBuzzMs;
}

void SecurityController::resetAlarmBuzzer_(){
    _alarm_buzz_state = false;
    _alarm_buzz_next_ms = millis() + kAlarmBuzzMs;
    writeBuzzer_(false);
}

bool SecurityController::buzzerEnabled_() const{
    return !_plc || _plc->buzzerEnabled();
}

void SecurityController::writeBuzzer_(bool on){
    _gpio.writeDyn(ActiveBoardProfile::BUZZER_PIN, buzzerEnabled_() ? on : false);
}

void SecurityController::logDetect_(const SecurityController::SensorConfig &cfg){
    _logs.warn(F("SECURITY"), F("detect id: %u type: %s"),
               (unsigned)cfg.id, typeName_(cfg.type));
}

void SecurityController::notifyDetect_(const SecurityController::SensorConfig &cfg){
    notifyDetectEvent_(cfg);
    if (!_notify_enabled)
        return;
    sendSmsNotify_(cfg);
}

void SecurityController::notifyArmAction_(bool armed, const char *src, const String &user){
    (void)armed;
    (void)src;
    (void)user;
}

bool SecurityController::isAllowedPhone_(const String &number) const{
    if (!_users)
        return false;
    if (UsersRegistry::normalizePhone(number).length() == 0)
        return false;
    const auto users_guard = _users->guard();
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled)
            continue;
        if (u.gsm_phone.length() == 0)
            continue;
        if (phonesMatch_(u.gsm_phone, number))
            return true;
    }
    return false;
}

bool SecurityController::matchPhone_(const String &number, String &user) const{
    if (!_users)
        return false;
    if (UsersRegistry::normalizePhone(number).length() == 0)
        return false;
    const auto users_guard = _users->guard();
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled)
            continue;
        if (u.gsm_phone.length() == 0)
            continue;
        if (phonesMatch_(u.gsm_phone, number))
        {
            user = u.username;
            return true;
        }
    }
    return false;
}

void SecurityController::sendSmsNotify_(const SecurityController::SensorConfig &cfg){
    if (!_gsm || !_users)
        return;
    const auto users_guard = _users->guard();
    String msg = F("ALARM sensor ");
    msg += String((unsigned)cfg.id);
    if (cfg.name.length())
    {
        msg += F(" (");
        msg += cfg.name;
        msg += F(")");
    }
    size_t sms_targets = 0;
    size_t call_targets = 0;
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled || !u.gsm_sms)
            continue;
        if (u.gsm_phone.length() == 0)
            continue;
        const String phone = formatOutgoingPhone_(u.gsm_phone);
        if (phone.length() == 0)
            continue;
        ++sms_targets;
        if (!_gsm->sendSms(phone, msg))
            _logs.warn(F("SECURITY"), F("GSM sms enqueue failed: user: %s phone: %s"),
                       u.username.c_str(), phone.c_str());
    }
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled || !u.gsm_call)
            continue;
        if (u.gsm_phone.length() == 0)
            continue;
        const String phone = formatOutgoingPhone_(u.gsm_phone);
        if (phone.length() == 0)
            continue;
        ++call_targets;
        if (!_gsm->dial(phone))
            _logs.warn(F("SECURITY"), F("GSM call enqueue failed: user: %s phone: %s"),
                       u.username.c_str(), phone.c_str());
        else
            _logs.info(F("SECURITY"), F("GSM call queued: user: %s phone: %s"),
                       u.username.c_str(), phone.c_str());
    }
    if (sms_targets == 0)
        _logs.warn(F("SECURITY"), F("No GSM SMS targets for alarm"));
    if (call_targets == 0)
        _logs.warn(F("SECURITY"), F("No GSM call targets for alarm"));
}

void SecurityController::sendSmsNotify_(uint8_t sensor_id, const String &name){
    if (!_gsm || !_users)
        return;
    const auto users_guard = _users->guard();
    String msg = F("ALARM sensor ");
    msg += String((unsigned)sensor_id);
    if (name.length())
    {
        msg += F(" (");
        msg += name;
        msg += F(")");
    }
    size_t sms_targets = 0;
    size_t call_targets = 0;
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled || !u.gsm_sms)
            continue;
        if (u.gsm_phone.length() == 0)
            continue;
        const String phone = formatOutgoingPhone_(u.gsm_phone);
        if (phone.length() == 0)
            continue;
        ++sms_targets;
        if (!_gsm->sendSms(phone, msg))
            _logs.warn(F("SECURITY"), F("GSM sms enqueue failed: user: %s phone: %s"),
                       u.username.c_str(), phone.c_str());
    }
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled || !u.gsm_call)
            continue;
        if (u.gsm_phone.length() == 0)
            continue;
        const String phone = formatOutgoingPhone_(u.gsm_phone);
        if (phone.length() == 0)
            continue;
        ++call_targets;
        if (!_gsm->dial(phone))
            _logs.warn(F("SECURITY"), F("GSM call enqueue failed: user: %s phone: %s"),
                       u.username.c_str(), phone.c_str());
        else
            _logs.info(F("SECURITY"), F("GSM call queued: user: %s phone: %s"),
                       u.username.c_str(), phone.c_str());
    }
    if (sms_targets == 0)
        _logs.warn(F("SECURITY"), F("No GSM SMS targets for alarm"));
    if (call_targets == 0)
        _logs.warn(F("SECURITY"), F("No GSM call targets for alarm"));
}

bool SecurityController::matchKey_(const uint8_t addr[8], String &user) const{
    if (!_users)
        return false;
    const auto users_guard = _users->guard();
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

bool SecurityController::matchRfidKey_(const PN532::UID &uid, String &user) const{
    if (uid.len == 0 || uid.len > 10 || !_users)
        return false;
    const auto users_guard = _users->guard();
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

void SecurityController::logArmAction_(bool armed, const char *src, const String &user){
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

void SecurityController::notifyArmState_(bool armed){
    if (_arm_state_cb)
        _arm_state_cb(_arm_state_ctx, armed);
    if (_arm_state_cb_secondary)
        _arm_state_cb_secondary(_arm_state_ctx_secondary, armed);
}

void SecurityController::notifyAlarmState_(bool alarm_on){
    if (_alarm_state_cb)
        _alarm_state_cb(_alarm_state_ctx, alarm_on);
    if (_alarm_state_cb_secondary)
        _alarm_state_cb_secondary(_alarm_state_ctx_secondary, alarm_on);
}

void SecurityController::notifyClearDetect_(){
    if (_clear_detect_cb)
        _clear_detect_cb(_clear_detect_ctx);
    if (_clear_detect_cb_secondary)
        _clear_detect_cb_secondary(_clear_detect_ctx_secondary);
}

void SecurityController::notifyDetectEvent_(const SecurityController::SensorConfig &cfg){
    if (_detect_cb)
        _detect_cb(_detect_ctx, cfg.id, cfg.name, cfg.silent);
    if (_detect_cb_secondary)
        _detect_cb_secondary(_detect_ctx_secondary, cfg.id, cfg.name, cfg.silent);
}

int SecurityController::hexNibble_(char c){
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    return -1;
}

bool SecurityController::parseHexAddr_(const char *s, uint8_t out[8]){
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

bool SecurityController::parseRfidUid_(const char *s, PN532::UID &out){
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

String SecurityController::rfidUidToString_(const uint8_t *bytes, uint8_t len){
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
