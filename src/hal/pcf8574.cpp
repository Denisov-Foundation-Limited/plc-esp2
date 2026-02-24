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

#include "hal/pcf8574.hpp"
#include <Arduino.h>
#include <Wire.h>

bool Pcf8574::begin(TwoWire &wire, uint8_t addr)
{
    _wire = &wire;
    _addr = addr;
    _err = Error::Ok;
    _out = 0xFF;
    _dirty = true;
    return flush();
}

Pcf8574::Error Pcf8574::lastError() const
{
    return _err;
}

bool Pcf8574::ready() const
{
    return _wire != nullptr;
}

bool Pcf8574::pinMode(uint8_t pin, uint8_t mode)
{
    if (pin > 7)
    {
        _err = Error::InvalidPin;
        return false;
    }

    const uint8_t mask = (uint8_t)(1u << pin);
    if (mode == OUTPUT || mode == OUTPUT_OPEN_DRAIN)
        _out &= (uint8_t)~mask;
    else
        _out |= mask;
    _dirty = true;
    return true;
}

bool Pcf8574::writePin(uint8_t pin, bool level)
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

bool Pcf8574::readPin(uint8_t pin, bool &out) const
{
    if (pin > 7)
        return false;
    uint8_t v = 0;
    if (!readPort(v))
        return false;
    out = (v & (uint8_t)(1u << pin)) != 0;
    return true;
}

bool Pcf8574::writePort(uint8_t value)
{
    _out = value;
    _dirty = true;
    return true;
}

bool Pcf8574::readPort(uint8_t &out) const
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

bool Pcf8574::flush()
{
    if (!_dirty)
        return true;
    return writeOut_();
}

bool Pcf8574::writeOut_()
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
