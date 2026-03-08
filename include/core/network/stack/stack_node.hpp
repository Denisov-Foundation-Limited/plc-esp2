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
#include <AsyncTCP.h>

#include "core/network/stack/stack_protocol.hpp"
#include "utils/logger.hpp"

class StackNode
{
public:
    using FrameHandler = void (*)(void *ctx, const StackFrame &frame);
    using StatusProvider = size_t (*)(void *ctx, uint8_t *out, size_t cap);

    explicit StackNode(Logger &log);

    void setFrameHandler(FrameHandler cb, void *ctx);

    void setNodeId(uint32_t id);
    void setDeviceName(const String &name);
    void setCaps(uint32_t caps);
    void setServer(const String &host, uint16_t port);

    void setReconnectMs(uint32_t ms);
    void setHelloIntervalMs(uint32_t ms);
    void setStatusIntervalMs(uint32_t ms);

    void setStatusProvider(StatusProvider cb, void *ctx);

    void disconnect();

    const String &host() const;
    uint16_t port() const;

    void begin();

    void loop();

    bool send(uint8_t type, const uint8_t *payload, size_t len);

    bool connected() const;
    bool helloSentCurrentConnection() const;

    bool sendHello(uint16_t fw_ver = 0, uint32_t caps = 0xFFFFFFFFu);

private:
    String _host;
    uint16_t _port = 0;
    uint32_t _node_id = 0;
    String _device_name;
    uint32_t _caps = 0;
    uint32_t _reconnect_ms = 3000;
    uint32_t _last_connect_ms = 0;
    uint32_t _hello_interval_ms = 15000;
    uint32_t _status_interval_ms = 2000;
    uint32_t _last_hello_ms = 0;
    uint32_t _last_status_ms = 0;
    bool _hello_sent_current_connection = false;
    FrameHandler _frame_cb = nullptr;
    void *_frame_ctx = nullptr;
    StatusProvider _status_cb = nullptr;
    void *_status_ctx = nullptr;
    Logger *_log = nullptr;
    StackCodec _codec;

    AsyncClient _client;
    uint8_t _tx_payload_buf[StackCodec::kMaxPayload] = {};
    uint8_t _tx_frame_buf[StackCodec::kMaxFrame] = {};

    void setupClient_();

    void connect_();

    void onConnect_();

    void onDisconnect_();

    void onData_(const uint8_t *data, size_t len);

    void handleFrame_(const StackFrame &frame);

    void sendStatus_();
};

