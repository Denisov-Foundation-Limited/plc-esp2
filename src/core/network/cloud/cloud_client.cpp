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

#include "core/network/cloud/cloud_client.hpp"

#include <WiFi.h>
#include <stdlib.h>
#include <string.h>
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"

#include "boards/board_profile.hpp"
#include "controllers/controllers.hpp"
#include "core/network/gsm_modem.hpp"
#include "core/network/network.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "core/rules_controller.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"
#include "utils/users_registry.hpp"

namespace
{
uint32_t parseNodeId_(JsonVariantConst v)
{
    if (v.is<uint32_t>())
        return v.as<uint32_t>();
    if (v.is<unsigned long>())
        return (uint32_t)v.as<unsigned long>();
    if (v.is<unsigned int>())
        return (uint32_t)v.as<unsigned int>();
    if (v.is<const char *>())
    {
        const char *s = v.as<const char *>();
        if (s && s[0])
            return (uint32_t)strtoul(s, nullptr, 10);
    }
    return 0;
}

uint32_t cloudBackoffMs_(uint8_t streak, uint32_t base_ms)
{
    const uint32_t base = (base_ms < 2000u) ? 2000u : base_ms;
    if (streak == 0)
        return base;
    const uint8_t shift = (streak > 3) ? 3 : streak;
    uint32_t out = base << shift;
    if (out > 120000u)
        out = 120000u;
    return out;
}

} // namespace

struct CloudClient::ScratchBuffer
{
    struct SocketSnapshotItem
    {
        bool valid = false;
        SocketController::SocketConfig cfg{};
        SocketController::SocketState st{};
    };

    struct MeteoSnapshotItem
    {
        bool valid = false;
        MeteoController::SensorConfig cfg{};
        MeteoController::SensorState st{};
    };

    struct ThermoSnapshotItem
    {
        bool valid = false;
        ThermoController::DeviceConfig cfg{};
        ThermoController::DeviceState st{};
    };

    struct ThermoSensorItem
    {
        bool found = false;
        String name;
        bool has_temp = false;
        float temp_c = 0.0f;
    };

    struct TankSnapshotItem
    {
        bool valid = false;
        TankController::TankConfig cfg{};
        TankController::TankState st{};
    };

    struct SepticSnapshotItem
    {
        bool valid = false;
        SepticController::SepticConfig cfg{};
        SepticController::SepticState st{};
    };

    struct WateringSnapshotItem
    {
        bool valid = false;
        WateringController::RuleConfig cfg{};
        WateringController::RuleState st{};
    };

    struct TankNameItem
    {
        bool valid = false;
        String name;
    };

    struct SecuritySnapshotItem
    {
        bool valid = false;
        SecurityController::SensorConfig cfg{};
        SecurityController::SensorState st{};
    };

    struct LeakSnapshotItem
    {
        bool valid = false;
        LeakController::ZoneConfig cfg{};
        LeakController::ZoneState st{};
    };

    SocketSnapshotItem sockets[SocketController::kSocketCount]{};
    MeteoSnapshotItem meteo[MeteoController::kSensorCount]{};
    ThermoSnapshotItem thermo[ThermoController::kDeviceCount]{};
    ThermoSensorItem thermo_sensors[MeteoController::kSensorCount + 1]{};
    TankSnapshotItem tanks[TankController::kTankCount]{};
    SepticSnapshotItem septic[SepticController::kSepticCount]{};
    WateringSnapshotItem watering[WateringController::kRuleCount]{};
    TankNameItem watering_tanks[TankController::kTankCount + 1]{};
    SecuritySnapshotItem security[SecurityController::kSensorCount]{};
    LeakSnapshotItem leak[LeakController::kZoneCount]{};
};

CloudClient::CloudClient(Logger &log, Controllers &controllers, PlcControl &plc, WifiManager &wifi, RTC &rtc)
    : _log(log),
      _controllers(controllers),
      _plc(plc),
      _wifi(wifi),
      _rtc(rtc)
{
    setTransport(_default_transport);
}

CloudClient::~CloudClient()
{
    releaseScratch_();
}
void CloudClient::setGsm(GsmModem *gsm)
{ _gsm = gsm; }
void CloudClient::setNetwork(Network *network)
{ _network = network; }
void CloudClient::setStackNodeNameProvider(StackNodeNameProvider cb, void *ctx)
{
    _stack_node_name_cb = cb;
    _stack_node_name_ctx = ctx;
}
void CloudClient::setConfigsManager(ConfigsManagerIface *cfg)
{ _configs = cfg; }
void CloudClient::setUsersRegistry(UsersRegistry *users)
{ _users = users; }
void CloudClient::setRulesController(RulesController *rules)
{ _rules = rules; }
void CloudClient::setTransport(CloudTransport &transport)
{
    _transport = &transport;
    _transport->setMessageHandler(&CloudClient::onTransportMessage_, this);
    _transport->setEventHandler(&CloudClient::onTransportEvent_, this);
}
void CloudClient::useDefaultTransport()
{
    setTransport(_default_transport);
}
void CloudClient::bindControllerCallbacks()
{
    _controllers.sockets().setEventHandler(&CloudClient::onSocketEvent_, this);
    _controllers.meteo().setAlarmHandlerSecondary(&CloudClient::onMeteoAlarmEvent_, this);
    _controllers.thermo().setEventHandler(&CloudClient::onThermoEvent_, this);
    _controllers.tanks().setDetectHandlerSecondary(&CloudClient::onTankEvent_, this);
    _controllers.septic().setDetectHandlerSecondary(&CloudClient::onSepticEvent_, this);
    _controllers.security().setArmStateHandlerSecondary(&CloudClient::onSecurityArmEvent_, this);
    _controllers.security().setAlarmStateHandlerSecondary(&CloudClient::onSecurityAlarmEvent_, this);
    _controllers.security().setClearDetectHandlerSecondary(&CloudClient::onSecurityClearEvent_, this);
    _controllers.security().setDetectHandlerSecondary(&CloudClient::onSecurityDetectEvent_, this);
    _controllers.watering().setEventHandlerSecondary(&CloudClient::onWateringEvent_, this);
    _controllers.ring().setHoldHandlerSecondary(&CloudClient::onRingEvent_, this);
    _controllers.avr().setEventHandler(&CloudClient::onAvrEvent_, this);
    _controllers.leak().setEventHandler(&CloudClient::onLeakEvent_, this);
}
void CloudClient::bindRuleCallbacks()
{
    if (_rules)
        _rules->setTriggerHandler(&CloudClient::onRuleTriggered_, this);
}

bool CloudClient::ensureScratch_() const
{
    if (_scratch)
        return true;
    const auto guard = _scratch_lock.guard();
    if (_scratch)
        return true;
    const size_t bytes = sizeof(ScratchBuffer);
    void *mem = nullptr;
#if defined(ESP32)
    if (psramFound())
        mem = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    if (!mem)
        mem = calloc(1, bytes);
    if (!mem)
        return false;
    memset(mem, 0, bytes);
    _scratch = static_cast<ScratchBuffer *>(mem);
    return true;
}

CloudClient::ScratchBuffer *CloudClient::scratch_() const
{
    return ensureScratch_() ? _scratch : nullptr;
}

void CloudClient::releaseScratch_()
{
    const auto guard = _scratch_lock.guard();
    if (_scratch)
    {
        free(_scratch);
        _scratch = nullptr;
    }
}
bool CloudClient::publishEvent(const String &kind, const String &reason, const String &data_json)
{
    return enqueueEvent_(kind, reason, data_json);
}
bool CloudClient::publishScopedEvent(const String &unit, uint32_t node_id,
                                     const String &kind, const String &reason,
                                     const String &data_json)
{
    return enqueueEvent_(kind, reason, data_json, unit, node_id);
}
void CloudClient::setEnabled(bool enabled)
{
    if (enabled == _enabled)
        return;
    _enabled = enabled;
    if (!_enabled)
    {
        _log.info(F("CLOUD"), F("Disabled"));
        if (_transport)
            _transport->disconnect();
        _session_id = "";
        clearPending_();
    }
    else
    {
        _log.info(F("CLOUD"), F("Enabled"));
    }
}
bool CloudClient::enabled() const
{ return _enabled; }
bool CloudClient::isConnected() const
{ return _transport ? _transport->isConnected() : false; }
void CloudClient::disconnect()
{
    if (_transport)
        _transport->disconnect();
    _session_id = "";
    clearPending_();
}
void CloudClient::setApiKey(const String &key)
{ _api_key = key; }
void CloudClient::setFirmwareVersion(const String &ver)
{ _fw_version = ver; }
void CloudClient::setAutoEventIntervalMs(uint32_t ms)
{ _event_interval_ms = ms; }
void CloudClient::begin(const CloudClient::Config &cfg)
{
    if (!_enabled)
        return;
    if (!_transport)
        return;
    _transport->disconnect();
    _session_id = "";
    _last_connect_ms = 0;
    _last_rx_ms = 0;
    _last_hello_ms = 0;
    clearPending_();
    _cfg = cfg;
    if (_cfg.path.length() == 0)
        _cfg.path = "/";
    if (_cfg.reconnect_ms < CloudClient::kFastReconnectMs)
        _cfg.reconnect_ms = CloudClient::kFastReconnectMs;
    _log.info(F("CLOUD"), F("Begin: transport: %s host: %s port: %u%s%s"),
              transportName_(),
              _cfg.host.c_str(), _cfg.port,
              _cfg.use_ssl ? " ssl " : " ",
              _cfg.path.c_str());
    CloudTransport::Config transport_cfg;
    transport_cfg.host = _cfg.host;
    transport_cfg.port = _cfg.port;
    transport_cfg.path = _cfg.path;
    transport_cfg.use_ssl = _cfg.use_ssl;
    transport_cfg.reconnect_ms = _cfg.reconnect_ms;
    transport_cfg.transport = _cfg.transport;
    _transport->begin(transport_cfg);
}
void CloudClient::loop()
{
    if (!_enabled)
        return;
    if (_cfg.host.length() == 0 || _cfg.port == 0)
        return;
    if (!_wifi.isConnected())
    {
        if (isConnected())
            _transport->disconnect();
        handlePendingTimeouts_();
        return;
    }
    const uint32_t now = millis();
    if (!isConnected() && _reconnect_backoff_until_ms != 0 &&
        (int32_t)(now - _reconnect_backoff_until_ms) < 0)
    {
        handlePendingTimeouts_();
        return;
    }
    _transport->loop();
    maintainConnectionHealth_();
    handlePendingTimeouts_();
    flushQueuedEvents_();
    if (_event_interval_ms)
        maybeSendPeriodicEvent_();
}
void CloudClient::onTransportMessage_(void *ctx, const uint8_t *payload, size_t len)
{
    if (!ctx || !payload || len == 0)
        return;
    auto *self = static_cast<CloudClient *>(ctx);
    self->_last_rx_ms = millis();
    self->handleMessage_(payload, len);
}

void CloudClient::onTransportEvent_(void *ctx, CloudTransport::Event event, const uint8_t *payload, size_t len)
{
    if (!ctx)
        return;
    static_cast<CloudClient *>(ctx)->handleTransportEvent_(event, payload, len);
}

void CloudClient::handleTransportEvent_(CloudTransport::Event event, const uint8_t *payload, size_t len)
{
    switch (event)
    {
    case CloudTransport::Event::Connected:
        _session_id = "";
        _last_connect_ms = millis();
        _last_rx_ms = _last_connect_ms;
        _last_disconnect_ms = 0;
        _reconnect_backoff_until_ms = 0;
        _reconnect_fail_streak = 0;
        _disconnect_reported = false;
        _log.info(F("CLOUD"), F("%s connected: path: %s"), transportName_(), _cfg.path.c_str());
        sendHello_();
        break;
    case CloudTransport::Event::Error:
    {
        // WebSocketsClient doesn't expose structured error details here,
        // but payload sometimes contains a textual hint.
        if (payload && len)
        {
            String msg;
            msg.reserve(len + 1);
            for (size_t i = 0; i < len; ++i)
                msg += (char)payload[i];
            _log.warn(F("CLOUD"), F("%s error: %s"), transportName_(), msg.c_str());
        }
        else
        {
            _log.warn(F("CLOUD"), F("%s error"), transportName_());
        }
        ++_reconnect_fail_streak;
        const uint32_t wait_ms = cloudBackoffMs_(_reconnect_fail_streak, _cfg.reconnect_ms);
        _reconnect_backoff_until_ms = millis() + wait_ms;
        break;
    }
    case CloudTransport::Event::Disconnected:
    {
        const String session = _session_id;
        _last_disconnect_ms = millis();
        const uint32_t connected_ms = _last_connect_ms ? (_last_disconnect_ms - _last_connect_ms) : 0u;
        const uint32_t connected_s = connected_ms / 1000u;
        const bool had_session = session.length() != 0;
        const bool was_established = had_session || connected_ms >= 5000u;
        if (was_established)
        {
            // A live session dropped: retry quickly without long exponential backoff.
            _reconnect_fail_streak = 0;
            _reconnect_backoff_until_ms = _last_disconnect_ms + CloudClient::kFastReconnectMs;
        }
        else
        {
            ++_reconnect_fail_streak;
            _reconnect_backoff_until_ms = _last_disconnect_ms + cloudBackoffMs_(_reconnect_fail_streak, _cfg.reconnect_ms);
        }
        clearPending_();
        if (!_disconnect_reported)
        {
            _log.warn(F("CLOUD"), F("%s disconnected: connected_s: %lu session: %s"),
                      transportName_(),
                      (unsigned long)connected_s,
                      session.length() ? session.c_str() : "-");
            _disconnect_reported = true;
        }
        _session_id = "";
        break;
    }
    default:
        break;
    }
}
void CloudClient::handleMessage_(const uint8_t *payload, size_t len)
{
    DynamicJsonDocument doc(kWsDocCapacity);
    if (deserializeJson(doc, payload, len))
    {
        _log.warn(F("CLOUD"), F("WS JSON parse error (len=%u)"), (unsigned)len);
        return;
    }
    const String type = doc["type"] | "";
    const String id = doc["id"] | "";
    if (type.length() == 0 || id.length() == 0)
    {
        _log.warn(F("CLOUD"), F("WS message missing type/id"));
        return;
    }

    if (type == "hello_ack")
    {
        const char *sid = doc["payload"]["session_id"] | "";
        if (sid && sid[0] != '\0')
        {
            _session_id = sid;
            _log.info(F("CLOUD"), F("Session: %s"), _session_id.c_str());
        }
        else
        {
            _log.warn(F("CLOUD"), F("hello_ack without session_id"));
        }
        return;
    }
    if (type == "error")
    {
        const String reply_to = doc["reply_to"] | "";
        const String code = doc["payload"]["code"] | "";
        const String message = doc["payload"]["message"] | "";
        String details_str;
        JsonVariantConst details = doc["payload"]["details"];
        if (!details.isNull())
            serializeJson(details, details_str);
        _log.warn(F("CLOUD"), F("WS error rx: reply_to: %s code: %s message: %s details: %s"),
                  reply_to.length() ? reply_to.c_str() : "-",
                  code.length() ? code.c_str() : "-",
                  message.length() ? message.c_str() : "-",
                  details_str.length() ? details_str.c_str() : "-");
        return;
    }
    if (type == "ping")
    {
        sendPong_(id, doc["payload"]);
        return;
    }
    if (type == "get")
    {
        handleGet_(id, doc);
        return;
    }
    if (type == "cmd")
    {
        handleCmd_(id, doc);
        return;
    }
}
void CloudClient::sendHello_()
{
    _last_hello_ms = millis();
    DynamicJsonDocument doc(kWsDocCapacity);
    doc["v"] = kProtoVersion;
    doc["type"] = "hello";
    doc["id"] = nextWsId_();
    doc["ts"] = (uint64_t)millis();
    if (_api_key.length())
    {
        JsonObject auth = doc["auth"].to<JsonObject>();
        auth["api_key"] = _api_key;
    }
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["device_id"] = deviceId_();
    payload["device_name"] = _plc.deviceName();
    payload["fw_version"] = _fw_version;
    payload["hw"] = "esp32";
    payload["uptime_s"] = (uint32_t)(millis() / 1000u);
    payload["mac"] = WiFi.macAddress();
    payload["ip"] = localIp_();
    fillStackInfo_(payload.createNestedObject("stack"));

    sendJson_(doc);
}
void CloudClient::maintainConnectionHealth_()
{
    const uint32_t now = millis();
    const bool connected = isConnected();
    if (!connected)
    {
        if (_reconnect_backoff_until_ms != 0 && (int32_t)(now - _reconnect_backoff_until_ms) < 0)
            return;
        if (_last_disconnect_ms == 0)
            _last_disconnect_ms = now;
        if (_cfg.host.length() &&
            (int32_t)(now - _last_disconnect_ms) >= (int32_t)kWsReinitDisconnectedMs)
        {
        _log.warn(F("CLOUD"), F("%s reconnect stalled, reinit"), transportName_());
            _last_disconnect_ms = now;
            begin(_cfg);
        }
        return;
    }

    if (_session_id.length() == 0)
    {
        if ((int32_t)(now - _last_hello_ms) >= (int32_t)kHelloRetryMs)
        {
            _log.warn(F("CLOUD"), F("Session missing, hello retry"));
            sendHello_();
        }
        if (_last_connect_ms != 0 &&
            (int32_t)(now - _last_connect_ms) >= (int32_t)kHelloSessionTimeoutMs)
        {
            _log.warn(F("CLOUD"), F("Session timeout, reconnect"));
            _transport->disconnect();
            return;
        }
    }

    if (_last_rx_ms != 0 &&
        (int32_t)(now - _last_rx_ms) >= (int32_t)kWsSilentTimeoutMs)
    {
        _log.warn(F("CLOUD"), F("%s silent timeout, reconnect"), transportName_());
        _transport->disconnect();
    }
}

bool CloudClient::enqueueEvent_(const String &kind, const String &reason, const String &data_json,
                                const String &unit, uint32_t node_id)
{
    if (kind.length() == 0 || reason.length() == 0)
        return false;
    if (tryCoalesceQueuedEvent_(kind, reason, data_json, unit, node_id))
        return true;
    if (_event_count >= kMaxQueuedEvents)
    {
        _log.warn(F("CLOUD"), F("Event queue full, drop oldest"));
        _event_head = (uint8_t)((_event_head + 1u) % kMaxQueuedEvents);
        --_event_count;
    }
    const uint8_t idx = (uint8_t)((_event_head + _event_count) % kMaxQueuedEvents);
    QueuedEvent &slot = _event_queue[idx];
    slot.used = true;
    slot.unit = unit;
    slot.node_id = node_id;
    slot.kind = kind;
    slot.reason = reason;
    slot.data_json = data_json;
    ++_event_count;
    return true;
}

bool CloudClient::tryCoalesceQueuedEvent_(const String &kind, const String &reason, const String &data_json,
                                          const String &unit, uint32_t node_id)
{
    if (!isCoalescibleStateEvent_(kind))
        return false;
    const uint32_t item_id = eventItemId_(data_json);
    if (item_id == 0)
        return false;
    for (uint8_t i = 0; i < _event_count; ++i)
    {
        const uint8_t idx = (uint8_t)((_event_head + i) % kMaxQueuedEvents);
        QueuedEvent &slot = _event_queue[idx];
        if (!slot.used)
            continue;
        if (slot.kind != kind || slot.unit != unit || slot.node_id != node_id)
            continue;
        if (eventItemId_(slot.data_json) != item_id)
            continue;
        slot.reason = reason;
        slot.data_json = data_json;
        return true;
    }
    return false;
}

bool CloudClient::isCoalescibleStateEvent_(const String &kind)
{
    return kind == "sockets.state" || kind == "lights.state";
}

uint32_t CloudClient::eventItemId_(const String &data_json)
{
    if (!data_json.length())
        return 0;
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, data_json))
        return 0;
    JsonVariantConst id = doc["id"];
    if (id.is<uint32_t>())
        return id.as<uint32_t>();
    if (id.is<unsigned>())
        return (uint32_t)id.as<unsigned>();
    if (id.is<int>())
    {
        const int v = id.as<int>();
        return v > 0 ? (uint32_t)v : 0;
    }
    return 0;
}

bool CloudClient::sendEvent_(const String &kind, const String &reason, const String &data_json,
                             const String &unit, uint32_t node_id)
{
    if (!_session_id.length())
        return false;
    DynamicJsonDocument doc(2048);
    doc["v"] = kProtoVersion;
    doc["type"] = "event";
    doc["id"] = nextWsId_();
    doc["session_id"] = _session_id;
    if (unit.length())
    {
        doc["unit"] = unit;
        if (node_id)
            doc["node_id"] = node_id;
    }
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["kind"] = kind;
    payload["reason"] = reason;
    JsonObject data = payload["data"].to<JsonObject>();
    if (data_json.length())
    {
        DynamicJsonDocument tmp(1024);
        if (!deserializeJson(tmp, data_json))
            data.set(tmp.as<JsonObjectConst>());
    }
    const String source_name = eventSourceName_(unit, node_id);
    if (source_name.length())
    {
        data["source_name"] = source_name;
        if (unit == "stack" && !data["unit_name"].is<const char *>())
            data["unit_name"] = source_name;
    }
    sendJson_(doc);
    logEvent_(F("Notify send"), kind, reason, unit, node_id);
    return true;
}

void CloudClient::logEvent_(const __FlashStringHelper *stage, const String &kind, const String &reason,
                            const String &unit, uint32_t node_id)
{
    if (kind == "periodic" && reason == "periodic")
        return;
    if (unit == "stack" && kind == "stack.snapshot" && reason == "update")
        return;
    if (unit == "stack" && kind == "stack.node" &&
        (reason == "online" || reason == "offline"))
        return;
    if (unit.length())
    {
        if (unit == "stack" && node_id != 0)
        {
            const String unit_name = stackNodeName_(node_id);
            if (unit_name.length())
            {
                _log.info(F("CLOUD"), F("%s: unit: %s slave: %s kind: %s reason: %s"),
                          stage,
                          unit.c_str(),
                          unit_name.c_str(),
                          kind.c_str(),
                          reason.c_str());
                return;
            }
        }
        _log.info(F("CLOUD"), F("%s: unit: %s node_id: %lu kind: %s reason: %s"),
                  stage,
                  unit.c_str(),
                  (unsigned long)node_id,
                  kind.c_str(),
                  reason.c_str());
        return;
    }
    _log.info(F("CLOUD"), F("%s: kind: %s reason: %s"),
              stage,
              kind.c_str(),
              reason.c_str());
}

void CloudClient::flushQueuedEvents_()
{
    if (!_session_id.length() || _event_count == 0)
        return;
    while (_event_count)
    {
        QueuedEvent &slot = _event_queue[_event_head];
        const bool ok = sendEvent_(slot.kind, slot.reason, slot.data_json,
                                   slot.unit, slot.node_id);
        slot = QueuedEvent{};
        _event_head = (uint8_t)((_event_head + 1u) % kMaxQueuedEvents);
        --_event_count;
        if (!ok)
            break;
    }
}
void CloudClient::sendPong_(const String &reply_to, JsonVariantConst payload)
{
    DynamicJsonDocument doc(256);
    doc["v"] = kProtoVersion;
    doc["type"] = "pong";
    doc["id"] = nextWsId_();
    doc["reply_to"] = reply_to;
    if (_session_id.length())
        doc["session_id"] = _session_id;
    if (payload.is<JsonObjectConst>())
        doc["payload"] = payload;
    sendJson_(doc);
}
void CloudClient::handleGet_(const String &req_id, JsonDocument &doc)
{
    const String unit = doc["unit"] | "local";
    const uint32_t node_id = parseNodeId_(doc["node_id"]);
    JsonArrayConst what = doc["payload"]["what"].as<JsonArrayConst>();
    if (unit == "stack")
    {
        if (node_id == 0)
        {
            sendError_(req_id, "missing node_id");
            return;
        }
        handleGetStack_(req_id, node_id, what);
        return;
    }
    handleGetLocal_(req_id, what);
}
void CloudClient::handleGetLocal_(const String &req_id, JsonArrayConst what)
{
    DynamicJsonDocument out(kWsDocCapacity);
    out["v"] = kProtoVersion;
    out["type"] = "result";
    out["id"] = nextWsId_();
    out["reply_to"] = req_id;
    if (_session_id.length())
        out["session_id"] = _session_id;
    out["payload"]["ok"] = true;
    JsonObject data = out["payload"]["data"].to<JsonObject>();

    if (hasWhat_(what, "system"))
        fillSystemInfo_(data.createNestedObject("system"));
    if (hasWhat_(what, "controllers"))
        fillControllersInfo_(data.createNestedObject("controllers"));
    if (hasWhat_(what, "stack"))
        fillStackInfo_(data.createNestedObject("stack"));
    if (hasWhat_(what, "authz"))
        fillAuthzInfo_(data.createNestedObject("authz"));

    sendJson_(out);
}
void CloudClient::handleGetStack_(const String &req_id, uint32_t node_id, JsonArrayConst what)
{
    if (!isStackMaster_())
    {
        sendError_(req_id, "stack role is slave");
        return;
    }
    if (!_network)
    {
        sendError_(req_id, "stack route missing");
        return;
    }
    DynamicJsonDocument out(kWsDocCapacity);
    out["v"] = kProtoVersion;
    out["type"] = "result";
    out["id"] = nextWsId_();
    out["reply_to"] = req_id;
    out["unit"] = "stack";
    out["node_id"] = node_id;
    if (_session_id.length())
        out["session_id"] = _session_id;
    out["payload"]["ok"] = true;
    JsonObject data = out["payload"]["data"].to<JsonObject>();
    if (hasWhat_(what, "system"))
        fillStackCachedSystem_(data.createNestedObject("system"), node_id);
    if (hasWhat_(what, "controllers"))
        fillStackCachedControllers_(data.createNestedObject("controllers"), node_id);
    sendJson_(out);
}
void CloudClient::handleCmd_(const String &req_id, JsonDocument &doc)
{
    String unit = doc["unit"] | "local";
    const uint32_t node_id = (uint32_t)(doc["node_id"] | 0UL);
    JsonObjectConst payload = doc["payload"].as<JsonObjectConst>();
    String ctrl = payload["controller"] | "";
    String action = payload["action"] | "";
    unit.trim();
    ctrl.trim();
    action.trim();
    JsonObjectConst args = payload["args"].as<JsonObjectConst>();
    ActorInfo actor;

    if (!parseActor_(payload, actor) || !resolveActor_(actor))
    {
        _log.warn(F("CLOUD"), F("Cmd rejected: ctrl: %s action: %s actor invalid"),
                  ctrl.c_str(), action.c_str());
        sendError_(req_id, "invalid actor");
        return;
    }
    if (!aclCanControl_(actor, ctrl, action, args, unit == "stack" ? node_id : 0))
    {
        _log.warn(F("CLOUD"), F("Cmd rejected: ctrl: %s action: %s user: %s acl deny"),
                  ctrl.c_str(), action.c_str(),
                  actor.resolved_user.length() ? actor.resolved_user.c_str() : "-");
        sendError_(req_id, "acl deny");
        return;
    }
    if (unit == "stack")
    {
        if (node_id == 0)
        {
            sendError_(req_id, "bad node_id");
            return;
        }
        handleCmdStack_(req_id, node_id, ctrl, action, args, actor);
        return;
    }
    handleCmdLocal_(req_id, ctrl, action, args, actor);
}
void CloudClient::handleCmdLocal_(const String &req_id, const String &ctrl, const String &action,
                                  JsonObjectConst args, const ActorInfo &actor)
{
    bool ok = false;
    String error = "failed";
    if (ctrl == "sockets")
        ok = handleCmdSockets_(_controllers.sockets(), action, args, false, actor, &error);
    else if (ctrl == "lights")
        ok = handleCmdSockets_(_controllers.sockets(), action, args, true, actor, &error);
    else if (ctrl == "thermo")
        ok = handleCmdThermo_(action, args);
    else if (ctrl == "tanks")
        ok = handleCmdTanks_(action, args);
    else if (ctrl == "septic")
        ok = handleCmdSeptic_(action, args);
    else if (ctrl == "watering")
        ok = handleCmdWatering_(action, args);
    else if (ctrl == "security")
        ok = handleCmdSecurity_(action, args, actor);
    else if (ctrl == "ring")
        ok = handleCmdRing_(action, args);
    else if (ctrl == "avr")
        ok = handleCmdAvr_(action, args);
    else if (ctrl == "leak")
        ok = handleCmdLeak_(action, args);

    sendAck_(req_id, ok, ok ? "" : error.c_str());
}
void CloudClient::handleCmdStack_(const String &req_id, uint32_t node_id,
                     const String &ctrl, const String &action, JsonObjectConst args, const ActorInfo &actor)
{
    String ctrl_key = ctrl;
    String action_key = action;
    ctrl_key.trim();
    action_key.trim();
    ctrl_key.toLowerCase();
    action_key.toLowerCase();
    if (!isStackMaster_())
    {
        sendError_(req_id, "stack master missing");
        return;
    }
    const auto requestStackSnapshotRefresh = [&](bool refresh_sockets, bool refresh_lights,
                                                 bool refresh_meteo, bool refresh_thermo) {
        if (!_network)
            return;
        _network->stackRoute().sendRequest(node_id, "system", "snapshot_req", nullptr,
                                           StackRouteAdapter::Mode::Json, true);
        _network->stackRoute().sendRequest(node_id, "controllers", "summary_req", nullptr,
                                           StackRouteAdapter::Mode::Json, true);
        if (refresh_sockets)
        {
            DynamicJsonDocument req(64);
            req["offset"] = 0;
            req["limit"] = StackUnitSnapshot::kPageSize;
            _network->stackRoute().sendRequest(node_id, "sockets", "snapshot_req", &req,
                                               StackRouteAdapter::Mode::Json, true);
        }
        if (refresh_lights)
        {
            DynamicJsonDocument req(64);
            req["offset"] = 0;
            req["limit"] = StackUnitSnapshot::kPageSize;
            _network->stackRoute().sendRequest(node_id, "lights", "snapshot_req", &req,
                                               StackRouteAdapter::Mode::Json, true);
        }
        if (refresh_meteo)
        {
            DynamicJsonDocument req(64);
            req["offset"] = 0;
            req["limit"] = StackUnitSnapshot::kPageSize;
            _network->stackRoute().sendRequest(node_id, "meteo", "snapshot_req", &req,
                                               StackRouteAdapter::Mode::Json, true);
        }
        if (refresh_thermo)
        {
            DynamicJsonDocument req(64);
            req["offset"] = 0;
            req["limit"] = StackUnitSnapshot::kPageSize;
            _network->stackRoute().sendRequest(node_id, "thermo", "snapshot_req", &req,
                                               StackRouteAdapter::Mode::Json, true);
        }
    };

    if (ctrl_key == "sockets")
    {
        const uint32_t item_id = (uint32_t)(args["id"] | 0);
        if (item_id == 0)
        {
            sendError_(req_id, "bad id");
            return;
        }
        if (action_key != "toggle" && action_key != "set")
        {
            sendError_(req_id, "unsupported action");
            return;
        }
        if (!_network)
        {
            sendError_(req_id, "stack route missing");
            return;
        }
        DynamicJsonDocument params(128);
        params["source"] = "cloud";
        params["source_user"] = actor.resolved_user.length() ? actor.resolved_user
                                                              : (actor.plc_username.length() ? actor.plc_username : String("cloud"));
        JsonArray items = params["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = (unsigned)item_id;
        if (action_key == "toggle")
            o["toggle"] = true;
        else
            o["state"] = (String(args["state"] | "") == "on");
        const bool sent = _network->stackRoute().sendEvent(node_id, "sockets", "set", &params,
                                                           StackRouteAdapter::Mode::Json);
        if (!sent)
        {
            sendError_(req_id, "stack route send failed");
            return;
        }
        requestStackSnapshotRefresh(true, false, false, false);
        sendAck_(req_id, true, "");
        return;
    }

    if (ctrl_key == "lights")
    {
        const uint32_t item_id = (uint32_t)(args["id"] | 0);
        if (item_id == 0)
        {
            sendError_(req_id, "bad id");
            return;
        }
        if (action_key != "toggle" && action_key != "set")
        {
            sendError_(req_id, "unsupported action");
            return;
        }
        if (!_network)
        {
            sendError_(req_id, "stack route missing");
            return;
        }
        DynamicJsonDocument params(128);
        params["source"] = "cloud";
        params["source_user"] = actor.resolved_user.length() ? actor.resolved_user
                                                              : (actor.plc_username.length() ? actor.plc_username : String("cloud"));
        JsonArray items = params["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = (unsigned)item_id;
        if (action_key == "toggle")
            o["toggle"] = true;
        else
            o["state"] = (String(args["state"] | "") == "on");
        const bool sent = _network->stackRoute().sendEvent(node_id, "sockets", "set_lights", &params,
                                                           StackRouteAdapter::Mode::Json);
        if (!sent)
        {
            sendError_(req_id, "stack route send failed");
            return;
        }
        requestStackSnapshotRefresh(false, true, false, false);
        sendAck_(req_id, true, "");
        return;
    }
    if (ctrl_key == "meteo")
    {
        const uint32_t item_id = (uint32_t)(args["id"] | 0);
        if (item_id == 0)
        {
            sendError_(req_id, "bad id");
            return;
        }
        if (action_key != "set")
        {
            sendError_(req_id, "unsupported action");
            return;
        }
        if (!_network)
        {
            sendError_(req_id, "stack route missing");
            return;
        }
        DynamicJsonDocument params(384);
        params["source"] = "cloud";
        params["source_user"] = actor.resolved_user.length() ? actor.resolved_user
                                                             : (actor.plc_username.length() ? actor.plc_username : String("cloud"));
        JsonArray items = params["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = (unsigned)item_id;
        if (args.containsKey("enabled"))
            o["enabled"] = args["enabled"];
        if (args.containsKey("name"))
            o["name"] = args["name"];
        if (args.containsKey("group_id"))
            o["group_id"] = args["group_id"];
        if (args.containsKey("type_id"))
            o["type_id"] = args["type_id"];
        if (args.containsKey("pin"))
            o["pin"] = args["pin"];
        if (args.containsKey("addr"))
        {
            o["addr_set"] = true;
            o["addr"] = args["addr"];
        }
        if (args.containsKey("addr_set"))
            o["addr_set"] = args["addr_set"];
        if (args.containsKey("src_node"))
            o["src_node"] = args["src_node"];
        if (args.containsKey("src_sensor"))
            o["src_sensor"] = args["src_sensor"];
        const bool sent = _network->stackRoute().sendEvent(node_id, "meteo", "set", &params,
                                                           StackRouteAdapter::Mode::Json);
        if (!sent)
        {
            sendError_(req_id, "stack route send failed");
            return;
        }
        _network->stackRoute().sendRequest(node_id, "controllers", "summary_req", nullptr,
                                           StackRouteAdapter::Mode::Json, true);
        DynamicJsonDocument req(64);
        req["offset"] = 0;
        req["limit"] = StackUnitSnapshot::kPageSize;
        _network->stackRoute().sendRequest(node_id, "meteo", "snapshot_req", &req,
                                           StackRouteAdapter::Mode::Json, true);
        sendAck_(req_id, true, "");
        return;
    }
    if (ctrl_key == "thermo")
    {
        const uint32_t item_id = (uint32_t)(args["id"] | 0);
        if (item_id == 0)
        {
            sendError_(req_id, "bad id");
            return;
        }
        if (action_key != "toggle" && action_key != "set")
        {
            sendError_(req_id, "unsupported action");
            return;
        }
        if (!_network)
        {
            sendError_(req_id, "stack route missing");
            return;
        }
        DynamicJsonDocument params(384);
        params["source"] = "cloud";
        params["source_user"] = actor.resolved_user.length() ? actor.resolved_user
                                                             : (actor.plc_username.length() ? actor.plc_username : String("cloud"));
        JsonArray items = params["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = (unsigned)item_id;
        if (action_key == "toggle")
        {
            o["toggle"] = true;
        }
        else
        {
            if (args.containsKey("enabled"))
                o["enabled"] = args["enabled"];
            if (args.containsKey("name"))
                o["name"] = args["name"];
            if (args.containsKey("group_id"))
                o["group_id"] = args["group_id"];
            if (args.containsKey("sensor_id"))
                o["sensor_id"] = args["sensor_id"];
            if (args.containsKey("sensor_node_id"))
                o["sensor_node_id"] = args["sensor_node_id"];
            if (args.containsKey("mode_id"))
                o["mode_id"] = args["mode_id"];
            if (args.containsKey("target_c"))
                o["target_c"] = args["target_c"];
            if (args.containsKey("hyst"))
                o["hyst"] = args["hyst"];
            if (args.containsKey("heat_port"))
                o["heat_port"] = args["heat_port"];
            if (args.containsKey("cool_port"))
                o["cool_port"] = args["cool_port"];
            if (args.containsKey("button_port"))
                o["button_port"] = args["button_port"];
            if (args.containsKey("power_on"))
                o["power_on"] = args["power_on"];
            else if (args.containsKey("state"))
                o["power_on"] = (String(args["state"] | "") == "on");
        }
        const bool sent = _network->stackRoute().sendEvent(node_id, "thermo", "set", &params,
                                                           StackRouteAdapter::Mode::Json);
        if (!sent)
        {
            sendError_(req_id, "stack route send failed");
            return;
        }
        requestStackSnapshotRefresh(false, false, false, true);
        sendAck_(req_id, true, "");
        return;
    }
    sendError_(req_id, "stack controller not migrated");
}
bool CloudClient::handleCmdSockets_(SocketController &s, const String &action, JsonObjectConst args, bool lights,
                                    const ActorInfo &actor, String *error_out)
{
    _controllers.ensureSocketConfigsLoaded();
    const uint8_t id = (uint8_t)(args["id"] | 0);
    if (id == 0)
    {
        if (error_out)
            *error_out = "bad id";
        return false;
    }
    const char *ctrl_name = lights ? "lights" : "sockets";
    const char *user_name = actor.username.length()
        ? actor.username.c_str()
        : (actor.plc_username.length() ? actor.plc_username.c_str() : "-");
    const bool controller_enabled = lights ? s.lightsEnabled() : s.controllerEnabled();
    if (!controller_enabled)
    {
        if (error_out)
            *error_out = "controller disabled";
        _log.warn(F("CLOUD"), F("Cmd rejected: %s id: %u reason: controller disabled user: %s"),
                  ctrl_name, (unsigned)id, user_name);
        return false;
    }
    const auto *cfg = lights ? s.lightConfig(id) : s.config(id);
    if (!cfg)
    {
        if (error_out)
            *error_out = "item not found";
        _log.warn(F("CLOUD"), F("Cmd rejected: %s id: %u reason: item missing user: %s"),
                  ctrl_name, (unsigned)id, user_name);
        return false;
    }
    if (!cfg->enabled)
    {
        if (error_out)
            *error_out = "item disabled";
        _log.warn(F("CLOUD"), F("Cmd rejected: %s id: %u reason: item disabled user: %s"),
                  ctrl_name, (unsigned)id, user_name);
        return false;
    }
    if (cfg->relay_port == SocketController::kInvalidPort)
    {
        if (error_out)
            *error_out = "relay port missing";
        _log.warn(F("CLOUD"), F("Cmd rejected: %s id: %u reason: relay missing user: %s"),
                  ctrl_name, (unsigned)id, user_name);
        return false;
    }
    if (action == "toggle")
    {
        _log.info(F("CLOUD"), F("Cmd: %s id: %u action: toggle user: %s"),
                  ctrl_name, (unsigned)id, user_name);
        const bool ok = lights ? s.toggleLightRelayById(id) : s.toggleRelayById(id);
        if (!ok && error_out && !error_out->length())
            *error_out = "toggle failed";
        return ok;
    }
    if (action == "set")
    {
        const String st = args["state"] | "";
        const bool on = (st == "on");
        _log.info(F("CLOUD"), F("Cmd: %s id: %u action: set state: %s user: %s"),
                  ctrl_name, (unsigned)id, on ? "on" : "off", user_name);
        const bool ok = lights ? s.setLightRelayById(id, on) : s.setRelayById(id, on);
        if (!ok && error_out && !error_out->length())
            *error_out = "set failed";
        return ok;
    }
    if (error_out)
        *error_out = "unsupported action";
    return false;
}
bool CloudClient::handleCmdThermo_(const String &action, JsonObjectConst args)
{
    const uint8_t id = (uint8_t)(args["id"] | 0);
    if (id == 0)
        return false;
    if (action == "power")
        return _controllers.thermo().setPower(id, (String(args["state"] | "") == "on"), "cloud");
    if (action == "mode")
        return _controllers.thermo().setMode(id, parseThermoMode_(args["mode"] | ""));
    if (action == "target")
        return _controllers.thermo().setTarget(id, args["target_c"] | 0.0f);
    return false;
}
bool CloudClient::handleCmdTanks_(const String &action, JsonObjectConst args)
{
    if (action != "power")
        return false;
    const uint8_t id = (uint8_t)(args["id"] | 0);
    if (id == 0)
        return false;
    return _controllers.tanks().setPower(id, (String(args["state"] | "") == "on"));
}
bool CloudClient::handleCmdSeptic_(const String &action, JsonObjectConst args)
{
    if (action != "monitor")
        return false;
    const uint8_t id = (uint8_t)(args["id"] | 1);
    return _controllers.septic().setMonitoring(id, (String(args["state"] | "") == "on"));
}
bool CloudClient::handleCmdWatering_(const String &action, JsonObjectConst args)
{
    const uint8_t id = (uint8_t)(args["id"] | 0);
    if (id == 0)
        return false;
    uint8_t slot = (uint8_t)(args["slot"] | 1);
    if (slot < 1 || slot > 3)
        slot = 1;
    const uint8_t slot_idx = (uint8_t)(slot - 1u);
    if (action == "status")
        return _controllers.watering().setStatus(id, (String(args["state"] | "") == "on"));
    if (action == "weekdays")
        return _controllers.watering().setWeekdaysMask(
            id,
            (uint8_t)((unsigned)(args["weekdays_mask"] | 0) & 0x7Fu));
    if (action == "time")
    {
        const uint8_t hour = (uint8_t)(args["hour"] | 0);
        const uint8_t minute = (uint8_t)(args["minute"] | 0);
        if (hour > 23 || minute > 59)
            return false;
        return _controllers.watering().setStartTimeSlot(id, slot_idx, hour, minute);
    }
    if (action == "duration")
        return _controllers.watering().setDurationSlot(
            id,
            slot_idx,
            (uint32_t)(args["duration_s"] | 0UL));
    return false;
}
bool CloudClient::handleCmdSecurity_(const String &action, JsonObjectConst args, const ActorInfo &actor)
{
    const String src_user = actor.resolved_user.length() ? actor.resolved_user : String("cloud");
    if (action == "arm")
        return _controllers.security().armFrom("cloud", src_user);
    if (action == "disarm")
        return _controllers.security().disarmFrom("cloud", src_user);
    if (action == "clear")
    {
        _controllers.security().clearDetect();
        return true;
    }
    if (action == "rfid")
    {
        String uid = args["uid"] | "";
        if (uid.length() == 0)
            uid = args["serial"] | "";
        if (!uid.length())
            return false;
        const String src = actor.resolved_user.length() ? actor.resolved_user : String(args["name"] | "cloud");
        return _controllers.security().processRfidUidString(uid.c_str(), src.c_str());
    }
    if (action == "ibutton")
    {
        const String serial = args["serial"] | "";
        if (!serial.length())
            return false;
        const String src = actor.resolved_user.length() ? actor.resolved_user : String(args["name"] | "cloud");
        return _controllers.security().processIButtonSerialString(serial.c_str(), src.c_str());
    }
    return false;
}
bool CloudClient::handleCmdRing_(const String &action, JsonObjectConst args)
{
    if (action != "hold")
        return false;
    const bool on = (String(args["state"] | "") == "on");
    return _controllers.ring().setHoldRelayWithSource(on, RingController::Source::Web);
}
bool CloudClient::handleCmdAvr_(const String &action, JsonObjectConst args)
{
    if (action == "auto")
        return _controllers.avr().setAutoMode((String(args["state"] | "") == "on"));
    if (action == "source")
    {
        String src = args["source"] | "";
        src.toLowerCase();
        if (src == "main")
            return _controllers.avr().setManualSource(AvrController::Source::Main);
        if (src == "reserve")
            return _controllers.avr().setManualSource(AvrController::Source::Reserve);
        return _controllers.avr().setManualSource(AvrController::Source::Off);
    }
    if (action == "clear_fault")
    {
        _controllers.avr().clearFault();
        return true;
    }
    return false;
}
bool CloudClient::handleCmdLeak_(const String &action, JsonObjectConst args)
{
    if (action == "ack_all")
        return _controllers.leak().ackAll();
    if (action == "ack")
    {
        const uint8_t id = (uint8_t)(args["id"] | 0);
        if (id == 0)
            return false;
        return _controllers.leak().ack(id);
    }
    if (action == "power")
    {
        const uint8_t id = (uint8_t)(args["id"] | 0);
        if (id == 0)
            return false;
        return _controllers.leak().setPower(id, (String(args["state"] | "") == "on"));
    }
    return false;
}
void CloudClient::sendAck_(const String &reply_to, bool ok, const char *error)
{
    DynamicJsonDocument doc(256);
    doc["v"] = kProtoVersion;
    doc["type"] = "ack";
    doc["id"] = nextWsId_();
    doc["reply_to"] = reply_to;
    if (_session_id.length())
        doc["session_id"] = _session_id;
    doc["payload"]["ok"] = ok;
    if (!ok && error)
        doc["payload"]["error"] = error;
    sendJson_(doc);
}
void CloudClient::sendError_(const String &reply_to, const char *msg)
{
    DynamicJsonDocument doc(256);
    doc["v"] = kProtoVersion;
    doc["type"] = "error";
    doc["id"] = nextWsId_();
    doc["reply_to"] = reply_to;
    if (_session_id.length())
        doc["session_id"] = _session_id;
    doc["payload"]["code"] = "bad_request";
    doc["payload"]["message"] = msg ? msg : "error";
    sendJson_(doc);
}
void CloudClient::onSocketEvent_(void *ctx, bool lights, uint8_t id, const String &name, bool state_on,
                                 const char *source)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    DynamicJsonDocument doc(256);
    doc["controller"] = lights ? "lights" : "sockets";
    doc["id"] = id;
    if (name.length())
        doc["name"] = name;
    doc["state"] = state_on;
    if (source && source[0] != '\0')
        doc["source"] = source;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_(lights ? "lights.state" : "sockets.state", "change", json);
}

void CloudClient::onMeteoAlarmEvent_(void *ctx, uint32_t node_id, uint8_t sensor_id, bool alarm)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    if (node_id != 0)
        return;
    DynamicJsonDocument doc(192);
    doc["sensor_id"] = sensor_id;
    doc["alarm"] = alarm;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("meteo.sensor", alarm ? "alarm" : "restore", json);
}

void CloudClient::onThermoEvent_(void *ctx, uint8_t id, const String &name, bool power_on, const char *source)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    DynamicJsonDocument doc(192);
    doc["id"] = id;
    if (name.length())
        doc["name"] = name;
    doc["power_on"] = power_on;
    if (source && source[0] != '\0')
        doc["source"] = source;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("thermo.power", "change", json);
}

void CloudClient::onTankEvent_(void *ctx, uint8_t tank_id, const String &name, bool empty)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    DynamicJsonDocument doc(192);
    doc["id"] = tank_id;
    if (name.length())
        doc["name"] = name;
    doc["empty"] = empty;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("tanks.level", empty ? "empty" : "change", json);
}

void CloudClient::onSepticEvent_(void *ctx, uint8_t septic_id, const String &name, bool is_alarm)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    DynamicJsonDocument doc(192);
    doc["id"] = septic_id;
    if (name.length())
        doc["name"] = name;
    doc["alarm"] = is_alarm;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("septic.level", is_alarm ? "alarm" : "warning", json);
}

void CloudClient::onSecurityArmEvent_(void *ctx, bool armed)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    DynamicJsonDocument doc(64);
    doc["armed"] = armed;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("security.arm", armed ? "armed" : "disarmed", json);
}

void CloudClient::onSecurityAlarmEvent_(void *ctx, bool alarm_on)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    DynamicJsonDocument doc(64);
    doc["alarm_on"] = alarm_on;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("security.alarm", alarm_on ? "alarm" : "clear", json);
}

void CloudClient::onSecurityClearEvent_(void *ctx)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    self->enqueueEvent_("security.detect", "clear", String("{}"));
}

void CloudClient::onSecurityDetectEvent_(void *ctx, uint8_t sensor_id, const String &name, bool silent)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    DynamicJsonDocument doc(192);
    doc["sensor_id"] = sensor_id;
    if (name.length())
        doc["name"] = name;
    doc["silent"] = silent;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("security.detect", silent ? "silent" : "detect", json);
}

void CloudClient::onWateringEvent_(void *ctx, WateringController::Event ev,
                                   const WateringController::RuleConfig &cfg,
                                   const WateringController::RuleState &st)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    const char *reason = "change";
    switch (ev)
    {
    case WateringController::Event::Start: reason = "start"; break;
    case WateringController::Event::PauseEmpty: reason = "pause_empty"; break;
    case WateringController::Event::Resume: reason = "resume"; break;
    case WateringController::Event::Stop: reason = "stop"; break;
    case WateringController::Event::StopEmpty: reason = "stop_empty"; break;
    case WateringController::Event::StopDone: reason = "stop_done"; break;
    }
    DynamicJsonDocument doc(256);
    doc["id"] = cfg.id;
    if (cfg.name.length())
        doc["name"] = cfg.name;
    doc["active"] = st.active;
    doc["paused"] = st.paused;
    doc["tank_id"] = cfg.tank_id;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("watering.rule", reason, json);
}

void CloudClient::onRingEvent_(void *ctx, bool on)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    DynamicJsonDocument doc(64);
    doc["hold_on"] = on;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("ring.hold", on ? "start" : "stop", json);
}

void CloudClient::onAvrEvent_(void *ctx, AvrController::Event ev, const AvrController::State &st,
                              const char *message)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    const char *kind = "avr.state";
    const char *reason = "change";
    if (ev == AvrController::Event::MainState)
    {
        kind = "avr.main";
        reason = (message && message[0]) ? message : "change";
    }
    else if (ev == AvrController::Event::SourceSwitch)
    {
        kind = "avr.source";
        reason = (message && message[0]) ? message : "change";
    }
    else if (ev == AvrController::Event::Fault)
    {
        kind = "avr.fault";
        reason = (message && message[0]) ? message : "fault";
    }
    DynamicJsonDocument doc(256);
    doc["active_source"] = AvrController::sourceName(st.active_source);
    doc["target_source"] = AvrController::sourceName(st.target_source);
    doc["fault"] = AvrController::faultName(st.fault);
    doc["main_ok"] = st.main_ok;
    doc["reserve_ok"] = st.reserve_ok;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_(kind, reason, json);
}

void CloudClient::onLeakEvent_(void *ctx, LeakController::Event ev, uint8_t id, const String &name,
                               bool wet, bool alarm_latched)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;
    DynamicJsonDocument doc(192);
    doc["id"] = id;
    if (name.length())
        doc["name"] = name;
    doc["wet"] = wet;
    doc["alarm_latched"] = alarm_latched;
    String json;
    serializeJson(doc, json);
    self->enqueueEvent_("leak.zone", (ev == LeakController::Event::Detect) ? "detect" : "ack", json);
}

void CloudClient::onRuleTriggered_(void *ctx, const RulesController::Rule &rule)
{
    auto *self = static_cast<CloudClient *>(ctx);
    if (!self)
        return;

    DynamicJsonDocument trigger_doc(256);
    trigger_doc["rule_id"] = rule.id;
    if (rule.name.length())
        trigger_doc["rule_name"] = rule.name;
    String trigger_json;
    serializeJson(trigger_doc, trigger_json);
    self->enqueueEvent_("rules.trigger", "trigger", trigger_json);

    for (size_t i = 0; i < RulesController::kActionCount; ++i)
    {
        const auto &action = rule.actions[i];
        if (!action.enabled || action.kind != RulesController::ActionKind::Notify)
            continue;
        DynamicJsonDocument doc(320);
        doc["rule_id"] = rule.id;
        doc["action_id"] = action.id;
        if (rule.name.length())
            doc["rule_name"] = rule.name;
        if (action.value.length())
            doc["message"] = action.value;
        if (action.node_id)
            doc["node_id"] = action.node_id;
        if (action.delay_ms)
            doc["delay_ms"] = action.delay_ms;
        String json;
        serializeJson(doc, json);
        const String kind = action.controller.length() ? action.controller : String("system.notify");
        const String reason = action.parameter.length() ? action.parameter : String("rule");
        self->enqueueEvent_(kind, reason, json);
    }
}

void CloudClient::fillSystemInfo_(JsonObject out)
{
    out["device_name"] = _plc.deviceName();
    out["uptime_ms"] = (uint32_t)millis();
    out["board"] = ActiveBoardProfile::UI_NAME;
    out["fw_version"] = _fw_version;

    JsonObject wifi = out["wifi"].to<JsonObject>();
    wifi["mode"] = _wifi.modeName();
    if (_wifi.staEnabled())
        wifi["ssid"] = _wifi.ssid();
    if (_wifi.apEnabled())
        wifi["ap_ssid"] = _wifi.apSsid();
    wifi["ip"] = localIp_();
    wifi["mac"] = WiFi.macAddress();

    JsonObject plc = out["plc"].to<JsonObject>();
    plc["board_temp"] = _plc.boardTemp();

    JsonObject fan = out["fan"].to<JsonObject>();
    fan["mode"] = _plc.fanManualMode() ? "manual" : "auto";
    fan["fan_on"] = _plc.fanStatus();
    fan["on_c"] = _plc.fanOnC();
    fan["hyst_c"] = _plc.fanHysteresisC();

    Ds3231Mz::DateTime dt{};
    if (_rtc.Time(dt))
    {
        char date_buf[16] = {};
        char time_buf[16] = {};
        snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
                 (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
                 (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
        float t = 0.0f;
        _rtc.readTemp(t);
        JsonObject rtc = out["rtc"].to<JsonObject>();
        rtc["date"] = date_buf;
        rtc["time"] = time_buf;
        rtc["weekday"] = (unsigned)dt.day_of_week;
        rtc["temp_c"] = t;
    }

    if (_gsm)
    {
        JsonObject gsm = out["gsm"].to<JsonObject>();
        gsm["enabled"] = _gsm->enabled();
        gsm["started"] = _gsm->started();
        gsm["imei"] = _gsm->imei();
        gsm["imsi"] = _gsm->imsi();
        gsm["operator"] = _gsm->operatorName();
        gsm["signal"] = _gsm->signalQuality();
        gsm["reg_status"] = _gsm->regStatus();
        gsm["last_error"] = _gsm->lastError();
        gsm["last_urc"] = _gsm->lastUrc();
        gsm["last_call"] = _gsm->lastCallNumber();
        gsm["last_ussd"] = _gsm->lastUssd();
        gsm["last_http_status"] = _gsm->lastHttpStatus();
        gsm["last_http_len"] = _gsm->lastHttpLen();
    }
}

namespace
{
const char *aclControllerKey_(UsersRegistry::AclController ctrl)
{
    switch (ctrl)
    {
    case UsersRegistry::AclController::Sockets: return "sockets";
    case UsersRegistry::AclController::Lights: return "lights";
    case UsersRegistry::AclController::Meteo: return "meteo";
    case UsersRegistry::AclController::Thermo: return "thermo";
    case UsersRegistry::AclController::Tanks: return "tanks";
    case UsersRegistry::AclController::Septic: return "septic";
    case UsersRegistry::AclController::Security: return "security";
    case UsersRegistry::AclController::Watering: return "watering";
    case UsersRegistry::AclController::Leak: return "leak";
    case UsersRegistry::AclController::Avr: return "avr";
    case UsersRegistry::AclController::Ring: return "ring";
    }
    return "";
}
} // namespace

void CloudClient::fillAuthzInfo_(JsonObject out)
{
    if (!_users)
        return;

    JsonObject users = out["users"].to<JsonObject>();
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled || u.username.length() == 0)
            continue;

        JsonObject entry = users[u.username].to<JsonObject>();
        entry["plc_username"] = u.username;
        JsonObject permissions = entry["permissions"].to<JsonObject>();
        permissions["read_all"] = false;
        permissions["write_all"] = false;
        permissions["status"]["read"] = true;
        permissions["network"]["read"] = true;
        JsonObject controllers = permissions["controllers"].to<JsonObject>();

        JsonObject quick = controllers["quick_actions"].to<JsonObject>();
        quick["read"] = true;
        quick["write"] = u.tg_quick_actions;

        for (uint8_t ctrl_idx = 0; ctrl_idx < (uint8_t)UsersRegistry::kAclControllerCount; ++ctrl_idx)
        {
            const auto ctrl = static_cast<UsersRegistry::AclController>(ctrl_idx);
            const char *key = aclControllerKey_(ctrl);
            if (!key || key[0] == '\0')
                continue;
            JsonObject policy = controllers[key].to<JsonObject>();
            const bool ctrl_allowed = u.controllerAllowed(0, ctrl);
            if (!ctrl_allowed)
            {
                policy["read"] = false;
                policy["write"] = false;
                continue;
            }

            const uint16_t max_id = UsersRegistry::kAclItemsPerController[ctrl_idx];
            uint16_t view_count = 0;
            uint16_t control_count = 0;
            for (uint16_t item_id = 1; item_id <= max_id; ++item_id)
            {
                if (u.itemViewAllowedRaw(0, ctrl, item_id))
                    ++view_count;
                if (u.itemControlAllowedRaw(0, ctrl, item_id))
                    ++control_count;
            }

            // Controller-level allow is the base permission. Item-level ACL narrows it down via ids.
            const bool read = true;
            const bool write = (control_count != 0 || max_id == 0);
            policy["read"] = read;
            policy["write"] = write;
            const bool partial_view = (view_count != 0 && view_count < max_id);
            const bool partial_control = (control_count != 0 && control_count < max_id);
            if ((partial_view || partial_control) && max_id != 0)
            {
                JsonArray ids = policy["ids"].to<JsonArray>();
                for (uint16_t item_id = 1; item_id <= max_id; ++item_id)
                {
                    if (u.itemViewAllowedRaw(0, ctrl, item_id) ||
                        u.itemControlAllowedRaw(0, ctrl, item_id))
                        ids.add(item_id);
                }
            }
        }
    }
}
void CloudClient::fillControllersInfo_(JsonObject out)
{
    _controllers.ensureSocketConfigsLoaded();
    fillGroups_(out.createNestedArray("groups"));
    fillSockets_(out.createNestedArray("sockets"), false);
    fillSockets_(out.createNestedArray("lights"), true);
    fillMeteo_(out.createNestedArray("meteo"));
    fillThermo_(out.createNestedArray("thermo"));
    fillTanks_(out.createNestedArray("tanks"));
    fillSeptic_(out.createNestedArray("septic"));
    fillWatering_(out.createNestedArray("watering"));
    fillSecurity_(out.createNestedObject("security"));
    fillRing_(out.createNestedObject("ring"));
    fillAvr_(out.createNestedObject("avr"));
    fillLeak_(out.createNestedArray("leak"));
}
void CloudClient::fillGroups_(JsonArray out)
{
    if (!_configs)
        return;
    for (size_t i = 0; i < _configs->groupCount(); ++i)
    {
        ConfigsManagerIface::GroupConfig g;
        if (!_configs->groupByIndex(i, g) || g.id == 0 || g.name.length() == 0)
            continue;
        JsonObject o = out.add<JsonObject>();
        o["id"] = (unsigned)g.id;
        o["name"] = g.name;
        o["sort"] = (unsigned)g.sort;
    }
}
void CloudClient::fillSockets_(JsonArray out, bool lights)
{
    ScratchBuffer *scratch = scratch_();
    if (!scratch)
        return;
    const size_t count = lights ? SocketController::kLightCount : SocketController::kSocketCount;
    bool controller_enabled = false;
    {
        const auto scratch_guard = _scratch_lock.guard();
        auto guard = _controllers.sockets().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            _log.warn(F("CLOUD"), F("Snapshot lock timeout: sockets lights: %u"), lights ? 1u : 0u);
            return;
        }
        controller_enabled = lights ? _controllers.sockets().lightsEnabled()
                                    : _controllers.sockets().controllerEnabled();
        for (size_t i = 0; i < count; ++i)
        {
            const auto *cfg = lights ? _controllers.sockets().lightConfigByIndex(i)
                                     : _controllers.sockets().configByIndex(i);
            const auto *st = lights ? _controllers.sockets().lightStateByIndex(i)
                                    : _controllers.sockets().stateByIndex(i);
            scratch->sockets[i].valid = false;
            if (!cfg || !st || !cfg->enabled)
                continue;
            scratch->sockets[i].valid = true;
            scratch->sockets[i].cfg = *cfg;
            scratch->sockets[i].st = *st;
        }
        for (size_t i = 0; i < count; ++i)
        {
        const auto &item = scratch->sockets[i];
        if (!item.valid)
            continue;
        const auto &cfg = item.cfg;
        const auto &st = item.st;
        JsonObject o = out.add<JsonObject>();
        o["id"] = (unsigned)cfg.id;
        o["group_id"] = (unsigned)cfg.group_id;
        o["enabled"] = controller_enabled && cfg.enabled &&
                       cfg.relay_port != SocketController::kInvalidPort;
        if (cfg.name.length())
            o["name"] = cfg.name;
        if (cfg.button_port != SocketController::kInvalidPort)
            o["button"] = cfg.button_port;
        if (cfg.relay_port != SocketController::kInvalidPort)
            o["relay"] = cfg.relay_port;
        o["state"] = st.relay_on;
    }
    }
}
void CloudClient::fillMeteo_(JsonArray out)
{
    ScratchBuffer *scratch = scratch_();
    if (!scratch)
        return;
    {
        const auto scratch_guard = _scratch_lock.guard();
        auto guard = _controllers.meteo().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            static uint32_t last_warn_ms = 0;
            const uint32_t now = millis();
            if (last_warn_ms == 0 || (uint32_t)(now - last_warn_ms) >= kSnapshotWarnIntervalMs)
            {
                last_warn_ms = now;
                _log.warn(F("CLOUD"), F("Snapshot lock timeout: meteo"));
            }
            return;
        }
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = _controllers.meteo().configByIndex(i);
            const auto *st = _controllers.meteo().stateByIndex(i);
            scratch->meteo[i].valid = false;
            if (!cfg || !st || !cfg->enabled)
                continue;
            scratch->meteo[i].valid = true;
            scratch->meteo[i].cfg = *cfg;
            scratch->meteo[i].st = *st;
        }
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
        const auto &item = scratch->meteo[i];
        if (!item.valid)
            continue;
        const auto &cfg = item.cfg;
        const auto &st = item.st;
        JsonObject o = out.add<JsonObject>();
        o["id"] = (unsigned)cfg.id;
        o["group_id"] = (unsigned)cfg.group_id;
        o["enabled"] = cfg.enabled;
        if (cfg.name.length())
            o["name"] = cfg.name;
        o["type"] = MeteoController::typeName(cfg.type);
        if (cfg.type == MeteoController::SensorType::Dht22 &&
            cfg.dht_pin != MeteoController::kInvalidPin)
            o["pin"] = cfg.dht_pin;
        if (cfg.type == MeteoController::SensorType::Ds18b20 && cfg.ds18_addr_set)
        {
            char hex[17] = {};
            MeteoController::formatHexAddr(cfg.ds18_addr, hex);
            o["addr"] = hex;
        }
        if (st.has_temp)
            o["temp_c"] = st.temp_c;
        if (st.has_humidity)
            o["hum"] = st.humidity;
        o["has_temp"] = st.has_temp;
        o["has_hum"] = st.has_humidity;
        o["ok"] = st.ok;
    }
    }
}
void CloudClient::fillThermo_(JsonArray out)
{
    ScratchBuffer *scratch = scratch_();
    if (!scratch)
        return;
    {
        const auto scratch_guard = _scratch_lock.guard();
        auto guard = _controllers.thermo().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            _log.warn(F("CLOUD"), F("Snapshot lock timeout: thermo"));
            return;
        }
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = _controllers.thermo().configByIndex(i);
            const auto *st = _controllers.thermo().stateByIndex(i);
            scratch->thermo[i].valid = false;
            if (!cfg || !st || !cfg->enabled)
                continue;
            scratch->thermo[i].valid = true;
            scratch->thermo[i].cfg = *cfg;
            scratch->thermo[i].st = *st;
        }
        auto meteo_guard = _controllers.meteo().lockGuard(kSnapshotLockTimeoutMs);
        if (meteo_guard.locked())
        {
            for (size_t i = 0; i <= MeteoController::kSensorCount; ++i)
                scratch->thermo_sensors[i] = ScratchBuffer::ThermoSensorItem{};
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = _controllers.meteo().configByIndex(i);
                const auto *st = _controllers.meteo().stateByIndex(i);
                if (!cfg || cfg->id == 0 || cfg->id > MeteoController::kSensorCount)
                    continue;
                auto &dst = scratch->thermo_sensors[cfg->id];
                dst.found = true;
                if (cfg->name.length())
                    dst.name = cfg->name;
                if (st)
                {
                    dst.has_temp = st->has_temp;
                    dst.temp_c = st->temp_c;
                }
            }
        }
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
        const auto &item = scratch->thermo[i];
        if (!item.valid)
            continue;
        const auto &cfg = item.cfg;
        const auto &st = item.st;
        JsonObject o = out.add<JsonObject>();
        o["id"] = (unsigned)cfg.id;
        o["group_id"] = (unsigned)cfg.group_id;
        o["enabled"] = cfg.enabled;
        if (cfg.name.length())
            o["name"] = cfg.name;
        o["sensor"] = (unsigned)cfg.sensor_id;
        if (cfg.sensor_node_id != 0)
            o["sensor_node"] = (unsigned long)cfg.sensor_node_id;
        o["mode"] = ThermoController::modeName(cfg.mode);
        o["target"] = cfg.target_c;
        o["hyst"] = cfg.hysteresis;
        if (cfg.heat_port != ThermoController::kInvalidPort)
            o["heat"] = cfg.heat_port;
        if (cfg.cool_port != ThermoController::kInvalidPort)
            o["cool"] = cfg.cool_port;
        if (cfg.button_port != ThermoController::kInvalidPort)
            o["button"] = cfg.button_port;
        o["power_on"] = st.power_on;
        o["heat_on"] = st.heat_on;
        o["cool_on"] = st.cool_on;

        if (cfg.sensor_id != ThermoController::kInvalidSensor &&
            cfg.sensor_node_id == 0 &&
            cfg.sensor_id <= MeteoController::kSensorCount)
        {
            const auto &sensor = scratch->thermo_sensors[cfg.sensor_id];
            if (sensor.found)
            {
                if (sensor.name.length())
                    o["sensor_name"] = sensor.name;
                o["has_temp"] = sensor.has_temp;
                if (sensor.has_temp)
                {
                    o["temp_c"] = sensor.temp_c;
                }
            }
        }
    }
    }
}
void CloudClient::fillTanks_(JsonArray out)
{
    ScratchBuffer *scratch = scratch_();
    if (!scratch)
        return;
    {
        const auto scratch_guard = _scratch_lock.guard();
        auto guard = _controllers.tanks().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            _log.warn(F("CLOUD"), F("Snapshot lock timeout: tanks"));
            return;
        }
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = _controllers.tanks().configByIndex(i);
            const auto *st = _controllers.tanks().stateByIndex(i);
            scratch->tanks[i].valid = false;
            if (!cfg || !st || !cfg->enabled)
                continue;
            scratch->tanks[i].valid = true;
            scratch->tanks[i].cfg = *cfg;
            scratch->tanks[i].st = *st;
        }
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
        const auto &item = scratch->tanks[i];
        if (!item.valid)
            continue;
        const auto &cfg = item.cfg;
        const auto &st = item.st;
        JsonObject o = out.add<JsonObject>();
        o["id"] = (unsigned)cfg.id;
        o["group_id"] = (unsigned)cfg.group_id;
        o["enabled"] = cfg.enabled;
        o["power_on"] = cfg.power_on;
        if (cfg.name.length())
            o["name"] = cfg.name;
        if (cfg.level_low != TankController::kInvalidPort)
            o["low"] = cfg.level_low;
        if (cfg.level_mid != TankController::kInvalidPort)
            o["mid"] = cfg.level_mid;
        if (cfg.level_full != TankController::kInvalidPort)
            o["full"] = cfg.level_full;
        if (cfg.relay_valve != TankController::kInvalidPort)
            o["valve"] = cfg.relay_valve;
        if (cfg.relay_pump != TankController::kInvalidPort)
            o["pump"] = cfg.relay_pump;
        if (cfg.relay_alarm != TankController::kInvalidPort)
            o["alarm"] = cfg.relay_alarm;
        o["level_low"] = st.level_low;
        o["level_mid"] = st.level_mid;
        o["level_full"] = st.level_full;
        o["levels_ok"] = st.levels_ok;
        o["valve_on"] = st.valve_on;
        o["pump_on"] = st.pump_on;
        o["alarm_on"] = st.alarm_on;
    }
    }
}
void CloudClient::fillSeptic_(JsonArray out)
{
    ScratchBuffer *scratch = scratch_();
    if (!scratch)
        return;
    {
        const auto scratch_guard = _scratch_lock.guard();
        auto guard = _controllers.septic().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            _log.warn(F("CLOUD"), F("Snapshot lock timeout: septic"));
            return;
        }
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = _controllers.septic().configByIndex(i);
            const auto *st = _controllers.septic().stateByIndex(i);
            scratch->septic[i].valid = false;
            if (!cfg || !st || !cfg->enabled)
                continue;
            scratch->septic[i].valid = true;
            scratch->septic[i].cfg = *cfg;
            scratch->septic[i].st = *st;
        }
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
        const auto &item = scratch->septic[i];
        if (!item.valid)
            continue;
        const auto &cfg = item.cfg;
        const auto &st = item.st;
        JsonObject o = out.add<JsonObject>();
        o["id"] = (unsigned)cfg.id;
        o["group_id"] = (unsigned)cfg.group_id;
        o["enabled"] = cfg.enabled;
        if (cfg.name.length())
            o["name"] = cfg.name;
        o["monitor"] = cfg.monitoring_on;
        if (cfg.warning_port != SepticController::kInvalidPort)
            o["warning_port"] = cfg.warning_port;
        if (cfg.alarm_port != SepticController::kInvalidPort)
            o["alarm_port"] = cfg.alarm_port;
        if (cfg.relay_warning != SepticController::kInvalidPort)
            o["relay_warning"] = cfg.relay_warning;
        if (cfg.relay_alarm != SepticController::kInvalidPort)
            o["relay_alarm"] = cfg.relay_alarm;
        o["warning"] = st.warning;
        o["alarm"] = st.alarm;
    }
    }
}
void CloudClient::fillWatering_(JsonArray out)
{
    ScratchBuffer *scratch = scratch_();
    if (!scratch)
        return;
    {
        const auto scratch_guard = _scratch_lock.guard();
        auto guard = _controllers.watering().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            _log.warn(F("CLOUD"), F("Snapshot lock timeout: watering"));
            return;
        }
        for (size_t i = 0; i < WateringController::kRuleCount; ++i)
        {
            const auto *cfg = _controllers.watering().configByIndex(i);
            const auto *st = _controllers.watering().stateByIndex(i);
            scratch->watering[i].valid = false;
            if (!cfg || !st || !cfg->enabled)
                continue;
            scratch->watering[i].valid = true;
            scratch->watering[i].cfg = *cfg;
            scratch->watering[i].st = *st;
        }
        auto tank_guard = _controllers.tanks().lockGuard(kSnapshotLockTimeoutMs);
        if (tank_guard.locked())
        {
            for (size_t i = 0; i <= TankController::kTankCount; ++i)
                scratch->watering_tanks[i] = ScratchBuffer::TankNameItem{};
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = _controllers.tanks().configByIndex(i);
                if (!cfg || cfg->id == 0 || cfg->id > TankController::kTankCount || !cfg->name.length())
                    continue;
                scratch->watering_tanks[cfg->id].valid = true;
                scratch->watering_tanks[cfg->id].name = cfg->name;
            }
        }
        for (size_t i = 0; i < WateringController::kRuleCount; ++i)
        {
        const auto &item = scratch->watering[i];
        if (!item.valid)
            continue;
        const auto &cfg = item.cfg;
        const auto &st = item.st;
        JsonObject o = out.add<JsonObject>();
        o["id"] = (unsigned)cfg.id;
        o["enabled"] = cfg.enabled;
        o["status"] = st.status;
        if (cfg.name.length())
            o["name"] = cfg.name;
        if (cfg.port != WateringController::kInvalidPort)
            o["port"] = cfg.port;
        if (cfg.tank_id)
        {
            o["tank"] = cfg.tank_id;
            if (cfg.tank_id <= TankController::kTankCount && scratch->watering_tanks[cfg.tank_id].valid)
                o["tank_name"] = scratch->watering_tanks[cfg.tank_id].name;
        }
        if (cfg.weekdays_mask)
            o["weekdays_mask"] = cfg.weekdays_mask;
        if (cfg.duration_sec && cfg.hour <= 23 && cfg.minute <= 59)
        {
            o["hour"] = cfg.hour;
            o["minute"] = cfg.minute;
            o["duration_s"] = cfg.duration_sec;
        }
        if (cfg.duration2_sec && cfg.hour2 <= 23 && cfg.minute2 <= 59)
        {
            o["hour2"] = cfg.hour2;
            o["minute2"] = cfg.minute2;
            o["duration2_s"] = cfg.duration2_sec;
        }
        if (cfg.duration3_sec && cfg.hour3 <= 23 && cfg.minute3 <= 59)
        {
            o["hour3"] = cfg.hour3;
            o["minute3"] = cfg.minute3;
            o["duration3_s"] = cfg.duration3_sec;
        }
        o["resume"] = cfg.resume_after_refill;
        o["resume_level"] = cfg.resume_level;
        o["active"] = st.active;
        o["paused"] = st.paused;
        if (st.remaining_ms)
            o["remaining_ms"] = st.remaining_ms;
    }
    }
}
void CloudClient::fillSecurity_(JsonObject out)
{
    ScratchBuffer *scratch = scratch_();
    if (!scratch)
        return;
    bool controller_enabled = false;
    bool armed = false;
    bool alarm = false;
    uint8_t siren_port = SecurityController::kInvalidPort;
    {
        const auto scratch_guard = _scratch_lock.guard();
        auto guard = _controllers.security().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            _log.warn(F("CLOUD"), F("Snapshot lock timeout: security"));
            return;
        }
        controller_enabled = _controllers.security().controllerEnabled();
        armed = _controllers.security().armed();
        alarm = _controllers.security().alarmOn();
        siren_port = _controllers.security().sirenPort();
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = _controllers.security().configByIndex(i);
            const auto *st = _controllers.security().stateByIndex(i);
            scratch->security[i].valid = false;
            if (!cfg || !st || !cfg->enabled)
                continue;
            scratch->security[i].valid = true;
            scratch->security[i].cfg = *cfg;
            scratch->security[i].st = *st;
        }
        out["enabled"] = controller_enabled;
        out["armed"] = armed;
        out["alarm"] = alarm;
        if (siren_port != SecurityController::kInvalidPort)
            out["siren"] = (unsigned)siren_port;

        JsonArray arr = out["sensors"].to<JsonArray>();
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
        const auto &item = scratch->security[i];
        if (!item.valid)
            continue;
        const auto &cfg = item.cfg;
        const auto &st = item.st;
        JsonObject o = arr.add<JsonObject>();
        o["id"] = (unsigned)cfg.id;
        o["group_id"] = (unsigned)cfg.group_id;
        o["enabled"] = cfg.enabled;
        o["type"] = (cfg.type == SecurityController::SensorType::Reed) ? "reed" : "pir";
        if (cfg.port != SecurityController::kInvalidPort)
            o["port"] = cfg.port;
        if (cfg.name.length())
            o["name"] = cfg.name;
        o["silent"] = cfg.silent;
        o["detect"] = st.active;
    }
    }
}
void CloudClient::fillRing_(JsonObject out)
{
    RingController::Config cfg{};
    RingController::State st{};
    {
        auto guard = _controllers.ring().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            _log.warn(F("CLOUD"), F("Snapshot lock timeout: ring"));
            return;
        }
        cfg = _controllers.ring().config();
        st = _controllers.ring().state();
    }
    out["enabled"] = cfg.enabled;
    if (cfg.button_port != RingController::kInvalidPort)
        out["button"] = cfg.button_port;
    if (cfg.relay_port != RingController::kInvalidPort)
        out["relay"] = cfg.relay_port;
    out["relay_on"] = st.relay_on;
}
void CloudClient::fillAvr_(JsonObject out)
{
    AvrController::Config cfg{};
    AvrController::State st{};
    {
        auto guard = _controllers.avr().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            static uint32_t last_warn_ms = 0;
            const uint32_t now = millis();
            if (last_warn_ms == 0 || (uint32_t)(now - last_warn_ms) >= kSnapshotWarnIntervalMs)
            {
                last_warn_ms = now;
#if RTOS_LOCK_DIAG
                const char *owner = _controllers.avr().lockOwnerName();
                const uint32_t held_ms = _controllers.avr().lockHeldMs();
                _log.warn(F("CLOUD"), F("Snapshot lock timeout: avr owner: %s held_ms: %lu"),
                          owner ? owner : "-", (unsigned long)held_ms);
#else
                _log.warn(F("CLOUD"), F("Snapshot lock timeout: avr"));
#endif
            }
            return;
        }
        cfg = _controllers.avr().config();
        st = _controllers.avr().state();
    }
    out["enabled"] = cfg.enabled;
    out["auto_mode"] = cfg.auto_mode;
    out["prefer_main"] = cfg.prefer_main;
    out["auto_return_main"] = cfg.auto_return_main;
    if (cfg.main_ok_port != AvrController::kInvalidPort)
        out["main_ok_port"] = cfg.main_ok_port;
    if (cfg.reserve_ok_port != AvrController::kInvalidPort)
        out["reserve_ok_port"] = cfg.reserve_ok_port;
    if (cfg.relay_main_port != AvrController::kInvalidPort)
        out["relay_main_port"] = cfg.relay_main_port;
    if (cfg.relay_reserve_port != AvrController::kInvalidPort)
        out["relay_reserve_port"] = cfg.relay_reserve_port;
    if (cfg.feedback_main_port != AvrController::kInvalidPort)
        out["feedback_main_port"] = cfg.feedback_main_port;
    if (cfg.feedback_reserve_port != AvrController::kInvalidPort)
        out["feedback_reserve_port"] = cfg.feedback_reserve_port;
    out["main_ok"] = st.main_ok;
    out["reserve_ok"] = st.reserve_ok;
    out["relay_main_on"] = st.relay_main_on;
    out["relay_reserve_on"] = st.relay_reserve_on;
    out["active_source"] = AvrController::sourceName(st.active_source);
    out["target_source"] = AvrController::sourceName(st.target_source);
    out["fault"] = AvrController::faultName(st.fault);
    out["transfer"] = st.transfer_in_progress;
}
void CloudClient::fillLeak_(JsonArray out)
{
    ScratchBuffer *scratch = scratch_();
    if (!scratch)
        return;
    {
        const auto scratch_guard = _scratch_lock.guard();
        auto guard = _controllers.leak().lockGuard(kSnapshotLockTimeoutMs);
        if (!guard.locked())
        {
            _log.warn(F("CLOUD"), F("Snapshot lock timeout: leak"));
            return;
        }
        for (size_t i = 0; i < LeakController::kZoneCount; ++i)
        {
            const auto *cfg = _controllers.leak().configByIndex(i);
            const auto *st = _controllers.leak().stateByIndex(i);
            scratch->leak[i].valid = false;
            if (!cfg || !st || !cfg->enabled)
                continue;
            scratch->leak[i].valid = true;
            scratch->leak[i].cfg = *cfg;
            scratch->leak[i].st = *st;
        }
        for (size_t i = 0; i < LeakController::kZoneCount; ++i)
        {
        const auto &item = scratch->leak[i];
        if (!item.valid)
            continue;
        const auto &cfg = item.cfg;
        const auto &st = item.st;
        JsonObject o = out.add<JsonObject>();
        o["id"] = (unsigned)cfg.id;
        o["enabled"] = cfg.enabled;
        o["power_on"] = cfg.power_on;
        o["sensor_active_low"] = cfg.sensor_active_low;
        if (cfg.sensor_port != LeakController::kInvalidPort)
            o["sensor"] = cfg.sensor_port;
        if (cfg.valve_port != LeakController::kInvalidPort)
            o["valve"] = cfg.valve_port;
        if (cfg.alarm_port != LeakController::kInvalidPort)
            o["alarm"] = cfg.alarm_port;
        if (cfg.name.length())
            o["name"] = cfg.name;
        o["wet"] = st.wet;
        o["alarm_latched"] = st.alarm_latched;
    }
    }
}
void CloudClient::fillStackInfo_(JsonObject out)
{
    out["role"] = stackRoleName_();
    out["node_id"] = _network ? _network->stackLocalNodeId() : deviceId_();
    JsonArray nodes = out["nodes"].to<JsonArray>();
    if (!isStackMaster_() || !_network)
        return;
    const size_t count = _network->stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!_network->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
            continue;
        if ((device.caps & kStackCapController) == 0)
            continue;
        JsonObject n = nodes.add<JsonObject>();
        n["node_id"] = device.node_id;
        n["name"] = sanitizeUtf8_(String(device.name));
        n["online"] = true;
        n["last_seen_ms"] = device.last_seen_ms;
    }
}
bool CloudClient::fillStackCachedSystem_(JsonObject out, uint32_t node_id)
{
    const uint32_t now = millis();
    const uint32_t stale_ms = 15000;
    out["device_name"] = sanitizeUtf8_(stackNodeName_(node_id));
    if (!_network)
        return false;
    bool has_any = false;
    StackUnitSnapshot::State status{};
    if (_network->stackIndexState(node_id, status))
    {
        if (status.has_plc)
        {
            JsonObject plc = out["plc"].to<JsonObject>();
            plc["board_temp"] = status.board_temp;
            JsonObject fan = out["fan"].to<JsonObject>();
            fan["fan_on"] = status.fan_on;
            has_any = true;
        }
        if (status.has_rtc)
        {
            JsonObject rtc = out["rtc"].to<JsonObject>();
            rtc["date"] = status.rtc_date;
            rtc["time"] = status.rtc_time;
            rtc["temp_c"] = status.rtc_temp;
            has_any = true;
        }
    }
    const bool request_ready = _network->prepareStackIndexStateRequest(node_id, now, stale_ms, kStackTimeoutMs);
    if (request_ready)
    {
        const bool sent = _network->stackRoute().sendRequest(node_id, "system", "snapshot_req", nullptr,
                                                             StackRouteAdapter::Mode::Json, true);
        (void)sent;
    }
    return has_any;
}
bool CloudClient::fillStackCachedControllers_(JsonObject out, uint32_t node_id)
{
    if (!_network)
        return false;
    bool has_any = false;
    const uint32_t now = millis();
    const uint32_t stale_ms = 15000;
    StackUnitSnapshot::State snapshot{};
    StackUnitSnapshot::CacheState cache{};
    if (_network->stackIndexState(node_id, snapshot) && _network->stackIndexCacheState(node_id, cache))
    {
            if (cache.socket_count > 0)
            {
                JsonArray sockets = out.createNestedArray("sockets");
                _network->forEachStackSocket(node_id, cache.socket_count, [&](uint8_t, const StackUnitSnapshot::SocketItem &it) {
                    if (it.id == 0)
                        return;
                    JsonObject o = sockets.add<JsonObject>();
                    o["id"] = it.id;
                    o["enabled"] = it.enabled;
                    o["state"] = it.state;
                    if (it.name[0])
                        o["name"] = sanitizeUtf8_(String(it.name));
                });
                has_any = true;
            }
            else
            {
                JsonObject sockets = out.createNestedObject("sockets");
                sockets["enabled_count"] = snapshot.sockets_enabled;
                sockets["on_count"] = snapshot.sockets_on;
                has_any = has_any || (snapshot.sockets_enabled > 0);
            }

            if (cache.light_count > 0)
            {
                JsonArray lights = out.createNestedArray("lights");
                _network->forEachStackLight(node_id, cache.light_count, [&](uint8_t, const StackUnitSnapshot::SocketItem &it) {
                    if (it.id == 0)
                        return;
                    JsonObject o = lights.add<JsonObject>();
                    o["id"] = it.id;
                    o["group_id"] = it.group_id;
                    o["enabled"] = it.enabled;
                    if (it.name[0])
                        o["name"] = sanitizeUtf8_(String(it.name));
                    if (it.button_port != SocketController::kInvalidPort)
                        o["button"] = it.button_port;
                    if (it.relay_port != SocketController::kInvalidPort)
                        o["relay"] = it.relay_port;
                    o["state"] = it.state;
                });
                has_any = true;
            }
            else
            {
                JsonObject lights = out.createNestedObject("lights");
                lights["enabled_count"] = snapshot.lights_enabled;
                lights["on_count"] = snapshot.lights_on;
                has_any = has_any || (snapshot.lights_enabled > 0);
            }

            if (cache.meteo_count > 0)
            {
                JsonArray meteo = out.createNestedArray("meteo");
                _network->forEachStackMeteo(node_id, cache.meteo_count, [&](uint8_t, const StackUnitSnapshot::MeteoItem &it) {
                    if (it.id == 0)
                        return;
                    JsonObject o = meteo.add<JsonObject>();
                    o["id"] = it.id;
                    o["group_id"] = it.group_id;
                    o["enabled"] = it.enabled;
                    o["type"] = MeteoController::typeName((MeteoController::SensorType)it.type);
                    o["type_id"] = it.type;
                    if (it.name[0])
                        o["name"] = sanitizeUtf8_(String(it.name));
                    if (it.dht_pin != MeteoController::kInvalidPin)
                        o["pin"] = it.dht_pin;
                    if (it.ds18_addr_set)
                    {
                        char hex[17] = {};
                        MeteoController::formatHexAddr(it.ds18_addr, hex);
                        o["addr"] = hex;
                    }
                    if (it.source_node_id != 0)
                        o["src_node"] = (unsigned long)it.source_node_id;
                    if (it.source_sensor_id != 0)
                        o["src_sensor"] = it.source_sensor_id;
                    if (it.has_temp)
                        o["temp_c"] = it.temp_c;
                    if (it.has_humidity)
                        o["hum"] = it.humidity;
                    o["has_temp"] = it.has_temp;
                    o["has_hum"] = it.has_humidity;
                    o["has_read"] = it.has_read;
                    o["ok"] = it.ok;
                    o["age_s"] = it.age_s;
                });
                has_any = true;
            }
            else
            {
                JsonObject meteo = out.createNestedObject("meteo");
                meteo["enabled_count"] = snapshot.meteo_enabled;
                meteo["ok_count"] = snapshot.meteo_ok;
                has_any = has_any || (snapshot.meteo_enabled > 0);
            }

            if (cache.thermo_count > 0)
            {
                JsonArray thermo = out.createNestedArray("thermo");
                _network->forEachStackThermo(node_id, cache.thermo_count, [&](uint8_t, const StackUnitSnapshot::ThermoItem &it) {
                    if (it.id == 0)
                        return;
                    JsonObject o = thermo.add<JsonObject>();
                    o["id"] = it.id;
                    o["group_id"] = it.group_id;
                    o["enabled"] = it.enabled;
                    o["sensor_id"] = it.sensor_id;
                    if (it.sensor_node_id != 0)
                        o["sensor_node_id"] = (unsigned long)it.sensor_node_id;
                    if (it.heat_port != ThermoController::kInvalidPort)
                        o["heat_port"] = it.heat_port;
                    if (it.cool_port != ThermoController::kInvalidPort)
                        o["cool_port"] = it.cool_port;
                    if (it.button_port != ThermoController::kInvalidPort)
                        o["button_port"] = it.button_port;
                    o["mode_id"] = it.mode;
                    o["mode"] = ThermoController::modeName((ThermoController::Mode)it.mode);
                    o["target_c"] = it.target_c;
                    o["hyst"] = it.hysteresis;
                    o["power_on"] = it.power_on;
                    o["heat_on"] = it.heat_on;
                    o["cool_on"] = it.cool_on;
                    if (it.name[0])
                        o["name"] = sanitizeUtf8_(String(it.name));
                });
                has_any = true;
            }
            else
            {
                JsonObject thermo = out.createNestedObject("thermo");
                thermo["enabled_count"] = snapshot.thermo_enabled;
                thermo["active_count"] = snapshot.thermo_active;
                has_any = has_any || (snapshot.thermo_enabled > 0);
            }

            JsonObject tanks = out.createNestedObject("tanks");
            tanks["enabled_count"] = snapshot.tanks_enabled;
            tanks["alert_count"] = snapshot.tanks_alert;
            has_any = has_any || (snapshot.tanks_enabled > 0);

            JsonObject septic = out.createNestedObject("septic");
            septic["enabled_count"] = snapshot.septic_enabled;
            septic["alert_count"] = snapshot.septic_alert;
            has_any = has_any || (snapshot.septic_enabled > 0);

            JsonObject watering = out.createNestedObject("watering");
            watering["enabled_count"] = snapshot.watering_enabled;
            watering["active_count"] = snapshot.watering_active;
            has_any = has_any || (snapshot.watering_enabled > 0);

            JsonObject security = out.createNestedObject("security");
            security["enabled"] = snapshot.security_enabled;
            security["armed"] = snapshot.security_armed;
            security["alarm"] = snapshot.security_alarm;
            security["sensors_enabled"] = snapshot.security_sensors_enabled;
            has_any = has_any || snapshot.security_enabled || (snapshot.security_sensors_enabled > 0);

            JsonObject ring = out.createNestedObject("ring");
            ring["enabled"] = snapshot.ring_enabled;
            ring["relay_on"] = snapshot.ring_on;
            has_any = has_any || snapshot.ring_enabled;

            JsonObject avr = out.createNestedObject("avr");
            avr["enabled"] = snapshot.avr_enabled;
            avr["fault"] = snapshot.avr_fault;
            avr["active_source_id"] = snapshot.avr_active_source;
            avr["active_source"] = snapshot.avr_active_source == (uint8_t)AvrController::Source::Main
                                       ? "main"
                                       : snapshot.avr_active_source == (uint8_t)AvrController::Source::Reserve ? "reserve"
                                                                                                                 : "off";
            has_any = has_any || snapshot.avr_enabled;

            JsonObject leak = out.createNestedObject("leak");
            leak["enabled_count"] = snapshot.leak_enabled;
            leak["alert_count"] = snapshot.leak_alert;
            has_any = has_any || (snapshot.leak_enabled > 0);
    }
    const bool request_ready = _network->prepareStackIndexStateRequest(node_id, now, stale_ms, kStackTimeoutMs);
    if (request_ready)
    {
        _network->stackRoute().sendRequest(node_id, "controllers", "summary_req", nullptr,
                                           StackRouteAdapter::Mode::Json, true);
    }
    if (snapshot.sockets_enabled > 0 && cache.socket_count < snapshot.sockets_enabled)
    {
        if (_network->prepareStackPageRequest(StackUnitSnapshot::PageKind::Sockets, node_id, now, cache.socket_count,
                                             4000u))
        {
            DynamicJsonDocument req(64);
            req["offset"] = cache.socket_count;
            req["limit"] = StackUnitSnapshot::kPageSize;
            _network->stackRoute().sendRequest(node_id, "sockets", "snapshot_req", &req,
                                               StackRouteAdapter::Mode::Json, true);
        }
    }
    if (snapshot.lights_enabled > 0 && cache.light_count < snapshot.lights_enabled)
    {
        if (_network->prepareStackPageRequest(StackUnitSnapshot::PageKind::Lights, node_id, now, cache.light_count,
                                             4000u))
        {
            DynamicJsonDocument req(64);
            req["offset"] = cache.light_count;
            req["limit"] = StackUnitSnapshot::kPageSize;
            _network->stackRoute().sendRequest(node_id, "lights", "snapshot_req", &req,
                                               StackRouteAdapter::Mode::Json, true);
        }
    }
    if (snapshot.meteo_enabled > 0 && cache.meteo_count < snapshot.meteo_enabled)
    {
        if (_network->prepareStackPageRequest(StackUnitSnapshot::PageKind::Meteo, node_id, now, cache.meteo_count,
                                             4000u))
        {
            DynamicJsonDocument req(64);
            req["offset"] = cache.meteo_count;
            req["limit"] = StackUnitSnapshot::kPageSize;
            _network->stackRoute().sendRequest(node_id, "meteo", "snapshot_req", &req,
                                               StackRouteAdapter::Mode::Json, true);
        }
    }
    if (snapshot.thermo_enabled > 0 && cache.thermo_count < snapshot.thermo_enabled)
    {
        if (_network->prepareStackPageRequest(StackUnitSnapshot::PageKind::Thermo, node_id, now, cache.thermo_count,
                                             4000u))
        {
            DynamicJsonDocument req(64);
            req["offset"] = cache.thermo_count;
            req["limit"] = StackUnitSnapshot::kPageSize;
            _network->stackRoute().sendRequest(node_id, "thermo", "snapshot_req", &req,
                                               StackRouteAdapter::Mode::Json, true);
        }
    }
    return has_any;
}
void CloudClient::maybeSendPeriodicEvent_()
{
    const uint32_t now = millis();
    if (_next_event_ms != 0 && (int32_t)(now - _next_event_ms) < 0)
        return;
    _next_event_ms = now + _event_interval_ms;
    if (!_session_id.length())
        return;

    DynamicJsonDocument doc(kWsDocCapacity);
    doc["v"] = kProtoVersion;
    doc["type"] = "event";
    doc["id"] = nextWsId_();
    doc["session_id"] = _session_id;
    JsonObject payload = doc["payload"].to<JsonObject>();
    payload["kind"] = "periodic";
    payload["reason"] = "periodic";
    JsonObject data = payload["data"].to<JsonObject>();
    fillSystemInfo_(data.createNestedObject("system"));
    fillControllersInfo_(data.createNestedObject("controllers"));
    sendJson_(doc);
}
void CloudClient::handlePendingTimeouts_()
{
    // Stack cloud path is now snapshot/route based and no longer uses legacy pending CmdGet/Ack state.
}
void CloudClient::clearPending_()
{
    // No-op: legacy pending state removed together with old stack cache/cmd path.
}
bool CloudClient::hasWhat_(JsonArrayConst what, const char *name)
{
    if (what.isNull() || !name)
        return false;
    for (JsonVariantConst v : what)
    {
        if (!v.is<const char *>())
            continue;
        if (strcmp(v.as<const char *>(), name) == 0)
            return true;
    }
    return false;
}
const char *CloudClient::transportName_() const
{
    return (_cfg.transport == CloudTransportKind::Http) ? "HTTP" : "WS";
}
const char *CloudClient::stackRoleName_() const
{
    if (!_configs)
        return "master";
    const auto role = _configs->stackRole();
    return (role == ConfigsManagerIface::StackRole::Master) ? "master" : "slave";
}
bool CloudClient::isStackMaster_() const
{
    if (!_configs)
        return true;
    return _configs->stackRole() == ConfigsManagerIface::StackRole::Master;
}
uint32_t CloudClient::deviceId_() const
{
    return (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFu);
}
String CloudClient::localIp_() const
{
    if (_wifi.staEnabled() && WiFi.status() == WL_CONNECTED)
        return WiFi.localIP().toString();
    if (_wifi.apEnabled())
        return WiFi.softAPIP().toString();
    return String();
}
String CloudClient::stackNodeName_(uint32_t node_id) const
{
    if (_stack_node_name_cb)
    {
        String out;
        if (_stack_node_name_cb(_stack_node_name_ctx, node_id, out) && out.length())
            return sanitizeUtf8_(out);
    }
    if (_network)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (_network->stackDeviceSnapshotByNodeId(node_id, device) && device.name[0])
            return sanitizeUtf8_(String(device.name));
    }
    return String();
}
String CloudClient::eventSourceName_(const String &unit, uint32_t node_id) const
{
    if (unit == "stack" && node_id != 0)
    {
        const String name = stackNodeName_(node_id);
        if (name.length())
            return name;
    }
    return _plc.deviceName();
}
ThermoController::Mode CloudClient::parseThermoMode_(const String &mode)
{
    String m = mode;
    m.toLowerCase();
    if (m == "heat" || m == "heat_only" || m == "only_heat")
        return ThermoController::Mode::Heat;
    if (m == "cool" || m == "cool_only" || m == "only_cool")
        return ThermoController::Mode::Cool;
    if (m == "auto")
        return ThermoController::Mode::Auto;
    return ThermoController::Mode::Off;
}
String CloudClient::nextWsId_()
{
    static uint32_t seq = 0;
    return String("ws") + String(++seq);
}
void CloudClient::sendJson_(JsonDocument &doc)
{
    String out;
    serializeJson(doc, out);
    if (out.length())
    {
        if (!_transport || !_transport->sendText(out))
            _log.warn(F("CLOUD"), F("Transport tx failed"));
    }
    else
    {
        _log.warn(F("CLOUD"), F("WS tx skipped: empty json"));
    }
}
String CloudClient::sanitizeUtf8_(const String &in)
{
    if (isValidUtf8_(in))
        return in;
    return cp1251ToUtf8_(in);
}
bool CloudClient::isValidUtf8_(const String &in)
{
    size_t i = 0;
    while (i < (size_t)in.length())
    {
        const uint8_t c = (uint8_t)in[i];
        if (c < 0x80)
        {
            ++i;
            continue;
        }
        size_t need = 0;
        if ((c & 0xE0) == 0xC0)
        {
            if (c < 0xC2)
                return false;
            need = 1;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            need = 2;
        }
        else if ((c & 0xF8) == 0xF0)
        {
            if (c > 0xF4)
                return false;
            need = 3;
        }
        else
        {
            return false;
        }

        if (i + need >= (size_t)in.length())
            return false;

        for (size_t j = 1; j <= need; ++j)
        {
            const uint8_t cc = (uint8_t)in[i + j];
            if ((cc & 0xC0) != 0x80)
                return false;
        }
        i += need + 1;
    }
    return true;
}
void CloudClient::appendUtf8_(String &out, uint16_t code)
{
    if (code < 0x80)
    {
        out += (char)code;
        return;
    }
    if (code < 0x800)
    {
        out += (char)(0xC0 | (code >> 6));
        out += (char)(0x80 | (code & 0x3F));
        return;
    }
    out += (char)(0xE0 | (code >> 12));
    out += (char)(0x80 | ((code >> 6) & 0x3F));
    out += (char)(0x80 | (code & 0x3F));
}
String CloudClient::cp1251ToUtf8_(const String &in)
{
    String out;
    out.reserve(in.length() * 2);
    for (size_t i = 0; i < (size_t)in.length(); ++i)
    {
        const uint8_t c = (uint8_t)in[i];
        if (c < 0x80)
        {
            out += (char)c;
            continue;
        }
        uint16_t code = '?';
        if (c == 0xA8)
            code = 0x0401;
        else if (c == 0xB8)
            code = 0x0451;
        else if (c >= 0xC0 && c <= 0xFF)
            code = (uint16_t)(0x0410 + (c - 0xC0));
        else
            code = '?';
        appendUtf8_(out, code);
    }
    return out;
}
bool CloudClient::parseActor_(JsonObjectConst payload, CloudClient::ActorInfo &out) const
{
    JsonObjectConst actor = payload["actor"].as<JsonObjectConst>();
    if (actor.isNull())
        return false;
    out.uid = actor["uid"] | "";
    out.username = actor["username"] | "";
    out.plc_username = actor["plc_username"] | "";
    out.source = actor["source"] | "";
    out.session_id = actor["session_id"] | "";
    out.resolved_user = "";
    return out.plc_username.length() != 0 || out.username.length() != 0;
}
bool CloudClient::resolveActor_(CloudClient::ActorInfo &actor) const
{
    if (!_users)
        return false;
    String key = actor.plc_username;
    if (key.length() == 0)
        key = actor.username;
    key = UsersRegistry::normalizeUsername(key);
    if (key.length() == 0)
        return false;
    for (size_t i = 0; i < _users->size(); ++i)
    {
        const auto &u = _users->user(i);
        if (!u.enabled || u.username.length() == 0)
            continue;
        if (UsersRegistry::normalizeUsername(u.username) != key)
            continue;
        actor.resolved_user = u.username;
        actor.resolved_idx = (uint8_t)i;
        actor.is_admin = u.tg_admin;
        return true;
    }
    return false;
}
uint8_t CloudClient::aclUnitByNodeId_(uint32_t node_id) const
{
    if (node_id == 0)
        return 0;
    if (!_network)
        return UsersRegistry::kAclUnitCount;
    const size_t count = _network->stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!_network->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
            continue;
        if (device.node_id == node_id)
        {
            const size_t unit = i + 1u;
            if (unit >= (size_t)UsersRegistry::kAclUnitCount)
                return UsersRegistry::kAclUnitCount;
            return (uint8_t)unit;
        }
    }
    return UsersRegistry::kAclUnitCount;
}
bool CloudClient::aclControllerByName_(const String &ctrl, UsersRegistry::AclController &out)
{
    if (ctrl == "sockets")
        out = UsersRegistry::AclController::Sockets;
    else if (ctrl == "lights")
        out = UsersRegistry::AclController::Lights;
    else if (ctrl == "meteo")
        out = UsersRegistry::AclController::Meteo;
    else if (ctrl == "thermo")
        out = UsersRegistry::AclController::Thermo;
    else if (ctrl == "tanks")
        out = UsersRegistry::AclController::Tanks;
    else if (ctrl == "septic")
        out = UsersRegistry::AclController::Septic;
    else if (ctrl == "security")
        out = UsersRegistry::AclController::Security;
    else if (ctrl == "watering")
        out = UsersRegistry::AclController::Watering;
    else if (ctrl == "leak")
        out = UsersRegistry::AclController::Leak;
    else if (ctrl == "avr")
        out = UsersRegistry::AclController::Avr;
    else if (ctrl == "ring")
        out = UsersRegistry::AclController::Ring;
    else
        return false;
    return true;
}
uint16_t CloudClient::aclItemIdForCmd_(const String &ctrl, const String &action, JsonObjectConst args)
{
    const uint16_t id = (uint16_t)(args["id"] | 0);
    if (ctrl == "avr" || ctrl == "ring")
        return 1;
    if (ctrl == "security" && (action == "arm" || action == "disarm" || action == "clear"))
        return 1;
    if (ctrl == "septic")
        return id ? id : 1;
    if (ctrl == "leak" && action == "ack_all")
        return 1;
    return id;
}
bool CloudClient::aclCanControl_(const ActorInfo &actor, const String &ctrl, const String &action,
                                 JsonObjectConst args, uint32_t node_id) const
{
    if (!_users || actor.resolved_idx == 0xFF)
        return false;
    const auto &u = _users->user(actor.resolved_idx);
    if (!u.enabled)
        return false;
    if (actor.is_admin)
        return true;

    UsersRegistry::AclController acl_ctrl = UsersRegistry::AclController::Sockets;
    if (!aclControllerByName_(ctrl, acl_ctrl))
        return false;
    const uint8_t unit = aclUnitByNodeId_(node_id);
    if (unit >= UsersRegistry::kAclUnitCount)
        return false;
    const uint16_t item_id = aclItemIdForCmd_(ctrl, action, args);
    if (item_id != 0)
        return u.canControlItem(unit, acl_ctrl, item_id);
    return u.controllerAllowed(unit, acl_ctrl);
}
