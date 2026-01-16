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
#include <string.h>
#include <stdint.h>
#include <array>

enum class StackMsgType : uint8_t
{
    Hello = 1,
    Features = 2,
    Status = 3,
    CmdSet = 4,
    CmdGet = 5,
    Ack = 6,
    Err = 7
};

struct StackHeader
{
    uint8_t magic = 0xA5;
    uint8_t version = 1;
    uint8_t type = 0;
    uint16_t length = 0;
};

struct StackFrame
{
    uint8_t type = 0;
    const uint8_t *payload = nullptr;
    size_t payload_len = 0;
};

class StackCodec
{
public:
    static constexpr uint8_t kMagic = 0xA5;
    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kHeaderSize = 5;
    static constexpr size_t kCrcSize = 2;
    static constexpr size_t kMaxPayload = 1024;
    static constexpr size_t kMaxFrame = kHeaderSize + kMaxPayload + kCrcSize;

    using FrameHandler = void (*)(void *ctx, const StackFrame &frame);

    void clear()
    {
        _head = 0;
        _len = 0;
    }

    void feed(const uint8_t *data, size_t len, FrameHandler cb, void *ctx)
    {
        if (!data || len == 0)
            return;
        if (len > kMaxFrame)
        {
            clear();
            return;
        }
        if ((_len + len) > kMaxFrame)
            clear();
        for (size_t i = 0; i < len; ++i)
            push_(data[i]);

        while (true)
        {
            sync_();
            if (_len < kHeaderSize + kCrcSize)
                return;
            const uint16_t payload_len = readU16_(3);
            if (payload_len > kMaxPayload)
            {
                pop_(1);
                continue;
            }
            const size_t total = kHeaderSize + payload_len + kCrcSize;
            if (_len < total)
                return;
            const uint16_t crc_rx = readU16_(kHeaderSize + payload_len);
            const uint16_t crc_calc = crc16_(0, kHeaderSize + payload_len);
            if (crc_rx != crc_calc)
            {
                pop_(1);
                continue;
            }
            StackFrame frame;
            frame.type = at_(2);
            copyOut_(kHeaderSize, payload_len);
            frame.payload = _payload.data();
            frame.payload_len = payload_len;
            if (cb)
                cb(ctx, frame);
            pop_(total);
        }
    }

    static size_t encode(uint8_t type, const uint8_t *payload, size_t len, uint8_t *out, size_t out_cap)
    {
        if (len > kMaxPayload)
            return 0;
        const size_t total = kHeaderSize + len + kCrcSize;
        if (!out || out_cap < total)
            return 0;
        out[0] = kMagic;
        out[1] = kVersion;
        out[2] = type;
        writeU16_(out, 3, (uint16_t)len);
        if (payload && len > 0)
            memcpy(out + kHeaderSize, payload, len);
        const uint16_t crc = crc16Raw_(out, kHeaderSize + len);
        writeU16_(out, kHeaderSize + len, crc);
        return total;
    }

private:
    std::array<uint8_t, kMaxPayload> _payload = {};
    std::array<uint8_t, kMaxFrame> _buf = {};
    size_t _head = 0;
    size_t _len = 0;

    void sync_()
    {
        while (_len > 0 && at_(0) != kMagic)
            pop_(1);
        if (_len >= kHeaderSize)
        {
            if (at_(0) != kMagic || at_(1) != kVersion)
                pop_(1);
        }
    }

    uint16_t crc16_(size_t offset, size_t len) const
    {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; ++i)
        {
            crc ^= at_(offset + i);
            for (uint8_t b = 0; b < 8; ++b)
            {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xA001;
                else
                    crc >>= 1;
            }
        }
        return crc;
    }

    static uint16_t crc16Raw_(const uint8_t *data, size_t len)
    {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; ++i)
        {
            crc ^= data[i];
            for (uint8_t b = 0; b < 8; ++b)
            {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xA001;
                else
                    crc >>= 1;
            }
        }
        return crc;
    }

    uint16_t readU16_(size_t offset) const
    {
        return (uint16_t)at_(offset) | (uint16_t)at_(offset + 1) << 8;
    }

    static void writeU16_(uint8_t *out, size_t offset, uint16_t v)
    {
        out[offset] = (uint8_t)(v & 0xFF);
        out[offset + 1] = (uint8_t)((v >> 8) & 0xFF);
    }

    uint8_t at_(size_t offset) const
    {
        return _buf[(size_t)((_head + offset) % _buf.size())];
    }

    void push_(uint8_t v)
    {
        const size_t pos = (_head + _len) % _buf.size();
        _buf[pos] = v;
        ++_len;
    }

    void pop_(size_t n)
    {
        if (n >= _len)
        {
            clear();
            return;
        }
        _head = (_head + n) % _buf.size();
        _len -= n;
    }

    void copyOut_(size_t offset, size_t len)
    {
        if (len == 0)
            return;
        if (len > _payload.size())
            len = _payload.size();
        for (size_t i = 0; i < len; ++i)
            _payload[i] = at_(offset + i);
    }
};
