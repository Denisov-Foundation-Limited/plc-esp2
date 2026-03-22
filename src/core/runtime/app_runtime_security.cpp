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

#include "core/runtime/app_runtime.hpp"

#include "app.hpp"
void AppRuntime::updateSecurityAlarms_(){
    SecurityController &sec = control.controllers.security();
    auto sec_guard = sec.lockGuard();
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
        const size_t count = net.network.stackOnlineDeviceCount();
        for (size_t i = 0; i < count; ++i)
        {
            StackDeviceRegistry::DeviceInfo device{};
            if (!net.network.stackDeviceSnapshotAt(i, device))
                continue;
            const uint32_t node_id = device.node_id;
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

void AppRuntime::onSecurityArmState_(void *ctx, bool armed){
    if (!ctx)
        return;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    self->broadcastSecurityState_(armed);
    if (!armed)
        self->broadcastSecurityAlarm_(false);
}

bool AppRuntime::onSecurityPreArmCheck_(void *ctx, String &out, String *plain_out){
    if (!ctx)
        return false;
    return static_cast<AppRuntime *>(ctx)->collectRemoteSecurityDetections_(out, plain_out);
}

void AppRuntime::onSecurityAlarmState_(void *ctx, bool alarm_on){
    if (!ctx)
        return;
    static_cast<AppRuntime *>(ctx)->broadcastSecurityAlarm_(alarm_on);
}

void AppRuntime::onSecurityClearDetect_(void *ctx){
    if (!ctx)
        return;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    self->hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Security, 0);
    self->hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Security, 0);
    self->broadcastSecurityClear_();
}

void AppRuntime::onSecurityDetect_(void *ctx, uint8_t sensor_id, const String &name, bool silent){
    if (!ctx)
        return;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    if (!silent && sensor_id > 0 && sensor_id <= 32)
        self->hw.plc.setAlarmDetail(PlcControl::AlarmModule::Security, (uint8_t)(sensor_id - 1), true);
    self->sendSecurityDetectToMaster_(sensor_id, name, silent);
}

bool AppRuntime::onSecurityRfidUid_(void *ctx, const String &uid){
    if (!ctx)
        return false;
    return static_cast<AppRuntime *>(ctx)->handleSecurityRfidUid_(uid);
}

bool AppRuntime::onSecurityIButtonSerial_(void *ctx, const String &serial){
    if (!ctx)
        return false;
    return static_cast<AppRuntime *>(ctx)->handleSecurityIButtonSerial_(serial);
}

void AppRuntime::broadcastSecurityState_(bool armed){
    if (!stackMasterActive_())
        return;
    const size_t count = net.network.stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device))
            continue;
        sendSecurityStateToNode_(device.node_id, armed, false);
    }
}

void AppRuntime::sendSecurityStateToNode_(uint32_t node_id){
    auto &sec = control.controllers.security();
    auto sec_guard = sec.lockGuard();
    sendSecurityStateToNode_(node_id, sec.armed(), false);
}

void AppRuntime::sendSecurityStateToNode_(uint32_t node_id, bool armed, bool force){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StaticJsonDocument<128> doc;
    doc["armed"] = armed;
    auto &sec = control.controllers.security();
    auto sec_guard = sec.lockGuard();
    doc["alarm"] = armed ? sec.alarmOn() : false;
    if (force && armed)
        doc["force"] = true;
    net.network.stackRoute().sendEvent(node_id, "security", "set", &doc, StackRouteAdapter::Mode::Json);
}

void AppRuntime::sendSecurityDetectToMaster_(uint8_t sensor_id, const String &name, bool silent){
    if (!stackSlaveActive_())
        return;
    StaticJsonDocument<192> doc;
    doc["alarm"] = true;
    doc["sensor_id"] = sensor_id;
    if (name.length())
        doc["name"] = name;
    if (silent)
        doc["silent"] = true;
    if (!net.network.stackRoute().sendEvent(0, "security", "alarm", &doc, StackRouteAdapter::Mode::Json))
    {
        _pending_detect = true;
        _pending_sensor_id = sensor_id;
        _pending_sensor_name = name;
        _pending_sensor_silent = silent;
    }
}

void AppRuntime::updateSecurityNotifyMode_(){
    control.controllers.security().setNotifyEnabled(stackMasterActive_());
}

void AppRuntime::flushPendingSecurityDetect_(){
    if (!_pending_detect)
        return;
    if (!stackSlaveActive_())
        return;
    _pending_detect = false;
    sendSecurityDetectToMaster_(_pending_sensor_id, _pending_sensor_name, _pending_sensor_silent);
}

void AppRuntime::flushPendingRfid_(){
    if (!_pending_rfid)
        return;
    if (!stackSlaveActive_())
        return;
    _pending_rfid = false;
    sendRfidToMaster_(_pending_rfid_uid, hw.plc.deviceName());
}

void AppRuntime::flushPendingIButton_(){
    if (!_pending_ibutton)
        return;
    if (!stackSlaveActive_())
        return;
    _pending_ibutton = false;
    sendIButtonToMaster_(_pending_ibutton_serial, hw.plc.deviceName());
}

bool AppRuntime::handleSecurityRfidUid_(const String &uid_str){
    if (!stackSlaveActive_())
        return false;
    if (uid_str.length() == 0)
        return false;
    const String name = hw.plc.deviceName();
    if (!sendRfidToMaster_(uid_str, name))
    {
        _pending_rfid = true;
        _pending_rfid_uid = uid_str;
        return true;
    }
    return true;
}

bool AppRuntime::handleSecurityIButtonSerial_(const String &serial){
    if (!stackSlaveActive_())
        return false;
    if (serial.length() == 0)
        return false;
    const String name = hw.plc.deviceName();
    if (!sendIButtonToMaster_(serial, name))
    {
        _pending_ibutton = true;
        _pending_ibutton_serial = serial;
        return true;
    }
    return true;
}

bool AppRuntime::sendRfidToMaster_(const String &uid, const String &name){
    if (!stackSlaveActive_())
        return false;
    StaticJsonDocument<128> doc;
    doc["uid"] = uid;
    if (name.length())
        doc["name"] = name;
    return net.network.stackRoute().sendEvent(0, "security", "rfid", &doc, StackRouteAdapter::Mode::Json);
}

bool AppRuntime::sendIButtonToMaster_(const String &serial, const String &name){
    if (!stackSlaveActive_())
        return false;
    StaticJsonDocument<128> doc;
    doc["serial"] = serial;
    if (name.length())
        doc["name"] = name;
    return net.network.stackRoute().sendEvent(0, "security", "ibutton", &doc, StackRouteAdapter::Mode::Json);
}

void AppRuntime::sendRfidResultToNode_(uint32_t node_id, const String &uid, bool matched,
                           const String &result, bool armed){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StaticJsonDocument<160> doc;
    doc["uid"] = uid;
    doc["match"] = matched;
    if (result.length())
        doc["result"] = result;
    doc["armed"] = armed;
    net.network.stackRoute().sendEvent(node_id, "security", "rfid_result", &doc, StackRouteAdapter::Mode::Json);
}

void AppRuntime::sendIButtonResultToNode_(uint32_t node_id, const String &serial, bool matched,
                              const String &result, bool armed){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StaticJsonDocument<176> doc;
    doc["serial"] = serial;
    doc["match"] = matched;
    if (result.length())
        doc["result"] = result;
    doc["armed"] = armed;
    net.network.stackRoute().sendEvent(node_id, "security", "ibutton_result", &doc, StackRouteAdapter::Mode::Json);
}

void AppRuntime::pollSecurityStatusFromMaster_(){
    if (!stackSlaveActive_())
        return;
    const uint32_t now = millis();
    if ((uint32_t)(now - _last_rfid_status_ms) < 5000u)
        return;
    _last_rfid_status_ms = now;
    StaticJsonDocument<128> doc;
    net.network.stackRoute().sendRequest(0, "security", "status_req", &doc, StackRouteAdapter::Mode::Json, false);
}

bool AppRuntime::collectRemoteSecurityDetections_(String &out, String *plain_out){
    if (!stackMasterActive_())
        return false;
    const size_t count = net.network.stackOnlineDeviceCount();
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
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device))
            continue;
        const uint32_t node_id = device.node_id;
        if (node_id == 0 || !device.online || (uint32_t)(now - device.last_seen_ms) > kStackNodeStaleMs)
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
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device))
            continue;
        const uint32_t node_id = device.node_id;
        if (node_id == 0 || !device.online || (uint32_t)(millis() - device.last_seen_ms) > kStackNodeStaleMs)
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

void AppRuntime::broadcastSecurityAlarm_(bool alarm_on){
    if (!stackMasterActive_())
        return;
    const size_t count = net.network.stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device))
            continue;
        sendSecurityAlarmToNode_(device.node_id, alarm_on);
    }
}

void AppRuntime::sendSecurityAlarmToNode_(uint32_t node_id, bool alarm_on){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StaticJsonDocument<128> doc;
    doc["alarm"] = alarm_on;
    net.network.stackRoute().sendEvent(node_id, "security", "set", &doc, StackRouteAdapter::Mode::Json);
}

void AppRuntime::sendSecurityBeepToNode_(uint32_t node_id, const char *kind){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StaticJsonDocument<96> doc;
    doc["beep"] = kind ? kind : "reject";
    net.network.stackRoute().sendEvent(node_id, "security", "set", &doc, StackRouteAdapter::Mode::Json);
}

void AppRuntime::pollSecurityPrearmWarmup_(){
    if (!stackMasterActive_())
        return;
    const uint32_t now = millis();
    if ((uint32_t)(now - _last_prearm_poll_ms) < kPreArmPollMs)
        return;
    _last_prearm_poll_ms = now;
    const size_t count = net.network.stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device))
            continue;
        const uint32_t node_id = device.node_id;
        if (node_id == 0)
            continue;
        if (!device.online || (uint32_t)(now - device.last_seen_ms) > kStackNodeStaleMs)
            continue;
        const auto *cache = _stack_cache.securityPrearmCache(node_id);
        const bool stale = cache && cache->has_data && (uint32_t)(now - cache->updated_ms) > kPreArmFreshMs;
        if (!cache || !cache->has_data || !cache->items || !cache->last_ok || stale || cache->pending)
            _stack_cache.requestSecurityPrearm(node_id);
    }
}

void AppRuntime::broadcastSecurityClear_(){
    if (!stackMasterActive_())
        return;
    const size_t count = net.network.stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device))
            continue;
        sendSecurityClearToNode_(device.node_id);
    }
}

void AppRuntime::sendSecurityClearToNode_(uint32_t node_id){
    if (node_id == 0)
        return;
    if (!stackMasterActive_())
        return;
    StaticJsonDocument<96> doc;
    doc["clear"] = true;
    net.network.stackRoute().sendEvent(node_id, "security", "set", &doc, StackRouteAdapter::Mode::Json);
}

