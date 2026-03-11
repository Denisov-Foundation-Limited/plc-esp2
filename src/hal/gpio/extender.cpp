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

#include "hal/gpio/extender.hpp"

#include <Arduino.h>
#include <Wire.h>
#include "hal/bus/i2c.hpp"
#include "utils/logger.hpp"

void Extender::initState_()
{
    if (_dev_count == 0)
        return;
    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        _mcp_inited[i] = false;
        _pcf_inited[i] = false;
        _mcp_cache[i] = 0;
        _mcp_cache_valid[i] = 0;
        _pcf_cache[i] = 0;
        _pcf_cache_valid[i] = 0;
        _dev_failed[i] = false;
        _present[i] = false;
        _warned_missing[i] = false;
        _miss_streak[i] = 0;
        _hit_streak[i] = 0;
        _recover_attempts[i] = 0;
        _miss_since_ms[i] = 0;
        _hit_since_ms[i] = 0;
    }
}

Mcp23017 *Extender::mcp_(uint8_t dev) const
{
    if (dev >= _dev_count)
        return nullptr;
    return &_mcp[dev];
}

Pcf8574 *Extender::pcf_(uint8_t dev) const
{
    if (dev >= _dev_count)
        return nullptr;
    return &_pcf[dev];
}

void Extender::logInitFailOnce_(uint8_t dev, const __FlashStringHelper *msg) const
{
    if (!_log)
        return;
    if (_dev_failed[dev])
        return;
    _dev_failed[dev] = true;
    _log->warn(F("EXT"), msg, dev);
}

void Extender::logPresentChange_(uint8_t dev, bool present) const
{
    if (!_log)
        return;
    if (_suppress_first_pass_logs)
        return;
    if (present)
    {
        if (_recover_attempts[dev] > 0)
            _log->info(F("EXT"), F("Extender %u detected attempts: %u"), dev, (unsigned)_recover_attempts[dev]);
        else
            _log->info(F("EXT"), F("Extender %u detected"), dev);
    }
    else
        _log->warn(F("EXT"), F("Extender %u missing"), dev);
}

void Extender::setPresent_(uint8_t dev, bool present) const
{
    const bool prev = _present[dev];
    _present[dev] = present;
    if (prev == present)
        return;
    logPresentChange_(dev, present);
    if (!present)
    {
        _miss_streak[dev] = 0;
        _hit_streak[dev] = 0;
        _miss_since_ms[dev] = 0;
        _hit_since_ms[dev] = 0;
        _mcp_inited[dev] = false;
        _pcf_inited[dev] = false;
        _mcp_cache_valid[dev] = 0;
        _pcf_cache_valid[dev] = 0;
        _dev_failed[dev] = false;
        _warned_missing[dev] = false;
        scheduleFastRescan_();
    }
    else
    {
        _recover_attempts[dev] = 0;
        _miss_streak[dev] = 0;
        _hit_streak[dev] = 0;
        _miss_since_ms[dev] = 0;
        _hit_since_ms[dev] = 0;
        _dev_failed[dev] = false;
        _mcp_cache_valid[dev] = 0;
        _pcf_cache_valid[dev] = 0;
    }
}

void Extender::noteProbeResult_(uint8_t dev, bool present) const
{
    if (dev >= _dev_count)
        return;
    const uint32_t now = millis();
    if (present)
    {
        _miss_streak[dev] = 0;
        _miss_since_ms[dev] = 0;
        if (!_present[dev] && _hit_confirm_count > 1)
        {
            if (_hit_since_ms[dev] == 0)
                _hit_since_ms[dev] = now;
            if (_hit_streak[dev] < 0xFF)
                ++_hit_streak[dev];
            const uint32_t stable_ms = now - _hit_since_ms[dev];
            if (_hit_streak[dev] < _hit_confirm_count || stable_ms < _hit_stable_ms)
                return;
        }
        setPresent_(dev, true);
        return;
    }

    _hit_streak[dev] = 0;
    _hit_since_ms[dev] = 0;

    // Avoid false "missing" flaps on occasional I2C probe timeouts.
    if (_present[dev] && _miss_confirm_count > 1)
    {
        if (_miss_since_ms[dev] == 0)
            _miss_since_ms[dev] = now;
        if (_miss_streak[dev] < 0xFF)
            ++_miss_streak[dev];
        const uint32_t stable_ms = now - _miss_since_ms[dev];
        if (_miss_streak[dev] < _miss_confirm_count || stable_ms < _miss_stable_ms)
            return;
    }

    setPresent_(dev, false);
}

bool Extender::begin()
{
    _suppress_first_pass_logs = true;
    if (_i2c && _dev_count)
    {
        // Do a fast startup probe only for the base bus.
        // External extender buses are allowed to appear later via task()-driven rescans.
        for (uint8_t i = 0; i < _dev_count; ++i)
        {
            if (_devs[i].bus_num != 0)
                continue;
            scanDevice_(i);
        }
    }
    _scan_active = false;
    _scan_index = 0;
    _next_scan_ms = millis() + _rescan_interval_ms;
    return true;
}

void Extender::task()
{
    if (!_i2c || _dev_count == 0)
        return;

    const uint32_t now = millis();
    if ((int32_t)(now - _next_scan_ms) < 0)
        return;

    if (!_scan_active)
    {
        _scan_active = true;
        _scan_index = 0;
    }

    scanDevice_(_scan_index);
    ++_scan_index;

    if (_scan_index >= _dev_count)
    {
        _scan_active = false;
        _scan_index = 0;
        _suppress_first_pass_logs = false;
        _next_scan_ms = now + (anyMissing_() ? _fast_rescan_interval_ms : _rescan_interval_ms);
    }
    else
    {
        // Continue pass quickly, but spread probes over scheduler ticks.
        _next_scan_ms = now + 1;
    }
}

void Extender::noteRuntimeIoFailure_(uint8_t dev) const
{
    if (dev >= _dev_count)
        return;
    const uint32_t now = millis();
    // Runtime I/O can fail transiently (contention/noise). Confirm before
    // dropping device to avoid one-shot detected/missing flaps.
    if (_present[dev] && _miss_confirm_count > 1)
    {
        if (_miss_since_ms[dev] == 0)
            _miss_since_ms[dev] = now;
        if (_miss_streak[dev] < 0xFF)
            ++_miss_streak[dev];
        const uint32_t stable_ms = now - _miss_since_ms[dev];
        if (_miss_streak[dev] < _miss_confirm_count || stable_ms < _miss_stable_ms)
            return;
    }
    setPresent_(dev, false);
}

bool Extender::anyMissing_() const
{
    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        if (!isConfigured(i))
            continue;
        if (!_present[i])
            return true;
    }
    return false;
}

void Extender::scheduleFastRescan_() const
{
    const uint32_t now = millis();
    const uint32_t fast_at = now + _fast_rescan_interval_ms;
    if (!_scan_active || (int32_t)(_next_scan_ms - fast_at) > 0)
        _next_scan_ms = fast_at;
}

void Extender::rescan()
{
    if (!_i2c)
        return;

    for (uint8_t i = 0; i < _dev_count; ++i)
        scanDevice_(i);
}

void Extender::scanDevice_(uint8_t i)
{
    if (i >= _dev_count || !_i2c)
        return;
    if (!isConfigured(i))
    {
        setPresent_(i, false);
        return;
    }

    const uint8_t bus = _devs[i].bus_num;
    const uint8_t addr = _devs[i].i2c_addr;
    if (bus >= 3 || addr == 0 || addr >= 127)
    {
        setPresent_(i, false);
        return;
    }

    bool present = false;
    uint16_t attempts_used = 0;
    for (uint8_t retry = 0; retry <= _probe_retry_count; ++retry)
    {
        I2CManager::ScopedBusLock lk(*_i2c, bus);
        ++attempts_used;
        if (lk.locked())
            present = scanDeviceLocked_(i);
        if (present)
            break;
        if (retry < _probe_retry_count)
            delay(_probe_retry_delay_ms);
    }

    if (!_present[i])
    {
        const uint32_t total = (uint32_t)_recover_attempts[i] + attempts_used;
        _recover_attempts[i] = (total > 0xFFFFu) ? 0xFFFFu : (uint16_t)total;
    }
    else if (present)
    {
        _recover_attempts[i] = 0;
    }
    noteProbeResult_(i, present);
}

bool Extender::scanDeviceLocked_(uint8_t i)
{
    if (i >= _dev_count)
        return false;
    if (!isConfigured(i))
    {
        setPresent_(i, false);
        return false;
    }
    const uint8_t bus = _devs[i].bus_num;
    const uint8_t addr = _devs[i].i2c_addr;
    if (bus >= 3 || addr == 0 || addr >= 127)
    {
        setPresent_(i, false);
        return false;
    }
    return _i2c->probeAddressLocked(bus, addr);
}

bool Extender::isPresent(uint8_t dev) const
{
    if (dev >= _dev_count)
        return false;
    return _present[dev];
}

bool Extender::ensureDevLocked_(uint8_t dev) const
{
    if (!isConfigured(dev))
        return false;
    if (_dev_failed[dev])
        return false;
    if (!_present[dev])
        return false;
    if (_mcp_inited[dev])
        return true;
    if (_pcf_inited[dev])
        return true;
    if (!_i2c)
    {
        logInitFailOnce_(dev, F("I2C missing for extender %u"));
        return false;
    }

    const DevCfg &cfg = _devs[dev];
    TwoWire *wire = _i2c->wirePtr(cfg.bus_num);
    if (!wire)
    {
        logInitFailOnce_(dev, F("I2C bus invalid for extender %u"));
        return false;
    }
    wire->beginTransmission(cfg.i2c_addr);
    if (wire->endTransmission() != 0)
    {
        logInitFailOnce_(dev, F("I2C device not responding for extender %u"));
        setPresent_(dev, false);
        return false;
    }
    if (cfg.type == Type::MCP23017)
    {
        Mcp23017 *mcp = mcp_(dev);
        if (!mcp)
            return false;
        if (!mcp->begin(*wire, cfg.i2c_addr))
        {
            logInitFailOnce_(dev, F("MCP23017 init failed for extender %u"));
            setPresent_(dev, false);
            return false;
        }
        _mcp_inited[dev] = true;
        return true;
    }
    if (cfg.type == Type::PCF8574)
    {
        Pcf8574 *pcf = pcf_(dev);
        if (!pcf)
            return false;
        if (!pcf->begin(*wire, cfg.i2c_addr))
        {
            logInitFailOnce_(dev, F("PCF8574 init failed for extender %u"));
            setPresent_(dev, false);
            return false;
        }
        _pcf_inited[dev] = true;
        return true;
    }
    logInitFailOnce_(dev, F("Unsupported extender type for dev %u"));
    return false;
}

void Extender::pinMode(uint8_t dev, uint8_t pin, uint8_t mode)
{
    if (dev >= _dev_count || !_i2c)
        return;
    const DevCfg &cfg = _devs[dev];
    I2CManager::ScopedBusLock lk(*_i2c, cfg.bus_num);
    if (!lk.locked())
        return;
    if (!ensureDevLocked_(dev))
        return;
    if (cfg.type == Type::MCP23017)
    {
        Mcp23017 *mcp = mcp_(dev);
        if (!mcp)
            return;
        if (!mcp->pinMode(pin, mode))
            noteRuntimeIoFailure_(dev);
        return;
    }
    if (cfg.type == Type::PCF8574)
    {
        Pcf8574 *pcf = pcf_(dev);
        if (!pcf)
            return;
        if (!pcf->pinMode(pin, mode))
            noteRuntimeIoFailure_(dev);
        return;
    }
}

void Extender::write(uint8_t dev, uint8_t pin, bool level)
{
    if (dev >= _dev_count || !_i2c)
        return;
    const DevCfg &cfg = _devs[dev];
    I2CManager::ScopedBusLock lk(*_i2c, cfg.bus_num);
    if (!lk.locked())
        return;
    if (!ensureDevLocked_(dev))
        return;
    if (cfg.type == Type::MCP23017)
    {
        Mcp23017 *mcp = mcp_(dev);
        if (!mcp)
            return;
        if (!mcp->writePin(pin, level))
            noteRuntimeIoFailure_(dev);
        return;
    }
    if (cfg.type == Type::PCF8574)
    {
        Pcf8574 *pcf = pcf_(dev);
        if (!pcf)
            return;
        if (!pcf->writePin(pin, level))
            noteRuntimeIoFailure_(dev);
        return;
    }
}

bool Extender::read(uint8_t dev, uint8_t pin) const
{
    if (dev >= _dev_count || !_i2c)
        return false;
    const DevCfg &cfg = _devs[dev];
    I2CManager::ScopedBusLock lk(*_i2c, cfg.bus_num);
    if (!lk.locked())
        return false;
    if (!ensureDevLocked_(dev))
        return false;
    if (cfg.type == Type::MCP23017)
    {
        Mcp23017 *mcp = mcp_(dev);
        if (!mcp)
            return false;
        bool v = false;
        if (!mcp->readPin(pin, v))
        {
            noteRuntimeIoFailure_(dev);
            const uint8_t bit = (uint8_t)(pin & 0x0F);
            const uint16_t mask = (uint16_t)(1u << bit);
            if (_mcp_cache_valid[dev] & mask)
                return (_mcp_cache[dev] & mask) != 0;
            return false;
        }
        if (pin <= 15)
        {
            const uint16_t mask = (uint16_t)(1u << (uint8_t)(pin & 0x0F));
            if (v)
                _mcp_cache[dev] |= mask;
            else
                _mcp_cache[dev] &= (uint16_t)~mask;
            _mcp_cache_valid[dev] |= mask;
        }
        return v;
    }
    if (cfg.type == Type::PCF8574)
    {
        Pcf8574 *pcf = pcf_(dev);
        if (!pcf)
            return false;
        bool v = false;
        if (!pcf->readPin(pin, v))
        {
            noteRuntimeIoFailure_(dev);
            const uint8_t bit = (uint8_t)(pin & 0x07);
            const uint8_t mask = (uint8_t)(1u << bit);
            if (_pcf_cache_valid[dev] & mask)
                return (_pcf_cache[dev] & mask) != 0;
            return false;
        }
        if (pin <= 7)
        {
            const uint8_t mask = (uint8_t)(1u << (uint8_t)(pin & 0x07));
            if (v)
                _pcf_cache[dev] |= mask;
            else
                _pcf_cache[dev] &= (uint8_t)~mask;
            _pcf_cache_valid[dev] |= mask;
        }
        return v;
    }
    return false;
}

void Extender::flushAll()
{
    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        if (!_i2c)
            return;
        I2CManager::ScopedBusLock lk(*_i2c, _devs[i].bus_num);
        if (!lk.locked())
            continue;
        if (_mcp_inited[i] && !_mcp[i].flush())
            noteRuntimeIoFailure_(i);
        if (_pcf_inited[i] && !_pcf[i].flush())
            noteRuntimeIoFailure_(i);
    }
}
