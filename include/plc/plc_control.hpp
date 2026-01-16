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

#include "boards/board_profile.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/io_stack.hpp"
#include "hal/lm75ad.hpp"

class PlcControl
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        InvalidConfig,
        I2c
    };

    PlcControl(I2CManager &i2c, IoStack &io)
        : _i2c(i2c), _io(io), _device_name(ActiveBoardProfile::UI_NAME)
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
        float temp_c = _last_temp_c;
        if (timeDue_(now, _next_sample_ms))
        {
            _next_sample_ms = now + _sample_interval_ms;
            _last_cpu_temp_c = readCpuTemp_();
            const auto cfg = ActiveBoardProfile::BOARD_TEMP;
            float sample = 0.0f;
            if (_lm75.readTempC(sample))
            {
                _last_temp_c = sample;
                temp_c = sample;
                _temp_valid = true;
            }
        }

        if (_fan_manual)
        {
            setFans_(_fan_on);
            return;
        }
        if (!_temp_valid)
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
    }

    float boardTemp() const { return _last_temp_c; }
    float cpuTemp() const { return _last_cpu_temp_c; }
    bool fanStatus() const { return _fan_on; }
    const String &deviceName() const { return _device_name; }
    void setDeviceName(const String &name) { _device_name = name; }
    bool fanManualMode() const { return _fan_manual; }
    float fanOnC() const { return _fan_on_c; }
    float fanHysteresisC() const { return _fan_hyst_c; }

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
        return temperatureRead();
#else
        return 0.0f;
#endif
    }

    Lm75ad _lm75;
    I2CManager &_i2c;
    IoStack &_io;
    Error _err = Error::Ok;
    bool _fan_on = false;
    bool _fan_manual = false;
    float _fan_on_c = 0.0f;
    float _fan_hyst_c = 0.0f;
    float _last_temp_c = 0.0f;
    float _last_cpu_temp_c = 0.0f;
    bool _temp_valid = false;
    String _device_name;
    uint32_t _sample_interval_ms = 1000;
    uint32_t _next_sample_ms = 0;

    static bool timeDue_(uint32_t now, uint32_t at)
    {
        return (int32_t)(now - at) >= 0;
    }
};
