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
    int8_t busIndexById(OwBusType bus_id) const;
    bool lockBus(uint8_t bus_idx, uint32_t timeout_ms = 50);
    void unlockBus(uint8_t bus_idx);
    bool lockBusById(OwBusType bus_id, uint32_t timeout_ms = 50);
    void unlockBusById(OwBusType bus_id);
    Error lastError() const;

    class ScopedBusLock
    {
    public:
        ScopedBusLock(OneWireManager &mgr, uint8_t bus_idx, uint32_t timeout_ms = 50);
        ScopedBusLock(OneWireManager &mgr, OwBusType bus_id, uint32_t timeout_ms = 50);
        ~ScopedBusLock();
        ScopedBusLock(const ScopedBusLock &) = delete;
        ScopedBusLock &operator=(const ScopedBusLock &) = delete;
        bool locked() const { return _locked; }

    private:
        OneWireManager *_mgr = nullptr;
        int8_t _bus_idx = -1;
        bool _locked = false;
    };

private:
    OneWireBus _bus[MAX_BUSES] = {};
    OneWireCfg _cfg[MAX_BUSES] = {};
    uint8_t _count = 0;
    Error _err = Error::Ok;

    static bool oneWirePinFromPort_(int8_t port, uint8_t &out_gpio);
#if defined(ESP32)
    void *_bus_mtx_[MAX_BUSES] = {};
#endif
};
