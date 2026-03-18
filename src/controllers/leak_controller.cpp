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

#include "controllers/leak_controller.hpp"

#include <string.h>

LeakController::LeakController(Gpio &gpio, Logger &logs)
 : _gpio(gpio), _logs(logs){
    reset_();
}

void LeakController::setEventHandler(LeakController::EventHandler cb, void *ctx){
    auto guard = _lock.guard();
    _event_cb = cb;
    _event_ctx = ctx;
}

bool LeakController::begin(){
    auto guard = _lock.guard();
    if (!_controller_enabled)
        return true;
    for (size_t i = 0; i < kZoneCount; ++i)
        setupZone_(_cfg[i], _state[i]);
    _logs.info(F("LEAK"), F("Init done"));
    return true;
}

void LeakController::task(){
    ZoneConfig pending_cfg[kZoneCount]{};
    size_t pending_count = 0;
    {
        auto guard = _lock.guard();
        if (!_controller_enabled)
            return;
        for (size_t i = 0; i < kZoneCount; ++i)
        {
            ZoneConfig &cfg = _cfg[i];
            ZoneState &st = _state[i];
            if (!cfg.enabled)
                continue;
            if (!cfg.power_on)
            {
                writeOutputs_(cfg, st, false);
                st.last_wet = false;
                continue;
            }
            bool wet = false;
            if (!readSensor_(cfg, wet))
            {
                writeOutputs_(cfg, st, true);
                continue;
            }
            st.wet = wet;
            if (wet && !st.last_wet)
            {
                const uint32_t now = millis();
                if (st.last_detect_event_ms == 0 ||
                    (uint32_t)(now - st.last_detect_event_ms) >= kDetectEventDebounceMs)
                {
                    if (pending_count < kZoneCount)
                        pending_cfg[pending_count++] = cfg;
                    st.last_detect_event_ms = now;
                }
            }
            if (wet)
                st.alarm_latched = true;
            writeOutputs_(cfg, st, st.alarm_latched);
            st.last_wet = wet;
        }
    }
    for (size_t i = 0; i < pending_count; ++i)
    {
        notifyLeak_(pending_cfg[i]);
    }
}

void LeakController::applyConfig(JsonArrayConst zones){
    auto guard = _lock.guard();
    reset_();
    size_t idx = 0;
    for (JsonVariantConst v : zones)
    {
        if (idx >= kZoneCount)
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
            if (raw > 0 && raw <= 0xFFu)
                id = (uint8_t)raw;
        }
        size_t dst = 0;
        if (!indexById_(id, dst))
        {
            ++idx;
            continue;
        }
        ZoneConfig &cfg = _cfg[dst];
        cfg.id = id;
        bool enabled_set = false;
        if (obj["enabled"].is<bool>())
        {
            cfg.enabled = obj["enabled"].as<bool>();
            enabled_set = true;
        }
        if (obj["power_on"].is<bool>())
            cfg.power_on = obj["power_on"].as<bool>();
        if (obj["sensor_active_low"].is<bool>())
            cfg.sensor_active_low = obj["sensor_active_low"].as<bool>();
        cfg.valve_open_on_power = true;
        if (obj["name"].is<const char *>())
            cfg.name = obj["name"].as<const char *>();
        parsePort_(obj["sensor"], cfg.sensor_port);
        parsePort_(obj["valve"], cfg.valve_port);
        parsePort_(obj["alarm"], cfg.alarm_port);
        if (!enabled_set)
            cfg.enabled = true;
        ++idx;
    }
}

void LeakController::serialize(JsonArray out) const{
    auto guard = _lock.guard();
    for (size_t i = 0; i < kZoneCount; ++i)
    {
        const ZoneConfig &cfg = _cfg[i];
        if (!cfg.enabled)
            continue;
        JsonObject obj = out.add<JsonObject>();
        obj["id"] = cfg.id;
        obj["enabled"] = cfg.enabled;
        obj["power_on"] = cfg.power_on;
        obj["sensor_active_low"] = cfg.sensor_active_low;
        obj["valve_open_on_power"] = cfg.valve_open_on_power;
        if (cfg.name.length())
            obj["name"] = cfg.name;
        if (cfg.sensor_port != kInvalidPort)
            obj["sensor"] = cfg.sensor_port;
        if (cfg.valve_port != kInvalidPort)
            obj["valve"] = cfg.valve_port;
        if (cfg.alarm_port != kInvalidPort)
            obj["alarm"] = cfg.alarm_port;
    }
}

bool LeakController::controllerEnabled() const{
    auto guard = _lock.guard();
    return _controller_enabled;
}

bool LeakController::setControllerEnabled(bool enabled){
    auto guard = _lock.guard();
    if (_controller_enabled == enabled)
        return false;
    _controller_enabled = enabled;
    if (!_controller_enabled)
    {
        for (size_t i = 0; i < kZoneCount; ++i)
        {
            ZoneState &st = _state[i];
            writeOutputs_(_cfg[i], st, false);
            st.wet = false;
            st.alarm_latched = false;
        }
    }
    else
    {
        for (size_t i = 0; i < kZoneCount; ++i)
            setupZone_(_cfg[i], _state[i]);
    }
    return true;
}

bool LeakController::takeDirty(){
    auto guard = _lock.guard();
    if (!_dirty)
        return false;
    _dirty = false;
    return true;
}

bool LeakController::setEnabled(size_t id, bool enabled){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    ZoneConfig &cfg = _cfg[idx];
    ZoneState &st = _state[idx];
    if (cfg.enabled == enabled)
        return false;
    cfg.enabled = enabled;
    st = ZoneState{};
    if (_controller_enabled && cfg.enabled)
        setupZone_(cfg, st);
    else
        writeOutputs_(cfg, st, false);
    _dirty = true;
    return true;
}

bool LeakController::setPower(size_t id, bool on){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    ZoneConfig &cfg = _cfg[idx];
    ZoneState &st = _state[idx];
    if (cfg.power_on == on)
        return false;
    cfg.power_on = on;
    if (!on)
    {
        st.wet = false;
        st.last_wet = false;
        st.alarm_latched = false;
        writeOutputs_(cfg, st, false);
    }
    _dirty = true;
    return true;
}

bool LeakController::setSensorActiveLow(size_t id, bool active_low){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    ZoneConfig &cfg = _cfg[idx];
    if (cfg.sensor_active_low == active_low)
        return false;
    cfg.sensor_active_low = active_low;
    _dirty = true;
    return true;
}

bool LeakController::setValveOpenOnPower(size_t id, bool open_on_power){
    auto guard = _lock.guard();
    (void)open_on_power;
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    ZoneConfig &cfg = _cfg[idx];
    if (cfg.valve_open_on_power)
        return false;
    cfg.valve_open_on_power = true;
    if (_controller_enabled && cfg.enabled)
        writeOutputs_(cfg, _state[idx], _state[idx].alarm_latched);
    _dirty = true;
    return true;
}

bool LeakController::setName(size_t id, const String &name){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    if (_cfg[idx].name == name)
        return false;
    _cfg[idx].name = name;
    _dirty = true;
    return true;
}

bool LeakController::setSensorPort(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setPort_(id, port, &ZoneConfig::sensor_port, true);
}

bool LeakController::setValvePort(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setPort_(id, port, &ZoneConfig::valve_port, false);
}

bool LeakController::setAlarmPort(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setPort_(id, port, &ZoneConfig::alarm_port, false);
}

bool LeakController::ack(size_t id){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    ZoneState &st = _state[idx];
    if (st.wet || !st.alarm_latched)
        return false;
    st.alarm_latched = false;
    writeOutputs_(_cfg[idx], st, false);
    notifyEvent_(Event::Ack, _cfg[idx].id, _cfg[idx].name, st.wet, st.alarm_latched);
    return true;
}

bool LeakController::ackAll(){
    auto guard = _lock.guard();
    bool changed = false;
    for (size_t i = 0; i < kZoneCount; ++i)
        changed = ack(i + 1) || changed;
    return changed;
}

const LeakController::ZoneConfig *LeakController::config(size_t id) const{
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_cfg[idx];
}

const LeakController::ZoneState *LeakController::state(size_t id) const{
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_state[idx];
}

const LeakController::ZoneConfig *LeakController::configByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kZoneCount)
        return nullptr;
    return &_cfg[idx];
}

const LeakController::ZoneState *LeakController::stateByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kZoneCount)
        return nullptr;
    return &_state[idx];
}

void LeakController::reset_(){
    for (size_t i = 0; i < kZoneCount; ++i)
    {
        _cfg[i] = ZoneConfig{};
        _cfg[i].id = (uint8_t)(i + 1);
        _state[i] = ZoneState{};
    }
}

bool LeakController::parsePort_(JsonVariantConst v, uint8_t &out){
    if (v.is<unsigned>())
    {
        const unsigned raw = v.as<unsigned>();
        if (raw <= 0xFFu)
        {
            out = (uint8_t)raw;
            return true;
        }
    }
    return false;
}

bool LeakController::indexById_(size_t id, size_t &out){
    if (id == 0 || id > kZoneCount)
        return false;
    out = id - 1;
    return true;
}

bool LeakController::setPort_(size_t id, uint8_t port, uint8_t ZoneConfig::*field, bool input){
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    ZoneConfig &cfg = _cfg[idx];
    if (cfg.*field == port)
        return false;
    cfg.*field = port;
    if (_controller_enabled && cfg.enabled)
    {
        if (input)
            setupInput_(port);
        else
            setupOutput_(port);
    }
    _dirty = true;
    return true;
}

void LeakController::setupZone_(const LeakController::ZoneConfig &cfg, LeakController::ZoneState &st){
    st = ZoneState{};
    if (!cfg.enabled)
        return;
    setupInput_(cfg.sensor_port);
    setupOutput_(cfg.valve_port);
    setupOutput_(cfg.alarm_port);
    writeOutputs_(cfg, st, false);
}

void LeakController::setupInput_(uint8_t port){
    if (port == kInvalidPort)
        return;
    PortIO::PortMode mode = PortIO::PortMode::Input;
    const Cap caps = _gpio.capsDyn(port);
    if (has(caps, Cap::PullUp))
        mode = PortIO::PortMode::InputPullUp;
    if (!_gpio.pinModeDyn(port, mode))
        _gpio.pinModeDyn(port, PortIO::PortMode::Input);
}

void LeakController::setupOutput_(uint8_t port){
    if (port == kInvalidPort)
        return;
    _gpio.pinModeDyn(port, PortIO::PortMode::Output);
}

bool LeakController::readSensor_(const LeakController::ZoneConfig &cfg, bool &wet){
    if (cfg.sensor_port == kInvalidPort)
        return false;
    bool raw = false;
    if (!_gpio.readDyn(cfg.sensor_port, raw))
        return false;
    wet = cfg.sensor_active_low ? !raw : raw;
    return true;
}

void LeakController::writeOutputs_(const LeakController::ZoneConfig &cfg, LeakController::ZoneState &st, bool alarm){
    st.alarm_on = alarm;
    st.valve_closed = alarm;
    const bool valve_drive = !alarm;
    if (cfg.valve_port != kInvalidPort)
        _gpio.writeDyn(cfg.valve_port, valve_drive);
    if (cfg.alarm_port != kInvalidPort)
        _gpio.writeDyn(cfg.alarm_port, st.alarm_on);
}

void LeakController::notifyLeak_(const LeakController::ZoneConfig &cfg){
    _logs.warn(F("LEAK"), F("detected: zone: %u name: %s"),
               (unsigned)cfg.id, cfg.name.length() ? cfg.name.c_str() : "-");
    notifyEvent_(Event::Detect, cfg.id, cfg.name, true, true);
}

void LeakController::notifyEvent_(LeakController::Event ev, uint8_t id, const String &name, bool wet, bool alarm_latched){
    if (_event_cb)
        _event_cb(_event_ctx, ev, id, name, wet, alarm_latched);
}
