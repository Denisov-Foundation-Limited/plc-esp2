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

#include "hal/lm75ad.hpp"
#include <Wire.h>

Lm75ad::Lm75ad(TwoWire &wire)
    : _wire(&wire)
{
}

bool Lm75ad::begin(TwoWire &wire, uint8_t addr)
{
    _wire = &wire;
    _addr = addr;
    _err = _wire ? Error::Ok : Error::NoBus;
    return _wire != nullptr;
}

bool Lm75ad::readTempC(float &out_c)
{
    uint8_t buf[2] = {};
    if (!readRegs_(0x00, buf, sizeof(buf)))
        return false;

    const int8_t msb = (int8_t)buf[0];
    const uint8_t lsb = buf[1] >> 5;
    out_c = (float)msb + (lsb * 0.125f);
    return true;
}

Lm75ad::Error Lm75ad::lastError() const
{
    return _err;
}

bool Lm75ad::readRegs_(uint8_t reg, uint8_t *buf, uint8_t len)
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
