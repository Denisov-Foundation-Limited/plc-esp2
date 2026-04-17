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

#include "core/network/stack/stack_device_registry.hpp"
#include "core/network/stack/stack_transport.hpp"

class StackBinaryProtocol
{
public:
    enum class FrameKind : uint8_t
    {
        Unknown = 0,
        Route = 1,
        Auth = 2,
        AuthReply = 3,
        Notify = 4
    };

    static constexpr uint16_t kMagic = 0x5342; // 'SB'
    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kMaxNameLen = 31;
    static constexpr uint8_t kFlagExpectResponse = 0x01;
    static constexpr size_t kRouteHeaderSize = 26;
    static constexpr size_t kAuthHeaderSize = 18;
    static constexpr size_t kAuthReplyHeaderSize = 7;
    static constexpr size_t kNotifyHeaderSize = 20;
    static constexpr size_t kNotifyMessageMax = 128;

    struct RouteFrame
    {
        uint32_t source_node = 0;
        uint32_t target_node = 0;
        StackTransport::RouteMeta meta{};
        char feature[kMaxNameLen + 1] = {};
        char action[kMaxNameLen + 1] = {};
        const uint8_t *payload = nullptr;
        size_t payload_size = 0;
    };

    struct AuthFrame
    {
        uint32_t node_id = 0;
        uint32_t caps = 0;
        uint16_t fw_version = 0;
        char name[kMaxNameLen + 1] = {};
        char ip[StackDeviceRegistry::kIpLen] = {};
        char api_key[96] = {};
    };

    struct AuthReplyFrame
    {
        bool ok = false;
        char message[64] = {};
    };

    struct NotifyFrame
    {
        uint32_t source_node = 0;
        char level[16] = {};
        char feature[32] = {};
        char code[32] = {};
        const uint8_t *message = nullptr;
        size_t message_size = 0;
        const uint8_t *payload = nullptr;
        size_t payload_size = 0;
    };

    static FrameKind detectKind(const uint8_t *data, size_t size);
    static bool parseRoute(const uint8_t *data, size_t size, RouteFrame &out);
    static bool parseAuth(const uint8_t *data, size_t size, AuthFrame &out);
    static bool parseAuthReply(const uint8_t *data, size_t size, AuthReplyFrame &out);
    static bool parseNotify(const uint8_t *data, size_t size, NotifyFrame &out);
    static bool encodeRoute(uint32_t source_node, uint32_t target_node, const char *feature, const char *action,
                            const uint8_t *payload, size_t payload_size, uint8_t *out, size_t cap, size_t &used,
                            const StackTransport::RouteMeta *meta = nullptr);
    static bool encodeAuth(uint32_t node_id, uint32_t caps, uint16_t fw_version, const char *name, const char *ip,
                           const char *api_key, uint8_t *out, size_t cap, size_t &used);
    static bool encodeAuthReply(bool ok, const char *message, uint8_t *out, size_t cap, size_t &used);
    static bool encodeNotify(uint32_t source_node, const char *level, const char *feature, const char *code,
                             const uint8_t *message, size_t message_size, const uint8_t *payload, size_t payload_size,
                             uint8_t *out, size_t cap, size_t &used);
    static size_t encodedRouteSize(const char *feature, const char *action, size_t payload_size);
    static size_t encodedAuthSize(const char *name, const char *ip, const char *api_key);
    static size_t encodedAuthReplySize(const char *message);
    static size_t encodedNotifySize(const char *level, const char *feature, const char *code, size_t message_size,
                                    size_t payload_size);

private:
    static bool copyToken_(char *dst, size_t cap, const uint8_t *src, size_t len);
};
