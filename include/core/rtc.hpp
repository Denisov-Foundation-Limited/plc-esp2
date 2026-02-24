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

#include "hal/ds3231mz.hpp"

class I2CManager;

class RTC
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        InvalidConfig,
        I2c
    };

    RTC(I2CManager &i2c, Ds3231Mz &rtc);

    bool begin();
    bool setTime(const Ds3231Mz::DateTime &dt);
    bool Time(Ds3231Mz::DateTime &out);
    bool readTemp(float &out_c);
    Error lastError() const;

private:
    static constexpr bool busExists_(uint8_t bus_num);

    Ds3231Mz &_rtc;
    I2CManager &_i2c;
    Error _err = Error::Ok;
};
