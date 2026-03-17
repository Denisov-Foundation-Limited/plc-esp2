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

#include "controllers/septic_controller.hpp"

#include <string.h>

SepticController::SepticController(Gpio &gpio, Logger &logs, TelegramBot &bot, TelegramAllowedUsersProvider &users)
 : _gpio(gpio), _logs(logs), _tgbot(bot), _tgusers(users){
    reset_();
}

bool SepticController::begin(){
    auto guard = _lock.guard();
    if (!_controller_enabled)
        return true;
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

void SepticController::task(){
    SepticConfig pending_cfg[kSepticCount]{};
    bool pending_alarm[kSepticCount]{};
    size_t pending_count = 0;
    {
        auto guard = _lock.guard();
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
            if (!prev.warning && st.warning && pending_count < kSepticCount)
            {
                pending_cfg[pending_count] = cfg;
                pending_alarm[pending_count] = false;
                ++pending_count;
            }
            if (!prev.alarm && st.alarm && pending_count < kSepticCount)
            {
                pending_cfg[pending_count] = cfg;
                pending_alarm[pending_count] = true;
                ++pending_count;
            }
            st.last_warning = st.warning;
            st.last_alarm = st.alarm;
        }
    }
    for (size_t i = 0; i < pending_count; ++i)
    {
        notifyDetectEvent_(pending_cfg[i], pending_alarm[i]);
        notifyLevel_(pending_cfg[i], pending_alarm[i]);
    }
}

void SepticController::applyConfig(JsonArrayConst septic){
    auto guard = _lock.guard();
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
        if (obj["group_id"].is<unsigned>())
        {
            const unsigned raw = obj["group_id"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.group_id = (uint8_t)raw;
        }
        parsePort_(obj["warning"], cfg.warning_port);
        parsePort_(obj["alarm"], cfg.alarm_port);
        parsePort_(obj["relay_warning"], cfg.relay_warning);
        parsePort_(obj["relay_alarm"], cfg.relay_alarm);
        if (!enabled_set)
            cfg.enabled = true;
        ++idx;
    }
}

void SepticController::serialize(JsonArray out) const{
    auto guard = _lock.guard();
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
        if (cfg.group_id != 0)
            obj["group_id"] = cfg.group_id;
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

bool SepticController::controllerEnabled() const{
    auto guard = _lock.guard();
    return _controller_enabled;
}

void SepticController::setControllerEnabled(bool enabled){
    auto guard = _lock.guard();
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
        reset_();
        return;
    }
    _logs.info(F("SEPTIC"), F("controller: enabled"));
    begin();
}

bool SepticController::setEnabled(size_t id, bool enabled){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    SepticConfig &cfg = _cfg[idx];
    SepticState &st = _state[idx];
    if (!enabled)
    {
        writeRelay_(cfg.relay_warning, false);
        writeRelay_(cfg.relay_alarm, false);
        const uint8_t saved_id = cfg.id;
        String saved_name = cfg.name;
        cfg = SepticConfig{};
        cfg.id = saved_id;
        cfg.enabled = false;
        st = SepticState{};
        const char *name = saved_name.length() ? saved_name.c_str() : "-";
        _logs.info(F("SEPTIC"), F("id: %u name: %s enabled: false"), (unsigned)cfg.id, name);
        return true;
    }
    cfg.enabled = true;
    st = SepticState{};
    if (_controller_enabled && cfg.enabled)
    {
        setupInputs_(cfg);
        setupOutputs_(cfg, st);
        readLevels_(cfg, st);
        updateRelays_(cfg, st);
    }
    _logs.info(F("SEPTIC"), F("id: %u enabled: true"), (unsigned)cfg.id);
    return true;
}

bool SepticController::setMonitoring(size_t id, bool on){
    auto guard = _lock.guard();
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

bool SepticController::setName(size_t id, const String &name){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    _cfg[idx].name = name;
    return true;
}

bool SepticController::setGroupId(size_t id, uint8_t group_id){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_((uint8_t)id, idx))
        return false;
    _cfg[idx].group_id = group_id;
    return true;
}

bool SepticController::setWarningPort(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setLevelPort_(id, port, &SepticConfig::warning_port);
}

bool SepticController::setAlarmPort(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setLevelPort_(id, port, &SepticConfig::alarm_port);
}

bool SepticController::setWarningRelay(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setRelayPort_(id, port, &SepticConfig::relay_warning);
}

bool SepticController::setAlarmRelay(size_t id, uint8_t port){
    auto guard = _lock.guard();
    return setRelayPort_(id, port, &SepticConfig::relay_alarm);
}

void SepticController::setDetectHandler(SepticController::DetectHandler cb, void *ctx){
    auto guard = _lock.guard();
    _detect_cb = cb;
    _detect_ctx = ctx;
}

void SepticController::setNotifyEnabled(bool enabled){
    auto guard = _lock.guard();
    _notify_enabled = enabled;
}

void SepticController::notifyRemoteLevel(const String &source, uint8_t septic_id, const String &name, bool is_alarm){
    bool notify_enabled = false;
    {
        auto guard = _lock.guard();
        notify_enabled = _notify_enabled;
    }
    if (!notify_enabled)
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

const SepticController::SepticConfig *SepticController::configByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kSepticCount)
        return nullptr;
    return &_cfg[idx];
}

const SepticController::SepticState *SepticController::stateByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kSepticCount)
        return nullptr;
    return &_state[idx];
}

void SepticController::reset_(){
    for (size_t i = 0; i < kSepticCount; ++i)
    {
        _cfg[i] = SepticConfig{};
        _cfg[i].id = (uint8_t)(i + 1);
        _state[i] = SepticState{};
    }
}

bool SepticController::indexById_(uint8_t id, size_t &out){
    if (id == 0 || id > kSepticCount)
        return false;
    out = (size_t)(id - 1);
    return true;
}

void SepticController::setupInputs_(const SepticController::SepticConfig &cfg){
    setupInput_(cfg.warning_port);
    setupInput_(cfg.alarm_port);
}

void SepticController::setupInput_(uint8_t port){
    if (port == kInvalidPort)
        return;
    const PortIO::PortMode mode = kLevelPullup ? PortIO::PortMode::InputPullUp : PortIO::PortMode::Input;
    _gpio.pinModeDyn(port, mode);
}

void SepticController::setupOutputs_(const SepticController::SepticConfig &cfg, SepticController::SepticState &st){
    setupRelay_(cfg.relay_warning, st.relay_warning);
    setupRelay_(cfg.relay_alarm, st.relay_alarm);
}

void SepticController::setupRelay_(uint8_t port, bool &state){
    if (port == kInvalidPort)
        return;
    if (!_gpio.pinModeDyn(port, PortIO::PortMode::Output))
        return;
    state = false;
    writeRelay_(port, state);
}

void SepticController::readLevels_(const SepticController::SepticConfig &cfg, SepticController::SepticState &st){
    st.warning = readInput_(cfg.warning_port);
    st.alarm = readInput_(cfg.alarm_port);
}

bool SepticController::setLevelPort_(size_t id, uint8_t port, uint8_t SepticConfig::*field){
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

bool SepticController::setRelayPort_(size_t id, uint8_t port, uint8_t SepticConfig::*field){
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

bool SepticController::readInput_(uint8_t port){
    if (port == kInvalidPort)
        return false;
    bool raw = false;
    if (!_gpio.readDyn(port, raw))
        return false;
    return raw;
}

void SepticController::updateRelays_(const SepticController::SepticConfig &cfg, SepticController::SepticState &st){
    st.relay_warning = st.warning;
    st.relay_alarm = st.alarm;
    writeRelay_(cfg.relay_warning, st.relay_warning);
    writeRelay_(cfg.relay_alarm, st.relay_alarm);
}

void SepticController::writeRelay_(uint8_t port, bool on){
    if (port == kInvalidPort)
        return;
    const bool out = kRelayInvert ? !on : on;
    _gpio.writeDyn(port, out);
}

void SepticController::logLevelChange_(const SepticController::SepticConfig &cfg, const SepticController::SepticState &prev, const SepticController::SepticState &curr){
    if (prev.warning == curr.warning && prev.alarm == curr.alarm)
        return;
    _logs.info(F("SEPTIC"), F("id: %u warning: %u alarm: %u"),
               (unsigned)cfg.id,
               curr.warning ? 1u : 0u,
               curr.alarm ? 1u : 0u);
}

void SepticController::logRelayChange_(const SepticController::SepticConfig &cfg, const SepticController::SepticState &prev, const SepticController::SepticState &curr){
    if (prev.relay_warning != curr.relay_warning)
        _logs.info(F("SEPTIC"), F("id: %u warning_lamp: %s"),
                   (unsigned)cfg.id, curr.relay_warning ? "on" : "off");
    if (prev.relay_alarm != curr.relay_alarm)
        _logs.info(F("SEPTIC"), F("id: %u alarm_lamp: %s"),
                   (unsigned)cfg.id, curr.relay_alarm ? "on" : "off");
}

void SepticController::notifyLevel_(const SepticController::SepticConfig &cfg, bool is_alarm){
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

void SepticController::sendTgNotify_(const String &msg){
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

void SepticController::notifyDetectEvent_(const SepticController::SepticConfig &cfg, bool is_alarm){
    if (_detect_cb)
        _detect_cb(_detect_ctx, cfg.id, cfg.name, is_alarm);
}

bool SepticController::parsePort_(JsonVariantConst v, uint8_t &out){
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
