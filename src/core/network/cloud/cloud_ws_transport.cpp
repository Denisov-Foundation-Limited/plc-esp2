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

#include "core/network/cloud/cloud_ws_transport.hpp"

#include "utils/logger.hpp"

void CloudWsTransport::setLogger(Logger *logger)
{
    _logger = logger;
}

void CloudWsTransport::setMessageHandler(CloudTransport::MessageHandler cb, void *ctx)
{
    _message_cb = cb;
    _message_ctx = ctx;
}

void CloudWsTransport::setEventHandler(CloudTransport::EventHandler cb, void *ctx)
{
    _event_cb = cb;
    _event_ctx = ctx;
}

void CloudWsTransport::begin(const CloudTransport::Config &cfg)
{
    _cfg = cfg;
    _last_rx_ms = 0;
    _last_tx_attempt_ms = 0;
    _last_tx_ok_ms = 0;
    _last_connect_ms = 0;
    _tx_fail_streak = 0;
    _disconnect_logged = false;
    _ws.disconnect();
    _ws.onEvent([this](WStype_t t, uint8_t *p, size_t l) { onWsEvent_(t, p, l); });
    _ws.setReconnectInterval(_cfg.reconnect_ms);
    _ws.setExtraHeaders("");
    if (_cfg.use_ssl)
        _ws.beginSSL(_cfg.host.c_str(), _cfg.port, _cfg.path.c_str(), nullptr, "arduino");
    else
        _ws.begin(_cfg.host.c_str(), _cfg.port, _cfg.path.c_str(), "arduino");
}

void CloudWsTransport::loop()
{
    _ws.loop();
}

void CloudWsTransport::disconnect()
{
    _ws.disconnect();
}

bool CloudWsTransport::isConnected() const
{
    return const_cast<WebSocketsClient &>(_ws).isConnected();
}

bool CloudWsTransport::sendText(const String &text)
{
    if (!text.length())
        return false;
    const uint32_t now = millis();
    const bool connected = isConnected();
    _last_tx_attempt_ms = now;
    String payload = text;
    const bool ok = _ws.sendTXT(payload);
    if (ok)
    {
        _last_tx_ok_ms = now;
        _tx_fail_streak = 0;
    }
    else
    {
        if (_tx_fail_streak < 0xFFFFFFFFu)
            ++_tx_fail_streak;
        const uint32_t rx_idle_ms = _last_rx_ms ? (uint32_t)(now - _last_rx_ms) : 0u;
        const uint32_t tx_idle_ms = _last_tx_ok_ms ? (uint32_t)(now - _last_tx_ok_ms) : 0u;
        const bool rx_alive = _last_rx_ms && (rx_idle_ms <= kTxRxAliveWindowMs);
        const bool tx_dead_while_rx_alive = connected &&
                                            rx_alive &&
                                            _last_tx_ok_ms &&
                                            (tx_idle_ms >= kTxDeadWhileRxAliveMs);
        const bool repeated_tx_dead = connected &&
                                      _last_tx_ok_ms &&
                                      (tx_idle_ms >= kTxDeadWhileRxAliveMs) &&
                                      (_tx_fail_streak >= 2);
        const bool should_reset = tx_dead_while_rx_alive || repeated_tx_dead;
        if (_logger)
        {
            _logger->warn(F("CLOUD"),
                          F("WS transport send failed: connected: %u len: %u rx_idle_ms: %lu tx_idle_ms: %lu fail_streak: %lu"),
                          connected ? 1u : 0u,
                          (unsigned)payload.length(),
                          (unsigned long)rx_idle_ms,
                          (unsigned long)tx_idle_ms,
                          (unsigned long)_tx_fail_streak);
            if (should_reset)
            {
                _logger->warn(F("CLOUD"),
                              F("WS transport reset on tx stall: rx_idle_ms: %lu tx_idle_ms: %lu fail_streak: %lu len: %u"),
                              (unsigned long)rx_idle_ms,
                              (unsigned long)tx_idle_ms,
                              (unsigned long)_tx_fail_streak,
                              (unsigned)payload.length());
            }
        }
        if (should_reset)
        {
            _ws.disconnect();
        }
    }
    return ok;
}

void CloudWsTransport::onWsEvent_(WStype_t type, uint8_t *payload, size_t len)
{
    switch (type)
    {
    case WStype_CONNECTED:
        _last_connect_ms = millis();
        _last_rx_ms = _last_connect_ms;
        _last_tx_ok_ms = _last_connect_ms;
        _tx_fail_streak = 0;
        _disconnect_logged = false;
        if (_event_cb)
            _event_cb(_event_ctx, CloudTransport::Event::Connected, payload, len);
        break;
    case WStype_DISCONNECTED:
        if (_logger && !_disconnect_logged)
        {
            const uint32_t now = millis();
            const uint32_t connected_ms = _last_connect_ms ? (uint32_t)(now - _last_connect_ms) : 0u;
            const uint32_t rx_idle_ms = _last_rx_ms ? (uint32_t)(now - _last_rx_ms) : 0u;
            const uint32_t tx_idle_ms = _last_tx_ok_ms ? (uint32_t)(now - _last_tx_ok_ms) : 0u;
            _logger->warn(F("CLOUD"),
                          F("WS transport disconnected: connected_ms: %lu rx_idle_ms: %lu tx_idle_ms: %lu"),
                          (unsigned long)connected_ms,
                          (unsigned long)rx_idle_ms,
                          (unsigned long)tx_idle_ms);
        }
        _disconnect_logged = true;
        _last_rx_ms = 0;
        _last_tx_attempt_ms = 0;
        _last_tx_ok_ms = 0;
        _last_connect_ms = 0;
        _tx_fail_streak = 0;
        if (_event_cb)
            _event_cb(_event_ctx, CloudTransport::Event::Disconnected, payload, len);
        break;
    case WStype_ERROR:
        if (_event_cb)
            _event_cb(_event_ctx, CloudTransport::Event::Error, payload, len);
        break;
    case WStype_TEXT:
        _last_rx_ms = millis();
        if (_message_cb && payload && len)
            _message_cb(_message_ctx, payload, len);
        break;
    default:
        break;
    }
}
