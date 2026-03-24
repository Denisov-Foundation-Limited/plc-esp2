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

#include "core/network/stack/stack_rs485_transport.hpp"

#include "core/network/stack/stack_binary_protocol.hpp"
#include "core/network/stack/stack_json_protocol.hpp"
#include "utils/logger.hpp"

namespace
{
constexpr uint8_t kRs485TextType = 1;
constexpr uint8_t kRs485BinaryType = 2;
}

StackRs485Transport::StackRs485Transport(Logger &log) : _log(log)
{
}

void StackRs485Transport::setConfig(const Config &cfg)
{
    const auto guard = _lock.guard();
    _cfg = cfg;
}

StackRs485Transport::Config StackRs485Transport::config() const
{
    const auto guard = _lock.guard();
    return _cfg;
}

void StackRs485Transport::setEventHandler(EventHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _event_cb = cb;
    _event_ctx = ctx;
}

bool StackRs485Transport::begin(uint16_t port)
{
    const auto guard = _lock.guard();
    _logical_port = port;
    _started = true;
    _last_tx_size = 0;
    _bus_state = BusState::Idle;
    _state_since_ms = millis();
    _active_tx_index = 0xFFu;
    memset(_tx_queue, 0, sizeof(_tx_queue));
    memset(_pending, 0, sizeof(_pending));
    _request_timeout_count = 0;
    _tx_queue_drop_count = 0;
    _pending_full_drop_count = 0;
    _log.info(F("STACK"), F("RS485 stub started: port %u uart %u baud %lu"), (unsigned)port, (unsigned)_cfg.uart_index,
              (unsigned long)_cfg.baud_rate);
    return true;
}

void StackRs485Transport::loop()
{
    {
        const auto guard = _lock.guard();
        if (!_started)
            return;
    }
    checkPendingTimeouts_();
    processBusState_();
}

void StackRs485Transport::checkPendingTimeouts_()
{
    const uint32_t now = millis();
    const auto guard = _lock.guard();
    checkPendingTimeoutsLocked_(now);
}

void StackRs485Transport::checkPendingTimeoutsLocked_(uint32_t now)
{
    for (size_t i = 0; i < kPendingCap; ++i)
    {
        PendingEntry &entry = _pending[i];
        if (!entry.used || !expired_(now, entry.deadline_ms))
            continue;
        ++_request_timeout_count;
        _log.warn(F("STACK"), F("RS485 request timeout: req %lu client %u type %u"), (unsigned long)entry.request_id,
                  (unsigned)entry.client_id, (unsigned)entry.msg_type);
        entry.used = false;
    }
}

void StackRs485Transport::processBusState_()
{
    enum class StepAction : uint8_t
    {
        None = 0,
        StartNextTx,
        FinishActiveTx
    };

    StepAction action = StepAction::None;
    {
        const auto guard = _lock.guard();
        if (!_started)
            return;
        const uint32_t now = millis();
        switch (_bus_state)
        {
        case BusState::Idle:
            action = StepAction::StartNextTx;
            break;
        case BusState::TurnaroundTx:
            if (expired_(now, _state_since_ms + _cfg.turnaround_tx_ms))
            {
                _bus_state = BusState::Tx;
                _state_since_ms = now;
            }
            break;
        case BusState::Tx:
            action = StepAction::FinishActiveTx;
            break;
        case BusState::TurnaroundRx:
            if (expired_(now, _state_since_ms + _cfg.turnaround_rx_ms))
            {
                _bus_state = BusState::Idle;
                _state_since_ms = now;
                _active_tx_index = 0xFFu;
            }
            break;
        case BusState::Rx:
            if (expired_(now, _state_since_ms + _cfg.turnaround_rx_ms))
            {
                _bus_state = BusState::Idle;
                _state_since_ms = now;
            }
            break;
        }
    }

    if (action == StepAction::StartNextTx)
        startNextTx_();
    else if (action == StepAction::FinishActiveTx)
        finishActiveTx_();
}

void StackRs485Transport::startNextTx_()
{
    const auto guard = _lock.guard();
    if (!_started || _bus_state != BusState::Idle)
        return;
    for (size_t i = 0; i < kQueueCap; ++i)
    {
        if (!_tx_queue[i].used)
            continue;
        _active_tx_index = (uint8_t)i;
        _state_since_ms = millis();
        _bus_state = _cfg.half_duplex ? BusState::TurnaroundTx : BusState::Tx;
        return;
    }
}

void StackRs485Transport::finishActiveTx_()
{
    const auto guard = _lock.guard();
    if (_active_tx_index >= kQueueCap || !_tx_queue[_active_tx_index].used)
    {
        _bus_state = BusState::Idle;
        _state_since_ms = millis();
        _active_tx_index = 0xFFu;
        return;
    }

    TxEntry &entry = _tx_queue[_active_tx_index];
    _last_tx_size = entry.frame_size;
    memcpy(_last_tx, entry.frame, entry.frame_size);

    if (!addPending_(entry.client_id, entry.msg_type, entry.meta) &&
        entry.meta.exchange_kind == StackTransport::ExchangeKind::Request &&
        entry.meta.expect_response)
    {
        ++_pending_full_drop_count;
        _log.warn(F("STACK"), F("RS485 pending table full: req %lu"), (unsigned long)entry.meta.request_id);
    }

    entry.used = false;
    _state_since_ms = millis();
    _bus_state = _cfg.half_duplex ? BusState::TurnaroundRx : BusState::Idle;
    if (!_cfg.half_duplex)
        _active_tx_index = 0xFFu;
}

void StackRs485Transport::stop()
{
    const auto guard = _lock.guard();
    _started = false;
    _last_tx_size = 0;
    _bus_state = BusState::Idle;
    _active_tx_index = 0xFFu;
    memset(_tx_queue, 0, sizeof(_tx_queue));
    memset(_pending, 0, sizeof(_pending));
    _request_timeout_count = 0;
    _tx_queue_drop_count = 0;
    _pending_full_drop_count = 0;
}

StackTransport::Caps StackRs485Transport::caps() const
{
    StackTransport::Caps out;
    out.full_duplex = false;
    out.can_push_async = false;
    out.requires_arbitration = true;
    out.supports_request_response = true;
    out.ordered_delivery = true;
    return out;
}

bool StackRs485Transport::sendText(uint8_t client_id, const char *text)
{
    const auto guard = _lock.guard();
    if (!text || !text[0])
        return false;
    return sendFrame_(client_id, kRs485TextType, reinterpret_cast<const uint8_t *>(text), strlen(text));
}

bool StackRs485Transport::sendBinary(uint8_t client_id, const uint8_t *data, size_t size)
{
    const auto guard = _lock.guard();
    if (!data || size == 0)
        return false;
    return sendFrame_(client_id, kRs485BinaryType, data, size);
}

bool StackRs485Transport::disconnectClient(uint8_t client_id)
{
    const auto guard = _lock.guard();
    (void)client_id;
    return true;
}

bool StackRs485Transport::injectFrame(const uint8_t *frame, size_t size, uint8_t client_id)
{
    EventHandler event_cb = nullptr;
    void *event_ctx = nullptr;
    StackTransport::Event event;
    {
        const auto guard = _lock.guard();
        if (!_started)
            return false;

        StackRs485Protocol::FrameView parsed;
        if (!StackRs485Protocol::decode(frame, size, parsed))
            return false;

        _bus_state = BusState::Rx;
        _state_since_ms = millis();

        StackTransport::RouteMeta meta;
        if (tryParseRouteMeta_(parsed.msg_type, parsed.payload, parsed.payload_size, meta))
            completePending_(meta);

        event_cb = _event_cb;
        event_ctx = _event_ctx;
        if (!event_cb)
            return true;

        event.client_id = client_id;
        event.ip = IPAddress(0, 0, 0, 0);
        event.data = parsed.payload;
        event.size = parsed.payload_size;
        event.type = (parsed.msg_type == kRs485TextType) ? StackTransport::EventType::TextMessage
                                                         : StackTransport::EventType::BinaryMessage;
    }

    event_cb(event_ctx, event);
    return true;
}

size_t StackRs485Transport::lastTxSize() const
{
    const auto guard = _lock.guard();
    return _last_tx_size;
}

bool StackRs485Transport::copyLastTxData(uint8_t *out, size_t cap, size_t &out_size) const
{
    const auto guard = _lock.guard();
    out_size = _last_tx_size;
    if (!out)
        return _last_tx_size == 0;
    if (cap < _last_tx_size)
        return false;
    if (_last_tx_size != 0)
        memcpy(out, _last_tx, _last_tx_size);
    return true;
}

StackRs485Transport::Diagnostics StackRs485Transport::diagnostics() const
{
    const auto guard = _lock.guard();
    Diagnostics out;
    out.request_timeouts = _request_timeout_count;
    out.tx_queue_drops = _tx_queue_drop_count;
    out.pending_full_drops = _pending_full_drop_count;
    out.last_tx_size = _last_tx_size;
    out.lock_held_ms = _lock.heldMs();
    out.bus_state = _bus_state;
    out.started = _started;
    for (size_t i = 0; i < kQueueCap; ++i)
    {
        if (_tx_queue[i].used)
            ++out.tx_queue_used;
    }
    for (size_t i = 0; i < kPendingCap; ++i)
    {
        if (_pending[i].used)
            ++out.pending_used;
    }
    return out;
}

bool StackRs485Transport::sendFrame_(uint8_t client_id, uint8_t msg_type, const uint8_t *payload, size_t size)
{
    if (!_started || !_cfg.max_frame_size)
        return false;
    StackTransport::RouteMeta meta;
    if (!tryParseRouteMeta_(msg_type, payload, size, meta))
        meta = StackTransport::RouteMeta{};
    assignRequestId_(meta);

    uint8_t route_payload[kTxBufferCap] = {};
    const uint8_t *wire_payload = payload;
    size_t wire_payload_size = size;

    if (msg_type == kRs485TextType)
    {
        StackJsonProtocol::RouteMessage route;
        if (payload && size && StackJsonProtocol::parseRoute(payload, size, route))
        {
            route.meta = meta;
            const String normalized =
                StackJsonProtocol::makeRoute(route.source_node, route.target_node, route.feature, route.action,
                                             route.payload_json, &route.meta);
            wire_payload = reinterpret_cast<const uint8_t *>(normalized.c_str());
            wire_payload_size = normalized.length();
            if (wire_payload_size > sizeof(route_payload))
                return false;
            memcpy(route_payload, wire_payload, wire_payload_size);
            wire_payload = route_payload;
        }
    }
    else if (msg_type == kRs485BinaryType)
    {
        StackBinaryProtocol::RouteFrame route;
        if (payload && size && StackBinaryProtocol::parseRoute(payload, size, route))
        {
            route.meta = meta;
            size_t used = 0;
            if (!StackBinaryProtocol::encodeRoute(route.source_node, route.target_node, route.feature, route.action, route.payload,
                                                  route.payload_size, route_payload, sizeof(route_payload), used, &route.meta))
                return false;
            wire_payload = route_payload;
            wire_payload_size = used;
        }
    }

    if (wire_payload_size > 0xFFu)
        return false;
    const size_t need = StackRs485Protocol::encodedSize(wire_payload_size);
    if (need > _cfg.max_frame_size || need > kTxBufferCap)
        return false;
    uint8_t encoded[kTxBufferCap] = {};
    size_t used = 0;
    if (!StackRs485Protocol::encode(0, msg_type, wire_payload, wire_payload_size, encoded, sizeof(encoded), used))
        return false;
    return enqueueFrame_(client_id, msg_type, encoded, used, meta);
}

bool StackRs485Transport::enqueueFrame_(uint8_t client_id, uint8_t msg_type, const uint8_t *frame, size_t frame_size,
                                        const StackTransport::RouteMeta &meta)
{
    if (!frame || frame_size == 0)
        return false;
    const int slot = findFreeQueueSlot_();
    if (slot < 0)
    {
        ++_tx_queue_drop_count;
        _log.warn(F("STACK"), F("RS485 tx queue full: client %u type %u"), (unsigned)client_id, (unsigned)msg_type);
        return false;
    }
    TxEntry &entry = _tx_queue[slot];
    entry.used = true;
    entry.client_id = client_id;
    entry.msg_type = msg_type;
    entry.meta = meta;
    entry.frame_size = frame_size;
    memcpy(entry.frame, frame, frame_size);
    if (_bus_state == BusState::Idle)
        processBusState_();
    return true;
}

bool StackRs485Transport::tryParseRouteMeta_(uint8_t msg_type, const uint8_t *payload, size_t size, StackTransport::RouteMeta &meta)
{
    meta = StackTransport::RouteMeta{};
    if (!payload || size == 0)
        return false;

    if (msg_type == kRs485TextType)
    {
        StackJsonProtocol::RouteMessage route;
        if (!StackJsonProtocol::parseRoute(payload, size, route))
            return false;
        meta = route.meta;
        return true;
    }

    if (msg_type == kRs485BinaryType)
    {
        StackBinaryProtocol::RouteFrame route;
        if (!StackBinaryProtocol::parseRoute(payload, size, route))
            return false;
        meta = route.meta;
        return true;
    }

    return false;
}

void StackRs485Transport::assignRequestId_(StackTransport::RouteMeta &meta)
{
    if (meta.exchange_kind != StackTransport::ExchangeKind::Request || meta.request_id != 0)
        return;
    meta.request_id = _next_request_id++;
    if (_next_request_id == 0)
        _next_request_id = 1;
}

void StackRs485Transport::completePending_(const StackTransport::RouteMeta &meta)
{
    uint32_t matched = 0;
    if (meta.exchange_kind == StackTransport::ExchangeKind::Response)
        matched = meta.reply_to ? meta.reply_to : meta.request_id;
    else if (meta.exchange_kind == StackTransport::ExchangeKind::Event && meta.reply_to)
        matched = meta.reply_to;

    if (!matched)
        return;

    const int idx = findPendingByRequestId_(matched);
    if (idx < 0)
        return;

    _pending[idx].used = false;
    _log.debug(F("STACK"), F("RS485 pending complete: req %lu"), (unsigned long)matched);
}

bool StackRs485Transport::addPending_(uint8_t client_id, uint8_t msg_type, const StackTransport::RouteMeta &meta)
{
    if (meta.exchange_kind != StackTransport::ExchangeKind::Request || !meta.expect_response || meta.request_id == 0)
        return true;

    const int existing = findPendingByRequestId_(meta.request_id);
    if (existing >= 0)
    {
        _pending[existing].deadline_ms = millis() + _cfg.request_timeout_ms;
        return true;
    }

    for (size_t i = 0; i < kPendingCap; ++i)
    {
        if (_pending[i].used)
            continue;
        _pending[i].used = true;
        _pending[i].client_id = client_id;
        _pending[i].msg_type = msg_type;
        _pending[i].request_id = meta.request_id;
        _pending[i].deadline_ms = millis() + _cfg.request_timeout_ms;
        return true;
    }
    return false;
}

int StackRs485Transport::findFreeQueueSlot_() const
{
    for (size_t i = 0; i < kQueueCap; ++i)
    {
        if (!_tx_queue[i].used)
            return (int)i;
    }
    return -1;
}

int StackRs485Transport::findPendingByRequestId_(uint32_t request_id) const
{
    if (request_id == 0)
        return -1;
    for (size_t i = 0; i < kPendingCap; ++i)
    {
        if (_pending[i].used && _pending[i].request_id == request_id)
            return (int)i;
    }
    return -1;
}

bool StackRs485Transport::expired_(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}
