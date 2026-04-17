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

#include "core/network/stack/stack_route_adapter.hpp"

#include <memory>
#include <vector>

#include "core/network/stack/stack_device_registry.hpp"
#include "core/network/stack/stack_data_binary_codec.hpp"
#include "core/network/stack/stack_master_server.hpp"
#include "core/network/stack/stack_rs485_server.hpp"
#include "core/network/stack/stack_slave_client.hpp"
#include "utils/logger.hpp"

namespace
{
bool expiredMs_(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

int8_t b64Index_(char c)
{
    if (c >= 'A' && c <= 'Z')
        return (int8_t)(c - 'A');
    if (c >= 'a' && c <= 'z')
        return (int8_t)(26 + (c - 'a'));
    if (c >= '0' && c <= '9')
        return (int8_t)(52 + (c - '0'));
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

char b64Char_(uint8_t v)
{
    static const char *kTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    return kTable[v & 0x3Fu];
}

void appendU8_(std::vector<uint8_t> &out, uint8_t v)
{
    out.push_back(v);
}

void appendU16_(std::vector<uint8_t> &out, uint16_t v)
{
    out.push_back((uint8_t)(v & 0xFFu));
    out.push_back((uint8_t)((v >> 8) & 0xFFu));
}

void appendU32_(std::vector<uint8_t> &out, uint32_t v)
{
    out.push_back((uint8_t)(v & 0xFFu));
    out.push_back((uint8_t)((v >> 8) & 0xFFu));
    out.push_back((uint8_t)((v >> 16) & 0xFFu));
    out.push_back((uint8_t)((v >> 24) & 0xFFu));
}

void appendBytes_(std::vector<uint8_t> &out, const uint8_t *data, size_t size)
{
    if (!data || size == 0)
        return;
    out.insert(out.end(), data, data + size);
}

bool readU8_(const uint8_t *&p, size_t &left, uint8_t &out)
{
    if (!p || left < 1)
        return false;
    out = *p++;
    --left;
    return true;
}

bool readU16_(const uint8_t *&p, size_t &left, uint16_t &out)
{
    if (!p || left < 2)
        return false;
    out = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    p += 2;
    left -= 2;
    return true;
}

bool readU32_(const uint8_t *&p, size_t &left, uint32_t &out)
{
    if (!p || left < 4)
        return false;
    out = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    p += 4;
    left -= 4;
    return true;
}

bool readBytes_(const uint8_t *&p, size_t &left, const uint8_t *&data, size_t size)
{
    if (!p || left < size)
        return false;
    data = p;
    p += size;
    left -= size;
    return true;
}

String encodeBase64Raw_(const uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return String();
    String out;
    out.reserve(((len + 2u) / 3u) * 4u);
    for (size_t i = 0; i < len; i += 3u)
    {
        const uint32_t a = data[i];
        const uint32_t b = (i + 1u < len) ? data[i + 1u] : 0u;
        const uint32_t c = (i + 2u < len) ? data[i + 2u] : 0u;
        const uint32_t chunk = (a << 16) | (b << 8) | c;
        out += b64Char_((uint8_t)((chunk >> 18) & 0x3Fu));
        out += b64Char_((uint8_t)((chunk >> 12) & 0x3Fu));
        out += (i + 1u < len) ? b64Char_((uint8_t)((chunk >> 6) & 0x3Fu)) : '=';
        out += (i + 2u < len) ? b64Char_((uint8_t)(chunk & 0x3Fu)) : '=';
    }
    return out;
}

bool decodeBase64Raw_(const String &in, std::vector<uint8_t> &out)
{
    out.clear();
    if (!in.length())
        return true;
    out.reserve((in.length() / 4u) * 3u);
    uint8_t block[4] = {};
    uint8_t block_len = 0;
    uint8_t pad = 0;
    for (size_t i = 0; i < in.length(); ++i)
    {
        const char ch = in[i];
        if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t')
            continue;
        if (ch == '=')
        {
            block[block_len++] = 0;
            ++pad;
        }
        else
        {
            const int8_t v = b64Index_(ch);
            if (v < 0)
                return false;
            block[block_len++] = (uint8_t)v;
        }
        if (block_len != 4)
            continue;
        const uint32_t chunk =
            ((uint32_t)block[0] << 18) | ((uint32_t)block[1] << 12) | ((uint32_t)block[2] << 6) | (uint32_t)block[3];
        out.push_back((uint8_t)((chunk >> 16) & 0xFFu));
        if (pad < 2)
            out.push_back((uint8_t)((chunk >> 8) & 0xFFu));
        if (pad == 0)
            out.push_back((uint8_t)(chunk & 0xFFu));
        block_len = 0;
        pad = 0;
    }
    return block_len == 0;
}

String textFromBytes_(const uint8_t *data, size_t size)
{
    String out;
    if (!data || size == 0)
        return out;
    out.reserve(size);
    for (size_t i = 0; i < size; ++i)
        out += (char)data[i];
    return out;
}

String clampUtf8Bytes_(const char *text, size_t max_bytes)
{
    if (!text || !text[0] || max_bytes == 0)
        return String();
    const String in(text);
    if ((size_t)in.length() <= max_bytes)
        return in;

    String out;
    out.reserve(max_bytes);
    size_t i = 0;
    while (i < (size_t)in.length())
    {
        const uint8_t c = (uint8_t)in[i];
        size_t need = 1;
        if ((c & 0x80u) == 0)
            need = 1;
        else if ((c & 0xE0u) == 0xC0u)
            need = 2;
        else if ((c & 0xF0u) == 0xE0u)
            need = 3;
        else if ((c & 0xF8u) == 0xF0u)
            need = 4;
        if (out.length() + need > max_bytes || i + need > (size_t)in.length())
            break;
        for (size_t j = 0; j < need; ++j)
            out += in[i + j];
        i += need;
    }
    return out;
}
}

StackRouteAdapter::StackRouteAdapter(Logger &log) : _log(log)
{
}

void StackRouteAdapter::bindMaster(StackMasterServer &master)
{
    const auto guard = _lock.guard();
    _master = &master;
    _master->setRouteHandler(&StackRouteAdapter::onMasterJsonRoute_, this);
    _master->setBinaryRouteHandler(&StackRouteAdapter::onMasterBinaryRoute_, this);
    _master->setNotificationHandler(&StackRouteAdapter::onMasterNotify_, this);
}

void StackRouteAdapter::bindRs485Master(StackRs485Server &master)
{
    const auto guard = _lock.guard();
    _rs485_master = &master;
    _rs485_master->setRouteHandler(&StackRouteAdapter::onMasterJsonRoute_, this);
    _rs485_master->setBinaryRouteHandler(&StackRouteAdapter::onMasterBinaryRoute_, this);
    _rs485_master->setNotificationHandler(&StackRouteAdapter::onMasterNotify_, this);
}

void StackRouteAdapter::bindSlave(StackSlaveClient &slave)
{
    const auto guard = _lock.guard();
    _slave = &slave;
    _slave->setRouteHandler(&StackRouteAdapter::onSlaveJsonRoute_, this);
    _slave->setBinaryRouteHandler(&StackRouteAdapter::onSlaveBinaryRoute_, this);
    _slave->setNotificationHandler(&StackRouteAdapter::onSlaveNotify_, this);
}

void StackRouteAdapter::setLocalNodeId(uint32_t node_id)
{
    const auto guard = _lock.guard();
    _local_node_id = node_id;
}

uint32_t StackRouteAdapter::localNodeId() const
{
    const auto guard = _lock.guard();
    return _local_node_id;
}

void StackRouteAdapter::setExchangePolicy(ExchangePolicy policy)
{
    const auto guard = _lock.guard();
    _exchange_policy = policy;
}

StackRouteAdapter::ExchangePolicy StackRouteAdapter::exchangePolicy() const
{
    const auto guard = _lock.guard();
    return _exchange_policy;
}

void StackRouteAdapter::setPayloadMode(PayloadMode mode)
{
    const auto guard = _lock.guard();
    _payload_mode = mode;
}

StackRouteAdapter::PayloadMode StackRouteAdapter::payloadMode() const
{
    const auto guard = _lock.guard();
    return _payload_mode;
}

void StackRouteAdapter::setRuntimeBindings(bool master_ws_active, bool master_rs485_active, bool slave_active)
{
    const auto guard = _lock.guard();
    _master_ws_active = master_ws_active;
    _master_rs485_active = master_rs485_active;
    _slave_active = slave_active;
}

void StackRouteAdapter::loop()
{
    const uint32_t now = millis();
    bool start_exchange_poll = false;
    bool start_notify_poll = false;
    {
        const auto guard = _lock.guard();
        if (!hasRs485PollBackend_())
            return;

        expireMasterInbox_(now);
        if (_exchange_sync.phase != ExchangeSyncPhase::Idle && expiredMs_(now, _exchange_sync.deadline_ms))
        {
            _log.warn(F("STACK"), F("Exchange sync timeout: node 0x%08lX"),
                      (unsigned long)_exchange_sync.node_id);
            resetExchangeSync_();
        }

        if (_notify_sync.phase != NotifySyncPhase::Idle && expiredMs_(now, _notify_sync.deadline_ms))
        {
            _log.warn(F("STACK"), F("Notify sync timeout: node 0x%08lX phase %u"),
                      (unsigned long)_notify_sync.node_id,
                      (unsigned)_notify_sync.phase);
            resetNotifySync_();
        }

        if (_exchange_sync.phase == ExchangeSyncPhase::Idle && expiredMs_(now, _exchange_last_poll_ms + kExchangePollIntervalMs))
        {
            _exchange_last_poll_ms = now;
            start_exchange_poll = true;
        }

        if (_notify_sync.phase == NotifySyncPhase::Idle && expiredMs_(now, _notify_last_poll_ms + 250u))
        {
            _notify_last_poll_ms = now;
            start_notify_poll = true;
        }
    }

    if (start_exchange_poll)
        tryStartExchangePoll_();
    if (start_notify_poll)
        tryStartNotificationPoll_();
}

void StackRouteAdapter::setJsonRouteHandler(JsonRouteHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _json_cb = cb;
    _json_ctx = ctx;
}

void StackRouteAdapter::setBinaryRouteHandler(BinaryRouteHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _binary_cb = cb;
    _binary_ctx = ctx;
}

void StackRouteAdapter::setNotificationHandler(NotificationHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _notify_cb = cb;
    _notify_ctx = ctx;
}

StackTransport::RouteMeta StackRouteAdapter::makeEventMeta()
{
    StackTransport::RouteMeta meta;
    meta.exchange_kind = StackTransport::ExchangeKind::Event;
    meta.expect_response = false;
    return meta;
}

StackTransport::RouteMeta StackRouteAdapter::makeRequestMeta(bool expect_response, uint32_t request_id)
{
    StackTransport::RouteMeta meta;
    meta.exchange_kind = StackTransport::ExchangeKind::Request;
    meta.expect_response = expect_response;
    meta.request_id = request_id;
    return meta;
}

StackTransport::RouteMeta StackRouteAdapter::makeResponseMeta(uint32_t reply_to, uint32_t request_id)
{
    StackTransport::RouteMeta meta;
    meta.exchange_kind = StackTransport::ExchangeKind::Response;
    meta.reply_to = reply_to;
    meta.request_id = request_id;
    meta.expect_response = false;
    return meta;
}

bool StackRouteAdapter::sendRoute(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload,
                                  Mode mode, const StackTransport::RouteMeta *meta)
{
    std::vector<uint8_t> payload_binary;
    (void)feature;
    (void)action;
    if (mode == Mode::Json)
        return sendRouteJson(target_node, feature, action, payload, meta);
    if (payload && !StackDataBinaryCodec::encode(feature, action, payload->as<JsonVariantConst>(), payload_binary))
        return false;
    return sendRouteBinary(target_node, feature, action, payload_binary.empty() ? nullptr : payload_binary.data(),
                           payload_binary.size(), meta);
}

bool StackRouteAdapter::sendRouteJson(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload,
                                      const StackTransport::RouteMeta *meta)
{
    enum class JsonPath : uint8_t
    {
        None = 0,
        SlaveDirect,
        SlaveQueue,
        MasterWs,
        MasterRs485
    };

    JsonPath path = JsonPath::None;
    {
        const auto guard = _lock.guard();
        if (_slave && meta && meta->exchange_kind == StackTransport::ExchangeKind::Response)
            path = JsonPath::SlaveDirect;
        else if (canSlavePushDirect_())
            path = JsonPath::SlaveDirect;
        else if (_master_ws_active && _master)
            path = JsonPath::MasterWs;
        else if (_master_rs485_active && _rs485_master)
            path = JsonPath::MasterRs485;
        else if (_slave && feature && strcmp(feature, kXchgFeature) != 0)
            path = JsonPath::SlaveQueue;
        else if (_slave)
            path = JsonPath::SlaveDirect;
        else if (_master)
            path = JsonPath::MasterWs;
        else if (_rs485_master)
            path = JsonPath::MasterRs485;
    }

    if (path == JsonPath::SlaveDirect)
    {
        if (_slave->sendRoute(target_node, feature, action, payload, meta))
            return true;
        if (_slave && feature && strcmp(feature, kXchgFeature) != 0 &&
            !(meta && meta->exchange_kind == StackTransport::ExchangeKind::Response))
        {
            String payload_text;
            if (payload)
                serializeJson(*payload, payload_text);
            return queueExchangeFromSlave_(target_node, feature, action, payload_text, meta, false);
        }
        return false;
    }
    if (path == JsonPath::SlaveQueue)
    {
        String payload_text;
        if (payload)
            serializeJson(*payload, payload_text);
        return queueExchangeFromSlave_(target_node, feature, action, payload_text, meta, false);
    }
    if (path == JsonPath::MasterWs)
        return sendMasterJson_(target_node, feature, action, payload, meta);
    if (path == JsonPath::MasterRs485)
        return sendRs485Json_(target_node, feature, action, payload, meta);
    return false;
}

bool StackRouteAdapter::sendRouteBinary(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload,
                                        size_t payload_size, const StackTransport::RouteMeta *meta)
{
    enum class BinaryPath : uint8_t
    {
        None = 0,
        SlaveDirect,
        SlaveQueue,
        MasterWs,
        MasterRs485
    };

    BinaryPath path = BinaryPath::None;
    {
        const auto guard = _lock.guard();
        if (_slave && meta && meta->exchange_kind == StackTransport::ExchangeKind::Response)
            path = BinaryPath::SlaveDirect;
        else if (canSlavePushDirect_())
            path = BinaryPath::SlaveDirect;
        else if (_master_ws_active && _master)
            path = BinaryPath::MasterWs;
        else if (_master_rs485_active && _rs485_master)
            path = BinaryPath::MasterRs485;
        else if (_slave && feature && strcmp(feature, kXchgFeature) != 0)
            path = BinaryPath::SlaveQueue;
        else if (_slave)
            path = BinaryPath::SlaveDirect;
        else if (_master)
            path = BinaryPath::MasterWs;
        else if (_rs485_master)
            path = BinaryPath::MasterRs485;
    }

    if (path == BinaryPath::SlaveDirect)
    {
        if (_slave->sendRouteBinary(target_node, feature, action, payload, payload_size, meta))
            return true;
        if (_slave && feature && strcmp(feature, kXchgFeature) != 0 &&
            !(meta && meta->exchange_kind == StackTransport::ExchangeKind::Response))
            return queueExchangeFromSlave_(target_node, feature, action, encodeBase64_(payload, payload_size), meta, true);
        return false;
    }
    if (path == BinaryPath::SlaveQueue)
        return queueExchangeFromSlave_(target_node, feature, action, encodeBase64_(payload, payload_size), meta, true);
    if (path == BinaryPath::MasterWs)
        return sendMasterBinary_(target_node, feature, action, payload, payload_size, meta);
    if (path == BinaryPath::MasterRs485)
        return sendRs485Binary_(target_node, feature, action, payload, payload_size, meta);
    return false;
}

bool StackRouteAdapter::sendEvent(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload,
                                  Mode mode)
{
    const StackTransport::RouteMeta meta = makeEventMeta();
    return sendRoute(target_node, feature, action, payload, mode, &meta);
}

bool StackRouteAdapter::sendRequest(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload,
                                    Mode mode, bool expect_response, uint32_t request_id)
{
    if (expect_response && request_id == 0)
        request_id = nextInternalRequestId_();
    const StackTransport::RouteMeta meta = makeRequestMeta(expect_response, request_id);
    return sendRoute(target_node, feature, action, payload, mode, &meta);
}

bool StackRouteAdapter::sendResponse(uint32_t target_node, const char *feature, const char *action, uint32_t reply_to,
                                     const JsonDocument *payload, Mode mode, uint32_t request_id)
{
    if (reply_to == 0)
        return false;
    const StackTransport::RouteMeta meta = makeResponseMeta(reply_to, request_id);
    return sendRoute(target_node, feature, action, payload, mode, &meta);
}

bool StackRouteAdapter::sendEventSelected(ConfigsManagerIface::StackPayloadMode payload_mode, uint32_t target_node,
                                          const char *feature, const char *action, const JsonDocument *payload)
{
    return sendEvent(target_node, feature, action, payload, modeForDataPlane_(payload_mode));
}

bool StackRouteAdapter::sendRequestSelected(ConfigsManagerIface::StackPayloadMode payload_mode, uint32_t target_node,
                                            const char *feature, const char *action, const JsonDocument *payload,
                                            bool expect_response, uint32_t request_id)
{
    return sendRequest(target_node, feature, action, payload, modeForDataPlane_(payload_mode), expect_response,
                       request_id);
}

bool StackRouteAdapter::sendResponseSelected(ConfigsManagerIface::StackPayloadMode payload_mode, uint32_t target_node,
                                             const char *feature, const char *action, uint32_t reply_to,
                                             const JsonDocument *payload, uint32_t request_id)
{
    return sendResponse(target_node, feature, action, reply_to, payload, modeForDataPlane_(payload_mode), request_id);
}

bool StackRouteAdapter::sendEventBinary(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload,
                                        size_t payload_size)
{
    const StackTransport::RouteMeta meta = makeEventMeta();
    return sendRouteBinary(target_node, feature, action, payload, payload_size, &meta);
}

bool StackRouteAdapter::sendRequestBinary(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload,
                                          size_t payload_size, bool expect_response, uint32_t request_id)
{
    if (expect_response && request_id == 0)
        request_id = nextInternalRequestId_();
    const StackTransport::RouteMeta meta = makeRequestMeta(expect_response, request_id);
    return sendRouteBinary(target_node, feature, action, payload, payload_size, &meta);
}

bool StackRouteAdapter::sendResponseBinary(uint32_t target_node, const char *feature, const char *action, uint32_t reply_to,
                                           const uint8_t *payload, size_t payload_size, uint32_t request_id)
{
    if (reply_to == 0)
        return false;
    const StackTransport::RouteMeta meta = makeResponseMeta(reply_to, request_id);
    return sendRouteBinary(target_node, feature, action, payload, payload_size, &meta);
}

bool StackRouteAdapter::sendNotify(const char *level, const char *feature, const char *code, const char *message,
                                   const JsonDocument *payload)
{
    PayloadMode payload_mode = PayloadMode::Json;
    String payload_json;
    bool direct_push = false;
    bool has_slave = false;
    bool has_notify_cb = false;
    uint32_t local_node_id = 0;
    {
        const auto guard = _lock.guard();
        payload_mode = _payload_mode;
        direct_push = canSlavePushDirect_();
        has_slave = _slave != nullptr;
        has_notify_cb = _notify_cb != nullptr;
        local_node_id = _local_node_id;
    }
    if (payload_mode == PayloadMode::Binary && payload)
        return false;
    if (payload)
        serializeJson(*payload, payload_json);

    if (direct_push)
        return _slave->sendNotify(level, feature, code, message, payload);

    if (has_slave)
        return queueNotification_(local_node_id, payload_mode == PayloadMode::Binary, level, feature, code, message,
                                  payload ? &payload_json : nullptr);

    if (!has_notify_cb || !feature || !feature[0] || !level || !level[0])
        return false;

    return queueNotification_(local_node_id, payload_mode == PayloadMode::Binary, level, feature, code, message,
                              payload ? &payload_json : nullptr);
}

size_t StackRouteAdapter::exchangeHistoryCount() const
{
    const auto guard = _lock.guard();
    return _exchange_history_count;
}

bool StackRouteAdapter::exchangeHistoryAt(size_t idx, ExchangeHistoryRecord &out) const
{
    const auto guard = _lock.guard();
    if (idx >= _exchange_history_count)
        return false;
    const uint8_t slot = (uint8_t)((_exchange_history_head + idx) % kExchangeHistoryCap);
    if (!_exchange_history[slot].used)
        return false;
    out = _exchange_history[slot];
    return true;
}

StackRouteAdapter::ExchangeDiagnostics StackRouteAdapter::exchangeDiagnostics() const
{
    const auto guard = _lock.guard();
    ExchangeDiagnostics diag = _exchange_diag;
    diag.history_count = _exchange_history_count;
    diag.lock_held_ms = _lock.heldMs();
    diag.slave_outbox_used = 0;
    diag.master_inbox_used = 0;
    diag.notify_outbox_used = 0;
    for (size_t i = 0; i < kExchangeSlaveOutboxCap; ++i)
    {
        if (_exchange_slave_outbox[i].used)
            ++diag.slave_outbox_used;
    }
    for (size_t i = 0; i < kExchangeMasterInboxCap; ++i)
    {
        if (_exchange_master_inbox[i].used)
            ++diag.master_inbox_used;
    }
    for (size_t i = 0; i < kNotifyOutboxCap; ++i)
    {
        if (_notify_outbox[i].used)
            ++diag.notify_outbox_used;
    }
    return diag;
}

size_t StackRouteAdapter::notificationHistoryCount() const
{
    const auto guard = _lock.guard();
    return _notify_history_count;
}

bool StackRouteAdapter::notificationHistoryAt(size_t idx, NotificationRecord &out) const
{
    const auto guard = _lock.guard();
    if (idx >= _notify_history_count)
        return false;
    const uint8_t slot = (uint8_t)((_notify_history_head + idx) % kNotifyHistoryCap);
    if (!_notify_history[slot].used)
        return false;
    out = _notify_history[slot];
    return true;
}

void StackRouteAdapter::onMasterJsonRoute_(void *ctx, const StackDeviceRegistry::DeviceInfo &device,
                                           const StackJsonProtocol::RouteMessage &route)
{
    if (!ctx)
        return;
    static_cast<StackRouteAdapter *>(ctx)->handleMasterJsonRoute_(device.node_id, route);
}

void StackRouteAdapter::onMasterBinaryRoute_(void *ctx, const StackDeviceRegistry::DeviceInfo &device,
                                             const StackBinaryProtocol::RouteFrame &route)
{
    if (!ctx)
        return;
    static_cast<StackRouteAdapter *>(ctx)->handleMasterBinaryRoute_(device.node_id, route);
}

void StackRouteAdapter::onSlaveJsonRoute_(void *ctx, const StackJsonProtocol::RouteMessage &route)
{
    if (!ctx)
        return;
    static_cast<StackRouteAdapter *>(ctx)->handleSlaveJsonRoute_(route);
}

void StackRouteAdapter::onSlaveBinaryRoute_(void *ctx, const StackBinaryProtocol::RouteFrame &route)
{
    if (!ctx)
        return;
    static_cast<StackRouteAdapter *>(ctx)->handleSlaveBinaryRoute_(route);
}

void StackRouteAdapter::onMasterNotify_(void *ctx, const StackDeviceRegistry::DeviceInfo &device,
                                        const StackJsonProtocol::NotifyMessage &notify)
{
    if (!ctx)
        return;
    static_cast<StackRouteAdapter *>(ctx)->handleMasterNotify_(device.node_id, notify);
}

void StackRouteAdapter::onSlaveNotify_(void *ctx, const StackJsonProtocol::NotifyMessage &notify)
{
    if (!ctx)
        return;
    static_cast<StackRouteAdapter *>(ctx)->handleSlaveNotify_(notify);
}

void StackRouteAdapter::handleMasterJsonRoute_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route)
{
    JsonRouteHandler cb = nullptr;
    void *cb_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        if (handleExchangeRoute_(source_node, route))
            return;
        if (handleNotificationRoute_(source_node, route))
            return;
        cb = _json_cb;
        cb_ctx = _json_ctx;
    }
    if (cb)
        cb(cb_ctx, source_node, route);
}

void StackRouteAdapter::handleMasterBinaryRoute_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route)
{
    BinaryRouteHandler cb = nullptr;
    void *cb_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        if (handleExchangeRoute_(source_node, route))
            return;
        if (handleNotificationRoute_(source_node, route))
            return;
        cb = _binary_cb;
        cb_ctx = _binary_ctx;
    }
    if (cb)
        cb(cb_ctx, source_node, route);
}

void StackRouteAdapter::handleSlaveJsonRoute_(const StackJsonProtocol::RouteMessage &route)
{
    JsonRouteHandler cb = nullptr;
    void *cb_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        if (handleExchangeRoute_(route.source_node, route))
            return;
        if (handleNotificationRoute_(route.source_node, route))
            return;
        cb = _json_cb;
        cb_ctx = _json_ctx;
    }
    if (cb)
        cb(cb_ctx, route.source_node, route);
}

void StackRouteAdapter::handleSlaveBinaryRoute_(const StackBinaryProtocol::RouteFrame &route)
{
    BinaryRouteHandler cb = nullptr;
    void *cb_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        if (handleExchangeRoute_(route.source_node, route))
            return;
        if (handleNotificationRoute_(route.source_node, route))
            return;
        cb = _binary_cb;
        cb_ctx = _binary_ctx;
    }
    if (cb)
        cb(cb_ctx, route.source_node, route);
}

void StackRouteAdapter::handleMasterNotify_(uint32_t source_node, const StackJsonProtocol::NotifyMessage &notify)
{
    queueNotification_(source_node, notify.is_binary, notify.level, notify.feature, notify.code, notify.message.c_str(),
                       &notify.payload);
}

void StackRouteAdapter::handleSlaveNotify_(const StackJsonProtocol::NotifyMessage &notify)
{
    queueNotification_(notify.source_node, notify.is_binary, notify.level, notify.feature, notify.code, notify.message.c_str(),
                       &notify.payload);
}

bool StackRouteAdapter::handleExchangeRoute_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route)
{
    if (strcmp(route.feature, kXchgFeature) != 0)
        return false;

    if (route.meta.exchange_kind == StackTransport::ExchangeKind::Request && strcmp(route.action, "sync") == 0)
        return handleExchangeSyncRequest_(source_node, route);
    if (route.meta.exchange_kind == StackTransport::ExchangeKind::Response && strcmp(route.action, "sync") == 0)
        return handleExchangeSyncResponse_(source_node, route);
    return true;
}

bool StackRouteAdapter::handleExchangeRoute_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route)
{
    if (strcmp(route.feature, kXchgFeature) != 0)
        return false;

    if (route.meta.exchange_kind == StackTransport::ExchangeKind::Request && strcmp(route.action, "sync") == 0)
        return handleExchangeSyncRequest_(source_node, route);
    if (route.meta.exchange_kind == StackTransport::ExchangeKind::Response && strcmp(route.action, "sync") == 0)
        return handleExchangeSyncResponse_(source_node, route);
    return true;
}

bool StackRouteAdapter::handleExchangeSyncRequest_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route)
{
    uint32_t ack_out_upto = 0;
    JsonArrayConst deliver;
    if (!route.payload_json.isNull())
    {
        ack_out_upto = route.payload_json["ack_out_upto"] | 0u;
        deliver = route.payload_json["deliver"].as<JsonArrayConst>();
    }

    {
        const auto guard = _lock.guard();
        if (ack_out_upto)
            ackSlaveOutboxUpTo_(ack_out_upto);
    }

    uint32_t ack_in_upto = 0;
    for (JsonObjectConst item : deliver)
    {
        ExchangeRecord record;
        if (!readExchangeFromJson_(item, record))
            continue;
        {
            const auto guard = _lock.guard();
            record.target_node = _local_node_id;
        }
        DeferredJsonInvoke json_invoke;
        DeferredBinaryInvoke binary_invoke;
        emitExchangeToSlave_(record, json_invoke, binary_invoke);
        invokeDeferredJson_(json_invoke);
        invokeDeferredBinary_(binary_invoke);
        if (record.queue_id > ack_in_upto)
            ack_in_upto = record.queue_id;
    }

    DynamicJsonDocument resp(2048);
    buildExchangeSyncResponsePayload_(resp, ack_in_upto);
    return sendResponse(source_node, kXchgFeature, "sync", route.meta.request_id, &resp, Mode::Json);
}

bool StackRouteAdapter::handleExchangeSyncResponse_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route)
{
    {
        const auto guard = _lock.guard();
        if (_exchange_sync.phase != ExchangeSyncPhase::Wait || route.meta.reply_to != _exchange_sync.request_id ||
            source_node != _exchange_sync.node_id)
            return true;
    }

    JsonArrayConst outbound;
    if (!route.payload_json.isNull())
    {
        const uint32_t ack_in_upto = route.payload_json["ack_in_upto"] | 0u;
        {
            const auto guard = _lock.guard();
            if (ack_in_upto)
                ackMasterInboxUpTo_(source_node, ack_in_upto);
        }
        outbound = route.payload_json["outbound"].as<JsonArrayConst>();
        uint32_t max_out_id = 0;
        for (JsonObjectConst item : outbound)
        {
            ExchangeRecord record;
            if (!readExchangeFromJson_(item, record))
                continue;
            record.source_node = source_node;
            if (record.queue_id > max_out_id)
                max_out_id = record.queue_id;
            DeferredJsonInvoke json_invoke;
            DeferredBinaryInvoke binary_invoke;
            routeOutboundExchange_(record, json_invoke, binary_invoke);
            invokeDeferredJson_(json_invoke);
            invokeDeferredBinary_(binary_invoke);
        }
        {
            const auto guard = _lock.guard();
            if (ExchangeNodeState *state = exchangeNodeState_(source_node, true))
            {
                if (max_out_id > state->ack_out_upto)
                    state->ack_out_upto = max_out_id;
            }
        }
    }

    {
        const auto guard = _lock.guard();
        resetExchangeSync_();
    }
    return true;
}

bool StackRouteAdapter::handleExchangeSyncRequest_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route)
{
    uint32_t ack_out_upto = 0;
    uint32_t ack_in_upto = 0;
    const uint8_t *p = route.payload;
    size_t left = route.payload_size;
    uint8_t deliver_count = 0;
    if (!readU32_(p, left, ack_out_upto) || !readU8_(p, left, deliver_count))
        return true;

    {
        const auto guard = _lock.guard();
        if (ack_out_upto)
            ackSlaveOutboxUpTo_(ack_out_upto);
    }

    for (uint8_t i = 0; i < deliver_count; ++i)
    {
        ExchangeRecord record;
        record = ExchangeRecord{};
        record.used = true;
        uint8_t flags = 0;
        uint8_t feature_len = 0;
        uint8_t action_len = 0;
        uint16_t payload_len = 0;
        if (!readU32_(p, left, record.queue_id) || !readU32_(p, left, record.source_node) ||
            !readU32_(p, left, record.target_node) || !readU32_(p, left, record.meta.request_id) ||
            !readU32_(p, left, record.meta.reply_to) || !readU8_(p, left, flags) || !readU8_(p, left, feature_len) ||
            !readU8_(p, left, action_len) || !readU16_(p, left, payload_len))
            break;
        record.is_binary = (flags & 0x01u) != 0;
        record.meta.expect_response = (flags & 0x02u) != 0;
        const uint8_t kind_bits = (uint8_t)((flags >> 2) & 0x03u);
        record.meta.exchange_kind = kind_bits == 1 ? StackTransport::ExchangeKind::Request
                                   : kind_bits == 2 ? StackTransport::ExchangeKind::Response
                                                    : StackTransport::ExchangeKind::Event;
        const uint8_t *feature_data = nullptr;
        const uint8_t *action_data = nullptr;
        const uint8_t *payload_data = nullptr;
        if (!readBytes_(p, left, feature_data, feature_len) || !readBytes_(p, left, action_data, action_len) ||
            !readBytes_(p, left, payload_data, payload_len))
            break;
        const String feature_text = textFromBytes_(feature_data, feature_len);
        const String action_text = textFromBytes_(action_data, action_len);
        copyText_(record.feature, sizeof(record.feature), feature_text.c_str());
        copyText_(record.action, sizeof(record.action), action_text.c_str());
        if (record.is_binary)
            record.payload = encodeBase64Raw_(payload_data, payload_len);
        else
            record.payload = textFromBytes_(payload_data, payload_len);
        {
            const auto guard = _lock.guard();
            record.target_node = _local_node_id;
        }
        DeferredJsonInvoke json_invoke;
        DeferredBinaryInvoke binary_invoke;
        emitExchangeToSlave_(record, json_invoke, binary_invoke);
        invokeDeferredJson_(json_invoke);
        invokeDeferredBinary_(binary_invoke);
        if (record.queue_id > ack_in_upto)
            ack_in_upto = record.queue_id;
    }

    std::vector<uint8_t> resp;
    resp.reserve(512);
    appendU32_(resp, ack_in_upto);
    uint8_t outbound_count = 0;
    appendU8_(resp, 0);
    {
        const auto guard = _lock.guard();
        for (size_t i = 0; i < kExchangeSlaveOutboxCap && outbound_count < 4; ++i)
        {
            const ExchangeRecord &slot = _exchange_slave_outbox[i];
            if (!slot.used)
                continue;
            std::vector<uint8_t> payload_bytes;
            const uint8_t *payload_ptr = nullptr;
            size_t payload_len = 0;
            if (slot.is_binary)
            {
                if (!decodeBase64Raw_(slot.payload, payload_bytes))
                    continue;
                payload_ptr = payload_bytes.empty() ? nullptr : payload_bytes.data();
                payload_len = payload_bytes.size();
            }
            else
            {
                payload_ptr = slot.payload.length() ? reinterpret_cast<const uint8_t *>(slot.payload.c_str()) : nullptr;
                payload_len = slot.payload.length();
            }
            const uint8_t flags = (slot.is_binary ? 0x01u : 0u) | (slot.meta.expect_response ? 0x02u : 0u) |
                                  ((slot.meta.exchange_kind == StackTransport::ExchangeKind::Request
                                        ? 1u
                                        : slot.meta.exchange_kind == StackTransport::ExchangeKind::Response ? 2u : 0u)
                                   << 2);
            appendU32_(resp, slot.queue_id);
            appendU32_(resp, slot.source_node);
            appendU32_(resp, slot.target_node);
            appendU32_(resp, slot.meta.request_id);
            appendU32_(resp, slot.meta.reply_to);
            appendU8_(resp, flags);
            appendU8_(resp, (uint8_t)strlen(slot.feature));
            appendU8_(resp, (uint8_t)strlen(slot.action));
            appendU16_(resp, (uint16_t)payload_len);
            appendBytes_(resp, (const uint8_t *)slot.feature, strlen(slot.feature));
            appendBytes_(resp, (const uint8_t *)slot.action, strlen(slot.action));
            appendBytes_(resp, payload_ptr, payload_len);
            ++outbound_count;
        }
    }
    resp[4] = outbound_count;
    return sendResponseBinary(source_node, kXchgFeature, "sync", route.meta.request_id, resp.data(), resp.size());
}

bool StackRouteAdapter::handleExchangeSyncResponse_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route)
{
    {
        const auto guard = _lock.guard();
        if (_exchange_sync.phase != ExchangeSyncPhase::Wait || route.meta.reply_to != _exchange_sync.request_id ||
            source_node != _exchange_sync.node_id)
            return true;
    }

    const uint8_t *p = route.payload;
    size_t left = route.payload_size;
    uint32_t ack_in_upto = 0;
    uint8_t outbound_count = 0;
    if (!readU32_(p, left, ack_in_upto) || !readU8_(p, left, outbound_count))
        return true;
    {
        const auto guard = _lock.guard();
        if (ack_in_upto)
            ackMasterInboxUpTo_(source_node, ack_in_upto);
    }

    uint32_t max_out_id = 0;
    for (uint8_t i = 0; i < outbound_count; ++i)
    {
        ExchangeRecord record;
        record = ExchangeRecord{};
        record.used = true;
        uint8_t flags = 0;
        uint8_t feature_len = 0;
        uint8_t action_len = 0;
        uint16_t payload_len = 0;
        if (!readU32_(p, left, record.queue_id) || !readU32_(p, left, record.source_node) ||
            !readU32_(p, left, record.target_node) || !readU32_(p, left, record.meta.request_id) ||
            !readU32_(p, left, record.meta.reply_to) || !readU8_(p, left, flags) || !readU8_(p, left, feature_len) ||
            !readU8_(p, left, action_len) || !readU16_(p, left, payload_len))
            break;
        record.is_binary = (flags & 0x01u) != 0;
        record.meta.expect_response = (flags & 0x02u) != 0;
        const uint8_t kind_bits = (uint8_t)((flags >> 2) & 0x03u);
        record.meta.exchange_kind = kind_bits == 1 ? StackTransport::ExchangeKind::Request
                                   : kind_bits == 2 ? StackTransport::ExchangeKind::Response
                                                    : StackTransport::ExchangeKind::Event;
        const uint8_t *feature_data = nullptr;
        const uint8_t *action_data = nullptr;
        const uint8_t *payload_data = nullptr;
        if (!readBytes_(p, left, feature_data, feature_len) || !readBytes_(p, left, action_data, action_len) ||
            !readBytes_(p, left, payload_data, payload_len))
            break;
        const String feature_text = textFromBytes_(feature_data, feature_len);
        const String action_text = textFromBytes_(action_data, action_len);
        copyText_(record.feature, sizeof(record.feature), feature_text.c_str());
        copyText_(record.action, sizeof(record.action), action_text.c_str());
        record.source_node = source_node;
        if (record.is_binary)
            record.payload = encodeBase64Raw_(payload_data, payload_len);
        else
            record.payload = textFromBytes_(payload_data, payload_len);
        if (record.queue_id > max_out_id)
            max_out_id = record.queue_id;
        DeferredJsonInvoke json_invoke;
        DeferredBinaryInvoke binary_invoke;
        routeOutboundExchange_(record, json_invoke, binary_invoke);
        invokeDeferredJson_(json_invoke);
        invokeDeferredBinary_(binary_invoke);
    }

    {
        const auto guard = _lock.guard();
        if (ExchangeNodeState *state = exchangeNodeState_(source_node, true))
        {
            if (max_out_id > state->ack_out_upto)
                state->ack_out_upto = max_out_id;
        }
        resetExchangeSync_();
    }
    return true;
}

bool StackRouteAdapter::queueExchangeFromSlave_(uint32_t target_node, const char *feature, const char *action, const String &payload,
                                                const StackTransport::RouteMeta *meta, bool is_binary)
{
    const auto guard = _lock.guard();
    if (!_slave || !feature || !feature[0] || !action || !action[0])
        return false;

    StackTransport::RouteMeta local_meta = meta ? *meta : StackTransport::RouteMeta{};
    if (local_meta.exchange_kind == StackTransport::ExchangeKind::Request && local_meta.request_id == 0)
        local_meta.request_id = nextInternalRequestId_();

    for (size_t i = 0; i < kExchangeSlaveOutboxCap; ++i)
    {
        ExchangeRecord &slot = _exchange_slave_outbox[i];
        if (slot.used)
            continue;
        return appendExchangeRecord_(slot, _local_node_id, target_node, feature, action, payload, local_meta, _next_exchange_id++,
                                     is_binary);
    }

    _log.warn(F("STACK"), F("Exchange outbox full: target 0x%08lX feature %s action %s"),
              (unsigned long)target_node, feature, action);
    ExchangeRecord dropped;
    dropped.used = true;
    dropped.is_binary = is_binary;
    dropped.queue_id = _next_exchange_id;
    dropped.source_node = _local_node_id;
    dropped.target_node = target_node;
    dropped.enqueued_ms = millis();
    dropped.meta = local_meta;
    copyText_(dropped.feature, sizeof(dropped.feature), feature);
    copyText_(dropped.action, sizeof(dropped.action), action);
    dropped.payload = payload;
    appendExchangeHistory_(dropped, ExchangeStatus::Dropped);
    updateExchangeDiag_(ExchangeStatus::Dropped, is_binary);
    return false;
}

bool StackRouteAdapter::enqueueMasterInbox_(uint32_t source_node, uint32_t target_node, const char *feature, const char *action,
                                            const String &payload, const StackTransport::RouteMeta &meta, bool is_binary)
{
    const auto guard = _lock.guard();
    for (size_t i = 0; i < kExchangeMasterInboxCap; ++i)
    {
        ExchangeRecord &slot = _exchange_master_inbox[i];
        if (slot.used)
            continue;
        return appendExchangeRecord_(slot, source_node, target_node, feature, action, payload, meta, _next_exchange_id++, is_binary);
    }
    _log.warn(F("STACK"), F("Master inbox full: src 0x%08lX dst 0x%08lX feature %s action %s"),
              (unsigned long)source_node,
              (unsigned long)target_node, feature ? feature : "-", action ? action : "-");
    ExchangeRecord dropped;
    dropped.used = true;
    dropped.is_binary = is_binary;
    dropped.queue_id = _next_exchange_id;
    dropped.source_node = source_node;
    dropped.target_node = target_node;
    dropped.enqueued_ms = millis();
    dropped.meta = meta;
    copyText_(dropped.feature, sizeof(dropped.feature), feature);
    copyText_(dropped.action, sizeof(dropped.action), action);
    dropped.payload = payload;
    appendExchangeHistory_(dropped, ExchangeStatus::Dropped);
    updateExchangeDiag_(ExchangeStatus::Dropped, is_binary);
    return false;
}

bool StackRouteAdapter::appendExchangeRecord_(ExchangeRecord &entry, uint32_t source_node, uint32_t target_node, const char *feature,
                                              const char *action, const String &payload, const StackTransport::RouteMeta &meta,
                                              uint32_t queue_id, bool is_binary)
{
    entry = ExchangeRecord{};
    entry.used = true;
    entry.is_binary = is_binary;
    entry.queue_id = queue_id ? queue_id : 1;
    entry.source_node = source_node;
    entry.target_node = target_node;
    entry.enqueued_ms = millis();
    entry.expire_ms = entry.enqueued_ms + kExchangeExpireMs;
    entry.meta = meta;
    copyText_(entry.feature, sizeof(entry.feature), feature);
    copyText_(entry.action, sizeof(entry.action), action);
    entry.payload = payload;
    if (_next_exchange_id == 0)
        _next_exchange_id = 1;
    appendExchangeHistory_(entry, ExchangeStatus::Queued);
    updateExchangeDiag_(ExchangeStatus::Queued, is_binary);
    return true;
}

void StackRouteAdapter::ackSlaveOutboxUpTo_(uint32_t upto_id)
{
    for (size_t i = 0; i < kExchangeSlaveOutboxCap; ++i)
    {
        if (_exchange_slave_outbox[i].used && _exchange_slave_outbox[i].queue_id <= upto_id)
        {
            appendExchangeHistory_(_exchange_slave_outbox[i], ExchangeStatus::UplinkAcked);
            updateExchangeDiag_(ExchangeStatus::UplinkAcked, _exchange_slave_outbox[i].is_binary);
            _exchange_slave_outbox[i] = ExchangeRecord{};
        }
    }
}

void StackRouteAdapter::ackMasterInboxUpTo_(uint32_t node_id, uint32_t upto_id)
{
    for (size_t i = 0; i < kExchangeMasterInboxCap; ++i)
    {
        ExchangeRecord &slot = _exchange_master_inbox[i];
        if (slot.used && slot.target_node == node_id && slot.queue_id <= upto_id)
        {
            appendExchangeHistory_(slot, ExchangeStatus::Delivered);
            updateExchangeDiag_(ExchangeStatus::Delivered, slot.is_binary);
            slot = ExchangeRecord{};
        }
    }
}

bool StackRouteAdapter::buildExchangeSyncRequestPayload_(uint32_t node_id, DynamicJsonDocument &doc)
{
    const auto guard = _lock.guard();
    if (ExchangeNodeState *state = exchangeNodeState_(node_id, true))
        doc["ack_out_upto"] = state->ack_out_upto;
    JsonArray deliver = doc.createNestedArray("deliver");
    size_t count = 0;
    const uint32_t now = millis();
    for (size_t i = 0; i < kExchangeMasterInboxCap && count < 4; ++i)
    {
        ExchangeRecord &slot = _exchange_master_inbox[i];
        if (!slot.used || slot.target_node != node_id)
            continue;
        if (slot.retry_count >= kExchangeMaxRetries)
            continue;
        if (slot.delivery_count > 0 && !expiredMs_(now, slot.last_sync_ms + kExchangeRetryIntervalMs))
            continue;
        if (slot.delivery_count > 0)
        {
            ++slot.retry_count;
            appendExchangeHistory_(slot, ExchangeStatus::Retried);
            updateExchangeDiag_(ExchangeStatus::Retried, slot.is_binary);
        }
        ++slot.delivery_count;
        slot.last_sync_ms = now;
        writeExchangeToJson_(deliver.createNestedObject(), slot);
        ++count;
    }
    return true;
}

bool StackRouteAdapter::buildExchangeSyncResponsePayload_(DynamicJsonDocument &doc, uint32_t &ack_in_upto)
{
    const auto guard = _lock.guard();
    doc["ack_in_upto"] = ack_in_upto;
    JsonArray outbound = doc.createNestedArray("outbound");
    size_t count = 0;
    for (size_t i = 0; i < kExchangeSlaveOutboxCap && count < 4; ++i)
    {
        const ExchangeRecord &slot = _exchange_slave_outbox[i];
        if (!slot.used)
            continue;
        writeExchangeToJson_(outbound.createNestedObject(), slot);
        ++count;
    }
    return true;
}

bool StackRouteAdapter::tryStartExchangePoll_()
{
    const StackDeviceRegistry *registry = nullptr;
    uint32_t local_node_id = 0;
    {
        const auto guard = _lock.guard();
        if (_exchange_sync.phase != ExchangeSyncPhase::Idle)
            return false;
        if (!hasRs485PollBackend_())
            return false;
        if (_rs485_master)
            registry = &_rs485_master->router().registry();
        local_node_id = _local_node_id;
    }
    if (!registry)
        return false;

    for (size_t attempt = 0; attempt < StackDeviceRegistry::kMaxDevices; ++attempt)
    {
        size_t idx = 0;
        {
            const auto guard = _lock.guard();
            idx = (_exchange_poll_cursor + attempt) % StackDeviceRegistry::kMaxDevices;
        }
        StackDeviceRegistry::DeviceInfo device;
        if (!registry->snapshotAt(idx, device) || !device.online || device.node_id == 0 || device.node_id == local_node_id)
            continue;

        uint32_t request_id = 0;
        PayloadMode payload_mode = PayloadMode::Json;
        {
            const auto guard = _lock.guard();
            if (_exchange_sync.phase != ExchangeSyncPhase::Idle)
                return false;
            _exchange_sync.phase = ExchangeSyncPhase::Wait;
            _exchange_sync.node_id = device.node_id;
            _exchange_sync.request_id = nextInternalRequestId_();
            _exchange_sync.deadline_ms = millis() + kExchangeSyncTimeoutMs;
            _exchange_poll_cursor = (uint8_t)((idx + 1u) % StackDeviceRegistry::kMaxDevices);
            request_id = _exchange_sync.request_id;
            payload_mode = _payload_mode;
        }
        bool sent = false;
        if (payload_mode == PayloadMode::Binary)
        {
            std::vector<uint8_t> req;
            req.reserve(512);
            uint32_t ack_out_upto = 0;
            {
                const auto guard = _lock.guard();
                if (ExchangeNodeState *state = exchangeNodeState_(device.node_id, true))
                    ack_out_upto = state->ack_out_upto;
            }
            appendU32_(req, ack_out_upto);
            uint8_t deliver_count = 0;
            appendU8_(req, 0);
            {
                const auto guard = _lock.guard();
                const uint32_t now = millis();
                for (size_t i = 0; i < kExchangeMasterInboxCap && deliver_count < 4; ++i)
                {
                    ExchangeRecord &slot = _exchange_master_inbox[i];
                    if (!slot.used || slot.target_node != device.node_id)
                        continue;
                    if (slot.retry_count >= kExchangeMaxRetries)
                        continue;
                    if (slot.delivery_count > 0 && !expiredMs_(now, slot.last_sync_ms + kExchangeRetryIntervalMs))
                        continue;
                    if (slot.delivery_count > 0)
                    {
                        ++slot.retry_count;
                        appendExchangeHistory_(slot, ExchangeStatus::Retried);
                        updateExchangeDiag_(ExchangeStatus::Retried, slot.is_binary);
                    }
                    ++slot.delivery_count;
                    slot.last_sync_ms = now;
                    std::vector<uint8_t> payload_bytes;
                    const uint8_t *payload_ptr = nullptr;
                    size_t payload_len = 0;
                    if (slot.is_binary)
                    {
                        if (!decodeBase64Raw_(slot.payload, payload_bytes))
                            continue;
                        payload_ptr = payload_bytes.empty() ? nullptr : payload_bytes.data();
                        payload_len = payload_bytes.size();
                    }
                    else
                    {
                        payload_ptr = slot.payload.length() ? (const uint8_t *)slot.payload.c_str() : nullptr;
                        payload_len = slot.payload.length();
                    }
                    const uint8_t flags = (slot.is_binary ? 0x01u : 0u) | (slot.meta.expect_response ? 0x02u : 0u) |
                                          ((slot.meta.exchange_kind == StackTransport::ExchangeKind::Request
                                                ? 1u
                                                : slot.meta.exchange_kind == StackTransport::ExchangeKind::Response ? 2u : 0u)
                                           << 2);
                    appendU32_(req, slot.queue_id);
                    appendU32_(req, slot.source_node);
                    appendU32_(req, slot.target_node);
                    appendU32_(req, slot.meta.request_id);
                    appendU32_(req, slot.meta.reply_to);
                    appendU8_(req, flags);
                    appendU8_(req, (uint8_t)strlen(slot.feature));
                    appendU8_(req, (uint8_t)strlen(slot.action));
                    appendU16_(req, (uint16_t)payload_len);
                    appendBytes_(req, (const uint8_t *)slot.feature, strlen(slot.feature));
                    appendBytes_(req, (const uint8_t *)slot.action, strlen(slot.action));
                    appendBytes_(req, payload_ptr, payload_len);
                    ++deliver_count;
                }
            }
            req[4] = deliver_count;
            sent = sendRequestBinary(device.node_id, kXchgFeature, "sync", req.data(), req.size(), true, request_id);
        }
        else
        {
            DynamicJsonDocument req(2048);
            buildExchangeSyncRequestPayload_(device.node_id, req);
            sent = sendRequest(device.node_id, kXchgFeature, "sync", &req, Mode::Json, true, request_id);
        }
        if (!sent)
        {
            const auto guard = _lock.guard();
            if (_exchange_sync.phase == ExchangeSyncPhase::Wait && _exchange_sync.request_id == request_id &&
                _exchange_sync.node_id == device.node_id)
                resetExchangeSync_();
            continue;
        }
        return true;
    }
    return false;
}

void StackRouteAdapter::resetExchangeSync_()
{
    _exchange_sync = ExchangeSyncState{};
}

void StackRouteAdapter::expireMasterInbox_(uint32_t now_ms)
{
    for (size_t i = 0; i < kExchangeMasterInboxCap; ++i)
    {
        ExchangeRecord &slot = _exchange_master_inbox[i];
        if (!slot.used)
            continue;
        const bool ttl_expired = slot.expire_ms && expiredMs_(now_ms, slot.expire_ms);
        const bool retries_expired =
            slot.delivery_count > 0 && slot.retry_count >= kExchangeMaxRetries &&
            expiredMs_(now_ms, slot.last_sync_ms + kExchangeRetryIntervalMs);
        if (!ttl_expired && !retries_expired)
            continue;
        _log.warn(F("STACK"), F("Exchange expired: qid %lu src 0x%08lX dst 0x%08lX feature %s action %s retries %u"),
                  (unsigned long)slot.queue_id, (unsigned long)slot.source_node, (unsigned long)slot.target_node,
                  slot.feature, slot.action, (unsigned)slot.retry_count);
        appendExchangeHistory_(slot, ExchangeStatus::Expired);
        updateExchangeDiag_(ExchangeStatus::Expired, slot.is_binary);
        slot = ExchangeRecord{};
    }
}

StackRouteAdapter::ExchangeNodeState *StackRouteAdapter::exchangeNodeState_(uint32_t node_id, bool create)
{
    if (node_id == 0)
        return nullptr;
    for (size_t i = 0; i < StackDeviceRegistry::kMaxDevices; ++i)
    {
        if (_exchange_nodes[i].used && _exchange_nodes[i].node_id == node_id)
            return &_exchange_nodes[i];
    }
    if (!create)
        return nullptr;
    for (size_t i = 0; i < StackDeviceRegistry::kMaxDevices; ++i)
    {
        if (_exchange_nodes[i].used)
            continue;
        _exchange_nodes[i].used = true;
        _exchange_nodes[i].node_id = node_id;
        return &_exchange_nodes[i];
    }
    return nullptr;
}

void StackRouteAdapter::routeOutboundExchange_(const ExchangeRecord &record, DeferredJsonInvoke &json_invoke,
                                               DeferredBinaryInvoke &binary_invoke)
{
    json_invoke = DeferredJsonInvoke{};
    binary_invoke = DeferredBinaryInvoke{};

    bool local_delivery = false;
    bool has_master_backend = false;
    uint32_t local_node_id = 0;
    JsonRouteHandler json_cb = nullptr;
    void *json_ctx = nullptr;
    BinaryRouteHandler binary_cb = nullptr;
    void *binary_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        local_delivery = (record.target_node == 0 || record.target_node == _local_node_id || !hasMasterBackend_());
        has_master_backend = hasMasterBackend_();
        local_node_id = _local_node_id;
        json_cb = _json_cb;
        json_ctx = _json_ctx;
        binary_cb = _binary_cb;
        binary_ctx = _binary_ctx;
    }

    if (local_delivery)
    {
        if (record.is_binary)
        {
            if (!binary_cb)
            {
                const auto guard = _lock.guard();
                appendExchangeHistory_(record, ExchangeStatus::Dropped);
                updateExchangeDiag_(ExchangeStatus::Dropped, true);
            }
            else
            {
                String decoded;
                if (decodeBase64_(record.payload, decoded))
                {
                    binary_invoke.cb = binary_cb;
                    binary_invoke.ctx = binary_ctx;
                    binary_invoke.source_node = record.source_node;
                    binary_invoke.payload_storage = decoded;
                    binary_invoke.route.source_node = record.source_node;
                    binary_invoke.route.target_node = record.target_node;
                    binary_invoke.route.meta = record.meta;
                    copyText_(binary_invoke.route.feature, sizeof(binary_invoke.route.feature), record.feature);
                    copyText_(binary_invoke.route.action, sizeof(binary_invoke.route.action), record.action);
                    binary_invoke.route.payload =
                        reinterpret_cast<const uint8_t *>(binary_invoke.payload_storage.c_str());
                    binary_invoke.route.payload_size = binary_invoke.payload_storage.length();
                    const auto guard = _lock.guard();
                    appendExchangeHistory_(record, ExchangeStatus::LocalHandled);
                    updateExchangeDiag_(ExchangeStatus::LocalHandled, true);
                }
                else
                {
                    _log.warn(F("STACK"), F("Exchange binary decode failed: qid %lu feature %s action %s"),
                              (unsigned long)record.queue_id, record.feature, record.action);
                    const auto guard = _lock.guard();
                    appendExchangeHistory_(record, ExchangeStatus::Dropped);
                    updateExchangeDiag_(ExchangeStatus::Dropped, true);
                }
            }
        }
        else if (json_cb)
        {
            json_invoke.cb = json_cb;
            json_invoke.ctx = json_ctx;
            json_invoke.source_node = record.source_node;
            json_invoke.route.source_node = record.source_node;
            json_invoke.route.target_node = record.target_node;
            json_invoke.route.meta = record.meta;
            copyText_(json_invoke.route.feature, sizeof(json_invoke.route.feature), record.feature);
            copyText_(json_invoke.route.action, sizeof(json_invoke.route.action), record.action);
            StackJsonProtocol::parseRoutePayload(record.payload, json_invoke.route);
            const auto guard = _lock.guard();
            appendExchangeHistory_(record, ExchangeStatus::LocalHandled);
            updateExchangeDiag_(ExchangeStatus::LocalHandled, false);
        }
        else
        {
            const auto guard = _lock.guard();
            appendExchangeHistory_(record, ExchangeStatus::Dropped);
            updateExchangeDiag_(ExchangeStatus::Dropped, false);
        }
        return;
    }

    (void)has_master_backend;
    enqueueMasterInbox_(record.source_node, record.target_node, record.feature, record.action, record.payload, record.meta,
                        record.is_binary);
}

void StackRouteAdapter::emitExchangeToSlave_(const ExchangeRecord &record, DeferredJsonInvoke &json_invoke,
                                             DeferredBinaryInvoke &binary_invoke)
{
    json_invoke = DeferredJsonInvoke{};
    binary_invoke = DeferredBinaryInvoke{};
    JsonRouteHandler json_cb = nullptr;
    void *json_ctx = nullptr;
    BinaryRouteHandler binary_cb = nullptr;
    void *binary_ctx = nullptr;
    uint32_t local_node_id = 0;
    {
        const auto guard = _lock.guard();
        json_cb = _json_cb;
        json_ctx = _json_ctx;
        binary_cb = _binary_cb;
        binary_ctx = _binary_ctx;
        local_node_id = _local_node_id;
    }

    if (record.is_binary)
    {
        if (!binary_cb)
        {
            const auto guard = _lock.guard();
            appendExchangeHistory_(record, ExchangeStatus::Dropped);
            updateExchangeDiag_(ExchangeStatus::Dropped, true);
            return;
        }
        String decoded;
        if (!decodeBase64_(record.payload, decoded))
        {
            _log.warn(F("STACK"), F("Exchange binary decode failed: qid %lu feature %s action %s"),
                      (unsigned long)record.queue_id, record.feature, record.action);
            const auto guard = _lock.guard();
            appendExchangeHistory_(record, ExchangeStatus::Dropped);
            updateExchangeDiag_(ExchangeStatus::Dropped, true);
            return;
        }
        binary_invoke.cb = binary_cb;
        binary_invoke.ctx = binary_ctx;
        binary_invoke.source_node = record.source_node;
        binary_invoke.payload_storage = decoded;
        binary_invoke.route.source_node = record.source_node;
        binary_invoke.route.target_node = local_node_id;
        binary_invoke.route.meta = record.meta;
        copyText_(binary_invoke.route.feature, sizeof(binary_invoke.route.feature), record.feature);
        copyText_(binary_invoke.route.action, sizeof(binary_invoke.route.action), record.action);
        binary_invoke.route.payload = reinterpret_cast<const uint8_t *>(binary_invoke.payload_storage.c_str());
        binary_invoke.route.payload_size = binary_invoke.payload_storage.length();
        return;
    }

    if (!json_cb)
    {
        const auto guard = _lock.guard();
        appendExchangeHistory_(record, ExchangeStatus::Dropped);
        updateExchangeDiag_(ExchangeStatus::Dropped, false);
        return;
    }
    json_invoke.cb = json_cb;
    json_invoke.ctx = json_ctx;
    json_invoke.source_node = record.source_node;
    json_invoke.route.source_node = record.source_node;
    json_invoke.route.target_node = local_node_id;
    json_invoke.route.meta = record.meta;
    copyText_(json_invoke.route.feature, sizeof(json_invoke.route.feature), record.feature);
    copyText_(json_invoke.route.action, sizeof(json_invoke.route.action), record.action);
    StackJsonProtocol::parseRoutePayload(record.payload, json_invoke.route);
}

void StackRouteAdapter::appendExchangeHistory_(const ExchangeRecord &record, ExchangeStatus status)
{
    ExchangeHistoryRecord hist;
    hist.used = true;
    hist.is_binary = record.is_binary;
    hist.queue_id = record.queue_id;
    hist.source_node = record.source_node;
    hist.target_node = record.target_node;
    hist.enqueued_ms = record.enqueued_ms;
    hist.event_ms = millis();
    hist.retry_count = record.retry_count;
    hist.status = status;
    copyText_(hist.feature, sizeof(hist.feature), record.feature);
    copyText_(hist.action, sizeof(hist.action), record.action);
    const uint8_t slot = (uint8_t)((_exchange_history_head + _exchange_history_count) % kExchangeHistoryCap);
    _exchange_history[slot] = hist;
    if (_exchange_history_count < kExchangeHistoryCap)
        ++_exchange_history_count;
    else
        _exchange_history_head = (uint8_t)((_exchange_history_head + 1u) % kExchangeHistoryCap);
}

void StackRouteAdapter::updateExchangeDiag_(ExchangeStatus status, bool is_binary)
{
    switch (status)
    {
    case ExchangeStatus::Queued:
        ++_exchange_diag.queued;
        if (is_binary)
            ++_exchange_diag.binary_queued;
        break;
    case ExchangeStatus::Retried:
        ++_exchange_diag.retried;
        break;
    case ExchangeStatus::UplinkAcked:
        ++_exchange_diag.uplink_acked;
        break;
    case ExchangeStatus::Delivered:
        ++_exchange_diag.delivered;
        break;
    case ExchangeStatus::LocalHandled:
        ++_exchange_diag.local_handled;
        break;
    case ExchangeStatus::Expired:
        ++_exchange_diag.expired;
        break;
    case ExchangeStatus::Dropped:
        ++_exchange_diag.dropped;
        break;
    }
}

const char *StackRouteAdapter::exchangeStatusText_(ExchangeStatus status)
{
    switch (status)
    {
    case ExchangeStatus::Queued:
        return "queued";
    case ExchangeStatus::Retried:
        return "retried";
    case ExchangeStatus::UplinkAcked:
        return "uplink_acked";
    case ExchangeStatus::Delivered:
        return "delivered";
    case ExchangeStatus::LocalHandled:
        return "local";
    case ExchangeStatus::Expired:
        return "expired";
    case ExchangeStatus::Dropped:
        return "dropped";
    }
    return "unknown";
}

void StackRouteAdapter::writeExchangeToJson_(JsonObject obj, const ExchangeRecord &record)
{
    obj["queue_id"] = record.queue_id;
    obj["source_node"] = record.source_node;
    obj["target_node"] = record.target_node;
    obj["binary"] = record.is_binary;
    obj["feature"] = record.feature;
    obj["action"] = record.action;
    JsonObject meta = obj.createNestedObject("meta");
    meta["exchange"] = (record.meta.exchange_kind == StackTransport::ExchangeKind::Request)
                           ? "request"
                           : (record.meta.exchange_kind == StackTransport::ExchangeKind::Response) ? "response" : "event";
    meta["request_id"] = record.meta.request_id;
    meta["reply_to"] = record.meta.reply_to;
    meta["expect_response"] = record.meta.expect_response;
    if (record.payload.length())
    {
        if (record.is_binary)
            obj["payload_b64"] = record.payload;
        else
        {
            DynamicJsonDocument payload_doc(768);
            if (!deserializeJson(payload_doc, record.payload.c_str(), record.payload.length()))
                obj["payload"].set(payload_doc.as<JsonVariantConst>());
            else
                obj["payload_raw"] = record.payload;
        }
    }
}

bool StackRouteAdapter::readExchangeFromJson_(JsonObjectConst obj, ExchangeRecord &record)
{
    record = ExchangeRecord{};
    record.used = true;
    record.queue_id = obj["queue_id"] | 0u;
    record.source_node = obj["source_node"] | 0u;
    record.target_node = obj["target_node"] | 0u;
    record.is_binary = obj["binary"] | false;
    copyText_(record.feature, sizeof(record.feature), obj["feature"] | "");
    copyText_(record.action, sizeof(record.action), obj["action"] | "");
    const char *exchange = obj["meta"]["exchange"] | "event";
    if (strcmp(exchange, "request") == 0)
        record.meta.exchange_kind = StackTransport::ExchangeKind::Request;
    else if (strcmp(exchange, "response") == 0)
        record.meta.exchange_kind = StackTransport::ExchangeKind::Response;
    else
        record.meta.exchange_kind = StackTransport::ExchangeKind::Event;
    record.meta.request_id = obj["meta"]["request_id"] | 0u;
    record.meta.reply_to = obj["meta"]["reply_to"] | 0u;
    record.meta.expect_response = obj["meta"]["expect_response"] | false;
    if (record.is_binary)
        record.payload = obj["payload_b64"] | "";
    else if (obj["payload"].is<JsonVariantConst>())
        serializeJson(obj["payload"], record.payload);
    else
        record.payload = obj["payload_raw"] | "";
    return record.queue_id != 0 && record.feature[0] != '\0' && record.action[0] != '\0';
}

String StackRouteAdapter::encodeBase64_(const uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return String();
    String out;
    out.reserve(((len + 2u) / 3u) * 4u);
    for (size_t i = 0; i < len; i += 3u)
    {
        const uint32_t a = data[i];
        const uint32_t b = (i + 1u < len) ? data[i + 1u] : 0u;
        const uint32_t c = (i + 2u < len) ? data[i + 2u] : 0u;
        const uint32_t chunk = (a << 16) | (b << 8) | c;
        out += b64Char_((uint8_t)((chunk >> 18) & 0x3Fu));
        out += b64Char_((uint8_t)((chunk >> 12) & 0x3Fu));
        out += (i + 1u < len) ? b64Char_((uint8_t)((chunk >> 6) & 0x3Fu)) : '=';
        out += (i + 2u < len) ? b64Char_((uint8_t)(chunk & 0x3Fu)) : '=';
    }
    return out;
}

bool StackRouteAdapter::decodeBase64_(const String &in, String &out)
{
    out = String();
    if (!in.length())
        return true;

    const size_t out_cap = (in.length() / 4u) * 3u;
    char *buf = (char *)malloc(out_cap ? out_cap : 1u);
    if (!buf)
        return false;

    size_t out_len = 0;
    uint8_t block[4] = {};
    uint8_t block_len = 0;
    uint8_t pad = 0;
    for (size_t i = 0; i < in.length(); ++i)
    {
        const char ch = in[i];
        if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t')
            continue;
        if (ch == '=')
        {
            block[block_len++] = 0;
            ++pad;
        }
        else
        {
            const int8_t v = b64Index_(ch);
            if (v < 0)
            {
                free(buf);
                return false;
            }
            block[block_len++] = (uint8_t)v;
        }

        if (block_len != 4)
            continue;

        const uint32_t chunk =
            ((uint32_t)block[0] << 18) | ((uint32_t)block[1] << 12) | ((uint32_t)block[2] << 6) | (uint32_t)block[3];
        buf[out_len++] = (char)((chunk >> 16) & 0xFFu);
        if (pad < 2)
            buf[out_len++] = (char)((chunk >> 8) & 0xFFu);
        if (pad == 0)
            buf[out_len++] = (char)(chunk & 0xFFu);
        block_len = 0;
        pad = 0;
    }

    out.reserve(out_len);
    for (size_t i = 0; i < out_len; ++i)
        out += buf[i];
    free(buf);
    return true;
}

bool StackRouteAdapter::handleNotificationRoute_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route)
{
    if (strcmp(route.feature, kNotifyFeature) != 0)
        return false;

    if (route.meta.exchange_kind == StackTransport::ExchangeKind::Request)
    {
        if (strcmp(route.action, "pull") == 0)
            return handleNotificationPullRequest_(source_node, route);
        if (strcmp(route.action, "ack") == 0)
            return handleNotificationAckRequest_(source_node, route);
        return true;
    }

    if (route.meta.exchange_kind == StackTransport::ExchangeKind::Response)
    {
        if (strcmp(route.action, "pull") == 0)
            return handleNotificationPullResponse_(source_node, route);
        if (strcmp(route.action, "ack") == 0)
            return handleNotificationAckResponse_(source_node, route);
        return true;
    }

    return true;
}

bool StackRouteAdapter::handleNotificationRoute_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route)
{
    if (strcmp(route.feature, kNotifyFeature) != 0)
        return false;

    if (route.meta.exchange_kind == StackTransport::ExchangeKind::Request)
    {
        if (strcmp(route.action, "pull") == 0)
            return handleNotificationPullRequest_(source_node, route);
        if (strcmp(route.action, "ack") == 0)
            return handleNotificationAckRequest_(source_node, route);
        return true;
    }

    if (route.meta.exchange_kind == StackTransport::ExchangeKind::Response)
    {
        if (strcmp(route.action, "pull") == 0)
            return handleNotificationPullResponse_(source_node, route);
        if (strcmp(route.action, "ack") == 0)
            return handleNotificationAckResponse_(source_node, route);
        return true;
    }

    return true;
}

bool StackRouteAdapter::handleNotificationPullRequest_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route)
{
    size_t limit = 4;
    if (!route.payload_json.isNull())
    {
        const size_t parsed_limit = route.payload_json["limit"] | 4u;
        if (parsed_limit > 0)
            limit = parsed_limit;
    }

    DynamicJsonDocument resp(1024);
    uint32_t max_id = 0;
    buildPullResponsePayload_(resp, limit, max_id);
    resp["max_id"] = max_id;
    return sendResponse(source_node, kNotifyFeature, "pull", route.meta.request_id, &resp, Mode::Json);
}

bool StackRouteAdapter::handleNotificationAckRequest_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route)
{
    uint32_t upto_id = 0;
    if (!route.payload_json.isNull())
        upto_id = route.payload_json["upto_id"] | 0u;
    {
        const auto guard = _lock.guard();
        if (upto_id)
            ackOutboxUpTo_(upto_id);
    }

    DynamicJsonDocument resp(128);
    resp["ok"] = true;
    resp["upto_id"] = upto_id;
    return sendResponse(source_node, kNotifyFeature, "ack", route.meta.request_id, &resp, Mode::Json);
}

bool StackRouteAdapter::handleNotificationPullResponse_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route)
{
    {
        const auto guard = _lock.guard();
        if (_notify_sync.phase != NotifySyncPhase::PullWait || !_notify_sync.request_id ||
            route.meta.reply_to != _notify_sync.request_id || source_node != _notify_sync.node_id)
            return true;
    }

    uint32_t max_id = 0;
    if (!route.payload_json.isNull())
    {
        JsonArrayConst items = route.payload_json["items"].as<JsonArrayConst>();
        for (JsonObjectConst item : items)
        {
            NotificationRecord record;
            record.used = true;
            record.is_binary = item["binary"] | false;
            record.notification_id = item["notification_id"] | 0u;
            record.source_node = source_node;
            record.ts_ms = item["ts_ms"] | 0u;
            record.repeat_count = item["repeat_count"] | 1u;
            record.priority = item["priority"] | 0u;
            copyText_(record.level, sizeof(record.level), item["level"] | "info");
            copyText_(record.feature, sizeof(record.feature), item["feature"] | "");
            copyText_(record.code, sizeof(record.code), item["code"] | "");
            record.message = item["message"] | "";
            if (!record.is_binary && item["payload"].is<JsonVariantConst>())
                serializeJson(item["payload"], record.payload);
            if (record.notification_id > max_id)
                max_id = record.notification_id;
            DeferredNotifyInvoke invoke;
            emitNotification_(record, &invoke);
            invokeDeferredNotify_(invoke);
        }
        const uint32_t payload_max = route.payload_json["max_id"] | 0u;
        if (payload_max > max_id)
            max_id = payload_max;
    }

    {
        const auto guard = _lock.guard();
        _notify_sync.phase = NotifySyncPhase::AckWait;
        _notify_sync.ack_upto_id = max_id;
        _notify_sync.request_id = nextInternalRequestId_();
        _notify_sync.deadline_ms = millis() + 1500u;
    }

    DynamicJsonDocument ack(128);
    ack["upto_id"] = max_id;
    uint32_t request_id = 0;
    {
        const auto guard = _lock.guard();
        request_id = _notify_sync.request_id;
    }
    if (!sendRequest(source_node, kNotifyFeature, "ack", &ack, Mode::Json, true, request_id))
    {
        _log.warn(F("STACK"), F("Notify ack send failed: node 0x%08lX upto %lu"), (unsigned long)source_node,
                  (unsigned long)max_id);
        const auto guard = _lock.guard();
        resetNotifySync_();
    }
    return true;
}

bool StackRouteAdapter::handleNotificationAckResponse_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route)
{
    const auto guard = _lock.guard();
    if (_notify_sync.phase != NotifySyncPhase::AckWait || !_notify_sync.request_id ||
        route.meta.reply_to != _notify_sync.request_id || source_node != _notify_sync.node_id)
        return true;
    resetNotifySync_();
    return true;
}

bool StackRouteAdapter::handleNotificationPullRequest_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route)
{
    const uint8_t *p = route.payload;
    size_t left = route.payload_size;
    uint16_t limit = 4;
    if (left >= 2)
        readU16_(p, left, limit);
    if (limit == 0)
        limit = 1;

    std::vector<uint8_t> resp;
    resp.reserve(512);
    uint32_t max_id = 0;
    appendU32_(resp, 0);
    uint8_t item_count = 0;
    appendU8_(resp, 0);
    {
        const auto guard = _lock.guard();
        size_t emitted = 0;
        for (uint8_t pass = 0; pass < 2 && emitted < limit; ++pass)
        {
            for (size_t i = 0; i < kNotifyOutboxCap && emitted < limit; ++i)
            {
                const NotificationRecord &slot = _notify_outbox[i];
                if (!slot.used)
                    continue;
                if (pass == 0 && slot.priority < 2)
                    continue;
                if (pass == 1 && slot.priority >= 2)
                    continue;
                appendU32_(resp, slot.notification_id);
                appendU32_(resp, slot.source_node);
                appendU32_(resp, slot.ts_ms);
                appendU16_(resp, slot.repeat_count);
                appendU8_(resp, slot.priority);
                appendU8_(resp, (uint8_t)strlen(slot.level));
                appendU8_(resp, (uint8_t)strlen(slot.feature));
                appendU8_(resp, (uint8_t)strlen(slot.code));
                const String wire_message = clampUtf8Bytes_(slot.message.c_str(), StackBinaryProtocol::kNotifyMessageMax);
                appendU16_(resp, (uint16_t)wire_message.length());
                appendU16_(resp, 0u);
                appendBytes_(resp, (const uint8_t *)slot.level, strlen(slot.level));
                appendBytes_(resp, (const uint8_t *)slot.feature, strlen(slot.feature));
                appendBytes_(resp, (const uint8_t *)slot.code, strlen(slot.code));
                appendBytes_(resp, (const uint8_t *)wire_message.c_str(), wire_message.length());
                if (slot.notification_id > max_id)
                    max_id = slot.notification_id;
                ++item_count;
                ++emitted;
            }
        }
    }
    resp[0] = (uint8_t)(max_id & 0xFFu);
    resp[1] = (uint8_t)((max_id >> 8) & 0xFFu);
    resp[2] = (uint8_t)((max_id >> 16) & 0xFFu);
    resp[3] = (uint8_t)((max_id >> 24) & 0xFFu);
    resp[4] = item_count;
    return sendResponseBinary(source_node, kNotifyFeature, "pull", route.meta.request_id, resp.data(), resp.size());
}

bool StackRouteAdapter::handleNotificationAckRequest_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route)
{
    const uint8_t *p = route.payload;
    size_t left = route.payload_size;
    uint32_t upto_id = 0;
    if (left >= 4)
        readU32_(p, left, upto_id);
    {
        const auto guard = _lock.guard();
        if (upto_id)
            ackOutboxUpTo_(upto_id);
    }
    uint8_t resp[4] = {};
    resp[0] = (uint8_t)(upto_id & 0xFFu);
    resp[1] = (uint8_t)((upto_id >> 8) & 0xFFu);
    resp[2] = (uint8_t)((upto_id >> 16) & 0xFFu);
    resp[3] = (uint8_t)((upto_id >> 24) & 0xFFu);
    return sendResponseBinary(source_node, kNotifyFeature, "ack", route.meta.request_id, resp, sizeof(resp));
}

bool StackRouteAdapter::handleNotificationPullResponse_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route)
{
    {
        const auto guard = _lock.guard();
        if (_notify_sync.phase != NotifySyncPhase::PullWait || !_notify_sync.request_id ||
            route.meta.reply_to != _notify_sync.request_id || source_node != _notify_sync.node_id)
            return true;
    }

    const uint8_t *p = route.payload;
    size_t left = route.payload_size;
    uint32_t max_id = 0;
    uint8_t item_count = 0;
    if (!readU32_(p, left, max_id) || !readU8_(p, left, item_count))
        return true;
    for (uint8_t i = 0; i < item_count; ++i)
    {
        NotificationRecord record;
        record.used = true;
        record.is_binary = true;
        uint8_t level_len = 0;
        uint8_t feature_len = 0;
        uint8_t code_len = 0;
        uint16_t message_len = 0;
        uint16_t payload_len = 0;
        if (!readU32_(p, left, record.notification_id) || !readU32_(p, left, record.source_node) ||
            !readU32_(p, left, record.ts_ms) || !readU16_(p, left, record.repeat_count) ||
            !readU8_(p, left, record.priority) || !readU8_(p, left, level_len) || !readU8_(p, left, feature_len) ||
            !readU8_(p, left, code_len) || !readU16_(p, left, message_len) || !readU16_(p, left, payload_len))
            break;
        const uint8_t *level_data = nullptr;
        const uint8_t *feature_data = nullptr;
        const uint8_t *code_data = nullptr;
        const uint8_t *message_data = nullptr;
        const uint8_t *payload_data = nullptr;
        if (!readBytes_(p, left, level_data, level_len) || !readBytes_(p, left, feature_data, feature_len) ||
            !readBytes_(p, left, code_data, code_len) || !readBytes_(p, left, message_data, message_len) ||
            !readBytes_(p, left, payload_data, payload_len))
            break;
        const String level = textFromBytes_(level_data, level_len);
        const String feature = textFromBytes_(feature_data, feature_len);
        const String code = textFromBytes_(code_data, code_len);
        copyText_(record.level, sizeof(record.level), level.c_str());
        copyText_(record.feature, sizeof(record.feature), feature.c_str());
        copyText_(record.code, sizeof(record.code), code.c_str());
        record.message = textFromBytes_(message_data, message_len);
        (void)payload_data;
        (void)payload_len;
        if (record.notification_id > max_id)
            max_id = record.notification_id;
        DeferredNotifyInvoke invoke;
        emitNotification_(record, &invoke);
        invokeDeferredNotify_(invoke);
    }

    {
        const auto guard = _lock.guard();
        _notify_sync.phase = NotifySyncPhase::AckWait;
        _notify_sync.ack_upto_id = max_id;
        _notify_sync.request_id = nextInternalRequestId_();
        _notify_sync.deadline_ms = millis() + 1500u;
    }

    uint32_t request_id = 0;
    {
        const auto guard = _lock.guard();
        request_id = _notify_sync.request_id;
    }
    uint8_t ack[4] = {
        (uint8_t)(max_id & 0xFFu),
        (uint8_t)((max_id >> 8) & 0xFFu),
        (uint8_t)((max_id >> 16) & 0xFFu),
        (uint8_t)((max_id >> 24) & 0xFFu),
    };
    if (!sendRequestBinary(source_node, kNotifyFeature, "ack", ack, sizeof(ack), true, request_id))
    {
        _log.warn(F("STACK"), F("Notify ack send failed: node 0x%08lX upto %lu"), (unsigned long)source_node,
                  (unsigned long)max_id);
        const auto guard = _lock.guard();
        resetNotifySync_();
    }
    return true;
}

bool StackRouteAdapter::handleNotificationAckResponse_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route)
{
    const auto guard = _lock.guard();
    if (_notify_sync.phase != NotifySyncPhase::AckWait || !_notify_sync.request_id ||
        route.meta.reply_to != _notify_sync.request_id || source_node != _notify_sync.node_id)
        return true;
    resetNotifySync_();
    return true;
}

bool StackRouteAdapter::queueNotification_(uint32_t source_node, bool is_binary, const char *level, const char *feature,
                                           const char *code, const char *message, const String *payload_json)
{
    DeferredNotifyInvoke notify_invoke;
    bool should_invoke = false;
    bool result = false;
    {
        const auto guard = _lock.guard();
        if (!feature || !feature[0] || !level || !level[0])
            return false;

        if (!_slave && !hasMasterBackend_())
            return false;

        if (!_slave)
        {
            NotificationRecord record;
            if (!appendOutboxNotification_(record, source_node ? source_node : _local_node_id, level, feature, code, message,
                                           payload_json, is_binary))
                return false;
            emitNotification_(record, &notify_invoke);
            should_invoke = true;
            result = true;
        }
        else if (NotificationRecord *dup = findOutboxDuplicate_(source_node ? source_node : _local_node_id, is_binary, level,
                                                                feature, code, message, payload_json))
        {
            dup->ts_ms = millis();
            if (dup->repeat_count < 0xFFFFu)
                ++dup->repeat_count;
            result = true;
        }
        else
        {
            for (size_t i = 0; i < kNotifyOutboxCap; ++i)
            {
                NotificationRecord &slot = _notify_outbox[i];
                if (slot.used)
                    continue;
                result = appendOutboxNotification_(slot, source_node ? source_node : _local_node_id, level, feature, code, message,
                                                   payload_json, is_binary);
                break;
            }

            if (!result)
            {
                size_t drop_idx = 0;
                uint32_t oldest_ts = UINT32_MAX;
                for (size_t i = 0; i < kNotifyOutboxCap; ++i)
                {
                    if (_notify_outbox[i].ts_ms < oldest_ts)
                    {
                        oldest_ts = _notify_outbox[i].ts_ms;
                        drop_idx = i;
                    }
                }
                _log.warn(F("STACK"), F("Notify outbox full: drop oldest id %lu"),
                          (unsigned long)_notify_outbox[drop_idx].notification_id);
                result = appendOutboxNotification_(_notify_outbox[drop_idx], source_node ? source_node : _local_node_id, level,
                                                   feature, code, message, payload_json, is_binary);
            }
        }
    }
    if (should_invoke)
        invokeDeferredNotify_(notify_invoke);
    return result;
}

bool StackRouteAdapter::appendOutboxNotification_(NotificationRecord &entry, uint32_t source_node, const char *level, const char *feature,
                                                  const char *code, const char *message, const String *payload_json,
                                                  bool is_binary)
{
    entry = NotificationRecord{};
    entry.used = true;
    entry.is_binary = is_binary;
    entry.notification_id = _next_notify_id++;
    if (_next_notify_id == 0)
        _next_notify_id = 1;
    entry.source_node = source_node;
    entry.ts_ms = millis();
    entry.repeat_count = 1;
    entry.priority = notificationPriority_(level);
    copyText_(entry.level, sizeof(entry.level), level);
    copyText_(entry.feature, sizeof(entry.feature), feature);
    copyText_(entry.code, sizeof(entry.code), code ? code : "");
    if (message)
        entry.message = clampUtf8Bytes_(message, StackBinaryProtocol::kNotifyMessageMax);
    if (!is_binary && payload_json)
        entry.payload = *payload_json;
    return true;
}

StackRouteAdapter::NotificationRecord *StackRouteAdapter::findOutboxDuplicate_(uint32_t source_node, bool is_binary,
                                                                               const char *level, const char *feature,
                                                                               const char *code, const char *message,
                                                                               const String *payload_json)
{
    for (size_t i = 0; i < kNotifyOutboxCap; ++i)
    {
        NotificationRecord &slot = _notify_outbox[i];
        if (!slot.used || slot.source_node != source_node)
            continue;
        if (slot.is_binary != is_binary)
            continue;
        if (strcmp(slot.level, level) != 0 || strcmp(slot.feature, feature) != 0)
            continue;
        const char *norm_code = code ? code : "";
        const String norm_msg = clampUtf8Bytes_(message ? message : "", StackBinaryProtocol::kNotifyMessageMax);
        if (strcmp(slot.code, norm_code) != 0 || slot.message != norm_msg)
            continue;
        const String payload = (!is_binary && payload_json) ? *payload_json : String();
        if (slot.payload != payload)
            continue;
        return &slot;
    }
    return nullptr;
}

void StackRouteAdapter::appendHistory_(const NotificationRecord &record)
{
    const uint8_t idx = (uint8_t)((_notify_history_head + _notify_history_count) % kNotifyHistoryCap);
    if (_notify_history_count >= kNotifyHistoryCap)
    {
        _notify_history[_notify_history_head] = NotificationRecord{};
        _notify_history_head = (uint8_t)((_notify_history_head + 1u) % kNotifyHistoryCap);
        _notify_history_count = (uint8_t)(kNotifyHistoryCap - 1);
    }
    _notify_history[idx] = record;
    _notify_history[idx].used = true;
    ++_notify_history_count;
}

uint8_t StackRouteAdapter::notificationPriority_(const char *level)
{
    if (!level)
        return 0;
    if (strcmp(level, "critical") == 0)
        return 3;
    if (strcmp(level, "alarm") == 0 || strcmp(level, "error") == 0)
        return 2;
    if (strcmp(level, "warn") == 0 || strcmp(level, "warning") == 0)
        return 1;
    return 0;
}

bool StackRouteAdapter::buildPullResponsePayload_(DynamicJsonDocument &doc, size_t limit, uint32_t &max_id) const
{
    const auto guard = _lock.guard();
    JsonArray items = doc.createNestedArray("items");
    max_id = 0;
    if (limit == 0)
        limit = 1;

    size_t emitted = 0;
    for (uint8_t pass = 0; pass < 2 && emitted < limit; ++pass)
    {
        for (size_t i = 0; i < kNotifyOutboxCap && emitted < limit; ++i)
        {
            const NotificationRecord &slot = _notify_outbox[i];
            if (!slot.used)
                continue;
            if (pass == 0 && slot.priority < 2)
                continue;
            if (pass == 1 && slot.priority >= 2)
                continue;
            JsonObject item = items.createNestedObject();
            item["binary"] = slot.is_binary;
            item["notification_id"] = slot.notification_id;
            item["source_node"] = slot.source_node;
            item["ts_ms"] = slot.ts_ms;
            item["repeat_count"] = slot.repeat_count;
            item["priority"] = slot.priority;
            item["level"] = slot.level;
            item["feature"] = slot.feature;
            if (slot.code[0])
                item["code"] = slot.code;
            if (slot.message.length())
                item["message"] = slot.message;
            if (!slot.is_binary && slot.payload.length())
            {
                DynamicJsonDocument payload_doc(384);
                if (!deserializeJson(payload_doc, slot.payload.c_str(), slot.payload.length()))
                    item["payload"].set(payload_doc.as<JsonVariantConst>());
                else
                    item["payload_raw"] = slot.payload;
            }
            if (slot.notification_id > max_id)
                max_id = slot.notification_id;
            ++emitted;
        }
    }
    return true;
}

void StackRouteAdapter::ackOutboxUpTo_(uint32_t upto_id)
{
    if (upto_id == 0)
        return;
    for (size_t i = 0; i < kNotifyOutboxCap; ++i)
    {
        NotificationRecord &slot = _notify_outbox[i];
        if (slot.used && slot.notification_id <= upto_id)
            slot = NotificationRecord{};
    }
}

bool StackRouteAdapter::tryStartNotificationPoll_()
{
    const StackDeviceRegistry *registry = nullptr;
    uint32_t local_node_id = 0;
    {
        const auto guard = _lock.guard();
        if (!hasRs485PollBackend_())
            return false;
        if (_notify_sync.phase != NotifySyncPhase::Idle)
            return false;
        if (_rs485_master)
            registry = &_rs485_master->router().registry();
        local_node_id = _local_node_id;
    }
    if (!registry)
        return false;

    for (size_t attempt = 0; attempt < StackDeviceRegistry::kMaxDevices; ++attempt)
    {
        size_t idx = 0;
        {
            const auto guard = _lock.guard();
            idx = (_notify_poll_cursor + attempt) % StackDeviceRegistry::kMaxDevices;
        }
        StackDeviceRegistry::DeviceInfo device;
        if (!registry->snapshotAt(idx, device) || !device.online || device.node_id == 0 || device.node_id == local_node_id)
            continue;

        DynamicJsonDocument req(128);
        req["limit"] = 4;
        uint32_t request_id = 0;
        PayloadMode payload_mode = PayloadMode::Json;
        {
            const auto guard = _lock.guard();
            if (_notify_sync.phase != NotifySyncPhase::Idle)
                return false;
            _notify_sync.phase = NotifySyncPhase::PullWait;
            _notify_sync.node_id = device.node_id;
            _notify_sync.request_id = nextInternalRequestId_();
            _notify_sync.ack_upto_id = 0;
            _notify_sync.deadline_ms = millis() + 1500u;
            _notify_poll_cursor = (uint8_t)((idx + 1u) % StackDeviceRegistry::kMaxDevices);
            request_id = _notify_sync.request_id;
            payload_mode = _payload_mode;
        }
        bool sent = false;
        if (payload_mode == PayloadMode::Binary)
        {
            uint8_t req_bin[2] = {4u, 0u};
            sent = sendRequestBinary(device.node_id, kNotifyFeature, "pull", req_bin, sizeof(req_bin), true, request_id);
        }
        else
        {
            sent = sendRequest(device.node_id, kNotifyFeature, "pull", &req, Mode::Json, true, request_id);
        }
        if (!sent)
        {
            const auto guard = _lock.guard();
            if (_notify_sync.phase == NotifySyncPhase::PullWait && _notify_sync.request_id == request_id &&
                _notify_sync.node_id == device.node_id)
                resetNotifySync_();
            continue;
        }
        return true;
    }
    return false;
}

bool StackRouteAdapter::hasMasterBackend_() const
{
    return _master_ws_active || _master_rs485_active;
}

bool StackRouteAdapter::hasRs485PollBackend_() const
{
    if (!hasMasterBackend_())
        return false;
    if (_exchange_policy == ExchangePolicy::Poll)
        return true;
    return false;
}

bool StackRouteAdapter::canSlavePushDirect_() const
{
    if (!_slave_active || !_slave || !_slave->isAuthorized())
        return false;
    if (_exchange_policy == ExchangePolicy::Poll)
        return false;
    const StackTransport::Caps caps = _slave->caps();
    return caps.full_duplex && caps.can_push_async && !caps.requires_arbitration;
}

StackRouteAdapter::RoutePlane StackRouteAdapter::classifyPlane_(const char *feature, const char *action)
{
    (void)action;
    if (!feature || !feature[0])
        return RoutePlane::Control;
    if (strcmp(feature, kXchgFeature) == 0 || strcmp(feature, kNotifyFeature) == 0)
        return RoutePlane::Control;
    return RoutePlane::Data;
}

StackRouteAdapter::Mode StackRouteAdapter::modeForDataPlane_(ConfigsManagerIface::StackPayloadMode payload_mode)
{
    if (payload_mode == ConfigsManagerIface::StackPayloadMode::Binary)
        return Mode::Binary;
    return Mode::Json;
}

StackRouteAdapter::Mode StackRouteAdapter::chooseMode_(const char *feature, const char *action,
                                                       const JsonDocument *payload, Mode requested) const
{
    (void)feature;
    (void)action;
    (void)payload;
    return requested;
}

uint32_t StackRouteAdapter::nextInternalRequestId_()
{
    const uint32_t out = _next_internal_request_id++;
    if (_next_internal_request_id == 0)
        _next_internal_request_id = 1;
    return out ? out : nextInternalRequestId_();
}

void StackRouteAdapter::resetNotifySync_()
{
    _notify_sync = NotifySyncState{};
}

bool StackRouteAdapter::isInternalNotificationRequest_(const StackJsonProtocol::RouteMessage &route) const
{
    return strcmp(route.feature, kNotifyFeature) == 0;
}

bool StackRouteAdapter::copyText_(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0)
        return false;
    dst[0] = '\0';
    if (!src)
        return true;
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
    return true;
}

void StackRouteAdapter::emitNotification_(const NotificationRecord &record, DeferredNotifyInvoke *invoke)
{
    const auto guard = _lock.guard();
    appendHistory_(record);
    if (!_notify_cb || !invoke)
        return;

    invoke->cb = _notify_cb;
    invoke->ctx = _notify_ctx;
    invoke->source_node = record.source_node;
    invoke->notify.source_node = record.source_node;
    invoke->notify.is_binary = record.is_binary;
    copyText_(invoke->notify.level, sizeof(invoke->notify.level), record.level);
    copyText_(invoke->notify.feature, sizeof(invoke->notify.feature), record.feature);
    copyText_(invoke->notify.code, sizeof(invoke->notify.code), record.code);
    invoke->notify.message = record.message;
    if (!record.is_binary)
        invoke->notify.payload = record.payload;
}

void StackRouteAdapter::invokeDeferredJson_(const DeferredJsonInvoke &invoke)
{
    if (invoke.cb)
        invoke.cb(invoke.ctx, invoke.source_node, invoke.route);
}

void StackRouteAdapter::invokeDeferredBinary_(const DeferredBinaryInvoke &invoke)
{
    if (invoke.cb)
        invoke.cb(invoke.ctx, invoke.source_node, invoke.route);
}

void StackRouteAdapter::invokeDeferredNotify_(const DeferredNotifyInvoke &invoke)
{
    if (invoke.cb)
        invoke.cb(invoke.ctx, invoke.source_node, invoke.notify);
}

bool StackRouteAdapter::sendMasterJson_(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload,
                                        const StackTransport::RouteMeta *meta)
{
    if (!_master || target_node == 0 || !feature || !feature[0] || !action || !action[0])
        return false;
    const String msg = StackJsonProtocol::makeRoute(_local_node_id, target_node, feature, action, payload, meta);
    return _master->router().sendText(target_node, msg.c_str());
}

bool StackRouteAdapter::sendMasterBinary_(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload,
                                          size_t payload_size, const StackTransport::RouteMeta *meta)
{
    if (!_master || target_node == 0 || !feature || !feature[0] || !action || !action[0])
        return false;
    const size_t frame_size = StackBinaryProtocol::encodedRouteSize(feature, action, payload_size);
    std::unique_ptr<uint8_t[]> frame(new uint8_t[frame_size]);
    size_t used = 0;
    if (!frame || !StackBinaryProtocol::encodeRoute(_local_node_id, target_node, feature, action, payload, payload_size,
                                                    frame.get(), frame_size, used, meta))
        return false;
    return _master->router().sendBinary(target_node, frame.get(), used);
}

bool StackRouteAdapter::sendRs485Json_(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload,
                                       const StackTransport::RouteMeta *meta)
{
    if (!_rs485_master || target_node == 0 || !feature || !feature[0] || !action || !action[0])
        return false;
    const String msg = StackJsonProtocol::makeRoute(_local_node_id, target_node, feature, action, payload, meta);
    return _rs485_master->router().sendText(target_node, msg.c_str());
}

bool StackRouteAdapter::sendRs485Binary_(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload,
                                         size_t payload_size, const StackTransport::RouteMeta *meta)
{
    if (!_rs485_master || target_node == 0 || !feature || !feature[0] || !action || !action[0])
        return false;
    const size_t frame_size = StackBinaryProtocol::encodedRouteSize(feature, action, payload_size);
    std::unique_ptr<uint8_t[]> frame(new uint8_t[frame_size]);
    size_t used = 0;
    if (!frame || !StackBinaryProtocol::encodeRoute(_local_node_id, target_node, feature, action, payload, payload_size,
                                                    frame.get(), frame_size, used, meta))
        return false;
    return _rs485_master->router().sendBinary(target_node, frame.get(), used);
}
