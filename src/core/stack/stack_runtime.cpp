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

#include "core/network/stack/stack_features.hpp"

#ifndef STACK_BOOTSTRAP_DEBUG_LOGS
#define STACK_BOOTSTRAP_DEBUG_LOGS 0
#endif
#ifndef STACK_BOOTSTRAP_EVENT_LOGS
#define STACK_BOOTSTRAP_EVENT_LOGS 0
#endif
#define STACK_BOOT_TAG F("STACKBOOT")

#if STACK_BOOTSTRAP_DEBUG_LOGS
#define STACK_BOOTSTRAP_DBG(app_, fmt_, ...) (app_).core.logs.debug(STACK_BOOT_TAG, F(fmt_), ##__VA_ARGS__)
#else
#define STACK_BOOTSTRAP_DBG(app_, fmt_, ...) do {} while (0)
#endif

#if STACK_BOOTSTRAP_EVENT_LOGS
#define STACK_BOOTSTRAP_EVT_INFO(app_, fmt_, ...) (app_).core.logs.info(STACK_BOOT_TAG, F(fmt_), ##__VA_ARGS__)
#define STACK_BOOTSTRAP_EVT_WARN(app_, fmt_, ...) (app_).core.logs.warn(STACK_BOOT_TAG, F(fmt_), ##__VA_ARGS__)
#else
#define STACK_BOOTSTRAP_EVT_INFO(app_, fmt_, ...) do {} while (0)
#define STACK_BOOTSTRAP_EVT_WARN(app_, fmt_, ...) do {} while (0)
#endif

namespace
{
static constexpr uint16_t kStackBootstrapCoreFeatureMask =
    (uint16_t)((1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) |
               (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 9) |
               (1u << 10) | (1u << 13) | (1u << 14));
static constexpr uint8_t kStackRegularPollFeatures[] = {
    0,  // plc
    1,  // rtc
    2,  // sockets
    3,  // lights
    4,  // security
    6,  // thermo
    7,  // septic
    8,  // tanks
    9,  // meteo
    10, // watering
    11, // avr
    12  // leak
};
}

StackRuntime::StackRuntime(CoreContext &core, HardwareContext &hw, CommsContext &comms,
                           ControlContext &control, UiContext &ui, NetworkContext &net,
                           ConfigContext &cfg)
    : core(core),
      hw(hw),
      comms(comms),
      control(control),
      ui(ui),
      net(net),
      cfg(cfg),
      _stack_cache()
{
    _stack_cache.setLogger(&core.logs);
    _stack_cache.setConfigsManager(&cfg.configs_manager);
    _stack_cache.setStackMaster(&net.network.stackMaster());
}

StackCache &StackRuntime::stackCache()
{
    return _stack_cache;
}

void StackRuntime::bindCallbacks()
{
    control.controllers.thermo().setRemoteMeteoProvider(&StackRuntime::onRemoteMeteo_, this);
    control.controllers.meteo().setRemoteMeteoProvider(&StackRuntime::onRemoteMeteoProxy_, this);
    control.controllers.meteo().setRemoteNodeNameProvider(&StackRuntime::onRemoteNodeName_, this);
    control.controllers.meteo().setRemoteSensorNameProvider(&StackRuntime::onRemoteSensorName_, this);
    control.controllers.meteo().setRemoteSensorTypeProvider(&StackRuntime::onRemoteSensorType_, this);
    control.controllers.meteo().setAlarmHandler(&StackRuntime::onMeteoAlarm_, this);
    control.controllers.security().setArmStateHandler(&StackRuntime::onSecurityArmState_, this);
    control.controllers.security().setPreArmCheckHandler(&StackRuntime::onSecurityPreArmCheck_, this);
    control.controllers.security().setAlarmStateHandler(&StackRuntime::onSecurityAlarmState_, this);
    control.controllers.security().setClearDetectHandler(&StackRuntime::onSecurityClearDetect_, this);
    control.controllers.security().setDetectHandler(&StackRuntime::onSecurityDetect_, this);
    control.controllers.security().setRfidUidHandler(&StackRuntime::onSecurityRfidUid_, this);
    control.controllers.security().setIButtonSerialHandler(&StackRuntime::onSecurityIButtonSerial_, this);

    control.controllers.septic().setDetectHandler(&StackRuntime::onSepticDetect_, this);
    control.controllers.tanks().setDetectHandler(&StackRuntime::onTankEmpty_, this);
    control.controllers.ring().setHoldHandler(&StackRuntime::onRingHold_, this);
    control.controllers.watering().setEventHandler(&StackRuntime::onWateringEvent_, this);

    hw.display.setSlotProvider(&StackRuntime::onDisplaySlot_, this);
    net.network.stackMaster().setEventHandler(&StackRuntime::onStackNodeEvent_, this);
    net.network.stackMaster().setFrameHandlerTertiary(&StackRuntime::onStackFrame_, this);

    net.stack_slave.setConfigsManager(cfg.configs_manager);
#if defined(ESP32)
    net.stack_slave.attach(net.network.stackNode());
#endif
}

void StackRuntime::init()
{
    net.stack_slave.initAllocations();
    _stack_cache.initAllocations();
    _stack_cache.logAllocations();
}

void StackRuntime::applyLoadedConfig()
{
    updateSecurityNotifyMode_();
    updateSepticNotifyMode_();
    updateTanksNotifyMode_();
    updateDisplayLayout_();
    net.network.setStackDeviceName(hw.plc.deviceName());
}

void StackRuntime::loopBegin()
{
    updateDisplayLayout_();
    updateTankAlarms_();
    updateSepticAlarms_();
    updateSecurityAlarms_();
    updateMeteoAlarms_();
    pollSecurityPrearmWarmup_();
    pollStackCaches_();
    pollSecurityStatusFromMaster_();
}

void StackRuntime::loopAfterNetwork()
{
    updateStackMasterMode_();
    net.stack_slave.loop();
}

void StackRuntime::flushPending()
{
    flushPendingSecurityDetect_();
    flushPendingSepticDetect_();
    flushPendingTankEmpty_();
    flushPendingWateringEvent_();
    flushPendingRfid_();
    flushPendingIButton_();
    flushPendingRingButton_();
}

void StackRuntime::setTaskPhase(TaskPhase phase)
{
    _task_phase = phase;
}

void StackRuntime::taskPre()
{
    if (_task_phase != TaskPhase::PreNetwork)
        return;
    loopBegin();
}

void StackRuntime::taskPost()
{
    if (_task_phase != TaskPhase::PostNetwork)
        return;
    loopAfterNetwork();
}

void StackRuntime::taskFlush()
{
    if (_task_phase != TaskPhase::PostNetwork)
        return;
    flushPending();
}

bool StackRuntime::stackMasterActive_() const{
    return cfg.configs_manager.stackRole() == ConfigsManagerIface::StackRole::Master ||
           net.network.stackFallbackActive();
}

bool StackRuntime::stackSlaveActive_() const{
    return cfg.configs_manager.stackRole() == ConfigsManagerIface::StackRole::Slave &&
           !net.network.stackFallbackActive();
}

void StackRuntime::updateMasterLed_(bool master_active){
    const uint8_t pin = ActiveBoardProfile::MASTER_LED_PIN;
    if (pin == 0xFF)
        return;
    if (!_master_led_initialized)
    {
        hw.io.pinMode(pin, PortIO::PortMode::Output);
        _master_led_initialized = true;
    }
    if (_master_led_state == master_active)
        return;
    hw.io.write(pin, master_active);
    _master_led_state = master_active;
}

void StackRuntime::updateStackMasterMode_(){
    const bool active = net.network.stackMasterActive();
    if (_stack_master_effective != active)
    {
        _stack_master_effective = active;
        _stack_cache.setMasterOverride(active);
        updateSecurityNotifyMode_();
        updateSepticNotifyMode_();
        updateTanksNotifyMode_();
        if (active)
            core.logs.warn(F("STACK"), F("Role switch: master"));
        else
            core.logs.warn(F("STACK"), F("Role switch: slave"));
    }
    updateMasterLed_(active);
}

bool StackRuntime::requestStackPollFeature_(uint32_t node_id, uint8_t feature){
    switch (feature)
    {
    case 0:  return _stack_cache.requestPlcStatus(node_id);
    case 1:  return _stack_cache.requestRtcStatus(node_id);
    case 2:  return _stack_cache.requestSockets(node_id);
    case 3:  return _stack_cache.requestLights(node_id);
    case 4:  return _stack_cache.requestSecurity(node_id);
    case 5:  return _stack_cache.requestSecurityPrearm(node_id);
    case 6:  return _stack_cache.requestThermo(node_id);
    case 7:  return _stack_cache.requestSeptic(node_id);
    case 8:  return _stack_cache.requestTanks(node_id);
    case 9:  return _stack_cache.requestMeteo(node_id);
    case 10: return _stack_cache.requestWatering(node_id);
    case 11: return _stack_cache.requestAvr(node_id);
    case 12: return _stack_cache.requestLeak(node_id);
    case 13: return _stack_cache.requestPorts(node_id);
    case 14: return _stack_cache.requestTempSensors(node_id);
    default: return false;
    }
}
bool StackRuntime::shouldPollStackFeature_(uint32_t node_id, uint8_t feature, uint32_t now) const
{
    switch (feature)
    {
    case 0:
    {
        const auto *c = _stack_cache.statusCache(node_id);
        return !c || !c->has_plc || !c->plc_updated_ms ||
               (uint32_t)(now - c->plc_updated_ms) >= kStackRegularRefreshMs;
    }
    case 1:
    {
        const auto *c = _stack_cache.statusCache(node_id);
        return !c || !c->has_rtc || !c->rtc_updated_ms ||
               (uint32_t)(now - c->rtc_updated_ms) >= kStackRegularRefreshMs;
    }
    case 2:
    {
        const auto *c = _stack_cache.socketsCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    case 3:
    {
        const auto *c = _stack_cache.lightsCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    case 4:
    {
        const auto *c = _stack_cache.securityCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    case 6:
    {
        const auto *c = _stack_cache.thermoCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    case 7:
    {
        const auto *c = _stack_cache.septicCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    case 8:
    {
        const auto *c = _stack_cache.tanksCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    case 9:
    {
        const auto *c = _stack_cache.meteoCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    case 10:
    {
        const auto *c = _stack_cache.wateringCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    case 11:
    {
        const auto *c = _stack_cache.avrCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    case 12:
    {
        const auto *c = _stack_cache.leakCache(node_id);
        return !c || !c->has_data || !c->updated_ms ||
               (uint32_t)(now - c->updated_ms) >= kStackRegularRefreshMs;
    }
    default:
        return false;
    }
}

bool StackRuntime::bootstrapSyncCompleted_(uint32_t node_id) const{
    if (node_id == 0)
        return true;
    StackRuntime *self = const_cast<StackRuntime *>(this);
    StackInventoryLogState *st = self->inventoryLogState_(node_id, false);
    return st && st->sync_complete_logged;
}

void StackRuntime::enqueueStackBootstrapSync_(uint32_t node_id){
    if (node_id == 0)
        return;
    if (_stack_bootstrap_node_id == node_id)
    {
        _stack_bootstrap_started_ms = millis();
        _stack_bootstrap_feature_index = 0;
        _stack_bootstrap_pass = 0;
        _stack_bootstrap_feature_sent_mask = 0;
        _last_stack_poll_ms = 0;
        STACK_BOOTSTRAP_DBG((*this), "Bootstrap reset active: id: 0x%08lX", (unsigned long)node_id);
        return;
    }
    for (uint8_t i = 0; i < _stack_bootstrap_queue_count; ++i)
    {
        if (_stack_bootstrap_queue[i] != node_id)
            continue;
        STACK_BOOTSTRAP_DBG((*this), "Bootstrap queue skip duplicate: id: 0x%08lX", (unsigned long)node_id);
        return;
    }
    if (_stack_bootstrap_queue_count < StackMaster::MAX_SESSIONS)
    {
        _stack_bootstrap_queue[_stack_bootstrap_queue_count++] = node_id;
    }
    else
    {
        for (uint8_t i = 1; i < _stack_bootstrap_queue_count; ++i)
            _stack_bootstrap_queue[i - 1] = _stack_bootstrap_queue[i];
        _stack_bootstrap_queue[_stack_bootstrap_queue_count - 1] = node_id;
    }
    STACK_BOOTSTRAP_DBG((*this), "Bootstrap queued: id: 0x%08lX q:%u",
                        (unsigned long)node_id, (unsigned)_stack_bootstrap_queue_count);
    tryStartNextStackBootstrapSync_();
}

void StackRuntime::removeStackBootstrapSync_(uint32_t node_id){
    if (node_id == 0)
        return;
    if (_stack_bootstrap_node_id == node_id)
    {
        stopStackBootstrapSync_(false);
        return;
    }
    for (uint8_t i = 0; i < _stack_bootstrap_queue_count; ++i)
    {
        if (_stack_bootstrap_queue[i] != node_id)
            continue;
        for (uint8_t j = (uint8_t)(i + 1); j < _stack_bootstrap_queue_count; ++j)
            _stack_bootstrap_queue[j - 1] = _stack_bootstrap_queue[j];
        _stack_bootstrap_queue[_stack_bootstrap_queue_count - 1] = 0;
        --_stack_bootstrap_queue_count;
        STACK_BOOTSTRAP_DBG((*this), "Bootstrap queue remove: id: 0x%08lX q:%u",
                            (unsigned long)node_id, (unsigned)_stack_bootstrap_queue_count);
        return;
    }
}

void StackRuntime::tryStartNextStackBootstrapSync_(){
    if (_stack_bootstrap_node_id != 0)
        return;
    if (_stack_bootstrap_queue_count == 0)
        return;
    const uint32_t node_id = _stack_bootstrap_queue[0];
    for (uint8_t i = 1; i < _stack_bootstrap_queue_count; ++i)
        _stack_bootstrap_queue[i - 1] = _stack_bootstrap_queue[i];
    _stack_bootstrap_queue[_stack_bootstrap_queue_count - 1] = 0;
    --_stack_bootstrap_queue_count;
    startStackBootstrapSync_(node_id);
}

void StackRuntime::startStackBootstrapSync_(uint32_t node_id){
    if (node_id == 0)
        return;
    _stack_bootstrap_node_id = node_id;
    _stack_bootstrap_started_ms = millis();
    _stack_bootstrap_feature_index = 0;
    _stack_bootstrap_pass = 0;
    _stack_bootstrap_feature_sent_mask = 0;
    _last_stack_poll_ms = 0; // allow first bootstrap request on the next loop tick
    STACK_BOOTSTRAP_EVT_INFO((*this), "Bootstrap sync start: %s", stackNodeLabel_(node_id).c_str());
    STACK_BOOTSTRAP_DBG((*this), "Bootstrap active: id: 0x%08lX q:%u",
                        (unsigned long)node_id, (unsigned)_stack_bootstrap_queue_count);
}

void StackRuntime::stopStackBootstrapSync_(bool timeout){
    const uint32_t prev_node_id = _stack_bootstrap_node_id;
    if (_stack_bootstrap_node_id != 0)
    {
        const String unit = stackNodeLabel_(_stack_bootstrap_node_id);
        if (timeout)
            STACK_BOOTSTRAP_EVT_WARN((*this), "Bootstrap sync timeout: %s", unit.c_str());
        else
            STACK_BOOTSTRAP_EVT_INFO((*this), "Bootstrap sync done: %s", unit.c_str());
    }
    _stack_bootstrap_node_id = 0;
    _stack_bootstrap_started_ms = 0;
    _stack_bootstrap_feature_index = 0;
    _stack_bootstrap_pass = 0;
    _stack_bootstrap_feature_sent_mask = 0;
    if (prev_node_id != 0)
        tryStartNextStackBootstrapSync_();
}

void StackRuntime::pollStackCaches_(){
    if (!stackMasterActive_())
        return;
    logLocalInventory_();
    StackMaster &master = net.network.stackMaster();
    const size_t count = master.nodeCount();
    if (count == 0)
    {
        if (_stack_bootstrap_node_id != 0)
            stopStackBootstrapSync_(false);
        return;
    }

    const uint32_t now = millis();
    const bool bootstrap_active = (_stack_bootstrap_node_id != 0);
    const uint32_t poll_ms = bootstrap_active ? kStackBootstrapPollMs : kStackPollMs;
    if ((uint32_t)(now - _last_stack_poll_ms) < poll_ms)
        return;
    _last_stack_poll_ms = now;

    uint32_t node_id = 0;
    uint8_t feature = 0;
    bool use_bootstrap = false;

    if (bootstrap_active)
    {
        const uint32_t bs_node = _stack_bootstrap_node_id;
        if (!master.nodeIsOnline(bs_node, kStackNodeStaleMs))
        {
            stopStackBootstrapSync_(false);
        }
        else if ((uint32_t)(now - _stack_bootstrap_started_ms) >= kStackBootstrapTimeoutMs)
        {
            stopStackBootstrapSync_(true);
        }
        else
        {
            logStackNodeInventory_(bs_node);
            const bool core_requested =
                (_stack_bootstrap_feature_sent_mask & kStackBootstrapCoreFeatureMask) == kStackBootstrapCoreFeatureMask;
            if (bootstrapSyncCompleted_(bs_node) && core_requested)
            {
                stopStackBootstrapSync_(false);
            }
            else
            {
                node_id = bs_node;
                const uint8_t node_qdepth = master.queueDepth(node_id);
                if (node_qdepth > 0)
                {
                    STACK_BOOTSTRAP_DBG((*this), "Bootstrap wait queue: id: 0x%08lX depth:%u",
                                        (unsigned long)node_id, (unsigned)node_qdepth);
                    return;
                }
                feature = (uint8_t)(_stack_bootstrap_feature_index % kStackPollFeatureCount);
                _stack_bootstrap_feature_index =
                    (uint8_t)((_stack_bootstrap_feature_index + 1) % kStackPollFeatureCount);
                STACK_BOOTSTRAP_DBG((*this), "Bootstrap poll: id: 0x%08lX pass:%u feature:%u",
                                    (unsigned long)bs_node,
                                    (unsigned)_stack_bootstrap_pass,
                                    (unsigned)feature);
                if (_stack_bootstrap_feature_index == 0)
                {
                    ++_stack_bootstrap_pass;
                    STACK_BOOTSTRAP_DBG((*this), "Bootstrap pass complete: id: 0x%08lX pass:%u",
                                        (unsigned long)bs_node, (unsigned)_stack_bootstrap_pass);
                    if (_stack_bootstrap_pass >= kStackBootstrapPasses && bootstrapSyncCompleted_(bs_node))
                    {
                        stopStackBootstrapSync_(false);
                    }
                }
                if (_stack_bootstrap_node_id != 0)
                    use_bootstrap = true;
            }
        }
    }

    if (!use_bootstrap)
    {
        const auto tx_stats = master.txStats();
        if (tx_stats.depth >= kStackPollBackpressureDepth)
            return;
        if (_stack_poll_index >= count)
            _stack_poll_index = 0;
        node_id = master.nodeIdAt(_stack_poll_index++);
        if (node_id == 0)
            return;
        if (!master.nodeIsOnline(node_id, kStackNodeStaleMs))
            return;
        if (master.queueDepth(node_id) > 0)
            return;
        logStackNodeInventory_(node_id);
        constexpr uint8_t kRegularFeatureCount =
            (uint8_t)(sizeof(kStackRegularPollFeatures) / sizeof(kStackRegularPollFeatures[0]));
        bool found = false;
        for (uint8_t pass = 0; pass < kRegularFeatureCount; ++pass)
        {
            const uint8_t idx = (uint8_t)((_stack_poll_feature_index + pass) % kRegularFeatureCount);
            const uint8_t candidate = kStackRegularPollFeatures[idx];
            if (!shouldPollStackFeature_(node_id, candidate, now))
                continue;
            feature = candidate;
            _stack_poll_feature_index = (uint8_t)((idx + 1) % kRegularFeatureCount);
            found = true;
            break;
        }
        if (!found)
            return;
    }

    const bool sent = requestStackPollFeature_(node_id, feature);
    if (use_bootstrap && sent && feature < 16)
        _stack_bootstrap_feature_sent_mask |= (uint16_t)(1u << feature);
    if (use_bootstrap && !sent)
    {
        STACK_BOOTSTRAP_DBG((*this), "Bootstrap req skipped: id: 0x%08lX feature:%u",
                            (unsigned long)node_id, (unsigned)feature);
    }
}

void StackRuntime::logLocalInventory_(){
    if (_local_inventory_logged)
        return;
    const String node = hw.plc.deviceName().length() ? hw.plc.deviceName() : String("master");
    auto &sockets = control.controllers.sockets();
    auto sockets_guard = sockets.lockGuard();
    size_t enabled = 0;
    for (size_t i = 0; i < SocketController::kSocketCount; ++i)
    {
        const auto *cfg = sockets.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        ++enabled;
    }
    core.logs.info(F("STACK"), F("Sync master unit: %s item: sockets enabled: %u"), node.c_str(), (unsigned)enabled);
    for (size_t i = 0; i < SocketController::kSocketCount; ++i)
    {
        const auto *cfg = sockets.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        core.logs.info(F("STACK"), F("Sync master unit: %s item: sockets id: %u name: %s"),
                       node.c_str(), (unsigned)cfg->id, cfg->name.length() ? cfg->name.c_str() : "-");
    }

    enabled = 0;
    for (size_t i = 0; i < SocketController::kLightCount; ++i)
    {
        const auto *cfg = sockets.lightConfigByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        ++enabled;
    }
    core.logs.info(F("STACK"), F("Sync master unit: %s item: lights enabled: %u"), node.c_str(), (unsigned)enabled);
    for (size_t i = 0; i < SocketController::kLightCount; ++i)
    {
        const auto *cfg = sockets.lightConfigByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        core.logs.info(F("STACK"), F("Sync master unit: %s item: lights id: %u name: %s"),
                       node.c_str(), (unsigned)cfg->id, cfg->name.length() ? cfg->name.c_str() : "-");
    }

    auto &meteo = control.controllers.meteo();
    auto meteo_guard = meteo.lockGuard();
    enabled = 0;
    for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
    {
        const auto *cfg = meteo.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        ++enabled;
    }
    core.logs.info(F("STACK"), F("Sync master unit: %s item: meteo enabled: %u"), node.c_str(), (unsigned)enabled);
    for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
    {
        const auto *cfg = meteo.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        core.logs.info(F("STACK"), F("Sync master unit: %s item: meteo id: %u name: %s type: %s"),
                       node.c_str(), (unsigned)cfg->id, cfg->name.length() ? cfg->name.c_str() : "-",
                       MeteoController::typeName(cfg->type));
    }

    auto &thermo = control.controllers.thermo();
    auto thermo_guard = thermo.lockGuard();
    enabled = 0;
    for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
    {
        const auto *cfg = thermo.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        ++enabled;
    }
    core.logs.info(F("STACK"), F("Sync master unit: %s item: thermo enabled: %u"), node.c_str(), (unsigned)enabled);
    for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
    {
        const auto *cfg = thermo.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        core.logs.info(F("STACK"), F("Sync master unit: %s item: thermo id: %u name: %s mode: %s"),
                       node.c_str(), (unsigned)cfg->id, cfg->name.length() ? cfg->name.c_str() : "-",
                       ThermoController::modeName(cfg->mode));
    }

    auto &tanks = control.controllers.tanks();
    auto tanks_guard = tanks.lockGuard();
    enabled = 0;
    for (size_t i = 0; i < TankController::kTankCount; ++i)
    {
        const auto *cfg = tanks.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        ++enabled;
    }
    core.logs.info(F("STACK"), F("Sync master unit: %s item: tanks enabled: %u"), node.c_str(), (unsigned)enabled);
    for (size_t i = 0; i < TankController::kTankCount; ++i)
    {
        const auto *cfg = tanks.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        core.logs.info(F("STACK"), F("Sync master unit: %s item: tanks id: %u name: %s"),
                       node.c_str(), (unsigned)cfg->id, cfg->name.length() ? cfg->name.c_str() : "-");
    }

    auto &septic = control.controllers.septic();
    auto septic_guard = septic.lockGuard();
    enabled = 0;
    for (size_t i = 0; i < SepticController::kSepticCount; ++i)
    {
        const auto *cfg = septic.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        ++enabled;
    }
    core.logs.info(F("STACK"), F("Sync master unit: %s item: septic enabled: %u"), node.c_str(), (unsigned)enabled);
    for (size_t i = 0; i < SepticController::kSepticCount; ++i)
    {
        const auto *cfg = septic.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        core.logs.info(F("STACK"), F("Sync master unit: %s item: septic id: %u"), node.c_str(), (unsigned)cfg->id);
    }

    auto &security = control.controllers.security();
    auto security_guard = security.lockGuard();
    enabled = 0;
    for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
    {
        const auto *cfg = security.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        ++enabled;
    }
    core.logs.info(F("STACK"), F("Sync master unit: %s item: security enabled: %u"), node.c_str(), (unsigned)enabled);
    for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
    {
        const auto *cfg = security.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        const char *type = (cfg->type == SecurityController::SensorType::Reed) ? "reed" : "pir";
        core.logs.info(F("STACK"), F("Sync master unit: %s item: security id: %u name: %s type: %s"),
                       node.c_str(), (unsigned)cfg->id, cfg->name.length() ? cfg->name.c_str() : "-", type);
    }

    auto &watering = control.controllers.watering();
    auto watering_guard = watering.lockGuard();
    enabled = 0;
    for (size_t i = 0; i < WateringController::kRuleCount; ++i)
    {
        const auto *cfg = watering.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        ++enabled;
    }
    core.logs.info(F("STACK"), F("Sync master unit: %s item: watering enabled: %u"), node.c_str(), (unsigned)enabled);
    for (size_t i = 0; i < WateringController::kRuleCount; ++i)
    {
        const auto *cfg = watering.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        core.logs.info(F("STACK"), F("Sync master unit: %s item: watering id: %u name: %s"),
                       node.c_str(), (unsigned)cfg->id, cfg->name.length() ? cfg->name.c_str() : "-");
    }

    auto &leak = control.controllers.leak();
    auto leak_guard = leak.lockGuard();
    enabled = 0;
    for (size_t i = 0; i < LeakController::kZoneCount; ++i)
    {
        const auto *cfg = leak.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        ++enabled;
    }
    core.logs.info(F("STACK"), F("Sync master unit: %s item: leak enabled: %u"), node.c_str(), (unsigned)enabled);
    for (size_t i = 0; i < LeakController::kZoneCount; ++i)
    {
        const auto *cfg = leak.configByIndex(i);
        if (!cfg || !cfg->enabled)
            continue;
        core.logs.info(F("STACK"), F("Sync master unit: %s item: leak id: %u name: %s"),
                       node.c_str(), (unsigned)cfg->id, cfg->name.length() ? cfg->name.c_str() : "-");
    }

    {
        auto &avr = control.controllers.avr();
        auto avr_guard = avr.lockGuard();
        core.logs.info(F("STACK"), F("Sync master unit: %s item: avr enabled: %s"), node.c_str(),
                       avr.controllerEnabled() ? "true" : "false");
    }
    _local_inventory_logged = true;
}

bool StackRuntime::onRemoteMeteo_(void *ctx, uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp){
    if (!ctx || node_id == 0 || sensor_id == 0)
        return false;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    if (self->stackMasterActive_())
    {
        auto &_stack_cache = self->_stack_cache;
        const auto *cache = _stack_cache.meteoCache(node_id);
        if (!cache || !cache->has_data)
        {
            _stack_cache.requestMeteo(node_id);
            return false;
        }
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (it.id != sensor_id)
                continue;
            temp_c = it.temp_c;
            has_temp = it.has_temp;
            return true;
        }
        return false;
    }
    if (self->net.stack_slave.remoteMeteoTemp(node_id, sensor_id, temp_c, has_temp))
        return true;
    self->net.stack_slave.requestRemoteMeteoAll();
    return false;
}

bool StackRuntime::onRemoteMeteoProxy_(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                float &temp_c, bool &has_temp, float &hum, bool &has_hum, bool &ok){
    if (!ctx || node_id == 0 || sensor_id == 0)
        return false;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    if (self->stackMasterActive_())
    {
        const auto *cache = self->_stack_cache.meteoCache(node_id);
        if (!cache || !cache->has_data)
        {
            self->_stack_cache.requestMeteo(node_id);
            return false;
        }
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (it.id != sensor_id)
                continue;
            temp_c = it.temp_c;
            hum = it.hum;
            has_temp = it.has_temp;
            has_hum = it.has_hum;
            ok = it.ok;
            return true;
        }
        return false;
    }
    if (self->net.stack_slave.remoteMeteoRead(node_id, sensor_id, temp_c, has_temp, hum, has_hum, ok))
        return true;
    self->net.stack_slave.requestRemoteMeteoAll();
    return false;
}

bool StackRuntime::onRemoteNodeName_(void *ctx, uint32_t node_id, String &out){
    if (!ctx || node_id == 0)
        return false;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    out = self->stackNodeLabel_(node_id);
    return out.length() > 0;
}

bool StackRuntime::onRemoteSensorName_(void *ctx, uint32_t node_id, uint8_t sensor_id, String &out){
    if (!ctx || node_id == 0 || sensor_id == 0)
        return false;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    if (self->stackMasterActive_())
    {
        const auto *cache = self->_stack_cache.meteoCache(node_id);
        if (!cache || !cache->has_data)
        {
            self->_stack_cache.requestMeteo(node_id);
            return false;
        }
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (it.id != sensor_id)
                continue;
            if (it.name[0])
            {
                out = it.name;
                return true;
            }
            return false;
        }
        return false;
    }
    const auto *cache = self->net.stack_slave.remoteMeteoCache(node_id);
    if (!cache || !cache->has_data || !cache->items)
    {
        self->net.stack_slave.requestRemoteMeteoAll();
        return false;
    }
    for (size_t i = 0; i < cache->item_count; ++i)
    {
        const auto &it = cache->items[i];
        if (it.id != sensor_id)
            continue;
        if (it.name[0])
        {
            out = it.name;
            return true;
        }
        return false;
    }
    return false;
}

bool StackRuntime::onRemoteSensorType_(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                MeteoController::SensorType &out){
    out = MeteoController::SensorType::None;
    if (!ctx || node_id == 0 || sensor_id == 0)
        return false;
    StackRuntime *self = static_cast<StackRuntime *>(ctx);
    auto parseType = [](const char *type) -> MeteoController::SensorType {
        if (!type || !type[0])
            return MeteoController::SensorType::None;
        String t(type);
        t.toLowerCase();
        if (t == "ds18b20")
            return MeteoController::SensorType::Ds18b20;
        if (t == "dht22")
            return MeteoController::SensorType::Dht22;
        return MeteoController::SensorType::None;
    };
    if (self->stackMasterActive_())
    {
        const auto *cache = self->_stack_cache.meteoCache(node_id);
        if (!cache || !cache->has_data)
        {
            self->_stack_cache.requestMeteo(node_id);
            return false;
        }
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (it.id != sensor_id)
                continue;
            out = parseType(it.type);
            return out != MeteoController::SensorType::None;
        }
        return false;
    }
    const auto *cache = self->net.stack_slave.remoteMeteoCache(node_id);
    if (!cache || !cache->has_data || !cache->items)
    {
        self->net.stack_slave.requestRemoteMeteoAll();
        return false;
    }
    for (size_t i = 0; i < cache->item_count; ++i)
    {
        const auto &it = cache->items[i];
        if (it.id != sensor_id)
            continue;
        out = parseType(it.type);
        return out != MeteoController::SensorType::None;
    }
    return false;
}

