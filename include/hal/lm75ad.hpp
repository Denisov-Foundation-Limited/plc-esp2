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
#include <Wire.h>
#include <stdint.h>

class Lm75ad
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        I2c
    };

    static constexpr uint8_t kDefaultAddr = 0x48;

    Lm75ad() = default;
    explicit Lm75ad(TwoWire &wire) : _wire(&wire) {}

    bool begin(TwoWire &wire, uint8_t addr = kDefaultAddr)
    {
        _wire = &wire;
        _addr = addr;
        _err = _wire ? Error::Ok : Error::NoBus;
        return _wire != nullptr;
    }

    bool readTempC(float &out_c)
    {
        uint8_t buf[2] = {};
        if (!readRegs_(0x00, buf, sizeof(buf)))
            return false;

        const int8_t msb = (int8_t)buf[0];
        const uint8_t lsb = buf[1] >> 5;
        out_c = (float)msb + (lsb * 0.125f);
        return true;
    }

    Error lastError() const { return _err; }

private:
    bool readRegs_(uint8_t reg, uint8_t *buf, uint8_t len)
    {
        if (!_wire)
        {
            _err = Error::NoBus;
            return false;
        }

        _wire->beginTransmission(_addr);
        _wire->write(reg);
        if (_wire->endTransmission(false) != 0)
        {
            _err = Error::I2c;
            return false;
        }

        const uint8_t got = _wire->requestFrom(_addr, len);
        if (got != len)
        {
            _err = Error::I2c;
            return false;
        }

        for (uint8_t i = 0; i < len; ++i)
            buf[i] = _wire->read();

        _err = Error::Ok;
        return true;
    }

    TwoWire *_wire = nullptr;
    uint8_t _addr = kDefaultAddr;
    Error _err = Error::Ok;
};
