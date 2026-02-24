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

#include "hal/bus/onewire.hpp"
#include "boards/board_profile.hpp"
#include "hal/gpio/portio.hpp"

bool OneWireManager::beginAll()
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
        _bus[i].begin(gpio);
    }
    _count = ActiveBoardProfile::ONEWIRE_COUNT;
    return true;
}

uint8_t OneWireManager::count() const
{
    return _count;
}

OneWireBus *OneWireManager::busPtrByIndex(uint8_t idx)
{
    if (idx >= _count)
    {
        _err = Error::InvalidIndex;
        return nullptr;
    }
    return &_bus[idx];
}

OneWireBus *OneWireManager::busPtrById(OwBusType bus_id)
{
    for (uint8_t i = 0; i < _count; ++i)
    {
        if (_cfg[i].bus_id == static_cast<OneWireCfg::OwType>(bus_id))
            return &_bus[i];
    }
    _err = Error::InvalidIndex;
    return nullptr;
}

OneWireManager::Error OneWireManager::lastError() const
{
    return _err;
}

bool OneWireManager::oneWirePinFromPort_(int8_t port, uint8_t &out_gpio)
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
