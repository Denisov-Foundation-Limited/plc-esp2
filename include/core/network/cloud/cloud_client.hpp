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
#include <WebSocketsClient.h>

#include "controllers/thermo_controller.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_protocol.hpp"

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
        uint32_t reconnect_ms = 5000;
    };

    CloudClient(Logger &log, Controllers &controllers, PlcControl &plc, WifiManager &wifi, RTC &rtc);

    void setGsm(GsmModem *gsm);
    void setStackMaster(StackMaster *master);
    void setStackCache(StackCache *cache);
    void setConfigsManager(ConfigsManagerIface *cfg);

    void setEnabled(bool enabled);
    bool enabled() const;
    bool isConnected() const;
    void disconnect();

    void setApiKey(const String &key);
    void setFirmwareVersion(const String &ver);
    void setAutoEventIntervalMs(uint32_t ms);

    void begin(const Config &cfg);

    void loop();

private:
    static constexpr uint8_t kProtoVersion = 1;
    static constexpr uint16_t kStackCmdIdBase = 0x8000;
    static constexpr uint16_t kStackCmdIdMax = 0xFFFE;
    static constexpr uint8_t kMaxPending = 6;
    static constexpr uint8_t kMaxStackCmds = 32;
    static constexpr uint32_t kStackTimeoutMs = 1500;
    static constexpr size_t kWsDocCapacity = 8192;
    static constexpr uint32_t kHelloRetryMs = 10000;
    static constexpr uint32_t kHelloSessionTimeoutMs = 60000;
    static constexpr uint32_t kWsSilentTimeoutMs = 180000;
    static constexpr uint32_t kWsReinitDisconnectedMs = 60000;

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

    Logger &_log;
    Controllers &_controllers;
    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    GsmModem *_gsm = nullptr;
    StackMaster *_stack_master = nullptr;
    StackCache *_stack_cache = nullptr;
    ConfigsManagerIface *_configs = nullptr;

    Config _cfg;
    WebSocketsClient _ws;
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

    PendingRequest _pending[kMaxPending] = {};
    PendingStackCmd _stack_cmds[kMaxStackCmds] = {};
    uint16_t _next_stack_cmd_id = kStackCmdIdBase;

    void onWsEvent_(WStype_t type, uint8_t *payload, size_t len);

    void handleMessage_(const uint8_t *payload, size_t len);

    void sendHello_();

    void maintainConnectionHealth_();

    void sendPong_(const String &reply_to, JsonVariantConst payload);

    void handleGet_(const String &req_id, JsonDocument &doc);

    void handleGetLocal_(const String &req_id, JsonArrayConst what);

    void handleGetStack_(const String &req_id, uint32_t node_id, JsonArrayConst what);

    void handleCmd_(const String &req_id, JsonDocument &doc);

    void handleCmdLocal_(const String &req_id, const String &ctrl, const String &action, JsonObjectConst args);

    void handleCmdStack_(const String &req_id, uint32_t node_id,
                         const String &ctrl, const String &action, JsonObjectConst args);

    bool handleCmdSockets_(SocketController &s, const String &action, JsonObjectConst args, bool lights);

    bool handleCmdThermo_(const String &action, JsonObjectConst args);

    bool handleCmdTanks_(const String &action, JsonObjectConst args);

    bool handleCmdSeptic_(const String &action, JsonObjectConst args);

    bool handleCmdWatering_(const String &action, JsonObjectConst args);

    bool handleCmdSecurity_(const String &action, JsonObjectConst args);

    bool handleCmdRing_(const String &action, JsonObjectConst args);

    bool handleCmdAvr_(const String &action, JsonObjectConst args);

    bool handleCmdLeak_(const String &action, JsonObjectConst args);

    void sendAck_(const String &reply_to, bool ok, const char *error);

    void sendError_(const String &reply_to, const char *msg);

    void finalizePending_(PendingRequest *p, bool ok, const char *err);

    void scheduleStackSystem_(PendingRequest *p);

    void scheduleStackControllers_(PendingRequest *p);

    void sendStackCmd_(uint32_t node_id, StackMsgType type, StackFeature feature,
                       const char *action);

    void sendStackCmd_(uint32_t node_id, StackMsgType type, StackFeature feature,
                       const char *action, const DynamicJsonDocument &params);

    void sendStackCmdSimple_(uint32_t node_id, StackMsgType type, StackFeature feature,
                             const char *action, const DynamicJsonDocument &params);

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame);

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame);

    void applyStackPart_(PendingRequest *p, StackPart part, bool ok, JsonObject data, bool first_part, bool done);

    void applyStackSystem_(JsonObject root, StackPart part, JsonObject data, uint32_t node_id);

    void applyStackControllers_(JsonObject root, StackPart part, JsonObject data, bool first_part);

    static void copyItems_(JsonObject &dst_parent, const char *key, JsonArrayConst items, bool reset);

    void fillSystemInfo_(JsonObject out);

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

    void clearPending_();

    void ensurePendingDoc_(PendingRequest *p);

    PendingRequest *findPendingByNode_(uint32_t node_id);

    void registerStackCmd_(uint16_t cmd_id, PendingRequest *p, StackPart part);

    PendingStackCmd *findStackCmd_(uint16_t cmd_id);

    uint16_t nextStackCmdId_();

    StackPart partFrom_(StackFeature feature, const char *action) const;

    static uint32_t maskFor_(StackPart p);

    static bool hasWhat_(JsonArrayConst what, const char *name);

    const char *stackRoleName_() const;

    bool isStackMaster_() const;

    uint32_t deviceId_() const;

    String localIp_() const;

    String stackNodeName_(uint32_t node_id) const;

    static ThermoController::Mode parseThermoMode_(const String &mode);

    String nextWsId_();

    void sendJson_(JsonDocument &doc);
};
