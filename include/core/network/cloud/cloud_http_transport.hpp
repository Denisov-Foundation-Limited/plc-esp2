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

#include <HTTPClient.h>

#include "core/network/cloud/cloud_transport.hpp"

class CloudHttpTransport : public CloudTransport
{
public:
    void setMessageHandler(MessageHandler cb, void *ctx) override;
    void setEventHandler(EventHandler cb, void *ctx) override;

    void begin(const Config &cfg) override;
    void loop() override;
    void disconnect() override;
    bool isConnected() const override;
    bool sendText(const String &text) override;

private:
    HTTPClient _http;
    Config _cfg;
    MessageHandler _message_cb = nullptr;
    void *_message_ctx = nullptr;
    EventHandler _event_cb = nullptr;
    void *_event_ctx = nullptr;
    bool _connected = false;
    bool _configured = false;
    uint32_t _last_poll_ms = 0;
    String _base_url;
    String _outbox;

    void notifyEvent_(Event event, const uint8_t *payload = nullptr, size_t len = 0);
};

