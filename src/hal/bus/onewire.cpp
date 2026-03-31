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

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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
        if (_bus_mtx_[i] == nullptr)
            _bus_mtx_[i] = (void *)xSemaphoreCreateRecursiveMutex();
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

int8_t OneWireManager::busIndexById(OwBusType bus_id) const
{
    for (uint8_t i = 0; i < _count; ++i)
    {
        if (_cfg[i].bus_id == static_cast<OneWireCfg::OwType>(bus_id))
            return (int8_t)i;
    }
    return -1;
}

bool OneWireManager::lockBus(uint8_t bus_idx, uint32_t timeout_ms)
{
    if (bus_idx >= _count)
    {
        _err = Error::InvalidIndex;
        return false;
    }
    SemaphoreHandle_t mtx = (SemaphoreHandle_t)_bus_mtx_[bus_idx];
    if (mtx == nullptr)
    {
        _bus_mtx_[bus_idx] = (void *)xSemaphoreCreateRecursiveMutex();
        mtx = (SemaphoreHandle_t)_bus_mtx_[bus_idx];
        if (mtx == nullptr)
            return false;
    }
    if (xSemaphoreTakeRecursive(mtx, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
        return false;
    return true;
}

void OneWireManager::unlockBus(uint8_t bus_idx)
{
    if (bus_idx >= _count)
        return;
    SemaphoreHandle_t mtx = (SemaphoreHandle_t)_bus_mtx_[bus_idx];
    if (mtx == nullptr)
        return;
    xSemaphoreGiveRecursive(mtx);
}

bool OneWireManager::lockBusById(OwBusType bus_id, uint32_t timeout_ms)
{
    const int8_t idx = busIndexById(bus_id);
    if (idx < 0)
    {
        _err = Error::InvalidIndex;
        return false;
    }
    return lockBus((uint8_t)idx, timeout_ms);
}

void OneWireManager::unlockBusById(OwBusType bus_id)
{
    const int8_t idx = busIndexById(bus_id);
    if (idx < 0)
        return;
    unlockBus((uint8_t)idx);
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

OneWireManager::ScopedBusLock::ScopedBusLock(OneWireManager &mgr, uint8_t bus_idx, uint32_t timeout_ms)
    : _mgr(&mgr), _bus_idx((int8_t)bus_idx)
{
    _locked = _mgr->lockBus(bus_idx, timeout_ms);
}

OneWireManager::ScopedBusLock::ScopedBusLock(OneWireManager &mgr, OwBusType bus_id, uint32_t timeout_ms)
    : _mgr(&mgr), _bus_idx(mgr.busIndexById(bus_id))
{
    if (_bus_idx >= 0)
        _locked = _mgr->lockBus((uint8_t)_bus_idx, timeout_ms);
}

OneWireManager::ScopedBusLock::~ScopedBusLock()
{
    if (_mgr && _locked && _bus_idx >= 0)
        _mgr->unlockBus((uint8_t)_bus_idx);
}
