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

#include "core/network/stack/stack_rs485_protocol.hpp"

namespace
{
uint16_t readU16Le_(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

void writeU16Le_(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}
} // namespace

size_t StackRs485Protocol::encodedSize(size_t payload_size)
{
    return kHeaderSize + payload_size;
}

bool StackRs485Protocol::encode(uint8_t flags, uint8_t msg_type, const uint8_t *payload, size_t payload_size,
                                uint8_t *out, size_t cap, size_t &used)
{
    used = 0;
    if (!out || payload_size > 0xFFu)
        return false;
    const size_t total = encodedSize(payload_size);
    if (cap < total)
        return false;

    out[0] = kPreamble0;
    out[1] = kPreamble1;
    out[2] = kVersion;
    out[3] = flags;
    out[4] = msg_type;
    out[5] = (uint8_t)payload_size;
    if (payload && payload_size)
        memcpy(out + 6, payload, payload_size);
    const uint16_t crc = crc16_(out, 6 + payload_size);
    writeU16Le_(out + 6 + payload_size, crc);
    used = total;
    return true;
}

bool StackRs485Protocol::decode(const uint8_t *data, size_t size, FrameView &out)
{
    if (!data || size < kHeaderSize)
        return false;
    if (data[0] != kPreamble0 || data[1] != kPreamble1 || data[2] != kVersion)
        return false;
    const size_t payload_size = data[5];
    if (size < encodedSize(payload_size))
        return false;
    const uint16_t expected_crc = readU16Le_(data + 6 + payload_size);
    const uint16_t actual_crc = crc16_(data, 6 + payload_size);
    if (expected_crc != actual_crc)
        return false;

    out.flags = data[3];
    out.msg_type = data[4];
    out.payload = payload_size ? (data + 6) : nullptr;
    out.payload_size = payload_size;
    return true;
}

uint16_t StackRs485Protocol::crc16_(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b)
            crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
    }
    return crc;
}
