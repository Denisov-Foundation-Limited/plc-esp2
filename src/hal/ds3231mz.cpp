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

#include "hal/ds3231mz.hpp"
#include <Wire.h>

Ds3231Mz::Ds3231Mz(TwoWire &wire)
    : _wire(&wire)
{
}

bool Ds3231Mz::begin(TwoWire &wire, uint8_t addr)
{
    _wire = &wire;
    _addr = addr;
    _err = _wire ? Error::Ok : Error::NoBus;
    return _wire != nullptr;
}

bool Ds3231Mz::read(DateTime &out)
{
    uint8_t buf[7] = {};
    if (!readRegs_(0x00, buf, sizeof(buf)))
        return false;

    out.second = bcdToDec_(buf[0] & 0x7F);
    out.minute = bcdToDec_(buf[1] & 0x7F);
    out.hour = bcdToDec_(buf[2] & 0x3F);
    out.day_of_week = bcdToDec_(buf[3] & 0x07);
    out.day = bcdToDec_(buf[4] & 0x3F);
    out.month = bcdToDec_(buf[5] & 0x1F);
    out.year = 2000 + bcdToDec_(buf[6]);
    return true;
}

bool Ds3231Mz::set(const DateTime &dt)
{
    uint8_t buf[7] = {};
    buf[0] = decToBcd_(dt.second);
    buf[1] = decToBcd_(dt.minute);
    buf[2] = decToBcd_(dt.hour);
    buf[3] = decToBcd_(dt.day_of_week);
    buf[4] = decToBcd_(dt.day);
    buf[5] = decToBcd_(dt.month);
    buf[6] = decToBcd_((uint8_t)(dt.year % 100));
    return writeRegs_(0x00, buf, sizeof(buf));
}

bool Ds3231Mz::readTempC(float &out_c)
{
    uint8_t buf[2] = {};
    if (!readRegs_(0x11, buf, sizeof(buf)))
        return false;

    int8_t msb = (int8_t)buf[0];
    uint8_t lsb = buf[1] >> 6;
    out_c = (float)msb + (lsb * 0.25f);
    return true;
}

Ds3231Mz::Error Ds3231Mz::lastError() const
{
    return _err;
}

uint8_t Ds3231Mz::bcdToDec_(uint8_t v)
{
    return (uint8_t)((v >> 4) * 10 + (v & 0x0F));
}

uint8_t Ds3231Mz::decToBcd_(uint8_t v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

bool Ds3231Mz::readRegs_(uint8_t reg, uint8_t *buf, uint8_t len)
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

bool Ds3231Mz::writeRegs_(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    if (!_wire)
    {
        _err = Error::NoBus;
        return false;
    }

    _wire->beginTransmission(_addr);
    _wire->write(reg);
    for (uint8_t i = 0; i < len; ++i)
        _wire->write(buf[i]);
    if (_wire->endTransmission() != 0)
    {
        _err = Error::I2c;
        return false;
    }

    _err = Error::Ok;
    return true;
}
