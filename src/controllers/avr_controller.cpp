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

#include "controllers/avr_controller.hpp"

#include "hal/gpio/gpio_caps.hpp"

AvrController::AvrController(Gpio &gpio, Logger &logs, TelegramBot &bot, TelegramAllowedUsersProvider &users)
 : _gpio(gpio), _logs(logs), _tgbot(bot), _tgusers(users){}

bool AvrController::begin(){
    auto guard = _lock.guard();
    if (!_cfg.enabled)
        return true;
    setupHardware_();
    setRelays_(false, false);
    _st.active_source = Source::Off;
    _st.target_source = Source::Off;
    _st.transfer_state = TransferState::Idle;
    _st.transfer_in_progress = false;
    _main_state_known = false;
    _last_main_ok = false;
    return true;
}

void AvrController::task(){
    auto guard = _lock.guard();
    updateInputs_();
    if (!_cfg.enabled)
    {
        if (_st.active_source != Source::Off || _st.relay_main_on || _st.relay_reserve_on)
        {
            setRelays_(false, false);
            _st.active_source = Source::Off;
            _st.target_source = Source::Off;
            _st.transfer_state = TransferState::Idle;
            _st.transfer_in_progress = false;
        }
        return;
    }

    const uint32_t now = millis();
    if (_st.relay_main_on && _st.relay_reserve_on)
    {
        setFault_(Fault::Interlock);
        setRelays_(false, false);
        _st.active_source = Source::Off;
        _st.transfer_in_progress = false;
        _st.transfer_state = TransferState::Idle;
        return;
    }

    if (_st.transfer_in_progress)
    {
        processTransfer_(now);
        return;
    }

    if (_st.fault != Fault::None)
    {
        if (_cfg.auto_mode && (_st.main_ok || _st.reserve_ok))
            clearFault();
        else
            return;
    }

    if (!_cfg.auto_mode)
    {
        const Source desired = _st.manual_source;
        if (desired != _st.active_source)
            startTransfer_(desired, now);
        return;
    }

    const Source desired = decideAutoSource_(now);
    if (desired != _st.active_source)
        startTransfer_(desired, now);
}

void AvrController::applyConfig(JsonObjectConst obj){
    auto guard = _lock.guard();
    Config next = _cfg;
    if (obj["enabled"].is<bool>())
        next.enabled = obj["enabled"].as<bool>();
    if (obj["auto_mode"].is<bool>())
        next.auto_mode = obj["auto_mode"].as<bool>();
    if (obj["prefer_main"].is<bool>())
        next.prefer_main = obj["prefer_main"].as<bool>();
    if (obj["auto_return_main"].is<bool>())
        next.auto_return_main = obj["auto_return_main"].as<bool>();

    parsePort_(obj["main_ok"], next.main_ok_port);
    parsePort_(obj["reserve_ok"], next.reserve_ok_port);
    parsePort_(obj["relay_main"], next.relay_main_port);
    parsePort_(obj["relay_reserve"], next.relay_reserve_port);
    parsePort_(obj["feedback_main"], next.feedback_main_port);
    parsePort_(obj["feedback_reserve"], next.feedback_reserve_port);

    if (obj["main_ok_active_low"].is<bool>())
        next.main_ok_active_low = obj["main_ok_active_low"].as<bool>();
    if (obj["reserve_ok_active_low"].is<bool>())
        next.reserve_ok_active_low = obj["reserve_ok_active_low"].as<bool>();
    if (obj["feedback_main_active_low"].is<bool>())
        next.feedback_main_active_low = obj["feedback_main_active_low"].as<bool>();
    if (obj["feedback_reserve_active_low"].is<bool>())
        next.feedback_reserve_active_low = obj["feedback_reserve_active_low"].as<bool>();
    if (obj["relay_main_invert"].is<bool>())
        next.relay_main_invert = obj["relay_main_invert"].as<bool>();
    if (obj["relay_reserve_invert"].is<bool>())
        next.relay_reserve_invert = obj["relay_reserve_invert"].as<bool>();

    parseMs_(obj["debounce_ms"], next.debounce_ms);
    parseMs_(obj["loss_delay_ms"], next.loss_delay_ms);
    parseMs_(obj["return_delay_ms"], next.return_delay_ms);
    parseMs_(obj["break_ms"], next.break_ms);
    parseMs_(obj["warmup_ms"], next.warmup_ms);
    parseMs_(obj["transfer_timeout_ms"], next.transfer_timeout_ms);

    _cfg = next;
    setupHardware_();
}

void AvrController::serialize(JsonObject out) const{
    auto guard = _lock.guard();
    out["enabled"] = _cfg.enabled;
    out["auto_mode"] = _cfg.auto_mode;
    out["prefer_main"] = _cfg.prefer_main;
    out["auto_return_main"] = _cfg.auto_return_main;
    if (_cfg.main_ok_port != kInvalidPort)
        out["main_ok"] = _cfg.main_ok_port;
    if (_cfg.reserve_ok_port != kInvalidPort)
        out["reserve_ok"] = _cfg.reserve_ok_port;
    if (_cfg.relay_main_port != kInvalidPort)
        out["relay_main"] = _cfg.relay_main_port;
    if (_cfg.relay_reserve_port != kInvalidPort)
        out["relay_reserve"] = _cfg.relay_reserve_port;
    if (_cfg.feedback_main_port != kInvalidPort)
        out["feedback_main"] = _cfg.feedback_main_port;
    if (_cfg.feedback_reserve_port != kInvalidPort)
        out["feedback_reserve"] = _cfg.feedback_reserve_port;

    out["main_ok_active_low"] = _cfg.main_ok_active_low;
    out["reserve_ok_active_low"] = _cfg.reserve_ok_active_low;
    out["feedback_main_active_low"] = _cfg.feedback_main_active_low;
    out["feedback_reserve_active_low"] = _cfg.feedback_reserve_active_low;
    out["relay_main_invert"] = _cfg.relay_main_invert;
    out["relay_reserve_invert"] = _cfg.relay_reserve_invert;

    out["debounce_ms"] = _cfg.debounce_ms;
    out["loss_delay_ms"] = _cfg.loss_delay_ms;
    out["return_delay_ms"] = _cfg.return_delay_ms;
    out["break_ms"] = _cfg.break_ms;
    out["warmup_ms"] = _cfg.warmup_ms;
    out["transfer_timeout_ms"] = _cfg.transfer_timeout_ms;
}

bool AvrController::setControllerEnabled(bool enabled){
    auto guard = _lock.guard();
    if (_cfg.enabled == enabled)
        return false;
    _cfg.enabled = enabled;
    if (!enabled)
    {
        setRelays_(false, false);
        _st.active_source = Source::Off;
        _st.target_source = Source::Off;
        _st.transfer_state = TransferState::Idle;
        _st.transfer_in_progress = false;
        _st.fault = Fault::None;
        _main_state_known = false;
        _last_main_ok = false;
    }
    else
    {
        setupHardware_();
        setRelays_(false, false);
        _st.active_source = Source::Off;
        _st.target_source = Source::Off;
        _st.transfer_state = TransferState::Idle;
        _st.transfer_in_progress = false;
        _main_state_known = false;
        _last_main_ok = false;
    }
    return true;
}

bool AvrController::controllerEnabled() const{
    auto guard = _lock.guard();
    return _cfg.enabled;
}

bool AvrController::setAutoMode(bool auto_mode){
    auto guard = _lock.guard();
    if (_cfg.auto_mode == auto_mode)
        return false;
    _cfg.auto_mode = auto_mode;
    if (auto_mode)
        _st.manual_source = Source::Off;
    return true;
}

bool AvrController::autoMode() const{
    auto guard = _lock.guard();
    return _cfg.auto_mode;
}

bool AvrController::setPreferMain(bool prefer_main){
    auto guard = _lock.guard();
    if (_cfg.prefer_main == prefer_main)
        return false;
    _cfg.prefer_main = prefer_main;
    return true;
}

bool AvrController::preferMain() const{
    auto guard = _lock.guard();
    return _cfg.prefer_main;
}

bool AvrController::setAutoReturnMain(bool auto_return){
    auto guard = _lock.guard();
    if (_cfg.auto_return_main == auto_return)
        return false;
    _cfg.auto_return_main = auto_return;
    return true;
}

bool AvrController::autoReturnMain() const{
    auto guard = _lock.guard();
    return _cfg.auto_return_main;
}

bool AvrController::setManualSource(AvrController::Source src){
    auto guard = _lock.guard();
    if (_st.manual_source == src)
        return false;
    _st.manual_source = src;
    if (!_cfg.auto_mode)
        startTransfer_(src, millis());
    return true;
}

AvrController::Source AvrController::manualSource() const{
    auto guard = _lock.guard();
    return _st.manual_source;
}

AvrController::Source AvrController::activeSource() const{
    auto guard = _lock.guard();
    return _st.active_source;
}

const AvrController::Config &AvrController::config() const{
    auto guard = _lock.guard();
    return _cfg;
}

const AvrController::State &AvrController::state() const{
    auto guard = _lock.guard();
    return _st;
}

bool AvrController::setMainOkPort(uint8_t port){
    auto guard = _lock.guard();
    if (_cfg.main_ok_port == port)
        return false;
    _cfg.main_ok_port = port;
    setupInputPort_(port);
    return true;
}

bool AvrController::setReserveOkPort(uint8_t port){
    auto guard = _lock.guard();
    if (_cfg.reserve_ok_port == port)
        return false;
    _cfg.reserve_ok_port = port;
    setupInputPort_(port);
    return true;
}

bool AvrController::setRelayMainPort(uint8_t port){
    auto guard = _lock.guard();
    if (_cfg.relay_main_port == port)
        return false;
    _cfg.relay_main_port = port;
    setupRelayPort_(port);
    if (_cfg.relay_main_port == kInvalidPort)
        _st.relay_main_on = false;
    return true;
}

bool AvrController::setRelayReservePort(uint8_t port){
    auto guard = _lock.guard();
    if (_cfg.relay_reserve_port == port)
        return false;
    _cfg.relay_reserve_port = port;
    setupRelayPort_(port);
    if (_cfg.relay_reserve_port == kInvalidPort)
        _st.relay_reserve_on = false;
    return true;
}

bool AvrController::setFeedbackMainPort(uint8_t port){
    auto guard = _lock.guard();
    if (_cfg.feedback_main_port == port)
        return false;
    _cfg.feedback_main_port = port;
    setupInputPort_(port);
    return true;
}

bool AvrController::setFeedbackReservePort(uint8_t port){
    auto guard = _lock.guard();
    if (_cfg.feedback_reserve_port == port)
        return false;
    _cfg.feedback_reserve_port = port;
    setupInputPort_(port);
    return true;
}

bool AvrController::transferInProgress() const{
    auto guard = _lock.guard();
    return _st.transfer_in_progress;
}

AvrController::Fault AvrController::fault() const{
    auto guard = _lock.guard();
    return _st.fault;
}

void AvrController::clearFault(){
    auto guard = _lock.guard();
    _st.fault = Fault::None;
    _st.fault_ms = 0;
}

const char *AvrController::sourceName(AvrController::Source s){
    switch (s)
    {
    case Source::Main:
        return "main";
    case Source::Reserve:
        return "reserve";
    default:
        return "off";
    }
}

const char *AvrController::faultName(AvrController::Fault f){
    switch (f)
    {
    case Fault::NoSource:
        return "no_source";
    case Fault::TransferTimeout:
        return "transfer_timeout";
    case Fault::Interlock:
        return "interlock";
    case Fault::FeedbackMismatch:
        return "feedback_mismatch";
    default:
        return "none";
    }
}

void AvrController::setupHardware_(){
    setupInputPort_(_cfg.main_ok_port);
    setupInputPort_(_cfg.reserve_ok_port);
    setupInputPort_(_cfg.feedback_main_port);
    setupInputPort_(_cfg.feedback_reserve_port);
    setupRelayPort_(_cfg.relay_main_port);
    setupRelayPort_(_cfg.relay_reserve_port);
}

void AvrController::setupInputPort_(uint8_t port){
    if (port == kInvalidPort)
        return;
    PortIO::PortMode mode = PortIO::PortMode::Input;
    const Cap caps = _gpio.capsDyn(port);
    if (has(caps, Cap::PullUp))
        mode = PortIO::PortMode::InputPullUp;
    if (!_gpio.pinModeDyn(port, mode))
        _gpio.pinModeDyn(port, PortIO::PortMode::Input);
}

void AvrController::setupRelayPort_(uint8_t port){
    if (port == kInvalidPort)
        return;
    _gpio.pinModeDyn(port, PortIO::PortMode::Output);
}

bool AvrController::parsePort_(JsonVariantConst v, uint8_t &out){
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

void AvrController::parseMs_(JsonVariantConst v, uint32_t &out){
    if (!v.is<unsigned>())
        return;
    out = (uint32_t)v.as<unsigned>();
}

bool AvrController::readInput_(uint8_t port, bool active_low, bool &out){
    if (port == kInvalidPort)
        return false;
    bool raw = false;
    if (!_gpio.readDyn(port, raw))
        return false;
    out = active_low ? !raw : raw;
    return true;
}

bool AvrController::updateDebounce_(AvrController::InputDebounce &db, bool value, uint32_t now){
    if (!db.initialized)
    {
        db.initialized = true;
        db.raw = value;
        db.stable = value;
        db.raw_since_ms = now;
        return false;
    }
    if (db.raw != value)
    {
        db.raw = value;
        db.raw_since_ms = now;
        return false;
    }
    if (db.stable != db.raw && (uint32_t)(now - db.raw_since_ms) >= _cfg.debounce_ms)
    {
        db.stable = db.raw;
        return true;
    }
    return false;
}

void AvrController::updateInputs_(){
    const uint32_t now = millis();
    bool val = false;
    if (readInput_(_cfg.main_ok_port, _cfg.main_ok_active_low, val))
        updateDebounce_(_main_ok_db, val, now);
    if (readInput_(_cfg.reserve_ok_port, _cfg.reserve_ok_active_low, val))
        updateDebounce_(_reserve_ok_db, val, now);
    if (readInput_(_cfg.feedback_main_port, _cfg.feedback_main_active_low, val))
        updateDebounce_(_fb_main_db, val, now);
    if (readInput_(_cfg.feedback_reserve_port, _cfg.feedback_reserve_active_low, val))
        updateDebounce_(_fb_reserve_db, val, now);

    _st.main_ok = _main_ok_db.stable;
    _st.reserve_ok = _reserve_ok_db.stable;
    _st.fb_main_on = _fb_main_db.stable;
    _st.fb_reserve_on = _fb_reserve_db.stable;
    notifyMainStateIfChanged_();
}

void AvrController::setRelays_(bool main_on, bool reserve_on){
    if (main_on && reserve_on)
    {
        main_on = false;
        reserve_on = false;
        setFault_(Fault::Interlock);
    }
    _st.relay_main_on = main_on;
    _st.relay_reserve_on = reserve_on;

    if (_cfg.relay_main_port != kInvalidPort)
        _gpio.writeDyn(_cfg.relay_main_port, _cfg.relay_main_invert ? !main_on : main_on);
    if (_cfg.relay_reserve_port != kInvalidPort)
        _gpio.writeDyn(_cfg.relay_reserve_port, _cfg.relay_reserve_invert ? !reserve_on : reserve_on);
}

void AvrController::setFault_(AvrController::Fault f){
    if (_st.fault == f)
        return;
    _st.fault = f;
    _st.fault_ms = millis();
    _logs.error(F("AVR"), F("fault: %s"), faultName(f));
}

AvrController::Source AvrController::decideAutoSource_(uint32_t now){
    if (_st.active_source == Source::Main)
    {
        if (_st.main_ok)
        {
            _main_lost_since_ms = 0;
            return Source::Main;
        }
        if (_main_lost_since_ms == 0)
            _main_lost_since_ms = now;
        if ((uint32_t)(now - _main_lost_since_ms) < _cfg.loss_delay_ms)
            return Source::Main;
        if (_st.reserve_ok)
            return Source::Reserve;
        setFault_(Fault::NoSource);
        return Source::Off;
    }

    if (_st.active_source == Source::Reserve)
    {
        if (_cfg.auto_return_main && _st.main_ok)
        {
            if (_main_ok_since_ms == 0)
                _main_ok_since_ms = now;
            if ((uint32_t)(now - _main_ok_since_ms) >= _cfg.return_delay_ms)
                return Source::Main;
        }
        else
        {
            _main_ok_since_ms = 0;
        }
        if (_st.reserve_ok)
            return Source::Reserve;
        if (_st.main_ok)
            return Source::Main;
        setFault_(Fault::NoSource);
        return Source::Off;
    }

    // Off
    _main_lost_since_ms = 0;
    _main_ok_since_ms = _st.main_ok ? (_main_ok_since_ms ? _main_ok_since_ms : now) : 0;
    if (_cfg.prefer_main)
    {
        if (_st.main_ok)
            return Source::Main;
        if (_st.reserve_ok)
            return Source::Reserve;
    }
    else
    {
        if (_st.reserve_ok)
            return Source::Reserve;
        if (_st.main_ok)
            return Source::Main;
    }
    return Source::Off;
}

void AvrController::startTransfer_(AvrController::Source target, uint32_t now){
    if (target == _st.active_source)
        return;
    _st.target_source = target;
    _st.transfer_in_progress = true;
    _st.transfer_state = TransferState::Break;
    _st.transfer_start_ms = now;
    _st.transfer_step_ms = now;
    setRelays_(false, false);
    _logs.info(F("AVR"), F("transfer: %s -> %s"),
               sourceName(_st.active_source), sourceName(target));
}

void AvrController::processTransfer_(uint32_t now){
    if ((uint32_t)(now - _st.transfer_start_ms) > _cfg.transfer_timeout_ms)
    {
        setFault_(Fault::TransferTimeout);
        setRelays_(false, false);
        _st.active_source = Source::Off;
        _st.transfer_in_progress = false;
        _st.transfer_state = TransferState::Idle;
        return;
    }

    switch (_st.transfer_state)
    {
    case TransferState::Break:
        if ((uint32_t)(now - _st.transfer_step_ms) < _cfg.break_ms)
            return;
        if (_st.target_source == Source::Reserve && _cfg.warmup_ms > 0)
        {
            _st.transfer_state = TransferState::Warmup;
            _st.transfer_step_ms = now;
            return;
        }
        _st.transfer_state = TransferState::Apply;
        _st.transfer_step_ms = now;
        break;
    case TransferState::Warmup:
        if ((uint32_t)(now - _st.transfer_step_ms) < _cfg.warmup_ms)
            return;
        _st.transfer_state = TransferState::Apply;
        _st.transfer_step_ms = now;
        break;
    case TransferState::Apply:
    {
        const Source prev_source = _st.active_source;
        const bool main_on = _st.target_source == Source::Main;
        const bool reserve_on = _st.target_source == Source::Reserve;
        setRelays_(main_on, reserve_on);
        if ((_cfg.feedback_main_port != kInvalidPort || _cfg.feedback_reserve_port != kInvalidPort) &&
            ((main_on && !_st.fb_main_on && _cfg.feedback_main_port != kInvalidPort) ||
             (reserve_on && !_st.fb_reserve_on && _cfg.feedback_reserve_port != kInvalidPort)))
        {
            setFault_(Fault::FeedbackMismatch);
            setRelays_(false, false);
            _st.active_source = Source::Off;
        }
        else
        {
            _st.active_source = _st.target_source;
            _st.source_since_ms = now;
            _st.switch_count++;
            notifySourceSwitched_(prev_source, _st.active_source);
        }
        _st.transfer_in_progress = false;
        _st.transfer_state = TransferState::Idle;
        _st.target_source = _st.active_source;
        _main_lost_since_ms = 0;
        _main_ok_since_ms = _st.main_ok ? now : 0;
        return;
    }
    default:
        _st.transfer_state = TransferState::Idle;
        _st.transfer_in_progress = false;
        return;
    }
}

void AvrController::notifyMainStateIfChanged_(){
    if (!_cfg.enabled)
    {
        _main_state_known = false;
        _last_main_ok = _st.main_ok;
        return;
    }
    if (!_main_state_known)
    {
        _main_state_known = true;
        _last_main_ok = _st.main_ok;
        return;
    }
    if (_last_main_ok == _st.main_ok)
        return;
    _last_main_ok = _st.main_ok;
    if (_st.main_ok)
    {
        _logs.info(F("AVR"), F("main: restored"));
        sendTgNotify_(F("AVR: main power restored"));
    }
    else
    {
        _logs.warn(F("AVR"), F("main: lost"));
        sendTgNotify_(F("AVR: main power lost"));
    }
}

void AvrController::notifySourceSwitched_(AvrController::Source from, AvrController::Source to){
    if (from == to)
        return;
    if (to == Source::Reserve)
    {
        _logs.warn(F("AVR"), F("source switched: reserve"));
        sendTgNotify_(F("AVR: power switched to reserve"));
    }
    else if (to == Source::Main)
    {
        _logs.info(F("AVR"), F("source switched: main"));
        sendTgNotify_(F("AVR: power switched to main"));
    }
}

void AvrController::sendTgNotify_(const String &msg){
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
