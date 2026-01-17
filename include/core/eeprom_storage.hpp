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
#include <stdint.h>

#include "hal/at24lc512.hpp"

class EepromStorage
{
public:
    static constexpr uint16_t kDefaultBase = 0;
    static constexpr uint16_t kSocketCount = 72;
    static constexpr uint16_t kSocketMaskBytes = (kSocketCount + 7) / 8;

    struct SocketSnapshot
    {
        uint8_t enabled_mask[kSocketMaskBytes] = {};
        uint8_t state_mask[kSocketMaskBytes] = {};
    };

    EepromStorage() = default;
    explicit EepromStorage(At24lc512 &eeprom) : _eeprom(&eeprom) {}

    void bind(At24lc512 &eeprom) { _eeprom = &eeprom; }
    void setBase(uint16_t base) { _base = base; }
    void setReady(bool ready) { _ready = ready; }
    bool isReady() const { return _ready; }

    bool saveSockets(const SocketSnapshot &snap)
    {
        if (!_eeprom)
            return false;
        StorageHeader hdr{};
        hdr.magic = kMagic;
        hdr.version = kVersion;
        hdr.socket_count = kSocketCount;
        const uint16_t base = _base;
        if (!_eeprom->write(base, reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->write(off, snap.enabled_mask, kSocketMaskBytes))
            return false;
        return _eeprom->write(off + kSocketMaskBytes, snap.state_mask, kSocketMaskBytes);
    }

    bool loadSockets(SocketSnapshot &out)
    {
        if (!_eeprom)
            return false;
        StorageHeader hdr{};
        const uint16_t base = _base;
        if (!_eeprom->read(base, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        if (hdr.magic != kMagic || hdr.version != kVersion || hdr.socket_count != kSocketCount)
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->read(off, out.enabled_mask, kSocketMaskBytes))
            return false;
        return _eeprom->read(off + kSocketMaskBytes, out.state_mask, kSocketMaskBytes);
    }

private:
    struct StorageHeader
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t socket_count = 0;
    };

    static constexpr uint32_t kMagic = 0x45535031u; // "ESP1"
    static constexpr uint16_t kVersion = 1;

    At24lc512 *_eeprom = nullptr;
    uint16_t _base = kDefaultBase;
    bool _ready = false;
};
