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

#include "hal/mcp23017.hpp"

#include <Wire.h>

namespace
{
static constexpr uint8_t REG_IODIRA = 0x00;
static constexpr uint8_t REG_IODIRB = 0x01;
static constexpr uint8_t REG_IPOLA = 0x02;
static constexpr uint8_t REG_IPOLB = 0x03;
static constexpr uint8_t REG_GPINTENA = 0x04;
static constexpr uint8_t REG_GPINTENB = 0x05;
static constexpr uint8_t REG_DEFVALA = 0x06;
static constexpr uint8_t REG_DEFVALB = 0x07;
static constexpr uint8_t REG_INTCONA = 0x08;
static constexpr uint8_t REG_INTCONB = 0x09;
static constexpr uint8_t REG_IOCON = 0x0A;
static constexpr uint8_t REG_GPPUA = 0x0C;
static constexpr uint8_t REG_GPPUB = 0x0D;
static constexpr uint8_t REG_INTFA = 0x0E;
static constexpr uint8_t REG_INTFB = 0x0F;
static constexpr uint8_t REG_INTCAPA = 0x10;
static constexpr uint8_t REG_INTCAPB = 0x11;
static constexpr uint8_t REG_GPIOA = 0x12;
static constexpr uint8_t REG_GPIOB = 0x13;
static constexpr uint8_t REG_OLATA = 0x14;
static constexpr uint8_t REG_OLATB = 0x15;

static constexpr uint8_t IOCON_BANK = 1u << 7;
static constexpr uint8_t IOCON_MIRROR = 1u << 6;
static constexpr uint8_t IOCON_SEQOP = 1u << 5;
static constexpr uint8_t IOCON_DISSLW = 1u << 4;
static constexpr uint8_t IOCON_HAEN = 1u << 3;
static constexpr uint8_t IOCON_ODR = 1u << 2;
static constexpr uint8_t IOCON_INTPOL = 1u << 1;
} // namespace

bool Mcp23017::begin(TwoWire &wire, uint8_t addr)
{
    _wire = &wire;
    _addr = addr;
    _err = Error::Ok;
    _cache_valid = false;

    if (!cacheFromDevice_())
        return false;

    return true;
}

bool Mcp23017::cacheFromDevice_()
{
    if (!_wire)
    {
        _err = Error::NoBus;
        return false;
    }

    uint8_t v = 0;
    if (!readReg(REG_IODIRA, v))
        return false;
    _iodir[0] = v;
    if (!readReg(REG_IODIRB, v))
        return false;
    _iodir[1] = v;

    if (!readReg(REG_IPOLA, v))
        return false;
    _ipol[0] = v;
    if (!readReg(REG_IPOLB, v))
        return false;
    _ipol[1] = v;

    if (!readReg(REG_GPINTENA, v))
        return false;
    _gpinten[0] = v;
    if (!readReg(REG_GPINTENB, v))
        return false;
    _gpinten[1] = v;

    if (!readReg(REG_DEFVALA, v))
        return false;
    _defval[0] = v;
    if (!readReg(REG_DEFVALB, v))
        return false;
    _defval[1] = v;

    if (!readReg(REG_INTCONA, v))
        return false;
    _intcon[0] = v;
    if (!readReg(REG_INTCONB, v))
        return false;
    _intcon[1] = v;

    if (!readReg(REG_IOCON, v))
        return false;
    _iocon = v;

    if (!readReg(REG_GPPUA, v))
        return false;
    _gppu[0] = v;
    if (!readReg(REG_GPPUB, v))
        return false;
    _gppu[1] = v;

    if (!readReg(REG_OLATA, v))
        return false;
    _olat[0] = v;
    if (!readReg(REG_OLATB, v))
        return false;
    _olat[1] = v;

    _cache_valid = true;
    return true;
}

bool Mcp23017::pinMode(uint8_t pin, uint8_t mode)
{
    if (pin > 15)
    {
        _err = Error::InvalidPin;
        return false;
    }
    const Port port = (pin >= 8) ? Port::B : Port::A;
    const uint8_t bit = (uint8_t)(pin % 8);
    const uint8_t mask = (uint8_t)(1u << bit);

    bool is_output = (mode == OUTPUT || mode == OUTPUT_OPEN_DRAIN);
    if (is_output)
    {
        _iodir[(uint8_t)port] &= (uint8_t)~mask;
        _gppu[(uint8_t)port] &= (uint8_t)~mask;
        _dirty_dir = true;
        _dirty_pull = true;
        return true;
    }

    _iodir[(uint8_t)port] |= mask;
    if (mode == INPUT_PULLUP)
        _gppu[(uint8_t)port] |= mask;
    else
        _gppu[(uint8_t)port] &= (uint8_t)~mask;
    _dirty_dir = true;
    _dirty_pull = true;
    return true;
}

bool Mcp23017::writePin(uint8_t pin, bool level)
{
    if (pin > 15)
    {
        _err = Error::InvalidPin;
        return false;
    }
    const Port port = (pin >= 8) ? Port::B : Port::A;
    const uint8_t bit = (uint8_t)(pin % 8);
    const uint8_t mask = (uint8_t)(1u << bit);
    if (level)
        _olat[(uint8_t)port] |= mask;
    else
        _olat[(uint8_t)port] &= (uint8_t)~mask;
    _dirty_olat = true;
    return true;
}

bool Mcp23017::readPin(uint8_t pin, bool &out) const
{
    if (pin > 15)
        return false;
    const Port port = (pin >= 8) ? Port::B : Port::A;
    uint8_t v = 0;
    if (!readGpio(port, v))
        return false;
    out = (v & (uint8_t)(1u << (pin % 8))) != 0;
    return true;
}

bool Mcp23017::setDirection(Port port, uint8_t dir_mask)
{
    if (port != Port::A && port != Port::B)
    {
        _err = Error::InvalidPort;
        return false;
    }
    _iodir[(uint8_t)port] = dir_mask;
    _dirty_dir = true;
    return true;
}

bool Mcp23017::setPullup(Port port, uint8_t pull_mask)
{
    if (port != Port::A && port != Port::B)
    {
        _err = Error::InvalidPort;
        return false;
    }
    _gppu[(uint8_t)port] = pull_mask;
    _dirty_pull = true;
    return true;
}

bool Mcp23017::setPolarity(Port port, uint8_t pol_mask)
{
    if (port != Port::A && port != Port::B)
    {
        _err = Error::InvalidPort;
        return false;
    }
    _ipol[(uint8_t)port] = pol_mask;
    return setPortReg_(REG_IPOLA, port, pol_mask);
}

bool Mcp23017::setInterruptEnable(Port port, uint8_t mask)
{
    if (port != Port::A && port != Port::B)
    {
        _err = Error::InvalidPort;
        return false;
    }
    _gpinten[(uint8_t)port] = mask;
    return setPortReg_(REG_GPINTENA, port, mask);
}

bool Mcp23017::setInterruptDefault(Port port, uint8_t mask)
{
    if (port != Port::A && port != Port::B)
    {
        _err = Error::InvalidPort;
        return false;
    }
    _defval[(uint8_t)port] = mask;
    return setPortReg_(REG_DEFVALA, port, mask);
}

bool Mcp23017::setInterruptControl(Port port, uint8_t mask)
{
    if (port != Port::A && port != Port::B)
    {
        _err = Error::InvalidPort;
        return false;
    }
    _intcon[(uint8_t)port] = mask;
    return setPortReg_(REG_INTCONA, port, mask);
}

bool Mcp23017::writePort(Port port, uint8_t value)
{
    if (port != Port::A && port != Port::B)
    {
        _err = Error::InvalidPort;
        return false;
    }
    _olat[(uint8_t)port] = value;
    _dirty_olat = true;
    return true;
}

bool Mcp23017::readPort(Port port, uint8_t &out) const
{
    return readGpio(port, out);
}

bool Mcp23017::readGpio(Port port, uint8_t &out) const
{
    return getPortReg_(REG_GPIOA, port, out);
}

bool Mcp23017::readOlat(Port port, uint8_t &out) const
{
    return getPortReg_(REG_OLATA, port, out);
}

bool Mcp23017::writeOlat(Port port, uint8_t value)
{
    if (port != Port::A && port != Port::B)
    {
        _err = Error::InvalidPort;
        return false;
    }
    _olat[(uint8_t)port] = value;
    _dirty_olat = true;
    return true;
}

bool Mcp23017::readInterruptFlags(Port port, uint8_t &out) const
{
    return getPortReg_(REG_INTFA, port, out);
}

bool Mcp23017::readInterruptCapture(Port port, uint8_t &out) const
{
    return getPortReg_(REG_INTCAPA, port, out);
}

bool Mcp23017::setBanked(bool banked)
{
    const bool was_banked = isBanked_();
    updateBit_(_iocon, 7, banked);
    return writeIocon_(_iocon, was_banked);
}

bool Mcp23017::setSequential(bool enable)
{
    updateBit_(_iocon, 5, !enable);
    return writeIocon_(_iocon, isBanked_());
}

bool Mcp23017::setMirror(bool enable)
{
    updateBit_(_iocon, 6, enable);
    return writeIocon_(_iocon, isBanked_());
}

bool Mcp23017::setOpenDrain(bool enable)
{
    updateBit_(_iocon, 2, enable);
    return writeIocon_(_iocon, isBanked_());
}

bool Mcp23017::setInterruptPolarity(bool active_high)
{
    updateBit_(_iocon, 1, active_high);
    return writeIocon_(_iocon, isBanked_());
}

bool Mcp23017::setHardwareAddressing(bool enable)
{
    updateBit_(_iocon, 3, enable);
    return writeIocon_(_iocon, isBanked_());
}

bool Mcp23017::flush()
{
    bool ok = true;
    if (_dirty_dir)
    {
        ok = setPortReg_(REG_IODIRA, Port::A, _iodir[0]) && setPortReg_(REG_IODIRA, Port::B, _iodir[1]);
        _dirty_dir = false;
    }
    if (_dirty_pull)
    {
        ok = setPortReg_(REG_GPPUA, Port::A, _gppu[0]) && setPortReg_(REG_GPPUA, Port::B, _gppu[1]) && ok;
        _dirty_pull = false;
    }
    if (_dirty_olat)
    {
        ok = setPortReg_(REG_OLATA, Port::A, _olat[0]) && setPortReg_(REG_OLATA, Port::B, _olat[1]) && ok;
        _dirty_olat = false;
    }
    return ok;
}

bool Mcp23017::writeReg(uint8_t reg, uint8_t value)
{
    if (!_wire)
    {
        _err = Error::NoBus;
        return false;
    }
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write(value);
    if (_wire->endTransmission() != 0)
    {
        _err = Error::I2c;
        return false;
    }
    _err = Error::Ok;
    return true;
}

bool Mcp23017::readReg(uint8_t reg, uint8_t &out) const
{
    if (!_wire)
    {
        _err = Error::NoBus;
        return false;
    }
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0)
    {
        _err = Error::I2c;
        return false;
    }
    const uint8_t got = _wire->requestFrom(_addr, (uint8_t)1);
    if (got != 1)
    {
        _err = Error::I2c;
        return false;
    }
    out = _wire->read();
    _err = Error::Ok;
    return true;
}

bool Mcp23017::updateBit_(uint8_t &reg, uint8_t bit, bool value)
{
    const uint8_t mask = (uint8_t)(1u << bit);
    if (value)
        reg |= mask;
    else
        reg &= (uint8_t)~mask;
    return true;
}

bool Mcp23017::setPortReg_(uint8_t base_reg, Port port, uint8_t value)
{
    uint8_t reg = 0;
    if (!regForPort_(base_reg, port, reg))
        return false;
    return writeReg(reg, value);
}

bool Mcp23017::getPortReg_(uint8_t base_reg, Port port, uint8_t &out) const
{
    uint8_t reg = 0;
    if (!regForPort_(base_reg, port, reg))
        return false;
    return readReg(reg, out);
}

bool Mcp23017::regForPort_(uint8_t base_reg, Port port, uint8_t &out) const
{
    if (port != Port::A && port != Port::B)
        return false;

    if (!isBanked_())
    {
        out = (uint8_t)(base_reg + (port == Port::B ? 1 : 0));
        return true;
    }

    switch (base_reg)
    {
    case REG_IODIRA:
        out = (port == Port::A) ? 0x00 : 0x10;
        return true;
    case REG_IPOLA:
        out = (port == Port::A) ? 0x01 : 0x11;
        return true;
    case REG_GPINTENA:
        out = (port == Port::A) ? 0x02 : 0x12;
        return true;
    case REG_DEFVALA:
        out = (port == Port::A) ? 0x03 : 0x13;
        return true;
    case REG_INTCONA:
        out = (port == Port::A) ? 0x04 : 0x14;
        return true;
    case REG_IOCON:
        out = (port == Port::A) ? 0x05 : 0x15;
        return true;
    case REG_GPPUA:
        out = (port == Port::A) ? 0x06 : 0x16;
        return true;
    case REG_INTFA:
        out = (port == Port::A) ? 0x07 : 0x17;
        return true;
    case REG_INTCAPA:
        out = (port == Port::A) ? 0x08 : 0x18;
        return true;
    case REG_GPIOA:
        out = (port == Port::A) ? 0x09 : 0x19;
        return true;
    case REG_OLATA:
        out = (port == Port::A) ? 0x0A : 0x1A;
        return true;
    default:
        out = (uint8_t)(base_reg + (port == Port::B ? 1 : 0));
        return true;
    }
}

bool Mcp23017::ioconAddr_(Port port, bool banked_addr, uint8_t &out) const
{
    if (port != Port::A && port != Port::B)
        return false;
    if (!banked_addr)
    {
        out = (uint8_t)(REG_IOCON + (port == Port::B ? 1 : 0));
        return true;
    }
    out = (port == Port::A) ? 0x05 : 0x15;
    return true;
}

bool Mcp23017::writeIocon_(uint8_t value, bool banked_addr)
{
    uint8_t reg_a = 0;
    uint8_t reg_b = 0;
    if (!ioconAddr_(Port::A, banked_addr, reg_a))
        return false;
    if (!ioconAddr_(Port::B, banked_addr, reg_b))
        return false;
    return writeReg(reg_a, value) && writeReg(reg_b, value);
}
