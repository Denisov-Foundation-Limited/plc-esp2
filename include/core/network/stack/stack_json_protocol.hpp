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
#include <ArduinoJson.h>
#include <stdint.h>

#include "core/network/stack/stack_device_registry.hpp"
#include "core/network/stack/stack_transport.hpp"

class StackJsonProtocol
{
public:
    enum class MessageKind : uint8_t
    {
        Unknown = 0,
        Auth,
        Route,
        Notify
    };

    struct AuthMessage
    {
        uint32_t node_id = 0;
        uint32_t caps = 0;
        uint16_t fw_version = 0;
        char name[StackDeviceRegistry::kNameLen] = {};
        char ip[StackDeviceRegistry::kIpLen] = {};
        char api_key[96] = {};
    };

    struct RouteMessage
    {
        uint32_t source_node = 0;
        uint32_t target_node = 0;
        StackTransport::RouteMeta meta{};
        char feature[32] = {};
        char action[32] = {};
        String payload;
    };

    struct NotifyMessage
    {
        uint32_t source_node = 0;
        char level[16] = {};
        char feature[32] = {};
        char code[32] = {};
        String message;
        String payload;
    };

    static MessageKind detectKind(const uint8_t *data, size_t size);
    static bool parseAuth(const uint8_t *data, size_t size, AuthMessage &out);
    static bool parseRoute(const uint8_t *data, size_t size, RouteMessage &out);
    static bool parseNotify(const uint8_t *data, size_t size, NotifyMessage &out);
    static String makeRoute(uint32_t source_node, uint32_t target_node, const char *feature, const char *action,
                            const JsonDocument *payload = nullptr, const StackTransport::RouteMeta *meta = nullptr);
    static String makeNotify(uint32_t source_node, const char *level, const char *feature, const char *code,
                             const char *message = nullptr, const JsonDocument *payload = nullptr);
    static String makeOk(const char *message = nullptr);
    static String makeError(const char *message);
};
