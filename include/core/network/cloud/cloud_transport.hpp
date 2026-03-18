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

enum class CloudTransportKind : uint8_t
{
    WebSocket = 0,
    Http = 1
};

class CloudTransport
{
public:
    enum class Event : uint8_t
    {
        Connected = 0,
        Disconnected,
        Error
    };

    using MessageHandler = void (*)(void *ctx, const uint8_t *payload, size_t len);
    using EventHandler = void (*)(void *ctx, Event event, const uint8_t *payload, size_t len);

    struct Config
    {
        String host;
        uint16_t port = 0;
        String path = "/";
        bool use_ssl = false;
        uint32_t reconnect_ms = 2000;
        CloudTransportKind transport = CloudTransportKind::WebSocket;
    };

    virtual ~CloudTransport() = default;

    virtual void setMessageHandler(MessageHandler cb, void *ctx) = 0;
    virtual void setEventHandler(EventHandler cb, void *ctx) = 0;

    virtual void begin(const Config &cfg) = 0;
    virtual void loop() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual bool sendText(const String &text) = 0;
};
