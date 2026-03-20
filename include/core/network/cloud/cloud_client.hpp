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

#include "controllers/avr_controller.hpp"
#include "controllers/leak_controller.hpp"
#include "controllers/watering_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "core/network/cloud/cloud_transport.hpp"
#include "core/network/cloud/cloud_ws_transport.hpp"
#include "core/rules_controller.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "utils/users_registry.hpp"

class Logger;
class Controllers;
class SocketController;
class PlcControl;
class WifiManager;
class RTC;
class GsmModem;
class StackMaster;
class StackCache;
class ConfigsManagerIface;
class CloudClient
{
public:
    struct Config
    {
        String host;
        uint16_t port = 0;
        String path = "/";
        bool use_ssl = false;
        uint32_t reconnect_ms = 2000;
        CloudTransportKind transport = CloudTransportKind::WebSocket;
    };

    CloudClient(Logger &log, Controllers &controllers, PlcControl &plc, WifiManager &wifi, RTC &rtc);

    void setGsm(GsmModem *gsm);
    void setStackMaster(StackMaster *master);
    void setStackCache(StackCache *cache);
    void setConfigsManager(ConfigsManagerIface *cfg);
    void setUsersRegistry(UsersRegistry *users);
    void setRulesController(RulesController *rules);
    void setTransport(CloudTransport &transport);
    void useDefaultTransport();
    void bindControllerCallbacks();
    void bindRuleCallbacks();
    bool publishEvent(const String &kind, const String &reason, const String &data_json = String());
    bool publishScopedEvent(const String &unit, uint32_t node_id,
                            const String &kind, const String &reason,
                            const String &data_json = String());

    void setEnabled(bool enabled);
    bool enabled() const;
    bool isConnected() const;
    void disconnect();

    void setApiKey(const String &key);
    void setFirmwareVersion(const String &ver);
    void setAutoEventIntervalMs(uint32_t ms);

    void begin(const Config &cfg);

    void loop();
    uint8_t txFailStreak() const { return _tx_fail_streak; }
    uint32_t lastTxFailMs() const { return _last_tx_fail_ms; }

private:
    static constexpr uint8_t kProtoVersion = 1;
    static constexpr uint16_t kStackCmdIdBase = 0x8000;
    static constexpr uint16_t kStackCmdIdMax = 0xFFFE;
    static constexpr uint8_t kMaxPending = 6;
    static constexpr uint8_t kMaxStackCmds = 32;
    static constexpr uint8_t kMaxQueuedStackCmds = 24;
    static constexpr uint8_t kMaxQueuedEvents = 64;
    static constexpr uint8_t kMaxInFlightStackCmdsPerNode = 4;
    static constexpr uint32_t kStackTimeoutMs = 1500;
    static constexpr uint32_t kStackCmdRetryMs = 80;
    static constexpr uint8_t kStackCmdMaxAttempts = 8;
    static constexpr size_t kWsDocCapacity = 8192;
    static constexpr uint32_t kHelloRetryMs = 10000;
    static constexpr uint32_t kHelloSessionTimeoutMs = 60000;
    static constexpr uint32_t kWsSilentTimeoutMs = 180000;
    static constexpr uint32_t kWsReinitDisconnectedMs = 60000;
    static constexpr uint32_t kTxFailWindowMs = 90000;
    static constexpr uint32_t kTxRxAliveWindowMs = 1500;
    static constexpr uint32_t kTxDeadWhileRxAliveMs = 5000;
    static constexpr uint32_t kSnapshotLockTimeoutMs = 250;
    static constexpr uint32_t kSnapshotWarnIntervalMs = 5000;
    static constexpr uint32_t kFastReconnectMs = 2000;

    enum class StackPart : uint8_t
    {
        None = 0,
        SystemInfo,
        PlcStatus,
        FanStatus,
        RtcTime,
        Sockets,
        Lights,
        Meteo,
        Thermo,
        Tanks,
        Septic,
        Watering,
        SecurityStatus,
        SecuritySensors,
        Groups,
        Ring,
        Avr,
        Leak
    };

    struct PendingStackCmd
    {
        bool used = false;
        uint16_t cmd_id = 0;
        uint8_t pending_idx = 0;
        StackPart part = StackPart::None;
        bool started = false;
    };

    struct PendingRequest
    {
        bool used = false;
        String ws_id;
        uint32_t node_id = 0;
        uint32_t deadline_ms = 0;
        uint32_t pending_mask = 0;
        bool want_system = false;
        bool want_controllers = false;
        DynamicJsonDocument *doc = nullptr;
    };

    struct ActorInfo
    {
        String uid;
        String username;
        String plc_username;
        String source;
        String session_id;
        String resolved_user;
        uint8_t resolved_idx = 0xFF;
        bool is_admin = false;
    };
    struct QueuedEvent
    {
        bool used = false;
        String unit;
        uint32_t node_id = 0;
        String kind;
        String reason;
        String data_json;
    };

    struct QueuedStackCmd
    {
        bool used = false;
        uint32_t node_id = 0;
        StackMsgType type = StackMsgType::CmdGet;
        StackFeature feature = StackFeature::System;
        StackPart part = StackPart::None;
        uint8_t pending_idx = 0xFF;
        uint8_t attempts = 0;
        uint32_t next_retry_ms = 0;
        String action;
        String params_json;
    };

    Logger &_log;
    Controllers &_controllers;
    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    GsmModem *_gsm = nullptr;
    StackMaster *_stack_master = nullptr;
    StackCache *_stack_cache = nullptr;
    ConfigsManagerIface *_configs = nullptr;
    UsersRegistry *_users = nullptr;
    RulesController *_rules = nullptr;

    Config _cfg;
    CloudWsTransport _default_transport;
    CloudTransport *_transport = nullptr;
    String _api_key;
    String _fw_version;
    String _session_id;
    uint32_t _event_interval_ms = 0;
    uint32_t _next_event_ms = 0;
    bool _enabled = true;
    uint32_t _last_rx_ms = 0;
    uint32_t _last_connect_ms = 0;
    uint32_t _last_hello_ms = 0;
    uint32_t _last_disconnect_ms = 0;
    bool _disconnect_reported = false;
    uint32_t _reconnect_backoff_until_ms = 0;
    uint8_t _reconnect_fail_streak = 0;
    uint8_t _tx_fail_streak = 0;
    uint32_t _last_tx_fail_ms = 0;
    uint32_t _last_tx_ok_ms = 0;
    PendingRequest _pending[kMaxPending] = {};
    PendingStackCmd _stack_cmds[kMaxStackCmds] = {};
    uint16_t _next_stack_cmd_id = kStackCmdIdBase;
    QueuedStackCmd _stack_send_queue[kMaxQueuedStackCmds] = {};
    QueuedEvent _event_queue[kMaxQueuedEvents] = {};
    uint8_t _event_head = 0;
    uint8_t _event_count = 0;

    void handleMessage_(const uint8_t *payload, size_t len);

    static void onTransportMessage_(void *ctx, const uint8_t *payload, size_t len);
    static void onTransportEvent_(void *ctx, CloudTransport::Event event, const uint8_t *payload, size_t len);
    void handleTransportEvent_(CloudTransport::Event event, const uint8_t *payload, size_t len);

    void sendHello_();

    void maintainConnectionHealth_();
    void flushQueuedStackCmds_();
    void flushQueuedEvents_();
    bool enqueueEvent_(const String &kind, const String &reason, const String &data_json,
                       const String &unit = String(), uint32_t node_id = 0);
    bool sendEvent_(const String &kind, const String &reason, const String &data_json,
                    const String &unit = String(), uint32_t node_id = 0);
    bool tryCoalesceQueuedEvent_(const String &kind, const String &reason, const String &data_json,
                                 const String &unit, uint32_t node_id);
    static bool isCoalescibleStateEvent_(const String &kind);
    static uint32_t eventItemId_(const String &data_json);
    int findDroppableQueuedEventIndex_() const;
    bool dropQueuedEventAt_(uint8_t idx);
    void logEvent_(const __FlashStringHelper *stage, const String &kind, const String &reason,
                   const String &unit = String(), uint32_t node_id = 0);

    void sendPong_(const String &reply_to, JsonVariantConst payload);

    void handleGet_(const String &req_id, JsonDocument &doc);

    void handleGetLocal_(const String &req_id, JsonArrayConst what);

    void handleGetStack_(const String &req_id, uint32_t node_id, JsonArrayConst what);

    void handleCmd_(const String &req_id, JsonDocument &doc);

    void handleCmdLocal_(const String &req_id, const String &ctrl, const String &action,
                         JsonObjectConst args, const ActorInfo &actor);

    void handleCmdStack_(const String &req_id, uint32_t node_id,
                         const String &ctrl, const String &action, JsonObjectConst args, const ActorInfo &actor);

    bool handleCmdSockets_(SocketController &s, const String &action, JsonObjectConst args, bool lights,
                           const ActorInfo &actor);

    bool handleCmdThermo_(const String &action, JsonObjectConst args);

    bool handleCmdTanks_(const String &action, JsonObjectConst args);

    bool handleCmdSeptic_(const String &action, JsonObjectConst args);

    bool handleCmdWatering_(const String &action, JsonObjectConst args);

    bool handleCmdSecurity_(const String &action, JsonObjectConst args, const ActorInfo &actor);

    bool handleCmdRing_(const String &action, JsonObjectConst args);

    bool handleCmdAvr_(const String &action, JsonObjectConst args);

    bool handleCmdLeak_(const String &action, JsonObjectConst args);

    void sendAck_(const String &reply_to, bool ok, const char *error);

    void sendError_(const String &reply_to, const char *msg);

    void finalizePending_(PendingRequest *p, bool ok, const char *err);

    void scheduleStackSystem_(PendingRequest *p);

    void scheduleStackControllers_(PendingRequest *p);

    bool sendStackCmd_(uint32_t node_id, StackMsgType type, StackFeature feature,
                       const char *action, PendingRequest *p = nullptr);

    bool sendStackCmd_(uint32_t node_id, StackMsgType type, StackFeature feature,
                       const char *action, const DynamicJsonDocument &params,
                       PendingRequest *p = nullptr);

    bool sendStackCmdSimple_(uint32_t node_id, StackMsgType type, StackFeature feature,
                             const char *action, const DynamicJsonDocument &params,
                             PendingRequest *p = nullptr);
    bool enqueueStackCmdRetry_(uint32_t node_id, StackMsgType type, StackFeature feature, StackPart part,
                               PendingRequest *p, const char *action, const String &params_json);
    bool trySendStackCmdNow_(uint32_t node_id, StackMsgType type, StackFeature feature, StackPart part,
                             PendingRequest *p, const char *action, const String &params_json);
    void onStackCmdScheduleFailed_(PendingRequest *p, StackPart part, const char *action);

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame);

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame);

    void applyStackPart_(PendingRequest *p, StackPart part, bool ok, JsonObject data, bool first_part, bool done);

    void applyStackSystem_(JsonObject root, StackPart part, JsonObject data, uint32_t node_id);

    void applyStackControllers_(JsonObject root, StackPart part, JsonObject data, bool first_part);

    static void copyItems_(JsonObject &dst_parent, const char *key, JsonArrayConst items, bool reset);
    static void onSocketEvent_(void *ctx, bool lights, uint8_t id, const String &name, bool state_on,
                               const char *source);
    static void onMeteoAlarmEvent_(void *ctx, uint32_t node_id, uint8_t sensor_id, bool alarm);
    static void onThermoEvent_(void *ctx, uint8_t id, const String &name, bool power_on, const char *source);
    static void onTankEvent_(void *ctx, uint8_t tank_id, const String &name, bool empty);
    static void onSepticEvent_(void *ctx, uint8_t septic_id, const String &name, bool is_alarm);
    static void onSecurityArmEvent_(void *ctx, bool armed);
    static void onSecurityAlarmEvent_(void *ctx, bool alarm_on);
    static void onSecurityClearEvent_(void *ctx);
    static void onSecurityDetectEvent_(void *ctx, uint8_t sensor_id, const String &name, bool silent);
    static void onWateringEvent_(void *ctx, WateringController::Event ev,
                                 const WateringController::RuleConfig &cfg,
                                 const WateringController::RuleState &st);
    static void onRingEvent_(void *ctx, bool on);
    static void onAvrEvent_(void *ctx, AvrController::Event ev, const AvrController::State &st,
                            const char *message);
    static void onLeakEvent_(void *ctx, LeakController::Event ev, uint8_t id, const String &name,
                             bool wet, bool alarm_latched);
    static void onRuleTriggered_(void *ctx, const RulesController::Rule &rule);

    void fillSystemInfo_(JsonObject out);
    void fillAuthzInfo_(JsonObject out);

    void fillControllersInfo_(JsonObject out);

    void fillGroups_(JsonArray out);

    void fillSockets_(JsonArray out, bool lights);

    void fillMeteo_(JsonArray out);

    void fillThermo_(JsonArray out);

    void fillTanks_(JsonArray out);

    void fillSeptic_(JsonArray out);

    void fillWatering_(JsonArray out);

    void fillSecurity_(JsonObject out);

    void fillRing_(JsonObject out);

    void fillAvr_(JsonObject out);

    void fillLeak_(JsonArray out);

    void fillStackInfo_(JsonObject out);
    bool fillStackCachedSystem_(JsonObject out, uint32_t node_id);
    bool fillStackCachedControllers_(JsonObject out, uint32_t node_id);

    void maybeSendPeriodicEvent_();

    void handlePendingTimeouts_();

    PendingRequest *allocPending_(const String &ws_id, uint32_t node_id);

    void freePending_(PendingRequest *p);

    bool parseActor_(JsonObjectConst payload, ActorInfo &out) const;
    bool resolveActor_(ActorInfo &actor) const;
    uint8_t aclUnitByNodeId_(uint32_t node_id) const;
    static bool aclControllerByName_(const String &ctrl, UsersRegistry::AclController &out);
    static uint16_t aclItemIdForCmd_(const String &ctrl, const String &action, JsonObjectConst args);
    bool aclCanControl_(const ActorInfo &actor, const String &ctrl, const String &action,
                        JsonObjectConst args, uint32_t node_id) const;

    void clearPending_();
    void clearQueuedStackCmds_();
    void clearQueuedStackCmdsForPending_(uint8_t pending_idx);
    bool hasQueuedStackCmdsForPending_(uint8_t pending_idx) const;
    uint8_t countInflightStackCmdsForNode_(uint32_t node_id) const;
    bool hasFreeStackCmdSlot_() const;
    void extendPendingDeadline_(PendingRequest *p);

    void ensurePendingDoc_(PendingRequest *p);
    uint8_t pendingIndex_(PendingRequest *p) const;

    PendingRequest *findPendingByNode_(uint32_t node_id);

    bool registerStackCmd_(uint16_t cmd_id, PendingRequest *p, StackPart part);

    PendingStackCmd *findStackCmd_(uint16_t cmd_id);

    uint16_t nextStackCmdId_();

    StackPart partFrom_(StackFeature feature, const char *action) const;

    static uint32_t maskFor_(StackPart p);

    static bool hasWhat_(JsonArrayConst what, const char *name);
    const char *transportName_() const;

    const char *stackRoleName_() const;

    bool isStackMaster_() const;

    uint32_t deviceId_() const;

    String localIp_() const;

    String stackNodeName_(uint32_t node_id) const;
    String eventSourceName_(const String &unit, uint32_t node_id) const;

    static ThermoController::Mode parseThermoMode_(const String &mode);

    String nextWsId_();

    void sendJson_(JsonDocument &doc);
};
