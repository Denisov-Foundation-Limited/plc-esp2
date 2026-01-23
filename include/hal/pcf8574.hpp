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

class TwoWire;

class Pcf8574
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        I2c,
        InvalidPin
    };

    Pcf8574() = default;

    bool begin(TwoWire &wire, uint8_t addr = 0x20)
    {
        _wire = &wire;
        _addr = addr;
        _err = Error::Ok;
        _out = 0xFF;
        _dirty = true;
        return flush();
    }

    Error lastError() const { return _err; }
    bool ready() const { return _wire != nullptr; }

    bool pinMode(uint8_t pin, uint8_t mode)
    {
        if (pin > 7)
        {
            _err = Error::InvalidPin;
            return false;
        }

        const uint8_t mask = (uint8_t)(1u << pin);
        if (mode == OUTPUT || mode == OUTPUT_OPEN_DRAIN)
        {
            _out &= (uint8_t)~mask;
        }
        else
        {
            _out |= mask; // release for input/pull-up
        }
        _dirty = true;
        return true;
    }

    bool writePin(uint8_t pin, bool level)
    {
        if (pin > 7)
        {
            _err = Error::InvalidPin;
            return false;
        }
        const uint8_t mask = (uint8_t)(1u << pin);
        if (level)
            _out |= mask;
        else
            _out &= (uint8_t)~mask;
        _dirty = true;
        return true;
    }

    bool readPin(uint8_t pin, bool &out) const
    {
        if (pin > 7)
            return false;
        uint8_t v = 0;
        if (!readPort(v))
            return false;
        out = (v & (uint8_t)(1u << pin)) != 0;
        return true;
    }

    bool writePort(uint8_t value)
    {
        _out = value;
        _dirty = true;
        return true;
    }

    bool readPort(uint8_t &out) const
    {
        if (!_wire)
        {
            _err = Error::NoBus;
            return false;
        }
        const uint8_t got = _wire->requestFrom(_addr, (uint8_t)1);
        if (got != 1)
        {
            _err = Error::I2c;
            return false;
        }
        out = _wire->read();
        _err = Error::Ok;
        return true;
    }

    bool flush()
    {
        if (!_dirty)
            return true;
        return writeOut_();
    }

private:
    TwoWire *_wire = nullptr;
    uint8_t _addr = 0x20;
    mutable Error _err = Error::Ok;

    uint8_t _out = 0xFF;
    bool _dirty = false;

    bool writeOut_()
    {
        if (!_wire)
        {
            _err = Error::NoBus;
            return false;
        }
        _wire->beginTransmission(_addr);
        _wire->write(_out);
        if (_wire->endTransmission() != 0)
        {
            _err = Error::I2c;
            return false;
        }
        _dirty = false;
        _err = Error::Ok;
        return true;
    }
};
