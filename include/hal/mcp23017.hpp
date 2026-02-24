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

class Mcp23017
{
public:
    enum class Port : uint8_t
    {
        A = 0,
        B = 1
    };

    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        I2c,
        InvalidPin,
        InvalidPort
    };

    Mcp23017() = default;

    bool begin(TwoWire &wire, uint8_t addr = 0x20);

    Error lastError() const;
    bool ready() const;

    bool pinMode(uint8_t pin, uint8_t mode);
    bool writePin(uint8_t pin, bool level);
    bool readPin(uint8_t pin, bool &out) const;
    bool setDirection(Port port, uint8_t dir_mask);
    bool setPullup(Port port, uint8_t pull_mask);
    bool setPolarity(Port port, uint8_t pol_mask);
    bool setInterruptEnable(Port port, uint8_t mask);
    bool setInterruptDefault(Port port, uint8_t mask);
    bool setInterruptControl(Port port, uint8_t mask);
    bool writePort(Port port, uint8_t value);
    bool readPort(Port port, uint8_t &out) const;
    bool readGpio(Port port, uint8_t &out) const;
    bool readOlat(Port port, uint8_t &out) const;
    bool writeOlat(Port port, uint8_t value);
    bool readInterruptFlags(Port port, uint8_t &out) const;
    bool readInterruptCapture(Port port, uint8_t &out) const;
    bool setBanked(bool banked);
    bool setSequential(bool enable);
    bool setMirror(bool enable);
    bool setOpenDrain(bool enable);
    bool setInterruptPolarity(bool active_high);
    bool setHardwareAddressing(bool enable);
    bool flush();
    bool writeReg(uint8_t reg, uint8_t value);
    bool readReg(uint8_t reg, uint8_t &out) const;

private:
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

    TwoWire *_wire = nullptr;
    uint8_t _addr = 0x20;
    mutable Error _err = Error::Ok;

    bool _dirty_dir = false;
    bool _dirty_pull = false;
    bool _dirty_olat = false;

    uint8_t _iodir[2] = {0xFF, 0xFF};
    uint8_t _gppu[2] = {0x00, 0x00};
    uint8_t _ipol[2] = {0x00, 0x00};
    uint8_t _gpinten[2] = {0x00, 0x00};
    uint8_t _defval[2] = {0x00, 0x00};
    uint8_t _intcon[2] = {0x00, 0x00};
    uint8_t _olat[2] = {0x00, 0x00};
    uint8_t _iocon = 0x00;
    bool _cache_valid = false;

    bool cacheFromDevice_();
    bool updateBit_(uint8_t &reg, uint8_t bit, bool value);
    bool setPortReg_(uint8_t base_reg, Port port, uint8_t value);
    bool getPortReg_(uint8_t base_reg, Port port, uint8_t &out) const;
    bool regForPort_(uint8_t base_reg, Port port, uint8_t &out) const;
    bool ioconAddr_(Port port, bool banked_addr, uint8_t &out) const;
    bool writeIocon_(uint8_t value, bool banked_addr);
    bool isBanked_() const;
};
