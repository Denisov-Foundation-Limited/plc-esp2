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
    String payload = text;
    return _ws.sendTXT(payload);
}

void CloudWsTransport::onWsEvent_(WStype_t type, uint8_t *payload, size_t len)
{
    switch (type)
    {
    case WStype_CONNECTED:
        if (_event_cb)
            _event_cb(_event_ctx, CloudTransport::Event::Connected, payload, len);
        break;
    case WStype_DISCONNECTED:
        if (_event_cb)
            _event_cb(_event_ctx, CloudTransport::Event::Disconnected, payload, len);
        break;
    case WStype_ERROR:
        if (_event_cb)
            _event_cb(_event_ctx, CloudTransport::Event::Error, payload, len);
        break;
    case WStype_TEXT:
        if (_message_cb && payload && len)
            _message_cb(_message_ctx, payload, len);
        break;
    default:
        break;
    }
}
