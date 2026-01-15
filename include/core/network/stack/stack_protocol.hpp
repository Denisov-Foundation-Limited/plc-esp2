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
#include <vector>

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
    std::vector<uint8_t> payload;
};

class StackCodec
{
public:
    static constexpr uint8_t kMagic = 0xA5;
    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kHeaderSize = 5;
    static constexpr size_t kCrcSize = 2;
    static constexpr size_t kMaxPayload = 1024;

    using FrameHandler = void (*)(void *ctx, const StackFrame &frame);

    void clear() { _buf.clear(); }

    void feed(const uint8_t *data, size_t len, FrameHandler cb, void *ctx)
    {
        if (!data || len == 0)
            return;
        _buf.insert(_buf.end(), data, data + len);

        while (true)
        {
            sync_();
            if (_buf.size() < kHeaderSize + kCrcSize)
                return;
            const uint16_t payload_len = readU16_(&_buf[3]);
            if (payload_len > kMaxPayload)
            {
                _buf.erase(_buf.begin());
                continue;
            }
            const size_t total = kHeaderSize + payload_len + kCrcSize;
            if (_buf.size() < total)
                return;
            const uint16_t crc_rx = readU16_(&_buf[kHeaderSize + payload_len]);
            const uint16_t crc_calc = crc16_(_buf.data(), kHeaderSize + payload_len);
            if (crc_rx != crc_calc)
            {
                _buf.erase(_buf.begin());
                continue;
            }
            StackFrame frame;
            frame.type = _buf[2];
            frame.payload.assign(_buf.begin() + (int)kHeaderSize, _buf.begin() + (int)(kHeaderSize + payload_len));
            if (cb)
                cb(ctx, frame);
            _buf.erase(_buf.begin(), _buf.begin() + (int)total);
        }
    }

    static void encode(uint8_t type, const uint8_t *payload, size_t len, std::vector<uint8_t> &out)
    {
        if (len > kMaxPayload)
            return;
        out.clear();
        out.reserve(kHeaderSize + len + kCrcSize);
        out.push_back(kMagic);
        out.push_back(kVersion);
        out.push_back(type);
        writeU16_(out, (uint16_t)len);
        if (payload && len > 0)
            out.insert(out.end(), payload, payload + len);
        const uint16_t crc = crc16_(out.data(), kHeaderSize + len);
        writeU16_(out, crc);
    }

private:
    std::vector<uint8_t> _buf;

    void sync_()
    {
        while (!_buf.empty() && _buf[0] != kMagic)
            _buf.erase(_buf.begin());
        if (_buf.size() >= kHeaderSize)
        {
            if (_buf[0] != kMagic || _buf[1] != kVersion)
                _buf.erase(_buf.begin());
        }
    }

    static uint16_t crc16_(const uint8_t *data, size_t len)
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

    static uint16_t readU16_(const uint8_t *p)
    {
        return (uint16_t)p[0] | (uint16_t)p[1] << 8;
    }

    static void writeU16_(std::vector<uint8_t> &out, uint16_t v)
    {
        out.push_back((uint8_t)(v & 0xFF));
        out.push_back((uint8_t)((v >> 8) & 0xFF));
    }
};
