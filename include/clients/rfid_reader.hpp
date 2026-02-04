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

#include "boards/board_profile.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/gpio/portio.hpp"
#include "hal/pn532.hpp"

class RfidReader
{
public:
    using Uid = PN532::UID;
    using UidHandler = void (*)(void *ctx, const Uid &uid);

    bool begin(I2CManager &i2c, uint8_t addr7 = 0x24, int irqPin = -1, int rstPin = -1)
    {
        const uint8_t i2c_index = ActiveBoardProfile::RFID_I2C_INDEX;
        if (i2c_index >= ActiveBoardProfile::I2C_COUNT)
            return false;
        const auto cfg = ActiveBoardProfile::I2CS[i2c_index];
        TwoWire *wire = i2c.wirePtr(cfg.bus_num);
        if (!wire)
            return false;
        uint8_t sda_gpio = 0;
        uint8_t scl_gpio = 0;
        if (!portToGpio_(cfg.sda, sda_gpio) || !portToGpio_(cfg.scl, scl_gpio))
            return false;
        return begin(*wire, sda_gpio, scl_gpio, cfg.freq, addr7, irqPin, rstPin);
    }

    bool begin(TwoWire &wire, uint8_t sda_gpio, uint8_t scl_gpio, uint32_t freq = 400000,
               uint8_t addr7 = 0x24, int irqPin = -1, int rstPin = -1)
    {
        _ready = _pn.begin(wire, sda_gpio, scl_gpio, freq, addr7, irqPin, rstPin);
        _last_uid = Uid{};
        _last_uid_ms = 0;
        return _ready;
    }

    void task()
    {
        if (!_enabled || !_ready)
            return;
        Uid uid;
        if (_pn.readPassiveTargetID(uid, _read_timeout_ms) != PN532::Status::Ok)
            return;
        if (isRepeat_(uid))
            return;
        if (_uid_cb)
            _uid_cb(_uid_ctx, uid);
    }

    void setUidHandler(UidHandler cb, void *ctx)
    {
        _uid_cb = cb;
        _uid_ctx = ctx;
    }

    void setEnabled(bool enabled) { _enabled = enabled; }
    bool enabled() const { return _enabled; }
    bool ready() const { return _ready; }
    void setReadTimeoutMs(uint16_t ms) { _read_timeout_ms = ms; }
    bool lastUid(Uid &out) const
    {
        if (_last_uid.len == 0)
            return false;
        out = _last_uid;
        return true;
    }
    String lastUidString() const { return uidToString(_last_uid); }
    static String uidToString(const Uid &uid) { return PN532::uidToString(uid); }

private:
    static constexpr uint32_t kRepeatMs = 1500;

    PN532 _pn;
    bool _ready = false;
    bool _enabled = false;
    uint16_t _read_timeout_ms = 50;
    UidHandler _uid_cb = nullptr;
    void *_uid_ctx = nullptr;
    Uid _last_uid{};
    uint32_t _last_uid_ms = 0;

    static bool portToGpio_(uint8_t port, uint8_t &out_gpio)
    {
        if (port >= PortIO::PORT_COUNT)
            return false;
        const auto &desc = ActiveBoardProfile::PORTS[port];
        if (desc.backend != PortIO::Backend::Esp32)
            return false;
        if (desc.u.esp.gpio == 0xFF)
            return false;
        out_gpio = desc.u.esp.gpio;
        return true;
    }

    bool isRepeat_(const Uid &uid)
    {
        if (uid.len == 0 || uid.len > sizeof(_last_uid.bytes))
            return true;
        const uint32_t now = millis();
        if (uid.len == _last_uid.len &&
            memcmp(_last_uid.bytes, uid.bytes, uid.len) == 0)
        {
            if ((uint32_t)(now - _last_uid_ms) < kRepeatMs)
                return true;
        }
        _last_uid.len = uid.len;
        memcpy(_last_uid.bytes, uid.bytes, uid.len);
        _last_uid_ms = now;
        return false;
    }
};
