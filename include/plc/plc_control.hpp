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
#include "hal/gpio/portio.hpp"
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

    PlcControl(I2CManager &i2c, PortIO &portio)
        : _i2c(i2c), _portio(portio)
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

        _err = Error::Ok;
        return true;
    }

    void task()
    {
        _last_cpu_temp_c = readCpuTemp_();
        const auto cfg = ActiveBoardProfile::BOARD_TEMP;
        float temp_c = 0.0f;
        if (!_lm75.readTempC(temp_c))
            return;
        _last_temp_c = temp_c;

        bool want = _fan_on;
        const float on_c = cfg.fan_on_c;
        const float off_c = on_c - cfg.hysteresis_c;

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
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = _portio.desc(i);
            if (p.caps == Cap::None)
                continue;
            if (p.type != PortIO::PinType::Fan)
                continue;
            if (!has(p.caps, Cap::Output))
                continue;
            _portio.write(i, on);
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
    PortIO &_portio;
    Error _err = Error::Ok;
    bool _fan_on = false;
    float _last_temp_c = 0.0f;
    float _last_cpu_temp_c = 0.0f;
};
