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
void AppRuntime::updateTankAlarms_(){
    TankController &tanks = control.controllers.tanks();
    auto tanks_guard = tanks.lockGuard();
    uint32_t detail_mask = 0;
    uint32_t unit_mask = 0;
    for (size_t i = 0; i < TankController::kTankCount; ++i)
    {
        const auto *cfg = tanks.configByIndex(i);
        const auto *st = tanks.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;
        if (cfg->id == 0 || cfg->id > 32)
            continue;
        const bool empty = !(st->level_low || st->level_mid || st->level_full);
        if (!st->levels_ok || empty)
            detail_mask |= (1u << (cfg->id - 1));
    }
    hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Tanks, detail_mask);
    hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Tanks, unit_mask);
}

void AppRuntime::updateSepticAlarms_(){
    SepticController &septic = control.controllers.septic();
    auto septic_guard = septic.lockGuard();
    uint32_t detail_mask = 0;
    uint32_t unit_mask = 0;
    for (size_t i = 0; i < SepticController::kSepticCount; ++i)
    {
        const auto *cfg = septic.configByIndex(i);
        const auto *st = septic.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;
        if (cfg->id == 0 || cfg->id > 32)
            continue;
        if (st->alarm)
            detail_mask |= (1u << (cfg->id - 1));
    }
    hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Septic, detail_mask);
    hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Septic, unit_mask);
}

void AppRuntime::updateMeteoAlarms_(){
    MeteoController &meteo = control.controllers.meteo();
    auto meteo_guard = meteo.lockGuard();
    uint32_t detail_mask = 0;
    uint32_t unit_mask = 0;
    for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
    {
        const auto *cfg = meteo.configByIndex(i);
        const auto *st = meteo.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;
        if (cfg->id == 0 || cfg->id > 32)
            continue;
        if (!st->ok)
            detail_mask |= (1u << (cfg->id - 1));
    }
    hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Meteo, detail_mask);
    hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Meteo, unit_mask);
}

void AppRuntime::onMeteoAlarm_(void *ctx, uint32_t node_id, uint8_t sensor_id, bool alarm){
    if (!ctx || sensor_id == 0 || sensor_id > 32)
        return;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    if (node_id != 0 && self->stackSlaveActive_())
        return;
    self->hw.plc.setAlarmDetail(PlcControl::AlarmModule::Meteo, (uint8_t)(sensor_id - 1), alarm);
    if (node_id == 0)
        return;
    const int unit_idx = self->stackNodeIndex_(node_id);
    if (unit_idx >= 0 && unit_idx < 32)
        self->hw.plc.setAlarmUnit(PlcControl::AlarmModule::Meteo, (uint8_t)unit_idx, alarm);
    DynamicJsonDocument doc(192);
    doc["sensor_id"] = sensor_id;
    doc["alarm"] = alarm;
    const String unit_name = self->stackNodeLabel_(node_id);
    if (unit_name.length())
        doc["unit_name"] = unit_name;
    String json;
    serializeJson(doc, json);
    self->publishCloudStackEvent_(node_id, "meteo.sensor", alarm ? "alarm" : "restore", json);
}

void AppRuntime::onStackNodeEvent_(void *ctx, uint32_t node_id, bool online){
    if (!ctx || node_id == 0)
        return;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    self->clearInventoryLogState_(node_id);
    String name;
    String ip;
    uint16_t fw_ver = 0;
    StackDeviceRegistry::DeviceInfo device{};
    if (self->net.network.stackDeviceSnapshotByNodeId(node_id, device))
    {
        name = device.name;
        ip = device.ip;
        fw_ver = device.fw_version;
    }
    const String label = name.length() ? name : self->stackNodeLabel_(node_id);
    if (online)
    {
        self->comms.wifi.task();
        self->net.network.clearStackIndexStatePending(node_id);
        self->net.network.invalidateStackIndexState(node_id);
        self->net.network.clearStackSocketsPageRequest(node_id);
        self->net.network.clearStackLightsPageRequest(node_id);
        self->net.network.clearStackMeteoPageRequest(node_id);
        self->net.network.clearStackThermoPageRequest(node_id);
        self->net.network.clearStackTanksPageRequest(node_id);
        self->core.logs.info(F("STACK"), F("Sync slave %s system info"), label.length() ? label.c_str() : "unknown");
        self->net.network.stackRoute().sendRequest(node_id, "system", "snapshot_req", nullptr,
                                                   StackRouteAdapter::Mode::Json, true);
        self->core.logs.info(F("STACK"), F("Sync slave %s controllers"), label.length() ? label.c_str() : "unknown");
        self->net.network.stackRoute().sendRequest(node_id, "controllers", "summary_req", nullptr,
                                                   StackRouteAdapter::Mode::Json, true);
        DynamicJsonDocument sockets_doc(64);
        sockets_doc["offset"] = 0;
        sockets_doc["limit"] = 8;
        self->core.logs.info(F("STACK"), F("Sync slave %s sockets: 0-7"),
                             label.length() ? label.c_str() : "unknown");
        self->net.network.stackRoute().sendRequest(node_id, "sockets", "snapshot_req", &sockets_doc,
                                                   StackRouteAdapter::Mode::Json, true);
        DynamicJsonDocument lights_doc(64);
        lights_doc["offset"] = 0;
        lights_doc["limit"] = 8;
        self->core.logs.info(F("STACK"), F("Sync slave %s lights: 0-7"),
                             label.length() ? label.c_str() : "unknown");
        self->net.network.stackRoute().sendRequest(node_id, "lights", "snapshot_req", &lights_doc,
                                                   StackRouteAdapter::Mode::Json, true);
        DynamicJsonDocument meteo_doc(64);
        meteo_doc["offset"] = 0;
        meteo_doc["limit"] = 8;
        self->core.logs.info(F("STACK"), F("Sync slave %s meteo: 0-7"),
                             label.length() ? label.c_str() : "unknown");
        self->net.network.stackRoute().sendRequest(node_id, "meteo", "snapshot_req", &meteo_doc,
                                                   StackRouteAdapter::Mode::Json, true);
        DynamicJsonDocument thermo_doc(64);
        thermo_doc["offset"] = 0;
        thermo_doc["limit"] = 8;
        self->core.logs.info(F("STACK"), F("Sync slave %s thermo: 0-7"),
                             label.length() ? label.c_str() : "unknown");
        self->net.network.stackRoute().sendRequest(node_id, "thermo", "snapshot_req", &thermo_doc,
                                                   StackRouteAdapter::Mode::Json, true);
        DynamicJsonDocument tanks_doc(64);
        tanks_doc["offset"] = 0;
        tanks_doc["limit"] = 8;
        self->core.logs.info(F("STACK"), F("Sync slave %s tanks: 0-7"),
                             label.length() ? label.c_str() : "unknown");
        self->net.network.stackRoute().sendRequest(node_id, "tanks", "snapshot_req", &tanks_doc,
                                                   StackRouteAdapter::Mode::Json, true);
        auto &sec = self->control.controllers.security();
        auto sec_guard = sec.lockGuard();
        self->sendSecurityStateToNode_(node_id, sec.armed(), true);
        self->enqueueStackBootstrapSync_(node_id);
    }
    else
    {
        self->removeStackBootstrapSync_(node_id);
    }
    DynamicJsonDocument doc(192);
    doc["online"] = online;
    if (label.length())
        doc["unit_name"] = label;
    if (ip.length())
        doc["ip"] = ip;
    doc["fw"] = fw_ver;
    String json;
    serializeJson(doc, json);
    self->publishCloudStackEvent_(node_id, "stack.node", online ? "online" : "offline", json);
}

void AppRuntime::onSepticDetect_(void *ctx, uint8_t septic_id, const String &name, bool is_alarm){
    if (!ctx)
        return;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    if (is_alarm && septic_id > 0 && septic_id <= 32)
        self->hw.plc.setAlarmDetail(PlcControl::AlarmModule::Septic, (uint8_t)(septic_id - 1), true);
    self->sendSepticDetectToMaster_(septic_id, name, is_alarm);
}

void AppRuntime::onTankEmpty_(void *ctx, uint8_t tank_id, const String &name, bool empty){
    if (!ctx)
        return;
    if (!empty)
        return;
    static_cast<AppRuntime *>(ctx)->sendTankEmptyToMaster_(tank_id, name);
}

void AppRuntime::onWateringEvent_(void *ctx, WateringController::Event ev,
                             const WateringController::RuleConfig &cfg,
                             const WateringController::RuleState &st){
    if (!ctx)
        return;
    static_cast<AppRuntime *>(ctx)->sendWateringEventToMaster_(ev, cfg, st);
}

void AppRuntime::onRingHold_(void *ctx, bool on){
    if (!ctx)
        return;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    const RingController::Source src = self->control.controllers.ring().lastSource();
    if (src == RingController::Source::Button && self->stackSlaveActive_())
    {
        if (!self->sendRingButtonToMaster_(on))
        {
            self->_pending_ring_button = true;
            self->_pending_ring_button_pressed = on;
        }
    }
    self->broadcastRingHold_(on);
    if (on)
        self->notifyRingHold_();
}

void AppRuntime::onStackRoute_(void *ctx, uint32_t source_node, const StackJsonProtocol::RouteMessage &route){
    if (!ctx || source_node == 0)
        return;
    static_cast<AppRuntime *>(ctx)->handleStackRoute_(source_node, route);
}

void AppRuntime::sendSepticDetectToMaster_(uint8_t septic_id, const String &name, bool is_alarm){
    if (!stackSlaveActive_())
        return;
    StaticJsonDocument<192> doc;
    doc["id"] = septic_id;
    doc["alarm"] = is_alarm;
    doc["level"] = is_alarm ? "alarm" : "warning";
    if (name.length())
        doc["name"] = name;
    if (!net.network.stackRoute().sendEvent(0, "septic", "level", &doc, StackRouteAdapter::Mode::Json))
    {
        _pending_septic_detect = true;
        _pending_septic_id = septic_id;
        _pending_septic_name = name;
        _pending_septic_alarm = is_alarm;
    }
}

void AppRuntime::sendTankEmptyToMaster_(uint8_t tank_id, const String &name){
    if (!stackSlaveActive_())
        return;
    StaticJsonDocument<160> doc;
    doc["id"] = tank_id;
    doc["empty"] = true;
    if (name.length())
        doc["name"] = name;
    if (!net.network.stackRoute().sendEvent(0, "tanks", "empty", &doc, StackRouteAdapter::Mode::Json))
    {
        _pending_tank_empty = true;
        _pending_tank_id = tank_id;
        _pending_tank_name = name;
    }
}

void AppRuntime::sendWateringEventToMaster_(WateringController::Event ev,
                                const WateringController::RuleConfig &cfg,
                                const WateringController::RuleState &st){
    if (!stackSlaveActive_())
        return;
    StaticJsonDocument<256> doc;
    doc["id"] = cfg.id;
    if (cfg.name.length())
        doc["name"] = cfg.name;
    if (cfg.port != WateringController::kInvalidPort)
        doc["port"] = cfg.port;
    if (cfg.tank_id)
        doc["tank"] = cfg.tank_id;
    if (cfg.resume_after_refill)
        doc["resume"] = true;
    doc["resume_level"] = cfg.resume_level;
    doc["remaining_ms"] = st.remaining_ms;
    const char *event_str = "stop";
    switch (ev)
    {
    case WateringController::Event::Start:
        event_str = "start";
        break;
    case WateringController::Event::PauseEmpty:
        event_str = "pause";
        doc["reason"] = "empty";
        break;
    case WateringController::Event::Resume:
        event_str = "resume";
        break;
    case WateringController::Event::StopDone:
        event_str = "stop";
        doc["reason"] = "done";
        break;
    case WateringController::Event::StopEmpty:
        event_str = "stop";
        doc["reason"] = "empty";
        break;
    case WateringController::Event::Stop:
    default:
        event_str = "stop";
        break;
    }
    doc["event"] = event_str;
    if (!net.network.stackRoute().sendEvent(0, "watering", "event", &doc, StackRouteAdapter::Mode::Json))
    {
        _pending_watering_event = true;
        _pending_watering_event_type = ev;
        _pending_watering_event_cfg = cfg;
        _pending_watering_event_state = st;
    }
}

void AppRuntime::broadcastRingHold_(bool on){
    if (!stackMasterActive_())
        return;
    const size_t count = net.network.stackOnlineDeviceCount();
    if (count == 0)
        return;

    StaticJsonDocument<64> doc;
    doc["state"] = on;
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device))
            continue;
        net.network.stackRoute().sendEvent(device.node_id, "ring", "set", &doc, StackRouteAdapter::Mode::Json);
    }
}

void AppRuntime::notifyRingHold_(){
    const RingController::Source src = control.controllers.ring().lastSource();
    if (src == RingController::Source::Button)
    {
        const String tg_msg = F("Звонок включен по кнопке");
        core.logs.info(F("RING"), F("Ring enabled by button"));
        sendCloudNotify_("ring.hold", "button", tg_msg);
        return;
    }
    if (src == RingController::Source::Web)
    {
        const String tg_msg = F("Звонок включен из веб-интерфейса");
        core.logs.info(F("RING"), F("Ring enabled from web"));
        sendCloudNotify_("ring.hold", "web", tg_msg);
        return;
    }
    if (src == RingController::Source::Cli)
    {
        const String tg_msg = F("Звонок включен из CLI");
        core.logs.info(F("RING"), F("Ring enabled from CLI"));
        sendCloudNotify_("ring.hold", "cli", tg_msg);
        return;
    }
    if (src == RingController::Source::Stack)
    {
        const String tg_msg = F("Звонок включен из стека");
        core.logs.info(F("RING"), F("Ring enabled from stack"));
        sendCloudNotify_("ring.hold", "stack", tg_msg);
        return;
    }
}

void AppRuntime::sendCloudNotify_(const char *kind, const char *reason, const String &msg){
    CloudClient &cloud = net.network.cloudClient();
    if (!kind || !kind[0] || !reason || !reason[0])
        return;
    DynamicJsonDocument doc(256);
    if (msg.length())
        doc["message"] = msg;
    String json;
    serializeJson(doc, json);
    cloud.publishEvent(kind, reason, json);
}

void AppRuntime::publishCloudStackEvent_(uint32_t node_id, const char *kind, const char *reason,
                                           const String &data_json){
    if (node_id == 0 || !kind || !kind[0] || !reason || !reason[0])
        return;
    CloudClient &cloud = net.network.cloudClient();
    cloud.publishScopedEvent("stack", node_id, kind, reason, data_json);
}

void AppRuntime::handleStackRoute_(uint32_t node_id, const StackJsonProtocol::RouteMessage &route){
    if (!stackMasterActive_() && !stackSlaveActive_())
        return;
    String action = route.action;
    action.toLowerCase();
    if (!route.feature[0])
        return;
    DynamicJsonDocument payload_doc(512);
    JsonVariantConst params;
    if (!route.payload_json.isNull())
    {
        params = route.payload_json;
    }
    else if (route.payload.length())
    {
        if (deserializeJson(payload_doc, route.payload.c_str(), route.payload.length()))
            return;
        params = payload_doc.as<JsonVariantConst>();
    }
    if (strcmp(route.feature, "security") == 0)
    {
        if (action == "alarm")
        {
            const bool alarm = params["alarm"].is<bool>() ? params["alarm"].as<bool>()
                                                          : (params["alarm"].as<int>() != 0);
            if (!alarm)
                return;
            const uint8_t sensor_id = (uint8_t)(params["sensor_id"] | 0);
            const String name = params["name"] | "";
            const bool silent = params["silent"] | false;
            const String source = stackNodeLabel_(node_id);
            core.logs.warn(F("SECURITY"), F("remote detect: unit: %s id: %u name: %s silent: %u"),
                           source.c_str(),
                           (unsigned)sensor_id,
                           name.length() ? name.c_str() : "",
                           silent ? 1u : 0u);

            if (!silent && sensor_id > 0 && sensor_id <= 32)
                hw.plc.setAlarmDetail(PlcControl::AlarmModule::Security, (uint8_t)(sensor_id - 1), true);
            const int unit_idx = stackNodeIndex_(node_id);
            if (unit_idx >= 0 && unit_idx < 32 && !silent)
                hw.plc.setAlarmUnit(PlcControl::AlarmModule::Security, (uint8_t)unit_idx, true);

            control.controllers.security().setAlarmState(true);
            broadcastSecurityAlarm_(true);
            control.controllers.security().notifyRemoteDetect(source, sensor_id, name, silent);
            DynamicJsonDocument event_doc(224);
            event_doc["sensor_id"] = sensor_id;
            if (name.length())
                event_doc["name"] = name;
            event_doc["silent"] = silent;
            event_doc["alarm_on"] = true;
            if (source.length())
                event_doc["unit_name"] = source;
            String event_json;
            serializeJson(event_doc, event_json);
            publishCloudStackEvent_(node_id, "security.detect",
                                    silent ? "silent" : "detect", event_json);
            return;
        }
        if (action == "rfid")
        {
            const String uid = params["uid"] | "";
            if (!uid.length())
                return;
            const String source = params["name"] | stackNodeLabel_(node_id);
            core.logs.info(F("SECURITY"), F("remote RFID: unit: %s uid: %s"),
                           source.c_str(), uid.c_str());
            SecurityController &sec = control.controllers.security();
            const bool matched = sec.processRfidUidString(uid.c_str(), source.c_str());
            const String result = matched ? "disarm" : "reject";
            sendRfidResultToNode_(node_id, uid, matched, result, sec.armed());
            return;
        }
        if (action == "ibutton")
        {
            const String serial = params["serial"] | "";
            if (!serial.length())
                return;
            const String source = params["name"] | stackNodeLabel_(node_id);
            core.logs.info(F("SECURITY"), F("remote iButton: unit: %s serial: %s"),
                           source.c_str(), serial.c_str());
            SecurityController &sec = control.controllers.security();
            const bool matched = sec.processIButtonSerialString(serial.c_str(), source.c_str());
            const String result = matched ? "disarm" : "reject";
            sendIButtonResultToNode_(node_id, serial, matched, result, sec.armed());
            return;
        }
        if (action == "status_req")
        {
            sendSecurityStateToNode_(node_id);
            return;
        }
        if (action == "set")
        {
            auto &sec = control.controllers.security();
            if (params["armed"].is<bool>() || params["armed"].is<int>())
            {
                if (params["armed"].as<bool>())
                    sec.armForcedFrom("stack", stackNodeLabel_(node_id));
                else
                    sec.disarmFrom("stack", stackNodeLabel_(node_id), true);
            }
            if (params["alarm"].is<bool>() || params["alarm"].is<int>())
                sec.setAlarmState(params["alarm"].as<bool>());
            if (params["clear"].is<bool>() && params["clear"].as<bool>())
                sec.clearDetect();
            const char *beep = params["beep"] | "";
            (void)beep;
            return;
        }
        if (action == "rfid_result" || action == "ibutton_result")
            return;
        return;
    }
    if (strcmp(route.feature, "system") == 0)
    {
        if (action == "snapshot_req")
        {
            DynamicJsonDocument doc(320);
            appendSystemSnapshot_(doc.to<JsonObject>());
            net.network.stackSlaveSendResponse(route.source_node, "system", "snapshot",
                                               route.meta.request_id, &doc);
            return;
        }
        if (action == "snapshot")
        {
            StackUnitSnapshot::Snapshot state{};
            net.network.stackIndexStateSnapshot(node_id, state);
            state.node_id = node_id;
            state.updated_ms = millis();
            state.pending = false;
            state.has_plc = true;
            state.has_rtc = true;
            strlcpy(state.rtc_date, params["rtc_date"] | "", sizeof(state.rtc_date));
            strlcpy(state.rtc_time, params["rtc_time"] | "", sizeof(state.rtc_time));
            state.rtc_temp_ok = params["rtc_temp_ok"].as<bool>();
            state.rtc_temp = state.rtc_temp_ok
                                 ? (params["rtc_temp"].is<float>() ? params["rtc_temp"].as<float>()
                                                                   : (float)(params["rtc_temp"] | 0.0))
                                 : 0.0f;
            state.board_temp =
                params["board_temp"].is<float>() ? params["board_temp"].as<float>() : (float)(params["board_temp"] | 0.0);
            state.fan_on = params["fan_on"].is<bool>() ? params["fan_on"].as<bool>() : ((int)(params["fan_on"] | 0) != 0);
            if (state.rtc_date[0] == '\0' || state.rtc_time[0] == '\0')
                state.has_rtc = false;
            net.network.updateStackIndexState(node_id, state);
            return;
        }
    }
    if (strcmp(route.feature, "controllers") == 0)
    {
        if (action == "summary_req")
        {
            DynamicJsonDocument doc(768);
            appendControllerSnapshotSummary_(doc.to<JsonObject>());
            net.network.stackSlaveSendResponse(route.source_node, "controllers", "summary",
                                               route.meta.request_id, &doc);
            return;
        }
        if (action == "summary")
        {
            StackUnitSnapshot::Snapshot state{};
            net.network.stackIndexStateSnapshot(node_id, state);
            state.node_id = node_id;
            state.updated_ms = millis();
            JsonVariantConst sockets_summary = params["summary"]["sockets"];
            JsonVariantConst lights_summary = params["summary"]["lights"];
            JsonVariantConst meteo_summary = params["summary"]["meteo"];
            JsonVariantConst thermo_summary = params["summary"]["thermo"];
            JsonVariantConst tanks_summary = params["summary"]["tanks"];
            JsonVariantConst septic_summary = params["summary"]["septic"];
            JsonVariantConst watering_summary = params["summary"]["watering"];
            JsonVariantConst security_summary = params["summary"]["security"];
            JsonVariantConst ring_summary = params["summary"]["ring"];
            JsonVariantConst avr_summary = params["summary"]["avr"];
            JsonVariantConst leak_summary = params["summary"]["leak"];
            state.sockets_enabled = sockets_summary["enabled"] | 0;
            state.sockets_on = sockets_summary["on"] | 0;
            state.lights_enabled = lights_summary["enabled"] | 0;
            state.lights_on = lights_summary["on"] | 0;
            state.meteo_enabled = meteo_summary["enabled"] | 0;
            state.meteo_ok = meteo_summary["ok"] | 0;
            state.thermo_enabled = thermo_summary["enabled"] | 0;
            state.thermo_active = thermo_summary["active"] | 0;
            state.tanks_enabled = tanks_summary["enabled"] | 0;
            state.tanks_alert = tanks_summary["alert"] | 0;
            state.septic_enabled = septic_summary["enabled"] | 0;
            state.septic_alert = septic_summary["alert"] | 0;
            state.watering_enabled = watering_summary["enabled"] | 0;
            state.watering_active = watering_summary["active"] | 0;
            state.security_enabled = security_summary["enabled"] | false;
            state.security_sensors_enabled = security_summary["sensors_enabled"] | 0;
            state.security_armed = security_summary["armed"] | false;
            state.security_alarm = security_summary["alarm"] | false;
            state.ring_enabled = ring_summary["enabled"] | false;
            state.ring_on = ring_summary["on"] | false;
            state.avr_enabled = avr_summary["enabled"] | false;
            state.avr_fault = avr_summary["fault"] | false;
            state.avr_active_source = (uint8_t)(avr_summary["active_source_id"] | 0);
            state.leak_enabled = leak_summary["enabled"] | 0;
            state.leak_alert = leak_summary["alert"] | 0;
            net.network.updateStackIndexState(node_id, state);
            return;
        }
    }
    if (strcmp(route.feature, "web") == 0)
    {
        if (action == "index_state_req")
        {
            DynamicJsonDocument doc(3072);
            Ds3231Mz::DateTime dt{};
            char date_buf[16] = {};
            char time_buf[16] = {};
            float rtc_temp_c = 0.0f;
            const bool rtc_time_ok = hw.rtc.Time(dt);
            const bool rtc_temp_ok = hw.rtc.readTemp(rtc_temp_c);

            if (rtc_time_ok)
            {
                snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u", (unsigned)dt.year, (unsigned)dt.month,
                         (unsigned)dt.day);
                snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u", (unsigned)dt.hour, (unsigned)dt.minute,
                         (unsigned)dt.second);
            }

            doc["device_name"] = hw.plc.deviceName();
            doc["rtc_date"] = rtc_time_ok ? String(date_buf) : String("n/a");
            doc["rtc_time"] = rtc_time_ok ? String(time_buf) : String("n/a");
            doc["rtc_temp"] = rtc_temp_ok ? rtc_temp_c : 0.0f;
            doc["rtc_temp_ok"] = rtc_temp_ok;
            doc["board_temp"] = hw.plc.boardTemp();
            doc["fan_on"] = hw.plc.fanStatus();
            doc["fan_html"] = hw.plc.fanStatus()
                                  ? "<span class=\"status-dot status-on\" title=\"enabled\"></span>"
                                  : "<span class=\"status-dot status-off\" title=\"disabled\"></span>";
            appendControllerSnapshotSummary_(doc.to<JsonObject>());
            appendSocketSnapshotItems_(doc.to<JsonObject>());
            appendLightSnapshotItems_(doc.to<JsonObject>());
            const JsonArrayConst sockets = doc["controllers"]["sockets"].as<JsonArrayConst>();
            const size_t sockets_count = sockets.isNull() ? 0u : sockets.size();
            const JsonArrayConst lights = doc["controllers"]["lights"].as<JsonArrayConst>();
            const size_t lights_count = lights.isNull() ? 0u : lights.size();
            core.logs.info(F("STACK"), F("Slave snapshot tx prepare: dst 0x%08lX req: %lu sockets: %u lights: %u heap: %u"),
                           (unsigned long)route.source_node, (unsigned long)route.meta.request_id,
                           (unsigned)sockets_count, (unsigned)lights_count, (unsigned)ESP.getFreeHeap());
            const bool sent = net.network.stackSlaveSendResponse(route.source_node, "system", "snapshot",
                                                                 route.meta.request_id, &doc);
            core.logs.info(F("STACK"), F("Slave snapshot tx result: dst 0x%08lX req: %lu sent: %u"),
                           (unsigned long)route.source_node, (unsigned long)route.meta.request_id,
                           (unsigned)(sent ? 1 : 0));
            return;
        }
        if (action == "index_state")
        {
            StackUnitSnapshot::Snapshot state{};
            state.node_id = node_id;
            state.updated_ms = millis();
            state.pending = false;
            state.has_plc = true;
            state.has_rtc = true;
            strlcpy(state.rtc_date, params["rtc_date"] | "", sizeof(state.rtc_date));
            strlcpy(state.rtc_time, params["rtc_time"] | "", sizeof(state.rtc_time));
            state.rtc_temp_ok = params["rtc_temp_ok"].as<bool>();
            state.rtc_temp = state.rtc_temp_ok
                                 ? (params["rtc_temp"].is<float>() ? params["rtc_temp"].as<float>()
                                                                   : (float)(params["rtc_temp"] | 0.0))
                                 : 0.0f;
            state.board_temp =
                params["board_temp"].is<float>() ? params["board_temp"].as<float>() : (float)(params["board_temp"] | 0.0);
            state.fan_on = params["fan_on"].is<bool>() ? params["fan_on"].as<bool>() : ((int)(params["fan_on"] | 0) != 0);
            if (state.rtc_date[0] == '\0' || state.rtc_time[0] == '\0')
                state.has_rtc = false;
            DynamicJsonDocument doc(1024);
            doc["device_name"] = params["device_name"] | "";
            JsonObject system = doc["system"].to<JsonObject>();
            if (state.has_plc)
            {
                JsonObject plc = system["plc"].to<JsonObject>();
                plc["board_temp"] = state.board_temp;
                JsonObject fan = system["fan"].to<JsonObject>();
                fan["fan_on"] = state.fan_on;
            }
            if (state.has_rtc)
            {
                JsonObject rtc = system["rtc"].to<JsonObject>();
                rtc["date"] = state.rtc_date;
                rtc["time"] = state.rtc_time;
                rtc["temp_c"] = state.rtc_temp;
            }
            JsonObject summary = doc["summary"].to<JsonObject>();
            JsonVariantConst sockets_summary = params["summary"]["sockets"];
            JsonVariantConst lights_summary = params["summary"]["lights"];
            if (!sockets_summary.isNull())
            {
                JsonObject sockets = summary["sockets"].to<JsonObject>();
                sockets["enabled"] = sockets_summary["enabled"] | 0;
                sockets["on"] = sockets_summary["on"] | 0;
            }
            if (!lights_summary.isNull())
            {
                JsonObject lights = summary["lights"].to<JsonObject>();
                lights["enabled"] = lights_summary["enabled"] | 0;
                lights["on"] = lights_summary["on"] | 0;
            }
            JsonVariantConst meteo_summary = params["summary"]["meteo"];
            JsonVariantConst thermo_summary = params["summary"]["thermo"];
            JsonVariantConst tanks_summary = params["summary"]["tanks"];
            JsonVariantConst septic_summary = params["summary"]["septic"];
            JsonVariantConst watering_summary = params["summary"]["watering"];
            JsonVariantConst security_summary = params["summary"]["security"];
            JsonVariantConst ring_summary = params["summary"]["ring"];
            JsonVariantConst avr_summary = params["summary"]["avr"];
            JsonVariantConst leak_summary = params["summary"]["leak"];
            if (!meteo_summary.isNull())
            {
                JsonObject meteo = summary["meteo"].to<JsonObject>();
                meteo["enabled"] = meteo_summary["enabled"] | 0;
                meteo["ok"] = meteo_summary["ok"] | 0;
            }
            if (!thermo_summary.isNull())
            {
                JsonObject thermo = summary["thermo"].to<JsonObject>();
                thermo["enabled"] = thermo_summary["enabled"] | 0;
                thermo["active"] = thermo_summary["active"] | 0;
            }
            if (!tanks_summary.isNull())
            {
                JsonObject tanks = summary["tanks"].to<JsonObject>();
                tanks["enabled"] = tanks_summary["enabled"] | 0;
                tanks["alert"] = tanks_summary["alert"] | 0;
            }
            if (!septic_summary.isNull())
            {
                JsonObject septic = summary["septic"].to<JsonObject>();
                septic["enabled"] = septic_summary["enabled"] | 0;
                septic["alert"] = septic_summary["alert"] | 0;
            }
            if (!watering_summary.isNull())
            {
                JsonObject watering = summary["watering"].to<JsonObject>();
                watering["enabled"] = watering_summary["enabled"] | 0;
                watering["active"] = watering_summary["active"] | 0;
            }
            if (!security_summary.isNull())
            {
                JsonObject security = summary["security"].to<JsonObject>();
                security["enabled"] = security_summary["enabled"] | false;
                security["sensors_enabled"] = security_summary["sensors_enabled"] | 0;
                security["armed"] = security_summary["armed"] | false;
                security["alarm"] = security_summary["alarm"] | false;
            }
            if (!ring_summary.isNull())
            {
                JsonObject ring = summary["ring"].to<JsonObject>();
                ring["enabled"] = ring_summary["enabled"] | false;
                ring["on"] = ring_summary["on"] | false;
            }
            if (!avr_summary.isNull())
            {
                JsonObject avr = summary["avr"].to<JsonObject>();
                avr["enabled"] = avr_summary["enabled"] | false;
                avr["fault"] = avr_summary["fault"] | false;
                avr["active_source"] = avr_summary["active_source"] | "off";
            }
            if (!leak_summary.isNull())
            {
                JsonObject leak = summary["leak"].to<JsonObject>();
                leak["enabled"] = leak_summary["enabled"] | 0;
                leak["alert"] = leak_summary["alert"] | 0;
            }
            state.sockets_enabled = sockets_summary["enabled"] | 0;
            state.sockets_on = sockets_summary["on"] | 0;
            state.lights_enabled = lights_summary["enabled"] | 0;
            state.lights_on = lights_summary["on"] | 0;
            state.meteo_enabled = meteo_summary["enabled"] | 0;
            state.meteo_ok = meteo_summary["ok"] | 0;
            state.thermo_enabled = thermo_summary["enabled"] | 0;
            state.thermo_active = thermo_summary["active"] | 0;
            state.tanks_enabled = tanks_summary["enabled"] | 0;
            state.tanks_alert = tanks_summary["alert"] | 0;
            state.septic_enabled = septic_summary["enabled"] | 0;
            state.septic_alert = septic_summary["alert"] | 0;
            state.watering_enabled = watering_summary["enabled"] | 0;
            state.watering_active = watering_summary["active"] | 0;
            state.security_enabled = security_summary["enabled"] | false;
            state.security_sensors_enabled = security_summary["sensors_enabled"] | 0;
            state.security_armed = security_summary["armed"] | false;
            state.security_alarm = security_summary["alarm"] | false;
            state.ring_enabled = ring_summary["enabled"] | false;
            state.ring_on = ring_summary["on"] | false;
            state.avr_enabled = avr_summary["enabled"] | false;
            state.avr_fault = avr_summary["fault"] | false;
            state.avr_active_source = (uint8_t)(avr_summary["active_source_id"] | 0);
            state.leak_enabled = leak_summary["enabled"] | 0;
            state.leak_alert = leak_summary["alert"] | 0;
            JsonArrayConst sockets_items = params["controllers"]["sockets"].as<JsonArrayConst>();
            state.socket_count = 0;
            memset(state.sockets, 0, sizeof(state.sockets));
            if (!sockets_items.isNull())
            {
                for (JsonObjectConst item : sockets_items)
                {
                    if (state.socket_count >= StackUnitSnapshot::kSocketCount)
                        break;
                    auto &dst = state.sockets[state.socket_count];
                    dst.id = (uint8_t)(item["id"] | 0);
                    dst.enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                             : (item["enabled"].as<int>() != 0);
                    dst.state = item["state"].is<bool>() ? item["state"].as<bool>()
                                                         : (item["state"].as<int>() != 0);
                    strlcpy(dst.name, item["name"] | "", sizeof(dst.name));
                    dst.button_port = (uint8_t)(item["button"] | SocketController::kInvalidPort);
                    dst.relay_port = (uint8_t)(item["relay"] | SocketController::kInvalidPort);
                    dst.group_id = (uint8_t)(item["group_id"] | 0);
                    ++state.socket_count;
                }
            }
            JsonArrayConst lights_items = params["controllers"]["lights"].as<JsonArrayConst>();
            state.light_count = 0;
            memset(state.lights, 0, sizeof(state.lights));
            if (!lights_items.isNull())
            {
                for (JsonObjectConst item : lights_items)
                {
                    if (state.light_count >= StackUnitSnapshot::kSocketCount)
                        break;
                    auto &dst = state.lights[state.light_count];
                    dst.id = (uint8_t)(item["id"] | 0);
                    dst.enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                             : (item["enabled"].as<int>() != 0);
                    dst.state = item["state"].is<bool>() ? item["state"].as<bool>()
                                                         : (item["state"].as<int>() != 0);
                    dst.button_port = (uint8_t)(item["button"] | SocketController::kInvalidPort);
                    dst.relay_port = (uint8_t)(item["relay"] | SocketController::kInvalidPort);
                    dst.group_id = (uint8_t)(item["group_id"] | 0);
                    strlcpy(dst.name, item["name"] | "", sizeof(dst.name));
                    ++state.light_count;
                }
            }
            if (state.socket_count > 0)
            {
                JsonObject controllers = doc["controllers"].to<JsonObject>();
                JsonArray sockets = controllers["sockets"].to<JsonArray>();
                for (uint8_t i = 0; i < state.socket_count && i < StackUnitSnapshot::kSocketCount; ++i)
                {
                    const auto &src = state.sockets[i];
                    if (src.id == 0)
                        continue;
                    JsonObject item = sockets.add<JsonObject>();
                    item["id"] = src.id;
                    item["enabled"] = src.enabled;
                    item["state"] = src.state;
                    if (src.name[0] != '\0')
                        item["name"] = src.name;
                }
            }
            if (state.light_count > 0)
            {
                JsonObject controllers = doc["controllers"].to<JsonObject>();
                JsonArray lights = controllers["lights"].to<JsonArray>();
                for (uint8_t i = 0; i < state.light_count && i < StackUnitSnapshot::kSocketCount; ++i)
                {
                    const auto &src = state.lights[i];
                    if (src.id == 0)
                        continue;
                    JsonObject item = lights.add<JsonObject>();
                    item["id"] = src.id;
                    item["enabled"] = src.enabled;
                    item["state"] = src.state;
                    item["button"] = src.button_port;
                    item["relay"] = src.relay_port;
                    item["group_id"] = src.group_id;
                    if (src.name[0] != '\0')
                        item["name"] = src.name;
                }
            }
            net.network.updateStackIndexState(node_id, state);
            String json;
            serializeJson(doc, json);
            publishCloudStackEvent_(node_id, "stack.snapshot", "update", json);
            return;
        }
        return;
    }
    if (strcmp(route.feature, "ring") == 0)
    {
        if (action == "button")
        {
            const bool pressed = params["pressed"].is<bool>() ? params["pressed"].as<bool>()
                                                              : (params["pressed"].as<int>() != 0);
            control.controllers.ring().setHoldRelayWithSource(pressed, RingController::Source::Stack);
            return;
        }
        if (action == "set")
        {
            const bool state = params["state"].is<bool>() ? params["state"].as<bool>()
                                                          : (params["state"].as<int>() != 0);
            control.controllers.ring().setHoldRelayWithSource(state, RingController::Source::Stack);
            return;
        }
        return;
    }
    if (strcmp(route.feature, "sockets") == 0)
    {
        if (action == "snapshot_req")
        {
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            uint16_t limit = (uint16_t)(params["limit"] | 8);
            if (limit == 0)
                limit = 8;
            if (limit > 8)
                limit = 8;
            const uint16_t range_end = (limit == 0) ? offset : (uint16_t)(offset + limit - 1u);
            SocketController &sockets = control.controllers.sockets();
            size_t sockets_count = 0;
            {
                auto guard = sockets.lockGuard();
                uint16_t current_index = 0;
                for (size_t i = 0; i < SocketController::kSocketCount; ++i)
                {
                    const auto *cfg = sockets.configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    if (current_index < offset)
                    {
                        ++current_index;
                        continue;
                    }
                    if (sockets_count >= limit)
                        break;
                    ++sockets_count;
                    ++current_index;
                }
            }
            _pending_stack_sockets_response = true;
            _pending_stack_sockets_target_node = route.source_node;
            _pending_stack_sockets_reply_to = route.meta.request_id;
            _pending_stack_sockets_response_offset = offset;
            _pending_stack_sockets_response_limit = limit;
            return;
        }
        if (action == "snapshot")
        {
            StackUnitSnapshot::Snapshot state{};
            net.network.stackIndexStateSnapshot(node_id, state);
            state.node_id = node_id;
            state.updated_ms = millis();
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            const uint16_t total = (uint16_t)(params["total"] | 0);
            const uint16_t summary_total = (uint16_t)(params["summary"]["sockets"]["enabled"] | 0);
            const JsonArrayConst sockets_items = params["controllers"]["sockets"].as<JsonArrayConst>();
            const uint16_t received_count = sockets_items.isNull() ? 0u : (uint16_t)sockets_items.size();
            net.network.completeStackSocketsPageRequest(node_id, offset);
            if (offset == 0)
            {
                state.socket_count = 0;
                memset(state.sockets, 0, sizeof(state.sockets));
            }
            if (!sockets_items.isNull())
            {
                uint16_t idx = offset;
                for (JsonObjectConst item : sockets_items)
                {
                    if (idx >= StackUnitSnapshot::kSocketCount)
                        break;
                    auto &dst = state.sockets[idx];
                    dst.id = (uint8_t)(item["id"] | 0);
                    dst.enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                             : (item["enabled"].as<int>() != 0);
                    dst.state = item["state"].is<bool>() ? item["state"].as<bool>()
                                                         : (item["state"].as<int>() != 0);
                    dst.button_port = (uint8_t)(item["button"] | SocketController::kInvalidPort);
                    dst.relay_port = (uint8_t)(item["relay"] | SocketController::kInvalidPort);
                    dst.group_id = (uint8_t)(item["group_id"] | 0);
                    strlcpy(dst.name, item["name"] | "", sizeof(dst.name));
                    ++idx;
                }
                if (idx > state.socket_count)
                    state.socket_count = (uint8_t)idx;
            }
            net.network.updateStackIndexState(node_id, state);
            const uint16_t expected_total = (total > 0) ? total : summary_total;
            const uint16_t target_total =
                (expected_total > StackUnitSnapshot::kSocketCount) ? (uint16_t)StackUnitSnapshot::kSocketCount
                                                                   : expected_total;
            if (target_total > 0 && state.socket_count < target_total)
            {
                const uint16_t next_offset = state.socket_count;
                if (net.network.prepareStackSocketsPageRequest(node_id, millis(), next_offset, 4000u))
                {
                    _pending_stack_sockets_page = true;
                    _pending_stack_sockets_node_id = node_id;
                    _pending_stack_sockets_offset = next_offset;
                    _pending_stack_sockets_limit = 8;
                    const bool bootstrap_log = shouldLogStackBootstrapSync_(node_id);
                    _pending_stack_sockets_log =
                        bootstrap_log &&
                        !(_stack_bootstrap_logged_sockets_node_id == node_id &&
                          _stack_bootstrap_logged_sockets_offset == next_offset);
                    if (_pending_stack_sockets_log)
                    {
                        _stack_bootstrap_logged_sockets_node_id = node_id;
                        _stack_bootstrap_logged_sockets_offset = next_offset;
                    }
                }
            }
            return;
        }
        handleSocketFrame_(node_id, action, params);
        return;
    }
    if (strcmp(route.feature, "lights") == 0)
    {
        if (action == "snapshot_req")
        {
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            uint16_t limit = (uint16_t)(params["limit"] | 8);
            if (limit == 0)
                limit = 8;
            if (limit > 8)
                limit = 8;
            _pending_stack_lights_response = true;
            _pending_stack_lights_target_node = route.source_node;
            _pending_stack_lights_reply_to = route.meta.request_id;
            _pending_stack_lights_response_offset = offset;
            _pending_stack_lights_response_limit = limit;
            return;
        }
        if (action == "snapshot")
        {
            StackUnitSnapshot::Snapshot state{};
            net.network.stackIndexStateSnapshot(node_id, state);
            state.node_id = node_id;
            state.updated_ms = millis();
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            const uint16_t total = (uint16_t)(params["total"] | 0);
            const uint16_t summary_total = (uint16_t)(params["summary"]["lights"]["enabled"] | 0);
            const JsonArrayConst lights_items = params["controllers"]["lights"].as<JsonArrayConst>();
            net.network.completeStackLightsPageRequest(node_id, offset);
            if (offset == 0)
            {
                state.light_count = 0;
                memset(state.lights, 0, sizeof(state.lights));
            }
            if (!lights_items.isNull())
            {
                uint16_t idx = offset;
                for (JsonObjectConst item : lights_items)
                {
                    if (idx >= StackUnitSnapshot::kSocketCount)
                        break;
                    auto &dst = state.lights[idx];
                    dst.id = (uint8_t)(item["id"] | 0);
                    dst.enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                             : (item["enabled"].as<int>() != 0);
                    dst.state = item["state"].is<bool>() ? item["state"].as<bool>()
                                                         : (item["state"].as<int>() != 0);
                    dst.button_port = (uint8_t)(item["button"] | SocketController::kInvalidPort);
                    dst.relay_port = (uint8_t)(item["relay"] | SocketController::kInvalidPort);
                    dst.group_id = (uint8_t)(item["group_id"] | 0);
                    strlcpy(dst.name, item["name"] | "", sizeof(dst.name));
                    ++idx;
                }
                if (idx > state.light_count)
                    state.light_count = (uint8_t)idx;
            }
            net.network.updateStackIndexState(node_id, state);
            const uint16_t expected_total = (total > 0) ? total : summary_total;
            const uint16_t target_total =
                (expected_total > StackUnitSnapshot::kSocketCount) ? (uint16_t)StackUnitSnapshot::kSocketCount
                                                                   : expected_total;
            if (target_total > 0 && state.light_count < target_total)
            {
                const uint16_t next_offset = state.light_count;
                if (net.network.prepareStackLightsPageRequest(node_id, millis(), next_offset, 4000u))
                {
                    _pending_stack_lights_page = true;
                    _pending_stack_lights_node_id = node_id;
                    _pending_stack_lights_offset = next_offset;
                    _pending_stack_lights_limit = 8;
                    const bool bootstrap_log = shouldLogStackBootstrapSync_(node_id);
                    _pending_stack_lights_log =
                        bootstrap_log &&
                        !(_stack_bootstrap_logged_lights_node_id == node_id &&
                          _stack_bootstrap_logged_lights_offset == next_offset);
                    if (_pending_stack_lights_log)
                    {
                        _stack_bootstrap_logged_lights_node_id = node_id;
                        _stack_bootstrap_logged_lights_offset = next_offset;
                    }
                }
            }
            return;
        }
        return;
    }
    if (strcmp(route.feature, "meteo") == 0)
    {
        if (action == "snapshot_req")
        {
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            uint16_t limit = (uint16_t)(params["limit"] | 8);
            if (limit == 0)
                limit = 8;
            if (limit > 8)
                limit = 8;
            DynamicJsonDocument doc(2048);
            appendMeteoSnapshotPage_(doc.to<JsonObject>(), offset, limit);
            net.network.stackSlaveSendResponse(route.source_node, "meteo", "snapshot",
                                               route.meta.request_id, &doc);
            return;
        }
        if (action == "snapshot")
        {
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            const uint16_t total = (uint16_t)(params["total"] | 0);
            const uint16_t summary_total = (uint16_t)(params["summary"]["meteo"]["enabled"] | 0);
            const uint16_t ok_total = (uint16_t)(params["summary"]["meteo"]["ok"] | 0);
            StackUnitSnapshot::MeteoItem items[8]{};
            uint8_t item_count = 0;
            const JsonArrayConst meteo_items = params["controllers"]["meteo"].as<JsonArrayConst>();
            if (!meteo_items.isNull())
            {
                for (JsonObjectConst item : meteo_items)
                {
                    if (item_count >= 8)
                        break;
                    auto &dst = items[item_count];
                    dst.id = (uint8_t)(item["id"] | 0);
                    dst.enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                             : (item["enabled"].as<int>() != 0);
                    dst.group_id = (uint8_t)(item["group_id"] | 0);
                    dst.type = (uint8_t)(item["type_id"] | 0);
                    dst.dht_pin = (uint8_t)(item["pin"] | MeteoController::kInvalidPin);
                    dst.ds18_addr_set = item["addr_set"].is<bool>() ? item["addr_set"].as<bool>()
                                                                    : (item["addr_set"].as<int>() != 0);
                    const char *addr_hex = item["addr"] | "";
                    if (dst.ds18_addr_set && addr_hex && addr_hex[0] != '\0')
                        MeteoController::parseHexAddr(addr_hex, dst.ds18_addr);
                    dst.source_node_id = (uint32_t)(item["src_node"] | 0u);
                    dst.source_sensor_id = (uint8_t)(item["src_sensor"] | 0);
                    dst.has_read = item["has_read"].is<bool>() ? item["has_read"].as<bool>()
                                                               : (item["has_read"].as<int>() != 0);
                    dst.ok = item["ok"].is<bool>() ? item["ok"].as<bool>()
                                                   : (item["ok"].as<int>() != 0);
                    dst.has_temp = item["has_temp"].is<bool>() ? item["has_temp"].as<bool>()
                                                               : (item["has_temp"].as<int>() != 0);
                    dst.has_humidity = item["has_hum"].is<bool>() ? item["has_hum"].as<bool>()
                                                                  : (item["has_hum"].as<int>() != 0);
                    dst.temp_c = item["temp_c"].is<float>() ? item["temp_c"].as<float>()
                                                            : (float)(item["temp_c"] | 0.0);
                    dst.humidity = item["hum"].is<float>() ? item["hum"].as<float>()
                                                           : (float)(item["hum"] | 0.0);
                    dst.age_s = (uint16_t)(item["age_s"] | 0);
                    strlcpy(dst.name, item["name"] | "", sizeof(dst.name));
                    ++item_count;
                }
            }
            net.network.completeStackMeteoPageRequest(node_id, offset);
            net.network.updateStackIndexMeteoPage(node_id, offset, total > 0 ? total : summary_total, ok_total,
                                                  items, item_count, millis());
            StackUnitSnapshot::State state{};
            if (net.network.stackIndexState(node_id, state))
            {
                const uint16_t expected_total = (total > 0) ? total : summary_total;
                const uint16_t target_total =
                    (expected_total > StackUnitSnapshot::kMeteoCount) ? (uint16_t)StackUnitSnapshot::kMeteoCount
                                                                      : expected_total;
                if (target_total > 0 && state.meteo_count < target_total)
                {
                    const uint16_t next_offset = state.meteo_count;
                    if (net.network.prepareStackMeteoPageRequest(node_id, millis(), next_offset, 4000u))
                    {
                        _pending_stack_meteo_page = true;
                        _pending_stack_meteo_node_id = node_id;
                        _pending_stack_meteo_offset = next_offset;
                        _pending_stack_meteo_limit = 8;
                        const bool bootstrap_log = shouldLogStackBootstrapSync_(node_id);
                        _pending_stack_meteo_log =
                            bootstrap_log &&
                            !(_stack_bootstrap_logged_meteo_node_id == node_id &&
                              _stack_bootstrap_logged_meteo_offset == next_offset);
                        if (_pending_stack_meteo_log)
                        {
                            _stack_bootstrap_logged_meteo_node_id = node_id;
                            _stack_bootstrap_logged_meteo_offset = next_offset;
                        }
                    }
                }
            }
            return;
        }
        if (action == "set")
        {
            MeteoController &meteo = control.controllers.meteo();
            auto guard = meteo.lockGuard();
            const JsonArrayConst items = params["items"].as<JsonArrayConst>();
            for (JsonObjectConst item : items)
            {
                const uint8_t id = (uint8_t)(item["id"] | 0);
                if (id == 0)
                    continue;
                if (item.containsKey("enabled"))
                {
                    const bool enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                                    : (item["enabled"].as<int>() != 0);
                    meteo.setEnabled(id, enabled);
                }
                if (item.containsKey("name"))
                    meteo.setName(id, String(item["name"] | ""));
                if (item.containsKey("group_id"))
                    meteo.setGroupId(id, (uint8_t)(item["group_id"] | 0));
                if (item.containsKey("type_id"))
                    meteo.setType(id, (MeteoController::SensorType)(uint8_t)(item["type_id"] | 0));
                if (item.containsKey("pin"))
                    meteo.setDht22Pin(id, (uint8_t)(item["pin"] | MeteoController::kInvalidPin));
                if (item.containsKey("addr_set"))
                {
                    uint8_t addr[MeteoController::kAddrLen] = {};
                    const bool addr_set = item["addr_set"].is<bool>() ? item["addr_set"].as<bool>()
                                                                       : (item["addr_set"].as<int>() != 0);
                    const char *addr_hex = item["addr"] | "";
                    if (addr_set && addr_hex && addr_hex[0] != '\0' &&
                        MeteoController::parseHexAddr(addr_hex, addr))
                        meteo.setDs18b20Addr(id, addr, true);
                    else if (!addr_set)
                        meteo.setDs18b20Addr(id, addr, false);
                }
                if (item.containsKey("src_node") || item.containsKey("src_sensor"))
                    meteo.setRemoteSource(id, (uint32_t)(item["src_node"] | 0u), (uint8_t)(item["src_sensor"] | 0));
            }
            return;
        }
        return;
    }
    if (strcmp(route.feature, "thermo") == 0)
    {
        if (action == "snapshot_req")
        {
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            uint16_t limit = (uint16_t)(params["limit"] | 8);
            if (limit == 0)
                limit = 8;
            if (limit > 8)
                limit = 8;
            DynamicJsonDocument doc(2048);
            appendThermoSnapshotPage_(doc.to<JsonObject>(), offset, limit);
            net.network.stackSlaveSendResponse(route.source_node, "thermo", "snapshot",
                                               route.meta.request_id, &doc);
            return;
        }
        if (action == "snapshot")
        {
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            const uint16_t total = (uint16_t)(params["total"] | 0);
            const uint16_t summary_total = (uint16_t)(params["summary"]["thermo"]["enabled"] | 0);
            const uint16_t active_total = (uint16_t)(params["summary"]["thermo"]["active"] | 0);
            StackUnitSnapshot::ThermoItem items[8]{};
            uint8_t item_count = 0;
            const JsonArrayConst thermo_items = params["controllers"]["thermo"].as<JsonArrayConst>();
            if (!thermo_items.isNull())
            {
                for (JsonObjectConst item : thermo_items)
                {
                    if (item_count >= 8)
                        break;
                    auto &dst = items[item_count];
                    dst.id = (uint8_t)(item["id"] | 0);
                    dst.enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                             : (item["enabled"].as<int>() != 0);
                    dst.group_id = (uint8_t)(item["group_id"] | 0);
                    dst.sensor_id = (uint8_t)(item["sensor_id"] | 0);
                    dst.sensor_node_id = (uint32_t)(item["sensor_node_id"] | 0u);
                    dst.heat_port = (uint8_t)(item["heat_port"] | ThermoController::kInvalidPort);
                    dst.cool_port = (uint8_t)(item["cool_port"] | ThermoController::kInvalidPort);
                    dst.button_port = (uint8_t)(item["button_port"] | ThermoController::kInvalidPort);
                    dst.mode = (uint8_t)(item["mode_id"] | 0);
                    dst.target_c = item["target_c"].is<float>() ? item["target_c"].as<float>()
                                                                : (float)(item["target_c"] | 0.0);
                    dst.hysteresis = item["hyst"].is<float>() ? item["hyst"].as<float>()
                                                              : (float)(item["hyst"] | 0.0);
                    dst.power_on = item["power_on"].is<bool>() ? item["power_on"].as<bool>()
                                                               : (item["power_on"].as<int>() != 0);
                    dst.heat_on = item["heat_on"].is<bool>() ? item["heat_on"].as<bool>()
                                                             : (item["heat_on"].as<int>() != 0);
                    dst.cool_on = item["cool_on"].is<bool>() ? item["cool_on"].as<bool>()
                                                             : (item["cool_on"].as<int>() != 0);
                    strlcpy(dst.name, item["name"] | "", sizeof(dst.name));
                    ++item_count;
                }
            }
            net.network.completeStackThermoPageRequest(node_id, offset);
            net.network.updateStackIndexThermoPage(node_id, offset, total > 0 ? total : summary_total, active_total,
                                                   items, item_count, millis());
            StackUnitSnapshot::State state{};
            if (net.network.stackIndexState(node_id, state))
            {
                const uint16_t expected_total = (total > 0) ? total : summary_total;
                const uint16_t target_total =
                    (expected_total > StackUnitSnapshot::kThermoCount) ? (uint16_t)StackUnitSnapshot::kThermoCount
                                                                       : expected_total;
                if (target_total > 0 && state.thermo_count < target_total)
                {
                    const uint16_t next_offset = state.thermo_count;
                    if (net.network.prepareStackThermoPageRequest(node_id, millis(), next_offset, 4000u))
                    {
                        _pending_stack_thermo_page = true;
                        _pending_stack_thermo_node_id = node_id;
                        _pending_stack_thermo_offset = next_offset;
                        _pending_stack_thermo_limit = 8;
                        const bool bootstrap_log = shouldLogStackBootstrapSync_(node_id);
                        _pending_stack_thermo_log =
                            bootstrap_log &&
                            !(_stack_bootstrap_logged_thermo_node_id == node_id &&
                              _stack_bootstrap_logged_thermo_offset == next_offset);
                        if (_pending_stack_thermo_log)
                        {
                            _stack_bootstrap_logged_thermo_node_id = node_id;
                            _stack_bootstrap_logged_thermo_offset = next_offset;
                        }
                    }
                }
            }
            return;
        }
        if (action == "set")
        {
            ThermoController &thermo = control.controllers.thermo();
            auto guard = thermo.lockGuard();
            const JsonArrayConst items = params["items"].as<JsonArrayConst>();
            for (JsonObjectConst item : items)
            {
                const uint8_t id = (uint8_t)(item["id"] | 0);
                if (id == 0)
                    continue;
                if (item.containsKey("enabled"))
                {
                    const bool enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                                    : (item["enabled"].as<int>() != 0);
                    thermo.setEnabled(id, enabled);
                }
                if (item.containsKey("name"))
                    thermo.setName(id, String(item["name"] | ""));
                if (item.containsKey("group_id"))
                    thermo.setGroupId(id, (uint8_t)(item["group_id"] | 0));
                if (item.containsKey("sensor_node_id") || item.containsKey("sensor_id"))
                {
                    const uint32_t sensor_node_id = (uint32_t)(item["sensor_node_id"] | 0u);
                    const uint8_t sensor_id = (uint8_t)(item["sensor_id"] | 0);
                    if (sensor_node_id != 0)
                        thermo.setSensorSource(id, sensor_node_id, sensor_id);
                    else
                        thermo.setSensor(id, sensor_id);
                }
                if (item.containsKey("mode_id"))
                    thermo.setMode(id, (ThermoController::Mode)(uint8_t)(item["mode_id"] | 0));
                if (item.containsKey("target_c"))
                    thermo.setTarget(id, item["target_c"].is<float>() ? item["target_c"].as<float>()
                                                                      : (float)(item["target_c"] | 0.0));
                if (item.containsKey("hyst"))
                    thermo.setHysteresis(id, item["hyst"].is<float>() ? item["hyst"].as<float>()
                                                                      : (float)(item["hyst"] | 0.0));
                if (item.containsKey("heat_port"))
                    thermo.setHeatPort(id, (uint8_t)(item["heat_port"] | ThermoController::kInvalidPort));
                if (item.containsKey("cool_port"))
                    thermo.setCoolPort(id, (uint8_t)(item["cool_port"] | ThermoController::kInvalidPort));
                if (item.containsKey("button_port"))
                    thermo.setButtonPort(id, (uint8_t)(item["button_port"] | ThermoController::kInvalidPort));
                const String source = item["source"] | (params["source"] | "stack");
                if (item["toggle"].is<bool>() && item["toggle"].as<bool>())
                    thermo.togglePower(id, source.c_str());
                else if (item.containsKey("power_on"))
                {
                    const bool power_on = item["power_on"].is<bool>() ? item["power_on"].as<bool>()
                                                                      : (item["power_on"].as<int>() != 0);
                    thermo.setPower(id, power_on, source.c_str());
                }
            }
            return;
        }
        return;
    }
    if (strcmp(route.feature, "septic") == 0)
    {
        handleSepticFrame_(node_id, action, params);
        return;
    }
    if (strcmp(route.feature, "tanks") == 0)
    {
        if (action == "snapshot_req")
        {
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            uint16_t limit = (uint16_t)(params["limit"] | 8);
            if (limit == 0)
                limit = 8;
            if (limit > 8)
                limit = 8;
            DynamicJsonDocument doc(2048);
            appendTankSnapshotPage_(doc.to<JsonObject>(), offset, limit);
            net.network.stackSlaveSendResponse(route.source_node, "tanks", "snapshot",
                                               route.meta.request_id, &doc);
            return;
        }
        if (action == "snapshot")
        {
            const uint16_t offset = (uint16_t)(params["offset"] | 0);
            const uint16_t total = (uint16_t)(params["total"] | 0);
            const uint16_t summary_total = (uint16_t)(params["summary"]["tanks"]["enabled"] | 0);
            const uint16_t alert_total = (uint16_t)(params["summary"]["tanks"]["alert"] | 0);
            StackUnitSnapshot::TankItem items[8]{};
            uint8_t item_count = 0;
            const JsonArrayConst tanks_items = params["controllers"]["tanks"].as<JsonArrayConst>();
            if (!tanks_items.isNull())
            {
                for (JsonObjectConst item : tanks_items)
                {
                    if (item_count >= 8)
                        break;
                    auto &dst = items[item_count];
                    dst.id = (uint8_t)(item["id"] | 0);
                    dst.enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                             : (item["enabled"].as<int>() != 0);
                    dst.group_id = (uint8_t)(item["group_id"] | 0);
                    dst.power_on = item["power_on"].is<bool>() ? item["power_on"].as<bool>()
                                                               : (item["power_on"].as<int>() != 0);
                    dst.level_low_port = (uint8_t)(item["low"] | TankController::kInvalidPort);
                    dst.level_mid_port = (uint8_t)(item["mid"] | TankController::kInvalidPort);
                    dst.level_full_port = (uint8_t)(item["full"] | TankController::kInvalidPort);
                    dst.relay_valve_port = (uint8_t)(item["valve"] | TankController::kInvalidPort);
                    dst.relay_pump_port = (uint8_t)(item["pump"] | TankController::kInvalidPort);
                    dst.relay_alarm_port = (uint8_t)(item["alarm"] | TankController::kInvalidPort);
                    dst.level_low = item["level_low"].is<bool>() ? item["level_low"].as<bool>()
                                                                  : (item["level_low"].as<int>() != 0);
                    dst.level_mid = item["level_mid"].is<bool>() ? item["level_mid"].as<bool>()
                                                                  : (item["level_mid"].as<int>() != 0);
                    dst.level_full = item["level_full"].is<bool>() ? item["level_full"].as<bool>()
                                                                    : (item["level_full"].as<int>() != 0);
                    dst.levels_ok = item["levels_ok"].is<bool>() ? item["levels_ok"].as<bool>()
                                                                  : (item["levels_ok"].as<int>() != 0);
                    dst.valve_on = item["valve_on"].is<bool>() ? item["valve_on"].as<bool>()
                                                                : (item["valve_on"].as<int>() != 0);
                    dst.pump_on = item["pump_on"].is<bool>() ? item["pump_on"].as<bool>()
                                                              : (item["pump_on"].as<int>() != 0);
                    dst.alarm_on = item["alarm_on"].is<bool>() ? item["alarm_on"].as<bool>()
                                                                : (item["alarm_on"].as<int>() != 0);
                    strlcpy(dst.name, item["name"] | "", sizeof(dst.name));
                    ++item_count;
                }
            }
            net.network.completeStackTanksPageRequest(node_id, offset);
            net.network.updateStackIndexTanksPage(node_id, offset, total > 0 ? total : summary_total, alert_total,
                                                  items, item_count, millis());
            StackUnitSnapshot::State state{};
            if (net.network.stackIndexState(node_id, state))
            {
                const uint16_t expected_total = (total > 0) ? total : summary_total;
                const uint16_t target_total =
                    (expected_total > StackUnitSnapshot::kTankCount) ? (uint16_t)StackUnitSnapshot::kTankCount
                                                                     : expected_total;
                if (target_total > 0 && state.tank_count < target_total)
                {
                    const uint16_t next_offset = state.tank_count;
                    if (net.network.prepareStackTanksPageRequest(node_id, millis(), next_offset, 4000u))
                    {
                        _pending_stack_tanks_page = true;
                        _pending_stack_tanks_node_id = node_id;
                        _pending_stack_tanks_offset = next_offset;
                        _pending_stack_tanks_limit = 8;
                        const bool bootstrap_log = shouldLogStackBootstrapSync_(node_id);
                        _pending_stack_tanks_log =
                            bootstrap_log &&
                            !(_stack_bootstrap_logged_tanks_node_id == node_id &&
                              _stack_bootstrap_logged_tanks_offset == next_offset);
                        if (_pending_stack_tanks_log)
                        {
                            _stack_bootstrap_logged_tanks_node_id = node_id;
                            _stack_bootstrap_logged_tanks_offset = next_offset;
                        }
                    }
                }
            }
            return;
        }
        if (action == "set")
        {
            TankController &tanks = control.controllers.tanks();
            auto guard = tanks.lockGuard();
            const JsonArrayConst items = params["items"].as<JsonArrayConst>();
            for (JsonObjectConst item : items)
            {
                const uint8_t id = (uint8_t)(item["id"] | 0);
                if (id == 0)
                    continue;
                if (item.containsKey("enabled"))
                {
                    const bool enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                                    : (item["enabled"].as<int>() != 0);
                    tanks.setEnabled(id, enabled);
                }
                if (item.containsKey("toggle"))
                {
                    const auto *cfg = tanks.config(id);
                    if (cfg)
                        tanks.setPower(id, !cfg->power_on);
                }
                else if (item.containsKey("power_on"))
                {
                    const bool power_on = item["power_on"].is<bool>() ? item["power_on"].as<bool>()
                                                                      : (item["power_on"].as<int>() != 0);
                    tanks.setPower(id, power_on);
                }
                if (item.containsKey("name"))
                    tanks.setName(id, String(item["name"] | ""));
                if (item.containsKey("group_id"))
                    tanks.setGroupId(id, (uint8_t)(item["group_id"] | 0));
                if (item.containsKey("low"))
                    tanks.setLevelLow(id, (uint8_t)(item["low"] | TankController::kInvalidPort));
                if (item.containsKey("mid"))
                    tanks.setLevelMid(id, (uint8_t)(item["mid"] | TankController::kInvalidPort));
                if (item.containsKey("full"))
                    tanks.setLevelFull(id, (uint8_t)(item["full"] | TankController::kInvalidPort));
                if (item.containsKey("valve"))
                    tanks.setValveRelay(id, (uint8_t)(item["valve"] | TankController::kInvalidPort));
                if (item.containsKey("pump"))
                    tanks.setPumpRelay(id, (uint8_t)(item["pump"] | TankController::kInvalidPort));
                if (item.containsKey("alarm"))
                    tanks.setAlarmRelay(id, (uint8_t)(item["alarm"] | TankController::kInvalidPort));
            }
            return;
        }
        handleTankFrame_(node_id, action, params);
        return;
    }
    if (strcmp(route.feature, "watering") == 0)
    {
        handleWateringFrame_(node_id, action, params);
        return;
    }
}

void AppRuntime::handleSocketFrame_(uint32_t node_id, const String &action, JsonVariantConst params){
    (void)node_id;
    const bool lights = (action == "set_lights");
    if (!lights && action != "set")
        return;

    JsonArrayConst items = params["items"].as<JsonArrayConst>();
    if (items.isNull())
    {
        JsonVariantConst nested = params["params"];
        if (!nested.isNull())
            items = nested["items"].as<JsonArrayConst>();
    }
    if (items.isNull())
        return;

    SocketController &sockets = control.controllers.sockets();
    String source = params["source"] | "";
    if (!source.length())
    {
        JsonVariantConst nested = params["params"];
        if (!nested.isNull())
            source = nested["source"] | "";
    }
    String source_user = params["source_user"] | "";
    if (!source_user.length())
    {
        JsonVariantConst nested = params["params"];
        if (!nested.isNull())
            source_user = nested["source_user"] | "";
    }
    const char *source_c = source.length() ? source.c_str() : "stack";
    const char *source_user_c = source_user.length() ? source_user.c_str() : "-";
    const String slave_name = hw.plc.deviceName();
    auto logSwitch = [&](uint8_t id, bool state_after) {
        if (lights)
        {
            const auto *cfg = sockets.lightConfig(id);
            const char *name_c = (cfg && cfg->name.length()) ? cfg->name.c_str() : "-";
            core.logs.info(F("STACK"), F("Light switched: slave: %s source: %s user: %s id: %u name: %s state: %s"),
                           slave_name.length() ? slave_name.c_str() : "unknown",
                           source_c,
                           source_user_c,
                           (unsigned)id,
                           name_c,
                           state_after ? "on" : "off");
            return;
        }
        const auto *cfg = sockets.config(id);
        const char *name_c = (cfg && cfg->name.length()) ? cfg->name.c_str() : "-";
        core.logs.info(F("STACK"), F("Socket switched: slave: %s source: %s user: %s id: %u name: %s state: %s"),
                       slave_name.length() ? slave_name.c_str() : "unknown",
                       source_c,
                       source_user_c,
                       (unsigned)id,
                       name_c,
                       state_after ? "on" : "off");
    };
    bool changed = false;
    for (JsonObjectConst item : items)
    {
        const uint8_t id = (uint8_t)(item["id"] | 0);
        if (id == 0)
            continue;

        bool state_before_known = false;
        bool state_before = false;
        if (lights)
        {
            if (const auto *st_before = sockets.lightState(id))
            {
                state_before = st_before->relay_on;
                state_before_known = true;
            }
        }
        else
        {
            if (const auto *st_before = sockets.state(id))
            {
                state_before = st_before->relay_on;
                state_before_known = true;
            }
        }

        if (item["enabled"].is<bool>() || item["enabled"].is<int>())
        {
            const bool enabled = item["enabled"].as<bool>();
            changed = (lights ? sockets.setLightEnabled(id, enabled)
                              : sockets.setEnabled(id, enabled)) || changed;
        }
        if (!item["name"].isNull())
        {
            const String name = item["name"] | "";
            changed = (lights ? sockets.setLightName(id, name)
                              : sockets.setName(id, name)) || changed;
        }
        if (item["button"].is<int>())
        {
            const uint8_t port = (uint8_t)(item["button"] | SocketController::kInvalidPort);
            changed = (lights ? sockets.setLightButtonPort(id, port)
                              : sockets.setButtonPort(id, port)) || changed;
        }
        if (item["relay"].is<int>())
        {
            const uint8_t port = (uint8_t)(item["relay"] | SocketController::kInvalidPort);
            changed = (lights ? sockets.setLightRelayPort(id, port)
                              : sockets.setRelayPort(id, port)) || changed;
        }
        if (item["group_id"].is<int>())
        {
            const uint8_t group_id = (uint8_t)(item["group_id"] | 0);
            changed = (lights ? sockets.setLightGroupId(id, group_id)
                              : sockets.setGroupId(id, group_id)) || changed;
        }

        if (item["state"].is<bool>() || item["state"].is<int>())
        {
            const bool state_on = item["state"].as<bool>();
            const bool ok = lights ? sockets.setLightRelayById(id, state_on)
                                   : sockets.setRelayById(id, state_on);
            changed = ok || changed;
            if (ok)
            {
                const bool state_after = lights
                                             ? (sockets.lightState(id) ? sockets.lightState(id)->relay_on : state_on)
                                             : (sockets.state(id) ? sockets.state(id)->relay_on : state_on);
                if (!state_before_known || state_after != state_before)
                    logSwitch(id, state_after);
            }
            continue;
        }

        const bool toggle = item["toggle"].is<bool>()
            ? item["toggle"].as<bool>()
            : (item["toggle"].is<int>() && item["toggle"].as<int>() != 0);
        if (toggle)
        {
            const bool ok = lights ? sockets.toggleLightRelayById(id)
                                   : sockets.toggleRelayById(id);
            changed = ok || changed;
            if (ok)
            {
                const bool state_after = lights
                                             ? (sockets.lightState(id) ? sockets.lightState(id)->relay_on : false)
                                             : (sockets.state(id) ? sockets.state(id)->relay_on : false);
                logSwitch(id, state_after);
            }
        }
    }
}

void AppRuntime::appendSocketSnapshotSummary_(JsonObject root) const{
    JsonObject summary = root["summary"].to<JsonObject>();
    JsonObject sockets_out = summary["sockets"].to<JsonObject>();
    JsonObject lights_out = summary["lights"].to<JsonObject>();

    uint16_t sockets_enabled = 0;
    uint16_t sockets_on = 0;
    uint16_t lights_enabled = 0;
    uint16_t lights_on = 0;

    SocketController &sockets = control.controllers.sockets();
    auto guard = sockets.lockGuard();
    for (size_t i = 0; i < SocketController::kSocketCount; ++i)
    {
        const auto *cfg = sockets.configByIndex(i);
        const auto *st = sockets.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;
        ++sockets_enabled;
        if (st->relay_on)
            ++sockets_on;
    }
    for (size_t i = 0; i < SocketController::kLightCount; ++i)
    {
        const auto *cfg = sockets.lightConfigByIndex(i);
        const auto *st = sockets.lightStateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;
        ++lights_enabled;
        if (st->relay_on)
            ++lights_on;
    }

    sockets_out["enabled"] = sockets_enabled;
    sockets_out["on"] = sockets_on;
    lights_out["enabled"] = lights_enabled;
    lights_out["on"] = lights_on;
}

void AppRuntime::appendSystemSnapshot_(JsonObject root) const{
    Ds3231Mz::DateTime dt{};
    char date_buf[16] = {};
    char time_buf[16] = {};
    float rtc_temp_c = 0.0f;
    const bool rtc_time_ok = hw.rtc.Time(dt);
    const bool rtc_temp_ok = hw.rtc.readTemp(rtc_temp_c);

    if (rtc_time_ok)
    {
        snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u", (unsigned)dt.year, (unsigned)dt.month,
                 (unsigned)dt.day);
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u", (unsigned)dt.hour, (unsigned)dt.minute,
                 (unsigned)dt.second);
    }

    root["device_name"] = hw.plc.deviceName();
    root["rtc_date"] = rtc_time_ok ? String(date_buf) : String("n/a");
    root["rtc_time"] = rtc_time_ok ? String(time_buf) : String("n/a");
    root["rtc_temp"] = rtc_temp_ok ? rtc_temp_c : 0.0f;
    root["rtc_temp_ok"] = rtc_temp_ok;
    root["board_temp"] = hw.plc.boardTemp();
    root["fan_on"] = hw.plc.fanStatus();
    root["fan_html"] = hw.plc.fanStatus()
                           ? "<span class=\"status-dot status-on\" title=\"enabled\"></span>"
                           : "<span class=\"status-dot status-off\" title=\"disabled\"></span>";
}

void AppRuntime::appendSocketSnapshotItems_(JsonObject root) const{
    JsonObject controllers_out = root["controllers"].to<JsonObject>();
    JsonArray sockets_out = controllers_out["sockets"].to<JsonArray>();

    SocketController &sockets = control.controllers.sockets();
    auto guard = sockets.lockGuard();
    for (size_t i = 0; i < SocketController::kSocketCount; ++i)
    {
        const auto *cfg = sockets.configByIndex(i);
        const auto *st = sockets.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;
        JsonObject o = sockets_out.add<JsonObject>();
        o["id"] = cfg->id;
        o["enabled"] = true;
        o["state"] = st->relay_on;
        o["button"] = cfg->button_port;
        o["relay"] = cfg->relay_port;
        o["group_id"] = cfg->group_id;
        if (cfg->name.length())
            o["name"] = cfg->name;
    }
}

void AppRuntime::appendLightSnapshotItems_(JsonObject root) const{
    JsonObject controllers_out = root["controllers"].to<JsonObject>();
    JsonArray lights_out = controllers_out["lights"].to<JsonArray>();

    SocketController &sockets = control.controllers.sockets();
    auto guard = sockets.lockGuard();
    for (size_t i = 0; i < SocketController::kLightCount; ++i)
    {
        const auto *cfg = sockets.lightConfigByIndex(i);
        const auto *st = sockets.lightStateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;
        JsonObject o = lights_out.add<JsonObject>();
        o["id"] = cfg->id;
        o["enabled"] = true;
        o["state"] = st->relay_on;
        o["button"] = cfg->button_port;
        o["relay"] = cfg->relay_port;
        o["group_id"] = cfg->group_id;
        if (cfg->name.length())
            o["name"] = cfg->name;
    }
}

void AppRuntime::appendSocketSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const{
    JsonObject summary = root["summary"].to<JsonObject>();
    JsonObject sockets_summary = summary["sockets"].to<JsonObject>();
    JsonObject controllers_out = root["controllers"].to<JsonObject>();
    JsonArray sockets_out = controllers_out["sockets"].to<JsonArray>();

    uint16_t sockets_enabled = 0;
    uint16_t sockets_on = 0;
    uint16_t current_index = 0;

    SocketController &sockets = control.controllers.sockets();
    auto guard = sockets.lockGuard();
    for (size_t i = 0; i < SocketController::kSocketCount; ++i)
    {
        const auto *cfg = sockets.configByIndex(i);
        const auto *st = sockets.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;

        ++sockets_enabled;
        if (st->relay_on)
            ++sockets_on;

        if (current_index < offset)
        {
            ++current_index;
            continue;
        }
        if ((uint16_t)sockets_out.size() >= limit)
            continue;

        JsonObject o = sockets_out.add<JsonObject>();
        o["id"] = cfg->id;
        o["enabled"] = true;
        o["state"] = st->relay_on;
        o["button"] = cfg->button_port;
        o["relay"] = cfg->relay_port;
        o["group_id"] = cfg->group_id;
        if (cfg->name.length())
            o["name"] = cfg->name;
        ++current_index;
    }

    sockets_summary["enabled"] = sockets_enabled;
    sockets_summary["on"] = sockets_on;
    root["offset"] = offset;
    root["limit"] = limit;
    root["total"] = sockets_enabled;
}

void AppRuntime::appendLightSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const{
    JsonObject summary = root["summary"].to<JsonObject>();
    JsonObject lights_summary = summary["lights"].to<JsonObject>();
    JsonObject controllers_out = root["controllers"].to<JsonObject>();
    JsonArray lights_out = controllers_out["lights"].to<JsonArray>();

    uint16_t lights_enabled = 0;
    uint16_t lights_on = 0;
    uint16_t current_index = 0;

    SocketController &sockets = control.controllers.sockets();
    auto guard = sockets.lockGuard();
    for (size_t i = 0; i < SocketController::kLightCount; ++i)
    {
        const auto *cfg = sockets.lightConfigByIndex(i);
        const auto *st = sockets.lightStateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;

        ++lights_enabled;
        if (st->relay_on)
            ++lights_on;

        if (current_index < offset)
        {
            ++current_index;
            continue;
        }
        if ((uint16_t)lights_out.size() >= limit)
            continue;

        JsonObject o = lights_out.add<JsonObject>();
        o["id"] = cfg->id;
        o["enabled"] = true;
        o["state"] = st->relay_on;
        o["button"] = cfg->button_port;
        o["relay"] = cfg->relay_port;
        o["group_id"] = cfg->group_id;
        if (cfg->name.length())
            o["name"] = cfg->name;
        ++current_index;
    }

    lights_summary["enabled"] = lights_enabled;
    lights_summary["on"] = lights_on;
    root["offset"] = offset;
    root["limit"] = limit;
    root["total"] = lights_enabled;
}

void AppRuntime::appendMeteoSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const{
    JsonObject summary = root["summary"].to<JsonObject>();
    JsonObject meteo_summary = summary["meteo"].to<JsonObject>();
    JsonObject controllers_out = root["controllers"].to<JsonObject>();
    JsonArray meteo_out = controllers_out["meteo"].to<JsonArray>();

    uint16_t meteo_enabled = 0;
    uint16_t meteo_ok = 0;
    uint16_t current_index = 0;

    MeteoController &meteo = control.controllers.meteo();
    auto guard = meteo.lockGuard();
    for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
    {
        const auto *cfg = meteo.configByIndex(i);
        const auto *st = meteo.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;

        ++meteo_enabled;
        if (st->ok)
            ++meteo_ok;

        if (current_index < offset)
        {
            ++current_index;
            continue;
        }
        if ((uint16_t)meteo_out.size() >= limit)
            continue;

        JsonObject o = meteo_out.add<JsonObject>();
        o["id"] = cfg->id;
        o["enabled"] = true;
        o["group_id"] = cfg->group_id;
        o["type_id"] = (uint8_t)cfg->type;
        o["type"] = MeteoController::typeName(cfg->type);
        o["pin"] = cfg->dht_pin;
        o["addr_set"] = cfg->ds18_addr_set;
        if (cfg->ds18_addr_set)
        {
            char hex[17] = {};
            MeteoController::formatHexAddr(cfg->ds18_addr, hex);
            o["addr"] = hex;
        }
        o["src_node"] = (unsigned long)cfg->source_node_id;
        o["src_sensor"] = cfg->source_sensor_id;
        o["has_read"] = (st->last_read_ms != 0);
        o["ok"] = st->ok;
        o["has_temp"] = st->has_temp;
        o["has_hum"] = st->has_humidity;
        if (st->has_temp)
            o["temp_c"] = st->temp_c;
        if (st->has_humidity)
            o["hum"] = st->humidity;
        const uint32_t age_s = st->last_read_ms ? (uint32_t)((millis() - st->last_read_ms) / 1000u) : 0u;
        o["age_s"] = age_s;
        if (cfg->name.length())
            o["name"] = cfg->name;
        ++current_index;
    }

    meteo_summary["enabled"] = meteo_enabled;
    meteo_summary["ok"] = meteo_ok;
    root["offset"] = offset;
    root["limit"] = limit;
    root["total"] = meteo_enabled;
}

void AppRuntime::appendThermoSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const{
    JsonObject summary = root["summary"].to<JsonObject>();
    JsonObject thermo_summary = summary["thermo"].to<JsonObject>();
    JsonObject controllers_out = root["controllers"].to<JsonObject>();
    JsonArray thermo_out = controllers_out["thermo"].to<JsonArray>();

    uint16_t thermo_enabled = 0;
    uint16_t thermo_active = 0;
    uint16_t current_index = 0;

    ThermoController &thermo = control.controllers.thermo();
    auto guard = thermo.lockGuard();
    for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
    {
        const auto *cfg = thermo.configByIndex(i);
        const auto *st = thermo.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;

        ++thermo_enabled;
        if (st->heat_on || st->cool_on)
            ++thermo_active;

        if (current_index < offset)
        {
            ++current_index;
            continue;
        }
        if ((uint16_t)thermo_out.size() >= limit)
            continue;

        JsonObject o = thermo_out.add<JsonObject>();
        o["id"] = cfg->id;
        o["enabled"] = true;
        o["group_id"] = cfg->group_id;
        o["sensor_id"] = cfg->sensor_id;
        o["sensor_node_id"] = (unsigned long)cfg->sensor_node_id;
        o["heat_port"] = cfg->heat_port;
        o["cool_port"] = cfg->cool_port;
        o["button_port"] = cfg->button_port;
        o["mode_id"] = (uint8_t)cfg->mode;
        o["mode"] = ThermoController::modeName(cfg->mode);
        o["target_c"] = cfg->target_c;
        o["hyst"] = cfg->hysteresis;
        o["power_on"] = st->power_on;
        o["heat_on"] = st->heat_on;
        o["cool_on"] = st->cool_on;
        if (cfg->name.length())
            o["name"] = cfg->name;
        ++current_index;
    }

    thermo_summary["enabled"] = thermo_enabled;
    thermo_summary["active"] = thermo_active;
    root["offset"] = offset;
    root["limit"] = limit;
    root["total"] = thermo_enabled;
}

void AppRuntime::appendTankSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const{
    JsonObject summary = root["summary"].to<JsonObject>();
    JsonObject tanks_summary = summary["tanks"].to<JsonObject>();
    JsonObject controllers_out = root["controllers"].to<JsonObject>();
    JsonArray tanks_out = controllers_out["tanks"].to<JsonArray>();

    uint16_t tanks_enabled = 0;
    uint16_t tanks_alert = 0;
    uint16_t current_index = 0;

    TankController &tanks = control.controllers.tanks();
    auto guard = tanks.lockGuard();
    for (size_t i = 0; i < TankController::kTankCount; ++i)
    {
        const auto *cfg = tanks.configByIndex(i);
        const auto *st = tanks.stateByIndex(i);
        if (!cfg || !st || !cfg->enabled)
            continue;

        ++tanks_enabled;
        if (!st->levels_ok || (!st->level_low && !st->level_mid && !st->level_full) || st->alarm_on || st->pump_on)
            ++tanks_alert;

        if (current_index < offset)
        {
            ++current_index;
            continue;
        }
        if ((uint16_t)tanks_out.size() >= limit)
            continue;

        JsonObject o = tanks_out.add<JsonObject>();
        o["id"] = cfg->id;
        o["enabled"] = true;
        o["group_id"] = cfg->group_id;
        o["power_on"] = cfg->power_on;
        o["low"] = cfg->level_low;
        o["mid"] = cfg->level_mid;
        o["full"] = cfg->level_full;
        o["valve"] = cfg->relay_valve;
        o["pump"] = cfg->relay_pump;
        o["alarm"] = cfg->relay_alarm;
        o["level_low"] = st->level_low;
        o["level_mid"] = st->level_mid;
        o["level_full"] = st->level_full;
        o["levels_ok"] = st->levels_ok;
        o["valve_on"] = st->valve_on;
        o["pump_on"] = st->pump_on;
        o["alarm_on"] = st->alarm_on;
        if (cfg->name.length())
            o["name"] = cfg->name;
        ++current_index;
    }

    tanks_summary["enabled"] = tanks_enabled;
    tanks_summary["alert"] = tanks_alert;
    root["offset"] = offset;
    root["limit"] = limit;
    root["total"] = tanks_enabled;
}

void AppRuntime::appendControllerSnapshotSummary_(JsonObject root) const{
    appendSocketSnapshotSummary_(root);
    JsonObject summary = root["summary"].to<JsonObject>();

    uint16_t meteo_enabled = 0;
    uint16_t meteo_ok = 0;
    {
        MeteoController &meteo = control.controllers.meteo();
        auto guard = meteo.lockGuard();
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            const auto *st = meteo.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            ++meteo_enabled;
            if (st->ok)
                ++meteo_ok;
        }
    }
    JsonObject meteo_out = summary["meteo"].to<JsonObject>();
    meteo_out["enabled"] = meteo_enabled;
    meteo_out["ok"] = meteo_ok;

    uint16_t thermo_enabled = 0;
    uint16_t thermo_active = 0;
    {
        ThermoController &thermo = control.controllers.thermo();
        auto guard = thermo.lockGuard();
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            const auto *st = thermo.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            ++thermo_enabled;
            if (st->heat_on || st->cool_on)
                ++thermo_active;
        }
    }
    JsonObject thermo_out = summary["thermo"].to<JsonObject>();
    thermo_out["enabled"] = thermo_enabled;
    thermo_out["active"] = thermo_active;

    uint16_t tanks_enabled = 0;
    uint16_t tanks_alert = 0;
    {
        TankController &tanks = control.controllers.tanks();
        auto guard = tanks.lockGuard();
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = tanks.configByIndex(i);
            const auto *st = tanks.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            ++tanks_enabled;
            if (!st->levels_ok || (!st->level_low && !st->level_mid && !st->level_full) || st->alarm_on || st->pump_on)
                ++tanks_alert;
        }
    }
    JsonObject tanks_out = summary["tanks"].to<JsonObject>();
    tanks_out["enabled"] = tanks_enabled;
    tanks_out["alert"] = tanks_alert;

    uint16_t septic_enabled = 0;
    uint16_t septic_alert = 0;
    {
        SepticController &septic = control.controllers.septic();
        auto guard = septic.lockGuard();
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = septic.configByIndex(i);
            const auto *st = septic.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            ++septic_enabled;
            if (st->warning || st->alarm)
                ++septic_alert;
        }
    }
    JsonObject septic_out = summary["septic"].to<JsonObject>();
    septic_out["enabled"] = septic_enabled;
    septic_out["alert"] = septic_alert;

    uint16_t watering_enabled = 0;
    uint16_t watering_active = 0;
    {
        WateringController &watering = control.controllers.watering();
        auto guard = watering.lockGuard();
        for (size_t i = 0; i < WateringController::kRuleCount; ++i)
        {
            const auto *cfg = watering.configByIndex(i);
            const auto *st = watering.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            ++watering_enabled;
            if (st->active)
                ++watering_active;
        }
    }
    JsonObject watering_out = summary["watering"].to<JsonObject>();
    watering_out["enabled"] = watering_enabled;
    watering_out["active"] = watering_active;

    {
        SecurityController &security = control.controllers.security();
        auto guard = security.lockGuard();
        uint16_t sensors_enabled = 0;
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = security.configByIndex(i);
            if (cfg && cfg->enabled)
                ++sensors_enabled;
        }
        JsonObject security_out = summary["security"].to<JsonObject>();
        security_out["enabled"] = security.controllerEnabled();
        security_out["sensors_enabled"] = sensors_enabled;
        security_out["armed"] = security.armed();
        security_out["alarm"] = security.alarmOn();
    }

    {
        RingController &ring = control.controllers.ring();
        auto guard = ring.lockGuard();
        JsonObject ring_out = summary["ring"].to<JsonObject>();
        ring_out["enabled"] = ring.controllerEnabled() && ring.config().enabled;
        ring_out["on"] = ring.state().relay_on;
    }

    {
        AvrController &avr = control.controllers.avr();
        auto guard = avr.lockGuard();
        JsonObject avr_out = summary["avr"].to<JsonObject>();
        avr_out["enabled"] = avr.controllerEnabled() && avr.config().enabled;
        avr_out["fault"] = avr.fault() != AvrController::Fault::None;
        avr_out["active_source"] = AvrController::sourceName(avr.activeSource());
        avr_out["active_source_id"] = (uint8_t)avr.activeSource();
    }

    uint16_t leak_enabled = 0;
    uint16_t leak_alert = 0;
    {
        LeakController &leak = control.controllers.leak();
        auto guard = leak.lockGuard();
        for (size_t i = 0; i < LeakController::kZoneCount; ++i)
        {
            const auto *cfg = leak.configByIndex(i);
            const auto *st = leak.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            ++leak_enabled;
            if (st->wet || st->alarm_latched)
                ++leak_alert;
        }
    }
    JsonObject leak_out = summary["leak"].to<JsonObject>();
    leak_out["enabled"] = leak_enabled;
    leak_out["alert"] = leak_alert;
}

void AppRuntime::handleSepticFrame_(uint32_t node_id, const String &action, JsonVariantConst params){
    if (action != "level")
        return;
    const String level = params["level"] | "";
    bool is_alarm = false;
    if (level.length())
    {
        String low = level;
        low.toLowerCase();
        is_alarm = (low == "alarm" || low == "overflow");
    }
    else
    {
        is_alarm = params["alarm"].is<bool>() ? params["alarm"].as<bool>()
                                              : (params["alarm"].as<int>() != 0);
    }
    const uint8_t septic_id = (uint8_t)(params["id"] | 0);
    const String name = params["name"] | "";
    const String source = stackNodeLabel_(node_id);
    core.logs.warn(F("SEPTIC"), F("remote %s: unit: %s id: %u name: %s"),
                   is_alarm ? "alarm" : "warning",
                   source.c_str(),
                   (unsigned)septic_id,
                   name.length() ? name.c_str() : "");
    if (septic_id > 0 && septic_id <= 32)
        hw.plc.setAlarmDetail(PlcControl::AlarmModule::Septic, (uint8_t)(septic_id - 1), is_alarm);
    const int unit_idx = stackNodeIndex_(node_id);
    if (unit_idx >= 0 && unit_idx < 32)
        hw.plc.setAlarmUnit(PlcControl::AlarmModule::Septic, (uint8_t)unit_idx, is_alarm);
    control.controllers.septic().notifyRemoteLevel(source, septic_id, name, is_alarm);
    DynamicJsonDocument event_doc(224);
    event_doc["id"] = septic_id;
    if (name.length())
        event_doc["name"] = name;
    event_doc["alarm"] = is_alarm;
    if (source.length())
        event_doc["unit_name"] = source;
    String event_json;
    serializeJson(event_doc, event_json);
    publishCloudStackEvent_(node_id, "septic.level", is_alarm ? "alarm" : "warning", event_json);
}

void AppRuntime::handleTankFrame_(uint32_t node_id, const String &action, JsonVariantConst params){
    if (action != "empty")
        return;
    const bool empty = params["empty"].is<bool>() ? params["empty"].as<bool>()
                                                  : (params["empty"].as<int>() != 0);
    if (!empty)
        return;
    const uint8_t tank_id = (uint8_t)(params["id"] | 0);
    const String name = params["name"] | "";
    const String source = stackNodeLabel_(node_id);
    core.logs.warn(F("TANK"), F("remote empty: unit: %s id: %u name: %s"),
                   source.c_str(),
                   (unsigned)tank_id,
                   name.length() ? name.c_str() : "");
    if (tank_id > 0 && tank_id <= 32)
        hw.plc.setAlarmDetail(PlcControl::AlarmModule::Tanks, (uint8_t)(tank_id - 1), true);
    const int unit_idx = stackNodeIndex_(node_id);
    if (unit_idx >= 0 && unit_idx < 32)
        hw.plc.setAlarmUnit(PlcControl::AlarmModule::Tanks, (uint8_t)unit_idx, true);
    control.controllers.tanks().notifyRemoteEmpty(source, tank_id, name);
    DynamicJsonDocument event_doc(192);
    event_doc["id"] = tank_id;
    if (name.length())
        event_doc["name"] = name;
    event_doc["empty"] = true;
    if (source.length())
        event_doc["unit_name"] = source;
    String event_json;
    serializeJson(event_doc, event_json);
    publishCloudStackEvent_(node_id, "tanks.level", "empty", event_json);
}

void AppRuntime::handleWateringFrame_(uint32_t node_id, const String &action, JsonVariantConst params){
    if (action != "event")
        return;
    const String event = params["event"] | "";
    if (!event.length())
        return;
    const uint8_t rule_id = (uint8_t)(params["id"] | 0);
    const uint8_t port = (uint8_t)(params["port"] | WateringController::kInvalidPort);
    const uint8_t tank_id = (uint8_t)(params["tank"] | 0);
    const String name = params["name"] | "";
    const String reason = params["reason"] | "";
    const uint32_t remaining_ms = (uint32_t)(params["remaining_ms"] | 0u);
    const uint8_t resume_level = (uint8_t)(params["resume_level"] | 0u);
    const String source = stackNodeLabel_(node_id);

    String msg;
    msg.reserve(64);
    msg += "remote ";
    msg += event;
    if (reason.length())
    {
        msg += " (";
        msg += reason;
        msg += ")";
    }
    core.logs.info(F("WATER"), F("%s: unit: %s rule: %u name: %s port: %u tank: %u rem_ms: %lu resume_lvl: %u"),
                   msg.c_str(),
                   source.c_str(),
                   (unsigned)rule_id,
                   name.length() ? name.c_str() : "",
                   (unsigned)port,
                   (unsigned)tank_id,
                   (unsigned long)remaining_ms,
                   (unsigned)resume_level);
    DynamicJsonDocument event_doc(288);
    event_doc["id"] = rule_id;
    if (name.length())
        event_doc["name"] = name;
    event_doc["port"] = port;
    event_doc["tank_id"] = tank_id;
    event_doc["remaining_ms"] = remaining_ms;
    event_doc["resume_level"] = resume_level;
    if (reason.length())
        event_doc["reason_detail"] = reason;
    if (source.length())
        event_doc["unit_name"] = source;
    String event_json;
    serializeJson(event_doc, event_json);
    publishCloudStackEvent_(node_id, "watering.rule", event.c_str(), event_json);
}

void AppRuntime::updateSepticNotifyMode_(){
    control.controllers.septic().setNotifyEnabled(stackMasterActive_());
}

void AppRuntime::updateTanksNotifyMode_(){
    control.controllers.tanks().setNotifyEnabled(stackMasterActive_());
}

void AppRuntime::flushPendingSepticDetect_(){
    if (!_pending_septic_detect)
        return;
    if (!stackSlaveActive_())
        return;
    _pending_septic_detect = false;
    sendSepticDetectToMaster_(_pending_septic_id, _pending_septic_name, _pending_septic_alarm);
}

void AppRuntime::flushPendingTankEmpty_(){
    if (!_pending_tank_empty)
        return;
    if (!stackSlaveActive_())
        return;
    _pending_tank_empty = false;
    sendTankEmptyToMaster_(_pending_tank_id, _pending_tank_name);
}

void AppRuntime::flushPendingWateringEvent_(){
    if (!_pending_watering_event)
        return;
    if (!stackSlaveActive_())
        return;
    _pending_watering_event = false;
    sendWateringEventToMaster_(_pending_watering_event_type, _pending_watering_event_cfg,
                               _pending_watering_event_state);
}

void AppRuntime::flushPendingStackSocketsResponse_(){
    if (!_pending_stack_sockets_response)
        return;
    if (!stackSlaveActive_())
        return;
    if (!net.network.stackSlaveAuthorized())
        return;

    const uint32_t target_node = _pending_stack_sockets_target_node;
    const uint32_t reply_to = _pending_stack_sockets_reply_to;
    const uint16_t offset = _pending_stack_sockets_response_offset;
    uint16_t limit = _pending_stack_sockets_response_limit;
    if (target_node == 0)
        return;
    if (limit == 0)
        limit = 8;
    if (limit > 8)
        limit = 8;

    DynamicJsonDocument doc(1536);
    appendSocketSnapshotPage_(doc.to<JsonObject>(), offset, limit);
    if (doc.overflowed())
    {
        core.logs.warn(F("STACK"), F("Slave sockets snapshot overflow: range: %u-%u"),
                       (unsigned)offset, (unsigned)(offset + limit - 1u));
    }
    const bool sent = net.network.stackSlaveSendResponse(target_node, "sockets", "snapshot", reply_to, &doc);
    if (sent)
        _pending_stack_sockets_response = false;
}

void AppRuntime::flushPendingStackSocketsPage_(){
    if (!_pending_stack_sockets_page)
        return;
    if (!stackMasterActive_())
        return;
    _pending_stack_sockets_page = false;
    const bool log_sync = _pending_stack_sockets_log;
    _pending_stack_sockets_log = false;

    const uint32_t node_id = _pending_stack_sockets_node_id;
    const uint16_t offset = _pending_stack_sockets_offset;
    uint16_t limit = _pending_stack_sockets_limit;
    if (node_id == 0)
        return;
    if (limit == 0)
        limit = 8;

    DynamicJsonDocument req(64);
    req["offset"] = offset;
    req["limit"] = limit;

    const uint16_t range_end = (uint16_t)(offset + limit - 1u);
    const String label = stackNodeLabel_(node_id);

    const bool sent = net.network.stackRoute().sendRequest(node_id, "sockets", "snapshot_req", &req,
                                                           StackRouteAdapter::Mode::Json, true);
    if (sent)
    {
        if (log_sync)
        {
            core.logs.info(F("STACK"), F("Sync slave %s sockets: %u-%u"),
                           label.length() ? label.c_str() : "unknown",
                           (unsigned)offset, (unsigned)range_end);
        }
    }
    else
    {
        net.network.clearStackSocketsPageRequest(node_id);
        core.logs.warn(F("STACK"), F("Sync slave sockets request failed: node 0x%08lX range: %u-%u"),
                       (unsigned long)node_id, (unsigned)offset, (unsigned)range_end);
    }
}

void AppRuntime::flushPendingStackLightsResponse_(){
    if (!_pending_stack_lights_response)
        return;
    if (!stackSlaveActive_())
        return;
    if (!net.network.stackSlaveAuthorized())
        return;

    const uint32_t target_node = _pending_stack_lights_target_node;
    const uint32_t reply_to = _pending_stack_lights_reply_to;
    const uint16_t offset = _pending_stack_lights_response_offset;
    uint16_t limit = _pending_stack_lights_response_limit;
    if (target_node == 0)
        return;
    if (limit == 0)
        limit = 8;
    if (limit > 8)
        limit = 8;

    DynamicJsonDocument doc(1536);
    appendLightSnapshotPage_(doc.to<JsonObject>(), offset, limit);
    if (doc.overflowed())
    {
        core.logs.warn(F("STACK"), F("Slave lights snapshot overflow: range: %u-%u"),
                       (unsigned)offset, (unsigned)(offset + limit - 1u));
    }
    const bool sent = net.network.stackSlaveSendResponse(target_node, "lights", "snapshot", reply_to, &doc);
    if (sent)
        _pending_stack_lights_response = false;
}

void AppRuntime::flushPendingStackLightsPage_(){
    if (!_pending_stack_lights_page)
        return;
    if (!stackMasterActive_())
        return;
    _pending_stack_lights_page = false;
    const bool log_sync = _pending_stack_lights_log;
    _pending_stack_lights_log = false;

    const uint32_t node_id = _pending_stack_lights_node_id;
    const uint16_t offset = _pending_stack_lights_offset;
    uint16_t limit = _pending_stack_lights_limit;
    if (node_id == 0)
        return;
    if (limit == 0)
        limit = 8;

    DynamicJsonDocument req(64);
    req["offset"] = offset;
    req["limit"] = limit;

    const uint16_t range_end = (uint16_t)(offset + limit - 1u);
    const String label = stackNodeLabel_(node_id);

    const bool sent = net.network.stackRoute().sendRequest(node_id, "lights", "snapshot_req", &req,
                                                           StackRouteAdapter::Mode::Json, true);
    if (sent)
    {
        if (log_sync)
        {
            core.logs.info(F("STACK"), F("Sync slave %s lights: %u-%u"),
                           label.length() ? label.c_str() : "unknown",
                           (unsigned)offset, (unsigned)range_end);
        }
    }
    else
    {
        net.network.clearStackLightsPageRequest(node_id);
        core.logs.warn(F("STACK"), F("Sync slave lights request failed: node 0x%08lX range: %u-%u"),
                       (unsigned long)node_id, (unsigned)offset, (unsigned)range_end);
    }
}

void AppRuntime::flushPendingStackMeteoPage_(){
    if (!_pending_stack_meteo_page)
        return;
    if (!stackMasterActive_())
        return;
    _pending_stack_meteo_page = false;
    const bool log_sync = _pending_stack_meteo_log;
    _pending_stack_meteo_log = false;

    const uint32_t node_id = _pending_stack_meteo_node_id;
    const uint16_t offset = _pending_stack_meteo_offset;
    uint16_t limit = _pending_stack_meteo_limit;
    if (node_id == 0)
        return;
    if (limit == 0)
        limit = 8;

    DynamicJsonDocument req(64);
    req["offset"] = offset;
    req["limit"] = limit;

    const uint16_t range_end = (uint16_t)(offset + limit - 1u);
    const String label = stackNodeLabel_(node_id);

    const bool sent = net.network.stackRoute().sendRequest(node_id, "meteo", "snapshot_req", &req,
                                                           StackRouteAdapter::Mode::Json, true);
    if (sent)
    {
        if (log_sync)
        {
            core.logs.info(F("STACK"), F("Sync slave %s meteo: %u-%u"),
                           label.length() ? label.c_str() : "unknown",
                           (unsigned)offset, (unsigned)range_end);
        }
    }
    else
    {
        net.network.clearStackMeteoPageRequest(node_id);
        core.logs.warn(F("STACK"), F("Sync slave meteo request failed: node 0x%08lX range: %u-%u"),
                       (unsigned long)node_id, (unsigned)offset, (unsigned)range_end);
    }
}

void AppRuntime::flushPendingStackThermoPage_(){
    if (!_pending_stack_thermo_page)
        return;
    if (!stackMasterActive_())
        return;
    _pending_stack_thermo_page = false;
    const bool log_sync = _pending_stack_thermo_log;
    _pending_stack_thermo_log = false;

    const uint32_t node_id = _pending_stack_thermo_node_id;
    const uint16_t offset = _pending_stack_thermo_offset;
    uint16_t limit = _pending_stack_thermo_limit;
    if (node_id == 0)
        return;
    if (limit == 0)
        limit = 8;

    DynamicJsonDocument req(64);
    req["offset"] = offset;
    req["limit"] = limit;

    const uint16_t range_end = (uint16_t)(offset + limit - 1u);
    const String label = stackNodeLabel_(node_id);

    const bool sent = net.network.stackRoute().sendRequest(node_id, "thermo", "snapshot_req", &req,
                                                           StackRouteAdapter::Mode::Json, true);
    if (sent)
    {
        if (log_sync)
        {
            core.logs.info(F("STACK"), F("Sync slave %s thermo: %u-%u"),
                           label.length() ? label.c_str() : "unknown",
                           (unsigned)offset, (unsigned)range_end);
        }
    }
    else
    {
        net.network.clearStackThermoPageRequest(node_id);
        core.logs.warn(F("STACK"), F("Sync slave thermo request failed: node 0x%08lX range: %u-%u"),
                       (unsigned long)node_id, (unsigned)offset, (unsigned)range_end);
    }
}

void AppRuntime::flushPendingStackTanksPage_(){
    if (!_pending_stack_tanks_page)
        return;
    if (!stackMasterActive_())
        return;
    _pending_stack_tanks_page = false;
    const bool log_sync = _pending_stack_tanks_log;
    _pending_stack_tanks_log = false;

    const uint32_t node_id = _pending_stack_tanks_node_id;
    const uint16_t offset = _pending_stack_tanks_offset;
    uint16_t limit = _pending_stack_tanks_limit;
    if (node_id == 0)
        return;
    if (limit == 0)
        limit = 8;

    DynamicJsonDocument req(64);
    req["offset"] = offset;
    req["limit"] = limit;

    const uint16_t range_end = (uint16_t)(offset + limit - 1u);
    const String label = stackNodeLabel_(node_id);

    const bool sent = net.network.stackRoute().sendRequest(node_id, "tanks", "snapshot_req", &req,
                                                           StackRouteAdapter::Mode::Json, true);
    if (sent)
    {
        if (log_sync)
        {
            core.logs.info(F("STACK"), F("Sync slave %s tanks: %u-%u"),
                           label.length() ? label.c_str() : "unknown",
                           (unsigned)offset, (unsigned)range_end);
        }
    }
    else
    {
        net.network.clearStackTanksPageRequest(node_id);
        core.logs.warn(F("STACK"), F("Sync slave tanks request failed: node 0x%08lX range: %u-%u"),
                       (unsigned long)node_id, (unsigned)offset, (unsigned)range_end);
    }
}

void AppRuntime::flushPendingRingButton_(){
    if (!_pending_ring_button)
        return;
    if (!stackSlaveActive_())
        return;
    _pending_ring_button = false;
    sendRingButtonToMaster_(_pending_ring_button_pressed);
}

bool AppRuntime::sendRingButtonToMaster_(bool pressed){
    if (!stackSlaveActive_())
        return false;
    StaticJsonDocument<64> doc;
    doc["pressed"] = pressed;
    return net.network.stackRoute().sendEvent(0, "ring", "button", &doc, StackRouteAdapter::Mode::Json);
}

String AppRuntime::stackNodeLabel_(uint32_t node_id) const{
    const size_t count = net.network.stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device))
            continue;
        if (device.node_id == node_id)
        {
            String name = device.name;
            if (name.length() > 0)
                return name;
        }
    }
    char buf[12] = {};
    snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)node_id);
    return String(buf);
}

String AppRuntime::escapeHtml_(const String &in){
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); ++i)
    {
        const char c = in.charAt(i);
        switch (c)
        {
        case '&':
            out += F("&amp;");
            break;
        case '<':
            out += F("&lt;");
            break;
        case '>':
            out += F("&gt;");
            break;
        case '"':
            out += F("&quot;");
            break;
        case '\'':
            out += F("&#39;");
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

int AppRuntime::stackNodeIndex_(uint32_t node_id) const{
    const size_t count = net.network.stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(i, device))
            continue;
        if (device.node_id == node_id)
            return (int)i;
    }
    return -1;
}

void AppRuntime::logStackNodeInventory_(uint32_t node_id){
    if (node_id == 0 || !stackMasterActive_())
        return;
    StackDeviceRegistry::DeviceInfo device{};
    if (!net.network.stackDeviceSnapshotByNodeId(node_id, device) ||
        !device.online || (uint32_t)(millis() - device.last_seen_ms) > kStackNodeStaleMs)
        return;
    StackInventoryLogState *state = inventoryLogState_(node_id, true);
    if (!state)
        return;
    const String node = stackNodeLabel_(node_id);
    StackUnitSnapshot::Snapshot snapshot{};
    const bool has_snapshot = net.network.stackIndexStateSnapshot(node_id, snapshot);
    const bool sockets_snapshot_ready =
        has_snapshot && snapshot.updated_ms != 0 && snapshot.socket_count >= snapshot.sockets_enabled;
    const bool lights_snapshot_ready =
        has_snapshot && snapshot.updated_ms != 0 && snapshot.light_count >= snapshot.lights_enabled;

    if ((state->logged_mask & kInvSockets) == 0)
    {
        if (sockets_snapshot_ready)
        {
            for (uint8_t i = 0; i < snapshot.socket_count && i < StackUnitSnapshot::kSocketCount; ++i)
            {
                const auto &it = snapshot.sockets[i];
                if (!it.enabled)
                    continue;
                core.logs.info(F("STACK"), F("Sync slave unit: %s item: sockets id: %u name: %s"),
                               node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-");
            }
            state->logged_mask |= kInvSockets;
        }
    }

    if ((state->logged_mask & kInvLights) == 0)
    {
        if (lights_snapshot_ready)
        {
            for (uint8_t i = 0; i < snapshot.light_count && i < StackUnitSnapshot::kSocketCount; ++i)
            {
                const auto &it = snapshot.lights[i];
                if (!it.enabled)
                    continue;
                core.logs.info(F("STACK"), F("Sync slave unit: %s item: lights id: %u name: %s"),
                               node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-");
            }
            state->logged_mask |= kInvLights;
        }
    }

    state->logged_mask |= (kInvMeteo | kInvThermo | kInvTanks | kInvSeptic |
                           kInvSecurity | kInvWatering | kInvLeak | kInvAvr);

    if (!state->sync_complete_logged && (state->logged_mask & kInvAll) == kInvAll)
    {
        const String unit = stackNodeLabel_(node_id);
        core.logs.info(F("STACK"), F("Sync slave unit complete: %s"), unit.c_str());
        state->sync_complete_logged = true;
    }
}

AppRuntime::StackInventoryLogState *AppRuntime::inventoryLogState_(uint32_t node_id, bool create){
    if (node_id == 0)
        return nullptr;
    for (size_t i = 0; i < StackDeviceRegistry::kMaxDevices; ++i)
    {
        if (_stack_inventory_log[i].node_id == node_id)
            return &_stack_inventory_log[i];
    }
    if (!create)
        return nullptr;
    for (size_t i = 0; i < StackDeviceRegistry::kMaxDevices; ++i)
    {
        if (_stack_inventory_log[i].node_id == 0)
        {
            _stack_inventory_log[i].node_id = node_id;
            _stack_inventory_log[i].logged_mask = 0;
            _stack_inventory_log[i].sync_complete_logged = false;
            return &_stack_inventory_log[i];
        }
    }
    _stack_inventory_log[0].node_id = node_id;
    _stack_inventory_log[0].logged_mask = 0;
    _stack_inventory_log[0].sync_complete_logged = false;
    return &_stack_inventory_log[0];
}

void AppRuntime::clearInventoryLogState_(uint32_t node_id){
    if (node_id == 0)
        return;
    for (size_t i = 0; i < StackDeviceRegistry::kMaxDevices; ++i)
    {
        if (_stack_inventory_log[i].node_id != node_id)
            continue;
        _stack_inventory_log[i].node_id = 0;
        _stack_inventory_log[i].logged_mask = 0;
        _stack_inventory_log[i].sync_complete_logged = false;
        return;
    }
}
