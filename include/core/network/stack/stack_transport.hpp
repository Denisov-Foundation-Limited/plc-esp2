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
#include <IPAddress.h>
#include <stdint.h>

class StackTransport
{
public:
    enum class ExchangeKind : uint8_t
    {
        Event = 0,
        Request,
        Response
    };

    struct RouteMeta
    {
        ExchangeKind exchange_kind = ExchangeKind::Event;
        uint32_t request_id = 0;
        uint32_t reply_to = 0;
        bool expect_response = false;
    };

    struct Caps
    {
        bool full_duplex = false;
        bool can_push_async = false;
        bool requires_arbitration = false;
        bool supports_request_response = true;
        bool ordered_delivery = true;
    };

    enum class EventType : uint8_t
    {
        Connected = 0,
        Disconnected,
        TextMessage,
        BinaryMessage
    };

    struct Event
    {
        EventType type = EventType::Connected;
        uint8_t client_id = 0;
        IPAddress ip{};
        const uint8_t *data = nullptr;
        size_t size = 0;
    };

    using EventHandler = void (*)(void *ctx, const Event &event);

    virtual ~StackTransport() = default;

    virtual void setEventHandler(EventHandler cb, void *ctx) = 0;
    virtual bool begin(uint16_t port) = 0;
    virtual void loop() = 0;
    virtual void stop() = 0;
    virtual Caps caps() const = 0;

    virtual bool sendText(uint8_t client_id, const char *text) = 0;
    virtual bool sendBinary(uint8_t client_id, const uint8_t *data, size_t size) = 0;
    virtual bool disconnectClient(uint8_t client_id) = 0;
};
