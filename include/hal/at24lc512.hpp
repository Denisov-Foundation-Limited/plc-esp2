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
#include <Wire.h>
#include <stdint.h>

class At24lc512
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        I2c
    };

    static constexpr uint8_t kDefaultAddr = 0x50;
    static constexpr uint32_t kSizeBytes = 65536;

    At24lc512() = default;
    explicit At24lc512(TwoWire &wire) : _wire(&wire) {}

    bool begin(TwoWire &wire, uint8_t addr = kDefaultAddr)
    {
        _wire = &wire;
        _addr = addr;
        _err = _wire ? Error::Ok : Error::NoBus;
        return _wire != nullptr;
    }

    bool read(uint16_t mem_addr, uint8_t *buf, uint16_t len)
    {
        if (!_wire)
        {
            _err = Error::NoBus;
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

    bool write(uint16_t mem_addr, const uint8_t *buf, uint16_t len)
    {
        if (!_wire)
        {
            _err = Error::NoBus;
            return false;
        }

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

        uint32_t end = (uint32_t)mem_addr + (uint32_t)len;
        if (end > kSizeBytes)
            end = kSizeBytes;
        if (end > _used_end)
            _used_end = end;

        _err = Error::Ok;
        return true;
    }

    Error lastError() const { return _err; }

    static constexpr uint32_t capacityBytes() { return kSizeBytes; }
    static constexpr uint32_t remainingBytes(uint32_t offset)
    {
        return (offset >= kSizeBytes) ? 0 : (kSizeBytes - offset);
    }
    uint32_t usedBytes() const { return _used_end; }

private:
    TwoWire *_wire = nullptr;
    uint8_t _addr = kDefaultAddr;
    Error _err = Error::Ok;
    uint32_t _used_end = 0;
};
