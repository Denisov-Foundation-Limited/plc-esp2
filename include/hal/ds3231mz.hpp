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

class TwoWire;

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

    Ds3231Mz() = default;
    explicit Ds3231Mz(TwoWire &wire);

    bool begin(TwoWire &wire, uint8_t addr = 0x68);
    bool read(DateTime &out);
    bool set(const DateTime &dt);
    bool readTempC(float &out_c);
    Error lastError() const;

private:
    static uint8_t bcdToDec_(uint8_t v);
    static uint8_t decToBcd_(uint8_t v);
    bool readRegs_(uint8_t reg, uint8_t *buf, uint8_t len);
    bool writeRegs_(uint8_t reg, const uint8_t *buf, uint8_t len);

    TwoWire *_wire = nullptr;
    uint8_t _addr = 0x68;
    Error _err = Error::Ok;
};
