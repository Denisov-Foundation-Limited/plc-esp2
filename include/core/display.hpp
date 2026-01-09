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

#include <stdint.h>

#include "boards/board_profile.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/lcd1602_i2c.hpp"

class Display
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        InvalidConfig,
        I2c
    };

    Display(I2CManager &i2c, Lcd1602I2c &lcd) : _i2c(i2c), _lcd(lcd) {}

    bool begin()
    {
        const uint8_t bus_num = ActiveBoardProfile::LCD.bus_num;
        const uint8_t addr = ActiveBoardProfile::LCD.addr;
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

        if (!_lcd.begin(*wire, addr))
        {
            _err = Error::I2c;
            return false;
        }

        _err = Error::Ok;
        return true;
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

    I2CManager &_i2c;
    Lcd1602I2c &_lcd;
    Error _err = Error::Ok;
};
