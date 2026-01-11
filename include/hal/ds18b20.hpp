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
#include "hal/bus/one_wire_bus.hpp"
#include <stdint.h>
#include <vector>

class Ds18b20
{
public:
    Ds18b20() = default;
    explicit Ds18b20(OneWireBus &bus) : _bus(&bus) {}

    bool begin(OneWireBus &bus)
    {
        _bus = &bus;
        _has_addr = false;
        return findFirst_();
    }

    bool readTempC(float &out_c)
    {
        if (!_has_addr && !findFirst_())
            return false;
        return readTempByAddr_(_addr, out_c);
    }

    bool readTempC(uint8_t addr[8], float &out_c)
    {
        if (!readTempByAddr_(addr, out_c))
            return false;
        for (uint8_t i = 0; i < 8; ++i)
            _addr[i] = addr[i];
        _has_addr = true;
        return true;
    }

    bool readTempC(const String &hex_serial, float &out_c)
    {
        uint8_t addr[8] = {};
        if (!parseHexSerial_(hex_serial, addr))
            return false;
        return readTempC(addr, out_c);
    }

    bool startConversion()
    {
        OneWireBus *bus = bus_();
        if (!bus)
            return false;
        if (!bus->reset())
            return false;
        bus->skip();
        bus->write(0x44);
        _last_conv_ms = millis();
        _has_conv = true;
        return true;
    }

    bool ready() const
    {
        if (!_has_conv)
            return false;
        return (millis() - _last_conv_ms) >= _conv_time_ms;
    }

    bool readTempCNoWait(float &out_c)
    {
        if (!_has_addr && !findFirst_())
            return false;
        return readTempByAddrNoWait_(_addr, out_c);
    }

    bool readTempCNoWait(uint8_t addr[8], float &out_c)
    {
        if (!readTempByAddrNoWait_(addr, out_c))
            return false;
        for (uint8_t i = 0; i < 8; ++i)
            _addr[i] = addr[i];
        _has_addr = true;
        return true;
    }

    bool readTempCNoWait(const String &hex_serial, float &out_c)
    {
        uint8_t addr[8] = {};
        if (!parseHexSerial_(hex_serial, addr))
            return false;
        return readTempCNoWait(addr, out_c);
    }

    void listSerials(std::vector<String> &out)
    {
        OneWireBus *bus = bus_();
        if (!bus)
            return;

        uint8_t addr[8] = {};
        bus->reset_search();
        while (bus->search(addr))
        {
            if (addr[0] != kFamily)
                continue;
            if (OneWireBus::crc8(addr, 7) != addr[7])
                continue;
            out.push_back(toString_(addr));
        }
        return;
    }

private:
    static constexpr uint8_t kFamily = 0x28;
    static constexpr uint16_t kDefaultConvMs = 750;

    OneWireBus *bus_() { return _bus; }

    bool findFirst_()
    {
        OneWireBus *bus = bus_();
        if (!bus)
            return false;

        bus->reset_search();
        while (bus->search(_addr))
        {
            if (_addr[0] != kFamily)
                continue;
            if (OneWireBus::crc8(_addr, 7) != _addr[7])
                continue;
            _has_addr = true;
            return true;
        }
        _has_addr = false;
        return false;
    }

    bool readTempByAddr_(const uint8_t addr[8], float &out_c)
    {
        OneWireBus *bus = bus_();
        if (!bus)
            return false;
        if (addr[0] != kFamily)
            return false;
        if (OneWireBus::crc8(addr, 7) != addr[7])
            return false;

        bus->reset();
        bus->select(addr);
        bus->write(0x44);
        delay(750);
        return readScratchpadTemp_(addr, out_c);
    }

    bool readTempByAddrNoWait_(const uint8_t addr[8], float &out_c)
    {
        return readScratchpadTemp_(addr, out_c);
    }

    bool readScratchpadTemp_(const uint8_t addr[8], float &out_c)
    {
        OneWireBus *bus = bus_();
        if (!bus)
            return false;
        if (addr[0] != kFamily)
            return false;
        if (OneWireBus::crc8(addr, 7) != addr[7])
            return false;

        uint8_t data[9] = {};
        bus->reset();
        bus->select(addr);
        bus->write(0xBE);
        for (uint8_t i = 0; i < 9; ++i)
            data[i] = bus->read();

        if (OneWireBus::crc8(data, 8) != data[8])
            return false;

        int16_t raw = (int16_t)((data[1] << 8) | data[0]);
        out_c = (float)raw / 16.0f;
        return true;
    }

    static bool parseHexSerial_(const String &hex, uint8_t out[8])
    {
        if (hex.length() != 16)
            return false;
        for (uint8_t i = 0; i < 8; ++i)
        {
            int hi = hexNibble_(hex[i * 2]);
            int lo = hexNibble_(hex[i * 2 + 1]);
            if (hi < 0 || lo < 0)
                return false;
            out[i] = (uint8_t)((hi << 4) | lo);
        }
        return true;
    }

    static String toString_(const uint8_t in[8])
    {
        char buf[17] = {};
        toHex_(in, buf);
        return String(buf);
    }

    static void toHex_(const uint8_t in[8], char out[17])
    {
        static const char kHex[] = "0123456789ABCDEF";
        for (uint8_t i = 0; i < 8; ++i)
        {
            out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
            out[i * 2 + 1] = kHex[in[i] & 0x0F];
        }
        out[16] = '\0';
    }

    static int hexNibble_(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        return -1;
    }

    OneWireBus *_bus = nullptr;
    uint8_t _addr[8] = {};
    bool _has_addr = false;
    uint32_t _last_conv_ms = 0;
    uint16_t _conv_time_ms = kDefaultConvMs;
    bool _has_conv = false;
};
