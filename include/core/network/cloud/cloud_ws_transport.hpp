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

#include <WebSocketsClient.h>

#include "core/network/cloud/cloud_transport.hpp"

class Logger;

class CloudWsTransport : public CloudTransport
{
public:
    static constexpr uint32_t kTxRxAliveWindowMs = 1500;
    static constexpr uint32_t kTxDeadWhileRxAliveMs = 5000;

    void setLogger(Logger *logger);
    void setMessageHandler(MessageHandler cb, void *ctx) override;
    void setEventHandler(EventHandler cb, void *ctx) override;

    void begin(const Config &cfg) override;
    void loop() override;
    void disconnect() override;
    bool isConnected() const override;
    bool sendText(const String &text) override;

private:
    WebSocketsClient _ws;
    Config _cfg;
    Logger *_logger = nullptr;
    MessageHandler _message_cb = nullptr;
    void *_message_ctx = nullptr;
    EventHandler _event_cb = nullptr;
    void *_event_ctx = nullptr;
    uint32_t _last_rx_ms = 0;
    uint32_t _last_tx_attempt_ms = 0;
    uint32_t _last_tx_ok_ms = 0;
    uint32_t _last_connect_ms = 0;
    uint32_t _tx_fail_streak = 0;
    bool _disconnect_logged = false;

    void onWsEvent_(WStype_t type, uint8_t *payload, size_t len);
};
