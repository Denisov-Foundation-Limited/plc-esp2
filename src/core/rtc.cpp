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

#include "core/rtc.hpp"

#include "boards/board_profile.hpp"
#include "hal/bus/i2c.hpp"

RTC::RTC(I2CManager &i2c, Ds3231Mz &rtc) : _i2c(i2c), _rtc(rtc) {}

bool RTC::begin()
{
    const uint8_t bus_num = ActiveBoardProfile::RTC.bus_num;
    const uint8_t addr = ActiveBoardProfile::RTC.addr;
    if (!busExists_(bus_num))
    {
        _err = Error::InvalidConfig;
        return false;
    }

    TwoWire *wire = _i2c.wirePtr(bus_num);
    if (!wire)
    {
        _err = Error::NoBus;
        return false;
    }

    if (!_rtc.begin(*wire, addr))
    {
        _err = Error::I2c;
        return false;
    }

    _err = Error::Ok;
    return true;
}

bool RTC::setTime(const Ds3231Mz::DateTime &dt)
{
    if (!_rtc.set(dt))
    {
        _err = Error::I2c;
        return false;
    }
    _err = Error::Ok;
    return true;
}

bool RTC::Time(Ds3231Mz::DateTime &out)
{
    if (!_rtc.read(out))
    {
        _err = Error::I2c;
        return false;
    }
    _err = Error::Ok;
    return true;
}

bool RTC::readTemp(float &out_c)
{
    if (!_rtc.readTempC(out_c))
    {
        _err = Error::I2c;
        return false;
    }
    _err = Error::Ok;
    return true;
}

RTC::Error RTC::lastError() const
{
    return _err;
}

constexpr bool RTC::busExists_(uint8_t bus_num)
{
    for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
    {
        if (ActiveBoardProfile::I2CS[i].bus_num == bus_num)
            return true;
    }
    return false;
}
