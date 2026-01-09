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
#include <array>
#include <stddef.h>
#include <stdint.h>
class I2CManager;

class Extender
{
public:
    enum class Type : uint8_t
    {
        None = 0,
        PCF8574 = 1,
        MCP23017 = 2
    };

    struct DevCfg
    {
        uint8_t bus_num;  // I2C bus number
        uint8_t i2c_addr; // 7-bit; 0 means "unused slot"
        Type type;
    };

    template <size_t N>
    Extender(I2CManager &i2c, const std::array<DevCfg, N> &devs)
        : _devs(devs.data()), _dev_count((uint8_t)N), _i2c(&i2c)
    {
    }

    bool isConfigured(uint8_t dev) const
    {
        if (dev >= _dev_count)
            return false;
        return _devs[dev].i2c_addr != 0 && _devs[dev].type != Type::None;
    }

    // Basic GPIO API over expanders (stub).
    // Replace with real PCF8574 / MCP23017 driver calls.
    void pinMode(uint8_t dev, uint8_t pin, uint8_t mode)
    {
        (void)mode;
        if (!isConfigured(dev))
            return;
        (void)pin;
    }

    void write(uint8_t dev, uint8_t pin, bool level)
    {
        if (!isConfigured(dev))
            return;
        (void)pin;
        (void)level;
    }

    bool read(uint8_t dev, uint8_t pin) const
    {
        if (!isConfigured(dev))
            return false;
        (void)pin;
        return false;
    }

    void flushAll()
    {
        // optional: push buffered writes to devices
    }

    const DevCfg *devs() const { return _devs; }
    uint8_t devCount() const { return _dev_count; }

private:
    const DevCfg *_devs = nullptr;
    uint8_t _dev_count = 0;
    I2CManager *_i2c = nullptr;
};
