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

class IButton
{
public:
    IButton() = default;
    explicit IButton(OneWireBus &bus) : _bus(&bus) {}

    bool begin(OneWireBus &bus)
    {
        _bus = &bus;
        _bus->reset_search();
        return true;
    }

    bool readSerial(uint8_t out[8])
    {
        OneWireBus *bus = bus_();
        if (!bus)
            return false;

        bus->reset_search();
        if (!bus->search(out))
            return false;

        if (OneWireBus::crc8(out, 7) != out[7])
            return false;

        return true;
    }

    bool readSerial(uint64_t &out)
    {
        uint8_t buf[8] = {};
        if (!readSerial(buf))
            return false;

        uint64_t v = 0;
        for (uint8_t i = 0; i < 8; ++i)
            v |= (uint64_t)buf[i] << (8 * i);
        out = v;
        return true;
    }

    static void toHex(const uint8_t in[8], char out[17])
    {
        static const char kHex[] = "0123456789ABCDEF";
        for (uint8_t i = 0; i < 8; ++i)
        {
            out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
            out[i * 2 + 1] = kHex[in[i] & 0x0F];
        }
        out[16] = '\0';
    }

    static String toString(const uint8_t in[8])
    {
        char buf[17] = {};
        toHex(in, buf);
        return String(buf);
    }

    bool readSerialHex(char out[17])
    {
        uint8_t buf[8] = {};
        if (!readSerial(buf))
            return false;
        toHex(buf, out);
        return true;
    }

    String readSerialString()
    {
        uint8_t buf[8] = {};
        if (!readSerial(buf))
            return String();
        return toString(buf);
    }

private:
    OneWireBus *bus_() { return _bus; }

    OneWireBus *_bus = nullptr;
};
