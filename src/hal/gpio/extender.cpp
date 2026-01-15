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

#include <vector>

#include "hal/bus/i2c.hpp"
#include "utils/logger.hpp"

void Extender::initState_()
{
    if (_dev_count == 0)
        return;
    _mcp = new Mcp23017[_dev_count];
    _mcp_inited = new bool[_dev_count];
    _pcf = new Pcf8574[_dev_count];
    _pcf_inited = new bool[_dev_count];
    _dev_failed = new bool[_dev_count];
    _present = new bool[_dev_count];
    _warned_missing = new bool[_dev_count];
    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        _mcp_inited[i] = false;
        _pcf_inited[i] = false;
        _dev_failed[i] = false;
        _present[i] = false;
        _warned_missing[i] = false;
    }
}

Mcp23017 *Extender::mcp_(uint8_t dev) const
{
    if (!_mcp || dev >= _dev_count)
        return nullptr;
    return &_mcp[dev];
}

Pcf8574 *Extender::pcf_(uint8_t dev) const
{
    if (!_pcf || dev >= _dev_count)
        return nullptr;
    return &_pcf[dev];
}

void Extender::logInitFailOnce_(uint8_t dev, const __FlashStringHelper *msg) const
{
    if (!_log || !_dev_failed)
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
    if (present)
        _log->info(F("EXT"), F("Extender %u detected"), dev);
    else
        _log->warn(F("EXT"), F("Extender %u missing"), dev);
}

void Extender::setPresent_(uint8_t dev, bool present) const
{
    if (!_present)
        return;
    const bool prev = _present[dev];
    _present[dev] = present;
    if (prev == present)
        return;
    logPresentChange_(dev, present);
    if (!present)
    {
        if (_mcp_inited)
            _mcp_inited[dev] = false;
        if (_pcf_inited)
            _pcf_inited[dev] = false;
        if (_dev_failed)
            _dev_failed[dev] = false;
        if (_warned_missing)
            _warned_missing[dev] = false;
    }
    else
    {
        if (_dev_failed)
            _dev_failed[dev] = false;
    }
}

bool Extender::begin()
{
    rescan();
    return true;
}

void Extender::task()
{
    rescan();
}

void Extender::rescan()
{
    if (!_i2c || !_present)
        return;

    bool bus_used[3] = {false, false, false};
    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        if (!isConfigured(i))
            continue;
        const uint8_t bus = _devs[i].bus_num;
        if (bus < 3)
            bus_used[bus] = true;
    }

    std::vector<uint8_t> addrs[3];
    bool bus_ok[3] = {true, true, true};
    for (uint8_t b = 0; b < 3; ++b)
    {
        if (!bus_used[b])
            continue;
        if (!_i2c->scanDevices(b, addrs[b]))
            bus_ok[b] = false;
    }

    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        if (!isConfigured(i))
        {
            setPresent_(i, false);
            continue;
        }
        const uint8_t bus = _devs[i].bus_num;
        const uint8_t addr = _devs[i].i2c_addr;
        if (bus >= 3 || !bus_ok[bus])
        {
            setPresent_(i, false);
            continue;
        }
        bool found = false;
        for (size_t a = 0; a < addrs[bus].size(); ++a)
        {
            if (addrs[bus][a] == addr)
            {
                found = true;
                break;
            }
        }
        setPresent_(i, found);
    }
}

bool Extender::isPresent(uint8_t dev) const
{
    if (!_present || dev >= _dev_count)
        return false;
    return _present[dev];
}

bool Extender::ensureDev_(uint8_t dev) const
{
    if (!isConfigured(dev))
        return false;
    if (_dev_failed && _dev_failed[dev])
        return false;
    if (_present && !_present[dev])
        return false;
    if (_mcp_inited && _mcp_inited[dev])
        return true;
    if (_pcf_inited && _pcf_inited[dev])
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
        if (_present)
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
        if (_mcp_inited)
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
        if (_pcf_inited)
            _pcf_inited[dev] = true;
        return true;
    }
    logInitFailOnce_(dev, F("Unsupported extender type for dev %u"));
    return false;
}

void Extender::pinMode(uint8_t dev, uint8_t pin, uint8_t mode)
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

void Extender::write(uint8_t dev, uint8_t pin, bool level)
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

bool Extender::read(uint8_t dev, uint8_t pin) const
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
            return false;
        return v;
    }
    if (cfg.type == Type::PCF8574)
    {
        Pcf8574 *pcf = pcf_(dev);
        if (!pcf)
            return false;
        bool v = false;
        if (!pcf->readPin(pin, v))
            return false;
        return v;
    }
    return false;
}

void Extender::flushAll()
{
    if ((!_mcp || !_mcp_inited) && (!_pcf || !_pcf_inited))
        return;
    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        if (_mcp_inited && _mcp_inited[i])
            (void)_mcp[i].flush();
        if (_pcf_inited && _pcf_inited[i])
            (void)_pcf[i].flush();
    }
}
