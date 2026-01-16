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

#include "hal/mcp23017.hpp"
#include "hal/pcf8574.hpp"

class I2CManager;
class Logger;

class Extender
{
public:
    static constexpr uint8_t MAX_DEVS = 16;

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
    Extender(I2CManager &i2c, const std::array<DevCfg, N> &devs, Logger *log = nullptr)
        : _devs(devs.data()), _dev_count((uint8_t)N), _i2c(&i2c), _log(log)
    {
        static_assert(N <= MAX_DEVS, "Extender supports up to 16 devices");
        initState_();
    }

    bool begin();
    void task();
    void rescan();
    bool isPresent(uint8_t dev) const;
    uint32_t rescanIntervalMs() const { return _rescan_interval_ms; }

    bool isConfigured(uint8_t dev) const
    {
        if (dev >= _dev_count)
            return false;
        return _devs[dev].i2c_addr != 0 && _devs[dev].type != Type::None;
    }

    void pinMode(uint8_t dev, uint8_t pin, uint8_t mode);
    void write(uint8_t dev, uint8_t pin, bool level);
    bool read(uint8_t dev, uint8_t pin) const;
    void flushAll();

    const DevCfg *devs() const { return _devs; }
    uint8_t devCount() const { return _dev_count; }

private:
    const DevCfg *_devs = nullptr;
    uint8_t _dev_count = 0;
    I2CManager *_i2c = nullptr;
    Logger *_log = nullptr;
    mutable Mcp23017 _mcp[MAX_DEVS] = {};
    mutable bool _mcp_inited[MAX_DEVS] = {};
    mutable Pcf8574 _pcf[MAX_DEVS] = {};
    mutable bool _pcf_inited[MAX_DEVS] = {};
    mutable bool _dev_failed[MAX_DEVS] = {};
    mutable bool _present[MAX_DEVS] = {};
    mutable bool _warned_missing[MAX_DEVS] = {};
    uint32_t _rescan_interval_ms = 5000;

    void initState_();
    bool ensureDev_(uint8_t dev) const;
    Mcp23017 *mcp_(uint8_t dev) const;
    Pcf8574 *pcf_(uint8_t dev) const;
    void logInitFailOnce_(uint8_t dev, const __FlashStringHelper *msg) const;
    void logPresentChange_(uint8_t dev, bool present) const;
    void setPresent_(uint8_t dev, bool present) const;
};
