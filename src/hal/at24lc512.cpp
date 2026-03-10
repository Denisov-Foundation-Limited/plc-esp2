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

#include "hal/at24lc512.hpp"
#include <Arduino.h>
#include <Wire.h>

namespace
{
struct At24LockGuard
{
    At24LockGuard(At24lc512::LockCallback lock_cb, At24lc512::UnlockCallback unlock_cb, void *ctx, uint32_t timeout_ms)
        : _unlock_cb(unlock_cb), _ctx(ctx)
    {
        _locked = lock_cb ? lock_cb(ctx, timeout_ms) : true;
    }
    ~At24LockGuard()
    {
        if (_locked && _unlock_cb)
            _unlock_cb(_ctx);
    }
    bool locked() const { return _locked; }

private:
    At24lc512::UnlockCallback _unlock_cb = nullptr;
    void *_ctx = nullptr;
    bool _locked = false;
};
} // namespace

At24lc512::At24lc512(TwoWire &wire)
    : _wire(&wire)
{
}

bool At24lc512::begin(TwoWire &wire, uint8_t addr)
{
    _wire = &wire;
    _addr = addr;
    _err = _wire ? Error::Ok : Error::NoBus;
    return _wire != nullptr;
}

void At24lc512::setBusLockCallbacks(LockCallback lock_cb, UnlockCallback unlock_cb, void *ctx)
{
    _lock_cb = lock_cb;
    _unlock_cb = unlock_cb;
    _lock_ctx = ctx;
}

bool At24lc512::read(uint16_t mem_addr, uint8_t *buf, uint16_t len)
{
    if (!_wire)
    {
        _err = Error::NoBus;
        return false;
    }

    At24LockGuard lk(_lock_cb, _unlock_cb, _lock_ctx, 100);
    if (!lk.locked())
    {
        _err = Error::I2c;
        return false;
    }

    _wire->beginTransmission(_addr);
    _wire->write((uint8_t)(mem_addr >> 8));
    _wire->write((uint8_t)(mem_addr & 0xFF));
    if (_wire->endTransmission(false) != 0)
    {
        _err = Error::I2c;
        return false;
    }

    uint16_t i = 0;
    while (i < len)
    {
        const uint8_t chunk = (uint8_t)min<uint16_t>(len - i, 32);
        const uint8_t got = _wire->requestFrom(_addr, chunk);
        if (got != chunk)
        {
            _err = Error::I2c;
            return false;
        }
        for (uint8_t j = 0; j < chunk; ++j)
            buf[i++] = _wire->read();
    }

    _err = Error::Ok;
    return true;
}

bool At24lc512::write(uint16_t mem_addr, const uint8_t *buf, uint16_t len)
{
    if (!_wire)
    {
        _err = Error::NoBus;
        return false;
    }

    At24LockGuard lk(_lock_cb, _unlock_cb, _lock_ctx, 100);
    if (!lk.locked())
    {
        _err = Error::I2c;
        return false;
    }

    const uint16_t start_addr = mem_addr;
    uint16_t i = 0;
    while (i < len)
    {
        const uint8_t page_off = (uint8_t)(mem_addr & 0x7F);
        const uint8_t chunk = (uint8_t)min<uint16_t>(len - i, 128 - page_off);

        _wire->beginTransmission(_addr);
        _wire->write((uint8_t)(mem_addr >> 8));
        _wire->write((uint8_t)(mem_addr & 0xFF));
        for (uint8_t j = 0; j < chunk; ++j)
            _wire->write(buf[i + j]);
        if (_wire->endTransmission() != 0)
        {
            _err = Error::I2c;
            return false;
        }

        delay(5);

        mem_addr += chunk;
        i += chunk;
    }

    uint32_t end = (uint32_t)start_addr + (uint32_t)len;
    if (end > kSizeBytes)
        end = kSizeBytes;
    if (end > _used_end)
        _used_end = end;

    _err = Error::Ok;
    return true;
}

At24lc512::Error At24lc512::lastError() const
{
    return _err;
}

uint32_t At24lc512::usedBytes() const
{
    return _used_end;
}
