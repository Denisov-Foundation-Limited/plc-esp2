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

class I2CManager
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        InvalidBus,
        InvalidPins
    };

    bool beginAll();
    TwoWire *wirePtr(uint8_t bus_num);
    TwoWire &wire(uint8_t bus_num);
    Error lastError() const;
    bool scanDevices(uint8_t bus_num, bool present[127]);
    bool probeAddress(uint8_t bus_num, uint8_t addr);
    bool scanDevicesLocked(uint8_t bus_num, bool present[127]);
    bool probeAddressLocked(uint8_t bus_num, uint8_t addr);
    bool lockBus(uint8_t bus_num, uint32_t timeout_ms = 50);
    void unlockBus(uint8_t bus_num);
    bool busLockHeldByCurrentTask(uint8_t bus_num) const;

    class ScopedBusLock
    {
    public:
        ScopedBusLock(I2CManager &mgr, uint8_t bus_num, uint32_t timeout_ms = 50);
        ~ScopedBusLock();
        ScopedBusLock(const ScopedBusLock &) = delete;
        ScopedBusLock &operator=(const ScopedBusLock &) = delete;
        bool locked() const { return _locked; }

    private:
        I2CManager *_mgr = nullptr;
        uint8_t _bus_num = 0;
        bool _locked = false;
    };

private:
    Error _err = Error::Ok;

    static bool i2cPinsFromPorts_(uint8_t sda_port, uint8_t scl_port,
                                  uint8_t &out_sda_gpio, uint8_t &out_scl_gpio);
    static TwoWire *wirePtr_(uint8_t bus_num);
    static int8_t busIdx_(uint8_t bus_num);
    static bool busPins_(uint8_t bus_num, uint8_t &out_sda_gpio, uint8_t &out_scl_gpio);
    static void recoverBus_(uint8_t sda_gpio, uint8_t scl_gpio);
    static void recoverBus_(uint8_t bus_num);
#if defined(ESP32)
    void ensureBusMutex_(uint8_t bus_num);
    void *currentTaskToken_() const;
    void *_bus_owner_[3] = {nullptr, nullptr, nullptr};
    void *_bus_mtx_[3] = {nullptr, nullptr, nullptr};
#endif
};
