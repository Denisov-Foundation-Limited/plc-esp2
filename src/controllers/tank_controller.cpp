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

#include "controllers/tank_controller.hpp"

#include <string.h>

TankController::TankController(Gpio &gpio, Logger &logs)
 : _gpio(gpio), _logs(logs){
    reset_();
}

bool TankController::begin(){
    auto guard = _lock.guard();
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

void TankController::task(){
    TankConfig pending_cfg[kTankCount]{};
    bool pending_empty[kTankCount]{};
    size_t pending_count = 0;
    {
        auto guard = _lock.guard();
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
                    if (pending_count < kTankCount)
                    {
                        pending_cfg[pending_count] = cfg;
                        pending_empty[pending_count] = true;
                        ++pending_count;
                    }
                    st.last_empty_event_ms = now;
                }
            }
            st.last_empty = empty;
        }
    }
    for (size_t i = 0; i < pending_count; ++i)
    {
        notifyDetectEvent_(pending_cfg[i], pending_empty[i]);
        notifyEmpty_(pending_cfg[i]);
    }
}

void TankController::applyConfig(JsonArrayConst tanks){
    auto guard = _lock.guard();
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
        if (obj["group_id"].is<unsigned>())
        {
            const unsigned raw = obj["group_id"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.group_id = (uint8_t)raw;
        }
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

void TankController::serialize(JsonArray out) const{
    auto guard = _lock.guard();
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
        if (cfg.group_id != 0)
            obj["group_id"] = cfg.group_id;
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

void TankController::buildSnapshot(uint8_t *power_mask, size_t bytes) const{
    auto guard = _lock.guard();
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

void TankController::applySnapshot(const uint8_t *power_mask, size_t bytes){
    auto guard = _lock.guard();
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

bool TankController::takeDirty(){
    auto guard = _lock.guard();
    if (!_dirty)
        return false;
    _dirty = false;
    return true;
}

bool TankController::controllerEnabled() const{
    auto guard = _lock.guard();
    return _controller_enabled;
}

void TankController::setControllerEnabled(bool enabled){
    auto guard = _lock.guard();
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

bool TankController::setEnabled(size_t id, bool enabled){
    auto guard = _lock.guard();
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

bool TankController::setPower(size_t id, bool on){
    auto guard = _lock.guard();
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

bool TankController::setName(size_t id, const String &name){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].name = name;
    return true;
}

bool TankController::setGroupId(size_t id, uint8_t group_id){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    _cfg[idx].group_id = group_id;
    return true;
}

bool TankController::setLevelLow(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setLevelPort_(id, port, &TankConfig::level_low);
}

bool TankController::setLevelMid(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setLevelPort_(id, port, &TankConfig::level_mid);
}

bool TankController::setLevelFull(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setLevelPort_(id, port, &TankConfig::level_full);
}

bool TankController::setValveRelay(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setRelayPort_(id, port, &TankConfig::relay_valve, 0);
}

bool TankController::setPumpRelay(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setRelayPort_(id, port, &TankConfig::relay_pump, 1);
}

bool TankController::setAlarmRelay(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setRelayPort_(id, port, &TankConfig::relay_alarm, 2);
}

void TankController::setDetectHandler(TankController::DetectHandler cb, void *ctx){
    auto guard = _lock.guard();
    _detect_cb = cb;
    _detect_ctx = ctx;
}

void TankController::setDetectHandlerSecondary(TankController::DetectHandler cb, void *ctx){
    auto guard = _lock.guard();
    _detect_cb_secondary = cb;
    _detect_ctx_secondary = ctx;
}

void TankController::setNotifyEnabled(bool enabled){
    auto guard = _lock.guard();
    _notify_enabled = enabled;
}

void TankController::notifyRemoteEmpty(const String &source, uint8_t tank_id, const String &name){
    bool notify_enabled = false;
    {
        auto guard = _lock.guard();
        notify_enabled = _notify_enabled;
    }
    if (!notify_enabled)
        return;
    (void)source;
    TankConfig cfg;
    cfg.id = tank_id;
    cfg.name = name;
    notifyDetectEvent_(cfg, true);
}

const TankController::TankConfig *TankController::config(size_t id) const{
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_cfg[idx];
}

const TankController::TankState *TankController::state(size_t id) const{
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_state[idx];
}

const TankController::TankConfig *TankController::configByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kTankCount)
        return nullptr;
    return &_cfg[idx];
}

const TankController::TankState *TankController::stateByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kTankCount)
        return nullptr;
    return &_state[idx];
}

void TankController::reset_(){
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

bool TankController::parsePort_(JsonVariantConst v, uint8_t &out){
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

bool TankController::indexById_(uint8_t id, size_t &out){
    if (id == 0 || id > kTankCount)
        return false;
    out = (size_t)(id - 1);
    return true;
}

void TankController::setupInputs_(const TankController::TankConfig &cfg){
    setupInput_(cfg.level_low);
    setupInput_(cfg.level_mid);
    setupInput_(cfg.level_full);
}

void TankController::setupInput_(uint8_t port){
    if (port == kInvalidPort)
        return;
    const PortIO::PortMode mode = kLevelPullup ? PortIO::PortMode::InputPullUp : PortIO::PortMode::Input;
    if (!_gpio.pinModeDyn(port, mode))
        _logs.warn(F("TANK"), F("input setup failed: port: %u mode: %s"),
                   (unsigned)port, kLevelPullup ? "input_pullup" : "input");
}

bool TankController::setLevelPort_(size_t id, uint8_t port, uint8_t TankConfig::*field){
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

bool TankController::setRelayPort_(size_t id, uint8_t port, uint8_t TankConfig::*field, uint8_t){
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

void TankController::setupOutputs_(const TankController::TankConfig &cfg, TankController::TankState &st){
    setupRelay_(cfg.relay_valve, st.valve_on);
    setupRelay_(cfg.relay_pump, st.pump_on);
    setupRelay_(cfg.relay_alarm, st.alarm_on);
}

void TankController::setupRelay_(uint8_t port, bool &state){
    if (port == kInvalidPort)
        return;
    if (!_gpio.pinModeDyn(port, PortIO::PortMode::Output))
        return;
    state = false;
    writeRelay_(port, state);
}

void TankController::readLevels_(const TankController::TankConfig &cfg, TankController::TankState &st){
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
    if (!st.levels_ok)
    {
        if (st.levels_ok_prev)
        {
            _logs.warn(F("TANK"),
                       F("id: %u level read failed (low:%u port:%u mid:%u port:%u full:%u port:%u)"),
                       (unsigned)cfg.id,
                       ok_low ? 1u : 0u,
                       (unsigned)cfg.level_low,
                       ok_mid ? 1u : 0u,
                       (unsigned)cfg.level_mid,
                       ok_full ? 1u : 0u,
                       (unsigned)cfg.level_full);
        }
    }
    else if (!st.levels_ok_prev)
    {
        _logs.info(F("TANK"), F("id: %u level read ok"), (unsigned)cfg.id);
    }
    st.levels_ok_prev = st.levels_ok;
}

bool TankController::readInput_(uint8_t port, bool &out){
    if (port == kInvalidPort)
        return false;
    bool raw = false;
    if (!_gpio.readDyn(port, raw))
        return false;
    out = raw;
    return true;
}

void TankController::updateControl_(const TankController::TankConfig &cfg, TankController::TankState &st){
    const bool empty = isEmpty_(st);
    const bool full = st.level_full;
    st.alarm_on = empty;
    if (st.alarm_on)
    {
        // Safety interlock: in alarm/empty state stop all actuators.
        st.pump_on = false;
        st.valve_on = false;
    }
    else
    {
        st.pump_on = true;
        st.valve_on = !full;
    }
    writeRelay_(cfg.relay_pump, st.pump_on);
    writeRelay_(cfg.relay_valve, st.valve_on);
    writeRelay_(cfg.relay_alarm, st.alarm_on);
}

void TankController::logLevelChange_(const TankController::TankConfig &cfg, const TankController::TankState &prev, const TankController::TankState &curr){
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

void TankController::logRelayChange_(const TankController::TankConfig &cfg, const TankController::TankState &prev, const TankController::TankState &curr){
    if (prev.pump_on != curr.pump_on)
        _logs.info(F("TANK"), F("id: %u pump: %s"),
                   (unsigned)cfg.id, curr.pump_on ? "on" : "off");
    if (prev.valve_on != curr.valve_on)
        _logs.info(F("TANK"), F("id: %u valve: %s"),
                   (unsigned)cfg.id, curr.valve_on ? "on" : "off");
}

bool TankController::isEmpty_(const TankController::TankState &st){
    return !(st.level_low || st.level_mid || st.level_full);
}

void TankController::writeAllOff_(const TankController::TankConfig &cfg, TankController::TankState &st){
    st.valve_on = false;
    st.pump_on = false;
    st.alarm_on = false;
    writeRelay_(cfg.relay_pump, st.pump_on);
    writeRelay_(cfg.relay_valve, st.valve_on);
    writeRelay_(cfg.relay_alarm, st.alarm_on);
}

void TankController::writeRelay_(uint8_t port, bool on){
    if (port == kInvalidPort)
        return;
    _gpio.writeDyn(port, on);
}

void TankController::notifyEmpty_(const TankController::TankConfig &cfg){
    (void)cfg;
}

void TankController::notifyDetectEvent_(const TankController::TankConfig &cfg, bool empty){
    if (_detect_cb)
        _detect_cb(_detect_ctx, cfg.id, cfg.name, empty);
    if (_detect_cb_secondary)
        _detect_cb_secondary(_detect_ctx_secondary, cfg.id, cfg.name, empty);
}
