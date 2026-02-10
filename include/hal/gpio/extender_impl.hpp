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
#include "hal/gpio/extender.hpp"

#include "hal/bus/i2c.hpp"
#include "utils/logger.hpp"

inline void Extender::initState_()
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
    }
}

inline Mcp23017 *Extender::mcp_(uint8_t dev) const
{
    if (dev >= _dev_count)
        return nullptr;
    return &_mcp[dev];
}

inline Pcf8574 *Extender::pcf_(uint8_t dev) const
{
    if (dev >= _dev_count)
        return nullptr;
    return &_pcf[dev];
}

inline void Extender::logInitFailOnce_(uint8_t dev, const __FlashStringHelper *msg) const
{
    if (!_log)
        return;
    if (_dev_failed[dev])
        return;
    _dev_failed[dev] = true;
    _log->warn(F("EXT"), msg, dev);
}

inline void Extender::logPresentChange_(uint8_t dev, bool present) const
{
    if (!_log)
        return;
    if (present)
        _log->info(F("EXT"), F("Extender %u detected"), dev);
    else
        _log->warn(F("EXT"), F("Extender %u missing"), dev);
}

inline void Extender::setPresent_(uint8_t dev, bool present) const
{
    const bool prev = _present[dev];
    _present[dev] = present;
    if (prev == present)
        return;
    logPresentChange_(dev, present);
    if (!present)
    {
        _mcp_inited[dev] = false;
        _pcf_inited[dev] = false;
        _mcp_cache_valid[dev] = 0;
        _pcf_cache_valid[dev] = 0;
        _dev_failed[dev] = false;
        _warned_missing[dev] = false;
    }
    else
    {
        _dev_failed[dev] = false;
        _mcp_cache_valid[dev] = 0;
        _pcf_cache_valid[dev] = 0;
    }
}

inline bool Extender::begin()
{
    rescan();
    return true;
}

inline void Extender::task()
{
    rescan();
}

inline void Extender::rescan()
{
    if (!_i2c)
        return;

    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        if (!isConfigured(i))
        {
            setPresent_(i, false);
            continue;
        }
        const uint8_t bus = _devs[i].bus_num;
        const uint8_t addr = _devs[i].i2c_addr;
        if (bus >= 3 || addr == 0 || addr >= 127)
        {
            setPresent_(i, false);
            continue;
        }
        setPresent_(i, _i2c->probeAddress(bus, addr));
    }
}

inline bool Extender::isPresent(uint8_t dev) const
{
    if (dev >= _dev_count)
        return false;
    return _present[dev];
}

inline bool Extender::ensureDev_(uint8_t dev) const
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
            return false;
        }
        _pcf_inited[dev] = true;
        return true;
    }
    logInitFailOnce_(dev, F("Unsupported extender type for dev %u"));
    return false;
}

inline void Extender::pinMode(uint8_t dev, uint8_t pin, uint8_t mode)
{
    if (!ensureDev_(dev))
        return;
    const DevCfg &cfg = _devs[dev];
    if (cfg.type == Type::MCP23017)
    {
        Mcp23017 *mcp = mcp_(dev);
        if (!mcp)
            return;
        (void)mcp->pinMode(pin, mode);
        return;
    }
    if (cfg.type == Type::PCF8574)
    {
        Pcf8574 *pcf = pcf_(dev);
        if (!pcf)
            return;
        (void)pcf->pinMode(pin, mode);
        return;
    }
}

inline void Extender::write(uint8_t dev, uint8_t pin, bool level)
{
    if (!ensureDev_(dev))
        return;
    const DevCfg &cfg = _devs[dev];
    if (cfg.type == Type::MCP23017)
    {
        Mcp23017 *mcp = mcp_(dev);
        if (!mcp)
            return;
        (void)mcp->writePin(pin, level);
        return;
    }
    if (cfg.type == Type::PCF8574)
    {
        Pcf8574 *pcf = pcf_(dev);
        if (!pcf)
            return;
        (void)pcf->writePin(pin, level);
        return;
    }
}

inline bool Extender::read(uint8_t dev, uint8_t pin) const
{
    if (!ensureDev_(dev))
        return false;
    const DevCfg &cfg = _devs[dev];
    if (cfg.type == Type::MCP23017)
    {
        Mcp23017 *mcp = mcp_(dev);
        if (!mcp)
            return false;
        bool v = false;
        if (!mcp->readPin(pin, v))
        {
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

inline void Extender::flushAll()
{
    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        if (_mcp_inited[i])
            (void)_mcp[i].flush();
        if (_pcf_inited[i])
            (void)_pcf[i].flush();
    }
}
