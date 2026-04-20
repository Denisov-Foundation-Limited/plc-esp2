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

#include "core/network/stack/stack_ws_server_transport.hpp"

namespace
{
constexpr uint32_t kWsHeartbeatPingMs = 5000u;
// WebSockets heartbeat timeout is checked from connection time, even before the
// first ping is sent. Keep pong timeout above ping interval, otherwise fresh
// slave sessions may be disconnected before the first heartbeat round-trip.
constexpr uint32_t kWsHeartbeatPongTimeoutMs = 12000u;
constexpr uint8_t kWsHeartbeatDisconnectCount = 3u;
}

StackWsServerTransport::StackWsServerTransport()
{
}

void StackWsServerTransport::setEventHandler(EventHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _event_cb = cb;
    _event_ctx = ctx;
}

bool StackWsServerTransport::begin(uint16_t port)
{
    const auto guard = _lock.guard();
    if (_started && _port == port)
        return true;

    stop();
    _port = port;
    _server.reset(new WebSocketsServer(_port));
    _server->begin();
    _server->enableHeartbeat(kWsHeartbeatPingMs, kWsHeartbeatPongTimeoutMs, kWsHeartbeatDisconnectCount);
    _server->onEvent([this](uint8_t client_id, WStype_t type, uint8_t *payload, size_t len) {
        onWsEvent_(client_id, type, payload, len);
    });
    _started = true;
    return true;
}

void StackWsServerTransport::loop()
{
    const auto guard = _lock.guard();
    if (_started && _server)
        _server->loop();
}

void StackWsServerTransport::stop()
{
    const auto guard = _lock.guard();
    if (!_started && !_server)
        return;

    if (_server)
    {
        _server->close();
        _server.reset();
    }

    _started = false;
}

StackTransport::Caps StackWsServerTransport::caps() const
{
    StackTransport::Caps out;
    out.full_duplex = true;
    out.can_push_async = true;
    out.requires_arbitration = false;
    out.supports_request_response = true;
    out.ordered_delivery = true;
    return out;
}

bool StackWsServerTransport::sendText(uint8_t client_id, const char *text)
{
    const auto guard = _lock.guard();
    if (!_started || !_server || !text || !text[0])
        return false;
    return _server->sendTXT(client_id, text);
}

bool StackWsServerTransport::sendBinary(uint8_t client_id, const uint8_t *data, size_t size)
{
    const auto guard = _lock.guard();
    if (!_started || !_server || !data || size == 0)
        return false;
    return _server->sendBIN(client_id, const_cast<uint8_t *>(data), size);
}

bool StackWsServerTransport::disconnectClient(uint8_t client_id)
{
    const auto guard = _lock.guard();
    if (!_started || !_server)
        return false;
    _server->disconnect(client_id);
    return true;
}

void StackWsServerTransport::onWsEvent_(uint8_t client_id, WStype_t type, uint8_t *payload, size_t len)
{
    EventHandler event_cb = nullptr;
    void *event_ctx = nullptr;
    IPAddress remote_ip;
    {
        const auto guard = _lock.guard();
        if (!_event_cb || !_server)
            return;
        event_cb = _event_cb;
        event_ctx = _event_ctx;
        remote_ip = _server->remoteIP(client_id);
    }

    if (!event_cb)
        return;

    StackTransport::Event event;
    event.client_id = client_id;
    event.ip = remote_ip;
    event.data = payload;
    event.size = len;

    switch (type)
    {
    case WStype_CONNECTED:
        event.type = StackTransport::EventType::Connected;
        event_cb(event_ctx, event);
        break;
    case WStype_DISCONNECTED:
        event.type = StackTransport::EventType::Disconnected;
        event_cb(event_ctx, event);
        break;
    case WStype_TEXT:
        event.type = StackTransport::EventType::TextMessage;
        event_cb(event_ctx, event);
        break;
    case WStype_BIN:
        event.type = StackTransport::EventType::BinaryMessage;
        event_cb(event_ctx, event);
        break;
    default:
        break;
    }
}
