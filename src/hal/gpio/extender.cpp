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
    for (uint8_t i = 0; i < _dev_count; ++i)
    {
        _mcp_inited[i] = false;
        _pcf_inited[i] = false;
        _dev_failed[i] = false;
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

bool Extender::ensureDev_(uint8_t dev) const
{
    if (!isConfigured(dev))
        return false;
    if (_dev_failed && _dev_failed[dev])
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
