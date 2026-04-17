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
    if (!data || size < 4)
        return FrameKind::Unknown;
    if (readU16_(data) != kMagic)
        return FrameKind::Unknown;
    if (data[2] != kVersion)
        return FrameKind::Unknown;
    switch ((FrameKind)data[3])
    {
    case FrameKind::Route:
    case FrameKind::Auth:
    case FrameKind::AuthReply:
    case FrameKind::Notify:
        return (FrameKind)data[3];
    default:
        return FrameKind::Unknown;
    }
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

    const size_t total = kRouteHeaderSize + (size_t)feature_len + (size_t)action_len + (size_t)payload_len;
    if (size < total || feature_len == 0 || action_len == 0)
        return false;

    const uint8_t *p = data + kRouteHeaderSize;
    if (!copyToken_(out.feature, sizeof(out.feature), p, feature_len))
        return false;
    p += feature_len;
    if (!copyToken_(out.action, sizeof(out.action), p, action_len))
        return false;
    p += action_len;

    out.payload = payload_len ? p : nullptr;
    out.payload_size = payload_len;
    return true;
}

bool StackBinaryProtocol::parseAuth(const uint8_t *data, size_t size, AuthFrame &out)
{
    if (detectKind(data, size) != FrameKind::Auth || size < kAuthHeaderSize)
        return false;

    out = AuthFrame{};
    const uint8_t name_len = data[4];
    const uint8_t ip_len = data[5];
    const uint16_t api_key_len = readU16_(data + 6);
    out.node_id = readU32_(data + 8);
    out.caps = readU32_(data + 12);
    out.fw_version = readU16_(data + 16);
    const size_t total = kAuthHeaderSize + (size_t)name_len + (size_t)ip_len + (size_t)api_key_len;
    if (size < total || name_len == 0)
        return false;

    const uint8_t *p = data + kAuthHeaderSize;
    if (!copyToken_(out.name, sizeof(out.name), p, name_len))
        return false;
    p += name_len;
    if (ip_len && !copyToken_(out.ip, sizeof(out.ip), p, ip_len))
        return false;
    p += ip_len;
    if (api_key_len && !copyToken_(out.api_key, sizeof(out.api_key), p, api_key_len))
        return false;
    return out.node_id != 0 && out.name[0] != '\0';
}

bool StackBinaryProtocol::parseAuthReply(const uint8_t *data, size_t size, AuthReplyFrame &out)
{
    if (detectKind(data, size) != FrameKind::AuthReply || size < kAuthReplyHeaderSize)
        return false;

    out = AuthReplyFrame{};
    out.ok = data[4] != 0;
    const uint16_t message_len = readU16_(data + 5);
    const size_t total = kAuthReplyHeaderSize + (size_t)message_len;
    if (size < total)
        return false;
    if (message_len && !copyToken_(out.message, sizeof(out.message), data + kAuthReplyHeaderSize, message_len))
        return false;
    return true;
}

bool StackBinaryProtocol::parseNotify(const uint8_t *data, size_t size, NotifyFrame &out)
{
    if (detectKind(data, size) != FrameKind::Notify || size < kNotifyHeaderSize)
        return false;

    out = NotifyFrame{};
    const uint8_t level_len = data[4];
    const uint8_t feature_len = data[5];
    const uint8_t code_len = data[6];
    const uint8_t reserved = data[7];
    (void)reserved;
    const uint16_t message_len = readU16_(data + 8);
    const uint16_t payload_len = readU16_(data + 10);
    out.source_node = readU32_(data + 12);
    const size_t total = kNotifyHeaderSize + (size_t)level_len + (size_t)feature_len + (size_t)code_len +
                         (size_t)message_len + (size_t)payload_len;
    if (size < total || level_len == 0 || feature_len == 0)
        return false;

    const uint8_t *p = data + kNotifyHeaderSize;
    if (!copyToken_(out.level, sizeof(out.level), p, level_len))
        return false;
    p += level_len;
    if (!copyToken_(out.feature, sizeof(out.feature), p, feature_len))
        return false;
    p += feature_len;
    if (code_len && !copyToken_(out.code, sizeof(out.code), p, code_len))
        return false;
    p += code_len;
    out.message = message_len ? p : nullptr;
    out.message_size = message_len;
    p += message_len;
    out.payload = payload_len ? p : nullptr;
    out.payload_size = payload_len;
    return out.source_node != 0;
}

bool StackBinaryProtocol::encodeRoute(uint32_t source_node, uint32_t target_node, const char *feature, const char *action,
                                      const uint8_t *payload, size_t payload_size, uint8_t *out, size_t cap, size_t &used,
                                      const StackTransport::RouteMeta *meta)
{
    used = 0;
    if (!out || !feature || !feature[0] || !action || !action[0])
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

    uint8_t *p = out + kRouteHeaderSize;
    memcpy(p, feature, feature_len);
    p += feature_len;
    memcpy(p, action, action_len);
    p += action_len;
    if (payload && payload_size)
        memcpy(p, payload, payload_size);
    used = total;
    return true;
}

bool StackBinaryProtocol::encodeAuth(uint32_t node_id, uint32_t caps, uint16_t fw_version, const char *name, const char *ip,
                                     const char *api_key, uint8_t *out, size_t cap, size_t &used)
{
    used = 0;
    if (!out || !name || !name[0] || node_id == 0)
        return false;
    const size_t name_len = strnlen(name, kMaxNameLen + 1);
    const size_t ip_len = ip ? strnlen(ip, StackDeviceRegistry::kIpLen) : 0;
    const size_t api_key_len = api_key ? strnlen(api_key, 96) : 0;
    if (name_len == 0 || name_len > kMaxNameLen || ip_len >= StackDeviceRegistry::kIpLen || api_key_len >= 96)
        return false;
    const size_t total = encodedAuthSize(name, ip, api_key);
    if (cap < total)
        return false;

    writeU16_(out, kMagic);
    out[2] = kVersion;
    out[3] = (uint8_t)FrameKind::Auth;
    out[4] = (uint8_t)name_len;
    out[5] = (uint8_t)ip_len;
    writeU16_(out + 6, (uint16_t)api_key_len);
    writeU32_(out + 8, node_id);
    writeU32_(out + 12, caps);
    writeU16_(out + 16, fw_version);
    uint8_t *p = out + kAuthHeaderSize;
    memcpy(p, name, name_len);
    p += name_len;
    if (ip_len)
    {
        memcpy(p, ip, ip_len);
        p += ip_len;
    }
    if (api_key_len)
        memcpy(p, api_key, api_key_len);
    used = total;
    return true;
}

bool StackBinaryProtocol::encodeAuthReply(bool ok, const char *message, uint8_t *out, size_t cap, size_t &used)
{
    used = 0;
    if (!out)
        return false;
    const size_t message_len = message ? strnlen(message, sizeof(AuthReplyFrame::message)) : 0;
    if (message_len >= sizeof(AuthReplyFrame::message))
        return false;
    const size_t total = encodedAuthReplySize(message);
    if (cap < total)
        return false;
    writeU16_(out, kMagic);
    out[2] = kVersion;
    out[3] = (uint8_t)FrameKind::AuthReply;
    out[4] = ok ? 1u : 0u;
    writeU16_(out + 5, (uint16_t)message_len);
    if (message_len)
        memcpy(out + kAuthReplyHeaderSize, message, message_len);
    used = total;
    return true;
}

bool StackBinaryProtocol::encodeNotify(uint32_t source_node, const char *level, const char *feature, const char *code,
                                       const uint8_t *message, size_t message_size, const uint8_t *payload,
                                       size_t payload_size, uint8_t *out, size_t cap, size_t &used)
{
    used = 0;
    if (!out || source_node == 0 || !level || !level[0] || !feature || !feature[0])
        return false;
    const size_t level_len = strnlen(level, sizeof(NotifyFrame::level));
    const size_t feature_len = strnlen(feature, sizeof(NotifyFrame::feature));
    const size_t code_len = code ? strnlen(code, sizeof(NotifyFrame::code)) : 0;
    const size_t wire_message_size = message_size > kNotifyMessageMax ? kNotifyMessageMax : message_size;
    if (level_len == 0 || level_len >= sizeof(NotifyFrame::level) || feature_len == 0 ||
        feature_len >= sizeof(NotifyFrame::feature) || code_len >= sizeof(NotifyFrame::code) || payload_size != 0u)
        return false;
    const size_t total = encodedNotifySize(level, feature, code, wire_message_size, 0u);
    if (cap < total)
        return false;

    writeU16_(out, kMagic);
    out[2] = kVersion;
    out[3] = (uint8_t)FrameKind::Notify;
    out[4] = (uint8_t)level_len;
    out[5] = (uint8_t)feature_len;
    out[6] = (uint8_t)code_len;
    out[7] = 0u;
    writeU16_(out + 8, (uint16_t)wire_message_size);
    writeU16_(out + 10, 0u);
    writeU32_(out + 12, source_node);
    writeU32_(out + 16, 0u);
    uint8_t *p = out + kNotifyHeaderSize;
    memcpy(p, level, level_len);
    p += level_len;
    memcpy(p, feature, feature_len);
    p += feature_len;
    if (code_len)
    {
        memcpy(p, code, code_len);
        p += code_len;
    }
    if (message && wire_message_size)
    {
        memcpy(p, message, wire_message_size);
        p += wire_message_size;
    }
    used = total;
    return true;
}

size_t StackBinaryProtocol::encodedRouteSize(const char *feature, const char *action, size_t payload_size)
{
    const size_t feature_len = feature ? strnlen(feature, kMaxNameLen + 1) : 0;
    const size_t action_len = action ? strnlen(action, kMaxNameLen + 1) : 0;
    return kRouteHeaderSize + feature_len + action_len + payload_size;
}

size_t StackBinaryProtocol::encodedAuthSize(const char *name, const char *ip, const char *api_key)
{
    const size_t name_len = name ? strnlen(name, kMaxNameLen + 1) : 0;
    const size_t ip_len = ip ? strnlen(ip, StackDeviceRegistry::kIpLen) : 0;
    const size_t api_key_len = api_key ? strnlen(api_key, 96) : 0;
    return kAuthHeaderSize + name_len + ip_len + api_key_len;
}

size_t StackBinaryProtocol::encodedAuthReplySize(const char *message)
{
    const size_t message_len = message ? strnlen(message, sizeof(AuthReplyFrame::message)) : 0;
    return kAuthReplyHeaderSize + message_len;
}

size_t StackBinaryProtocol::encodedNotifySize(const char *level, const char *feature, const char *code, size_t message_size,
                                              size_t payload_size)
{
    const size_t level_len = level ? strnlen(level, sizeof(NotifyFrame::level)) : 0;
    const size_t feature_len = feature ? strnlen(feature, sizeof(NotifyFrame::feature)) : 0;
    const size_t code_len = code ? strnlen(code, sizeof(NotifyFrame::code)) : 0;
    const size_t wire_message_size = message_size > kNotifyMessageMax ? kNotifyMessageMax : message_size;
    (void)payload_size;
    return kNotifyHeaderSize + level_len + feature_len + code_len + wire_message_size;
}

bool StackBinaryProtocol::copyToken_(char *dst, size_t cap, const uint8_t *src, size_t len)
{
    if (!dst || cap == 0 || !src || len == 0 || len >= cap)
        return false;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return true;
}
