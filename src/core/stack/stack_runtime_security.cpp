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

#include "app.hpp"
void StackRuntime::updateSecurityAlarms_(){
    SecurityController &sec = control.controllers.security();
    uint32_t detail_mask = 0;
    uint32_t unit_mask = 0;
    for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
    {
        const auto *cfg = sec.configByIndex(i);
        const auto *st = sec.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled || cfg->silent)
            continue;
        if (cfg->id == 0 || cfg->id > 32)
            continue;
        if (st->is_detect)
            detail_mask |= (1u << (cfg->id - 1));
    }
    if (stackMasterActive_())
    {
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t node_id = master.nodeIdAt(i);
            if (node_id == 0)
                continue;
            const auto *cache = _stack_cache.securityCache(node_id);
            if (!cache || !cache->has_data || !cache->items || !cache->last_ok)
                continue;
            for (size_t j = 0; j < cache->item_count; ++j)
            {
                const auto &it = cache->items[j];
                if (!it.enabled || it.silent)
                    continue;
                if (it.id == 0 || it.id > 32)
                    continue;
                if (it.detect)
                {
                    detail_mask |= (1u << (it.id - 1));
                    if (i < 32)
                        unit_mask |= (1u << i);
                }
            }
        }
    }
    hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Security, detail_mask);
    hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Security, unit_mask);
}

void StackRuntime::onSecurityArmState_(void *ctx, bool armed){
    if (!ctx)
        return;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    self->broadcastSecurityState_(armed);
    if (!armed)
        self->broadcastSecurityAlarm_(false);
}

bool StackRuntime::onSecurityPreArmCheck_(void *ctx, String &out, String *plain_out){
    if (!ctx)
        return false;
    return static_cast<StackRuntime *>(ctx)->collectRemoteSecurityDetections_(out, plain_out);
}

void StackRuntime::onSecurityAlarmState_(void *ctx, bool alarm_on){
    if (!ctx)
        return;
    static_cast<StackRuntime *>(ctx)->broadcastSecurityAlarm_(alarm_on);
}

void StackRuntime::onSecurityClearDetect_(void *ctx){
    if (!ctx)
        return;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    self->hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Security, 0);
    self->hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Security, 0);
    self->broadcastSecurityClear_();
}

void StackRuntime::onSecurityDetect_(void *ctx, uint8_t sensor_id, const String &name, bool silent){
    if (!ctx)
        return;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    if (!silent && sensor_id > 0 && sensor_id <= 32)
        self->hw.plc.setAlarmDetail(PlcControl::AlarmModule::Security, (uint8_t)(sensor_id - 1), true);
    self->sendSecurityDetectToMaster_(sensor_id, name, silent);
}

bool StackRuntime::onSecurityRfidUid_(void *ctx, const String &uid){
    if (!ctx)
        return false;
    return static_cast<StackRuntime *>(ctx)->handleSecurityRfidUid_(uid);
}

bool StackRuntime::onSecurityIButtonSerial_(void *ctx, const String &serial){
    if (!ctx)
        return false;
    return static_cast<StackRuntime *>(ctx)->handleSecurityIButtonSerial_(serial);
}

void StackRuntime::broadcastSecurityState_(bool armed){
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();
    const size_t count = master.nodeCount();
    for (size_t i = 0; i < count; ++i)
        sendSecurityStateToNode_(master.nodeIdAt(i), armed, false);
}

void StackRuntime::sendSecurityStateToNode_(uint32_t node_id){
    sendSecurityStateToNode_(node_id, control.controllers.security().armed(), false);
}

void StackRuntime::sendSecurityStateToNode_(uint32_t node_id, bool armed, bool force){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();

    StaticJsonDocument<128> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "set";
    JsonObject params = doc["params"].to<JsonObject>();
    params["armed"] = armed;
    params["alarm"] = armed ? control.controllers.security().alarmOn() : false;
    if (force && armed)
        params["force"] = true;

    char payload[128] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                  reinterpret_cast<const uint8_t *>(payload), len);
}

void StackRuntime::sendSecurityDetectToMaster_(uint8_t sensor_id, const String &name, bool silent){
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
    {
        _pending_detect = true;
        _pending_sensor_id = sensor_id;
        _pending_sensor_name = name;
        _pending_sensor_silent = silent;
        return;
    }
    StaticJsonDocument<192> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "alarm";
    JsonObject params = doc["params"].to<JsonObject>();
    params["alarm"] = true;
    params["sensor_id"] = sensor_id;
    if (name.length())
        params["name"] = name;
    if (silent)
        params["silent"] = true;

    char payload[160] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    if (!node.send((uint8_t)StackMsgType::CmdSet,
                   reinterpret_cast<const uint8_t *>(payload), len))
    {
        _pending_detect = true;
        _pending_sensor_id = sensor_id;
        _pending_sensor_name = name;
        _pending_sensor_silent = silent;
    }
}

void StackRuntime::updateSecurityNotifyMode_(){
    control.controllers.security().setNotifyEnabled(stackMasterActive_());
}

void StackRuntime::flushPendingSecurityDetect_(){
    if (!_pending_detect)
        return;
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return;
    _pending_detect = false;
    sendSecurityDetectToMaster_(_pending_sensor_id, _pending_sensor_name, _pending_sensor_silent);
}

void StackRuntime::flushPendingRfid_(){
    if (!_pending_rfid)
        return;
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return;
    _pending_rfid = false;
    sendRfidToMaster_(_pending_rfid_uid, hw.plc.deviceName());
}

void StackRuntime::flushPendingIButton_(){
    if (!_pending_ibutton)
        return;
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return;
    _pending_ibutton = false;
    sendIButtonToMaster_(_pending_ibutton_serial, hw.plc.deviceName());
}

bool StackRuntime::handleSecurityRfidUid_(const String &uid_str){
    if (!stackSlaveActive_())
        return false;
    if (uid_str.length() == 0)
        return false;
    const String name = hw.plc.deviceName();
    StackNode &node = net.network.stackNode();
    if (!node.connected())
    {
        _pending_rfid = true;
        _pending_rfid_uid = uid_str;
        return true;
    }
    if (!sendRfidToMaster_(uid_str, name))
    {
        _pending_rfid = true;
        _pending_rfid_uid = uid_str;
        return true;
    }
    return true;
}

bool StackRuntime::handleSecurityIButtonSerial_(const String &serial){
    if (!stackSlaveActive_())
        return false;
    if (serial.length() == 0)
        return false;
    const String name = hw.plc.deviceName();
    StackNode &node = net.network.stackNode();
    if (!node.connected())
    {
        _pending_ibutton = true;
        _pending_ibutton_serial = serial;
        return true;
    }
    if (!sendIButtonToMaster_(serial, name))
    {
        _pending_ibutton = true;
        _pending_ibutton_serial = serial;
        return true;
    }
    return true;
}

bool StackRuntime::sendRfidToMaster_(const String &uid, const String &name){
    if (!stackSlaveActive_())
        return false;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return false;
    StaticJsonDocument<128> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "rfid";
    JsonObject params = doc["params"].to<JsonObject>();
    params["uid"] = uid;
    if (name.length())
        params["name"] = name;
    char payload[128] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return false;
    return node.send((uint8_t)StackMsgType::CmdSet,
                     reinterpret_cast<const uint8_t *>(payload), len);
}

bool StackRuntime::sendIButtonToMaster_(const String &serial, const String &name){
    if (!stackSlaveActive_())
        return false;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return false;
    StaticJsonDocument<128> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "ibutton";
    JsonObject params = doc["params"].to<JsonObject>();
    params["serial"] = serial;
    if (name.length())
        params["name"] = name;
    char payload[128] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return false;
    return node.send((uint8_t)StackMsgType::CmdSet,
                     reinterpret_cast<const uint8_t *>(payload), len);
}

void StackRuntime::sendRfidResultToNode_(uint32_t node_id, const String &uid, bool matched,
                           const String &result, bool armed){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();
    StaticJsonDocument<160> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "rfid_result";
    JsonObject params = doc["params"].to<JsonObject>();
    params["uid"] = uid;
    params["match"] = matched;
    if (result.length())
        params["result"] = result;
    params["armed"] = armed;
    char payload[160] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                  reinterpret_cast<const uint8_t *>(payload), len);
}

void StackRuntime::sendIButtonResultToNode_(uint32_t node_id, const String &serial, bool matched,
                              const String &result, bool armed){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();
    StaticJsonDocument<176> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "ibutton_result";
    JsonObject params = doc["params"].to<JsonObject>();
    params["serial"] = serial;
    params["match"] = matched;
    if (result.length())
        params["result"] = result;
    params["armed"] = armed;
    char payload[176] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                  reinterpret_cast<const uint8_t *>(payload), len);
}

void StackRuntime::pollSecurityStatusFromMaster_(){
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return;
    const uint32_t now = millis();
    if ((uint32_t)(now - _last_rfid_status_ms) < 5000u)
        return;
    _last_rfid_status_ms = now;
    StaticJsonDocument<128> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "status_req";
    char payload[128] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    node.send((uint8_t)StackMsgType::CmdSet,
              reinterpret_cast<const uint8_t *>(payload), len);
}

bool StackRuntime::collectRemoteSecurityDetections_(String &out, String *plain_out){
    if (!stackMasterActive_())
        return false;
    StackMaster &master = net.network.stackMaster();
    const size_t count = master.nodeCount();
    bool any = false;
    bool missing = false;
    uint32_t beep_nodes[StackMaster::MAX_SESSIONS] = {};
    size_t beep_count = 0;
    auto mark_beep = [&beep_nodes, &beep_count](uint32_t node_id) {
        for (size_t i = 0; i < beep_count; ++i)
        {
            if (beep_nodes[i] == node_id)
                return;
        }
        if (beep_count < StackMaster::MAX_SESSIONS)
            beep_nodes[beep_count++] = node_id;
    };
    uint32_t pending_nodes[StackMaster::MAX_SESSIONS] = {};
    uint32_t pending_req_ms[StackMaster::MAX_SESSIONS] = {};
    size_t pending_count = 0;
    const uint32_t now = millis();
    for (size_t i = 0; i < count; ++i)
    {
        const uint32_t node_id = master.nodeIdAt(i);
        if (node_id == 0)
            continue;
        if (!master.nodeIsOnline(node_id, kStackNodeStaleMs))
            continue;
        const auto *cache = _stack_cache.securityPrearmCache(node_id);
        const bool stale = cache && cache->has_data && (uint32_t)(now - cache->updated_ms) > kPreArmFreshMs;
        if (!cache || cache->pending || !cache->has_data || !cache->items || !cache->last_ok || stale)
        {
            _stack_cache.requestSecurityPrearmForce(node_id);
            if (pending_count < StackMaster::MAX_SESSIONS)
            {
                pending_nodes[pending_count] = node_id;
                pending_req_ms[pending_count] = millis();
                ++pending_count;
            }
        }
    }
    if (pending_count)
    {
        const uint32_t wait_until = millis() + kPreArmWaitMs;
        bool any_pending = true;
        while (any_pending && (int32_t)(millis() - wait_until) < 0)
        {
            any_pending = false;
            for (size_t i = 0; i < pending_count; ++i)
            {
                const uint32_t node_id = pending_nodes[i];
                if (node_id == 0)
                    continue;
                const auto *cache = _stack_cache.securityPrearmCache(node_id);
                if (!cache)
                    continue;
                if (cache->pending || !cache->has_data || !cache->items || !cache->last_ok ||
                    cache->updated_ms < pending_req_ms[i])
                {
                    any_pending = true;
                }
            }
            if (any_pending)
                delay(20);
        }
    }
    for (size_t i = 0; i < count; ++i)
    {
        const uint32_t node_id = master.nodeIdAt(i);
        if (node_id == 0)
            continue;
        if (!master.nodeIsOnline(node_id, kStackNodeStaleMs))
            continue;
        const auto *cache = _stack_cache.securityPrearmCache(node_id);
        const uint32_t now2 = millis();
        uint32_t req_ms = 0;
        for (size_t j = 0; j < pending_count; ++j)
        {
            if (pending_nodes[j] == node_id)
            {
                req_ms = pending_req_ms[j];
                break;
            }
        }
        const bool stale = cache && cache->has_data && (uint32_t)(now2 - cache->updated_ms) > kPreArmFreshMs;
        if (!cache || cache->pending || !cache->has_data || !cache->items || !cache->last_ok || stale ||
            (req_ms != 0 && cache->updated_ms < req_ms))
        {
            const String label = stackNodeLabel_(node_id);
            if (out.length())
                out += F("\n");
            out += F("Р С•Р В¶Р С‘Р Т‘Р В°Р Р…Р С‘Р Вµ Р Т‘Р В°Р Р…Р Р…РЎвЂ№РЎвЂ¦: ");
            out += escapeHtml_(label);
            if (plain_out)
            {
                if (plain_out->length())
                    *plain_out += F(", ");
                *plain_out += F("Р С•Р В¶Р С‘Р Т‘Р В°Р Р…Р С‘Р Вµ Р Т‘Р В°Р Р…Р Р…РЎвЂ№РЎвЂ¦: ");
                *plain_out += label;
            }
            core.logs.warn(F("SECURITY"), F("prearm waiting data from %s"), label.c_str());
            missing = true;
            any = true;
            mark_beep(node_id);
            continue;
        }
        String line;
        String plain_line;
        bool node_any = false;
        for (size_t j = 0; j < cache->item_count; ++j)
        {
            const auto &it = cache->items[j];
            if (!node_any)
            {
                const String label = stackNodeLabel_(node_id);
                line += escapeHtml_(label);
                line += F(": ");
                plain_line += label;
                plain_line += F(": ");
            }
            else
            {
                line += F(", ");
                plain_line += F(", ");
            }
            line += String((unsigned)it.id);
            plain_line += String((unsigned)it.id);
            if (it.name[0])
            {
                const String name = String(it.name);
                line += F(" (");
                line += F("<b>");
                line += escapeHtml_(name);
                line += F("</b>");
                line += F(")");
                plain_line += F(" (");
                plain_line += name;
                plain_line += F(")");
            }
            core.logs.warn(F("SECURITY"), F("prearm blocked %s sensor %u (%s)"),
                           stackNodeLabel_(node_id).c_str(),
                           (unsigned)it.id,
                           it.name[0] ? it.name : "-");
            node_any = true;
        }
        if (!node_any)
            continue;
        mark_beep(node_id);
        if (out.length())
            out += F("\n");
        out += line;
        if (plain_out)
        {
            if (plain_out->length())
                *plain_out += F(", ");
            *plain_out += plain_line;
        }
        any = true;
    }
    if (beep_count)
    {
        for (size_t i = 0; i < beep_count; ++i)
            sendSecurityBeepToNode_(beep_nodes[i], "reject");
    }
    if (missing)
        return true;
    return any;
}

void StackRuntime::broadcastSecurityAlarm_(bool alarm_on){
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();
    const size_t count = master.nodeCount();
    for (size_t i = 0; i < count; ++i)
        sendSecurityAlarmToNode_(master.nodeIdAt(i), alarm_on);
}

void StackRuntime::sendSecurityAlarmToNode_(uint32_t node_id, bool alarm_on){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();

    StaticJsonDocument<128> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "set";
    JsonObject params = doc["params"].to<JsonObject>();
    params["alarm"] = alarm_on;

    char payload[128] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                  reinterpret_cast<const uint8_t *>(payload), len);
}

void StackRuntime::sendSecurityBeepToNode_(uint32_t node_id, const char *kind){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();

    StaticJsonDocument<96> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "set";
    JsonObject params = doc["params"].to<JsonObject>();
    params["beep"] = kind ? kind : "reject";

    char payload[96] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                  reinterpret_cast<const uint8_t *>(payload), len);
}

void StackRuntime::pollSecurityPrearmWarmup_(){
    if (!stackMasterActive_())
        return;
    const uint32_t now = millis();
    if ((uint32_t)(now - _last_prearm_poll_ms) < kPreArmPollMs)
        return;
    _last_prearm_poll_ms = now;
    StackMaster &master = net.network.stackMaster();
    const size_t count = master.nodeCount();
    for (size_t i = 0; i < count; ++i)
    {
        const uint32_t node_id = master.nodeIdAt(i);
        if (node_id == 0)
            continue;
        if (!master.nodeIsOnline(node_id, kStackNodeStaleMs))
            continue;
        const auto *cache = _stack_cache.securityPrearmCache(node_id);
        const bool stale = cache && cache->has_data && (uint32_t)(now - cache->updated_ms) > kPreArmFreshMs;
        if (!cache || !cache->has_data || !cache->items || !cache->last_ok || stale || cache->pending)
            _stack_cache.requestSecurityPrearm(node_id);
    }
}

void StackRuntime::broadcastSecurityClear_(){
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();
    const size_t count = master.nodeCount();
    for (size_t i = 0; i < count; ++i)
        sendSecurityClearToNode_(master.nodeIdAt(i));
}

void StackRuntime::sendSecurityClearToNode_(uint32_t node_id){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();

    StaticJsonDocument<96> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Security;
    doc["action"] = "set";
    JsonObject params = doc["params"].to<JsonObject>();
    params["clear"] = true;

    char payload[96] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                  reinterpret_cast<const uint8_t *>(payload), len);
}

