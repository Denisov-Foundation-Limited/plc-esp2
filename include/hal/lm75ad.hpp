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

class Lm75ad
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        I2c
    };

    static constexpr uint8_t kDefaultAddr = 0x48;

    Lm75ad() = default;
    explicit Lm75ad(TwoWire &wire);

    bool begin(TwoWire &wire, uint8_t addr = kDefaultAddr);
    bool readTempC(float &out_c);
    Error lastError() const;

private:
    bool readRegs_(uint8_t reg, uint8_t *buf, uint8_t len);

    TwoWire *_wire = nullptr;
    uint8_t _addr = kDefaultAddr;
    Error _err = Error::Ok;
};
