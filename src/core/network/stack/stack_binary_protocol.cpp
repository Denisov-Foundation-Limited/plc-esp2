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

#include "core/network/stack/stack_binary_protocol.hpp"

#include <string.h>

namespace
{
uint16_t readU16_(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint32_t readU32_(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void writeU16_(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

void writeU32_(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

StackTransport::ExchangeKind exchangeKindFromByte_(uint8_t v)
{
    switch (v)
    {
    case 1:
        return StackTransport::ExchangeKind::Request;
    case 2:
        return StackTransport::ExchangeKind::Response;
    default:
        return StackTransport::ExchangeKind::Event;
    }
}

uint8_t exchangeKindToByte_(StackTransport::ExchangeKind kind)
{
    switch (kind)
    {
    case StackTransport::ExchangeKind::Request:
        return 1;
    case StackTransport::ExchangeKind::Response:
        return 2;
    case StackTransport::ExchangeKind::Event:
    default:
        return 0;
    }
}
} // namespace

StackBinaryProtocol::FrameKind StackBinaryProtocol::detectKind(const uint8_t *data, size_t size)
{
    if (!data || size < kHeaderSize)
        return FrameKind::Unknown;
    if (readU16_(data) != kMagic)
        return FrameKind::Unknown;
    if (data[2] != kVersion)
        return FrameKind::Unknown;
    if (data[3] == (uint8_t)FrameKind::Route)
        return FrameKind::Route;
    return FrameKind::Unknown;
}

bool StackBinaryProtocol::parseRoute(const uint8_t *data, size_t size, RouteFrame &out)
{
    if (detectKind(data, size) != FrameKind::Route)
        return false;

    out.meta.exchange_kind = exchangeKindFromByte_(data[4]);
    const uint8_t flags = data[5];
    const uint8_t feature_len = data[6];
    const uint8_t action_len = data[7];
    const uint16_t payload_len = readU16_(data + 8);
    out.source_node = readU32_(data + 10);
    out.target_node = readU32_(data + 14);
    out.meta.request_id = readU32_(data + 18);
    out.meta.reply_to = readU32_(data + 22);
    out.meta.expect_response = (flags & kFlagExpectResponse) != 0;

    const size_t total = kHeaderSize + (size_t)feature_len + (size_t)action_len + (size_t)payload_len;
    if (size < total || feature_len == 0 || action_len == 0)
        return false;

    const uint8_t *p = data + kHeaderSize;
    if (!copyToken_(out.feature, sizeof(out.feature), p, feature_len))
        return false;
    p += feature_len;
    if (!copyToken_(out.action, sizeof(out.action), p, action_len))
        return false;
    p += action_len;

    out.payload = payload_len ? p : nullptr;
    out.payload_size = payload_len;
    return out.target_node != 0;
}

bool StackBinaryProtocol::encodeRoute(uint32_t source_node, uint32_t target_node, const char *feature, const char *action,
                                      const uint8_t *payload, size_t payload_size, uint8_t *out, size_t cap, size_t &used,
                                      const StackTransport::RouteMeta *meta)
{
    used = 0;
    if (!out || !feature || !feature[0] || !action || !action[0] || target_node == 0)
        return false;

    const size_t feature_len = strnlen(feature, kMaxNameLen + 1);
    const size_t action_len = strnlen(action, kMaxNameLen + 1);
    if (feature_len == 0 || feature_len > kMaxNameLen || action_len == 0 || action_len > kMaxNameLen)
        return false;

    const size_t total = encodedRouteSize(feature, action, payload_size);
    if (cap < total || payload_size > 0xFFFFu)
        return false;

    writeU16_(out, kMagic);
    out[2] = kVersion;
    out[3] = (uint8_t)FrameKind::Route;
    out[4] = exchangeKindToByte_(meta ? meta->exchange_kind : StackTransport::ExchangeKind::Event);
    out[5] = (meta && meta->expect_response) ? kFlagExpectResponse : 0u;
    out[6] = (uint8_t)feature_len;
    out[7] = (uint8_t)action_len;
    writeU16_(out + 8, (uint16_t)payload_size);
    writeU32_(out + 10, source_node);
    writeU32_(out + 14, target_node);
    writeU32_(out + 18, meta ? meta->request_id : 0u);
    writeU32_(out + 22, meta ? meta->reply_to : 0u);

    uint8_t *p = out + kHeaderSize;
    memcpy(p, feature, feature_len);
    p += feature_len;
    memcpy(p, action, action_len);
    p += action_len;
    if (payload && payload_size)
        memcpy(p, payload, payload_size);
    used = total;
    return true;
}

size_t StackBinaryProtocol::encodedRouteSize(const char *feature, const char *action, size_t payload_size)
{
    const size_t feature_len = feature ? strnlen(feature, kMaxNameLen + 1) : 0;
    const size_t action_len = action ? strnlen(action, kMaxNameLen + 1) : 0;
    return kHeaderSize + feature_len + action_len + payload_size;
}

bool StackBinaryProtocol::copyToken_(char *dst, size_t cap, const uint8_t *src, size_t len)
{
    if (!dst || cap == 0 || !src || len == 0 || len >= cap)
        return false;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return true;
}
