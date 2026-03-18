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

#include "core/network/cloud/cloud_http_transport.hpp"

void CloudHttpTransport::setMessageHandler(CloudTransport::MessageHandler cb, void *ctx)
{
    _message_cb = cb;
    _message_ctx = ctx;
}

void CloudHttpTransport::setEventHandler(CloudTransport::EventHandler cb, void *ctx)
{
    _event_cb = cb;
    _event_ctx = ctx;
}

void CloudHttpTransport::begin(const CloudTransport::Config &cfg)
{
    _cfg = cfg;
    _configured = (_cfg.host.length() != 0 && _cfg.port != 0);
    _connected = false;
    _last_poll_ms = 0;
    _outbox = "";
    if (_cfg.path.length() == 0)
        _cfg.path = "/";
    _base_url = String(_cfg.use_ssl ? "https://" : "http://") + _cfg.host + ":" + String(_cfg.port) + _cfg.path;
}

void CloudHttpTransport::loop()
{
    if (!_configured)
        return;
    const uint32_t now = millis();
    if (_last_poll_ms != 0 && (uint32_t)(now - _last_poll_ms) < 30000u)
        return;
    _last_poll_ms = now;
    static const char kMsg[] = "HTTP transport placeholder: server device HTTP endpoints not implemented";
    notifyEvent_(CloudTransport::Event::Error, reinterpret_cast<const uint8_t *>(kMsg), sizeof(kMsg) - 1u);
}

void CloudHttpTransport::disconnect()
{
    if (_connected)
        notifyEvent_(CloudTransport::Event::Disconnected);
    _connected = false;
    _outbox = "";
    _http.end();
}

bool CloudHttpTransport::isConnected() const
{
    return _connected;
}

bool CloudHttpTransport::sendText(const String &text)
{
    if (!_configured || text.length() == 0)
        return false;
    _outbox = text;
    return false;
}

void CloudHttpTransport::notifyEvent_(CloudTransport::Event event, const uint8_t *payload, size_t len)
{
    if (_event_cb)
        _event_cb(_event_ctx, event, payload, len);
}
