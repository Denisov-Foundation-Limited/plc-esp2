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
    bool begin(TwoWire &wire, uint8_t addr = 0x20);

    Error lastError() const;
    bool ready() const;

    bool pinMode(uint8_t pin, uint8_t mode);
    bool writePin(uint8_t pin, bool level);
    bool readPin(uint8_t pin, bool &out) const;
    bool writePort(uint8_t value);
    bool readPort(uint8_t &out) const;
    bool flush();

private:
    TwoWire *_wire = nullptr;
    uint8_t _addr = 0x20;
    mutable Error _err = Error::Ok;

    uint8_t _out = 0xFF;
    bool _dirty = false;

    bool writeOut_();
};
