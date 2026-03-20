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

#include "plc/plc_control.hpp"

using PlcAlarmModule = PlcControl::AlarmModule;
using PlcError = PlcControl::Error;

PlcControl::PlcControl(I2CManager &i2c, IoStack &io, RTC &rtc)
    : _i2c(i2c), _io(io), _rtc(rtc), _device_name(ActiveBoardProfile::UI_NAME){
}

bool PlcControl::begin(){
    const auto cfg = ActiveBoardProfile::BOARD_TEMP;
    _lm75_bus = cfg.bus_num;
    if (!busExists_(cfg.bus_num))
    {
        _err = Error::InvalidConfig;
        return false;
    }

    TwoWire *wire = _i2c.wirePtr(cfg.bus_num);
    if (!wire)
    {
        _err = Error::NoBus;
        return false;
    }

    {
        I2CManager::ScopedBusLock lk(_i2c, _lm75_bus);
        if (!lk.locked())
        {
            _err = Error::I2c;
            return false;
        }
        if (!_lm75.begin(*wire, cfg.addr))
        {
            _err = Error::I2c;
            return false;
        }
    }

    _fan_on_c = cfg.fan_on_c;
    _fan_hyst_c = cfg.hysteresis_c;
    _err = Error::Ok;
    return true;
}

void PlcControl::task(){
    const uint32_t now = millis();
    if (timeDue_(now, _next_sample_ms))
    {
        _next_sample_ms = now + _sample_interval_ms;
        float sample = 0.0f;
        {
            I2CManager::ScopedBusLock lk(_i2c, _lm75_bus);
            if (lk.locked() && _lm75.readTempC(sample))
            {
                _last_temp_c = sample;
                _temp_valid = true;
            }
        }
        float rtc_sample = 0.0f;
        if (_rtc.readTemp(rtc_sample))
        {
            _last_rtc_temp_c = rtc_sample;
            _rtc_temp_valid = true;
        }
    }

    if (_fan_manual)
    {
        setFans_(_fan_on);
        return;
    }
    float temp_c = 0.0f;
    bool has_temp = false;
    if (_temp_valid)
    {
        temp_c = _last_temp_c;
        has_temp = true;
    }
    if (_rtc_temp_valid && (!has_temp || _last_rtc_temp_c > temp_c))
    {
        temp_c = _last_rtc_temp_c;
        has_temp = true;
    }
    if (!has_temp)
        return;
    bool want = _fan_on;
    const float on_c = _fan_on_c;
    const float off_c = on_c - _fan_hyst_c;

    if (!_fan_on && temp_c >= on_c)
        want = true;
    else if (_fan_on && temp_c <= off_c)
        want = false;

    if (want != _fan_on)
    {
        _fan_on = want;
        setFans_(_fan_on);
    }

    updateAlarmLed_(now);
}

float PlcControl::boardTemp() const{ return _last_temp_c; }

bool PlcControl::fanStatus() const{ return _fan_on; }

const String &PlcControl::deviceName() const{ return _device_name; }

void PlcControl::setDeviceName(const String &name){ _device_name = name; }

bool PlcControl::buzzerEnabled() const{ return _buzzer_enabled; }

void PlcControl::setBuzzerEnabled(bool enabled){ _buzzer_enabled = enabled; }

bool PlcControl::fanManualMode() const{ return _fan_manual; }

float PlcControl::fanOnC() const{ return _fan_on_c; }

float PlcControl::fanHysteresisC() const{ return _fan_hyst_c; }

bool PlcControl::portState(uint8_t id, bool &out) const{
    if (id >= IoStack::PORT_COUNT)
        return false;
    const auto &p = _io.desc(id);
    if (p.caps == Cap::None)
        return false;
    return _io.tryRead(id, out);
}

void PlcControl::setFanAuto(){ _fan_manual = false; }

void PlcControl::setFanManual(bool on){
    _fan_manual = true;
    _fan_on = on;
    setFans_(_fan_on);
}

void PlcControl::setFanThresholds(float on_c, float hyst_c){
    _fan_on_c = on_c;
    _fan_hyst_c = hyst_c;
}

void PlcControl::setAlarmMask(uint32_t mask){ _alarm_mask = mask; }

uint32_t PlcControl::alarmMask() const{ return _alarm_mask; }

void PlcControl::setAlarmUnitMask(PlcAlarmModule module, uint32_t mask){
    const uint8_t idx = static_cast<uint8_t>(module);
    if (idx >= kAlarmModuleCount)
        return;
    _alarm_unit_mask[idx] = mask;
    setAlarmModule_(idx, (_alarm_detail_mask[idx] | _alarm_unit_mask[idx]) != 0);
}

uint32_t PlcControl::alarmUnitMask(PlcAlarmModule module) const{
    const uint8_t idx = static_cast<uint8_t>(module);
    if (idx >= kAlarmModuleCount)
        return 0;
    return _alarm_unit_mask[idx];
}

void PlcControl::setAlarmDetailMask(PlcAlarmModule module, uint32_t mask){
    const uint8_t idx = static_cast<uint8_t>(module);
    if (idx >= kAlarmModuleCount)
        return;
    _alarm_detail_mask[idx] = mask;
    setAlarmModule_(idx, (_alarm_detail_mask[idx] | _alarm_unit_mask[idx]) != 0);
}

uint32_t PlcControl::alarmDetailMask(PlcAlarmModule module) const{
    const uint8_t idx = static_cast<uint8_t>(module);
    if (idx >= kAlarmModuleCount)
        return 0;
    return _alarm_detail_mask[idx];
}

void PlcControl::setAlarmModule(PlcAlarmModule module, bool has_alarm){
    setAlarmModule_(static_cast<uint8_t>(module), has_alarm);
}

void PlcControl::setAlarm(PlcAlarmModule module){ setAlarmModule(module, true); }

void PlcControl::clearAlarm(PlcAlarmModule module){ setAlarmModule(module, false); }

void PlcControl::setAlarmModule(uint8_t module, bool has_alarm){
    setAlarmModule_(module, has_alarm);
}

void PlcControl::setAlarmDetail(PlcAlarmModule module, uint8_t bit, bool has_alarm){
    setAlarmDetail_(module, bit, has_alarm);
}

void PlcControl::setAlarmDetailBit(PlcAlarmModule module, uint8_t bit){ setAlarmDetail(module, bit, true); }

void PlcControl::clearAlarmDetailBit(PlcAlarmModule module, uint8_t bit){ setAlarmDetail(module, bit, false); }

void PlcControl::setAlarmUnit(PlcAlarmModule module, uint8_t unit_bit, bool has_alarm){
    const uint8_t idx = static_cast<uint8_t>(module);
    if (idx >= kAlarmModuleCount)
        return;
    if (unit_bit >= 32)
        return;
    const uint32_t bit = 1u << unit_bit;
    if (has_alarm)
        _alarm_unit_mask[idx] |= bit;
    else
        _alarm_unit_mask[idx] &= ~bit;
    setAlarmModule_(idx, (_alarm_detail_mask[idx] | _alarm_unit_mask[idx]) != 0);
}

PlcError PlcControl::lastError() const{ return _err; }

void PlcControl::setFans_(bool on){
    for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
    {
        const auto &p = _io.desc(i);
        if (p.caps == Cap::None)
            continue;
        if (p.type != PortIO::PinType::Fan)
            continue;
        if (!has(p.caps, Cap::Output))
            continue;
        _io.write(i, on);
    }
}

void PlcControl::setAlarmModule_(uint8_t module, bool has_alarm){
    if (module >= 32)
        return;
    const uint32_t bit = 1u << module;
    if (has_alarm)
        _alarm_mask |= bit;
    else
        _alarm_mask &= ~bit;
}

void PlcControl::setAlarmDetail_(PlcAlarmModule module, uint8_t bit_index, bool has_alarm){
    const uint8_t idx = static_cast<uint8_t>(module);
    if (idx >= kAlarmModuleCount)
        return;
    if (bit_index >= 32)
        return;
    const uint32_t bit = 1u << bit_index;
    if (has_alarm)
        _alarm_detail_mask[idx] |= bit;
    else
        _alarm_detail_mask[idx] &= ~bit;
    setAlarmModule_(idx, (_alarm_detail_mask[idx] | _alarm_unit_mask[idx]) != 0);
}

void PlcControl::ensureAlarmLed_(){
    if (_alarm_led_ready)
        return;
    const uint8_t pin = ActiveBoardProfile::ALARM_LED_PIN;
    if (pin == 0xFF)
        return;
    _io.pinMode(pin, PortIO::PortMode::Output);
    _io.write(pin, false);
    _alarm_led_ready = true;
    _alarm_led_state = false;
}

void PlcControl::setAlarmLed_(bool on){
    const uint8_t pin = ActiveBoardProfile::ALARM_LED_PIN;
    if (pin == 0xFF)
        return;
    _io.write(pin, on);
    _alarm_led_state = on;
}

void PlcControl::updateAlarmLed_(uint32_t now){
    uint32_t detail_any = 0;
    for (size_t i = 0; i < kAlarmModuleCount; ++i)
        detail_any |= _alarm_detail_mask[i];
    uint32_t unit_any = 0;
    for (size_t i = 0; i < kAlarmModuleCount; ++i)
        unit_any |= _alarm_unit_mask[i];
    if ((_alarm_mask | detail_any | unit_any) == 0)
    {
        if (_alarm_led_state)
            setAlarmLed_(false);
        return;
    }
    ensureAlarmLed_();
    if (!_alarm_led_ready)
        return;
    if (timeDue_(now, _alarm_next_toggle_ms))
    {
        _alarm_next_toggle_ms = now + _alarm_blink_ms;
        setAlarmLed_(!_alarm_led_state);
    }
}

bool PlcControl::timeDue_(uint32_t now, uint32_t at){
    return (int32_t)(now - at) >= 0;
}
