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

#include <esp_system.h>

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

const char *resetReasonText_(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_UNKNOWN: return "unknown";
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "other";
    }
}
}

AppRuntime::AppRuntime(CoreContext &core, HardwareContext &hw, CommsContext &comms,
                           ControlContext &control, UiContext &ui, NetworkContext &net,
                           ConfigContext &cfg)
    : core(core),
      hw(hw),
      comms(comms),
      control(control),
      ui(ui),
      net(net),
      cfg(cfg)
{
}

void AppRuntime::bindCallbacks()
{
    net.network.stackRoute().setJsonRouteHandler(&AppRuntime::onStackRoute_, this);
    net.network.setStackNodeEventHandler(&AppRuntime::onStackNodeEvent_, this);
    control.controllers.thermo().setRemoteMeteoProvider(&AppRuntime::onRemoteMeteo_, this);
    control.controllers.meteo().setRemoteMeteoProvider(&AppRuntime::onRemoteMeteoProxy_, this);
    control.controllers.meteo().setRemoteNodeNameProvider(&AppRuntime::onRemoteNodeName_, this);
    control.controllers.meteo().setRemoteSensorNameProvider(&AppRuntime::onRemoteSensorName_, this);
    control.controllers.meteo().setRemoteSensorTypeProvider(&AppRuntime::onRemoteSensorType_, this);
    control.controllers.meteo().setAlarmHandler(&AppRuntime::onMeteoAlarm_, this);
    control.controllers.security().setArmStateHandler(&AppRuntime::onSecurityArmState_, this);
    control.controllers.security().setPreArmCheckHandler(&AppRuntime::onSecurityPreArmCheck_, this);
    control.controllers.security().setAlarmStateHandler(&AppRuntime::onSecurityAlarmState_, this);
    control.controllers.security().setClearDetectHandler(&AppRuntime::onSecurityClearDetect_, this);
    control.controllers.security().setDetectHandler(&AppRuntime::onSecurityDetect_, this);
    control.controllers.security().setRfidUidHandler(&AppRuntime::onSecurityRfidUid_, this);
    control.controllers.security().setIButtonSerialHandler(&AppRuntime::onSecurityIButtonSerial_, this);

    control.controllers.septic().setDetectHandler(&AppRuntime::onSepticDetect_, this);
    control.controllers.tanks().setDetectHandler(&AppRuntime::onTankEmpty_, this);
    control.controllers.ring().setHoldHandler(&AppRuntime::onRingHold_, this);
    control.controllers.watering().setEventHandler(&AppRuntime::onWateringEvent_, this);

    hw.display.setSlotProvider(&AppRuntime::onDisplaySlot_, this);
}

void AppRuntime::init()
{
    const esp_reset_reason_t reset_reason = esp_reset_reason();
    core.logs.info(F("APP"), F("Reset reason: %s code: %d"), resetReasonText_(reset_reason), (int)reset_reason);
}

void AppRuntime::applyLoadedConfig()
{
    updateSecurityNotifyMode_();
    updateSepticNotifyMode_();
    updateTanksNotifyMode_();
    updateDisplayLayout_();
}

void AppRuntime::loopBegin()
{
    updateDisplayLayout_();
    updateTankAlarms_();
    updateSepticAlarms_();
    updateSecurityAlarms_();
    updateMeteoAlarms_();
}

void AppRuntime::loopAfterNetwork()
{
    updateStackMasterMode_();
}

void AppRuntime::flushPending()
{
    flushPendingSecurityDetect_();
    flushPendingSepticDetect_();
    flushPendingTankEmpty_();
    flushPendingWateringEvent_();
    flushPendingStackSocketsResponse_();
    flushPendingStackSocketsPage_();
    flushPendingStackLightsResponse_();
    flushPendingStackLightsPage_();
    flushPendingStackMeteoPage_();
    flushPendingStackThermoPage_();
    flushPendingStackTanksPage_();
    flushPendingRfid_();
    flushPendingIButton_();
    flushPendingRingButton_();
}

void AppRuntime::setTaskPhase(TaskPhase phase)
{
    _task_phase = phase;
}

void AppRuntime::taskPre()
{
    if (_task_phase != TaskPhase::PreNetwork)
        return;
    loopBegin();
}

void AppRuntime::taskPost()
{
    if (_task_phase != TaskPhase::PostNetwork)
        return;
    loopAfterNetwork();
}

void AppRuntime::taskFlush()
{
    if (_task_phase != TaskPhase::PostNetwork)
        return;
    if (_pending_display_stack_sockets_page && !_pending_stack_sockets_page)
    {
        const uint32_t node_id = _pending_display_stack_sockets_node_id;
        const uint16_t offset = _pending_display_stack_sockets_offset;
        _pending_display_stack_sockets_page = false;
        if (node_id != 0 && net.network.prepareStackSocketsPageRequest(node_id, millis(), offset, 4000u))
        {
            _pending_stack_sockets_page = true;
            _pending_stack_sockets_node_id = node_id;
            _pending_stack_sockets_offset = offset;
            _pending_stack_sockets_limit = 8;
            _pending_stack_sockets_log = false;
        }
    }
    if (_pending_display_stack_lights_page && !_pending_stack_lights_page)
    {
        const uint32_t node_id = _pending_display_stack_lights_node_id;
        const uint16_t offset = _pending_display_stack_lights_offset;
        _pending_display_stack_lights_page = false;
        if (node_id != 0 && net.network.prepareStackLightsPageRequest(node_id, millis(), offset, 4000u))
        {
            _pending_stack_lights_page = true;
            _pending_stack_lights_node_id = node_id;
            _pending_stack_lights_offset = offset;
            _pending_stack_lights_limit = 8;
            _pending_stack_lights_log = false;
        }
    }
    if (_pending_display_stack_meteo_page && !_pending_stack_meteo_page)
    {
        const uint32_t node_id = _pending_display_stack_meteo_node_id;
        const uint16_t offset = _pending_display_stack_meteo_offset;
        _pending_display_stack_meteo_page = false;
        if (node_id != 0 && net.network.prepareStackMeteoPageRequest(node_id, millis(), offset, 4000u))
        {
            _pending_stack_meteo_page = true;
            _pending_stack_meteo_node_id = node_id;
            _pending_stack_meteo_offset = offset;
            _pending_stack_meteo_limit = 8;
            _pending_stack_meteo_log = false;
        }
    }
    if (_pending_display_stack_thermo_page && !_pending_stack_thermo_page)
    {
        const uint32_t node_id = _pending_display_stack_thermo_node_id;
        const uint16_t offset = _pending_display_stack_thermo_offset;
        _pending_display_stack_thermo_page = false;
        if (node_id != 0 && net.network.prepareStackThermoPageRequest(node_id, millis(), offset, 4000u))
        {
            _pending_stack_thermo_page = true;
            _pending_stack_thermo_node_id = node_id;
            _pending_stack_thermo_offset = offset;
            _pending_stack_thermo_limit = 8;
            _pending_stack_thermo_log = false;
        }
    }
    if (_pending_display_stack_tanks_page && !_pending_stack_tanks_page)
    {
        const uint32_t node_id = _pending_display_stack_tanks_node_id;
        const uint16_t offset = _pending_display_stack_tanks_offset;
        _pending_display_stack_tanks_page = false;
        if (node_id != 0 && net.network.prepareStackTanksPageRequest(node_id, millis(), offset, 4000u))
        {
            _pending_stack_tanks_page = true;
            _pending_stack_tanks_node_id = node_id;
            _pending_stack_tanks_offset = offset;
            _pending_stack_tanks_limit = 8;
            _pending_stack_tanks_log = false;
        }
    }
    flushPending();
}

bool AppRuntime::stackMasterActive_() const{
    return net.network.stackMasterActive();
}

bool AppRuntime::stackSlaveActive_() const{
    return net.network.stackRole() == ConfigsManagerIface::StackRole::Slave &&
           !net.network.stackFallbackActive();
}

void AppRuntime::updateMasterLed_(bool master_active){
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

void AppRuntime::updateStackMasterMode_(){
    const bool active = false;
    if (_stack_master_effective != active)
    {
        _stack_master_effective = active;
        updateSecurityNotifyMode_();
        updateSepticNotifyMode_();
        updateTanksNotifyMode_();
    }
    updateMasterLed_(active);
}

bool AppRuntime::requestStackPollFeature_(uint32_t node_id, uint8_t feature){
    switch (feature)
    {
    case 0:
        return net.network.stackRoute().sendRequest(node_id, "system", "snapshot_req", nullptr,
                                                    StackRouteAdapter::Mode::Json, true);
    case 1:
        return net.network.stackRoute().sendRequest(node_id, "controllers", "summary_req", nullptr,
                                                    StackRouteAdapter::Mode::Json, true);
    case 2:
    {
        DynamicJsonDocument req(64);
        req["offset"] = 0;
        req["limit"] = 8;
        return net.network.stackRoute().sendRequest(node_id, "meteo", "snapshot_req", &req,
                                                    StackRouteAdapter::Mode::Json, true);
    }
    case 3:
    {
        DynamicJsonDocument req(64);
        req["offset"] = 0;
        req["limit"] = 8;
        return net.network.stackRoute().sendRequest(node_id, "thermo", "snapshot_req", &req,
                                                    StackRouteAdapter::Mode::Json, true);
    }
    case 4:
    {
        DynamicJsonDocument req(64);
        req["offset"] = 0;
        req["limit"] = 8;
        return net.network.stackRoute().sendRequest(node_id, "tanks", "snapshot_req", &req,
                                                    StackRouteAdapter::Mode::Json, true);
    }
    case 5:
    {
        DynamicJsonDocument req(64);
        req["offset"] = 0;
        req["limit"] = 8;
        return net.network.stackRoute().sendRequest(node_id, "sockets", "snapshot_req", &req,
                                                    StackRouteAdapter::Mode::Json, true);
    }
    case 6:
    {
        DynamicJsonDocument req(64);
        req["offset"] = 0;
        req["limit"] = 8;
        return net.network.stackRoute().sendRequest(node_id, "lights", "snapshot_req", &req,
                                                    StackRouteAdapter::Mode::Json, true);
    }
    default: return false;
    }
}

bool AppRuntime::bootstrapSyncCompleted_(uint32_t node_id) const{
    if (node_id == 0)
        return true;
    StackUnitSnapshot::State snapshot{};
    if (!net.network.stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
        return false;
    const bool sockets_ready = snapshot.socket_count >= snapshot.sockets_enabled;
    const bool lights_ready = snapshot.light_count >= snapshot.lights_enabled;
    const bool meteo_ready = snapshot.meteo_count >= snapshot.meteo_enabled;
    const bool thermo_ready = snapshot.thermo_count >= snapshot.thermo_enabled;
    const bool tanks_ready = snapshot.tank_count >= snapshot.tanks_enabled;
    return sockets_ready && lights_ready && meteo_ready && thermo_ready && tanks_ready;
}

bool AppRuntime::shouldLogStackBootstrapSync_(uint32_t node_id) const{
    if (node_id == 0)
        return false;
    if (_stack_bootstrap_node_id == node_id)
        return true;
    for (uint8_t i = 0; i < _stack_bootstrap_queue_count; ++i)
    {
        if (_stack_bootstrap_queue[i] == node_id)
            return true;
    }
    return false;
}

bool AppRuntime::queueDisplayStackSnapshotPage_(uint32_t node_id, const char *feature, uint16_t offset){
    if (node_id == 0 || !feature || !stackMasterActive_())
        return false;
    if (strcmp(feature, "sockets") == 0)
    {
        if (_pending_stack_sockets_page || _pending_display_stack_sockets_page)
            return false;
        _pending_display_stack_sockets_page = true;
        _pending_display_stack_sockets_node_id = node_id;
        _pending_display_stack_sockets_offset = offset;
        return true;
    }
    if (strcmp(feature, "lights") == 0)
    {
        if (_pending_stack_lights_page || _pending_display_stack_lights_page)
            return false;
        _pending_display_stack_lights_page = true;
        _pending_display_stack_lights_node_id = node_id;
        _pending_display_stack_lights_offset = offset;
        return true;
    }
    if (strcmp(feature, "meteo") == 0)
    {
        if (_pending_stack_meteo_page || _pending_display_stack_meteo_page)
            return false;
        _pending_display_stack_meteo_page = true;
        _pending_display_stack_meteo_node_id = node_id;
        _pending_display_stack_meteo_offset = offset;
        return true;
    }
    if (strcmp(feature, "thermo") == 0)
    {
        if (_pending_stack_thermo_page || _pending_display_stack_thermo_page)
            return false;
        _pending_display_stack_thermo_page = true;
        _pending_display_stack_thermo_node_id = node_id;
        _pending_display_stack_thermo_offset = offset;
        return true;
    }
    if (strcmp(feature, "tanks") == 0)
    {
        if (_pending_stack_tanks_page || _pending_display_stack_tanks_page)
            return false;
        _pending_display_stack_tanks_page = true;
        _pending_display_stack_tanks_node_id = node_id;
        _pending_display_stack_tanks_offset = offset;
        return true;
    }
    return false;
}

void AppRuntime::enqueueStackBootstrapSync_(uint32_t node_id){
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
    if (_stack_bootstrap_queue_count < StackDeviceRegistry::kMaxDevices)
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

void AppRuntime::removeStackBootstrapSync_(uint32_t node_id){
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

void AppRuntime::tryStartNextStackBootstrapSync_(){
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

void AppRuntime::startStackBootstrapSync_(uint32_t node_id){
    if (node_id == 0)
        return;
    _stack_bootstrap_node_id = node_id;
    _stack_bootstrap_started_ms = millis();
    _stack_bootstrap_feature_index = 0;
    _stack_bootstrap_pass = 0;
    _stack_bootstrap_feature_sent_mask = 0;
    _stack_bootstrap_logged_sockets_node_id = 0;
    _stack_bootstrap_logged_lights_node_id = 0;
    _stack_bootstrap_logged_meteo_node_id = 0;
    _stack_bootstrap_logged_thermo_node_id = 0;
    _stack_bootstrap_logged_tanks_node_id = 0;
    _stack_bootstrap_logged_sockets_offset = 0xFFFF;
    _stack_bootstrap_logged_lights_offset = 0xFFFF;
    _stack_bootstrap_logged_meteo_offset = 0xFFFF;
    _stack_bootstrap_logged_thermo_offset = 0xFFFF;
    _stack_bootstrap_logged_tanks_offset = 0xFFFF;
    _last_stack_poll_ms = 0; // allow first bootstrap request on the next loop tick
    STACK_BOOTSTRAP_EVT_INFO((*this), "Bootstrap sync start: %s", stackNodeLabel_(node_id).c_str());
    STACK_BOOTSTRAP_DBG((*this), "Bootstrap active: id: 0x%08lX q:%u",
                        (unsigned long)node_id, (unsigned)_stack_bootstrap_queue_count);
}

void AppRuntime::stopStackBootstrapSync_(bool timeout){
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
    _stack_bootstrap_logged_sockets_node_id = 0;
    _stack_bootstrap_logged_lights_node_id = 0;
    _stack_bootstrap_logged_meteo_node_id = 0;
    _stack_bootstrap_logged_thermo_node_id = 0;
    _stack_bootstrap_logged_tanks_node_id = 0;
    _stack_bootstrap_logged_sockets_offset = 0xFFFF;
    _stack_bootstrap_logged_lights_offset = 0xFFFF;
    _stack_bootstrap_logged_meteo_offset = 0xFFFF;
    _stack_bootstrap_logged_thermo_offset = 0xFFFF;
    _stack_bootstrap_logged_tanks_offset = 0xFFFF;
    if (prev_node_id != 0)
        tryStartNextStackBootstrapSync_();
}

void AppRuntime::pollStackCaches_(){
    if (!stackMasterActive_())
        return;
    logLocalInventory_();
    const size_t count = net.network.stackOnlineDeviceCount();
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
        StackDeviceRegistry::DeviceInfo bootstrap_device{};
        if (!net.network.stackDeviceSnapshotByNodeId(bs_node, bootstrap_device) ||
            !bootstrap_device.online || (uint32_t)(now - bootstrap_device.last_seen_ms) > kStackNodeStaleMs)
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
        if (_stack_poll_index >= count)
            _stack_poll_index = 0;
        StackDeviceRegistry::DeviceInfo device{};
        if (!net.network.stackDeviceSnapshotAt(_stack_poll_index++, device))
            return;
        node_id = device.node_id;
        if (node_id == 0 || !device.online || (uint32_t)(now - device.last_seen_ms) > kStackNodeStaleMs)
            return;
        logStackNodeInventory_(node_id);
        feature = (uint8_t)(_stack_poll_feature_index % kStackBackgroundPollFeatureCount);
        _stack_poll_feature_index =
            (uint8_t)((_stack_poll_feature_index + 1) % kStackBackgroundPollFeatureCount);
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

void AppRuntime::logLocalInventory_(){
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

bool AppRuntime::onRemoteMeteo_(void *ctx, uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp){
    if (!ctx || node_id == 0 || sensor_id == 0)
        return false;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    StackUnitSnapshot::MeteoItem item{};
    if (!self->net.network.stackIndexMeteoById(node_id, sensor_id, item))
        return false;
    if (!item.enabled || !item.ok || !item.has_temp)
        return false;
    temp_c = item.temp_c;
    has_temp = true;
    return true;
}

bool AppRuntime::onRemoteMeteoProxy_(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                float &temp_c, bool &has_temp, float &hum, bool &has_hum, bool &ok){
    if (!ctx || node_id == 0 || sensor_id == 0)
        return false;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    StackUnitSnapshot::MeteoItem item{};
    if (!self->net.network.stackIndexMeteoById(node_id, sensor_id, item))
        return false;
    if (!item.enabled)
        return false;
    temp_c = item.temp_c;
    has_temp = item.has_temp;
    hum = item.humidity;
    has_hum = item.has_humidity;
    ok = item.ok;
    return item.has_read;
}

bool AppRuntime::onRemoteNodeName_(void *ctx, uint32_t node_id, String &out){
    if (!ctx || node_id == 0)
        return false;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    if (!self->stackMasterActive_() && !self->stackSlaveActive_())
        return false;
    out = self->stackNodeLabel_(node_id);
    return out.length() > 0;
}

bool AppRuntime::onRemoteSensorName_(void *ctx, uint32_t node_id, uint8_t sensor_id, String &out){
    if (!ctx || node_id == 0 || sensor_id == 0)
        return false;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    StackUnitSnapshot::MeteoItem item{};
    if (!self->net.network.stackIndexMeteoById(node_id, sensor_id, item))
        return false;
    if (item.name[0] != '\0')
        out = item.name;
    else
        out = String("Sensor ") + String((unsigned)sensor_id);
    return out.length() > 0;
}

bool AppRuntime::onRemoteSensorType_(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                MeteoController::SensorType &out){
    out = MeteoController::SensorType::None;
    if (!ctx || node_id == 0 || sensor_id == 0)
        return false;
    AppRuntime *self = static_cast<AppRuntime *>(ctx);
    StackUnitSnapshot::MeteoItem item{};
    if (!self->net.network.stackIndexMeteoById(node_id, sensor_id, item))
        return false;
    out = (MeteoController::SensorType)item.type;
    return out != MeteoController::SensorType::None;
}
