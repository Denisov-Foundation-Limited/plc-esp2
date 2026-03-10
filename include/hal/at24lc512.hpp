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

class At24lc512
{
public:
    using LockCallback = bool (*)(void *ctx, uint32_t timeout_ms);
    using UnlockCallback = void (*)(void *ctx);

    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        I2c
    };

    static constexpr uint8_t kDefaultAddr = 0x50;
    static constexpr uint32_t kSizeBytes = 65536;

    At24lc512() = default;
    explicit At24lc512(TwoWire &wire);

    bool begin(TwoWire &wire, uint8_t addr = kDefaultAddr);
    void setBusLockCallbacks(LockCallback lock_cb, UnlockCallback unlock_cb, void *ctx);
    bool read(uint16_t mem_addr, uint8_t *buf, uint16_t len);
    bool write(uint16_t mem_addr, const uint8_t *buf, uint16_t len);

    Error lastError() const;

    static constexpr uint32_t capacityBytes() { return kSizeBytes; }
    static constexpr uint32_t remainingBytes(uint32_t offset)
    {
        return (offset >= kSizeBytes) ? 0 : (kSizeBytes - offset);
    }
    uint32_t usedBytes() const;

private:
    TwoWire *_wire = nullptr;
    uint8_t _addr = kDefaultAddr;
    Error _err = Error::Ok;
    uint32_t _used_end = 0;
    LockCallback _lock_cb = nullptr;
    UnlockCallback _unlock_cb = nullptr;
    void *_lock_ctx = nullptr;
};
