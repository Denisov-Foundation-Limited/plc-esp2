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

#include "hal/bus/i2c.hpp"
#include <Arduino.h>
#include <Wire.h>
#include "boards/board_profile.hpp"
#include "hal/gpio/portio.hpp"
#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

#ifndef I2C_LOCK_TIMEOUT_MS
#define I2C_LOCK_TIMEOUT_MS 50
#endif

#ifndef I2C_LOCK_DEBUG
#define I2C_LOCK_DEBUG 0
#endif

#ifndef I2C_LOCK_WARN_WAIT_US
#define I2C_LOCK_WARN_WAIT_US 5000UL
#endif

bool I2CManager::beginAll()
{
    _err = Error::Ok;
    for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
    {
        const auto c = ActiveBoardProfile::I2CS[i];
        TwoWire *w = wirePtr_(c.bus_num);
        if (!w)
        {
            _err = Error::InvalidBus;
            return false;
        }

#if defined(ESP32)
        uint8_t sda_gpio = 0;
        uint8_t scl_gpio = 0;
        if (!i2cPinsFromPorts_(c.sda, c.scl, sda_gpio, scl_gpio))
        {
            _err = Error::InvalidPins;
            return false;
        }
        w->begin(sda_gpio, scl_gpio, c.freq);
#else
        w->begin();
        w->setClock(c.freq);
#endif
    }
    return true;
}

TwoWire *I2CManager::wirePtr(uint8_t bus_num)
{
    return wirePtr_(bus_num);
}

TwoWire &I2CManager::wire(uint8_t bus_num)
{
    TwoWire *w = wirePtr_(bus_num);
    if (!w)
    {
        _err = Error::InvalidBus;
        return Wire;
    }
    return *w;
}

I2CManager::Error I2CManager::lastError() const
{
    return _err;
}

bool I2CManager::scanDevices(uint8_t bus_num, bool present[127])
{
    ScopedBusLock lk(*this, bus_num, I2C_LOCK_TIMEOUT_MS);
    if (!lk.locked())
        return false;
    return scanDevicesLocked(bus_num, present);
}

bool I2CManager::scanDevicesLocked(uint8_t bus_num, bool present[127])
{
#if defined(ESP32) && I2C_LOCK_DEBUG
    if (!busLockHeldByCurrentTask(bus_num))
        Serial.printf("I2C unlocked access: op: scan bus: %u\n", (unsigned)bus_num);
#endif
    TwoWire *w = wirePtr_(bus_num);
    if (!w)
    {
        _err = Error::InvalidBus;
        return false;
    }
    for (uint8_t addr = 1; addr < 127; ++addr)
        present[addr] = false;
    for (uint8_t addr = 1; addr < 127; ++addr)
    {
        w->beginTransmission(addr);
        uint8_t res = w->endTransmission();
        if (res == 0)
            present[addr] = true;
    }
    return true;
}

bool I2CManager::probeAddress(uint8_t bus_num, uint8_t addr)
{
    ScopedBusLock lk(*this, bus_num, I2C_LOCK_TIMEOUT_MS);
    if (!lk.locked())
        return false;
    return probeAddressLocked(bus_num, addr);
}

bool I2CManager::probeAddressLocked(uint8_t bus_num, uint8_t addr)
{
#if defined(ESP32) && I2C_LOCK_DEBUG
    if (!busLockHeldByCurrentTask(bus_num))
        Serial.printf("I2C unlocked access: op: probe bus: %u\n", (unsigned)bus_num);
#endif
    if (addr == 0 || addr >= 127)
        return false;
    TwoWire *w = wirePtr_(bus_num);
    if (!w)
    {
        _err = Error::InvalidBus;
        return false;
    }
    w->beginTransmission(addr);
    return w->endTransmission() == 0;
}

bool I2CManager::lockBus(uint8_t bus_num, uint32_t timeout_ms)
{
#if defined(ESP32)
    const int8_t idx = busIdx_(bus_num);
    if (idx < 0)
    {
        _err = Error::InvalidBus;
        return false;
    }
    ensureBusMutex_(bus_num);
    SemaphoreHandle_t mtx = (SemaphoreHandle_t)_bus_mtx_[idx];
    if (!mtx)
        return false;
    if (_bus_owner_[idx] == currentTaskToken_())
    {
#if I2C_LOCK_DEBUG
        Serial.printf("I2C nested lock denied: bus: %u\n", (unsigned)bus_num);
#endif
        return false;
    }

    const uint32_t t0 = micros();
    if (xSemaphoreTake(mtx, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
    {
#if I2C_LOCK_DEBUG
        Serial.printf("I2C lock timeout: bus: %u timeout_ms: %lu\n",
                      (unsigned)bus_num, (unsigned long)timeout_ms);
#endif
        return false;
    }
    _bus_owner_[idx] = currentTaskToken_();
#if I2C_LOCK_DEBUG
    const uint32_t wait_us = (uint32_t)(micros() - t0);
    if (wait_us >= I2C_LOCK_WARN_WAIT_US)
    {
        Serial.printf("I2C lock wait: bus: %u wait_us: %lu\n",
                      (unsigned)bus_num, (unsigned long)wait_us);
    }
#endif
    return true;
#else
    (void)bus_num;
    (void)timeout_ms;
    return true;
#endif
}

void I2CManager::unlockBus(uint8_t bus_num)
{
#if defined(ESP32)
    const int8_t idx = busIdx_(bus_num);
    if (idx < 0)
        return;
    SemaphoreHandle_t mtx = (SemaphoreHandle_t)_bus_mtx_[idx];
    if (!mtx)
        return;
    _bus_owner_[idx] = nullptr;
    xSemaphoreGive(mtx);
#else
    (void)bus_num;
#endif
}

bool I2CManager::busLockHeldByCurrentTask(uint8_t bus_num) const
{
#if defined(ESP32)
    const int8_t idx = busIdx_(bus_num);
    if (idx < 0)
        return false;
    return _bus_owner_[idx] == currentTaskToken_();
#else
    (void)bus_num;
    return false;
#endif
}

I2CManager::ScopedBusLock::ScopedBusLock(I2CManager &mgr, uint8_t bus_num, uint32_t timeout_ms)
    : _mgr(&mgr), _bus_num(bus_num)
{
    _locked = _mgr->lockBus(_bus_num, timeout_ms);
}

I2CManager::ScopedBusLock::~ScopedBusLock()
{
    if (_locked && _mgr)
        _mgr->unlockBus(_bus_num);
}

bool I2CManager::i2cPinsFromPorts_(uint8_t sda_port, uint8_t scl_port,
                                   uint8_t &out_sda_gpio, uint8_t &out_scl_gpio)
{
    if (sda_port >= PortIO::PORT_COUNT || scl_port >= PortIO::PORT_COUNT)
        return false;
    const auto &psda = ActiveBoardProfile::PORTS[sda_port];
    const auto &pscl = ActiveBoardProfile::PORTS[scl_port];
    if (psda.backend != PortIO::Backend::Esp32 || pscl.backend != PortIO::Backend::Esp32)
        return false;
    if (psda.u.esp.gpio == 0xFF || pscl.u.esp.gpio == 0xFF)
        return false;
    out_sda_gpio = psda.u.esp.gpio;
    out_scl_gpio = pscl.u.esp.gpio;
    return true;
}

TwoWire *I2CManager::wirePtr_(uint8_t bus_num)
{
    switch (bus_num)
    {
    case 0:
        return &Wire;
#if defined(ESP32)
    case 1:
        return &Wire1;
#if defined(Wire2)
    case 2:
        return &Wire2;
#endif
#endif
    default:
        return nullptr;
    }
}

int8_t I2CManager::busIdx_(uint8_t bus_num)
{
    if (bus_num <= 2)
        return (int8_t)bus_num;
    return -1;
}

#if defined(ESP32)
void I2CManager::ensureBusMutex_(uint8_t bus_num)
{
    const int8_t idx = busIdx_(bus_num);
    if (idx < 0)
        return;
    if (_bus_mtx_[idx] == nullptr)
        _bus_mtx_[idx] = (void *)xSemaphoreCreateMutex();
}

void *I2CManager::currentTaskToken_() const
{
    return (void *)xTaskGetCurrentTaskHandle();
}
#endif
