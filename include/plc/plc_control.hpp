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
#include <stdint.h>
#include <math.h>

#include "boards/board_profile.hpp"
#include "core/rtc.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/io_stack.hpp"
#include "hal/lm75ad.hpp"

class PlcControl
{
public:
    enum class AlarmModule : uint8_t
    {
        Sockets = 0,
        Lights,
        Meteo,
        Thermo,
        Tanks,
        Septic,
        Security,
        Ring
    };
    static constexpr size_t kAlarmModuleCount = 8;

    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        InvalidConfig,
        I2c
    };

    PlcControl(I2CManager &i2c, IoStack &io, RTC &rtc)
        : _i2c(i2c), _io(io), _rtc(rtc), _device_name(ActiveBoardProfile::UI_NAME)
    {
    }

    bool begin()
    {
        const auto cfg = ActiveBoardProfile::BOARD_TEMP;
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

        if (!_lm75.begin(*wire, cfg.addr))
        {
            _err = Error::I2c;
            return false;
        }

        _fan_on_c = cfg.fan_on_c;
        _fan_hyst_c = cfg.hysteresis_c;
        _err = Error::Ok;
        return true;
    }

    void task()
    {
        const uint32_t now = millis();
        if (timeDue_(now, _next_sample_ms))
        {
            _next_sample_ms = now + _sample_interval_ms;
            _last_cpu_temp_c = readCpuTemp_();
            _cpu_temp_valid = isfinite(_last_cpu_temp_c);
            const auto cfg = ActiveBoardProfile::BOARD_TEMP;
            float sample = 0.0f;
            if (_lm75.readTempC(sample))
            {
                _last_temp_c = sample;
                _temp_valid = true;
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
        if (_cpu_temp_valid && (!has_temp || _last_cpu_temp_c > temp_c))
        {
            temp_c = _last_cpu_temp_c;
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

    float boardTemp() const { return _last_temp_c; }
    float cpuTemp() const { return _last_cpu_temp_c; }
    bool fanStatus() const { return _fan_on; }
    const String &deviceName() const { return _device_name; }
    void setDeviceName(const String &name) { _device_name = name; }
    bool buzzerEnabled() const { return _buzzer_enabled; }
    void setBuzzerEnabled(bool enabled) { _buzzer_enabled = enabled; }
    bool fanManualMode() const { return _fan_manual; }
    float fanOnC() const { return _fan_on_c; }
    float fanHysteresisC() const { return _fan_hyst_c; }

    bool portState(uint8_t id, bool &out) const
    {
        if (id >= IoStack::PORT_COUNT)
            return false;
        const auto &p = _io.desc(id);
        if (p.caps == Cap::None)
            return false;
        out = _io.read(id);
        return true;
    }

    void setFanAuto() { _fan_manual = false; }

    void setFanManual(bool on)
    {
        _fan_manual = true;
        _fan_on = on;
        setFans_(_fan_on);
    }

    void setFanThresholds(float on_c, float hyst_c)
    {
        _fan_on_c = on_c;
        _fan_hyst_c = hyst_c;
    }

    void setAlarmMask(uint32_t mask) { _alarm_mask = mask; }
    uint32_t alarmMask() const { return _alarm_mask; }
    void setAlarmUnitMask(AlarmModule module, uint32_t mask)
    {
        const uint8_t idx = static_cast<uint8_t>(module);
        if (idx >= kAlarmModuleCount)
            return;
        _alarm_unit_mask[idx] = mask;
        setAlarmModule_(idx, (_alarm_detail_mask[idx] | _alarm_unit_mask[idx]) != 0);
    }
    uint32_t alarmUnitMask(AlarmModule module) const
    {
        const uint8_t idx = static_cast<uint8_t>(module);
        if (idx >= kAlarmModuleCount)
            return 0;
        return _alarm_unit_mask[idx];
    }
    void setAlarmDetailMask(AlarmModule module, uint32_t mask)
    {
        const uint8_t idx = static_cast<uint8_t>(module);
        if (idx >= kAlarmModuleCount)
            return;
        _alarm_detail_mask[idx] = mask;
        setAlarmModule_(idx, (_alarm_detail_mask[idx] | _alarm_unit_mask[idx]) != 0);
    }
    uint32_t alarmDetailMask(AlarmModule module) const
    {
        const uint8_t idx = static_cast<uint8_t>(module);
        if (idx >= kAlarmModuleCount)
            return 0;
        return _alarm_detail_mask[idx];
    }

    void setAlarmModule(AlarmModule module, bool has_alarm)
    {
        setAlarmModule_(static_cast<uint8_t>(module), has_alarm);
    }

    void setAlarm(AlarmModule module) { setAlarmModule(module, true); }
    void clearAlarm(AlarmModule module) { setAlarmModule(module, false); }

    void setAlarmModule(uint8_t module, bool has_alarm)
    {
        setAlarmModule_(module, has_alarm);
    }

    void setAlarmDetail(AlarmModule module, uint8_t bit, bool has_alarm)
    {
        setAlarmDetail_(module, bit, has_alarm);
    }

    void setAlarmDetailBit(AlarmModule module, uint8_t bit) { setAlarmDetail(module, bit, true); }
    void clearAlarmDetailBit(AlarmModule module, uint8_t bit) { setAlarmDetail(module, bit, false); }
    void setAlarmUnit(AlarmModule module, uint8_t unit_bit, bool has_alarm)
    {
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

    Error lastError() const { return _err; }

private:
    static constexpr bool busExists_(uint8_t bus_num)
    {
        for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
        {
            if (ActiveBoardProfile::I2CS[i].bus_num == bus_num)
                return true;
        }
        return false;
    }

    void setFans_(bool on)
    {
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

    static float readCpuTemp_()
    {
#if defined(ESP32)
        const uint32_t now = millis();
        static uint32_t last_read_ms = 0;
        static float last_value = 0.0f;
        if (timeDue_(now, last_read_ms + 5000u))
        {
            last_read_ms = now;
            last_value = temperatureRead();
        }
        return last_value;
#else
        return 0.0f;
#endif
    }

    void setAlarmModule_(uint8_t module, bool has_alarm)
    {
        if (module >= 32)
            return;
        const uint32_t bit = 1u << module;
        if (has_alarm)
            _alarm_mask |= bit;
        else
            _alarm_mask &= ~bit;
    }

    void setAlarmDetail_(AlarmModule module, uint8_t bit_index, bool has_alarm)
    {
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

    void ensureAlarmLed_()
    {
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

    void setAlarmLed_(bool on)
    {
        const uint8_t pin = ActiveBoardProfile::ALARM_LED_PIN;
        if (pin == 0xFF)
            return;
        _io.write(pin, on);
        _alarm_led_state = on;
    }

    void updateAlarmLed_(uint32_t now)
    {
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

    Lm75ad _lm75;
    I2CManager &_i2c;
    IoStack &_io;
    RTC &_rtc;
    Error _err = Error::Ok;
    bool _fan_on = false;
    bool _fan_manual = false;
    float _fan_on_c = 0.0f;
    float _fan_hyst_c = 0.0f;
    float _last_temp_c = 0.0f;
    float _last_cpu_temp_c = 0.0f;
    float _last_rtc_temp_c = 0.0f;
    bool _temp_valid = false;
    bool _cpu_temp_valid = false;
    bool _rtc_temp_valid = false;
    String _device_name;
    uint32_t _sample_interval_ms = 1000;
    uint32_t _next_sample_ms = 0;
    uint32_t _alarm_mask = 0;
    uint32_t _alarm_detail_mask[kAlarmModuleCount] = {};
    uint32_t _alarm_unit_mask[kAlarmModuleCount] = {};
    bool _alarm_led_ready = false;
    bool _alarm_led_state = false;
    uint32_t _alarm_blink_ms = 500;
    uint32_t _alarm_next_toggle_ms = 0;
    bool _buzzer_enabled = true;


    static bool timeDue_(uint32_t now, uint32_t at)
    {
        return (int32_t)(now - at) >= 0;
    }
};
