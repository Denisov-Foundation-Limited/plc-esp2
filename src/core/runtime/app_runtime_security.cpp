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

namespace
{
constexpr uint32_t kSecurityPrearmRefreshMs = 4000u;
constexpr uint32_t kSecurityPrearmPollThrottleMs = 750u;
}
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
    SecurityController::RemoteDetect remote{};
    const size_t remote_count = sec.remoteDetectCount();
    for (size_t i = 0; i < remote_count; ++i)
    {
        if (!sec.remoteDetectAt(i, remote) || !remote.active || remote.silent)
            continue;
        if (remote.sensor_id > 0 && remote.sensor_id <= 32)
            detail_mask |= (1u << (remote.sensor_id - 1));
        const int unit_idx = stackNodeIndex_(remote.node_id);
        if (unit_idx >= 0 && unit_idx < 32)
            unit_mask |= (1u << (uint8_t)unit_idx);
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
    {
        core.logs.warn(F("SECURITY"), F("remote detect skipped: stack slave inactive id: %u"),
                       (unsigned)sensor_id);
        return;
    }
    if (!net.network.stackSlaveAuthorized())
    {
        core.logs.warn(F("SECURITY"), F("remote detect queued: stack slave unauthorized id: %u"),
                       (unsigned)sensor_id);
        _pending_detect = true;
        _pending_sensor_id = sensor_id;
        _pending_sensor_name = name;
        _pending_sensor_silent = silent;
        return;
    }
    StaticJsonDocument<192> doc;
    doc["alarm"] = true;
    doc["sensor_id"] = sensor_id;
    if (name.length())
        doc["name"] = name;
    if (silent)
        doc["silent"] = true;
    if (!net.network.stackRoute().sendEvent(0, "security", "alarm", &doc, StackRouteAdapter::Mode::Json))
    {
        core.logs.warn(F("SECURITY"), F("remote detect send failed: id: %u name: %s"),
                       (unsigned)sensor_id, name.length() ? name.c_str() : "-");
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
    if (!net.network.stackSlaveAuthorized())
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
    out = "";
    if (plain_out)
        *plain_out = "";
    bool any = false;
    const uint32_t now = millis();
    SecurityController &sec = control.controllers.security();
    SecurityController::RemoteDetect remote{};
    const size_t remote_count = sec.remoteDetectCount();
    for (size_t i = 0; i < remote_count; ++i)
    {
        if (!sec.remoteDetectAt(i, remote) || !remote.active)
            continue;
        if (any)
            out += F("\n");
        const String unit = remote.unit_name.length() ? remote.unit_name : stackNodeLabel_(remote.node_id);
        out += unit.length() ? escapeHtml_(unit) : String(F("stack"));
        out += F(": ");
        out += String((unsigned)remote.sensor_id);
        if (remote.sensor_name.length())
        {
            out += F(" (");
            out += F("<b>");
            out += escapeHtml_(remote.sensor_name);
            out += F("</b>");
            out += F(")");
        }
        if (remote.silent)
            out += F(" [silent]");
        if (plain_out)
        {
            if (plain_out->length())
                *plain_out += F(", ");
            if (unit.length())
            {
                *plain_out += unit;
                *plain_out += F(": ");
            }
            *plain_out += String((unsigned)remote.sensor_id);
            if (remote.sensor_name.length())
            {
                *plain_out += F(" (");
                *plain_out += remote.sensor_name;
                *plain_out += F(")");
            }
            if (remote.silent)
                *plain_out += F(" [silent]");
        }
        any = true;
    }
    const size_t node_count = net.network.stackOnlineDeviceCount();
    const bool allow_refresh = (_last_prearm_poll_ms == 0) ||
                               (uint32_t)(now - _last_prearm_poll_ms) >= kSecurityPrearmPollThrottleMs;
    for (size_t i = 0; i < node_count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
            continue;
        StackUnitSnapshot::State snapshot{};
        const bool have_snapshot = net.network.stackIndexState(device.node_id, snapshot);
        const uint32_t snapshot_age_ms = (have_snapshot && snapshot.updated_ms != 0) ? (uint32_t)(now - snapshot.updated_ms) : 0;
        const bool should_refresh = !have_snapshot || snapshot.updated_ms == 0 || snapshot_age_ms > kSecurityPrearmRefreshMs;
        if (should_refresh && allow_refresh)
        {
            net.network.stackRoute().sendRequest(device.node_id, "controllers", "summary_req", nullptr,
                                                 StackRouteAdapter::Mode::Json, true);
            _last_prearm_poll_ms = now;
        }
        if (!have_snapshot || snapshot.updated_ms == 0)
            continue;
        if (!snapshot.security_enabled || snapshot.security_detected == 0)
            continue;
        if (sec.remoteDetectCount(device.node_id) > 0)
            continue;
        const String unit = device.name[0] ? String(device.name) : stackNodeLabel_(device.node_id);
        if (snapshot.security_detect_preview_count > 0)
        {
            for (uint8_t preview_idx = 0; preview_idx < snapshot.security_detect_preview_count; ++preview_idx)
            {
                const auto &preview = snapshot.security_detect_preview[preview_idx];
                if (preview.id == 0)
                    continue;
                if (any)
                    out += F("\n");
                out += unit.length() ? escapeHtml_(unit) : String(F("stack"));
                out += F(": ");
                out += String((unsigned)preview.id);
                if (preview.name[0] != '\0')
                {
                    out += F(" (");
                    out += F("<b>");
                    out += escapeHtml_(String(preview.name));
                    out += F("</b>)");
                }
                if (plain_out)
                {
                    if (plain_out->length())
                        *plain_out += F(", ");
                    if (unit.length())
                    {
                        *plain_out += unit;
                        *plain_out += F(": ");
                    }
                    *plain_out += String((unsigned)preview.id);
                    if (preview.name[0] != '\0')
                    {
                        *plain_out += F(" (");
                        *plain_out += preview.name;
                        *plain_out += F(")");
                    }
                }
                any = true;
            }
            if (snapshot.security_detected > snapshot.security_detect_preview_count)
            {
                if (any)
                    out += F("\n");
                out += unit.length() ? escapeHtml_(unit) : String(F("stack"));
                out += F(": еще активных датчиков: ");
                out += String((unsigned)(snapshot.security_detected - snapshot.security_detect_preview_count));
                if (plain_out)
                {
                    if (plain_out->length())
                        *plain_out += F(", ");
                    if (unit.length())
                    {
                        *plain_out += unit;
                        *plain_out += F(": ");
                    }
                    *plain_out += F("еще активных датчиков: ");
                    *plain_out += String((unsigned)(snapshot.security_detected - snapshot.security_detect_preview_count));
                }
                any = true;
            }
        }
        else
        {
            if (any)
                out += F("\n");
            out += unit.length() ? escapeHtml_(unit) : String(F("stack"));
            out += F(": активные датчики: ");
            out += String((unsigned)snapshot.security_detected);
            if (plain_out)
            {
                if (plain_out->length())
                    *plain_out += F(", ");
                if (unit.length())
                {
                    *plain_out += unit;
                    *plain_out += F(": ");
                }
                *plain_out += F("активные датчики: ");
                *plain_out += String((unsigned)snapshot.security_detected);
            }
            any = true;
        }
    }
    return any;
}

void AppRuntime::syncRemoteSecurityAlarmFromSummary_(uint32_t node_id){
    if (!stackMasterActive_() || node_id == 0)
        return;
    const String source = stackNodeLabel_(node_id);
    core.logs.warn(F("SECURITY"), F("remote alarm sync: unit: %s source: summary"),
                   source.length() ? source.c_str() : "stack");
    const int unit_idx = stackNodeIndex_(node_id);
    if (unit_idx >= 0 && unit_idx < 32)
        hw.plc.setAlarmUnit(PlcControl::AlarmModule::Security, (uint8_t)unit_idx, true);
    control.controllers.security().setAlarmState(true);
    broadcastSecurityAlarm_(true);
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
    return;
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

