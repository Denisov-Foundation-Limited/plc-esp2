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

#include "controllers/ring_controller.hpp"

#include "hal/gpio/gpio_caps.hpp"

RingController::RingController(Gpio &gpio, Logger &logs) : _gpio(gpio), _logs(logs){
}

bool RingController::begin(){
    auto guard = _lock.guard();
    setupHardware_();
    return true;
}

void RingController::task(){
    {
        auto guard = _lock.guard();
        if (!_cfg.enabled)
        {
            ensureRelayOff_();
            return;
        }
        handleButton_();
        pollStackHoldTimeout_();
    }
    if (_pending_hold_notify)
    {
        const bool on = _pending_hold_on;
        _pending_hold_notify = false;
        if (_hold_cb)
            _hold_cb(_hold_ctx, on);
        if (_hold_cb_secondary)
            _hold_cb_secondary(_hold_ctx_secondary, on);
    }
}

void RingController::applyConfig(JsonObjectConst obj){
    auto guard = _lock.guard();
    Config next = _cfg;
    if (obj["enabled"].is<bool>())
        next.enabled = obj["enabled"].as<bool>();
    parsePort_(obj["button"], next.button_port);
    parsePort_(obj["relay"], next.relay_port);
    _cfg = next;
    setupHardware_();
}

void RingController::serialize(JsonObject out) const{
    auto guard = _lock.guard();
    out["enabled"] = _cfg.enabled;
    if (_cfg.button_port != kInvalidPort)
        out["button"] = _cfg.button_port;
    if (_cfg.relay_port != kInvalidPort)
        out["relay"] = _cfg.relay_port;
}

bool RingController::setControllerEnabled(bool enabled){
    auto guard = _lock.guard();
    if (_cfg.enabled == enabled)
        return false;
    if (!enabled)
    {
        ensureRelayOff_();
        _cfg = Config{};
        _st = State{};
        return true;
    }
    _cfg.enabled = true;
    return true;
}

bool RingController::controllerEnabled() const{
    auto guard = _lock.guard();
    return _cfg.enabled;
}

bool RingController::setButtonPort(uint8_t port){
    auto guard = _lock.guard();
    if (_cfg.button_port == port)
        return false;
    _cfg.button_port = port;
    _st.has_button = setupButton_();
    return true;
}

bool RingController::setRelayPort(uint8_t port){
    auto guard = _lock.guard();
    if (_cfg.relay_port == port)
        return false;
    _cfg.relay_port = port;
    _st.has_relay = setupRelay_();
    return true;
}

bool RingController::setHoldRelay(bool on){
    auto guard = _lock.guard();
    return setHoldRelay_(on, true, Source::Unknown);
}

bool RingController::setHoldRelayLocal(bool on){
    auto guard = _lock.guard();
    return setHoldRelay_(on, false, Source::Unknown);
}

bool RingController::setHoldRelayWithSource(bool on, RingController::Source source){
    auto guard = _lock.guard();
    return setHoldRelay_(on, true, source);
}

bool RingController::setHoldRelayLocalWithSource(bool on, RingController::Source source){
    auto guard = _lock.guard();
    return setHoldRelay_(on, false, source);
}

const RingController::Config &RingController::config() const{
    auto guard = _lock.guard();
    return _cfg;
}

const RingController::State &RingController::state() const{
    auto guard = _lock.guard();
    return _st;
}

RingController::Source RingController::lastSource() const{
    auto guard = _lock.guard();
    return static_cast<Source>(_st.last_source);
}

void RingController::setHoldHandler(RingController::HoldHandler cb, void *ctx){
    auto guard = _lock.guard();
    _hold_cb = cb;
    _hold_ctx = ctx;
}

void RingController::setHoldHandlerSecondary(RingController::HoldHandler cb, void *ctx){
    auto guard = _lock.guard();
    _hold_cb_secondary = cb;
    _hold_ctx_secondary = ctx;
}

void RingController::setupHardware_(){
    _st.has_button = setupButton_();
    _st.has_relay = setupRelay_();
    if (!_cfg.enabled)
        ensureRelayOff_();
}

void RingController::ensureRelayOff_(){
    if (_st.hold_active)
        setHoldActive_(false, false);
    if (_st.relay_on)
    {
        _st.relay_on = false;
        writeRelay_(false);
    }
    _st.stack_hold_until_ms = 0;
}

void RingController::handleButton_(){
    if (!_st.has_button)
        return;
    bool raw = false;
    if (!_gpio.readDyn(_cfg.button_port, raw))
        return;
    const bool pressed = kButtonActiveLow ? !raw : raw;
    const uint32_t now = millis();
    if (pressed)
    {
        if (_st.cooldown_until_ms != 0 && (int32_t)(now - _st.cooldown_until_ms) < 0)
        {
            _st.last_button = pressed;
            return;
        }
        _st.last_source = static_cast<uint8_t>(Source::Button);
        _st.stack_hold_until_ms = 0;
        setHoldActive_(true, true);
    }
    else if (_st.last_button)
    {
        setHoldActive_(false, true);
        _st.cooldown_until_ms = now + kReleaseCooldownMs;
        _st.stack_hold_until_ms = 0;
    }
    _st.last_button = pressed;
}

void RingController::setHoldActive_(bool on, bool notify){
    if (_st.hold_active == on)
        return;
    _st.hold_active = on;
    _st.relay_on = on;
    writeRelay_(on);
    if (notify && _hold_cb)
    {
        _pending_hold_notify = true;
        _pending_hold_on = on;
    }
}

bool RingController::setHoldRelay_(bool on, bool notify, RingController::Source source){
    if (!_cfg.enabled || _cfg.relay_port == kInvalidPort)
        return false;
    const uint32_t now = millis();
    if (on)
    {
        if (_st.cooldown_until_ms != 0 && (int32_t)(now - _st.cooldown_until_ms) < 0)
            return false;
        _st.last_source = static_cast<uint8_t>(source);
        if (source == Source::Stack)
            _st.stack_hold_until_ms = now + kStackHoldTimeoutMs;
        else
            _st.stack_hold_until_ms = 0;
    }
    const bool was_on = _st.hold_active;
    setHoldActive_(on, notify);
    if (!on)
    {
        if (was_on)
            _st.cooldown_until_ms = now + kReleaseCooldownMs;
        _st.stack_hold_until_ms = 0;
    }
    return true;
}

void RingController::pollStackHoldTimeout_(){
    if (!_st.hold_active)
        return;
    if (_st.last_source != static_cast<uint8_t>(Source::Stack))
        return;
    if (_st.stack_hold_until_ms == 0)
        return;
    const uint32_t now = millis();
    if ((int32_t)(now - _st.stack_hold_until_ms) < 0)
        return;
    _logs.warn(F("RING"), F("failsafe timeout: source: stack"));
    setHoldActive_(false, true);
    _st.cooldown_until_ms = now + kReleaseCooldownMs;
    _st.stack_hold_until_ms = 0;
}

bool RingController::setupButton_(){
    if (_cfg.button_port == kInvalidPort)
        return false;
    const Cap caps = _gpio.capsDyn(_cfg.button_port);
    PortIO::PortMode mode = PortIO::PortMode::Input;
    if (kButtonPullup && has(caps, Cap::PullUp))
        mode = PortIO::PortMode::InputPullUp;
    if (!_gpio.pinModeDyn(_cfg.button_port, mode))
    {
        if (mode != PortIO::PortMode::Input)
        {
            mode = PortIO::PortMode::Input;
            if (!_gpio.pinModeDyn(_cfg.button_port, mode))
                return false;
        }
        else
        {
            return false;
        }
    }
    bool raw = false;
    if (!_gpio.readDyn(_cfg.button_port, raw))
        return false;
    _st.last_button = kButtonActiveLow ? !raw : raw;
    return true;
}

bool RingController::setupRelay_(){
    if (_cfg.relay_port == kInvalidPort)
        return false;
    if (!_gpio.pinModeDyn(_cfg.relay_port, PortIO::PortMode::Output))
        return false;
    _st.relay_on = false;
    writeRelay_(false);
    return true;
}

void RingController::writeRelay_(bool on){
    if (_cfg.relay_port == kInvalidPort)
        return;
    const bool out = kRelayInvert ? !on : on;
    _gpio.writeDyn(_cfg.relay_port, out);
}

bool RingController::parsePort_(JsonVariantConst v, uint8_t &out){
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
