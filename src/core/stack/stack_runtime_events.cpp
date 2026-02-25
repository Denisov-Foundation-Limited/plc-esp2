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
void StackRuntime::updateTankAlarms_(){
    TankController &tanks = control.controllers.tanks();
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
    if (stackMasterActive_())
    {
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t node_id = master.nodeIdAt(i);
            if (node_id == 0)
                continue;
            const auto *cache = _stack_cache.tanksCache(node_id);
            if (!cache || !cache->has_data || !cache->last_ok)
                continue;
            for (size_t j = 0; j < cache->item_count; ++j)
            {
                const auto &it = cache->items[j];
                if (!it.enabled)
                    continue;
                if (it.id == 0 || it.id > 32)
                    continue;
                const bool empty = !(it.level_low || it.level_mid || it.level_full);
                if (!it.levels_ok || empty)
                {
                    detail_mask |= (1u << (it.id - 1));
                    if (i < 32)
                        unit_mask |= (1u << i);
                }
            }
        }
    }
    hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Tanks, detail_mask);
    hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Tanks, unit_mask);
}

void StackRuntime::updateSepticAlarms_(){
    SepticController &septic = control.controllers.septic();
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
    if (stackMasterActive_())
    {
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t node_id = master.nodeIdAt(i);
            if (node_id == 0)
                continue;
            const auto *cache = _stack_cache.septicCache(node_id);
            if (!cache || !cache->has_data || !cache->items || !cache->last_ok)
                continue;
            for (size_t j = 0; j < cache->item_count; ++j)
            {
                const auto &it = cache->items[j];
                if (!it.enabled)
                    continue;
                if (it.id == 0 || it.id > 32)
                    continue;
                if (it.alarm)
                {
                    detail_mask |= (1u << (it.id - 1));
                    if (i < 32)
                        unit_mask |= (1u << i);
                }
            }
        }
    }
    hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Septic, detail_mask);
    hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Septic, unit_mask);
}

void StackRuntime::updateMeteoAlarms_(){
    MeteoController &meteo = control.controllers.meteo();
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
    if (stackMasterActive_())
    {
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t node_id = master.nodeIdAt(i);
            if (node_id == 0)
                continue;
            const auto *cache = _stack_cache.meteoCache(node_id);
            if (!cache || !cache->has_data || !cache->items || !cache->last_ok)
                continue;
            for (size_t j = 0; j < cache->item_count; ++j)
            {
                const auto &it = cache->items[j];
                if (!it.enabled)
                    continue;
                if (it.id == 0 || it.id > 32)
                    continue;
                if (!it.ok)
                {
                    detail_mask |= (1u << (it.id - 1));
                    if (i < 32)
                        unit_mask |= (1u << i);
                }
            }
        }
    }
    hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Meteo, detail_mask);
    hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Meteo, unit_mask);
}

void StackRuntime::onMeteoAlarm_(void *ctx, uint32_t node_id, uint8_t sensor_id, bool alarm){
    if (!ctx || sensor_id == 0 || sensor_id > 32)
        return;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    if (node_id != 0 && self->stackSlaveActive_())
        return;
    self->hw.plc.setAlarmDetail(PlcControl::AlarmModule::Meteo, (uint8_t)(sensor_id - 1), alarm);
    if (node_id == 0)
        return;
    const int unit_idx = self->stackNodeIndex_(node_id);
    if (unit_idx >= 0 && unit_idx < 32)
        self->hw.plc.setAlarmUnit(PlcControl::AlarmModule::Meteo, (uint8_t)unit_idx, alarm);
}

void StackRuntime::onStackNodeEvent_(void *ctx, uint32_t node_id, bool online){
    if (!ctx || node_id == 0)
        return;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    self->clearInventoryLogState_(node_id);
    String name;
    String ip;
    uint16_t fw_ver = 0;
    self->net.network.stackMaster().nodeInfo(node_id, name, ip, fw_ver);
    const String label = name.length() ? name : self->stackNodeLabel_(node_id);
    const char *ip_c = ip.length() ? ip.c_str() : "n/a";
    if (online)
    {
        self->comms.wifi.task();
        self->core.logs.info(F("STACK"), F("Unit online: %s id: 0x%08lX ip: %s fw: %u"),
                             label.c_str(), (unsigned long)node_id, ip_c, (unsigned)fw_ver);
        self->sendSecurityStateToNode_(node_id, self->control.controllers.security().armed(), true);
        self->enqueueStackBootstrapSync_(node_id);
    }
    else
    {
        self->core.logs.warn(F("STACK"), F("Unit offline: %s id: 0x%08lX ip: %s fw: %u"),
                             label.c_str(), (unsigned long)node_id, ip_c, (unsigned)fw_ver);
        self->removeStackBootstrapSync_(node_id);
    }
}

void StackRuntime::onSepticDetect_(void *ctx, uint8_t septic_id, const String &name, bool is_alarm){
    if (!ctx)
        return;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    if (is_alarm && septic_id > 0 && septic_id <= 32)
        self->hw.plc.setAlarmDetail(PlcControl::AlarmModule::Septic, (uint8_t)(septic_id - 1), true);
    self->sendSepticDetectToMaster_(septic_id, name, is_alarm);
}

void StackRuntime::onTankEmpty_(void *ctx, uint8_t tank_id, const String &name, bool empty){
    if (!ctx)
        return;
    if (!empty)
        return;
    static_cast<StackRuntime *>(ctx)->sendTankEmptyToMaster_(tank_id, name);
}

void StackRuntime::onWateringEvent_(void *ctx, WateringController::Event ev,
                             const WateringController::RuleConfig &cfg,
                             const WateringController::RuleState &st){
    if (!ctx)
        return;
    static_cast<StackRuntime *>(ctx)->sendWateringEventToMaster_(ev, cfg, st);
}

void StackRuntime::onRingHold_(void *ctx, bool on){
    if (!ctx)
        return;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
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

void StackRuntime::onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame){
    if (!ctx || node_id == 0)
        return;
    static_cast<StackRuntime *>(ctx)->handleStackFrame_(node_id, frame);
}

void StackRuntime::sendSepticDetectToMaster_(uint8_t septic_id, const String &name, bool is_alarm){
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
    {
        _pending_septic_detect = true;
        _pending_septic_id = septic_id;
        _pending_septic_name = name;
        _pending_septic_alarm = is_alarm;
        return;
    }
    StaticJsonDocument<192> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Septic;
    doc["action"] = "level";
    JsonObject params = doc["params"].to<JsonObject>();
    params["id"] = septic_id;
    params["alarm"] = is_alarm;
    params["level"] = is_alarm ? "alarm" : "warning";
    if (name.length())
        params["name"] = name;

    char payload[160] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    if (!node.send((uint8_t)StackMsgType::CmdSet,
                   reinterpret_cast<const uint8_t *>(payload), len))
    {
        _pending_septic_detect = true;
        _pending_septic_id = septic_id;
        _pending_septic_name = name;
        _pending_septic_alarm = is_alarm;
    }
}

void StackRuntime::sendTankEmptyToMaster_(uint8_t tank_id, const String &name){
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
    {
        _pending_tank_empty = true;
        _pending_tank_id = tank_id;
        _pending_tank_name = name;
        return;
    }
    StaticJsonDocument<160> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Tanks;
    doc["action"] = "empty";
    JsonObject params = doc["params"].to<JsonObject>();
    params["id"] = tank_id;
    params["empty"] = true;
    if (name.length())
        params["name"] = name;

    char payload[140] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    if (!node.send((uint8_t)StackMsgType::CmdSet,
                   reinterpret_cast<const uint8_t *>(payload), len))
    {
        _pending_tank_empty = true;
        _pending_tank_id = tank_id;
        _pending_tank_name = name;
    }
}

void StackRuntime::sendWateringEventToMaster_(WateringController::Event ev,
                                const WateringController::RuleConfig &cfg,
                                const WateringController::RuleState &st){
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
    {
        _pending_watering_event = true;
        _pending_watering_event_type = ev;
        _pending_watering_event_cfg = cfg;
        _pending_watering_event_state = st;
        return;
    }
    StaticJsonDocument<256> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Watering;
    doc["action"] = "event";
    JsonObject params = doc["params"].to<JsonObject>();
    params["id"] = cfg.id;
    if (cfg.name.length())
        params["name"] = cfg.name;
    if (cfg.port != WateringController::kInvalidPort)
        params["port"] = cfg.port;
    if (cfg.tank_id)
        params["tank"] = cfg.tank_id;
    if (cfg.resume_after_refill)
        params["resume"] = true;
    params["resume_level"] = cfg.resume_level;
    params["remaining_ms"] = st.remaining_ms;
    const char *event_str = "stop";
    switch (ev)
    {
    case WateringController::Event::Start:
        event_str = "start";
        break;
    case WateringController::Event::PauseEmpty:
        event_str = "pause";
        params["reason"] = "empty";
        break;
    case WateringController::Event::Resume:
        event_str = "resume";
        break;
    case WateringController::Event::StopDone:
        event_str = "stop";
        params["reason"] = "done";
        break;
    case WateringController::Event::StopEmpty:
        event_str = "stop";
        params["reason"] = "empty";
        break;
    case WateringController::Event::Stop:
    default:
        event_str = "stop";
        break;
    }
    params["event"] = event_str;

    char payload[224] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    if (!node.send((uint8_t)StackMsgType::CmdSet,
                   reinterpret_cast<const uint8_t *>(payload), len))
    {
        _pending_watering_event = true;
        _pending_watering_event_type = ev;
        _pending_watering_event_cfg = cfg;
        _pending_watering_event_state = st;
    }
}

void StackRuntime::broadcastRingHold_(bool on){
    if (!stackMasterActive_())
        return;
    StackMaster &master = net.network.stackMaster();
    const size_t count = master.nodeCount();
    if (count == 0)
        return;

    StaticJsonDocument<128> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Ring;
    doc["action"] = "set";
    JsonObject params = doc["params"].to<JsonObject>();
    params["state"] = on;
    const String key = cfg.configs_manager.stackApiKey();
    if (key.length())
        doc["api_key"] = key;

    char payload[128] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return;
    for (size_t i = 0; i < count; ++i)
        master.sendTo(master.nodeIdAt(i), (uint8_t)StackMsgType::CmdSet,
                      reinterpret_cast<const uint8_t *>(payload), len);
}

void StackRuntime::notifyRingHold_(){
    const RingController::Source src = control.controllers.ring().lastSource();
    if (src == RingController::Source::Button)
    {
        const String tg_msg = F("Р вЂ”Р Р†Р С•Р Р…Р С•Р С” Р Р†Р С”Р В»РЎР‹РЎвЂЎР ВµР Р… Р С—Р С• Р С”Р Р…Р С•Р С—Р С”Р Вµ");
        core.logs.info(F("RING"), F("Ring enabled by button"));
        sendTelegramNotify_(tg_msg);
        return;
    }
    if (src == RingController::Source::Web)
    {
        const String tg_msg = F("Р вЂ”Р Р†Р С•Р Р…Р С•Р С” Р Р†Р С”Р В»РЎР‹РЎвЂЎР ВµР Р… Р С‘Р В· Р Р†Р ВµР В±-Р С‘Р Р…РЎвЂљР ВµРЎР‚РЎвЂћР ВµР в„–РЎРѓР В°");
        core.logs.info(F("RING"), F("Ring enabled from web"));
        sendTelegramNotify_(tg_msg);
        return;
    }
    if (src == RingController::Source::Cli)
    {
        const String tg_msg = F("Р вЂ”Р Р†Р С•Р Р…Р С•Р С” Р Р†Р С”Р В»РЎР‹РЎвЂЎР ВµР Р… Р С‘Р В· CLI");
        core.logs.info(F("RING"), F("Ring enabled from CLI"));
        sendTelegramNotify_(tg_msg);
        return;
    }
    if (src == RingController::Source::Stack)
    {
        const String tg_msg = F("Р вЂ”Р Р†Р С•Р Р…Р С•Р С” Р Р†Р С”Р В»РЎР‹РЎвЂЎР ВµР Р… Р С‘Р В· РЎРѓРЎвЂљР ВµР С”Р В°");
        core.logs.info(F("RING"), F("Ring enabled from stack"));
        sendTelegramNotify_(tg_msg);
        return;
    }
}

void StackRuntime::sendTelegramNotify_(const String &msg){
    const auto users = control.telegram_menu.allowedUsers();
    if (users.empty())
        return;
    for (size_t i = 0; i < users.size; ++i)
    {
        const auto &user = users[i];
        if (!user.enabled || !user.is_notify || user.chat_id == 0)
            continue;
        comms.telegram_bot.sendText(user.chat_id, msg);
    }
}

void StackRuntime::handleStackFrame_(uint32_t node_id, const StackFrame &frame){
    if (!stackMasterActive_())
        return;
    // Always feed shared stack cache with Ack/Err/Cmd* frames from slaves.
    StackCache::onStackFrame_(&_stack_cache, node_id, frame);
    if (frame.type != (uint8_t)StackMsgType::CmdSet)
        return;
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
    if (err)
        return;
    const uint8_t feature = (uint8_t)(doc["feature"] | 0);
    String action = doc["action"] | "";
    action.toLowerCase();
    if (feature == (uint8_t)StackFeature::Security)
    {
        if (action == "alarm")
        {
            JsonObjectConst params = doc["params"];
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
            return;
        }
        if (action == "rfid")
        {
            JsonObjectConst params = doc["params"];
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
            JsonObjectConst params = doc["params"];
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
        return;
    }
    if (feature == (uint8_t)StackFeature::Ring)
    {
        if (action != "button")
            return;
        JsonObjectConst params = doc["params"];
        const bool pressed = params["pressed"].is<bool>() ? params["pressed"].as<bool>()
                                                          : (params["pressed"].as<int>() != 0);
        control.controllers.ring().setHoldRelayWithSource(pressed, RingController::Source::Stack);
        return;
    }
    if (feature == (uint8_t)StackFeature::Septic)
    {
        handleSepticFrame_(node_id, action, doc["params"]);
        return;
    }
    if (feature == (uint8_t)StackFeature::Tanks)
    {
        handleTankFrame_(node_id, action, doc["params"]);
        return;
    }
    if (feature == (uint8_t)StackFeature::Watering)
    {
        handleWateringFrame_(node_id, action, doc["params"]);
        return;
    }
}

void StackRuntime::handleSepticFrame_(uint32_t node_id, const String &action, JsonVariantConst params){
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
}

void StackRuntime::handleTankFrame_(uint32_t node_id, const String &action, JsonVariantConst params){
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
}

void StackRuntime::handleWateringFrame_(uint32_t node_id, const String &action, JsonVariantConst params){
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
}

void StackRuntime::updateSepticNotifyMode_(){
    control.controllers.septic().setNotifyEnabled(stackMasterActive_());
}

void StackRuntime::updateTanksNotifyMode_(){
    control.controllers.tanks().setNotifyEnabled(stackMasterActive_());
}

void StackRuntime::flushPendingSepticDetect_(){
    if (!_pending_septic_detect)
        return;
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return;
    _pending_septic_detect = false;
    sendSepticDetectToMaster_(_pending_septic_id, _pending_septic_name, _pending_septic_alarm);
}

void StackRuntime::flushPendingTankEmpty_(){
    if (!_pending_tank_empty)
        return;
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return;
    _pending_tank_empty = false;
    sendTankEmptyToMaster_(_pending_tank_id, _pending_tank_name);
}

void StackRuntime::flushPendingWateringEvent_(){
    if (!_pending_watering_event)
        return;
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return;
    _pending_watering_event = false;
    sendWateringEventToMaster_(_pending_watering_event_type, _pending_watering_event_cfg,
                               _pending_watering_event_state);
}

void StackRuntime::flushPendingRingButton_(){
    if (!_pending_ring_button)
        return;
    if (!stackSlaveActive_())
        return;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return;
    _pending_ring_button = false;
    sendRingButtonToMaster_(_pending_ring_button_pressed);
}

bool StackRuntime::sendRingButtonToMaster_(bool pressed){
    if (!stackSlaveActive_())
        return false;
    StackNode &node = net.network.stackNode();
    if (!node.connected())
        return false;
    StaticJsonDocument<128> doc;
    doc["cmd_id"] = 0;
    doc["feature"] = (uint8_t)StackFeature::Ring;
    doc["action"] = "button";
    JsonObject params = doc["params"].to<JsonObject>();
    params["pressed"] = pressed;
    char payload[128] = {};
    const size_t len = serializeJson(doc, payload, sizeof(payload));
    if (len == 0)
        return false;
    return node.send((uint8_t)StackMsgType::CmdSet,
                     reinterpret_cast<const uint8_t *>(payload), len);
}

String StackRuntime::stackNodeLabel_(uint32_t node_id) const{
    StackMaster &master = const_cast<StackRuntime *>(this)->net.network.stackMaster();
    const size_t count = master.nodeCount();
    for (size_t i = 0; i < count; ++i)
    {
        if (master.nodeIdAt(i) == node_id)
        {
            String name = master.nodeNameAt(i);
            if (name.length() > 0)
                return name;
        }
    }
    char buf[12] = {};
    snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)node_id);
    return String(buf);
}

String StackRuntime::escapeHtml_(const String &in){
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

int StackRuntime::stackNodeIndex_(uint32_t node_id) const{
    StackMaster &master = const_cast<StackRuntime *>(this)->net.network.stackMaster();
    const size_t count = master.nodeCount();
    for (size_t i = 0; i < count; ++i)
    {
        if (master.nodeIdAt(i) == node_id)
            return (int)i;
    }
    return -1;
}

void StackRuntime::logStackNodeInventory_(uint32_t node_id){
    if (node_id == 0 || !stackMasterActive_())
        return;
    if (!net.network.stackMaster().nodeIsOnline(node_id, kStackNodeStaleMs))
        return;
    StackInventoryLogState *state = inventoryLogState_(node_id, true);
    if (!state)
        return;
    const String node = stackNodeLabel_(node_id);
    auto cacheReady = [](const auto *cache) -> bool {
        // Treat cache as ready only after an actual response (Ack/Err), not after request dispatch.
        return cache && (cache->has_data || cache->last_ok || cache->last_error.length());
    };
    auto cacheHasItemsForSyncLog = [](const auto *cache) -> bool {
        return cache && cache->last_ok && cache->items && cache->item_count > 0;
    };

    if ((state->logged_mask & kInvSockets) == 0)
    {
        const auto *cache = _stack_cache.socketsCache(node_id);
        if (cacheReady(cache))
        {
            if (cacheHasItemsForSyncLog(cache))
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    core.logs.info(F("STACK"), F("Sync slave unit: %s item: sockets id: %u name: %s"),
                                   node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-");
                }
            }
            state->logged_mask |= kInvSockets;
        }
    }

    if ((state->logged_mask & kInvLights) == 0)
    {
        const auto *cache = _stack_cache.lightsCache(node_id);
        if (cacheReady(cache))
        {
            if (cacheHasItemsForSyncLog(cache))
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    core.logs.info(F("STACK"), F("Sync slave unit: %s item: lights id: %u name: %s"),
                                   node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-");
                }
            }
            state->logged_mask |= kInvLights;
        }
    }

    if ((state->logged_mask & kInvMeteo) == 0)
    {
        const auto *cache = _stack_cache.meteoCache(node_id);
        if (cacheReady(cache))
        {
            if (cacheHasItemsForSyncLog(cache))
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    core.logs.info(F("STACK"), F("Sync slave unit: %s item: meteo id: %u name: %s type: %s"),
                                   node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-",
                                   it.type[0] ? it.type : "none");
                }
            }
            state->logged_mask |= kInvMeteo;
        }
    }

    if ((state->logged_mask & kInvThermo) == 0)
    {
        const auto *cache = _stack_cache.thermoCache(node_id);
        if (cacheReady(cache))
        {
            if (cacheHasItemsForSyncLog(cache))
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    core.logs.info(F("STACK"), F("Sync slave unit: %s item: thermo id: %u name: %s mode: %s"),
                                   node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-",
                                   it.mode[0] ? it.mode : "-");
                }
            }
            state->logged_mask |= kInvThermo;
        }
    }

    if ((state->logged_mask & kInvTanks) == 0)
    {
        const auto *cache = _stack_cache.tanksCache(node_id);
        if (cacheReady(cache))
        {
            if (cacheHasItemsForSyncLog(cache))
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    core.logs.info(F("STACK"), F("Sync slave unit: %s item: tanks id: %u name: %s"),
                                   node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-");
                }
            }
            state->logged_mask |= kInvTanks;
        }
    }

    if ((state->logged_mask & kInvSeptic) == 0)
    {
        const auto *cache = _stack_cache.septicCache(node_id);
        if (cacheReady(cache))
        {
            if (cacheHasItemsForSyncLog(cache))
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    core.logs.info(F("STACK"), F("Sync slave unit: %s item: septic id: %u"),
                                   node.c_str(), (unsigned)it.id);
                }
            }
            state->logged_mask |= kInvSeptic;
        }
    }

    if ((state->logged_mask & kInvSecurity) == 0)
    {
        const auto *cache = _stack_cache.securityCache(node_id);
        if (cacheReady(cache))
        {
            if (cacheHasItemsForSyncLog(cache))
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    core.logs.info(F("STACK"),
                                   F("Sync slave unit: %s item: security id: %u name: %s type: %s"),
                                   node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-",
                                   it.type[0] ? it.type : "-");
                }
            }
            state->logged_mask |= kInvSecurity;
        }
    }

    if ((state->logged_mask & kInvWatering) == 0)
    {
        const auto *cache = _stack_cache.wateringCache(node_id);
        if (cacheReady(cache))
        {
            if (cacheHasItemsForSyncLog(cache))
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    core.logs.info(F("STACK"), F("Sync slave unit: %s item: watering id: %u name: %s"),
                                   node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-");
                }
            }
            state->logged_mask |= kInvWatering;
        }
    }

    if ((state->logged_mask & kInvLeak) == 0)
    {
        const auto *cache = _stack_cache.leakCache(node_id);
        if (cacheReady(cache))
        {
            if (cacheHasItemsForSyncLog(cache))
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!it.enabled)
                        continue;
                    core.logs.info(F("STACK"), F("Sync slave unit: %s item: leak id: %u name: %s"),
                                   node.c_str(), (unsigned)it.id, it.name[0] ? it.name : "-");
                }
            }
            state->logged_mask |= kInvLeak;
        }
    }

    if ((state->logged_mask & kInvAvr) == 0)
    {
        const auto *cache = _stack_cache.avrCache(node_id);
        if (cacheReady(cache))
        {
            state->logged_mask |= kInvAvr;
        }
    }

    if (!state->sync_complete_logged && (state->logged_mask & kInvAll) == kInvAll)
    {
        const String unit = stackNodeLabel_(node_id);
        core.logs.info(F("STACK"), F("Sync slave unit complete: %s"), unit.c_str());
        state->sync_complete_logged = true;
    }
}

StackRuntime::StackInventoryLogState *StackRuntime::inventoryLogState_(uint32_t node_id, bool create){
    if (node_id == 0)
        return nullptr;
    for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
    {
        if (_stack_inventory_log[i].node_id == node_id)
            return &_stack_inventory_log[i];
    }
    if (!create)
        return nullptr;
    for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
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

void StackRuntime::clearInventoryLogState_(uint32_t node_id){
    if (node_id == 0)
        return;
    for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
    {
        if (_stack_inventory_log[i].node_id != node_id)
            continue;
        _stack_inventory_log[i].node_id = 0;
        _stack_inventory_log[i].logged_mask = 0;
        _stack_inventory_log[i].sync_complete_logged = false;
        return;
    }
}

