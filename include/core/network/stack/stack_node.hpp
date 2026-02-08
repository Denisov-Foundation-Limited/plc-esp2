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
#include "core/network/stack/stack_types.hpp"
#include "utils/logger.hpp"

class StackNode
{
public:
    using FrameHandler = void (*)(void *ctx, const StackFrame &frame);
    using StatusProvider = size_t (*)(void *ctx, uint8_t *out, size_t cap);

    explicit StackNode(Logger &log) : _log(&log) {}

    void setFrameHandler(FrameHandler cb, void *ctx)
    {
        _frame_cb = cb;
        _frame_ctx = ctx;
    }

    void setNodeId(uint32_t id) { _node_id = id; }
    void setDeviceName(const String &name) { _device_name = name; }
    void setCaps(uint32_t caps) { _caps = caps; }
    void setServer(const String &host, uint16_t port)
    {
        _host = host;
        _port = port;
    }

    void setReconnectMs(uint32_t ms) { _reconnect_ms = ms; }
    void setHelloIntervalMs(uint32_t ms) { _hello_interval_ms = ms; }
    void setStatusIntervalMs(uint32_t ms) { _status_interval_ms = ms; }

    void setStatusProvider(StatusProvider cb, void *ctx)
    {
        _status_cb = cb;
        _status_ctx = ctx;
    }

    void disconnect()
    {
        if (_client.connected())
            _client.close(true);
    }

    const String &host() const { return _host; }
    uint16_t port() const { return _port; }

    void begin()
    {
        if (_host.length() == 0 || _port == 0)
        {
            if (_log)
                _log->warn(F("STACK"), F("Node begin skipped, host/port missing"));
            return;
        }
        if (_log)
            _log->info(F("STACK"), F("Node begin: %s:%u"), _host.c_str(), (unsigned)_port);
        setupClient_();
        connect_();
    }

    void loop()
    {
        if (_client.connected())
        {
            const uint32_t now = millis();
            if ((now - _last_hello_ms) >= _hello_interval_ms)
            {
                sendHello();
                _last_hello_ms = now;
            }
            if ((now - _last_status_ms) >= _status_interval_ms)
            {
                sendStatus_();
                _last_status_ms = now;
            }
            return;
        }
        const uint32_t now = millis();
        if ((now - _last_connect_ms) >= _reconnect_ms)
            connect_();
    }

    bool send(uint8_t type, const uint8_t *payload, size_t len)
    {
        if (!_client.connected())
            return false;
        const size_t frame_len = StackCodec::encode(type, payload, len, _tx_frame_buf, sizeof(_tx_frame_buf));
        if (frame_len == 0)
            return false;
        _client.write((const char *)_tx_frame_buf, frame_len);
        return true;
    }

    bool connected() const { return _client.connected(); }

    bool sendHello(uint16_t fw_ver = 0, uint32_t caps = 0xFFFFFFFFu)
    {
        StackHello hello{};
        hello.node_id = _node_id;
        hello.proto_ver = StackCodec::kVersion;
        hello.fw_ver = fw_ver;
        hello.caps = (caps == 0xFFFFFFFFu) ? _caps : caps;
        hello.name = _device_name;
        const size_t payload_len = StackHello::encode(hello, _tx_payload_buf, sizeof(_tx_payload_buf));
        if (payload_len == 0)
            return false;
        return send((uint8_t)StackMsgType::Hello, _tx_payload_buf, payload_len);
    }

private:
    String _host;
    uint16_t _port = 0;
    uint32_t _node_id = 0;
    String _device_name;
    uint32_t _caps = 0;
    uint32_t _reconnect_ms = 3000;
    uint32_t _last_connect_ms = 0;
    uint32_t _hello_interval_ms = 15000;
    uint32_t _status_interval_ms = 5000;
    uint32_t _last_hello_ms = 0;
    uint32_t _last_status_ms = 0;
    FrameHandler _frame_cb = nullptr;
    void *_frame_ctx = nullptr;
    StatusProvider _status_cb = nullptr;
    void *_status_ctx = nullptr;
    Logger *_log = nullptr;
    StackCodec _codec;

    AsyncClient _client;
    uint8_t _tx_payload_buf[StackCodec::kMaxPayload] = {};
    uint8_t _tx_frame_buf[StackCodec::kMaxFrame] = {};

    void setupClient_()
    {
        _client.onData(
            [](void *arg, AsyncClient *, void *data, size_t len) {
                StackNode *self = static_cast<StackNode *>(arg);
                self->onData_((const uint8_t *)data, len);
            },
            this);
        _client.onConnect(
            [](void *arg, AsyncClient *) {
                StackNode *self = static_cast<StackNode *>(arg);
                self->onConnect_();
            },
            this);
        _client.onDisconnect(
            [](void *arg, AsyncClient *) {
                StackNode *self = static_cast<StackNode *>(arg);
                self->onDisconnect_();
            },
            this);
        _client.onError(
            [](void *arg, AsyncClient *, int8_t) {
                StackNode *self = static_cast<StackNode *>(arg);
                self->onDisconnect_();
            },
            this);
    }

    void connect_()
    {
        _last_connect_ms = millis();
        _client.connect(_host.c_str(), _port);
    }

    void onConnect_()
    {
        sendHello();
        const uint32_t now = millis();
        _last_hello_ms = now;
        _last_status_ms = now;
        sendStatus_();
    }

    void onDisconnect_() {}

    void onData_(const uint8_t *data, size_t len)
    {
        _codec.feed(
            data, len,
            [](void *ctx, const StackFrame &frame) {
                StackNode *self = static_cast<StackNode *>(ctx);
                self->handleFrame_(frame);
            },
            this);
    }

    void handleFrame_(const StackFrame &frame)
    {
        if (_frame_cb)
            _frame_cb(_frame_ctx, frame);
    }

    void sendStatus_()
    {
        size_t payload_len = 0;
        if (_status_cb)
            payload_len = _status_cb(_status_ctx, _tx_payload_buf, sizeof(_tx_payload_buf));
        else
        {
            StackStatus st{};
            st.uptime_ms = millis();
            payload_len = StackStatus::encode(st, _tx_payload_buf, sizeof(_tx_payload_buf));
        }
        if (payload_len > 0)
            send((uint8_t)StackMsgType::Status, _tx_payload_buf, payload_len);
    }
};
