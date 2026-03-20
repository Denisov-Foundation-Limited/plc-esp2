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
#include <ArduinoJson.h>
#include <stdint.h>
#include <array>

#include "core/network/stack/stack_protocol.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_transport.hpp"
#include "core/network/stack/stack_types.hpp"
#include "utils/rtos_lock.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"

class StackMaster
{
public:
    static constexpr size_t MAX_SESSIONS = 8;
    static constexpr uint8_t kMaxQueuedTx = 32;
    static constexpr uint32_t kTxRetryMs = 80;
    static constexpr uint8_t kTxMaxAttempts = 8;
    static constexpr uint32_t kTxWarmupMs = 500;
    static constexpr uint32_t kTxBackoffMs = 250;
    static constexpr uint8_t kTxFailRestartThreshold = 4;
    static constexpr uint8_t kTxStallQueueDepthThreshold = 4;
    static constexpr uint32_t kTxStallResetMs = 5000;
    static constexpr uint8_t kQueueDumpItems = 4;
    static constexpr uint32_t kStatsLogMs = 30000;
    using FrameHandler = void (*)(void *ctx, uint32_t node_id, const StackFrame &frame);
    using EventHandler = void (*)(void *ctx, uint32_t node_id, bool online);
    struct TxStats
    {
        uint8_t depth = 0;
        uint32_t queued = 0;
        uint32_t retries = 0;
        uint32_t coalesced = 0;
        uint32_t dropped = 0;
        uint32_t queue_full = 0;
        uint32_t session_resets = 0;
        uint32_t last_reset_ms = 0;
    };

    StackMaster(StackTransportServer &server, Logger &log) : _server(&server), _log(&log) {}

    void setFrameHandler(FrameHandler cb, void *ctx)
    {
        _frame_cb = cb;
        _frame_ctx = ctx;
    }

    void setFrameHandlerSecondary(FrameHandler cb, void *ctx)
    {
        _frame_cb_secondary = cb;
        _frame_ctx_secondary = ctx;
    }

    void setFrameHandlerTertiary(FrameHandler cb, void *ctx)
    {
        _frame_cb_tertiary = cb;
        _frame_ctx_tertiary = ctx;
    }

    void setEventHandler(EventHandler cb, void *ctx)
    {
        _event_cb = cb;
        _event_ctx = ctx;
    }

    void setConfigsManager(ConfigsManagerIface &cfg) { _configs = &cfg; }

    void begin()
    {
        if (!_server)
        {
            if (_log)
                _log->warn(F("STACK"), F("Master server missing"));
            return;
        }
        if (_log)
            _log->info(F("STACK"), F("Master server begin"));
        _server->setClientHandler(&StackMaster::onClientStatic_, this);
        _server->begin();
    }

    void loop()
    {
        flushQueuedTx_();
        logTxStats_();
    }

    void setSessionSilenceTimeoutMs(uint32_t ms) { _session_silence_timeout_ms = ms; }

    size_t nodeCount() const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return 0;
        size_t count = 0;
        for (const auto &s : _sessions)
            if (s.used && s.data.has_id && s.data.client)
                ++count;
        return count;
    }

    uint32_t nodeIdAt(size_t idx) const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return 0;
        size_t pos = 0;
        for (const auto &s : _sessions)
        {
            if (!s.used || !s.data.has_id || !s.data.client)
                continue;
            if (pos == idx)
                return s.data.node_id;
            ++pos;
        }
        return 0;
    }

    String nodeNameAt(size_t idx) const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return "";
        size_t pos = 0;
        for (const auto &s : _sessions)
        {
            if (!s.used || !s.data.has_id || !s.data.client)
                continue;
            if (pos == idx)
                return s.data.name;
            ++pos;
        }
        return "";
    }

    uint32_t nodeCapsAt(size_t idx) const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return 0;
        size_t pos = 0;
        for (const auto &s : _sessions)
        {
            if (!s.used || !s.data.has_id || !s.data.client)
                continue;
            if (pos == idx)
                return s.data.caps;
            ++pos;
        }
        return 0;
    }

    bool nodeIsControllerAt(size_t idx) const
    {
        return stackCapsHas(nodeCapsAt(idx), StackCapController);
    }

    bool nodeIsController(uint32_t node_id) const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return false;
        for (const auto &s : _sessions)
        {
            if (!s.used || !s.data.has_id || !s.data.client)
                continue;
            if (s.data.node_id == node_id)
                return stackCapsHas(s.data.caps, StackCapController);
        }
        return false;
    }

    String nodeIpAt(size_t idx) const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return "";
        size_t pos = 0;
        for (const auto &s : _sessions)
        {
            if (!s.used || !s.data.has_id || !s.data.client)
                continue;
            if (pos == idx)
                return s.data.ip;
            ++pos;
        }
        return "";
    }

    bool nodeInfo(uint32_t node_id, String &name, String &ip, uint16_t &fw_ver) const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return false;
        for (const auto &s : _sessions)
        {
            if (!s.used || !s.data.has_id || !s.data.client)
                continue;
            if (s.data.node_id == node_id)
            {
                name = s.data.name;
                ip = s.data.ip;
                fw_ver = s.data.fw_ver;
                return true;
            }
        }
        return false;
    }

    bool nodeIsOnline(uint32_t node_id, uint32_t max_silence_ms = 0) const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return false;
        for (const auto &s : _sessions)
        {
            if (!s.used || !s.data.has_id || !s.data.client)
                continue;
            if (s.data.node_id != node_id)
                continue;
            if (!s.data.client->connected())
                return false;
            if (max_silence_ms == 0)
                return true;
            const uint32_t now = millis();
            return (uint32_t)(now - s.data.last_seen_ms) <= max_silence_ms;
        }
        return false;
    }

    bool sendTo(uint32_t node_id, uint8_t type, const uint8_t *payload, size_t len)
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return false;
        Session *s = findByNode_(node_id);
        if (s && s->client && s->client->connected() &&
            sendToNowLocked_(node_id, type, payload, len) == TxResult::Sent)
            return true;
        s = findByNode_(node_id);
        return enqueueTxLocked_(node_id, s ? s->epoch : 0, type, payload, len);
    }

    bool enqueueTo(uint32_t node_id, uint8_t type, const uint8_t *payload, size_t len)
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return false;
        Session *s = findByNode_(node_id);
        return enqueueTxLocked_(node_id, s ? s->epoch : 0, type, payload, len);
    }

    void broadcast(uint8_t type, const uint8_t *payload, size_t len)
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return;
        for (auto &s : _sessions)
        {
            if (!s.used || !s.data.client || !s.data.client->connected() || !s.data.has_id)
                continue;
            if (sendToNowLocked_(s.data.node_id, type, payload, len) == TxResult::Sent)
                continue;
            enqueueTxLocked_(s.data.node_id, s.data.epoch, type, payload, len);
        }
    }

    TxStats txStats() const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return {};
        TxStats st = _tx_stats;
        for (const auto &q : _queued_tx)
            if (q.used && st.depth < 0xFF)
                ++st.depth;
        return st;
    }

    uint8_t queueDepth(uint32_t node_id) const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return 0;
        return queueDepthForNodeLocked_(node_id);
    }

    bool hasQueuedCommand(uint32_t node_id, uint8_t type, StackFeature feature, const char *action) const
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
            return false;
        const String action_str = action ? String(action) : String();
        for (const auto &q : _queued_tx)
        {
            if (!q.used || q.node_id != node_id || q.type != type)
                continue;
            String feature_name;
            String action_name;
            describeStackTxPayload_(q.type, q.payload, q.len, feature_name, action_name);
            const char *expected_feature = stackFeatureName_((uint8_t)feature);
            if (expected_feature && feature_name != expected_feature)
                continue;
            if (action_str.length() && action_name != action_str)
                continue;
            return true;
        }
        return false;
    }

private:
    enum class TxResult : uint8_t
    {
        Sent = 0,
        Warmup,
        Offline,
        Busy,
        ShortWrite,
        EncodeError
    };

    struct Session
    {
        StackTransportConnection *client = nullptr;
        StackTransportConnection *retired_client = nullptr;
        StackCodec codec;
        uint32_t node_id = 0;
        uint32_t epoch = 0;
        bool has_id = false;
        bool closing = false;
        uint16_t inflight_ops = 0;
        String name;
        String ip;
        uint16_t fw_ver = 0;
        uint32_t caps = 0;
        uint32_t last_seen_ms = 0;
        uint32_t last_tx_ok_ms = 0;
        uint32_t tx_ready_ms = 0;
        uint8_t tx_fail_streak = 0;
    };

    struct QueuedTx
    {
        bool used = false;
        uint32_t node_id = 0;
        uint32_t session_epoch = 0;
        uint8_t type = 0;
        uint16_t len = 0;
        uint8_t attempts = 0;
        uint32_t next_retry_ms = 0;
        uint8_t payload[StackCodec::kMaxPayload] = {};
    };

    StackTransportServer *_server = nullptr;
    uint8_t _payload_buf[StackCodec::kMaxPayload] = {};
    uint8_t _frame_buf[StackCodec::kMaxFrame] = {};
    struct Slot
    {
        bool used = false;
        Session data;
    };
    std::array<Slot, MAX_SESSIONS> _sessions = {};
    FrameHandler _frame_cb = nullptr;
    void *_frame_ctx = nullptr;
    FrameHandler _frame_cb_secondary = nullptr;
    void *_frame_ctx_secondary = nullptr;
    FrameHandler _frame_cb_tertiary = nullptr;
    void *_frame_ctx_tertiary = nullptr;
    EventHandler _event_cb = nullptr;
    void *_event_ctx = nullptr;
    Logger *_log = nullptr;
    ConfigsManagerIface *_configs = nullptr;
    DynamicJsonDocument _tx_doc{2048};
    mutable RtosRecursiveLock _lock;
    uint32_t _session_silence_timeout_ms = 7000;
    uint32_t _session_epoch_seq = 0;
    uint32_t _rr_cursor = 0;
    std::array<QueuedTx, kMaxQueuedTx> _queued_tx = {};
    TxStats _tx_stats = {};
    uint32_t _last_stats_log_ms = 0;
    TxStats _last_logged_tx_stats = {};

    static void onClientStatic_(void *ctx, StackTransportConnection *client)
    {
        if (!ctx)
            return;
        static_cast<StackMaster *>(ctx)->onClient_(client);
    }

    static void onDataStatic_(void *ctx, StackTransportConnection *client, const uint8_t *data, size_t len)
    {
        if (!ctx)
            return;
        static_cast<StackMaster *>(ctx)->onData_(client, data, len);
    }

    static void onDisconnectStatic_(void *ctx, StackTransportConnection *client)
    {
        if (!ctx)
            return;
        static_cast<StackMaster *>(ctx)->onDisconnect_(client);
    }

    static void onErrorStatic_(void *ctx, StackTransportConnection *client, int8_t)
    {
        if (!ctx)
            return;
        static_cast<StackMaster *>(ctx)->onDisconnect_(client);
    }

    void onClient_(StackTransportConnection *client)
    {
        const auto guard = _lock.guard();
        if (!guard.locked())
        {
            if (client)
            {
                client->close(true);
                client->destroy();
            }
            return;
        }
        Session *s = allocSession_();
        if (!s)
        {
            client->close(true);
            client->destroy();
            return;
        }
        s->client = client;
        s->epoch = ++_session_epoch_seq;
        s->last_seen_ms = millis();
        s->last_tx_ok_ms = s->last_seen_ms;
        s->tx_ready_ms = s->last_seen_ms + kTxWarmupMs;
        if (client)
            s->ip = client->remoteIp();

        client->setDataHandler(&StackMaster::onDataStatic_, this);
        client->setDisconnectHandler(&StackMaster::onDisconnectStatic_, this);
        client->setErrorHandler(&StackMaster::onErrorStatic_, this);
    }

    void onData_(StackTransportConnection *client, const uint8_t *data, size_t len)
    {
        Session *s = nullptr;
        {
            const auto guard = _lock.guard();
            if (!guard.locked())
                return;
            s = findByClient_(client);
            if (!s || s->closing)
                return;
            s->last_seen_ms = millis();
            ++s->inflight_ops;
        }
        FrameCtx ctx{this, s};
        s->codec.feed(
            data, len,
            [](void *ctx, const StackFrame &frame) {
                FrameCtx *fc = static_cast<FrameCtx *>(ctx);
                fc->self->handleFrame_(fc->session, frame);
            },
            &ctx);
        {
            const auto guard = _lock.guard();
            if (!guard.locked())
                return;
            if (s->inflight_ops > 0)
                --s->inflight_ops;
            if (s->closing && s->inflight_ops == 0)
                finalizeSessionLocked_(s);
        }
    }

    void onDisconnect_(StackTransportConnection *client)
    {
        uint32_t notify_node_id = 0;
        EventHandler event_cb = nullptr;
        void *event_ctx = nullptr;
        {
            const auto guard = _lock.guard();
            if (!guard.locked())
                return;
            Session *s = findByClient_(client);
            if (!s)
                return;
            if (s->has_id)
            {
                notify_node_id = s->node_id;
                event_cb = _event_cb;
                event_ctx = _event_ctx;
            }
            removeSessionLocked_(s, false);
        }
        if (notify_node_id != 0 && event_cb)
            event_cb(event_ctx, notify_node_id, false);
    }

    Session *findByClient_(StackTransportConnection *client)
    {
        for (auto &slot : _sessions)
            if (slot.used && slot.data.client == client)
                return &slot.data;
        return nullptr;
    }

    Session *findByNode_(uint32_t node_id)
    {
        for (auto &slot : _sessions)
            if (slot.used && slot.data.has_id && slot.data.node_id == node_id)
                return &slot.data;
        return nullptr;
    }

    struct FrameCtx
    {
        StackMaster *self = nullptr;
        Session *session = nullptr;
    };

    void handleFrame_(Session *session, const StackFrame &frame)
    {
        FrameHandler frame_cb = nullptr;
        void *frame_ctx = nullptr;
        FrameHandler frame_cb_secondary = nullptr;
        void *frame_ctx_secondary = nullptr;
        FrameHandler frame_cb_tertiary = nullptr;
        void *frame_ctx_tertiary = nullptr;
        EventHandler event_cb = nullptr;
        void *event_ctx = nullptr;
        uint32_t event_node_id = 0;
        bool event_online = false;
        uint32_t node_id = 0;
        {
            const auto guard = _lock.guard();
            if (!guard.locked())
                return;
            if (frame.type == (uint8_t)StackMsgType::Hello)
            {
                StackHello hello{};
                if (StackHello::decode(frame.payload, frame.payload_len, hello))
                {
                    Session *s = findByNode_(hello.node_id);
                    if (s && s != session)
                    {
                        const bool same_ip = (session && s->ip.length() && session->ip.length() && s->ip == session->ip);
                        const bool same_name = (s->name.length() && hello.name.length() && s->name == hello.name);
                        if (_log)
                        {
                            if (same_ip || same_name)
                            {
                                _log->info(F("STACK"), F("Unit reconnected: %s unit_id: 0x%08lX"),
                                           hello.name.length() ? hello.name.c_str() : "-",
                                           (unsigned long)hello.node_id);
                            }
                            else
                            {
                                _log->warn(F("STACK"),
                                           F("Duplicate unit_id conflict: 0x%08lX old: %s ip: %s new: %s ip: %s"),
                                           (unsigned long)hello.node_id,
                                           s->name.length() ? s->name.c_str() : "-",
                                           s->ip.length() ? s->ip.c_str() : "n/a",
                                           hello.name.length() ? hello.name.c_str() : "-",
                                           (session && session->ip.length()) ? session->ip.c_str() : "n/a");
                            }
                        }
                        removeSessionLocked_(s, true);
                        s = nullptr;
                    }
                    if (!s)
                    {
                        if (session)
                            s = session;
                        else
                        {
                            for (auto &slot : _sessions)
                                if (slot.used && !slot.data.has_id)
                                {
                                    s = &slot.data;
                                    break;
                                }
                        }
                    }
                    if (s)
                    {
                        const bool first_online = (!s->has_id) || (s->node_id != hello.node_id);
                        s->node_id = hello.node_id;
                        s->has_id = true;
                        s->name = hello.name;
                        s->fw_ver = hello.fw_ver;
                        s->caps = hello.caps;
                        const uint32_t now = millis();
                        if (s->tx_ready_ms < now + kTxWarmupMs)
                            s->tx_ready_ms = now + kTxWarmupMs;
                        if (first_online)
                        {
                            event_cb = _event_cb;
                            event_ctx = _event_ctx;
                            event_node_id = hello.node_id;
                            event_online = true;
                        }
                    }
                }
            }
            node_id = nodeIdFromFrame_(session, frame);
            frame_cb = _frame_cb;
            frame_ctx = _frame_ctx;
            frame_cb_secondary = _frame_cb_secondary;
            frame_ctx_secondary = _frame_ctx_secondary;
            frame_cb_tertiary = _frame_cb_tertiary;
            frame_ctx_tertiary = _frame_ctx_tertiary;
        }
        if (event_node_id != 0 && event_cb)
            event_cb(event_ctx, event_node_id, event_online);
        if (frame_cb)
            frame_cb(frame_ctx, node_id, frame);
        if (frame_cb_secondary)
            frame_cb_secondary(frame_ctx_secondary, node_id, frame);
        if (frame_cb_tertiary)
            frame_cb_tertiary(frame_ctx_tertiary, node_id, frame);
    }

    TxResult sendToNowLocked_(uint32_t node_id, uint8_t type, const uint8_t *payload, size_t len)
    {
        Session *s = findByNode_(node_id);
        if (!s || !s->client || !s->client->connected())
            return TxResult::Offline;
        const uint32_t now = millis();
        if ((int32_t)(now - s->tx_ready_ms) < 0)
            return TxResult::Warmup;
        const uint8_t *payload_ptr = payload;
        size_t payload_len = len;
        if (_configs && (type == (uint8_t)StackMsgType::CmdGet || type == (uint8_t)StackMsgType::CmdSet))
        {
            const String key = _configs->stackApiKey();
            if (key.length() > 0 && payload && len > 0)
            {
                _tx_doc.clear();
                DeserializationError err = deserializeJson(_tx_doc, payload, len);
                if (!err)
                {
                    if (!_tx_doc.containsKey("api_key"))
                        _tx_doc["api_key"] = key;
                    const size_t new_len = serializeJson(_tx_doc, _payload_buf, sizeof(_payload_buf));
                    if (new_len > 0 && new_len <= sizeof(_payload_buf))
                    {
                        payload_ptr = _payload_buf;
                        payload_len = new_len;
                    }
                }
            }
        }
        const size_t frame_len = StackCodec::encode(type, payload_ptr, payload_len, _frame_buf, sizeof(_frame_buf));
        if (frame_len == 0)
            return TxResult::EncodeError;
        String feature_name;
        String action_name;
        describeStackTxPayload_(type, payload_ptr, payload_len, feature_name, action_name);
        if (!s->client->canSend())
        {
            if (_log)
                _log->warn(F("STACK"), F("Master tx busy: node_id: 0x%08lX type: %u feature: %s action: %s len: %u"),
                           (unsigned long)node_id, (unsigned)type,
                           feature_name.length() ? feature_name.c_str() : "-",
                           action_name.length() ? action_name.c_str() : "-",
                           (unsigned)frame_len);
            if (s->tx_ready_ms < now + kTxBackoffMs)
                s->tx_ready_ms = now + kTxBackoffMs;
            return TxResult::Busy;
        }
        const size_t written = s->client->write(_frame_buf, frame_len);
        if (written != frame_len)
        {
            const uint8_t queue_depth = queueDepthForNodeLocked_(node_id);
            const uint32_t silent_ms = (uint32_t)(now - s->last_seen_ms);
            const uint32_t tx_idle_ms = (uint32_t)(now - s->last_tx_ok_ms);
            const String queue_dump = queueSummaryForNodeLocked_(node_id, kQueueDumpItems);
            if (written == 0)
            {
                if (s->tx_fail_streak < 0xFF)
                    ++s->tx_fail_streak;
            }
            else
            {
                s->tx_fail_streak = 0;
            }
            if (_log)
                _log->warn(F("STACK"),
                           F("Master tx short write: node_id: 0x%08lX type: %u feature: %s action: %s wr: %u len: %u connected: %u can_send: %u age_ms: %lu qdepth: %u streak: %u"),
                           (unsigned long)node_id,
                           (unsigned)type,
                           feature_name.length() ? feature_name.c_str() : "-",
                           action_name.length() ? action_name.c_str() : "-",
                           (unsigned)written,
                           (unsigned)frame_len,
                           s->client->connected() ? 1u : 0u,
                           s->client->canSend() ? 1u : 0u,
                           (unsigned long)silent_ms,
                           (unsigned)queue_depth,
                           (unsigned)s->tx_fail_streak);
            if (_log && queue_dump.length())
                _log->warn(F("STACK"), F("Master tx queue: node_id: 0x%08lX items: %s"),
                           (unsigned long)node_id, queue_dump.c_str());
            if (s->tx_ready_ms < now + kTxBackoffMs)
                s->tx_ready_ms = now + kTxBackoffMs;
            const bool tx_stalled = (queue_depth >= kTxStallQueueDepthThreshold) &&
                                    (tx_idle_ms >= kTxStallResetMs) &&
                                    (_session_silence_timeout_ms != 0) &&
                                    (silent_ms >= _session_silence_timeout_ms) &&
                                    (s->tx_fail_streak >= 2);
            if (written == 0 && (s->tx_fail_streak >= kTxFailRestartThreshold || tx_stalled))
            {
                if (_log)
                    _log->warn(F("STACK"),
                               F("Master tx reset session: node_id: 0x%08lX streak: %u age_ms: %lu tx_idle_ms: %lu qdepth: %u"),
                               (unsigned long)node_id,
                               (unsigned)s->tx_fail_streak,
                               (unsigned long)silent_ms,
                               (unsigned long)tx_idle_ms,
                               (unsigned)queue_depth);
                if (_log && queue_dump.length())
                    _log->warn(F("STACK"), F("Master tx reset queue: node_id: 0x%08lX items: %s"),
                               (unsigned long)node_id, queue_dump.c_str());
                ++_tx_stats.session_resets;
                _tx_stats.last_reset_ms = now;
                removeSessionLocked_(s, true);
                return TxResult::Offline;
            }
            return TxResult::ShortWrite;
        }
        s->last_tx_ok_ms = now;
        s->tx_fail_streak = 0;
        return TxResult::Sent;
    }

    bool enqueueTxLocked_(uint32_t node_id, uint32_t session_epoch, uint8_t type,
                          const uint8_t *payload, size_t len)
    {
        if (!payload || len == 0 || len > StackCodec::kMaxPayload)
            return false;
        String feature_name;
        String action_name;
        describeStackTxPayload_(type, payload, len, feature_name, action_name);
        for (auto &q : _queued_tx)
        {
            if (!q.used)
                continue;
            if (q.node_id != node_id || q.session_epoch != session_epoch || q.type != type)
                continue;
            if (!payloadEquivalentForQueue_(type, q.payload, q.len, payload, len))
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
            q.node_id = node_id;
            q.session_epoch = session_epoch;
            q.type = type;
            q.len = (uint16_t)len;
            q.attempts = 1;
            q.next_retry_ms = millis() + kTxRetryMs;
            memcpy(q.payload, payload, len);
            ++_tx_stats.queued;
            return true;
        }
        if (_log)
            _log->warn(F("STACK"), F("Master tx queue full: node_id: 0x%08lX type: %u feature: %s action: %s len: %u"),
                       (unsigned long)node_id, (unsigned)type,
                       feature_name.length() ? feature_name.c_str() : "-",
                       action_name.length() ? action_name.c_str() : "-",
                       (unsigned)len);
        ++_tx_stats.queue_full;
        return false;
    }

    static const char *stackFeatureName_(uint8_t feature)
    {
        switch (static_cast<StackFeature>(feature))
        {
        case StackFeature::System: return "system";
        case StackFeature::Ports: return "ports";
        case StackFeature::TempSensors: return "temp_sensors";
        case StackFeature::I2cScan: return "i2c";
        case StackFeature::OwScan: return "ow";
        case StackFeature::Fan: return "fan";
        case StackFeature::Rtc: return "rtc";
        case StackFeature::PlcStatus: return "plc";
        case StackFeature::Relays: return "relays";
        case StackFeature::DigitalInputs: return "din";
        case StackFeature::Telegram: return "telegram";
        case StackFeature::Storage: return "storage";
        case StackFeature::Extenders: return "extenders";
        case StackFeature::Sockets: return "sockets";
        case StackFeature::Meteo: return "meteo";
        case StackFeature::Thermo: return "thermo";
        case StackFeature::Security: return "security";
        case StackFeature::Septic: return "septic";
        case StackFeature::Tanks: return "tanks";
        case StackFeature::Ring: return "ring";
        case StackFeature::Watering: return "watering";
        case StackFeature::Avr: return "avr";
        case StackFeature::Leak: return "leak";
        case StackFeature::Groups: return "groups";
        }
        return nullptr;
    }

    static void describeStackTxPayload_(uint8_t type, const uint8_t *payload, size_t len,
                                        String &feature_out, String &action_out)
    {
        feature_out = "";
        action_out = "";
        if (!payload || len == 0)
            return;
        if (type != (uint8_t)StackMsgType::CmdGet &&
            type != (uint8_t)StackMsgType::CmdSet &&
            type != (uint8_t)StackMsgType::Ack &&
            type != (uint8_t)StackMsgType::Err)
            return;
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, payload, len))
            return;
        if (doc["feature"].is<unsigned>())
        {
            const uint8_t feature = (uint8_t)doc["feature"].as<unsigned>();
            const char *name = stackFeatureName_(feature);
            if (name && name[0] != '\0')
                feature_out = name;
            else
                feature_out = String(feature);
        }
        if (doc["action"].is<const char *>())
            action_out = String(doc["action"].as<const char *>());
    }

    static bool payloadEquivalentForQueue_(uint8_t type,
                                           const uint8_t *a, size_t a_len,
                                           const uint8_t *b, size_t b_len)
    {
        if (!a || !b)
            return false;
        if (type != (uint8_t)StackMsgType::CmdGet)
        {
            if (a_len != b_len)
                return false;
            return memcmp(a, b, a_len) == 0;
        }

        StaticJsonDocument<256> doc_a;
        StaticJsonDocument<256> doc_b;
        DeserializationError err_a = deserializeJson(doc_a, a, a_len);
        DeserializationError err_b = deserializeJson(doc_b, b, b_len);
        if (err_a || err_b)
        {
            if (a_len != b_len)
                return false;
            return memcmp(a, b, a_len) == 0;
        }

        doc_a.remove("cmd_id");
        doc_b.remove("cmd_id");

        char norm_a[StackCodec::kMaxPayload] = {};
        char norm_b[StackCodec::kMaxPayload] = {};
        const size_t norm_a_len = serializeJson(doc_a, norm_a, sizeof(norm_a));
        const size_t norm_b_len = serializeJson(doc_b, norm_b, sizeof(norm_b));
        if (norm_a_len == 0 || norm_b_len == 0 || norm_a_len != norm_b_len)
            return false;
        return memcmp(norm_a, norm_b, norm_a_len) == 0;
    }

    void flushQueuedTx_()
    {
        struct Event
        {
            uint32_t node_id = 0;
            bool online = false;
        };
        Event events[MAX_SESSIONS] = {};
        size_t event_count = 0;
        EventHandler event_cb = nullptr;
        void *event_ctx = nullptr;
        {
            const auto guard = _lock.guard();
            if (!guard.locked())
                return;
            const uint32_t now = millis();
            for (size_t step = 0; step < _queued_tx.size(); ++step)
            {
                const size_t idx = (_rr_cursor + step) % _queued_tx.size();
                auto &q = _queued_tx[idx];
                if (!q.used)
                    continue;
                if ((int32_t)(now - q.next_retry_ms) < 0)
                    continue;
                Session *s = findByNode_(q.node_id);
                if (q.session_epoch != 0)
                {
                    if (!s || s->epoch != q.session_epoch)
                    {
                        q = QueuedTx{};
                        continue;
                    }
                }
                if (!s || !s->client || !s->client->connected())
                {
                    q.next_retry_ms = now + kTxRetryMs;
                    continue;
                }
                if ((int32_t)(now - s->tx_ready_ms) < 0)
                {
                    q.next_retry_ms = s->tx_ready_ms;
                    continue;
                }
                const TxResult tx = sendToNowLocked_(q.node_id, q.type, q.payload, q.len);
                if (tx == TxResult::Sent)
                {
                    q = QueuedTx{};
                    _rr_cursor = (uint32_t)((idx + 1) % _queued_tx.size());
                    continue;
                }
                if (tx == TxResult::Warmup || tx == TxResult::Offline)
                {
                    q.next_retry_ms = now + kTxRetryMs;
                    continue;
                }
                ++q.attempts;
                ++_tx_stats.retries;
                if (q.attempts >= kTxMaxAttempts)
                {
                    String feature_name;
                    String action_name;
                    describeStackTxPayload_(q.type, q.payload, q.len, feature_name, action_name);
                    if (_log)
                        _log->warn(F("STACK"), F("Master tx dropped: node_id: 0x%08lX type: %u feature: %s action: %s"),
                                   (unsigned long)q.node_id, (unsigned)q.type,
                                   feature_name.length() ? feature_name.c_str() : "-",
                                   action_name.length() ? action_name.c_str() : "-");
                    ++_tx_stats.dropped;
                    q = QueuedTx{};
                    _rr_cursor = (uint32_t)((idx + 1) % _queued_tx.size());
                    continue;
                }
                if (tx == TxResult::Busy || tx == TxResult::ShortWrite)
                    q.next_retry_ms = s->tx_ready_ms;
                else
                    q.next_retry_ms = now + kTxRetryMs;
                _rr_cursor = (uint32_t)((idx + 1) % _queued_tx.size());
            }
            const uint32_t prune_now = now;
            for (auto &slot : _sessions)
            {
                if (!slot.used)
                    continue;
                Session &s = slot.data;
                if (!s.client)
                    continue;
                const uint32_t silent_ms = (uint32_t)(prune_now - s.last_seen_ms);
                if (_session_silence_timeout_ms == 0 || silent_ms <= _session_silence_timeout_ms)
                    continue;
                const uint32_t node_id = s.has_id ? s.node_id : 0;
                if (node_id != 0 && event_count < MAX_SESSIONS)
                {
                    events[event_count].node_id = node_id;
                    events[event_count].online = false;
                    ++event_count;
                }
                removeSessionLocked_(&s, true);
            }
            event_cb = _event_cb;
            event_ctx = _event_ctx;
        }
        for (size_t i = 0; i < event_count; ++i)
        {
            if (event_cb && events[i].node_id != 0)
                event_cb(event_ctx, events[i].node_id, events[i].online);
        }
    }

    uint32_t nodeIdFromFrame_(Session *session, const StackFrame &frame)
    {
        if (frame.type != (uint8_t)StackMsgType::Hello)
            return session && session->has_id ? session->node_id : 0;
        StackHello hello{};
        if (!StackHello::decode(frame.payload, frame.payload_len, hello))
            return 0;
        return hello.node_id;
    }

    uint8_t queueDepthForNodeLocked_(uint32_t node_id) const
    {
        uint8_t depth = 0;
        for (const auto &q : _queued_tx)
        {
            if (!q.used || q.node_id != node_id)
                continue;
            if (depth < 0xFF)
                ++depth;
        }
        return depth;
    }

    String queueSummaryForNodeLocked_(uint32_t node_id, uint8_t limit) const
    {
        String out;
        uint8_t count = 0;
        for (const auto &q : _queued_tx)
        {
            if (!q.used || q.node_id != node_id)
                continue;
            String feature_name;
            String action_name;
            describeStackTxPayload_(q.type, q.payload, q.len, feature_name, action_name);
            if (out.length())
                out += "; ";
            out += "t:";
            out += String((unsigned)q.type);
            out += " f:";
            out += feature_name.length() ? feature_name : "-";
            out += " a:";
            out += action_name.length() ? action_name : "-";
            out += " n:";
            out += String((unsigned)q.attempts);
            ++count;
            if (count >= limit)
                break;
        }
        return out;
    }

    Session *allocSession_()
    {
        for (auto &slot : _sessions)
            if (!slot.used)
            {
                slot.used = true;
                slot.data = Session{};
                return &slot.data;
            }
        return nullptr;
    }

    void purgeQueuedTxLocked_(uint32_t node_id, uint32_t session_epoch)
    {
        if (node_id == 0)
            return;
        for (auto &q : _queued_tx)
        {
            if (!q.used || q.node_id != node_id)
                continue;
            if (session_epoch != 0 && q.session_epoch != 0 && q.session_epoch != session_epoch)
                continue;
            q = QueuedTx{};
        }
    }

    void removeSessionLocked_(Session *s, bool close_client)
    {
        if (!s)
            return;
        for (auto &slot : _sessions)
            if (&slot.data == s)
            {
                if (slot.data.closing)
                    return;
                StackTransportConnection *client = slot.data.client;
                const uint32_t node_id = slot.data.has_id ? slot.data.node_id : 0;
                const uint32_t epoch = slot.data.epoch;
                purgeQueuedTxLocked_(node_id, epoch);
                slot.data.client = nullptr;
                slot.data.closing = true;
                if (client)
                {
                    if (close_client && client->connected())
                        client->close(true);
                    slot.data.retired_client = client;
                }
                if (slot.data.inflight_ops == 0)
                    finalizeSessionLocked_(&slot.data);
                return;
            }
    }

    void finalizeSessionLocked_(Session *s)
    {
        if (!s)
            return;
        for (auto &slot : _sessions)
            if (&slot.data == s)
            {
                StackTransportConnection *client = slot.data.retired_client;
                slot.used = false;
                slot.data = Session{};
                if (client)
                    client->destroy();
                return;
            }
    }

    void logTxStats_()
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
            st.queue_full == _last_logged_tx_stats.queue_full &&
            st.session_resets == _last_logged_tx_stats.session_resets)
            return;
        _last_logged_tx_stats = st;
        _log->debug(F("STACK"),
                    F("Master tx stats: depth: %u queued: %lu retries: %lu coalesced: %lu dropped: %lu full: %lu resets: %lu"),
                    (unsigned)st.depth,
                    (unsigned long)st.queued,
                    (unsigned long)st.retries,
                    (unsigned long)st.coalesced,
                    (unsigned long)st.dropped,
                    (unsigned long)st.queue_full,
                    (unsigned long)st.session_resets);
    }
};

