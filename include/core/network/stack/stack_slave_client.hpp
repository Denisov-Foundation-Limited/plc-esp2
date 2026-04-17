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
#include <WebSocketsClient.h>

#include "core/network/stack/stack_binary_protocol.hpp"
#include "core/network/stack/stack_json_protocol.hpp"
#include "core/network/stack/stack_rs485_transport.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/rtos_lock.hpp"

class Logger;

class StackSlaveClient
{
public:
    // StackSlaveClient uses the same outward API for websocket and rs485-stub modes.
    // Lock level is transport-side; do not take adapter/router locks below it.
    struct Config
    {
        enum class TransportKind : uint8_t
        {
            WebSocket = 0,
            Rs485Stub
        };

        TransportKind transport = TransportKind::WebSocket;
        String host;
        uint16_t port = 0;
        String path = "/";
        String api_key;
        String device_name;
        uint32_t node_id = 0;
        uint32_t caps = 0;
        uint16_t fw_version = 0;
        uint32_t reconnect_ms = 3000;
        uint8_t rs485_client_id = 1;
        ConfigsManagerIface::StackPayloadMode payload_mode = ConfigsManagerIface::StackPayloadMode::Json;
        StackRs485Transport::Config rs485;
    };

    using RouteHandler = void (*)(void *ctx, const StackJsonProtocol::RouteMessage &route);
    using BinaryRouteHandler = void (*)(void *ctx, const StackBinaryProtocol::RouteFrame &route);
    using NotificationHandler = void (*)(void *ctx, const StackJsonProtocol::NotifyMessage &notify);

    explicit StackSlaveClient(Logger &log);

    void setConfig(const Config &cfg);
    void setRouteHandler(RouteHandler cb, void *ctx);
    void setBinaryRouteHandler(BinaryRouteHandler cb, void *ctx);
    void setNotificationHandler(NotificationHandler cb, void *ctx);
    void begin();
    void loop();
    void disconnect();

    bool isConnected() const;
    bool isAuthorized() const;
    StackTransport::Caps caps() const;
    bool sendNotify(const char *level, const char *feature, const char *code, const char *message = nullptr,
                    const JsonDocument *payload = nullptr);
    bool sendRoute(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload = nullptr,
                   const StackTransport::RouteMeta *meta = nullptr);
    bool sendRouteBinary(uint32_t target_node, const char *feature, const char *action,
                         const uint8_t *payload = nullptr, size_t payload_size = 0,
                         const StackTransport::RouteMeta *meta = nullptr);

private:
    Logger &_log;
    WebSocketsClient _ws;
    StackRs485Transport _rs485;
    Config _cfg;
    bool _started = false;
    bool _authorized = false;
    bool _ws_connected = false;
    uint32_t _ws_started_ms = 0;
    uint32_t _ws_connected_ms = 0;
    uint32_t _ws_last_restart_ms = 0;
    uint32_t _ws_last_rx_ms = 0;
    char _disconnect_reason[40]{};
    RouteHandler _route_cb = nullptr;
    void *_route_ctx = nullptr;
    BinaryRouteHandler _binary_route_cb = nullptr;
    void *_binary_route_ctx = nullptr;
    NotificationHandler _notify_cb = nullptr;
    void *_notify_ctx = nullptr;
    mutable RtosRecursiveLock _lock;

    void sendAuth_();
    bool sendNotifyBinary_(const char *level, const char *feature, const char *code, const char *message,
                           const uint8_t *payload, size_t payload_size);
    bool shouldRestartWebSocket_() const;
    void restartWebSocket_(const char *reason);
    void setDisconnectReason_(const char *reason);
    void takeDisconnectReason_(char *out, size_t out_cap);
    void onWsEvent_(WStype_t type, uint8_t *payload, size_t len);
    void onRs485Event_(const StackTransport::Event &event);
};
