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

#include "core/network/stack/stack_slave_client.hpp"

#include <ArduinoJson.h>
#include <memory>

#include "core/network/stack/stack_binary_protocol.hpp"
#include "core/network/stack/stack_json_protocol.hpp"
#include "utils/logger.hpp"

namespace
{
constexpr uint32_t kWsConnectWatchdogMs = 15000u;
constexpr uint32_t kWsAuthWatchdogMs = 8000u;
constexpr uint32_t kWsIdleWatchdogMs = 45000u;
constexpr uint32_t kWsRestartCooldownMs = 2000u;
constexpr uint32_t kWsHeartbeatPingMs = 15000u;
constexpr uint32_t kWsHeartbeatPongTimeoutMs = 3000u;
constexpr uint8_t kWsHeartbeatDisconnectCount = 2u;
}

StackSlaveClient::StackSlaveClient(Logger &log) : _log(log), _rs485(log)
{
}

void StackSlaveClient::setConfig(const Config &cfg)
{
    const auto guard = _lock.guard();
    _cfg = cfg;
}

void StackSlaveClient::setRouteHandler(RouteHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _route_cb = cb;
    _route_ctx = ctx;
}

void StackSlaveClient::setBinaryRouteHandler(BinaryRouteHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _binary_route_cb = cb;
    _binary_route_ctx = ctx;
}

void StackSlaveClient::setNotificationHandler(NotificationHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _notify_cb = cb;
    _notify_ctx = ctx;
}

void StackSlaveClient::begin()
{
    const auto guard = _lock.guard();
    disconnect();
    if (_cfg.host.length() == 0 || _cfg.port == 0)
    {
        if (_cfg.transport != Config::TransportKind::Rs485Stub)
            return;
    }

    if (_cfg.transport == Config::TransportKind::Rs485Stub)
    {
        _rs485.setConfig(_cfg.rs485);
        _rs485.setEventHandler(
            [](void *ctx, const StackTransport::Event &event) {
                if (!ctx)
                    return;
                static_cast<StackSlaveClient *>(ctx)->onRs485Event_(event);
            },
            this);
        if (!_rs485.begin(_cfg.port))
            return;
        _started = true;
        _authorized = true;
        _log.info(F("STACK"), F("RS485 slave stub start: name %s node 0x%08lX client %u port %u"),
                  _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", (unsigned long)_cfg.node_id,
                  (unsigned)_cfg.rs485_client_id, (unsigned)_cfg.port);
        return;
    }

    _ws.onEvent([this](WStype_t type, uint8_t *payload, size_t len) { onWsEvent_(type, payload, len); });
    _ws.setReconnectInterval(_cfg.reconnect_ms);
    _ws.enableHeartbeat(kWsHeartbeatPingMs, kWsHeartbeatPongTimeoutMs, kWsHeartbeatDisconnectCount);
    _ws.begin(_cfg.host.c_str(), _cfg.port, _cfg.path.c_str(), "arduino");
    _started = true;
    _authorized = false;
    _ws_connected = false;
    _ws_started_ms = millis();
    _ws_connected_ms = 0;
    _ws_last_rx_ms = 0;
    _log.info(F("STACK"), F("WS slave start: name %s node 0x%08lX host %s port %u"),
              _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", (unsigned long)_cfg.node_id,
              _cfg.host.c_str(), (unsigned)_cfg.port);
}

void StackSlaveClient::loop()
{
    bool started = false;
    Config::TransportKind transport = Config::TransportKind::WebSocket;
    {
        const auto guard = _lock.guard();
        started = _started;
        transport = _cfg.transport;
    }
    if (started && transport == Config::TransportKind::Rs485Stub)
        _rs485.loop();
    else if (started)
    {
        _ws.loop();
        if (shouldRestartWebSocket_())
            restartWebSocket_("watchdog");
    }
}

void StackSlaveClient::disconnect()
{
    bool stop_rs485 = false;
    bool stop_ws = false;
    {
        const auto guard = _lock.guard();
        _authorized = false;
        _ws_connected = false;
        _ws_started_ms = 0;
        _ws_connected_ms = 0;
        _ws_last_rx_ms = 0;
        stop_rs485 = (_cfg.transport == Config::TransportKind::Rs485Stub);
        stop_ws = !stop_rs485;
        _started = false;
        _disconnect_reason[0] = '\0';
    }
    if (stop_rs485)
        _rs485.stop();
    if (stop_ws)
        _ws.disconnect();
}

bool StackSlaveClient::isConnected() const
{
    const auto guard = _lock.guard();
    if (_cfg.transport == Config::TransportKind::Rs485Stub)
        return _started;
    return const_cast<WebSocketsClient &>(_ws).isConnected();
}

bool StackSlaveClient::isAuthorized() const
{
    const auto guard = _lock.guard();
    return _authorized;
}

StackTransport::Caps StackSlaveClient::caps() const
{
    const auto guard = _lock.guard();
    if (_cfg.transport == Config::TransportKind::Rs485Stub)
        return _rs485.caps();
    StackTransport::Caps out;
    out.full_duplex = true;
    out.can_push_async = true;
    out.requires_arbitration = false;
    out.supports_request_response = true;
    out.ordered_delivery = true;
    return out;
}

bool StackSlaveClient::sendNotify(const char *level, const char *feature, const char *code, const char *message,
                                  const JsonDocument *payload)
{
    Config::TransportKind transport = Config::TransportKind::WebSocket;
    uint32_t node_id = 0;
    uint8_t rs485_client_id = 0;
    {
        const auto guard = _lock.guard();
        if (!_authorized || !feature || !feature[0] || !level || !level[0])
            return false;
        transport = _cfg.transport;
        node_id = _cfg.node_id;
        rs485_client_id = _cfg.rs485_client_id;
    }
    String msg = StackJsonProtocol::makeNotify(node_id, level, feature, code, message, payload);
    if (transport == Config::TransportKind::Rs485Stub)
        return _rs485.sendText(rs485_client_id, msg.c_str());
    return _ws.sendTXT(msg);
}

bool StackSlaveClient::sendRoute(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload,
                                 const StackTransport::RouteMeta *meta)
{
    Config::TransportKind transport = Config::TransportKind::WebSocket;
    uint32_t node_id = 0;
    uint8_t rs485_client_id = 0;
    {
        const auto guard = _lock.guard();
        if (!_authorized || target_node == 0 || !feature || !feature[0] || !action || !action[0])
            return false;
        transport = _cfg.transport;
        node_id = _cfg.node_id;
        rs485_client_id = _cfg.rs485_client_id;
    }
    String msg = StackJsonProtocol::makeRoute(node_id, target_node, feature, action, payload, meta);
    if (transport == Config::TransportKind::Rs485Stub)
        return _rs485.sendText(rs485_client_id, msg.c_str());
    const bool ok = _ws.sendTXT(msg);
    return ok;
}

bool StackSlaveClient::sendRouteBinary(uint32_t target_node, const char *feature, const char *action,
                                       const uint8_t *payload, size_t payload_size, const StackTransport::RouteMeta *meta)
{
    Config::TransportKind transport = Config::TransportKind::WebSocket;
    uint32_t node_id = 0;
    uint8_t rs485_client_id = 0;
    {
        const auto guard = _lock.guard();
        if (!_authorized || target_node == 0 || !feature || !feature[0] || !action || !action[0])
            return false;
        transport = _cfg.transport;
        node_id = _cfg.node_id;
        rs485_client_id = _cfg.rs485_client_id;
    }
    const size_t frame_size = StackBinaryProtocol::encodedRouteSize(feature, action, payload_size);
    std::unique_ptr<uint8_t[]> frame(new uint8_t[frame_size]);
    size_t used = 0;
    if (!frame || !StackBinaryProtocol::encodeRoute(node_id, target_node, feature, action, payload, payload_size,
                                                    frame.get(), frame_size, used, meta))
        return false;
    if (transport == Config::TransportKind::Rs485Stub)
        return _rs485.sendBinary(rs485_client_id, frame.get(), used);
    return _ws.sendBIN(frame.get(), used);
}

void StackSlaveClient::sendAuth_()
{
    const auto guard = _lock.guard();
    StaticJsonDocument<256> doc;
    doc["type"] = "auth";
    doc["api_key"] = _cfg.api_key;
    doc["node_id"] = _cfg.node_id;
    doc["name"] = _cfg.device_name;
    doc["caps"] = _cfg.caps;
    doc["fw_ver"] = _cfg.fw_version;
    String payload;
    serializeJson(doc, payload);
    _ws.sendTXT(payload);
}

bool StackSlaveClient::shouldRestartWebSocket_() const
{
    const auto guard = _lock.guard();
    if (!_started || _cfg.transport != Config::TransportKind::WebSocket)
        return false;

    const uint32_t now = millis();
    if (_ws_last_restart_ms && (uint32_t)(now - _ws_last_restart_ms) < kWsRestartCooldownMs)
        return false;

    if (!_ws_connected)
    {
        if (_ws_started_ms != 0 && (uint32_t)(now - _ws_started_ms) >= kWsConnectWatchdogMs)
            return true;
        return false;
    }

    if (!_authorized && _ws_connected_ms != 0 && (uint32_t)(now - _ws_connected_ms) >= kWsAuthWatchdogMs)
        return true;

    if (_authorized && _ws_last_rx_ms != 0 && (uint32_t)(now - _ws_last_rx_ms) >= kWsIdleWatchdogMs)
        return true;

    return false;
}

void StackSlaveClient::restartWebSocket_(const char *reason)
{
    Config cfg;
    const char *why = (reason && reason[0]) ? reason : "restart";
    {
        const auto guard = _lock.guard();
        if (_cfg.transport != Config::TransportKind::WebSocket)
            return;
        _ws_last_restart_ms = millis();
        cfg = _cfg;
        _authorized = false;
        _ws_connected = false;
        _ws_started_ms = _ws_last_restart_ms;
        _ws_connected_ms = 0;
        _ws_last_rx_ms = 0;
        _disconnect_reason[0] = '\0';
    }

    _log.warn(F("STACK"), F("WS slave reconnect watchdog: reason %s host %s port %u"), why, cfg.host.c_str(),
              (unsigned)cfg.port);
    _ws.disconnect();
    _ws.setReconnectInterval(cfg.reconnect_ms);
    _ws.enableHeartbeat(kWsHeartbeatPingMs, kWsHeartbeatPongTimeoutMs, kWsHeartbeatDisconnectCount);
    _ws.begin(cfg.host.c_str(), cfg.port, cfg.path.c_str(), "arduino");
}

void StackSlaveClient::setDisconnectReason_(const char *reason)
{
    const auto guard = _lock.guard();
    _disconnect_reason[0] = '\0';
    if (!reason || !reason[0])
        return;
    strncpy(_disconnect_reason, reason, sizeof(_disconnect_reason) - 1);
    _disconnect_reason[sizeof(_disconnect_reason) - 1] = '\0';
}

void StackSlaveClient::takeDisconnectReason_(char *out, size_t out_cap)
{
    if (!out || out_cap == 0)
        return;
    const auto guard = _lock.guard();
    out[0] = '\0';
    strncpy(out, _disconnect_reason, out_cap - 1);
    out[out_cap - 1] = '\0';
    _disconnect_reason[0] = '\0';
}

void StackSlaveClient::onWsEvent_(WStype_t type, uint8_t *payload, size_t len)
{
    switch (type)
    {
    case WStype_CONNECTED:
        {
            const auto guard = _lock.guard();
            _authorized = false;
            _ws_connected = true;
            _ws_connected_ms = millis();
            _ws_last_rx_ms = _ws_connected_ms;
            _disconnect_reason[0] = '\0';
        }
        _log.info(F("STACK"), F("WS slave connected: name %s host %s"),
                  _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", _cfg.host.c_str());
        sendAuth_();
        break;
    case WStype_DISCONNECTED:
    {
        bool was_authorized = false;
        char reason[40]{};
        {
            const auto guard = _lock.guard();
            was_authorized = _authorized;
            _authorized = false;
            _ws_connected = false;
            _ws_started_ms = millis();
            _ws_connected_ms = 0;
            _ws_last_rx_ms = 0;
        }
        takeDisconnectReason_(reason, sizeof(reason));
        if (was_authorized)
            _log.warn(F("STACK"), F("WS slave disconnected: name %s host %s reason: %s"),
                      _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", _cfg.host.c_str(),
                      reason[0] ? reason : "transport_disconnect");
        else
            _log.warn(F("STACK"), F("WS slave disconnected before auth: name %s host %s reason: %s"),
                      _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", _cfg.host.c_str(),
                      reason[0] ? reason : "transport_disconnect");
        break;
    }
    case WStype_TEXT:
    {
        {
            const auto guard = _lock.guard();
            _ws_last_rx_ms = millis();
        }
        if (StackJsonProtocol::detectKind(payload, len) == StackJsonProtocol::MessageKind::Notify)
        {
            StackJsonProtocol::NotifyMessage notify;
            NotificationHandler cb = nullptr;
            void *cb_ctx = nullptr;
            if (StackJsonProtocol::parseNotify(payload, len, notify))
            {
                const auto guard = _lock.guard();
                cb = _notify_cb;
                cb_ctx = _notify_ctx;
            }
            if (cb)
                cb(cb_ctx, notify);
            break;
        }
        if (StackJsonProtocol::detectKind(payload, len) == StackJsonProtocol::MessageKind::Route)
        {
            StackJsonProtocol::RouteMessage route;
            RouteHandler cb = nullptr;
            void *cb_ctx = nullptr;
            if (StackJsonProtocol::parseRoute(payload, len, route))
            {
                const auto guard = _lock.guard();
                cb = _route_cb;
                cb_ctx = _route_ctx;
            }
            if (cb)
                cb(cb_ctx, route);
            break;
        }
        StaticJsonDocument<160> doc;
        if (deserializeJson(doc, payload, len))
            return;
        if ((doc["ok"] | false) && strcmp(doc["message"] | "", "authorized") == 0)
        {
            const auto guard = _lock.guard();
            _authorized = true;
            _ws_last_rx_ms = millis();
            _log.info(F("STACK"), F("WS slave authorized: name %s node 0x%08lX"),
                      _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", (unsigned long)_cfg.node_id);
            return;
        }
        if (!(doc["ok"] | true))
        {
            const char *error = doc["error"] | "error";
            _log.warn(F("STACK"), F("WS slave auth error: name %s host %s error: %s"),
                      _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", _cfg.host.c_str(), error);
            {
                const auto guard = _lock.guard();
                _authorized = false;
            }
            setDisconnectReason_("auth_failed");
            _ws.disconnect();
        }
        break;
    }
    case WStype_BIN:
    {
        {
            const auto guard = _lock.guard();
            _ws_last_rx_ms = millis();
        }
        StackBinaryProtocol::RouteFrame route;
        BinaryRouteHandler cb = nullptr;
        void *cb_ctx = nullptr;
        if (StackBinaryProtocol::parseRoute(payload, len, route))
        {
            const auto guard = _lock.guard();
            cb = _binary_route_cb;
            cb_ctx = _binary_route_ctx;
        }
        if (cb)
            cb(cb_ctx, route);
        break;
    }
    case WStype_PONG:
        {
            const auto guard = _lock.guard();
            _ws_last_rx_ms = millis();
        }
        break;
    default:
        break;
    }
}

void StackSlaveClient::onRs485Event_(const StackTransport::Event &event)
{
    switch (event.type)
    {
    case StackTransport::EventType::TextMessage:
        if (StackJsonProtocol::detectKind(event.data, event.size) == StackJsonProtocol::MessageKind::Notify)
        {
            StackJsonProtocol::NotifyMessage notify;
            NotificationHandler cb = nullptr;
            void *cb_ctx = nullptr;
            if (StackJsonProtocol::parseNotify(event.data, event.size, notify))
            {
                const auto guard = _lock.guard();
                cb = _notify_cb;
                cb_ctx = _notify_ctx;
            }
            if (cb)
                cb(cb_ctx, notify);
            return;
        }
        if (StackJsonProtocol::detectKind(event.data, event.size) == StackJsonProtocol::MessageKind::Route)
        {
            StackJsonProtocol::RouteMessage route;
            RouteHandler cb = nullptr;
            void *cb_ctx = nullptr;
            if (StackJsonProtocol::parseRoute(event.data, event.size, route))
            {
                const auto guard = _lock.guard();
                cb = _route_cb;
                cb_ctx = _route_ctx;
            }
            if (cb)
                cb(cb_ctx, route);
        }
        return;
    case StackTransport::EventType::BinaryMessage:
    {
        StackBinaryProtocol::RouteFrame route;
        BinaryRouteHandler cb = nullptr;
        void *cb_ctx = nullptr;
        if (StackBinaryProtocol::parseRoute(event.data, event.size, route))
        {
            const auto guard = _lock.guard();
            cb = _binary_route_cb;
            cb_ctx = _binary_route_ctx;
        }
        if (cb)
            cb(cb_ctx, route);
        return;
    }
    default:
        return;
    }
}
