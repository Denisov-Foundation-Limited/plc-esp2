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
#include <OneWire.h>
#include <stdint.h>

#include "boards/board_profile.hpp"
#include "boards/board_profile_base.hpp"
#include "hal/gpio/portio.hpp"

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

    bool beginAll()
    {
        _err = Error::Ok;
        _count = 0;

        if (ActiveBoardProfile::ONEWIRE_COUNT == 0)
            return true;
        if (ActiveBoardProfile::ONEWIRE_COUNT > MAX_BUSES)
        {
            _err = Error::TooMany;
            return false;
        }

        for (uint8_t i = 0; i < ActiveBoardProfile::ONEWIRE_COUNT; ++i)
        {
            const OneWireCfg &c = ActiveBoardProfile::ONEWIRES[i];
            uint8_t gpio = 0;
            if (!oneWirePinFromPort_(c.pin, gpio))
            {
                _err = Error::InvalidPin;
                return false;
            }
            _cfg[i] = c;
            _bus[i] = OneWire(gpio);
        }
        _count = ActiveBoardProfile::ONEWIRE_COUNT;
        return true;
    }

    uint8_t count() const { return _count; }

    OneWire *busPtrByIndex(uint8_t idx)
    {
        if (idx >= _count)
        {
            _err = Error::InvalidIndex;
            return nullptr;
        }
        return &_bus[idx];
    }

    OneWire *busPtrById(OwBusType bus_id)
    {
        for (uint8_t i = 0; i < _count; ++i)
        {
            if (_cfg[i].bus_id == static_cast<OneWireCfg::OwType>(bus_id))
                return &_bus[i];
        }
        _err = Error::InvalidIndex;
        return nullptr;
    }

    Error lastError() const { return _err; }

private:
    OneWire _bus[MAX_BUSES] = {OneWire(0), OneWire(0), OneWire(0), OneWire(0)};
    OneWireCfg _cfg[MAX_BUSES] = {};
    uint8_t _count = 0;
    Error _err = Error::Ok;

    static bool oneWirePinFromPort_(int8_t port, uint8_t &out_gpio)
    {
        if (port < 0 || port >= PortIO::PORT_COUNT)
            return false;
        const auto &p = ActiveBoardProfile::PORTS[(uint8_t)port];
        if (p.backend != PortIO::Backend::Esp32)
            return false;
        if (p.u.esp.gpio == 0xFF)
            return false;
        out_gpio = p.u.esp.gpio;
        return true;
    }
};
