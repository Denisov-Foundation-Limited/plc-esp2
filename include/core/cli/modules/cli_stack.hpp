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
#include <string.h>
#include <string.h>

#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "controllers/meteo_controller.hpp"
#include "controllers/septic_controller.hpp"
#include "controllers/socket_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "utils/configs_manager_iface.hpp"

template <typename ConsoleT>
class CLIStackT
{
public:
    explicit CLIStackT(ConsoleT &console) : _c(console) {}

    void bind(StackMaster *master)
    {
        _stack_master = master;
        if (_stack_master)
            _stack_master->setFrameHandler(&CLIStackT::onStackFrame_, this);
    }

    void cmdShowStack_()
    {
        if (!_c._configs_manager)
        {
            _c._io->println(F("Config manager missing"));
            return;
        }
        _c._io->println(F("Stack:"));
        const size_t key_w = 16; // slave_controller
        const auto role = _c._configs_manager->stackRole();
        _c.printKeyValueTab_(F("role"), stackRoleName_(role), key_w);
        _c.printKeyValueTab_(F("master_host"), _c._configs_manager->stackMasterHost(), key_w);
        _c.printKeyValueTab_(F("fallback"), _c._configs_manager->stackFallbackEnabled() ? F("on") : F("off"), key_w);
        _c.printKeyValueTab_(F("fallback_host"), _c._configs_manager->stackFallbackHost(), key_w);
        _c.printKeyValueTab_(F("slave_controller"),
                             _c._configs_manager->stackSlaveController() ? F("on") : F("off"),
                             key_w);
        const String api_key = _c._configs_manager->stackApiKey();
        _c.printKeyValueTab_(F("api_key"), api_key.length() ? F("set") : F("none"), key_w);

        if (role == ConfigsManagerIface::StackRole::Master)
            listStackNodes_();
    }

    void cmdStack_(const String &line)
    {
        String cmd = line;
        cmd.trim();
        if (cmd == "stack trace")
        {
            _c._io->print(F("Stack trace: "));
            _c._io->println(_trace_enabled ? F("on") : F("off"));
            return;
        }
        if (cmd == "stack trace on")
        {
            _trace_enabled = true;
            _c._io->println(F("Stack trace enabled"));
            return;
        }
        if (cmd == "stack trace off")
        {
            _trace_enabled = false;
            _c._io->println(F("Stack trace disabled"));
            return;
        }
        if (cmd == "stack nodes")
        {
            listStackNodes_();
            return;
        }
        if (cmd.startsWith("stack socket "))
        {
            handleSocketCmd_(cmd);
            return;
        }
        if (cmd.startsWith("stack thermo "))
        {
            handleThermoCmd_(cmd);
            return;
        }
        if (cmd.startsWith("stack septic "))
        {
            handleSepticCmd_(cmd);
            return;
        }
        if (cmd.startsWith("stack security "))
        {
            handleSecurityCmd_(cmd);
            return;
        }
        if (cmd.startsWith("stack ring "))
        {
            handleRingCmd_(cmd);
            return;
        }
        if (!cmd.startsWith("stack send "))
        {
            _c._io->println(F("Usage: stack nodes"));
            _c._io->println(F("       stack trace <on|off>"));
            _c._io->println(F("       stack send <id> <get|set> <json>"));
            _c._io->print(F("       stack socket <unit> <on|off|toggle> <id>"));
            printSocketIdRangeInline_();
            _c._io->println();
            _c._io->print(F("       stack thermo <unit> <on|off|toggle> <id>"));
            printThermoIdRangeInline_();
            _c._io->println();
            _c._io->println(F("       stack security <unit> <arm|disarm|status|clear>"));
            _c._io->println(F("       stack septic <unit> <status|get|monitor>"));
            _c._io->println(F("       stack ring <unit> <on|off>"));
            return;
        }
        if (!_stack_master)
        {
            _c._io->println(F("Stack master unavailable"));
            return;
        }
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _c._io->println(F("Stack role is slave"));
            return;
        }
        String rest = cmd.substring(strlen("stack send "));
        rest.trim();
        const int sp1 = rest.indexOf(' ');
        if (sp1 <= 0)
        {
            _c._io->println(F("Invalid node id"));
            return;
        }
        String id_str = rest.substring(0, sp1);
        rest = rest.substring(sp1 + 1);
        rest.trim();
        const int sp2 = rest.indexOf(' ');
        if (sp2 <= 0)
        {
            _c._io->println(F("Missing get/set"));
            return;
        }
        String kind = rest.substring(0, sp2);
        kind.toLowerCase();
        String json = rest.substring(sp2 + 1);
        json.trim();
        if (json.length() == 0)
        {
            _c._io->println(F("Missing JSON payload"));
            return;
        }
        uint32_t node_id = (uint32_t)strtoul(id_str.c_str(), nullptr, 0);
        uint8_t type = 0;
        if (kind == "get")
            type = (uint8_t)StackMsgType::CmdGet;
        else if (kind == "set")
            type = (uint8_t)StackMsgType::CmdSet;
        else
        {
            _c._io->println(F("Invalid command type"));
            return;
        }
        const bool ok = _stack_master->sendTo(node_id, type,
                                              (const uint8_t *)json.c_str(), json.length());
        _c._io->println(ok ? F("OK") : F("Send failed"));
    }

    bool canRequestStackExt_() const
    {
        if (!_stack_master)
            return false;
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        return _stack_master->nodeCount() > 0;
    }

    void requestStackExtList_()
    {
        if (!canRequestStackExt_())
            return;

        const size_t count = _stack_master->nodeCount();
        _pending_ext_cmd_id = nextStackCmdId_();
        _pending_ext_left = (uint8_t)min<size_t>(count, 255);
        _pending_ext_scan = true;

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = _pending_ext_cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Extenders;
        doc["action"] = "get_list";
        appendApiKey_(doc);
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload, len);
        }
    }

    void requestStackI2cScan_()
    {
        if (!_stack_master)
            return;
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
            return;

        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return;

        _pending_i2c_cmd_id = nextStackCmdId_();
        _pending_i2c_left = (uint8_t)min<size_t>(count, 255);
        _pending_i2c_scan = true;

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = _pending_i2c_cmd_id;
        doc["feature"] = (uint8_t)StackFeature::I2cScan;
        doc["action"] = "run";
        appendApiKey_(doc);
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload, len);
        }
    }

    void requestStackPorts_()
    {
        if (!_stack_master)
            return;
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
            return;

        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return;

        _pending_ports_cmd_id = nextStackCmdId_();
        _pending_ports_left = (uint8_t)min<size_t>(count, 255);
        _pending_ports_scan = true;

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = _pending_ports_cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Ports;
        doc["action"] = "get_state";
        appendApiKey_(doc);
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload, len);
        }
    }

    bool requestStackSockets_()
    {
        if (!_stack_master)
            return false;
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;

        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return false;

        _pending_sockets_cmd_id = nextStackCmdId_();
        _pending_sockets_left = (uint8_t)min<size_t>(count, 255);
        _pending_sockets_scan = true;

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = _pending_sockets_cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "get";
        appendApiKey_(doc);
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload, len);
        }
        return true;
    }

    bool requestStackMeteo_()
    {
        if (!_stack_master)
            return false;
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;

        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return false;

        _pending_meteo_cmd_id = nextStackCmdId_();
        _pending_meteo_left = (uint8_t)min<size_t>(count, 255);
        _pending_meteo_scan = true;

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = _pending_meteo_cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Meteo;
        doc["action"] = "get";
        appendApiKey_(doc);
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload, len);
        }
        return true;
    }

    bool requestStackThermo_()
    {
        if (!_stack_master)
            return false;
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;

        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return false;

        _pending_thermo_cmd_id = nextStackCmdId_();
        _pending_thermo_left = (uint8_t)min<size_t>(count, 255);
        _pending_thermo_scan = true;

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = _pending_thermo_cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Thermo;
        doc["action"] = "get";
        appendApiKey_(doc);
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload, len);
        }
        return true;
    }

    void requestStackPlc_()
    {
        if (!_stack_master)
            return;
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
            return;

        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return;

        initStackPlcPending_(count);

        StaticJsonDocument<192> doc_plc;
        doc_plc["cmd_id"] = _pending_plc_cmd_id;
        doc_plc["feature"] = (uint8_t)StackFeature::PlcStatus;
        doc_plc["action"] = "get";
        appendApiKey_(doc_plc);
        char payload_plc[96] = {};
        const size_t len_plc = serializeJson(doc_plc, payload_plc, sizeof(payload_plc));
        if (len_plc == 0)
            return;

        StaticJsonDocument<192> doc_rtc;
        doc_rtc["cmd_id"] = _pending_plc_rtc_cmd_id;
        doc_rtc["feature"] = (uint8_t)StackFeature::Rtc;
        doc_rtc["action"] = "get_time";
        appendApiKey_(doc_rtc);
        char payload_rtc[96] = {};
        const size_t len_rtc = serializeJson(doc_rtc, payload_rtc, sizeof(payload_rtc));
        if (len_rtc == 0)
            return;

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload_plc, len_plc);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload_rtc, len_rtc);
        }
    }

    void requestStackRtc_()
    {
        if (!_stack_master)
            return;
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
            return;

        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return;

        _pending_rtc_cmd_id = nextStackCmdId_();
        _pending_rtc_left = (uint8_t)min<size_t>(count, 255);
        _pending_rtc_scan = true;

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = _pending_rtc_cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Rtc;
        doc["action"] = "get_time";
        appendApiKey_(doc);
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload, len);
        }
    }

    void requestStackOwScan_()
    {
        if (!_stack_master)
            return;
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
            return;

        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return;

        _pending_ow_cmd_id = nextStackCmdId_();
        _pending_ow_left = (uint8_t)min<size_t>(count, 255);
        _pending_ow_scan = true;

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = _pending_ow_cmd_id;
        doc["feature"] = (uint8_t)StackFeature::OwScan;
        doc["action"] = "run";
        appendApiKey_(doc);
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;

        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _stack_master->sendTo(id, (uint8_t)StackMsgType::CmdGet,
                                  (const uint8_t *)payload, len);
        }
    }

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_c._io)
            return;
        if (handleStackI2cReply_(node_id, frame))
            return;
        if (handleStackExtReply_(node_id, frame))
            return;
        if (handleStackOwReply_(node_id, frame))
            return;
        if (handleStackPortsReply_(node_id, frame))
            return;
        if (handleStackSocketsReply_(node_id, frame))
            return;
        if (handleStackMeteoReply_(node_id, frame))
            return;
        if (handleStackThermoReply_(node_id, frame))
            return;
        if (handleStackPlcReply_(node_id, frame))
            return;
        if (handleStackRtcReply_(node_id, frame))
            return;
        if (!_trace_enabled)
            return;
        String payload = payloadToString_(frame.payload, frame.payload_len);
        _c._io->println();
        _c._io->print(F("[STACK] node="));
        _c._io->print(node_id);
        _c._io->print(F(" type="));
        _c._io->print(stackMsgName_(frame.type));
        _c._io->print(F(" payload="));
        _c._io->println(payload.length() ? payload : String(F("<empty>")));
        _c._cmd_blank_after = true;
        _c.printPrompt_();
        _c._io->print(_c._line);
    }

private:
    struct PlcPendingRow
    {
        bool in_use = false;
        bool got_plc = false;
        bool got_rtc = false;
        bool plc_ok = false;
        bool rtc_ok = false;
        bool done = false;
        uint32_t node_id = 0;
        String device_name;
        bool fan_on = false;
        float board_temp = 0.0f;
        float cpu_temp = 0.0f;
        float on_c = 0.0f;
        float hyst_c = 0.0f;
        float rtc_temp = 0.0f;
    };

    ConsoleT &_c;
    StackMaster *_stack_master = nullptr;

    uint16_t _stack_cmd_id = 0;
    uint16_t _pending_i2c_cmd_id = 0;
    uint8_t _pending_i2c_left = 0;
    bool _pending_i2c_scan = false;
    uint16_t _pending_ow_cmd_id = 0;
    uint8_t _pending_ow_left = 0;
    bool _pending_ow_scan = false;
    uint16_t _pending_ports_cmd_id = 0;
    uint8_t _pending_ports_left = 0;
    bool _pending_ports_scan = false;
    uint16_t _pending_sockets_cmd_id = 0;
    uint8_t _pending_sockets_left = 0;
    bool _pending_sockets_scan = false;
    uint16_t _pending_meteo_cmd_id = 0;
    uint8_t _pending_meteo_left = 0;
    bool _pending_meteo_scan = false;
    uint16_t _pending_thermo_cmd_id = 0;
    uint8_t _pending_thermo_left = 0;
    bool _pending_thermo_scan = false;
    uint16_t _pending_rtc_cmd_id = 0;
    uint8_t _pending_rtc_left = 0;
    bool _pending_rtc_scan = false;
    uint16_t _pending_ext_cmd_id = 0;
    uint8_t _pending_ext_left = 0;
    bool _pending_ext_scan = false;
    static constexpr size_t kMaxStackNodes = 8;
    PlcPendingRow _plc_pending[kMaxStackNodes] = {};
    uint16_t _pending_plc_cmd_id = 0;
    uint16_t _pending_plc_rtc_cmd_id = 0;
    uint8_t _pending_plc_total = 0;
    uint8_t _pending_plc_done = 0;
    bool _pending_plc_scan = false;
    bool _trace_enabled = false;

    void appendApiKey_(JsonDocument &doc)
    {
        if (!_c._configs_manager)
            return;
        const String key = _c._configs_manager->stackApiKey();
        if (key.length())
            doc["api_key"] = key;
    }

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        if (!ctx)
            return;
        static_cast<CLIStackT *>(ctx)->handleStackFrame_(node_id, frame);
    }

    void listStackNodes_()
    {
        if (!_stack_master)
        {
            _c._io->println(F("Stack master unavailable"));
            return;
        }
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _c._io->println(F("Stack role is slave"));
            return;
        }
        const size_t count = _stack_master->nodeCount();
        if (count == 0)
        {
            _c._io->println(F("Stack nodes: none"));
            return;
        }
        _c._io->println(F("Stack nodes:"));
        _c._io->println(F("  Unit      DeviceName        NodeID     IP"));
        _c._io->println(F("  --------  ----------------  ---------  ---------------"));
        for (size_t i = 0; i < count; ++i)
        {
            uint32_t id = _stack_master->nodeIdAt(i);
            String name = _stack_master->nodeNameAt(i);
            String ip = _stack_master->nodeIpAt(i);
            String unit = stackNodeLabel_(id);
            char id_buf[12] = {};
            snprintf(id_buf, sizeof(id_buf), "%lu", (unsigned long)id);
            _c._io->print(F("  "));
            _c.printPadStr_(unit.c_str(), 8);
            _c._io->print(F("  "));
            _c.printPadStr_(name.length() ? name.c_str() : "-", 16);
            _c._io->print(F("  "));
            _c.printPadStr_(id_buf, 9);
            _c._io->print(F("  "));
            _c._io->println(ip.length() ? ip : String("-"));
        }
    }

    void printStackUnavailable_() const
    {
        if (!_stack_master)
        {
            _c._io->println(F("Stack master unavailable"));
            return;
        }
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _c._io->println(F("Stack role is slave"));
            return;
        }
        if (_stack_master->nodeCount() == 0)
        {
            _c._io->println(F("Stack nodes: none"));
            return;
        }
    }

    void handleSocketCmd_(const String &cmd)
    {
        if (!_stack_master)
        {
            _c._io->println(F("Stack master unavailable"));
            return;
        }
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _c._io->println(F("Stack role is slave"));
            return;
        }
        String rest = cmd.substring(strlen("stack socket "));
        rest.trim();
        const int sp1 = rest.indexOf(' ');
        if (sp1 <= 0)
        {
            _c._io->print(F("Usage: stack socket <unit> <on|off|toggle> <id>"));
            printSocketIdRangeInline_();
            _c._io->println();
            return;
        }
        String unit_str = rest.substring(0, sp1);
        rest = rest.substring(sp1 + 1);
        rest.trim();
        const int sp2 = rest.indexOf(' ');
        if (sp2 <= 0)
        {
            _c._io->print(F("Usage: stack socket <unit> <on|off|toggle> <id>"));
            printSocketIdRangeInline_();
            _c._io->println();
            return;
        }
        String action = rest.substring(0, sp2);
        action.toLowerCase();
        String id_str = rest.substring(sp2 + 1);
        id_str.trim();
        if (id_str.length() == 0)
        {
            printInvalidSocketId_();
            return;
        }
        uint32_t node_id = resolveUnitToNodeId_(unit_str);
        if (node_id == 0)
        {
            _c._io->println(F("Unknown unit"));
            return;
        }
        uint16_t socket_id = (uint16_t)strtoul(id_str.c_str(), nullptr, 0);
        if (!isSocketIdValid_(socket_id))
        {
            printInvalidSocketId_();
            return;
        }
        if (action != "on" && action != "off" && action != "toggle")
        {
            _c._io->println(F("Invalid action"));
            return;
        }

        StaticJsonDocument<256> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "set";
        JsonArray items = doc["params"]["items"].to<JsonArray>();
        JsonObject item = items.add<JsonObject>();
        item["id"] = socket_id;
        if (action == "toggle")
            item["toggle"] = true;
        else
            item["state"] = (action == "on");
        appendApiKey_(doc);

        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
        {
            _c._io->println(F("Serialize failed"));
            return;
        }
        const bool ok = _stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                              (const uint8_t *)payload, len);
        _c._io->println(ok ? F("OK") : F("Send failed"));
    }

    void handleThermoCmd_(const String &cmd)
    {
        if (!_stack_master)
        {
            _c._io->println(F("Stack master unavailable"));
            return;
        }
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _c._io->println(F("Stack role is slave"));
            return;
        }
        String rest = cmd.substring(strlen("stack thermo "));
        rest.trim();
        const int sp1 = rest.indexOf(' ');
        if (sp1 <= 0)
        {
            printThermoUsage_();
            return;
        }
        String unit_str = rest.substring(0, sp1);
        rest = rest.substring(sp1 + 1);
        rest.trim();
        const int sp2 = rest.indexOf(' ');
        if (sp2 <= 0)
        {
            printThermoUsage_();
            return;
        }
        String action = rest.substring(0, sp2);
        action.toLowerCase();
        String id_str = rest.substring(sp2 + 1);
        id_str.trim();
        if (id_str.length() == 0)
        {
            printInvalidThermoId_();
            return;
        }
        uint32_t node_id = resolveUnitToNodeId_(unit_str);
        if (node_id == 0)
        {
            _c._io->println(F("Unknown unit"));
            return;
        }
        uint16_t thermo_id = (uint16_t)strtoul(id_str.c_str(), nullptr, 0);
        if (!isThermoIdValid_(thermo_id))
        {
            printInvalidThermoId_();
            return;
        }
        if (action != "on" && action != "off" && action != "toggle")
        {
            _c._io->println(F("Invalid action"));
            return;
        }

        StaticJsonDocument<256> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Thermo;
        doc["action"] = "set";
        JsonArray items = doc["params"]["items"].to<JsonArray>();
        JsonObject item = items.add<JsonObject>();
        item["id"] = thermo_id;
        if (action == "toggle")
            item["toggle"] = true;
        else
            item["power"] = (action == "on");
        appendApiKey_(doc);

        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
        {
            _c._io->println(F("Serialize failed"));
            return;
        }
        const bool ok = _stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                              (const uint8_t *)payload, len);
        _c._io->println(ok ? F("OK") : F("Send failed"));
    }

    void handleSecurityCmd_(const String &cmd)
    {
        if (!_stack_master)
        {
            _c._io->println(F("Stack master unavailable"));
            return;
        }
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _c._io->println(F("Stack role is slave"));
            return;
        }
        String rest = cmd.substring(strlen("stack security "));
        rest.trim();
        const int sp1 = rest.indexOf(' ');
        if (sp1 <= 0)
        {
            _c._io->println(F("Usage: stack security <unit> <arm|disarm|status|clear>"));
            return;
        }
        String unit_str = rest.substring(0, sp1);
        String action = rest.substring(sp1 + 1);
        action.trim();
        action.toLowerCase();
        if (action.length() == 0)
        {
            _c._io->println(F("Usage: stack security <unit> <arm|disarm|status|clear>"));
            return;
        }
        uint32_t node_id = resolveUnitToNodeId_(unit_str);
        if (node_id == 0)
        {
            _c._io->println(F("Unknown unit"));
            return;
        }
        if (action != "arm" && action != "disarm" && action != "status" && action != "clear")
        {
            _c._io->println(F("Invalid action"));
            return;
        }

        StaticJsonDocument<256> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Security;
        if (action == "status")
        {
            doc["action"] = "status";
        }
        else
        {
            doc["action"] = "set";
            JsonObject params = doc["params"].to<JsonObject>();
            if (action == "arm")
                params["armed"] = true;
            else if (action == "disarm")
                params["armed"] = false;
            if (action == "clear")
                params["clear"] = true;
            params["user"] = "admin";
        }
        appendApiKey_(doc);

        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
        {
            _c._io->println(F("Serialize failed"));
            return;
        }
        const uint8_t type = (action == "status") ? (uint8_t)StackMsgType::CmdGet : (uint8_t)StackMsgType::CmdSet;
        const bool ok = _stack_master->sendTo(node_id, type, (const uint8_t *)payload, len);
        _c._io->println(ok ? F("OK") : F("Send failed"));
    }

    void handleSepticCmd_(const String &cmd)
    {
        if (!_stack_master)
        {
            _c._io->println(F("Stack master unavailable"));
            return;
        }
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _c._io->println(F("Stack role is slave"));
            return;
        }
        String rest = cmd.substring(strlen("stack septic "));
        rest.trim();
        const int sp1 = rest.indexOf(' ');
        if (sp1 <= 0)
        {
            _c._io->println(F("Usage: stack septic <unit> <status|get|monitor>"));
            return;
        }
        String unit_str = rest.substring(0, sp1);
        String tail = rest.substring(sp1 + 1);
        tail.trim();
        if (tail.length() == 0)
        {
            _c._io->println(F("Usage: stack septic <unit> <status|get|monitor>"));
            return;
        }
        String action;
        String args;
        const int sp2 = tail.indexOf(' ');
        if (sp2 < 0)
            action = tail;
        else
        {
            action = tail.substring(0, sp2);
            args = tail.substring(sp2 + 1);
            args.trim();
        }
        action.trim();
        action.toLowerCase();
        uint32_t node_id = resolveUnitToNodeId_(unit_str);
        if (node_id == 0)
        {
            _c._io->println(F("Unknown unit"));
            return;
        }
        if (action == "status" || action == "get")
        {
            StaticJsonDocument<192> doc;
            doc["cmd_id"] = nextStackCmdId_();
            doc["feature"] = (uint8_t)StackFeature::Septic;
            doc["action"] = action;
            appendApiKey_(doc);

            char payload[96] = {};
            const size_t len = serializeJson(doc, payload, sizeof(payload));
            if (len == 0)
            {
                _c._io->println(F("Serialize failed"));
                return;
            }
            const bool ok = _stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                                  (const uint8_t *)payload, len);
            _c._io->println(ok ? F("OK") : F("Send failed"));
            return;
        }
        if (action != "monitor")
        {
            _c._io->println(F("Invalid action"));
            return;
        }
        if (args.length() == 0)
        {
            _c._io->println(F("Usage: stack septic <unit> monitor <id> <on|off>"));
            return;
        }
        const int sp3 = args.indexOf(' ');
        if (sp3 <= 0)
        {
            _c._io->println(F("Usage: stack septic <unit> monitor <id> <on|off>"));
            return;
        }
        String id_str = args.substring(0, sp3);
        String val_str = args.substring(sp3 + 1);
        id_str.trim();
        val_str.trim();
        const uint16_t id = (uint16_t)strtoul(id_str.c_str(), nullptr, 10);
        if (id < 1 || id > SepticController::kSepticCount)
        {
            _c._io->print(F("Invalid septic id (1.."));
            _c._io->print(SepticController::kSepticCount);
            _c._io->println(F(")"));
            return;
        }
        bool on = false;
        String val = val_str;
        val.toLowerCase();
        if (val == "on" || val == "1" || val == "true" || val == "yes")
            on = true;
        else if (val == "off" || val == "0" || val == "false" || val == "no")
            on = false;
        else
        {
            _c._io->println(F("Invalid value"));
            return;
        }

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Septic;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["id"] = id;
        params["monitor"] = on;
        appendApiKey_(doc);

        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
        {
            _c._io->println(F("Serialize failed"));
            return;
        }
        const bool ok = _stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                              (const uint8_t *)payload, len);
        _c._io->println(ok ? F("OK") : F("Send failed"));
    }

    void handleRingCmd_(const String &cmd)
    {
        if (!_stack_master)
        {
            _c._io->println(F("Stack master unavailable"));
            return;
        }
        if (_c._configs_manager &&
            _c._configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _c._io->println(F("Stack role is slave"));
            return;
        }
        String rest = cmd.substring(strlen("stack ring "));
        rest.trim();
        const int sp1 = rest.indexOf(' ');
        if (sp1 <= 0)
        {
            _c._io->println(F("Usage: stack ring <unit> <on|off>"));
            return;
        }
        String unit_str = rest.substring(0, sp1);
        String action = rest.substring(sp1 + 1);
        action.trim();
        action.toLowerCase();
        if (action.length() == 0)
        {
            _c._io->println(F("Usage: stack ring <unit> <on|off>"));
            return;
        }
        uint32_t node_id = resolveUnitToNodeId_(unit_str);
        if (node_id == 0)
        {
            _c._io->println(F("Unknown unit"));
            return;
        }
        if (action != "on" && action != "off")
        {
            _c._io->println(F("Invalid action"));
            return;
        }

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Ring;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["state"] = (action == "on");
        appendApiKey_(doc);

        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
        {
            _c._io->println(F("Serialize failed"));
            return;
        }
        const bool ok = _stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                              (const uint8_t *)payload, len);
        _c._io->println(ok ? F("OK") : F("Send failed"));
    }

    void printThermoUsage_() const
    {
        _c._io->print(F("Usage: stack thermo <unit> <on|off|toggle> <id>"));
        printThermoIdRangeInline_();
        _c._io->println();
    }

    uint32_t resolveUnitToNodeId_(String unit_str) const
    {
        if (!_stack_master)
            return 0;
        unit_str.trim();
        unit_str.toLowerCase();
        if (unit_str.startsWith("unit"))
            unit_str = unit_str.substring(4);
        unit_str.trim();
        if (unit_str.length() == 0)
            return 0;
        const uint32_t idx = (uint32_t)strtoul(unit_str.c_str(), nullptr, 10);
        if (idx == 0)
            return 0;
        const size_t pos = (size_t)(idx - 1);
        return _stack_master->nodeIdAt(pos);
    }

    static String payloadToString_(const uint8_t *data, size_t len)
    {
        String out;
        if (!data || len == 0)
            return out;
        out.reserve(len + 1);
        for (size_t i = 0; i < len; ++i)
            out += (char)data[i];
        return out;
    }

    bool handleStackI2cReply_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_pending_i2c_scan)
            return false;
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return false;

        static DynamicJsonDocument doc(4096);
        doc.clear();
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return false;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        if (cmd_id != _pending_i2c_cmd_id)
            return false;

        const String unit = stackNodeLabel_(node_id);
        const bool ok = doc["ok"] | false;
        if (frame.type == (uint8_t)StackMsgType::Err || !ok)
        {
            finishStackI2c_();
            _c.refreshPrompt_();
            return true;
        }

        JsonArrayConst items = doc["data"]["items"].as<JsonArrayConst>();
        if (items.isNull() || items.size() == 0)
        {
            finishStackI2c_();
            _c.refreshPrompt_();
            return true;
        }

        for (JsonObjectConst o : items)
        {
            const uint8_t bus = (uint8_t)(o["bus"] | 0);
            const char *addr = o["addr"] | "-";
            _c.printI2cRow_(unit, bus, addr);
        }
        finishStackI2c_();
        _c.refreshPrompt_();
        return true;
    }

    void finishStackI2c_()
    {
        if (_pending_i2c_left > 0)
            --_pending_i2c_left;
        if (_pending_i2c_left == 0)
            _pending_i2c_scan = false;
    }

    bool handleStackOwReply_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_pending_ow_scan)
            return false;
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return false;

        static DynamicJsonDocument doc(4096);
        doc.clear();
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return false;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        if (cmd_id != _pending_ow_cmd_id)
            return false;

        if (frame.type == (uint8_t)StackMsgType::Err || !(doc["ok"] | false))
        {
            finishStackOw_();
            _c.refreshPrompt_();
            return true;
        }

        JsonArrayConst items = doc["data"]["items"].as<JsonArrayConst>();
        if (items.isNull() || items.size() == 0)
        {
            finishStackOw_();
            _c.refreshPrompt_();
            return true;
        }

        const String unit = stackNodeLabel_(node_id);
        for (JsonObjectConst o : items)
        {
            const uint8_t bus = (uint8_t)(o["bus"] | 0);
            const char *addr = o["addr"] | "-";
            const char *type = o["type"] | "-";
            _c.printOwRow_(unit, bus, nullptr, addr, type);
        }
        finishStackOw_();
        _c.refreshPrompt_();
        return true;
    }

    void finishStackOw_()
    {
        if (_pending_ow_left > 0)
            --_pending_ow_left;
        if (_pending_ow_left == 0)
            _pending_ow_scan = false;
    }

    bool handleStackPortsReply_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_pending_ports_scan)
            return false;
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return false;

        static DynamicJsonDocument doc(4096);
        doc.clear();
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return false;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        if (cmd_id != _pending_ports_cmd_id)
            return false;

        if (frame.type == (uint8_t)StackMsgType::Err || !(doc["ok"] | false))
        {
            finishStackPorts_();
            _c.refreshPrompt_();
            return true;
        }

        JsonObjectConst data = doc["data"].as<JsonObjectConst>();
        JsonArrayConst items = data["ports"].as<JsonArrayConst>();
        if (items.isNull())
            items = data["items"].as<JsonArrayConst>();
        const uint16_t part = data["part"] | 1;
        const uint16_t parts = data["parts"] | 1;
        const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);

        const String unit = stackNodeLabel_(node_id);
        for (JsonObjectConst o : items)
        {
            const uint8_t id = (uint8_t)(o["id"] | 0);
            const char *backend = o["backend"] | "--";
            const char *loc = mapStackLocToExt_(o["loc"] | "--");
            const char *type = o["type"] | "--";
            const bool ctrl = o["ctrl"] | false;
            const int dev = o["dev"] | -1;
            const int pin = o["pin"] | -1;
            const char *hw = o["hw"] | "--";
            _c.printPortStateRow_(unit, id, backend, loc, type, ctrl, dev, pin, hw);
        }
        if (done)
        {
            finishStackPorts_();
            _c.refreshPrompt_();
        }
        return true;
    }

    void finishStackPorts_()
    {
        if (_pending_ports_left > 0)
            --_pending_ports_left;
        if (_pending_ports_left == 0)
            _pending_ports_scan = false;
    }

    bool handleStackSocketsReply_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_pending_sockets_scan)
            return false;
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return false;

        static DynamicJsonDocument doc(4096);
        doc.clear();
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return false;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        if (cmd_id != _pending_sockets_cmd_id)
            return false;

        if (frame.type == (uint8_t)StackMsgType::Err || !(doc["ok"] | false))
        {
            finishStackSockets_();
            _c.refreshPrompt_();
            return true;
        }

        JsonObjectConst data = doc["data"].as<JsonObjectConst>();
        const uint16_t part = data["part"] | 1;
        const uint16_t parts = data["parts"] | 1;
        const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
        JsonArrayConst items = data["items"].as<JsonArrayConst>();
        if (items.isNull() || items.size() == 0)
        {
            if (done)
            {
                finishStackSockets_();
                _c.refreshPrompt_();
            }
            return true;
        }

        const String unit = stackNodeLabel_(node_id);
        for (JsonObjectConst o : items)
        {
            const uint8_t id = (uint8_t)(o["id"] | 0);
            const bool enabled = o["enabled"] | false;
            if (!enabled)
                continue;
            const char *name = o["name"] | "-";
            const int button = o["button"] | -1;
            const int relay = o["relay"] | -1;
            const bool state = o["state"] | false;
            _c.printSocketRow_(unit.c_str(), id, enabled, name, button, relay, state);
        }
        if (done)
            finishStackSockets_();
        _c.refreshPrompt_();
        return true;
    }

    void finishStackSockets_()
    {
        if (_pending_sockets_left > 0)
            --_pending_sockets_left;
        if (_pending_sockets_left == 0)
            _pending_sockets_scan = false;
    }

    bool handleStackMeteoReply_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_pending_meteo_scan)
            return false;
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return false;

        static DynamicJsonDocument doc(4096);
        doc.clear();
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return false;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        if (cmd_id != _pending_meteo_cmd_id)
            return false;

        if (frame.type == (uint8_t)StackMsgType::Err || !(doc["ok"] | false))
        {
            finishStackMeteo_();
            _c.refreshPrompt_();
            return true;
        }

        JsonObjectConst data = doc["data"].as<JsonObjectConst>();
        const uint16_t part = data["part"] | 1;
        const uint16_t parts = data["parts"] | 1;
        const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
        JsonArrayConst items = data["items"].as<JsonArrayConst>();
        if (items.isNull() || items.size() == 0)
        {
            if (done)
            {
                finishStackMeteo_();
                _c.refreshPrompt_();
            }
            return true;
        }

        const String unit = stackNodeLabel_(node_id);
        for (JsonObjectConst o : items)
        {
            const uint8_t id = (uint8_t)(o["id"] | 0);
            const bool enabled = o["enabled"] | false;
            if (!enabled)
                continue;
            const char *type = o["type"] | "-";
            const bool has_temp = o["has_temp"] | false;
            const bool has_hum = o["has_hum"] | false;
            const float temp = o["temp_c"] | 0.0f;
            const float hum = o["hum"] | 0.0f;
            char temp_buf[10] = {};
            char hum_buf[10] = {};
            const char *temp_str = "--";
            const char *hum_str = "--";
            if (has_temp)
            {
                dtostrf(temp, 0, 2, temp_buf);
                temp_str = temp_buf;
            }
            if (has_hum)
            {
                dtostrf(hum, 0, 1, hum_buf);
                hum_str = hum_buf;
            }

            char info_buf[24] = {};
            const char *info = "-";
            if (strcmp(type, "dht22") == 0)
            {
                if (o["pin"].is<unsigned>())
                {
                    const unsigned pin = o["pin"].as<unsigned>();
                    snprintf(info_buf, sizeof(info_buf), "pin=%u", pin);
                    info = info_buf;
                }
            }
            else if (strcmp(type, "ds18b20") == 0)
            {
                const char *addr = o["addr"] | "";
                if (addr && addr[0] != '\0')
                    info = addr;
            }
            const char *name = nullptr;
            if (o["name"].is<const char *>())
                name = o["name"].as<const char *>();
            _c._meteo_cli.printRow(unit.c_str(), id, enabled, name, type, temp_str, hum_str, info);
        }
        if (done)
            finishStackMeteo_();
        _c.refreshPrompt_();
        return true;
    }

    void finishStackMeteo_()
    {
        if (_pending_meteo_left > 0)
            --_pending_meteo_left;
        if (_pending_meteo_left == 0)
            _pending_meteo_scan = false;
    }

    bool handleStackThermoReply_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_pending_thermo_scan)
            return false;
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return false;

        static DynamicJsonDocument doc(4096);
        doc.clear();
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return false;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        if (cmd_id != _pending_thermo_cmd_id)
            return false;

        if (frame.type == (uint8_t)StackMsgType::Err || !(doc["ok"] | false))
        {
            finishStackThermo_();
            _c.refreshPrompt_();
            return true;
        }

        JsonObjectConst data = doc["data"].as<JsonObjectConst>();
        const uint16_t part = data["part"] | 1;
        const uint16_t parts = data["parts"] | 1;
        const bool done = data["done"].is<bool>() ? data["done"].as<bool>() : (part >= parts);
        JsonArrayConst items = data["items"].as<JsonArrayConst>();
        if (items.isNull() || items.size() == 0)
        {
            if (done)
            {
                finishStackThermo_();
                _c.refreshPrompt_();
            }
            return true;
        }

        const String unit = stackNodeLabel_(node_id);
        for (JsonObjectConst o : items)
        {
            ThermoController::DeviceConfig cfg{};
            ThermoController::DeviceState st{};
            cfg.id = (uint8_t)(o["id"] | 0);
            cfg.enabled = o["enabled"] | false;
            if (o["name"].is<const char *>())
                cfg.name = o["name"].as<const char *>();
            cfg.sensor_id = (uint8_t)(o["sensor"] | 0);
            cfg.mode = parseThermoMode_(o["mode"] | "off");
            cfg.target_c = o["target"] | 0.0f;
            cfg.hysteresis = o["hyst"] | 0.0f;
            cfg.heat_port = ThermoController::kInvalidPort;
            cfg.cool_port = ThermoController::kInvalidPort;
            cfg.button_port = ThermoController::kInvalidPort;
            if (o["heat"].is<unsigned>())
                cfg.heat_port = (uint8_t)o["heat"].as<unsigned>();
            if (o["cool"].is<unsigned>())
                cfg.cool_port = (uint8_t)o["cool"].as<unsigned>();
            if (o["button"].is<unsigned>())
                cfg.button_port = (uint8_t)o["button"].as<unsigned>();
            st.power_on = o["power_on"] | false;
            st.heat_on = o["heat_on"] | false;
            st.cool_on = o["cool_on"] | false;
            _c._thermo_cli.printRow(unit.c_str(), cfg, st);
        }
        if (done)
            finishStackThermo_();
        _c.refreshPrompt_();
        return true;
    }

    void finishStackThermo_()
    {
        if (_pending_thermo_left > 0)
            --_pending_thermo_left;
        if (_pending_thermo_left == 0)
            _pending_thermo_scan = false;
    }

    bool handleStackExtReply_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_pending_ext_scan)
            return false;
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return false;

        static DynamicJsonDocument doc(4096);
        doc.clear();
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return false;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        if (cmd_id != _pending_ext_cmd_id)
            return false;

        if (frame.type == (uint8_t)StackMsgType::Err || !(doc["ok"] | false))
        {
            finishStackExt_();
            _c.refreshPrompt_();
            return true;
        }

        JsonArrayConst items = doc["data"]["items"].as<JsonArrayConst>();
        if (items.isNull() || items.size() == 0)
        {
            finishStackExt_();
            _c.refreshPrompt_();
            return true;
        }

        const String unit = stackNodeLabel_(node_id);
        for (JsonObjectConst o : items)
        {
            const uint8_t id = (uint8_t)(o["id"] | 0);
            const uint8_t bus = (uint8_t)(o["bus"] | 0);
            const char *addr = o["addr"] | "--";
            const char *type = o["type"] | "--";
            const bool present = o["present"] | false;
            if (!present)
                continue;
            _c.printExtRow_(unit, id, bus, addr, nullptr, type);
        }
        finishStackExt_();
        _c.refreshPrompt_();
        return true;
    }

    void finishStackExt_()
    {
        if (_pending_ext_left > 0)
            --_pending_ext_left;
        if (_pending_ext_left == 0)
            _pending_ext_scan = false;
    }

    bool handleStackPlcReply_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_pending_plc_scan)
            return false;
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return false;

        static DynamicJsonDocument doc(4096);
        doc.clear();
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return false;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        int idx = findStackPlcIndex_(node_id);
        if (idx < 0)
            return false;

        if (cmd_id == _pending_plc_cmd_id)
        {
            const bool ok = doc["ok"] | false;
            _plc_pending[idx].plc_ok = ok && frame.type == (uint8_t)StackMsgType::Ack;
            if (!_plc_pending[idx].plc_ok)
            {
                _plc_pending[idx].got_plc = true;
                tryPrintStackPlc_(idx);
                _c.refreshPrompt_();
                return true;
            }
            _plc_pending[idx].fan_on = doc["data"]["fan_on"] | false;
            _plc_pending[idx].board_temp = doc["data"]["board_temp"] | 0.0f;
            _plc_pending[idx].cpu_temp = doc["data"]["cpu_temp"] | 0.0f;
            _plc_pending[idx].on_c = doc["data"]["on_c"] | 0.0f;
            _plc_pending[idx].hyst_c = doc["data"]["hyst_c"] | 0.0f;
            _plc_pending[idx].got_plc = true;
            tryPrintStackPlc_(idx);
            _c.refreshPrompt_();
            return true;
        }
        if (cmd_id == _pending_plc_rtc_cmd_id)
        {
            const bool ok = doc["ok"] | false;
            _plc_pending[idx].rtc_ok = ok && frame.type == (uint8_t)StackMsgType::Ack;
            if (_plc_pending[idx].rtc_ok)
            {
                _plc_pending[idx].rtc_temp = doc["data"]["temp_c"] | 0.0f;
            }
            _plc_pending[idx].got_rtc = true;
            tryPrintStackPlc_(idx);
            _c.refreshPrompt_();
            return true;
        }
        return false;
    }

    void tryPrintStackPlc_(int idx)
    {
        if (idx < 0)
            return;
        auto &row = _plc_pending[idx];
        if (!row.in_use || row.done)
            return;
        if (!row.got_plc || !row.got_rtc)
            return;
        row.done = true;
        if (row.plc_ok && row.rtc_ok)
            _c.printPlcRow_(stackNodeLabel_(row.node_id), row.device_name, row.fan_on,
                            row.board_temp, row.cpu_temp, row.on_c, row.hyst_c,
                            &row.rtc_temp);
        finishStackPlcRow_();
    }

    void finishStackPlcRow_()
    {
        if (_pending_plc_done < _pending_plc_total)
            ++_pending_plc_done;
        if (_pending_plc_done >= _pending_plc_total)
            _pending_plc_scan = false;
    }

    void initStackPlcPending_(size_t count)
    {
        _pending_plc_cmd_id = nextStackCmdId_();
        _pending_plc_rtc_cmd_id = nextStackCmdId_();
        _pending_plc_total = (uint8_t)min<size_t>(count, kMaxStackNodes);
        _pending_plc_done = 0;
        _pending_plc_scan = true;
        for (size_t i = 0; i < kMaxStackNodes; ++i)
            _plc_pending[i] = {};
        for (size_t i = 0; i < _pending_plc_total; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            _plc_pending[i].in_use = true;
            _plc_pending[i].node_id = id;
            _plc_pending[i].device_name = _stack_master->nodeNameAt(i);
        }
    }

    int findStackPlcIndex_(uint32_t node_id) const
    {
        for (size_t i = 0; i < kMaxStackNodes; ++i)
            if (_plc_pending[i].in_use && _plc_pending[i].node_id == node_id)
                return (int)i;
        return -1;
    }

    bool handleStackRtcReply_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_pending_rtc_scan)
            return false;
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return false;

        static DynamicJsonDocument doc(4096);
        doc.clear();
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return false;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        if (cmd_id != _pending_rtc_cmd_id)
            return false;

        if (frame.type == (uint8_t)StackMsgType::Err || !(doc["ok"] | false))
        {
            finishStackRtc_();
            _c.refreshPrompt_();
            return true;
        }

        const char *date = doc["data"]["date"] | "--";
        const char *time = doc["data"]["time"] | "--";
        const unsigned weekday = doc["data"]["weekday"] | 0;
        const String unit = stackNodeLabel_(node_id);
        _c.printRtcRow_(unit, date, time, weekday);
        finishStackRtc_();
        _c.refreshPrompt_();
        return true;
    }

    void finishStackRtc_()
    {
        if (_pending_rtc_left > 0)
            --_pending_rtc_left;
        if (_pending_rtc_left == 0)
            _pending_rtc_scan = false;
    }

    uint16_t nextStackCmdId_()
    {
        ++_stack_cmd_id;
        if (_stack_cmd_id == 0)
            _stack_cmd_id = 1;
        return _stack_cmd_id;
    }

    String stackNodeLabel_(uint32_t node_id) const
    {
        if (_stack_master)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
                if (_stack_master->nodeIdAt(i) == node_id)
                {
                    String name = _stack_master->nodeNameAt(i);
                    if (name.length() > 0)
                        return name;
                    break;
                }
        }
        char buf[12] = {};
        snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)node_id);
        return String(buf);
    }

    static const __FlashStringHelper *stackMsgName_(uint8_t type)
    {
        switch (type)
        {
        case (uint8_t)StackMsgType::Hello:
            return F("hello");
        case (uint8_t)StackMsgType::Features:
            return F("features");
        case (uint8_t)StackMsgType::Status:
            return F("status");
        case (uint8_t)StackMsgType::CmdSet:
            return F("cmd_set");
        case (uint8_t)StackMsgType::CmdGet:
            return F("cmd_get");
        case (uint8_t)StackMsgType::Ack:
            return F("ack");
        case (uint8_t)StackMsgType::Err:
            return F("err");
        default:
            return F("unknown");
        }
    }

    static const __FlashStringHelper *stackRoleName_(ConfigsManagerIface::StackRole role)
    {
        return (role == ConfigsManagerIface::StackRole::Master) ? F("master") : F("slave");
    }

    static ThermoController::Mode parseThermoMode_(const char *mode)
    {
        if (!mode || mode[0] == '\0')
            return ThermoController::Mode::Off;
        if (strcmp(mode, "heat") == 0 || strcmp(mode, "heat_only") == 0 || strcmp(mode, "only_heat") == 0)
            return ThermoController::Mode::Heat;
        if (strcmp(mode, "cool") == 0 || strcmp(mode, "cool_only") == 0 || strcmp(mode, "only_cool") == 0)
            return ThermoController::Mode::Cool;
        if (strcmp(mode, "auto") == 0)
            return ThermoController::Mode::Auto;
        return ThermoController::Mode::Off;
    }

    const char *mapStackLocToExt_(const char *loc) const
    {
        if (!loc || loc[0] == '\0')
            return "--";
        if (strcmp(loc, "CPU") == 0)
            return "CPU";
        if (strncmp(loc, "UNIT_", 5) != 0)
            return loc;
        const char *num = loc + 5;
        if (strcmp(num, "1") == 0)
            return "EXT_1";
        if (strcmp(num, "2") == 0)
            return "EXT_2";
        if (strcmp(num, "3") == 0)
            return "EXT_3";
        if (strcmp(num, "4") == 0)
            return "EXT_4";
        if (strcmp(num, "5") == 0)
            return "EXT_5";
        if (strcmp(num, "6") == 0)
            return "EXT_6";
        if (strcmp(num, "7") == 0)
            return "EXT_7";
        if (strcmp(num, "8") == 0)
            return "EXT_8";
        if (strcmp(num, "9") == 0)
            return "EXT_9";
        if (strcmp(num, "10") == 0)
            return "EXT_10";
        return "UNKNOWN";
    }

    static bool isSocketIdValid_(uint16_t id)
    {
        return id >= 1 && id <= SocketController::kSocketCount;
    }

    static bool isThermoIdValid_(uint16_t id)
    {
        return id >= 1 && id <= ThermoController::kDeviceCount;
    }

    void printSocketIdRangeInline_() const
    {
        _c._io->print(F(" (1.."));
        _c._io->print(SocketController::kSocketCount);
        _c._io->print(F(")"));
    }

    void printThermoIdRangeInline_() const
    {
        _c._io->print(F(" (1.."));
        _c._io->print(ThermoController::kDeviceCount);
        _c._io->print(F(")"));
    }

    void printInvalidSocketId_() const
    {
        _c._io->print(F("Invalid socket id (1.."));
        _c._io->print(SocketController::kSocketCount);
        _c._io->println(F(")"));
    }

    void printInvalidThermoId_() const
    {
        _c._io->print(F("Invalid thermo id (1.."));
        _c._io->print(ThermoController::kDeviceCount);
        _c._io->println(F(")"));
    }
};
