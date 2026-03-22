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

#include "core/network/stack/stack_transport.hpp"

class StackBinaryProtocol
{
public:
    enum class FrameKind : uint8_t
    {
        Unknown = 0,
        Route = 1
    };

    static constexpr uint16_t kMagic = 0x5342; // 'SB'
    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kMaxNameLen = 31;
    static constexpr uint8_t kFlagExpectResponse = 0x01;
    static constexpr size_t kHeaderSize = 26;

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

    static FrameKind detectKind(const uint8_t *data, size_t size);
    static bool parseRoute(const uint8_t *data, size_t size, RouteFrame &out);
    static bool encodeRoute(uint32_t source_node, uint32_t target_node, const char *feature, const char *action,
                            const uint8_t *payload, size_t payload_size, uint8_t *out, size_t cap, size_t &used,
                            const StackTransport::RouteMeta *meta = nullptr);
    static size_t encodedRouteSize(const char *feature, const char *action, size_t payload_size);

private:
    static bool copyToken_(char *dst, size_t cap, const uint8_t *src, size_t len);
};
