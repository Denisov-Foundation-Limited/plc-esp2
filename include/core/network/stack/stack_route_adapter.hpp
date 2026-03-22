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

#include "core/network/stack/stack_binary_protocol.hpp"
#include "core/network/stack/stack_json_protocol.hpp"
#include "utils/rtos_lock.hpp"

class Logger;
class StackMasterServer;
class StackRs485Server;
class StackSlaveClient;
class StackDeviceRegistry;

class StackRouteAdapter
{
public:
    // Lock order contract for stack core:
    // 1. StackRouteAdapter
    // 2. StackMasterRouter
    // 3. StackTransport / StackDeviceRegistry
    // Do not invert this order in callbacks or helper paths.
    enum class ExchangeStatus : uint8_t
    {
        Queued = 0,
        Retried,
        UplinkAcked,
        Delivered,
        LocalHandled,
        Expired,
        Dropped
    };

    struct ExchangeRecord
    {
        bool used = false;
        bool is_binary = false;
        uint32_t queue_id = 0;
        uint32_t source_node = 0;
        uint32_t target_node = 0;
        uint32_t enqueued_ms = 0;
        uint32_t last_sync_ms = 0;
        uint32_t expire_ms = 0;
        uint8_t delivery_count = 0;
        uint8_t retry_count = 0;
        StackTransport::RouteMeta meta{};
        char feature[32] = {};
        char action[32] = {};
        String payload;
    };

    struct ExchangeHistoryRecord
    {
        bool used = false;
        bool is_binary = false;
        uint32_t queue_id = 0;
        uint32_t source_node = 0;
        uint32_t target_node = 0;
        uint32_t enqueued_ms = 0;
        uint32_t event_ms = 0;
        uint8_t retry_count = 0;
        ExchangeStatus status = ExchangeStatus::Queued;
        char feature[32] = {};
        char action[32] = {};
    };

    struct ExchangeDiagnostics
    {
        uint16_t slave_outbox_used = 0;
        uint16_t master_inbox_used = 0;
        uint16_t notify_outbox_used = 0;
        uint16_t history_count = 0;
        uint32_t lock_held_ms = 0;
        uint32_t queued = 0;
        uint32_t retried = 0;
        uint32_t uplink_acked = 0;
        uint32_t delivered = 0;
        uint32_t local_handled = 0;
        uint32_t expired = 0;
        uint32_t dropped = 0;
        uint32_t binary_queued = 0;
    };

    struct NotificationRecord
    {
        bool used = false;
        uint32_t notification_id = 0;
        uint32_t source_node = 0;
        uint32_t ts_ms = 0;
        uint16_t repeat_count = 1;
        uint8_t priority = 0;
        char level[16] = {};
        char feature[32] = {};
        char code[32] = {};
        String message;
        String payload;
    };

    enum class Mode : uint8_t
    {
        Json = 0,
        Binary,
        Auto
    };

    enum class ExchangePolicy : uint8_t
    {
        Auto = 0,
        Direct,
        Poll
    };

    enum class PayloadMode : uint8_t
    {
        Auto = 0,
        Json,
        Binary
    };

    enum class RoutePlane : uint8_t
    {
        Control = 0,
        Data
    };

    using JsonRouteHandler = void (*)(void *ctx, uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    using BinaryRouteHandler = void (*)(void *ctx, uint32_t source_node, const StackBinaryProtocol::RouteFrame &route);
    using NotificationHandler = void (*)(void *ctx, uint32_t source_node, const StackJsonProtocol::NotifyMessage &notify);

    explicit StackRouteAdapter(Logger &log);

    void bindMaster(StackMasterServer &master);
    void bindRs485Master(StackRs485Server &master);
    void bindSlave(StackSlaveClient &slave);
    void setLocalNodeId(uint32_t node_id);
    uint32_t localNodeId() const;
    void setExchangePolicy(ExchangePolicy policy);
    ExchangePolicy exchangePolicy() const;
    void setPayloadMode(PayloadMode mode);
    PayloadMode payloadMode() const;
    void setRuntimeBindings(bool master_ws_active, bool master_rs485_active, bool slave_active);
    void loop();

    void setJsonRouteHandler(JsonRouteHandler cb, void *ctx);
    void setBinaryRouteHandler(BinaryRouteHandler cb, void *ctx);
    void setNotificationHandler(NotificationHandler cb, void *ctx);

    static StackTransport::RouteMeta makeEventMeta();
    static StackTransport::RouteMeta makeRequestMeta(bool expect_response = true, uint32_t request_id = 0);
    static StackTransport::RouteMeta makeResponseMeta(uint32_t reply_to, uint32_t request_id = 0);

    bool sendRoute(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload = nullptr,
                   Mode mode = Mode::Auto, const StackTransport::RouteMeta *meta = nullptr);
    bool sendRouteJson(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload = nullptr,
                       const StackTransport::RouteMeta *meta = nullptr);
    bool sendRouteBinary(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload = nullptr,
                         size_t payload_size = 0, const StackTransport::RouteMeta *meta = nullptr);
    bool sendEvent(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload = nullptr,
                   Mode mode = Mode::Auto);
    bool sendRequest(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload = nullptr,
                     Mode mode = Mode::Auto, bool expect_response = true, uint32_t request_id = 0);
    bool sendResponse(uint32_t target_node, const char *feature, const char *action, uint32_t reply_to,
                      const JsonDocument *payload = nullptr, Mode mode = Mode::Auto, uint32_t request_id = 0);
    bool sendEventBinary(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload = nullptr,
                         size_t payload_size = 0);
    bool sendRequestBinary(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload = nullptr,
                           size_t payload_size = 0, bool expect_response = true, uint32_t request_id = 0);
    bool sendResponseBinary(uint32_t target_node, const char *feature, const char *action, uint32_t reply_to,
                            const uint8_t *payload = nullptr, size_t payload_size = 0, uint32_t request_id = 0);
    bool sendNotify(const char *level, const char *feature, const char *code, const char *message = nullptr,
                    const JsonDocument *payload = nullptr);
    size_t exchangeHistoryCount() const;
    bool exchangeHistoryAt(size_t idx, ExchangeHistoryRecord &out) const;
    ExchangeDiagnostics exchangeDiagnostics() const;
    size_t notificationHistoryCount() const;
    bool notificationHistoryAt(size_t idx, NotificationRecord &out) const;

private:
    static constexpr const char *kXchgFeature = "stack_xchg";
    static constexpr const char *kNotifyFeature = "stack_notify";
    static constexpr size_t kExchangeSlaveOutboxCap = 16;
    static constexpr size_t kExchangeMasterInboxCap = 32;
    static constexpr size_t kExchangeHistoryCap = 48;
    static constexpr size_t kNotifyOutboxCap = 16;
    static constexpr size_t kNotifyHistoryCap = 32;
    static constexpr uint32_t kExchangeSyncTimeoutMs = 1500u;
    static constexpr uint32_t kExchangePollIntervalMs = 150u;
    static constexpr uint32_t kExchangeRetryIntervalMs = 600u;
    static constexpr uint32_t kExchangeExpireMs = 8000u;
    static constexpr uint8_t kExchangeMaxRetries = 3;

    enum class NotifySyncPhase : uint8_t
    {
        Idle = 0,
        PullWait,
        AckWait
    };

    enum class ExchangeSyncPhase : uint8_t
    {
        Idle = 0,
        Wait
    };

    struct NotifySyncState
    {
        NotifySyncPhase phase = NotifySyncPhase::Idle;
        uint32_t node_id = 0;
        uint32_t request_id = 0;
        uint32_t ack_upto_id = 0;
        uint32_t deadline_ms = 0;
    };

    struct ExchangeSyncState
    {
        ExchangeSyncPhase phase = ExchangeSyncPhase::Idle;
        uint32_t node_id = 0;
        uint32_t request_id = 0;
        uint32_t deadline_ms = 0;
    };

    struct ExchangeNodeState
    {
        bool used = false;
        uint32_t node_id = 0;
        uint32_t ack_out_upto = 0;
    };

    struct DeferredJsonInvoke
    {
        JsonRouteHandler cb = nullptr;
        void *ctx = nullptr;
        uint32_t source_node = 0;
        StackJsonProtocol::RouteMessage route{};
    };

    struct DeferredBinaryInvoke
    {
        BinaryRouteHandler cb = nullptr;
        void *ctx = nullptr;
        uint32_t source_node = 0;
        StackBinaryProtocol::RouteFrame route{};
        String payload_storage;
    };

    struct DeferredNotifyInvoke
    {
        NotificationHandler cb = nullptr;
        void *ctx = nullptr;
        uint32_t source_node = 0;
        StackJsonProtocol::NotifyMessage notify{};
    };

    Logger &_log;
    StackMasterServer *_master = nullptr;
    StackRs485Server *_rs485_master = nullptr;
    StackSlaveClient *_slave = nullptr;
    uint32_t _local_node_id = 0;
    ExchangePolicy _exchange_policy = ExchangePolicy::Auto;
    PayloadMode _payload_mode = PayloadMode::Auto;
    bool _master_ws_active = false;
    bool _master_rs485_active = false;
    bool _slave_active = false;
    JsonRouteHandler _json_cb = nullptr;
    void *_json_ctx = nullptr;
    BinaryRouteHandler _binary_cb = nullptr;
    void *_binary_ctx = nullptr;
    NotificationHandler _notify_cb = nullptr;
    void *_notify_ctx = nullptr;
    ExchangeRecord _exchange_slave_outbox[kExchangeSlaveOutboxCap];
    ExchangeRecord _exchange_master_inbox[kExchangeMasterInboxCap];
    ExchangeHistoryRecord _exchange_history[kExchangeHistoryCap];
    uint8_t _exchange_history_head = 0;
    uint8_t _exchange_history_count = 0;
    ExchangeDiagnostics _exchange_diag{};
    ExchangeNodeState _exchange_nodes[StackDeviceRegistry::kMaxDevices];
    uint32_t _next_exchange_id = 1;
    uint32_t _exchange_last_poll_ms = 0;
    uint8_t _exchange_poll_cursor = 0;
    ExchangeSyncState _exchange_sync{};
    NotificationRecord _notify_outbox[kNotifyOutboxCap];
    NotificationRecord _notify_history[kNotifyHistoryCap];
    uint8_t _notify_history_head = 0;
    uint8_t _notify_history_count = 0;
    uint32_t _next_notify_id = 1;
    uint32_t _next_internal_request_id = 1;
    uint32_t _notify_last_poll_ms = 0;
    uint8_t _notify_poll_cursor = 0;
    NotifySyncState _notify_sync{};
    mutable RtosRecursiveLock _lock;
    mutable ExchangeHistoryRecord _exchange_history_snapshot{};
    mutable NotificationRecord _notification_history_snapshot{};

    static void onMasterJsonRoute_(void *ctx, const StackDeviceRegistry::DeviceInfo &device,
                                   const StackJsonProtocol::RouteMessage &route);
    static void onMasterBinaryRoute_(void *ctx, const StackDeviceRegistry::DeviceInfo &device,
                                     const StackBinaryProtocol::RouteFrame &route);
    static void onSlaveJsonRoute_(void *ctx, const StackJsonProtocol::RouteMessage &route);
    static void onSlaveBinaryRoute_(void *ctx, const StackBinaryProtocol::RouteFrame &route);
    static void onMasterNotify_(void *ctx, const StackDeviceRegistry::DeviceInfo &device,
                                const StackJsonProtocol::NotifyMessage &notify);
    static void onSlaveNotify_(void *ctx, const StackJsonProtocol::NotifyMessage &notify);

    void handleMasterJsonRoute_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    void handleMasterBinaryRoute_(uint32_t source_node, const StackBinaryProtocol::RouteFrame &route);
    void handleSlaveJsonRoute_(const StackJsonProtocol::RouteMessage &route);
    void handleSlaveBinaryRoute_(const StackBinaryProtocol::RouteFrame &route);
    void handleMasterNotify_(uint32_t source_node, const StackJsonProtocol::NotifyMessage &notify);
    void handleSlaveNotify_(const StackJsonProtocol::NotifyMessage &notify);
    bool handleExchangeRoute_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    bool handleExchangeSyncRequest_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    bool handleExchangeSyncResponse_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    bool queueExchangeFromSlave_(uint32_t target_node, const char *feature, const char *action, const String &payload,
                                 const StackTransport::RouteMeta *meta, bool is_binary = false);
    bool enqueueMasterInbox_(uint32_t source_node, uint32_t target_node, const char *feature, const char *action,
                             const String &payload, const StackTransport::RouteMeta &meta, bool is_binary = false);
    bool appendExchangeRecord_(ExchangeRecord &entry, uint32_t source_node, uint32_t target_node, const char *feature,
                               const char *action, const String &payload, const StackTransport::RouteMeta &meta,
                               uint32_t queue_id, bool is_binary = false);
    void ackSlaveOutboxUpTo_(uint32_t upto_id);
    void ackMasterInboxUpTo_(uint32_t node_id, uint32_t upto_id);
    bool buildExchangeSyncRequestPayload_(uint32_t node_id, DynamicJsonDocument &doc);
    bool buildExchangeSyncResponsePayload_(DynamicJsonDocument &doc, uint32_t &ack_in_upto);
    bool tryStartExchangePoll_();
    void resetExchangeSync_();
    void expireMasterInbox_(uint32_t now_ms);
    ExchangeNodeState *exchangeNodeState_(uint32_t node_id, bool create);
    void routeOutboundExchange_(const ExchangeRecord &record, DeferredJsonInvoke &json_invoke, DeferredBinaryInvoke &binary_invoke);
    void emitExchangeToSlave_(const ExchangeRecord &record, DeferredJsonInvoke &json_invoke, DeferredBinaryInvoke &binary_invoke);
    void appendExchangeHistory_(const ExchangeRecord &record, ExchangeStatus status);
    void updateExchangeDiag_(ExchangeStatus status, bool is_binary);
    static const char *exchangeStatusText_(ExchangeStatus status);
    static void writeExchangeToJson_(JsonObject obj, const ExchangeRecord &record);
    static bool readExchangeFromJson_(JsonObjectConst obj, ExchangeRecord &record);
    static String encodeBase64_(const uint8_t *data, size_t len);
    static bool decodeBase64_(const String &in, String &out);
    bool handleNotificationRoute_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    bool handleNotificationPullRequest_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    bool handleNotificationAckRequest_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    bool handleNotificationPullResponse_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    bool handleNotificationAckResponse_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);
    bool queueNotification_(uint32_t source_node, const char *level, const char *feature, const char *code,
                            const char *message, const String *payload_json = nullptr);
    bool appendOutboxNotification_(NotificationRecord &entry, uint32_t source_node, const char *level, const char *feature,
                                   const char *code, const char *message, const String *payload_json);
    NotificationRecord *findOutboxDuplicate_(uint32_t source_node, const char *level, const char *feature, const char *code,
                                             const char *message, const String *payload_json);
    void appendHistory_(const NotificationRecord &record);
    static uint8_t notificationPriority_(const char *level);
    bool buildPullResponsePayload_(DynamicJsonDocument &doc, size_t limit, uint32_t &max_id) const;
    void ackOutboxUpTo_(uint32_t upto_id);
    bool tryStartNotificationPoll_();
    bool hasMasterBackend_() const;
    bool hasRs485PollBackend_() const;
    bool canSlavePushDirect_() const;
    static RoutePlane classifyPlane_(const char *feature, const char *action);
    Mode chooseMode_(const char *feature, const char *action, const JsonDocument *payload, Mode requested) const;
    uint32_t nextInternalRequestId_();
    void resetNotifySync_();
    bool isInternalNotificationRequest_(const StackJsonProtocol::RouteMessage &route) const;
    static bool copyText_(char *dst, size_t cap, const char *src);
    void emitNotification_(const NotificationRecord &record, DeferredNotifyInvoke *invoke = nullptr);
    static void invokeDeferredJson_(const DeferredJsonInvoke &invoke);
    static void invokeDeferredBinary_(const DeferredBinaryInvoke &invoke);
    static void invokeDeferredNotify_(const DeferredNotifyInvoke &invoke);

    bool sendMasterJson_(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload,
                         const StackTransport::RouteMeta *meta);
    bool sendMasterBinary_(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload,
                           size_t payload_size, const StackTransport::RouteMeta *meta);
    bool sendRs485Json_(uint32_t target_node, const char *feature, const char *action, const JsonDocument *payload,
                        const StackTransport::RouteMeta *meta);
    bool sendRs485Binary_(uint32_t target_node, const char *feature, const char *action, const uint8_t *payload,
                          size_t payload_size, const StackTransport::RouteMeta *meta);
};
