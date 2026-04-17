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
#include <WiFi.h>
#include <memory>

#include "core/network/stack/stack_binary_protocol.hpp"
#include "core/network/stack/stack_json_protocol.hpp"
#include "utils/logger.hpp"

namespace
{
constexpr uint32_t kWsConnectWatchdogMs = 15000u;
constexpr uint32_t kWsAuthWatchdogMs = 15000u;
constexpr uint32_t kWsIdleWatchdogMs = 90000u;
constexpr uint32_t kWsRestartCooldownMs = 2000u;
constexpr uint32_t kWsHeartbeatPingMs = 20000u;
constexpr uint32_t kWsHeartbeatPongTimeoutMs = 10000u;
constexpr uint8_t kWsHeartbeatDisconnectCount = 3u;

const char *wifiStatusLabel_(wl_status_t st)
{
    switch (st)
    {
    case WL_IDLE_STATUS:
        return "Idle";
    case WL_NO_SSID_AVAIL:
        return "No SSID";
    case WL_SCAN_COMPLETED:
        return "Scan done";
    case WL_CONNECTED:
        return "Connected";
    case WL_CONNECT_FAILED:
        return "Connect failed";
    case WL_CONNECTION_LOST:
        return "Connection lost";
    case WL_DISCONNECTED:
        return "Disconnected";
    default:
        return "Unknown";
    }
}

void fillWifiDiag_(char *status_out, size_t status_cap, char *ip_out, size_t ip_cap, long &rssi_out)
{
    if (!status_out || status_cap == 0 || !ip_out || ip_cap == 0)
        return;
    status_out[0] = '\0';
    ip_out[0] = '\0';
    const wl_status_t st = WiFi.status();
    strncpy(status_out, wifiStatusLabel_(st), status_cap - 1);
    status_out[status_cap - 1] = '\0';
    if (st == WL_CONNECTED)
    {
        const String ip = WiFi.localIP().toString();
        strncpy(ip_out, ip.c_str(), ip_cap - 1);
        ip_out[ip_cap - 1] = '\0';
        rssi_out = WiFi.RSSI();
    }
    else
    {
        strncpy(ip_out, "-", ip_cap - 1);
        ip_out[ip_cap - 1] = '\0';
        rssi_out = 0;
    }
}

String binaryTextToString_(const uint8_t *data, size_t size)
{
    String out;
    if (!data || size == 0)
        return out;
    out.reserve(size);
    for (size_t i = 0; i < size; ++i)
        out += (char)data[i];
    return out;
}
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
    char wifi_status[24]{};
    char wifi_ip[20]{};
    long wifi_rssi = 0;
    fillWifiDiag_(wifi_status, sizeof(wifi_status), wifi_ip, sizeof(wifi_ip), wifi_rssi);
    _log.info(F("STACK"), F("WS slave start: name %s node 0x%08lX host %s port %u wifi: %s ip: %s rssi: %ld"),
              _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", (unsigned long)_cfg.node_id,
              _cfg.host.c_str(), (unsigned)_cfg.port, wifi_status, wifi_ip, wifi_rssi);
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
    ConfigsManagerIface::StackPayloadMode payload_mode = ConfigsManagerIface::StackPayloadMode::Json;
    {
        const auto guard = _lock.guard();
        if (!_authorized || !feature || !feature[0] || !level || !level[0])
            return false;
        transport = _cfg.transport;
        node_id = _cfg.node_id;
        rs485_client_id = _cfg.rs485_client_id;
        payload_mode = _cfg.payload_mode;
    }
    if (payload_mode == ConfigsManagerIface::StackPayloadMode::Binary)
    {
        if (payload)
            return false;
        return sendNotifyBinary_(level, feature, code, message, nullptr, 0);
    }
    String payload_json;
    if (payload)
        serializeJson(*payload, payload_json);
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
        if (!_authorized || !feature || !feature[0] || !action || !action[0])
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
        if (!_authorized || !feature || !feature[0] || !action || !action[0])
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
    if (_cfg.payload_mode == ConfigsManagerIface::StackPayloadMode::Binary)
    {
        const size_t frame_size =
            StackBinaryProtocol::encodedAuthSize(_cfg.device_name.c_str(), "", _cfg.api_key.c_str());
        std::unique_ptr<uint8_t[]> frame(new uint8_t[frame_size]);
        size_t used = 0;
        if (frame && StackBinaryProtocol::encodeAuth(_cfg.node_id, _cfg.caps, _cfg.fw_version, _cfg.device_name.c_str(), "",
                                                     _cfg.api_key.c_str(), frame.get(), frame_size, used))
            _ws.sendBIN(frame.get(), used);
        return;
    }
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

bool StackSlaveClient::sendNotifyBinary_(const char *level, const char *feature, const char *code, const char *message,
                                         const uint8_t *payload, size_t payload_size)
{
    Config::TransportKind transport = Config::TransportKind::WebSocket;
    uint32_t node_id = 0;
    uint8_t rs485_client_id = 0;
    {
        const auto guard = _lock.guard();
        transport = _cfg.transport;
        node_id = _cfg.node_id;
        rs485_client_id = _cfg.rs485_client_id;
    }
    const uint8_t *msg_bytes = message ? reinterpret_cast<const uint8_t *>(message) : nullptr;
    const size_t msg_size = message ? strlen(message) : 0;
    const size_t frame_size =
        StackBinaryProtocol::encodedNotifySize(level, feature, code, msg_size, payload_size);
    std::unique_ptr<uint8_t[]> frame(new uint8_t[frame_size]);
    size_t used = 0;
    if (!frame || !StackBinaryProtocol::encodeNotify(node_id, level, feature, code, msg_bytes, msg_size, payload,
                                                     payload_size, frame.get(), frame_size, used))
        return false;
    if (transport == Config::TransportKind::Rs485Stub)
        return _rs485.sendBinary(rs485_client_id, frame.get(), used);
    return _ws.sendBIN(frame.get(), used);
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
    bool ws_connected = false;
    bool authorized = false;
    uint32_t connected_ms = 0;
    uint32_t last_rx_ms = 0;
    {
        const auto guard = _lock.guard();
        if (_cfg.transport != Config::TransportKind::WebSocket)
            return;
        _ws_last_restart_ms = millis();
        cfg = _cfg;
        ws_connected = _ws_connected;
        authorized = _authorized;
        connected_ms = _ws_connected_ms;
        last_rx_ms = _ws_last_rx_ms;
        _authorized = false;
        _ws_connected = false;
        _ws_started_ms = _ws_last_restart_ms;
        _ws_connected_ms = 0;
        _ws_last_rx_ms = 0;
        _disconnect_reason[0] = '\0';
    }

    char wifi_status[24]{};
    char wifi_ip[20]{};
    long wifi_rssi = 0;
    fillWifiDiag_(wifi_status, sizeof(wifi_status), wifi_ip, sizeof(wifi_ip), wifi_rssi);
    const uint32_t now = millis();
    const uint32_t connected_age_ms = connected_ms ? (uint32_t)(now - connected_ms) : 0;
    const uint32_t idle_ms = last_rx_ms ? (uint32_t)(now - last_rx_ms) : 0;
    _log.warn(F("STACK"),
              F("WS slave reconnect watchdog: reason %s host %s port %u ws_connected: %u auth: %u connected_age_ms: %lu idle_ms: %lu wifi: %s ip: %s rssi: %ld"),
              why, cfg.host.c_str(), (unsigned)cfg.port, (unsigned)(ws_connected ? 1u : 0u),
              (unsigned)(authorized ? 1u : 0u), (unsigned long)connected_age_ms, (unsigned long)idle_ms,
              wifi_status, wifi_ip, wifi_rssi);
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
        {
            char wifi_status[24]{};
            char wifi_ip[20]{};
            long wifi_rssi = 0;
            fillWifiDiag_(wifi_status, sizeof(wifi_status), wifi_ip, sizeof(wifi_ip), wifi_rssi);
            _log.info(F("STACK"), F("WS slave connected: name %s host %s wifi: %s ip: %s rssi: %ld"),
                      _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", _cfg.host.c_str(),
                      wifi_status, wifi_ip, wifi_rssi);
        }
        sendAuth_();
        break;
    case WStype_DISCONNECTED:
    {
        bool was_authorized = false;
        uint32_t connected_ms = 0;
        uint32_t last_rx_ms = 0;
        char reason[40]{};
        {
            const auto guard = _lock.guard();
            was_authorized = _authorized;
            connected_ms = _ws_connected_ms;
            last_rx_ms = _ws_last_rx_ms;
            _authorized = false;
            _ws_connected = false;
            _ws_started_ms = millis();
            _ws_connected_ms = 0;
            _ws_last_rx_ms = 0;
        }
        takeDisconnectReason_(reason, sizeof(reason));
        char wifi_status[24]{};
        char wifi_ip[20]{};
        long wifi_rssi = 0;
        fillWifiDiag_(wifi_status, sizeof(wifi_status), wifi_ip, sizeof(wifi_ip), wifi_rssi);
        const uint32_t now = millis();
        const uint32_t connected_age_ms = connected_ms ? (uint32_t)(now - connected_ms) : 0;
        const uint32_t idle_ms = last_rx_ms ? (uint32_t)(now - last_rx_ms) : 0;
        if (was_authorized)
            _log.warn(F("STACK"),
                      F("WS slave disconnected: name %s host %s reason: %s connected_age_ms: %lu idle_ms: %lu wifi: %s ip: %s rssi: %ld"),
                      _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", _cfg.host.c_str(),
                      reason[0] ? reason : "transport_disconnect", (unsigned long)connected_age_ms,
                      (unsigned long)idle_ms, wifi_status, wifi_ip, wifi_rssi);
        else
            _log.warn(F("STACK"),
                      F("WS slave disconnected before auth: name %s host %s reason: %s connected_age_ms: %lu idle_ms: %lu wifi: %s ip: %s rssi: %ld"),
                      _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", _cfg.host.c_str(),
                      reason[0] ? reason : "transport_disconnect", (unsigned long)connected_age_ms,
                      (unsigned long)idle_ms, wifi_status, wifi_ip, wifi_rssi);
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
            uint32_t connected_ms = 0;
            {
                const auto guard = _lock.guard();
                _authorized = true;
                _ws_last_rx_ms = millis();
                connected_ms = _ws_connected_ms;
            }
            const uint32_t auth_ms = connected_ms ? (uint32_t)(millis() - connected_ms) : 0;
            char wifi_status[24]{};
            char wifi_ip[20]{};
            long wifi_rssi = 0;
            fillWifiDiag_(wifi_status, sizeof(wifi_status), wifi_ip, sizeof(wifi_ip), wifi_rssi);
            _log.info(F("STACK"), F("WS slave authorized: name %s node 0x%08lX auth_ms: %lu wifi: %s ip: %s rssi: %ld"),
                      _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", (unsigned long)_cfg.node_id,
                      (unsigned long)auth_ms, wifi_status, wifi_ip, wifi_rssi);
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
        StackBinaryProtocol::NotifyFrame notify_frame;
        if (StackBinaryProtocol::parseNotify(payload, len, notify_frame))
        {
            NotificationHandler cb = nullptr;
            void *cb_ctx = nullptr;
            StackJsonProtocol::NotifyMessage notify;
            notify.is_binary = true;
            notify.source_node = notify_frame.source_node;
            strncpy(notify.level, notify_frame.level, sizeof(notify.level) - 1);
            strncpy(notify.feature, notify_frame.feature, sizeof(notify.feature) - 1);
            strncpy(notify.code, notify_frame.code, sizeof(notify.code) - 1);
            notify.message = binaryTextToString_(notify_frame.message, notify_frame.message_size);
            {
                const auto guard = _lock.guard();
                cb = _notify_cb;
                cb_ctx = _notify_ctx;
            }
            if (cb)
                cb(cb_ctx, notify);
            break;
        }
        StackBinaryProtocol::AuthReplyFrame auth_reply;
        if (StackBinaryProtocol::parseAuthReply(payload, len, auth_reply))
        {
            if (auth_reply.ok && strcmp(auth_reply.message, "authorized") == 0)
            {
                uint32_t connected_ms = 0;
                {
                    const auto guard = _lock.guard();
                    _authorized = true;
                    _ws_last_rx_ms = millis();
                    connected_ms = _ws_connected_ms;
                }
                const uint32_t auth_ms = connected_ms ? (uint32_t)(millis() - connected_ms) : 0;
                char wifi_status[24]{};
                char wifi_ip[20]{};
                long wifi_rssi = 0;
                fillWifiDiag_(wifi_status, sizeof(wifi_status), wifi_ip, sizeof(wifi_ip), wifi_rssi);
                _log.info(F("STACK"), F("WS slave authorized: name %s node 0x%08lX auth_ms: %lu wifi: %s ip: %s rssi: %ld"),
                          _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", (unsigned long)_cfg.node_id,
                          (unsigned long)auth_ms, wifi_status, wifi_ip, wifi_rssi);
            }
            else
            {
                _log.warn(F("STACK"), F("WS slave auth error: name %s host %s error: %s"),
                          _cfg.device_name.length() ? _cfg.device_name.c_str() : "-", _cfg.host.c_str(),
                          auth_reply.message[0] ? auth_reply.message : "error");
                {
                    const auto guard = _lock.guard();
                    _authorized = false;
                }
                setDisconnectReason_("auth_failed");
                _ws.disconnect();
            }
            break;
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
        StackBinaryProtocol::NotifyFrame notify_frame;
        if (StackBinaryProtocol::parseNotify(event.data, event.size, notify_frame))
        {
            NotificationHandler cb = nullptr;
            void *cb_ctx = nullptr;
            StackJsonProtocol::NotifyMessage notify;
            notify.is_binary = true;
            notify.source_node = notify_frame.source_node;
            strncpy(notify.level, notify_frame.level, sizeof(notify.level) - 1);
            strncpy(notify.feature, notify_frame.feature, sizeof(notify.feature) - 1);
            strncpy(notify.code, notify_frame.code, sizeof(notify.code) - 1);
            notify.message = binaryTextToString_(notify_frame.message, notify_frame.message_size);
            {
                const auto guard = _lock.guard();
                cb = _notify_cb;
                cb_ctx = _notify_ctx;
            }
            if (cb)
                cb(cb_ctx, notify);
            return;
        }
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
