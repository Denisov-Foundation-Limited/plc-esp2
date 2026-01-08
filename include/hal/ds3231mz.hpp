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

#include "hal/bus/i2c.hpp"

class Ds3231Mz
{
public:
    struct DateTime
    {
        uint16_t year; // 2000+
        uint8_t month;
        uint8_t day;
        uint8_t day_of_week; // 1..7
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
    };

    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        I2c
    };

    explicit Ds3231Mz(I2CManager &i2c) : _i2c(i2c)
    {
    }

    bool begin(uint8_t bus_num = 0, uint8_t addr = 0x68)
    {
        _wire = _i2c.wirePtr(bus_num);
        _addr = addr;
        if (!_wire)
            _err = Error::NoBus;
        _err = _wire ? Error::Ok : Error::NoBus;
        return _wire != nullptr;
    }

    bool read(DateTime &out)
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

    bool set(const DateTime &dt)
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

    bool readTempC(float &out_c)
    {
        uint8_t buf[2] = {};
        if (!readRegs_(0x11, buf, sizeof(buf)))
            return false;

        int8_t msb = (int8_t)buf[0];
        uint8_t lsb = buf[1] >> 6;
        out_c = (float)msb + (lsb * 0.25f);
        return true;
    }

    Error lastError() const { return _err; }

private:
    static uint8_t bcdToDec_(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
    static uint8_t decToBcd_(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

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

    bool writeRegs_(uint8_t reg, const uint8_t *buf, uint8_t len)
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

    TwoWire *_wire = nullptr;
    uint8_t _addr = 0x68;
    Error _err = Error::Ok;
    I2CManager &_i2c;
};
