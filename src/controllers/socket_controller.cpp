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

#include "controllers/socket_controller.hpp"

#include <string.h>
#include "hal/gpio/gpio_caps.hpp"

SocketController::SocketController(Gpio &gpio, Logger &logs) : _gpio(gpio), _logs(logs){
    resetSockets_();
    resetLights_();
}

void SocketController::applyConfig(JsonArrayConst sockets, bool legacy_lights ){
    resetSockets_();
    if (legacy_lights)
        resetLights_();
    size_t idx = 0;
    for (JsonVariantConst v : sockets)
    {
        if (idx >= kSocketCount)
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
        bool use_lights = false;
        if (legacy_lights && obj["type"].is<const char *>())
        {
            const char *type = obj["type"].as<const char *>();
            if (type && strcmp(type, "light") == 0)
                use_lights = true;
        }
        size_t dst = 0;
        if (use_lights)
        {
            if (!lightIndexById_(id, dst))
            {
                ++idx;
                continue;
            }
        }
        else
        {
            if (!indexById_(id, dst))
            {
                ++idx;
                continue;
            }
        }
        SocketConfig &cfg = use_lights ? _light_cfg[dst] : _cfg[dst];
        cfg.id = id;
        bool enabled_set = false;
        if (obj["enabled"].is<bool>())
        {
            cfg.enabled = obj["enabled"].as<bool>();
            enabled_set = true;
        }
        if (obj["name"].is<const char *>())
            cfg.name = obj["name"].as<const char *>();
        parsePort_(obj["button"], cfg.button_port);
        parsePort_(obj["relay"], cfg.relay_port);
        if (!enabled_set)
            cfg.enabled = true;
        ++idx;
    }
}

void SocketController::applyLightsConfig(JsonArrayConst lights){
    resetLights_();
    size_t idx = 0;
    for (JsonVariantConst v : lights)
    {
        if (idx >= kLightCount)
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
        if (!lightIndexById_(id, dst))
        {
            ++idx;
            continue;
        }
        LightConfig &cfg = _light_cfg[dst];
        cfg.id = id;
        bool enabled_set = false;
        if (obj["enabled"].is<bool>())
        {
            cfg.enabled = obj["enabled"].as<bool>();
            enabled_set = true;
        }
        if (obj["name"].is<const char *>())
            cfg.name = obj["name"].as<const char *>();
        parsePort_(obj["button"], cfg.button_port);
        parsePort_(obj["relay"], cfg.relay_port);
        if (!enabled_set)
            cfg.enabled = true;
        ++idx;
    }
}

bool SocketController::begin(){
    if (!_controller_enabled && !_lights_enabled)
        return true;
    if (_controller_enabled)
    {
        for (size_t i = 0; i < kSocketCount; ++i)
        {
            SocketConfig &cfg = _cfg[i];
            SocketState &st = _state[i];
            if (!cfg.enabled)
                continue;

            st.has_button = setupButton_(cfg, st);
            setupRelay_(cfg, st);
        }
    }
    if (_lights_enabled)
    {
        for (size_t i = 0; i < kLightCount; ++i)
        {
            LightConfig &cfg = _light_cfg[i];
            LightState &st = _light_state[i];
            if (!cfg.enabled)
                continue;

            st.has_button = setupButton_(cfg, st);
            setupRelay_(cfg, st);
        }
    }
    _logs.info(F("SOCKET"), F("Init done"));
    return true;
}

void SocketController::task(){
    if (_controller_enabled)
    {
        for (size_t i = 0; i < kSocketCount; ++i)
        {
            SocketConfig &cfg = _cfg[i];
            SocketState &st = _state[i];
            if (!cfg.enabled || !st.has_button || cfg.relay_port == kInvalidPort)
                continue;

            const uint32_t now = millis();
            bool raw = false;
            if (!_gpio.readDyn(cfg.button_port, raw))
                continue;
            bool pressed = kButtonInvert ? !raw : raw;
            if (st.cooldown_until_ms != 0 && (int32_t)(now - st.cooldown_until_ms) < 0)
            {
                st.last_button = pressed;
                continue;
            }
            if (pressed != st.button_idle && st.last_button == st.button_idle)
            {
                st.relay_on = !st.relay_on;
                writeRelay_(cfg, st.relay_on);
                _dirty_sockets = true;
                st.cooldown_until_ms = now + kButtonCooldownMs;
                const char *name = cfg.name.length() ? cfg.name.c_str() : "-";
                _logs.info(F("SOCKET"), F("Id: %u name: %s state: %s src: button"),
                           (unsigned)cfg.id, name, st.relay_on ? "on" : "off");
            }
            st.last_button = pressed;
        }
    }
    if (_lights_enabled)
    {
        for (size_t i = 0; i < kLightCount; ++i)
        {
            LightConfig &cfg = _light_cfg[i];
            LightState &st = _light_state[i];
            if (!cfg.enabled || !st.has_button || cfg.relay_port == kInvalidPort)
                continue;

            const uint32_t now = millis();
            bool raw = false;
            if (!_gpio.readDyn(cfg.button_port, raw))
                continue;
            bool pressed = kButtonInvert ? !raw : raw;
            if (st.cooldown_until_ms != 0 && (int32_t)(now - st.cooldown_until_ms) < 0)
            {
                st.last_button = pressed;
                continue;
            }
            if (pressed != st.button_idle && st.last_button == st.button_idle)
            {
                st.relay_on = !st.relay_on;
                writeRelay_(cfg, st.relay_on);
                _dirty_lights = true;
                st.cooldown_until_ms = now + kButtonCooldownMs;
                const char *name = cfg.name.length() ? cfg.name.c_str() : "-";
                _logs.info(F("LIGHT"), F("id: %u name: %s state: %s src: button"),
                           (unsigned)cfg.id, name, st.relay_on ? "on" : "off");
            }
            st.last_button = pressed;
        }
    }
}

bool SocketController::setRelay(size_t id, bool on){
    if (!_controller_enabled)
        return false;
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    SocketConfig &cfg = _cfg[idx];
    SocketState &st = _state[idx];
    if (!cfg.enabled || cfg.relay_port == kInvalidPort)
        return false;
    if (st.relay_on == on)
        return true;
    st.relay_on = on;
    writeRelay_(cfg, st.relay_on);
    syncButtonState_(cfg, st);
    _dirty_sockets = true;
    const char *name = cfg.name.length() ? cfg.name.c_str() : "-";
    _logs.info(F("SOCKET"), F("Id: %u name: %s state: %s"),
               (unsigned)cfg.id, name, st.relay_on ? "on" : "off");
    return true;
}

bool SocketController::toggleRelay(size_t id){
    if (!_controller_enabled)
        return false;
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    SocketConfig &cfg = _cfg[idx];
    SocketState &st = _state[idx];
    if (!cfg.enabled || cfg.relay_port == kInvalidPort)
        return false;
    st.relay_on = !st.relay_on;
    writeRelay_(cfg, st.relay_on);
    syncButtonState_(cfg, st);
    _dirty_sockets = true;
    const char *name = cfg.name.length() ? cfg.name.c_str() : "-";
    _logs.info(F("SOCKET"), F("Id: %u name: %s state: %s src: toggle"),
               (unsigned)cfg.id, name, st.relay_on ? "on" : "off");
    return true;
}

bool SocketController::relayState(size_t id, bool &out) const{
    if (!_controller_enabled)
        return false;
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    const SocketConfig &cfg = _cfg[idx];
    const SocketState &st = _state[idx];
    if (!cfg.enabled || cfg.relay_port == kInvalidPort)
        return false;
    out = st.relay_on;
    return true;
}

bool SocketController::setRelayById(uint8_t id, bool on){ return setRelay(id, on); }

bool SocketController::toggleRelayById(uint8_t id){ return toggleRelay(id); }

bool SocketController::relayStateById(uint8_t id, bool &out) const{ return relayState(id, out); }

bool SocketController::setLightRelay(size_t id, bool on){
    if (!_lights_enabled)
        return false;
    size_t idx = 0;
    if (!lightIndexById_(id, idx))
        return false;
    LightConfig &cfg = _light_cfg[idx];
    LightState &st = _light_state[idx];
    if (!cfg.enabled || cfg.relay_port == kInvalidPort)
        return false;
    if (st.relay_on == on)
        return true;
    st.relay_on = on;
    writeRelay_(cfg, st.relay_on);
    syncButtonState_(cfg, st);
    _dirty_lights = true;
    const char *name = cfg.name.length() ? cfg.name.c_str() : "-";
    _logs.info(F("LIGHT"), F("id: %u name: %s state: %s"),
               (unsigned)cfg.id, name, st.relay_on ? "on" : "off");
    return true;
}

bool SocketController::toggleLightRelay(size_t id){
    if (!_lights_enabled)
        return false;
    size_t idx = 0;
    if (!lightIndexById_(id, idx))
        return false;
    LightConfig &cfg = _light_cfg[idx];
    LightState &st = _light_state[idx];
    if (!cfg.enabled || cfg.relay_port == kInvalidPort)
        return false;
    st.relay_on = !st.relay_on;
    writeRelay_(cfg, st.relay_on);
    syncButtonState_(cfg, st);
    _dirty_lights = true;
    const char *name = cfg.name.length() ? cfg.name.c_str() : "-";
    _logs.info(F("LIGHT"), F("id: %u name: %s state: %s src: toggle"),
               (unsigned)cfg.id, name, st.relay_on ? "on" : "off");
    return true;
}

bool SocketController::lightRelayState(size_t id, bool &out) const{
    if (!_lights_enabled)
        return false;
    size_t idx = 0;
    if (!lightIndexById_(id, idx))
        return false;
    const LightConfig &cfg = _light_cfg[idx];
    const LightState &st = _light_state[idx];
    if (!cfg.enabled || cfg.relay_port == kInvalidPort)
        return false;
    out = st.relay_on;
    return true;
}

bool SocketController::setLightRelayById(uint8_t id, bool on){ return setLightRelay(id, on); }

bool SocketController::toggleLightRelayById(uint8_t id){ return toggleLightRelay(id); }

bool SocketController::lightRelayStateById(uint8_t id, bool &out) const{ return lightRelayState(id, out); }

bool SocketController::setEnabled(size_t id, bool enable){
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    SocketConfig &cfg = _cfg[idx];
    SocketState &st = _state[idx];
    if (!enable)
    {
        if (cfg.relay_port != kInvalidPort)
            writeRelay_(cfg, false);
        const uint8_t saved_id = cfg.id;
        cfg = SocketConfig{};
        cfg.id = saved_id;
        cfg.enabled = false;
        st = SocketState{};
        _dirty_sockets = true;
        _logs.info(F("SOCKET"), F("Id: %u enabled: false"), (unsigned)cfg.id);
        return true;
    }
    cfg.enabled = true;
    st = SocketState{};
    st.has_button = setupButton_(cfg, st);
    setupRelay_(cfg, st);
    _dirty_sockets = true;
    _logs.info(F("SOCKET"), F("Id: %u enabled: true"), (unsigned)cfg.id);
    return true;
}

bool SocketController::setButtonPort(size_t id, uint8_t port){
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    SocketConfig &cfg = _cfg[idx];
    SocketState &st = _state[idx];
    cfg.button_port = port;
    if (!cfg.enabled)
        return true;
    st.has_button = setupButton_(cfg, st);
    _dirty_sockets = true;
    return true;
}

bool SocketController::setRelayPort(size_t id, uint8_t port){
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    SocketConfig &cfg = _cfg[idx];
    SocketState &st = _state[idx];
    cfg.relay_port = port;
    if (!cfg.enabled)
        return true;
    setupRelay_(cfg, st);
    _dirty_sockets = true;
    return true;
}

bool SocketController::setName(size_t id, const String &name){
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].name = name;
    _dirty_sockets = true;
    return true;
}

bool SocketController::setLightEnabled(size_t id, bool enable){
    size_t idx = 0;
    if (!lightIndexById_(id, idx))
        return false;
    LightConfig &cfg = _light_cfg[idx];
    LightState &st = _light_state[idx];
    if (!enable)
    {
        if (cfg.relay_port != kInvalidPort)
            writeRelay_(cfg, false);
        const uint8_t saved_id = cfg.id;
        String saved_name = cfg.name;
        cfg = LightConfig{};
        cfg.id = saved_id;
        cfg.enabled = false;
        st = LightState{};
        _dirty_lights = true;
        const char *name = saved_name.length() ? saved_name.c_str() : "-";
        _logs.info(F("LIGHT"), F("id: %u name: %s enabled: false"), (unsigned)cfg.id, name);
        return true;
    }
    cfg.enabled = true;
    st = LightState{};
    st.has_button = setupButton_(cfg, st);
    setupRelay_(cfg, st);
    _dirty_lights = true;
    const char *name = cfg.name.length() ? cfg.name.c_str() : "-";
    _logs.info(F("LIGHT"), F("id: %u name: %s enabled: true"), (unsigned)cfg.id, name);
    return true;
}

bool SocketController::setLightButtonPort(size_t id, uint8_t port){
    size_t idx = 0;
    if (!lightIndexById_(id, idx))
        return false;
    LightConfig &cfg = _light_cfg[idx];
    LightState &st = _light_state[idx];
    cfg.button_port = port;
    if (!cfg.enabled)
        return true;
    st.has_button = setupButton_(cfg, st);
    _dirty_lights = true;
    return true;
}

bool SocketController::setLightRelayPort(size_t id, uint8_t port){
    size_t idx = 0;
    if (!lightIndexById_(id, idx))
        return false;
    LightConfig &cfg = _light_cfg[idx];
    LightState &st = _light_state[idx];
    cfg.relay_port = port;
    if (!cfg.enabled)
        return true;
    setupRelay_(cfg, st);
    _dirty_lights = true;
    return true;
}

bool SocketController::setLightName(size_t id, const String &name){
    size_t idx = 0;
    if (!lightIndexById_(id, idx))
        return false;
    _light_cfg[idx].name = name;
    _dirty_lights = true;
    return true;
}

const SocketController::SocketConfig *SocketController::config(size_t id) const{
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_cfg[idx];
}

const SocketController::SocketState *SocketController::state(size_t id) const{
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_state[idx];
}

const SocketController::SocketConfig *SocketController::configByIndex(size_t idx) const{
    if (idx >= kSocketCount)
        return nullptr;
    return &_cfg[idx];
}

const SocketController::SocketState *SocketController::stateByIndex(size_t idx) const{
    if (idx >= kSocketCount)
        return nullptr;
    return &_state[idx];
}

const SocketController::LightConfig *SocketController::lightConfig(size_t id) const{
    size_t idx = 0;
    if (!lightIndexById_(id, idx))
        return nullptr;
    return &_light_cfg[idx];
}

const SocketController::LightState *SocketController::lightState(size_t id) const{
    size_t idx = 0;
    if (!lightIndexById_(id, idx))
        return nullptr;
    return &_light_state[idx];
}

const SocketController::LightConfig *SocketController::lightConfigByIndex(size_t idx) const{
    if (idx >= kLightCount)
        return nullptr;
    return &_light_cfg[idx];
}

const SocketController::LightState *SocketController::lightStateByIndex(size_t idx) const{
    if (idx >= kLightCount)
        return nullptr;
    return &_light_state[idx];
}

void SocketController::serialize(JsonArray out) const{
    for (size_t i = 0; i < kSocketCount; ++i)
    {
        const SocketConfig &cfg = _cfg[i];
        if (!cfg.enabled)
            continue;
        JsonObject obj = out.add<JsonObject>();
        obj["id"] = cfg.id;
        obj["enabled"] = cfg.enabled;
        if (cfg.name.length())
            obj["name"] = cfg.name;
        if (cfg.button_port != kInvalidPort)
            obj["button"] = cfg.button_port;
        if (cfg.relay_port != kInvalidPort)
            obj["relay"] = cfg.relay_port;
    }
}

void SocketController::serializeLights(JsonArray out) const{
    for (size_t i = 0; i < kLightCount; ++i)
    {
        const LightConfig &cfg = _light_cfg[i];
        if (!cfg.enabled)
            continue;
        JsonObject obj = out.add<JsonObject>();
        obj["id"] = cfg.id;
        obj["enabled"] = cfg.enabled;
        if (cfg.name.length())
            obj["name"] = cfg.name;
        if (cfg.button_port != kInvalidPort)
            obj["button"] = cfg.button_port;
        if (cfg.relay_port != kInvalidPort)
            obj["relay"] = cfg.relay_port;
    }
}

void SocketController::buildSnapshot(uint8_t *enabled_mask, uint8_t *state_mask, size_t bytes) const{
    if (!enabled_mask || !state_mask)
        return;
    memset(enabled_mask, 0, bytes);
    memset(state_mask, 0, bytes);
    for (size_t i = 0; i < kSocketCount; ++i)
    {
        const SocketConfig &cfg = _cfg[i];
        const SocketState &st = _state[i];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            continue;
        const size_t byte = i / 8;
        const uint8_t bit = (uint8_t)(1u << (i % 8));
        if (byte >= bytes)
            break;
        enabled_mask[byte] |= bit;
        if (st.relay_on)
            state_mask[byte] |= bit;
    }
}

void SocketController::applySnapshot(const uint8_t *enabled_mask, const uint8_t *state_mask, size_t bytes){
    if (!enabled_mask || !state_mask)
        return;
    for (size_t i = 0; i < kSocketCount; ++i)
    {
        SocketConfig &cfg = _cfg[i];
        SocketState &st = _state[i];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            continue;
        const size_t byte = i / 8;
        const uint8_t bit = (uint8_t)(1u << (i % 8));
        if (byte >= bytes)
            break;
        if ((enabled_mask[byte] & bit) == 0)
            continue;
        const bool on = (state_mask[byte] & bit) != 0;
        if (st.relay_on != on)
        {
            st.relay_on = on;
            writeRelay_(cfg, st.relay_on);
        }
    }
}

void SocketController::buildLightsSnapshot(uint8_t *enabled_mask, uint8_t *state_mask, size_t bytes) const{
    if (!enabled_mask || !state_mask)
        return;
    memset(enabled_mask, 0, bytes);
    memset(state_mask, 0, bytes);
    for (size_t i = 0; i < kLightCount; ++i)
    {
        const LightConfig &cfg = _light_cfg[i];
        const LightState &st = _light_state[i];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            continue;
        const size_t byte = i / 8;
        const uint8_t bit = (uint8_t)(1u << (i % 8));
        if (byte >= bytes)
            break;
        enabled_mask[byte] |= bit;
        if (st.relay_on)
            state_mask[byte] |= bit;
    }
}

void SocketController::applyLightsSnapshot(const uint8_t *enabled_mask, const uint8_t *state_mask, size_t bytes){
    if (!enabled_mask || !state_mask)
        return;
    for (size_t i = 0; i < kLightCount; ++i)
    {
        LightConfig &cfg = _light_cfg[i];
        LightState &st = _light_state[i];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            continue;
        const size_t byte = i / 8;
        const uint8_t bit = (uint8_t)(1u << (i % 8));
        if (byte >= bytes)
            break;
        if ((enabled_mask[byte] & bit) == 0)
            continue;
        const bool on = (state_mask[byte] & bit) != 0;
        if (st.relay_on != on)
        {
            st.relay_on = on;
            writeRelay_(cfg, st.relay_on);
        }
    }
}

bool SocketController::takeDirty(){
    if (!_dirty_sockets)
        return false;
    _dirty_sockets = false;
    return true;
}

bool SocketController::controllerEnabled() const{ return _controller_enabled; }

bool SocketController::lightsEnabled() const{ return _lights_enabled; }

void SocketController::setControllerEnabled(bool enabled){
    if (_controller_enabled == enabled)
        return;
    _controller_enabled = enabled;
    if (!_controller_enabled)
    {
        resetSockets_();
        _dirty_sockets = true;
        return;
    }
    for (size_t i = 0; i < kSocketCount; ++i)
    {
        SocketConfig &cfg = _cfg[i];
        SocketState &st = _state[i];
        if (!cfg.enabled)
            continue;
        st = SocketState{};
        st.has_button = setupButton_(cfg, st);
        setupRelay_(cfg, st);
    }
}

void SocketController::setLightsEnabled(bool enabled){
    if (_lights_enabled == enabled)
        return;
    _lights_enabled = enabled;
    if (!_lights_enabled)
    {
        resetLights_();
        _dirty_lights = true;
        return;
    }
    for (size_t i = 0; i < kLightCount; ++i)
    {
        LightConfig &cfg = _light_cfg[i];
        LightState &st = _light_state[i];
        if (!cfg.enabled)
            continue;
        st = LightState{};
        st.has_button = setupButton_(cfg, st);
        setupRelay_(cfg, st);
    }
}

bool SocketController::takeLightsDirty(){
    if (!_dirty_lights)
        return false;
    _dirty_lights = false;
    return true;
}

void SocketController::resetSockets_(){
    for (size_t i = 0; i < kSocketCount; ++i)
    {
        _cfg[i] = SocketConfig{};
        _state[i] = SocketState{};
        _cfg[i].id = (uint8_t)(i + 1);
    }
}

void SocketController::resetLights_(){
    for (size_t i = 0; i < kLightCount; ++i)
    {
        _light_cfg[i] = LightConfig{};
        _light_state[i] = LightState{};
        _light_cfg[i].id = (uint8_t)(i + 1);
    }
}

bool SocketController::indexById_(uint8_t id, size_t &out) const{
    if (id == 0 || id > kSocketCount)
        return false;
    const size_t direct = static_cast<size_t>(id - 1);
    if (_cfg[direct].id == id)
    {
        out = direct;
        return true;
    }
    for (size_t i = 0; i < kSocketCount; ++i)
    {
        if (_cfg[i].id == id)
        {
            out = i;
            return true;
        }
    }
    return false;
}

bool SocketController::lightIndexById_(uint8_t id, size_t &out) const{
    if (id == 0 || id > kLightCount)
        return false;
    const size_t direct = static_cast<size_t>(id - 1);
    if (_light_cfg[direct].id == id)
    {
        out = direct;
        return true;
    }
    for (size_t i = 0; i < kLightCount; ++i)
    {
        if (_light_cfg[i].id == id)
        {
            out = i;
            return true;
        }
    }
    return false;
}

bool SocketController::parsePort_(JsonVariantConst v, uint8_t &out){
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

bool SocketController::setupButton_(const SocketController::SocketConfig &cfg, SocketController::SocketState &st){
    if (cfg.button_port == kInvalidPort)
        return false;
    const PortIO::PortMode mode = kButtonPullup ? PortIO::PortMode::InputPullUp : PortIO::PortMode::Input;
    if (!_gpio.pinModeDyn(cfg.button_port, mode))
        return false;
    bool raw = false;
    if (!_gpio.readDyn(cfg.button_port, raw))
        return false;
    st.last_button = kButtonInvert ? !raw : raw;
    st.button_idle = st.last_button;
    return true;
}

bool SocketController::setupRelay_(const SocketController::SocketConfig &cfg, SocketController::SocketState &st){
    if (cfg.relay_port == kInvalidPort)
        return false;
    if (!_gpio.pinModeDyn(cfg.relay_port, PortIO::PortMode::Output))
        return false;
    st.relay_on = false;
    writeRelay_(cfg, st.relay_on);
    return true;
}

bool SocketController::syncButtonState_(const SocketController::SocketConfig &cfg, SocketController::SocketState &st){
    if (!st.has_button || cfg.button_port == kInvalidPort)
        return false;
    bool raw = false;
    if (!_gpio.readDyn(cfg.button_port, raw))
        return false;
    st.last_button = kButtonInvert ? !raw : raw;
    st.button_idle = st.last_button;
    return true;
}

void SocketController::writeRelay_(const SocketController::SocketConfig &cfg, bool on){
    const bool out = kRelayInvert ? !on : on;
    _gpio.writeDyn(cfg.relay_port, out);
}
