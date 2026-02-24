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

#include "hal/ds18b20.hpp"

#include <cstring>

Ds18b20::Ds18b20(OneWireBus &bus)
    : _bus(&bus)
{
}

bool Ds18b20::begin(OneWireBus &bus)
{
    _bus = &bus;
    _has_addr = false;
    return findFirst_();
}

bool Ds18b20::readTempC(float &out_c)
{
    if (!_has_addr && !findFirst_())
        return false;
    return readTempByAddr_(_addr, out_c);
}

bool Ds18b20::readTempC(uint8_t addr[8], float &out_c)
{
    if (!readTempByAddr_(addr, out_c))
        return false;
    for (uint8_t i = 0; i < 8; ++i)
        _addr[i] = addr[i];
    _has_addr = true;
    return true;
}

bool Ds18b20::readTempC(const String &hex_serial, float &out_c)
{
    uint8_t addr[8] = {};
    if (!parseHexSerial_(hex_serial, addr))
        return false;
    return readTempC(addr, out_c);
}

bool Ds18b20::readTempC(const char *hex_serial, float &out_c)
{
    uint8_t addr[8] = {};
    if (!parseHexSerial_(hex_serial, addr))
        return false;
    return readTempC(addr, out_c);
}

bool Ds18b20::startConversion()
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

bool Ds18b20::ready() const
{
    if (!_has_conv)
        return false;
    return (millis() - _last_conv_ms) >= _conv_time_ms;
}

bool Ds18b20::readTempCNoWait(float &out_c)
{
    if (!_has_addr && !findFirst_())
        return false;
    return readTempByAddrNoWait_(_addr, out_c);
}

bool Ds18b20::readTempCNoWait(uint8_t addr[8], float &out_c)
{
    if (!readTempByAddrNoWait_(addr, out_c))
        return false;
    for (uint8_t i = 0; i < 8; ++i)
        _addr[i] = addr[i];
    _has_addr = true;
    return true;
}

bool Ds18b20::readTempCNoWait(const String &hex_serial, float &out_c)
{
    uint8_t addr[8] = {};
    if (!parseHexSerial_(hex_serial, addr))
        return false;
    return readTempCNoWait(addr, out_c);
}

bool Ds18b20::readTempCNoWait(const char *hex_serial, float &out_c)
{
    uint8_t addr[8] = {};
    if (!parseHexSerial_(hex_serial, addr))
        return false;
    return readTempCNoWait(addr, out_c);
}

void Ds18b20::listSerials(std::vector<String> &out)
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
}

void Ds18b20::listSerials(char out[][17], size_t max, size_t &count)
{
    count = 0;
    OneWireBus *bus = bus_();
    if (!bus || !out || max == 0)
        return;

    uint8_t addr[8] = {};
    bus->reset_search();
    while (bus->search(addr))
    {
        if (addr[0] != kFamily)
            continue;
        if (OneWireBus::crc8(addr, 7) != addr[7])
            continue;
        if (count >= max)
            return;
        toHex_(addr, out[count]);
        ++count;
    }
}

OneWireBus *Ds18b20::bus_()
{
    return _bus;
}

bool Ds18b20::findFirst_()
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

bool Ds18b20::readTempByAddr_(const uint8_t addr[8], float &out_c)
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

bool Ds18b20::readTempByAddrNoWait_(const uint8_t addr[8], float &out_c)
{
    return readScratchpadTemp_(addr, out_c);
}

bool Ds18b20::readScratchpadTemp_(const uint8_t addr[8], float &out_c)
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

bool Ds18b20::parseHexSerial_(const String &hex, uint8_t out[8])
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

bool Ds18b20::parseHexSerial_(const char *hex, uint8_t out[8])
{
    if (!hex || std::strlen(hex) != 16)
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

String Ds18b20::toString_(const uint8_t in[8])
{
    char buf[17] = {};
    toHex_(in, buf);
    return String(buf);
}

void Ds18b20::toHex_(const uint8_t in[8], char out[17])
{
    static const char kHex[] = "0123456789ABCDEF";
    for (uint8_t i = 0; i < 8; ++i)
    {
        out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[in[i] & 0x0F];
    }
    out[16] = '\0';
}

int Ds18b20::hexNibble_(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    return -1;
}
