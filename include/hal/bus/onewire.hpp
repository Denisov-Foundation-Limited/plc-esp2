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
#include "hal/bus/one_wire_bus.hpp"
#include <stdint.h>

#include "boards/board_profile_base.hpp"

class OneWireManager
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        InvalidIndex,
        InvalidPin,
        TooMany
    };

    enum class OwBusType : uint8_t
    {
        iButton = 0,
        Temp
    };

    static constexpr uint8_t MAX_BUSES = 4;

    bool beginAll();
    uint8_t count() const;
    OneWireBus *busPtrByIndex(uint8_t idx);
    OneWireBus *busPtrById(OwBusType bus_id);
    Error lastError() const;

private:
    OneWireBus _bus[MAX_BUSES] = {};
    OneWireCfg _cfg[MAX_BUSES] = {};
    uint8_t _count = 0;
    Error _err = Error::Ok;

    static bool oneWirePinFromPort_(int8_t port, uint8_t &out_gpio);
};
