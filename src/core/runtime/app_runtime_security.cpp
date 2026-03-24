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
    (void)out;
    (void)plain_out;
    if (!stackMasterActive_())
        return false;
    return false;
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

