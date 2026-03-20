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

#include "core/network/stack/stack_node.hpp"
#include "core/network/stack/stack_types.hpp"

StackNode::StackNode(StackTransportClient &client, Logger &log) : _log(&log), _client(&client) {}

void StackNode::setFrameHandler(FrameHandler cb, void *ctx)
{
    _frame_cb = cb;
    _frame_ctx = ctx;
}

void StackNode::setNodeId(uint32_t id) { _node_id = id; }

void StackNode::setDeviceName(const String &name) { _device_name = name; }

void StackNode::setCaps(uint32_t caps) { _caps = caps; }

void StackNode::setServer(const String &host, uint16_t port)
{
    _host = host;
    _port = port;
}

void StackNode::setReconnectMs(uint32_t ms) { _reconnect_ms = ms; }

void StackNode::setHelloIntervalMs(uint32_t ms) { _hello_interval_ms = ms; }

void StackNode::setStatusIntervalMs(uint32_t ms) { _status_interval_ms = ms; }

void StackNode::setStatusProvider(StatusProvider cb, void *ctx)
{
    _status_cb = cb;
    _status_ctx = ctx;
}

void StackNode::disconnect()
{
    if (_client && _client->connected())
        _client->close(true);
}

const String &StackNode::host() const { return _host; }

uint16_t StackNode::port() const { return _port; }

void StackNode::begin()
{
    if (_host.length() == 0 || _port == 0)
    {
        if (_log)
            _log->warn(F("STACK"), F("Unit begin skipped, host/port missing"));
        return;
    }
    if (_log)
        _log->info(F("STACK"), F("Unit begin: %s:%u"), _host.c_str(), (unsigned)_port);
    setupClient_();
    connect_();
}

void StackNode::loop()
{
    flushQueuedTx_();
    logTxStats_();
    if (_client && _client->connected())
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

bool StackNode::send(uint8_t type, const uint8_t *payload, size_t len)
{
    if (sendNow_(type, payload, len))
        return true;
    return enqueueTx_(type, payload, len);
}

bool StackNode::connected() const { return _client && _client->connected(); }

bool StackNode::helloSentCurrentConnection() const { return _hello_sent_current_connection; }

StackNode::TxStats StackNode::txStats() const
{
    TxStats st = _tx_stats;
    for (const auto &q : _queued_tx)
        if (q.used && st.depth < 0xFF)
            ++st.depth;
    return st;
}

bool StackNode::sendHello(uint16_t fw_ver, uint32_t caps)
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
    const bool ok = send((uint8_t)StackMsgType::Hello, _tx_payload_buf, payload_len);
    if (ok && _client && _client->connected())
        _hello_sent_current_connection = true;
    return ok;
}

void StackNode::setupClient_()
{
    if (!_client)
        return;
    _client->setDataHandler(
        [](void *ctx, StackTransportConnection *, const uint8_t *data, size_t len) {
            if (!ctx)
                return;
            static_cast<StackNode *>(ctx)->onData_(data, len);
        },
        this);
    _client->setConnectHandler(
        [](void *ctx, StackTransportConnection *) {
            if (!ctx)
                return;
            static_cast<StackNode *>(ctx)->onConnect_();
        },
        this);
    _client->setDisconnectHandler(
        [](void *ctx, StackTransportConnection *) {
            if (!ctx)
                return;
            static_cast<StackNode *>(ctx)->onDisconnect_();
        },
        this);
    _client->setErrorHandler(
        [](void *ctx, StackTransportConnection *, int8_t) {
            if (!ctx)
                return;
            static_cast<StackNode *>(ctx)->onDisconnect_();
        },
        this);
}

bool StackNode::sendNow_(uint8_t type, const uint8_t *payload, size_t len)
{
    if (!_client || !_client->connected())
        return false;
    const size_t frame_len = StackCodec::encode(type, payload, len, _tx_frame_buf, sizeof(_tx_frame_buf));
    if (frame_len == 0)
        return false;
    if (!_client->canSend())
    {
        if (_log)
            _log->warn(F("STACK"), F("Unit tx busy: type: %u len: %u"),
                       (unsigned)type, (unsigned)frame_len);
        return false;
    }
    const size_t written = _client->write(_tx_frame_buf, frame_len);
    if (written != frame_len)
    {
        if (_log)
            _log->warn(F("STACK"), F("Unit tx short write: type: %u wr: %u len: %u"),
                       (unsigned)type, (unsigned)written, (unsigned)frame_len);
        return false;
    }
    return true;
}

bool StackNode::enqueueTx_(uint8_t type, const uint8_t *payload, size_t len)
{
    if (!payload || len == 0 || len > StackCodec::kMaxPayload)
        return false;
    for (auto &q : _queued_tx)
    {
        if (!q.used)
            continue;
        if (q.type != type || q.len != len)
            continue;
        if (memcmp(q.payload, payload, len) != 0)
            continue;
        ++_tx_stats.coalesced;
        q.attempts = 1;
        q.next_retry_ms = millis() + kTxRetryMs;
        return true;
    }
    for (auto &q : _queued_tx)
    {
        if (q.used)
            continue;
        q.used = true;
        q.type = type;
        q.len = (uint16_t)len;
        q.attempts = 1;
        q.next_retry_ms = millis() + kTxRetryMs;
        memcpy(q.payload, payload, len);
        ++_tx_stats.queued;
        return true;
    }
    if (_log)
        _log->warn(F("STACK"), F("Unit tx queue full: type: %u len: %u"),
                   (unsigned)type, (unsigned)len);
    ++_tx_stats.queue_full;
    return false;
}

void StackNode::flushQueuedTx_()
{
    if (!_client || !_client->connected())
        return;
    const uint32_t now = millis();
    for (auto &q : _queued_tx)
    {
        if (!q.used)
            continue;
        if ((int32_t)(now - q.next_retry_ms) < 0)
            continue;
        if (sendNow_(q.type, q.payload, q.len))
        {
            q = QueuedTx{};
            continue;
        }
        ++q.attempts;
        ++_tx_stats.retries;
        if (q.attempts >= kTxMaxAttempts)
        {
            if (_log)
                _log->warn(F("STACK"), F("Unit tx dropped: type: %u"), (unsigned)q.type);
            ++_tx_stats.dropped;
            q = QueuedTx{};
            continue;
        }
        q.next_retry_ms = now + kTxRetryMs;
    }
}

void StackNode::connect_()
{
    _last_connect_ms = millis();
    if (_client)
        _client->connect(_host.c_str(), _port);
}

void StackNode::onConnect_()
{
    _hello_sent_current_connection = false;
    sendHello();
    const uint32_t now = millis();
    _last_hello_ms = now;
    _last_status_ms = now;
    sendStatus_();
}

void StackNode::onDisconnect_()
{
    _hello_sent_current_connection = false;
    _codec.clear();
}

void StackNode::onData_(const uint8_t *data, size_t len)
{
    _codec.feed(
        data, len,
        [](void *ctx, const StackFrame &frame) {
            StackNode *self = static_cast<StackNode *>(ctx);
            self->handleFrame_(frame);
        },
        this);
}

void StackNode::handleFrame_(const StackFrame &frame)
{
    if (_frame_cb)
        _frame_cb(_frame_ctx, frame);
}

void StackNode::sendStatus_()
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

void StackNode::logTxStats_()
{
    if (!_log)
        return;
    const uint32_t now = millis();
    if ((uint32_t)(now - _last_stats_log_ms) < kStatsLogMs)
        return;
    _last_stats_log_ms = now;
    const auto st = txStats();
    if (st.depth == _last_logged_tx_stats.depth &&
        st.queued == _last_logged_tx_stats.queued &&
        st.retries == _last_logged_tx_stats.retries &&
        st.coalesced == _last_logged_tx_stats.coalesced &&
        st.dropped == _last_logged_tx_stats.dropped &&
        st.queue_full == _last_logged_tx_stats.queue_full)
        return;
    _last_logged_tx_stats = st;
    _log->debug(F("STACK"),
                F("Unit tx stats: depth: %u queued: %lu retries: %lu coalesced: %lu dropped: %lu full: %lu"),
                (unsigned)st.depth,
                (unsigned long)st.queued,
                (unsigned long)st.retries,
                (unsigned long)st.coalesced,
                (unsigned long)st.dropped,
                (unsigned long)st.queue_full);
}
