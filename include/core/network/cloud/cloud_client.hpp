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
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <string.h>

#include "boards/board_profile.hpp"
#include "controllers/controllers.hpp"
#include "core/network/gsm_modem.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"

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

    CloudClient(Logger &log, Controllers &controllers, PlcControl &plc, WifiManager &wifi, RTC &rtc)
        : _log(log),
          _controllers(controllers),
          _plc(plc),
          _wifi(wifi),
          _rtc(rtc)
    {
    }

    void setGsm(GsmModem *gsm) { _gsm = gsm; }
    void setStackMaster(StackMaster *master) { _stack_master = master; }
    void setConfigsManager(ConfigsManagerIface *cfg) { _configs = cfg; }

    void setEnabled(bool enabled)
    {
        if (enabled == _enabled)
            return;
        _enabled = enabled;
        if (!_enabled)
        {
            _log.info(F("CLOUD"), F("Disabled"));
            _ws.disconnect();
            _session_id = "";
            clearPending_();
        }
        else
        {
            _log.info(F("CLOUD"), F("Enabled"));
        }
    }
    bool enabled() const { return _enabled; }
    bool isConnected() const { return const_cast<WebSocketsClient &>(_ws).isConnected(); }
    void disconnect()
    {
        _ws.disconnect();
        _session_id = "";
        clearPending_();
    }

    void setApiKey(const String &key) { _api_key = key; }
    void setFirmwareVersion(const String &ver) { _fw_version = ver; }
    void setAutoEventIntervalMs(uint32_t ms) { _event_interval_ms = ms; }

    void begin(const Config &cfg)
    {
        if (!_enabled)
            return;
        _cfg = cfg;
        if (_cfg.path.length() == 0)
            _cfg.path = "/";
        _log.info(F("CLOUD"), F("WS begin: %s:%u%s%s"),
                  _cfg.host.c_str(), _cfg.port,
                  _cfg.use_ssl ? " ssl " : " ",
                  _cfg.path.c_str());
        if (_cfg.use_ssl)
            _ws.beginSSL(_cfg.host.c_str(), _cfg.port, _cfg.path.c_str());
        else
            _ws.begin(_cfg.host.c_str(), _cfg.port, _cfg.path.c_str());
        _ws.onEvent([this](WStype_t t, uint8_t *p, size_t l) { onWsEvent_(t, p, l); });
        _ws.setReconnectInterval(_cfg.reconnect_ms);
        if (_stack_master)
            _stack_master->setFrameHandlerSecondary(&CloudClient::onStackFrame_, this);
    }

    void loop()
    {
        if (!_enabled)
            return;
        _ws.loop();
        maintainConnectionHealth_();
        handlePendingTimeouts_();
        if (_event_interval_ms)
            maybeSendPeriodicEvent_();
    }

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

    void onWsEvent_(WStype_t type, uint8_t *payload, size_t len)
    {
        switch (type)
        {
        case WStype_CONNECTED:
            _session_id = "";
            _last_connect_ms = millis();
            _last_rx_ms = _last_connect_ms;
            _last_disconnect_ms = 0;
            if (_disconnect_reported)
                _log.info(F("CLOUD"), F("WS connection restored"));
            _disconnect_reported = false;
            _log.info(F("CLOUD"), F("WS connected"));
            sendHello_();
            break;
        case WStype_ERROR:
        {
            // WebSocketsClient doesn't expose structured error details here,
            // but payload sometimes contains a textual hint.
            if (payload && len)
            {
                String msg;
                msg.reserve(len + 1);
                for (size_t i = 0; i < len; ++i)
                    msg += (char)payload[i];
                _log.warn(F("CLOUD"), F("WS error: %s"), msg.c_str());
            }
            else
            {
                _log.warn(F("CLOUD"), F("WS error"));
            }
            break;
        }
        case WStype_TEXT:
            if (payload && len)
            {
                _last_rx_ms = millis();
                handleMessage_(payload, len);
            }
            break;
        case WStype_DISCONNECTED:
            _session_id = "";
            _last_disconnect_ms = millis();
            clearPending_();
            if (!_disconnect_reported)
            {
                _log.warn(F("CLOUD"), F("WS disconnected"));
                _disconnect_reported = true;
            }
            break;
        default:
            break;
        }
    }

    void handleMessage_(const uint8_t *payload, size_t len)
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

    void sendHello_()
    {
        _last_hello_ms = millis();
        DynamicJsonDocument doc(2048);
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

        JsonObject stack = payload["stack"].to<JsonObject>();
        stack["role"] = stackRoleName_();
        stack["node_id"] = deviceId_();
        JsonArray nodes = stack["nodes"].to<JsonArray>();
        if (isStackMaster_() && _stack_master)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                if (!_stack_master->nodeIsControllerAt(i))
                    continue;
                JsonObject n = nodes.add<JsonObject>();
                n["node_id"] = _stack_master->nodeIdAt(i);
                n["name"] = _stack_master->nodeNameAt(i);
                n["online"] = true;
                n["last_seen_ms"] = 0;
            }
        }

        String dbg;
        serializeJson(doc, dbg);
        sendJson_(doc);
    }

    void maintainConnectionHealth_()
    {
        const uint32_t now = millis();
        const bool connected = isConnected();
        if (!connected)
        {
            if (_last_disconnect_ms == 0)
                _last_disconnect_ms = now;
            if (_cfg.host.length() &&
                (int32_t)(now - _last_disconnect_ms) >= (int32_t)kWsReinitDisconnectedMs)
            {
                _log.warn(F("CLOUD"), F("WS reconnect stalled, reinit"));
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
                _ws.disconnect();
                return;
            }
        }

        if (_last_rx_ms != 0 &&
            (int32_t)(now - _last_rx_ms) >= (int32_t)kWsSilentTimeoutMs)
        {
            _log.warn(F("CLOUD"), F("WS silent timeout, reconnect"));
            _ws.disconnect();
        }
    }

    void sendPong_(const String &reply_to, JsonVariantConst payload)
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

    void handleGet_(const String &req_id, JsonDocument &doc)
    {
        const String unit = doc["unit"] | "local";
        JsonArrayConst what = doc["payload"]["what"].as<JsonArrayConst>();
        if (unit == "stack")
        {
            const uint32_t node_id = doc["node_id"] | 0;
            if (!node_id)
            {
                sendError_(req_id, "missing node_id");
                return;
            }
            handleGetStack_(req_id, node_id, what);
            return;
        }
        handleGetLocal_(req_id, what);
    }

    void handleGetLocal_(const String &req_id, JsonArrayConst what)
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

        sendJson_(out);
    }

    void handleGetStack_(const String &req_id, uint32_t node_id, JsonArrayConst what)
    {
        PendingRequest *p = allocPending_(req_id, node_id);
        if (!p)
        {
            sendError_(req_id, "pending overflow");
            return;
        }
        p->want_system = hasWhat_(what, "system");
        p->want_controllers = hasWhat_(what, "controllers");

        if (p->want_system)
            scheduleStackSystem_(p);
        if (p->want_controllers)
            scheduleStackControllers_(p);

        if (p->pending_mask == 0)
        {
            finalizePending_(p, true, "");
        }
    }

    void handleCmd_(const String &req_id, JsonDocument &doc)
    {
        const String unit = doc["unit"] | "local";
        JsonObjectConst payload = doc["payload"].as<JsonObjectConst>();
        const String ctrl = payload["controller"] | "";
        const String action = payload["action"] | "";
        JsonObjectConst args = payload["args"].as<JsonObjectConst>();

        if (unit == "stack")
        {
            const uint32_t node_id = doc["node_id"] | 0;
            if (!node_id)
            {
                sendError_(req_id, "missing node_id");
                return;
            }
            handleCmdStack_(req_id, node_id, ctrl, action, args);
            return;
        }
        handleCmdLocal_(req_id, ctrl, action, args);
    }

    void handleCmdLocal_(const String &req_id, const String &ctrl, const String &action, JsonObjectConst args)
    {
        bool ok = false;
        if (ctrl == "sockets")
            ok = handleCmdSockets_(_controllers.sockets(), action, args, false);
        else if (ctrl == "lights")
            ok = handleCmdSockets_(_controllers.sockets(), action, args, true);
        else if (ctrl == "thermo")
            ok = handleCmdThermo_(action, args);
        else if (ctrl == "tanks")
            ok = handleCmdTanks_(action, args);
        else if (ctrl == "septic")
            ok = handleCmdSeptic_(action, args);
        else if (ctrl == "watering")
            ok = handleCmdWatering_(action, args);
        else if (ctrl == "security")
            ok = handleCmdSecurity_(action, args);
        else if (ctrl == "ring")
            ok = handleCmdRing_(action, args);
        else if (ctrl == "avr")
            ok = handleCmdAvr_(action, args);
        else if (ctrl == "leak")
            ok = handleCmdLeak_(action, args);

        sendAck_(req_id, ok, ok ? "" : "failed");
    }

    void handleCmdStack_(const String &req_id, uint32_t node_id,
                         const String &ctrl, const String &action, JsonObjectConst args)
    {
        if (!_stack_master || !isStackMaster_())
        {
            sendError_(req_id, "stack master missing");
            return;
        }
        StackFeature feature = StackFeature::System;
        String stack_action;
        DynamicJsonDocument params(512);

        if (ctrl == "sockets")
        {
            feature = StackFeature::Sockets;
            stack_action = (action == "toggle") ? "set" : "set";
            const uint32_t item_id = (uint32_t)(args["id"] | 0);
            if (action == "toggle")
                _log.info(F("CLOUD"), F("Cmd stack: sockets node_id: %u id: %u action: toggle"),
                          (unsigned)node_id, (unsigned)item_id);
            else
                _log.info(F("CLOUD"), F("Cmd stack: sockets node_id: %u id: %u action: set state: %s"),
                          (unsigned)node_id, (unsigned)item_id,
                          (String(args["state"] | "") == "on") ? "on" : "off");
            JsonArray items = params["items"].to<JsonArray>();
            JsonObject o = items.add<JsonObject>();
            o["id"] = (unsigned)(args["id"] | 0);
            if (action == "toggle")
                o["toggle"] = true;
            else
                o["state"] = (String(args["state"] | "") == "on");
        }
        else if (ctrl == "lights")
        {
            feature = StackFeature::Sockets;
            stack_action = (action == "toggle") ? "set_lights" : "set_lights";
            const uint32_t item_id = (uint32_t)(args["id"] | 0);
            if (action == "toggle")
                _log.info(F("CLOUD"), F("Cmd stack: lights node_id: %u id: %u action: toggle"),
                          (unsigned)node_id, (unsigned)item_id);
            else
                _log.info(F("CLOUD"), F("Cmd stack: lights node_id: %u id: %u action: set state: %s"),
                          (unsigned)node_id, (unsigned)item_id,
                          (String(args["state"] | "") == "on") ? "on" : "off");
            JsonArray items = params["items"].to<JsonArray>();
            JsonObject o = items.add<JsonObject>();
            o["id"] = (unsigned)(args["id"] | 0);
            if (action == "toggle")
                o["toggle"] = true;
            else
                o["state"] = (String(args["state"] | "") == "on");
        }
        else if (ctrl == "thermo")
        {
            feature = StackFeature::Thermo;
            stack_action = "set";
            JsonArray items = params["items"].to<JsonArray>();
            JsonObject o = items.add<JsonObject>();
            o["id"] = (unsigned)(args["id"] | 0);
            if (action == "power")
                o["power"] = (String(args["state"] | "") == "on");
            else if (action == "mode")
                o["mode"] = args["mode"] | "";
            else if (action == "target")
                o["target"] = args["target_c"] | 0.0f;
        }
        else if (ctrl == "tanks")
        {
            feature = StackFeature::Tanks;
            stack_action = "set";
            JsonArray items = params["items"].to<JsonArray>();
            JsonObject o = items.add<JsonObject>();
            o["id"] = (unsigned)(args["id"] | 0);
            o["power_on"] = (String(args["state"] | "") == "on");
        }
        else if (ctrl == "septic")
        {
            feature = StackFeature::Septic;
            stack_action = "set";
            params["id"] = (unsigned)(args["id"] | 1);
            params["monitor"] = (String(args["state"] | "") == "on");
        }
        else if (ctrl == "watering")
        {
            feature = StackFeature::Watering;
            stack_action = "set";
            params["id"] = (unsigned)(args["id"] | 0);
            params["state"] = (String(args["state"] | "") == "on");
        }
        else if (ctrl == "security")
        {
            if (action == "rfid")
            {
                String uid = args["uid"] | "";
                if (uid.length() == 0)
                    uid = args["serial"] | "";
                if (!uid.length())
                {
                    sendError_(req_id, "missing uid");
                    return;
                }
                const String src = args["name"] | stackNodeName_(node_id);
                const bool ok = _controllers.security().processRfidUidString(uid.c_str(), src.c_str());
                sendAck_(req_id, ok, ok ? "" : "failed");
                return;
            }
            if (action == "ibutton")
            {
                const String serial = args["serial"] | "";
                if (!serial.length())
                {
                    sendError_(req_id, "missing serial");
                    return;
                }
                const String src = args["name"] | stackNodeName_(node_id);
                const bool ok = _controllers.security().processIButtonSerialString(serial.c_str(), src.c_str());
                sendAck_(req_id, ok, ok ? "" : "failed");
                return;
            }
            feature = StackFeature::Security;
            stack_action = "set";
            if (action == "arm")
                params["armed"] = true;
            else if (action == "disarm")
                params["armed"] = false;
            else if (action == "clear")
                params["clear"] = true;
        }
        else if (ctrl == "ring")
        {
            feature = StackFeature::Ring;
            stack_action = "set";
            params["state"] = (String(args["state"] | "") == "on");
        }
        else if (ctrl == "avr")
        {
            feature = StackFeature::Avr;
            stack_action = "set";
            if (action == "auto")
                params["auto_mode"] = (String(args["state"] | "") == "on");
            else if (action == "source")
                params["manual_source"] = args["source"] | "off";
            else if (action == "clear_fault")
                params["clear_fault"] = true;
            else
            {
                sendError_(req_id, "unsupported action");
                return;
            }
        }
        else if (ctrl == "leak")
        {
            feature = StackFeature::Leak;
            stack_action = "set";
            if (action == "power")
            {
                JsonArray zones = params["zones"].to<JsonArray>();
                JsonObject z = zones.add<JsonObject>();
                z["id"] = (unsigned)(args["id"] | 0);
                z["power_on"] = (String(args["state"] | "") == "on");
            }
            else if (action == "ack_all")
            {
                params["ack_all"] = true;
            }
            else if (action == "ack")
            {
                // Stack leak API supports only ack_all. Keep a dedicated action for cloud API;
                // remote execution falls back to ack_all.
                params["ack_all"] = true;
            }
            else
            {
                sendError_(req_id, "unsupported action");
                return;
            }
        }
        else
        {
            sendError_(req_id, "unknown controller");
            return;
        }

        PendingRequest *p = allocPending_(req_id, node_id);
        if (!p)
        {
            sendError_(req_id, "pending overflow");
            return;
        }
        p->pending_mask = maskFor_(StackPart::None);
        p->want_controllers = true;
        sendStackCmdSimple_(node_id, StackMsgType::CmdSet, feature, stack_action.c_str(), params);
    }

    bool handleCmdSockets_(SocketController &s, const String &action, JsonObjectConst args, bool lights)
    {
        const uint8_t id = (uint8_t)(args["id"] | 0);
        if (id == 0)
            return false;
        const char *ctrl_name = lights ? "lights" : "sockets";
        if (action == "toggle")
        {
            _log.info(F("CLOUD"), F("Cmd: %s id: %u action: toggle"), ctrl_name, (unsigned)id);
            return lights ? s.toggleLightRelayById(id) : s.toggleRelayById(id);
        }
        if (action == "set")
        {
            const String st = args["state"] | "";
            const bool on = (st == "on");
            _log.info(F("CLOUD"), F("Cmd: %s id: %u action: set state: %s"),
                      ctrl_name, (unsigned)id, on ? "on" : "off");
            return lights ? s.setLightRelayById(id, on) : s.setRelayById(id, on);
        }
        return false;
    }

    bool handleCmdThermo_(const String &action, JsonObjectConst args)
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

    bool handleCmdTanks_(const String &action, JsonObjectConst args)
    {
        if (action != "power")
            return false;
        const uint8_t id = (uint8_t)(args["id"] | 0);
        if (id == 0)
            return false;
        return _controllers.tanks().setPower(id, (String(args["state"] | "") == "on"));
    }

    bool handleCmdSeptic_(const String &action, JsonObjectConst args)
    {
        if (action != "monitor")
            return false;
        const uint8_t id = (uint8_t)(args["id"] | 1);
        return _controllers.septic().setMonitoring(id, (String(args["state"] | "") == "on"));
    }

    bool handleCmdWatering_(const String &action, JsonObjectConst args)
    {
        if (action != "status")
            return false;
        const uint8_t id = (uint8_t)(args["id"] | 0);
        if (id == 0)
            return false;
        return _controllers.watering().setStatus(id, (String(args["state"] | "") == "on"));
    }

    bool handleCmdSecurity_(const String &action, JsonObjectConst args)
    {
        if (action == "arm")
            return _controllers.security().armFrom("cloud", "");
        if (action == "disarm")
            return _controllers.security().disarmFrom("cloud", "");
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
            const String src = args["name"] | "cloud";
            return _controllers.security().processRfidUidString(uid.c_str(), src.c_str());
        }
        if (action == "ibutton")
        {
            const String serial = args["serial"] | "";
            if (!serial.length())
                return false;
            const String src = args["name"] | "cloud";
            return _controllers.security().processIButtonSerialString(serial.c_str(), src.c_str());
        }
        return false;
    }

    bool handleCmdRing_(const String &action, JsonObjectConst args)
    {
        if (action != "hold")
            return false;
        const bool on = (String(args["state"] | "") == "on");
        return _controllers.ring().setHoldRelayWithSource(on, RingController::Source::Web);
    }

    bool handleCmdAvr_(const String &action, JsonObjectConst args)
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

    bool handleCmdLeak_(const String &action, JsonObjectConst args)
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

    void sendAck_(const String &reply_to, bool ok, const char *error)
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

    void sendError_(const String &reply_to, const char *msg)
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

    void finalizePending_(PendingRequest *p, bool ok, const char *err)
    {
        if (!p || !p->used)
            return;
        DynamicJsonDocument out(kWsDocCapacity);
        out["v"] = kProtoVersion;
        out["type"] = "result";
        out["id"] = nextWsId_();
        out["reply_to"] = p->ws_id;
        if (_session_id.length())
            out["session_id"] = _session_id;
        out["payload"]["ok"] = ok;
        if (!ok && err)
            out["payload"]["error"] = err;
        if (p->doc)
            out["payload"]["data"] = p->doc->as<JsonVariantConst>();
        sendJson_(out);
        freePending_(p);
    }

    void scheduleStackSystem_(PendingRequest *p)
    {
        if (!p)
            return;
        ensurePendingDoc_(p);
        p->pending_mask |= maskFor_(StackPart::SystemInfo);
        p->pending_mask |= maskFor_(StackPart::PlcStatus);
        p->pending_mask |= maskFor_(StackPart::FanStatus);
        p->pending_mask |= maskFor_(StackPart::RtcTime);
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::System, "get_info");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::PlcStatus, "get");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Fan, "get_status");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Rtc, "get_time");
    }

    void scheduleStackControllers_(PendingRequest *p)
    {
        if (!p)
            return;
        ensurePendingDoc_(p);
        p->pending_mask |= maskFor_(StackPart::Sockets);
        p->pending_mask |= maskFor_(StackPart::Lights);
        p->pending_mask |= maskFor_(StackPart::Meteo);
        p->pending_mask |= maskFor_(StackPart::Thermo);
        p->pending_mask |= maskFor_(StackPart::Tanks);
        p->pending_mask |= maskFor_(StackPart::Septic);
        p->pending_mask |= maskFor_(StackPart::Watering);
        p->pending_mask |= maskFor_(StackPart::SecurityStatus);
        p->pending_mask |= maskFor_(StackPart::SecuritySensors);
        p->pending_mask |= maskFor_(StackPart::Ring);
        p->pending_mask |= maskFor_(StackPart::Avr);
        p->pending_mask |= maskFor_(StackPart::Leak);

        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Sockets, "get");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Sockets, "get_lights");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Meteo, "get");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Thermo, "get");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Tanks, "get");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Septic, "get");
        DynamicJsonDocument watering_params(64);
        watering_params["offset"] = 0;
        watering_params["limit"] = (unsigned)WateringController::kRuleCount;
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Watering, "get", watering_params);
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Security, "status");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Security, "get");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Ring, "get");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Avr, "get");
        sendStackCmd_(p->node_id, StackMsgType::CmdGet, StackFeature::Leak, "get");
    }

    void sendStackCmd_(uint32_t node_id, StackMsgType type, StackFeature feature,
                       const char *action)
    {
        if (!_stack_master)
            return;
        DynamicJsonDocument doc(1024);
        const uint16_t cmd_id = nextStackCmdId_();
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)feature;
        doc["action"] = action;
        registerStackCmd_(cmd_id, findPendingByNode_(node_id), partFrom_(feature, action));
        uint8_t buf[StackCodec::kMaxPayload] = {};
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(buf), sizeof(buf));
        if (len == 0 || len > sizeof(buf))
            return;
        _stack_master->sendTo(node_id, (uint8_t)type, buf, len);
    }

    void sendStackCmd_(uint32_t node_id, StackMsgType type, StackFeature feature,
                       const char *action, const DynamicJsonDocument &params)
    {
        if (!_stack_master)
            return;
        DynamicJsonDocument doc(1024);
        const uint16_t cmd_id = nextStackCmdId_();
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)feature;
        doc["action"] = action;
        doc["params"] = params.as<JsonVariantConst>();
        registerStackCmd_(cmd_id, findPendingByNode_(node_id), partFrom_(feature, action));
        uint8_t buf[StackCodec::kMaxPayload] = {};
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(buf), sizeof(buf));
        if (len == 0 || len > sizeof(buf))
            return;
        _stack_master->sendTo(node_id, (uint8_t)type, buf, len);
    }

    void sendStackCmdSimple_(uint32_t node_id, StackMsgType type, StackFeature feature,
                             const char *action, const DynamicJsonDocument &params)
    {
        if (!_stack_master)
            return;
        DynamicJsonDocument doc(1024);
        const uint16_t cmd_id = nextStackCmdId_();
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)feature;
        doc["action"] = action;
        doc["params"] = params.as<JsonVariantConst>();
        registerStackCmd_(cmd_id, findPendingByNode_(node_id), StackPart::None);
        uint8_t buf[StackCodec::kMaxPayload] = {};
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(buf), sizeof(buf));
        if (len == 0 || len > sizeof(buf))
            return;
        _stack_master->sendTo(node_id, (uint8_t)type, buf, len);
    }

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        if (!ctx)
            return;
        static_cast<CloudClient *>(ctx)->handleStackFrame_(node_id, frame);
    }

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame)
    {
        if (frame.type != (uint8_t)StackMsgType::Ack && frame.type != (uint8_t)StackMsgType::Err)
            return;
        DynamicJsonDocument doc(2048);
        if (deserializeJson(doc, frame.payload, frame.payload_len))
            return;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        PendingStackCmd *cmd = findStackCmd_(cmd_id);
        if (!cmd || !cmd->used)
            return;
        PendingRequest *p = &_pending[cmd->pending_idx];
        if (!p->used || p->node_id != node_id)
        {
            cmd->used = false;
            return;
        }
        const bool ok = doc["ok"] | false;
        JsonObject data = doc["data"].as<JsonObject>();
        const uint16_t part_idx = data["part"] | 1;
        const uint16_t parts = data["parts"] | 1;
        const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part_idx >= parts);
        const bool first_part = !cmd->started || part_idx <= 1;
        applyStackPart_(p, cmd->part, ok, data, first_part, done);
        cmd->started = true;
        if (done || !ok)
            cmd->used = false;

        if (p->pending_mask == 0)
            finalizePending_(p, true, "");
    }

    void applyStackPart_(PendingRequest *p, StackPart part, bool ok, JsonObject data, bool first_part, bool done)
    {
        if (!p || !p->doc)
            return;
        if (!ok)
        {
            p->pending_mask &= ~maskFor_(part);
            return;
        }
        JsonObject root = p->doc->to<JsonObject>();
        if (p->want_system)
            applyStackSystem_(root, part, data, p->node_id);
        if (p->want_controllers)
            applyStackControllers_(root, part, data, first_part);
        if (done)
            p->pending_mask &= ~maskFor_(part);
    }

    void applyStackSystem_(JsonObject root, StackPart part, JsonObject data, uint32_t node_id)
    {
        JsonObject sys = root["system"].to<JsonObject>();
        if (sys.isNull())
            sys = root.createNestedObject("system");
        if (part == StackPart::SystemInfo)
        {
            sys["device_name"] = stackNodeName_(node_id);
            sys["uptime_ms"] = data["uptime_ms"] | 0;
            sys["board"] = data["board"] | "";
            sys["fw_version"] = data["fw_version"] | "";
        }
        else if (part == StackPart::PlcStatus)
        {
            JsonObject plc = sys["plc"].to<JsonObject>();
            plc["board_temp"] = data["board_temp"] | 0.0f;
            plc["cpu_temp"] = data["cpu_temp"] | 0.0f;
            JsonObject fan = sys["fan"].to<JsonObject>();
            fan["fan_on"] = data["fan_on"] | false;
            fan["on_c"] = data["on_c"] | 0.0f;
            fan["hyst_c"] = data["hyst_c"] | 0.0f;
        }
        else if (part == StackPart::FanStatus)
        {
            JsonObject fan = sys["fan"].to<JsonObject>();
            fan["mode"] = data["mode"] | "";
            fan["fan_on"] = data["fan_on"] | false;
            fan["on_c"] = data["on_c"] | 0.0f;
            fan["hyst_c"] = data["hyst_c"] | 0.0f;
            JsonObject plc = sys["plc"].to<JsonObject>();
            plc["board_temp"] = data["board_temp"] | plc["board_temp"] | 0.0f;
        }
        else if (part == StackPart::RtcTime)
        {
            JsonObject rtc = sys["rtc"].to<JsonObject>();
            rtc["date"] = data["date"] | "";
            rtc["time"] = data["time"] | "";
            rtc["weekday"] = data["weekday"] | 0;
            rtc["temp_c"] = data["temp_c"] | 0.0f;
        }
    }

    void applyStackControllers_(JsonObject root, StackPart part, JsonObject data, bool first_part)
    {
        JsonObject ctrls = root["controllers"].to<JsonObject>();
        if (ctrls.isNull())
            ctrls = root.createNestedObject("controllers");

        if (part == StackPart::Sockets)
            copyItems_(ctrls, "sockets", data["items"].as<JsonArrayConst>(), first_part);
        else if (part == StackPart::Lights)
            copyItems_(ctrls, "lights", data["items"].as<JsonArrayConst>(), first_part);
        else if (part == StackPart::Meteo)
            copyItems_(ctrls, "meteo", data["items"].as<JsonArrayConst>(), first_part);
        else if (part == StackPart::Thermo)
            copyItems_(ctrls, "thermo", data["items"].as<JsonArrayConst>(), first_part);
        else if (part == StackPart::Tanks)
            copyItems_(ctrls, "tanks", data["items"].as<JsonArrayConst>(), first_part);
        else if (part == StackPart::Septic)
            copyItems_(ctrls, "septic", data["items"].as<JsonArrayConst>(), first_part);
        else if (part == StackPart::Watering)
            copyItems_(ctrls, "watering", data["items"].as<JsonArrayConst>(), first_part);
        else if (part == StackPart::SecurityStatus)
        {
            JsonObject sec = ctrls["security"].to<JsonObject>();
            if (sec.isNull())
                sec = ctrls.createNestedObject("security");
            sec["enabled"] = data["enabled"] | false;
            sec["armed"] = data["armed"] | false;
            sec["alarm"] = data["alarm"] | false;
            if (data["siren"].is<unsigned>())
                sec["siren"] = data["siren"].as<unsigned>();
        }
        else if (part == StackPart::SecuritySensors)
        {
            JsonObject sec = ctrls["security"].to<JsonObject>();
            if (sec.isNull())
                sec = ctrls.createNestedObject("security");
            copyItems_(sec, "sensors", data["items"].as<JsonArrayConst>(), first_part);
        }
        else if (part == StackPart::Ring)
        {
            JsonObject ring = ctrls["ring"].to<JsonObject>();
            if (ring.isNull())
                ring = ctrls.createNestedObject("ring");
            ring["enabled"] = data["enabled"] | false;
            if (data["button"].is<unsigned>())
                ring["button"] = data["button"].as<unsigned>();
            if (data["relay"].is<unsigned>())
                ring["relay"] = data["relay"].as<unsigned>();
            ring["relay_on"] = data["relay_on"] | false;
        }
        else if (part == StackPart::Avr)
        {
            JsonObject avr = ctrls["avr"].to<JsonObject>();
            if (avr.isNull())
                avr = ctrls.createNestedObject("avr");
            avr["enabled"] = data["enabled"] | false;
            avr["auto_mode"] = data["auto_mode"] | true;
            avr["prefer_main"] = data["prefer_main"] | true;
            avr["auto_return_main"] = data["auto_return_main"] | true;
            if (data["main_ok_port"].is<unsigned>())
                avr["main_ok_port"] = data["main_ok_port"].as<unsigned>();
            if (data["reserve_ok_port"].is<unsigned>())
                avr["reserve_ok_port"] = data["reserve_ok_port"].as<unsigned>();
            if (data["relay_main_port"].is<unsigned>())
                avr["relay_main_port"] = data["relay_main_port"].as<unsigned>();
            if (data["relay_reserve_port"].is<unsigned>())
                avr["relay_reserve_port"] = data["relay_reserve_port"].as<unsigned>();
            if (data["feedback_main_port"].is<unsigned>())
                avr["feedback_main_port"] = data["feedback_main_port"].as<unsigned>();
            if (data["feedback_reserve_port"].is<unsigned>())
                avr["feedback_reserve_port"] = data["feedback_reserve_port"].as<unsigned>();
            avr["main_ok"] = data["main_ok"] | false;
            avr["reserve_ok"] = data["reserve_ok"] | false;
            avr["relay_main_on"] = data["relay_main_on"] | false;
            avr["relay_reserve_on"] = data["relay_reserve_on"] | false;
            avr["active_source"] = data["active_source"] | "";
            avr["target_source"] = data["target_source"] | "";
            avr["fault"] = data["fault"] | "";
            avr["transfer"] = data["transfer"] | false;
        }
        else if (part == StackPart::Leak)
        {
            copyItems_(ctrls, "leak", data["items"].as<JsonArrayConst>(), first_part);
        }
    }

    static void copyItems_(JsonObject &dst_parent, const char *key, JsonArrayConst items, bool reset)
    {
        if (reset)
            dst_parent.remove(key);
        JsonArray dst = dst_parent[key].to<JsonArray>();
        if (dst.isNull())
            dst = dst_parent.createNestedArray(key);
        if (items.isNull())
            return;
        for (JsonVariantConst v : items)
            dst.add(v);
    }

    void fillSystemInfo_(JsonObject out)
    {
        out["device_name"] = _plc.deviceName();
        out["uptime_ms"] = (uint32_t)millis();
        out["board"] = ActiveBoardProfile::UI_NAME;
        out["fw_version"] = _fw_version;

        JsonObject wifi = out["wifi"].to<JsonObject>();
        wifi["mode"] = _wifi.ap() ? "ap" : "sta";
        if (_wifi.ap())
            wifi["ap_ssid"] = _wifi.apSsid();
        else
            wifi["ssid"] = _wifi.ssid();
        wifi["ip"] = localIp_();
        wifi["mac"] = WiFi.macAddress();

        JsonObject plc = out["plc"].to<JsonObject>();
        plc["board_temp"] = _plc.boardTemp();
        plc["cpu_temp"] = _plc.cpuTemp();

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

    void fillControllersInfo_(JsonObject out)
    {
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

    void fillSockets_(JsonArray out, bool lights)
    {
        const size_t count = lights ? SocketController::kLightCount : SocketController::kSocketCount;
        for (size_t i = 0; i < count; ++i)
        {
            const auto *cfg = lights ? _controllers.sockets().lightConfigByIndex(i)
                                     : _controllers.sockets().configByIndex(i);
            const auto *st = lights ? _controllers.sockets().lightStateByIndex(i)
                                    : _controllers.sockets().stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = out.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            if (cfg->name.length())
                o["name"] = cfg->name;
            if (cfg->button_port != SocketController::kInvalidPort)
                o["button"] = cfg->button_port;
            if (cfg->relay_port != SocketController::kInvalidPort)
                o["relay"] = cfg->relay_port;
            o["state"] = st->relay_on;
        }
    }

    void fillMeteo_(JsonArray out)
    {
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = _controllers.meteo().configByIndex(i);
            const auto *st = _controllers.meteo().stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = out.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            if (cfg->name.length())
                o["name"] = cfg->name;
            o["type"] = MeteoController::typeName(cfg->type);
            if (cfg->type == MeteoController::SensorType::Dht22 &&
                cfg->dht_pin != MeteoController::kInvalidPin)
                o["pin"] = cfg->dht_pin;
            if (cfg->type == MeteoController::SensorType::Ds18b20 && cfg->ds18_addr_set)
            {
                char hex[17] = {};
                MeteoController::formatHexAddr(cfg->ds18_addr, hex);
                o["addr"] = hex;
            }
            if (st->has_temp)
                o["temp_c"] = st->temp_c;
            if (st->has_humidity)
                o["hum"] = st->humidity;
            o["has_temp"] = st->has_temp;
            o["has_hum"] = st->has_humidity;
            o["ok"] = st->ok;
        }
    }

    void fillThermo_(JsonArray out)
    {
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = _controllers.thermo().configByIndex(i);
            const auto *st = _controllers.thermo().stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = out.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            if (cfg->name.length())
                o["name"] = cfg->name;
            o["sensor"] = (unsigned)cfg->sensor_id;
            o["mode"] = ThermoController::modeName(cfg->mode);
            o["target"] = cfg->target_c;
            o["hyst"] = cfg->hysteresis;
            if (cfg->heat_port != ThermoController::kInvalidPort)
                o["heat"] = cfg->heat_port;
            if (cfg->cool_port != ThermoController::kInvalidPort)
                o["cool"] = cfg->cool_port;
            if (cfg->button_port != ThermoController::kInvalidPort)
                o["button"] = cfg->button_port;
            o["power_on"] = st->power_on;
            o["heat_on"] = st->heat_on;
            o["cool_on"] = st->cool_on;
        }
    }

    void fillTanks_(JsonArray out)
    {
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = _controllers.tanks().configByIndex(i);
            const auto *st = _controllers.tanks().stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = out.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            o["power_on"] = cfg->power_on;
            if (cfg->name.length())
                o["name"] = cfg->name;
            if (cfg->level_low != TankController::kInvalidPort)
                o["low"] = cfg->level_low;
            if (cfg->level_mid != TankController::kInvalidPort)
                o["mid"] = cfg->level_mid;
            if (cfg->level_full != TankController::kInvalidPort)
                o["full"] = cfg->level_full;
            if (cfg->relay_valve != TankController::kInvalidPort)
                o["valve"] = cfg->relay_valve;
            if (cfg->relay_pump != TankController::kInvalidPort)
                o["pump"] = cfg->relay_pump;
            if (cfg->relay_alarm != TankController::kInvalidPort)
                o["alarm"] = cfg->relay_alarm;
            o["level_low"] = st->level_low;
            o["level_mid"] = st->level_mid;
            o["level_full"] = st->level_full;
            o["levels_ok"] = st->levels_ok;
            o["valve_on"] = st->valve_on;
            o["pump_on"] = st->pump_on;
            o["alarm_on"] = st->alarm_on;
        }
    }

    void fillSeptic_(JsonArray out)
    {
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = _controllers.septic().configByIndex(i);
            const auto *st = _controllers.septic().stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = out.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            if (cfg->name.length())
                o["name"] = cfg->name;
            o["monitor"] = cfg->monitoring_on;
            if (cfg->warning_port != SepticController::kInvalidPort)
                o["warning_port"] = cfg->warning_port;
            if (cfg->alarm_port != SepticController::kInvalidPort)
                o["alarm_port"] = cfg->alarm_port;
            if (cfg->relay_warning != SepticController::kInvalidPort)
                o["relay_warning"] = cfg->relay_warning;
            if (cfg->relay_alarm != SepticController::kInvalidPort)
                o["relay_alarm"] = cfg->relay_alarm;
            o["warning"] = st->warning;
            o["alarm"] = st->alarm;
        }
    }

    void fillWatering_(JsonArray out)
    {
        for (size_t i = 0; i < WateringController::kRuleCount; ++i)
        {
            const auto *cfg = _controllers.watering().configByIndex(i);
            const auto *st = _controllers.watering().stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = out.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            o["status"] = st->status;
            if (cfg->name.length())
                o["name"] = cfg->name;
            if (cfg->port != WateringController::kInvalidPort)
                o["port"] = cfg->port;
            if (cfg->tank_id)
                o["tank"] = cfg->tank_id;
            if (cfg->weekdays_mask)
                o["weekdays_mask"] = cfg->weekdays_mask;
            if (cfg->duration_sec && cfg->hour <= 23 && cfg->minute <= 59)
            {
                o["hour"] = cfg->hour;
                o["minute"] = cfg->minute;
                o["duration_s"] = cfg->duration_sec;
            }
            if (cfg->duration2_sec && cfg->hour2 <= 23 && cfg->minute2 <= 59)
            {
                o["hour2"] = cfg->hour2;
                o["minute2"] = cfg->minute2;
                o["duration2_s"] = cfg->duration2_sec;
            }
            if (cfg->duration3_sec && cfg->hour3 <= 23 && cfg->minute3 <= 59)
            {
                o["hour3"] = cfg->hour3;
                o["minute3"] = cfg->minute3;
                o["duration3_s"] = cfg->duration3_sec;
            }
            o["resume"] = cfg->resume_after_refill;
            o["resume_level"] = cfg->resume_level;
            o["active"] = st->active;
            o["paused"] = st->paused;
            if (st->remaining_ms)
                o["remaining_ms"] = st->remaining_ms;
        }
    }

    void fillSecurity_(JsonObject out)
    {
        out["enabled"] = _controllers.security().controllerEnabled();
        out["armed"] = _controllers.security().armed();
        out["alarm"] = _controllers.security().alarmOn();
        if (_controllers.security().sirenPort() != SecurityController::kInvalidPort)
            out["siren"] = (unsigned)_controllers.security().sirenPort();

        JsonArray arr = out["sensors"].to<JsonArray>();
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = _controllers.security().configByIndex(i);
            const auto *st = _controllers.security().stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = arr.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            o["type"] = (cfg->type == SecurityController::SensorType::Reed) ? "reed" : "pir";
            if (cfg->port != SecurityController::kInvalidPort)
                o["port"] = cfg->port;
            if (cfg->name.length())
                o["name"] = cfg->name;
            o["silent"] = cfg->silent;
            o["detect"] = st->is_detect;
        }
    }

    void fillRing_(JsonObject out)
    {
        const auto &cfg = _controllers.ring().config();
        const auto &st = _controllers.ring().state();
        out["enabled"] = cfg.enabled;
        if (cfg.button_port != RingController::kInvalidPort)
            out["button"] = cfg.button_port;
        if (cfg.relay_port != RingController::kInvalidPort)
            out["relay"] = cfg.relay_port;
        out["relay_on"] = st.relay_on;
    }

    void fillAvr_(JsonObject out)
    {
        const auto &cfg = _controllers.avr().config();
        const auto &st = _controllers.avr().state();
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

    void fillLeak_(JsonArray out)
    {
        for (size_t i = 0; i < LeakController::kZoneCount; ++i)
        {
            const auto *cfg = _controllers.leak().configByIndex(i);
            const auto *st = _controllers.leak().stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = out.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            o["power_on"] = cfg->power_on;
            o["sensor_active_low"] = cfg->sensor_active_low;
            if (cfg->sensor_port != LeakController::kInvalidPort)
                o["sensor"] = cfg->sensor_port;
            if (cfg->valve_port != LeakController::kInvalidPort)
                o["valve"] = cfg->valve_port;
            if (cfg->alarm_port != LeakController::kInvalidPort)
                o["alarm"] = cfg->alarm_port;
            if (cfg->name.length())
                o["name"] = cfg->name;
            o["wet"] = st->wet;
            o["alarm_latched"] = st->alarm_latched;
        }
    }

    void fillStackInfo_(JsonObject out)
    {
        out["role"] = stackRoleName_();
        out["node_id"] = deviceId_();
        JsonArray nodes = out["nodes"].to<JsonArray>();
        if (!isStackMaster_() || !_stack_master)
            return;
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            if (!_stack_master->nodeIsControllerAt(i))
                continue;
            JsonObject n = nodes.add<JsonObject>();
            n["node_id"] = _stack_master->nodeIdAt(i);
            n["name"] = _stack_master->nodeNameAt(i);
            n["online"] = true;
            n["last_seen_ms"] = 0;
        }
    }

    void maybeSendPeriodicEvent_()
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

    void handlePendingTimeouts_()
    {
        const uint32_t now = millis();
        for (auto &p : _pending)
        {
            if (!p.used)
                continue;
            if ((int32_t)(now - p.deadline_ms) < 0)
                continue;
            finalizePending_(&p, false, "timeout");
        }
    }

    PendingRequest *allocPending_(const String &ws_id, uint32_t node_id)
    {
        for (auto &p : _pending)
        {
            if (!p.used)
            {
                p.used = true;
                p.ws_id = ws_id;
                p.node_id = node_id;
                p.deadline_ms = millis() + kStackTimeoutMs;
                p.pending_mask = 0;
                p.want_system = false;
                p.want_controllers = false;
                p.doc = nullptr;
                return &p;
            }
        }
        return nullptr;
    }

    void freePending_(PendingRequest *p)
    {
        if (!p)
            return;
        if (p->doc)
        {
            delete p->doc;
            p->doc = nullptr;
        }
        *p = PendingRequest{};
    }

    void clearPending_()
    {
        for (auto &p : _pending)
            if (p.used)
                freePending_(&p);
        for (auto &c : _stack_cmds)
            c.used = false;
    }

    void ensurePendingDoc_(PendingRequest *p)
    {
        if (!p)
            return;
        if (!p->doc)
            p->doc = new DynamicJsonDocument(kWsDocCapacity);
    }

    PendingRequest *findPendingByNode_(uint32_t node_id)
    {
        for (auto &p : _pending)
            if (p.used && p.node_id == node_id)
                return &p;
        return nullptr;
    }

    void registerStackCmd_(uint16_t cmd_id, PendingRequest *p, StackPart part)
    {
        if (!p)
            return;
        for (auto &c : _stack_cmds)
        {
            if (!c.used)
            {
                c.used = true;
                c.cmd_id = cmd_id;
                c.pending_idx = (uint8_t)(p - _pending);
                c.part = part;
                c.started = false;
                return;
            }
        }
    }

    PendingStackCmd *findStackCmd_(uint16_t cmd_id)
    {
        for (auto &c : _stack_cmds)
            if (c.used && c.cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    uint16_t nextStackCmdId_()
    {
        if (_next_stack_cmd_id < kStackCmdIdBase || _next_stack_cmd_id > kStackCmdIdMax)
            _next_stack_cmd_id = kStackCmdIdBase;
        const uint16_t out = _next_stack_cmd_id++;
        if (_next_stack_cmd_id > kStackCmdIdMax)
            _next_stack_cmd_id = kStackCmdIdBase;
        return out;
    }

    StackPart partFrom_(StackFeature feature, const char *action) const
    {
        if (feature == StackFeature::System && strcmp(action, "get_info") == 0)
            return StackPart::SystemInfo;
        if (feature == StackFeature::PlcStatus)
            return StackPart::PlcStatus;
        if (feature == StackFeature::Fan)
            return StackPart::FanStatus;
        if (feature == StackFeature::Rtc)
            return StackPart::RtcTime;
        if (feature == StackFeature::Sockets && strcmp(action, "get") == 0)
            return StackPart::Sockets;
        if (feature == StackFeature::Sockets && strcmp(action, "get_lights") == 0)
            return StackPart::Lights;
        if (feature == StackFeature::Meteo)
            return StackPart::Meteo;
        if (feature == StackFeature::Thermo)
            return StackPart::Thermo;
        if (feature == StackFeature::Tanks)
            return StackPart::Tanks;
        if (feature == StackFeature::Septic)
            return StackPart::Septic;
        if (feature == StackFeature::Watering)
            return StackPart::Watering;
        if (feature == StackFeature::Security && strcmp(action, "status") == 0)
            return StackPart::SecurityStatus;
        if (feature == StackFeature::Security && strcmp(action, "get") == 0)
            return StackPart::SecuritySensors;
        if (feature == StackFeature::Ring)
            return StackPart::Ring;
        if (feature == StackFeature::Avr && strcmp(action, "get") == 0)
            return StackPart::Avr;
        if (feature == StackFeature::Leak && strcmp(action, "get") == 0)
            return StackPart::Leak;
        return StackPart::None;
    }

    static uint32_t maskFor_(StackPart p)
    {
        return 1u << (uint8_t)p;
    }

    static bool hasWhat_(JsonArrayConst what, const char *name)
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

    const char *stackRoleName_() const
    {
        if (!_configs)
            return "master";
        const auto role = _configs->stackRole();
        return (role == ConfigsManagerIface::StackRole::Master) ? "master" : "slave";
    }

    bool isStackMaster_() const
    {
        if (!_configs)
            return true;
        return _configs->stackRole() == ConfigsManagerIface::StackRole::Master;
    }

    uint32_t deviceId_() const
    {
        return (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFu);
    }

    String localIp_() const
    {
        if (_wifi.ap())
            return WiFi.softAPIP().toString();
        if (WiFi.status() == WL_CONNECTED)
            return WiFi.localIP().toString();
        return String();
    }

    String stackNodeName_(uint32_t node_id) const
    {
        if (!_stack_master)
            return String();
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
            if (_stack_master->nodeIdAt(i) == node_id)
                return _stack_master->nodeNameAt(i);
        return String();
    }

    static ThermoController::Mode parseThermoMode_(const String &mode)
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

    String nextWsId_()
    {
        static uint32_t seq = 0;
        return String("ws") + String(++seq);
    }

    void sendJson_(JsonDocument &doc)
    {
        String out;
        serializeJson(doc, out);
        if (out.length())
            _ws.sendTXT(out);
    }
};
