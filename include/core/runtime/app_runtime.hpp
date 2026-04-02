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

#include "boards/board_profile.hpp"
#include "controllers/controllers.hpp"
#include "core/display.hpp"
#include "hal/ds3231mz.hpp"
#include "core/network/stack/stack_device_registry.hpp"
#include "core/network/stack/stack_unit_snapshot.hpp"
#include "core/network/stack/stack_json_protocol.hpp"

struct CoreContext;
struct HardwareContext;
struct CommsContext;
struct ControlContext;
struct UiContext;
struct NetworkContext;
struct ConfigContext;
class TaskBinder;

class AppRuntime
{
public:
    enum class TaskPhase : uint8_t
    {
        Idle = 0,
        PreNetwork,
        PostNetwork
    };

    AppRuntime(CoreContext &core, HardwareContext &hw, CommsContext &comms,
                 ControlContext &control, UiContext &ui, NetworkContext &net,
                 ConfigContext &cfg);

    void bindCallbacks();
    void init();
    void applyLoadedConfig();

private:
    friend class TaskBinder;

    void loopBegin();
    void loopAfterNetwork();
    void flushPending();
    void setTaskPhase(TaskPhase phase);
    void taskPre();
    void taskPost();
    void taskFlush();

    bool stackMasterActive_() const;

    bool stackSlaveActive_() const;

    void updateMasterLed_(bool master_active);

    void updateStackMasterMode_();

    void updateTankAlarms_();

    void updateSepticAlarms_();

    void updateSecurityAlarms_();

    void updateMeteoAlarms_();

    void pollStackCaches_();

    void logLocalInventory_();
    bool requestStackPollFeature_(uint32_t node_id, uint8_t feature);
    void enqueueStackBootstrapSync_(uint32_t node_id);
    void removeStackBootstrapSync_(uint32_t node_id);
    void tryStartNextStackBootstrapSync_();
    void startStackBootstrapSync_(uint32_t node_id);
    void stopStackBootstrapSync_(bool timeout);
    bool bootstrapSyncCompleted_(uint32_t node_id) const;
    bool shouldLogStackBootstrapSync_(uint32_t node_id) const;
    bool queueDisplayStackSnapshotPage_(uint32_t node_id, const char *feature, uint16_t offset);

    static bool onRemoteMeteo_(void *ctx, uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp);

    static bool onRemoteMeteoProxy_(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                    float &temp_c, bool &has_temp, float &hum, bool &has_hum, bool &ok);

    static bool onRemoteNodeName_(void *ctx, uint32_t node_id, String &out);

    static bool onRemoteSensorName_(void *ctx, uint32_t node_id, uint8_t sensor_id, String &out);

    static bool onRemoteSensorType_(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                    MeteoController::SensorType &out);

    static void onMeteoAlarm_(void *ctx, uint32_t node_id, uint8_t sensor_id, bool alarm);

    static void onSecurityArmState_(void *ctx, bool armed);

    static bool onSecurityPreArmCheck_(void *ctx, String &out, String *plain_out);

    static void onStackNodeEvent_(void *ctx, uint32_t node_id, bool online);

    static void onSecurityAlarmState_(void *ctx, bool alarm_on);

    static void onSecurityClearDetect_(void *ctx);

    static void onSecurityDetect_(void *ctx, uint8_t sensor_id, const String &name, bool silent);

    static void onSepticDetect_(void *ctx, uint8_t septic_id, const String &name, bool is_alarm);

    static void onTankEmpty_(void *ctx, uint8_t tank_id, const String &name, bool empty);

    static void onWateringEvent_(void *ctx, WateringController::Event ev,
                                 const WateringController::RuleConfig &cfg,
                                 const WateringController::RuleState &st);

    static void onRingHold_(void *ctx, bool on);

    static void onStackRoute_(void *ctx, uint32_t source_node, const StackJsonProtocol::RouteMessage &route);

    static bool onSecurityRfidUid_(void *ctx, const String &uid);

    static bool onSecurityIButtonSerial_(void *ctx, const String &serial);

    static bool onDisplaySlot_(void *ctx, const DisplaySlotConfig &slot, char out[5]);

    void broadcastSecurityState_(bool armed);

    void sendSecurityStateToNode_(uint32_t node_id);

    void sendSecurityStateToNode_(uint32_t node_id, bool armed, bool force);

    void sendSecurityDetectToMaster_(uint8_t sensor_id, const String &name, bool silent);

    void sendSepticDetectToMaster_(uint8_t septic_id, const String &name, bool is_alarm);

    void sendTankEmptyToMaster_(uint8_t tank_id, const String &name);

    void sendWateringEventToMaster_(WateringController::Event ev,
                                    const WateringController::RuleConfig &cfg,
                                    const WateringController::RuleState &st);

    void broadcastRingHold_(bool on);

    void notifyRingHold_();

    void sendCloudNotify_(const char *kind, const char *reason, const String &msg);
    void publishCloudStackEvent_(uint32_t node_id, const char *kind, const char *reason,
                                 const String &data_json);

    void handleStackRoute_(uint32_t source_node, const StackJsonProtocol::RouteMessage &route);

    void handleSepticFrame_(uint32_t node_id, const String &action, JsonVariantConst params);

    void handleTankFrame_(uint32_t node_id, const String &action, JsonVariantConst params);

    void handleSocketFrame_(uint32_t node_id, const String &action, JsonVariantConst params);

    void appendSystemSnapshot_(JsonObject root) const;
    void appendSocketSnapshotSummary_(JsonObject root) const;
    void appendSocketSnapshotItems_(JsonObject root) const;
    void appendSocketSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const;
    void appendLightSnapshotItems_(JsonObject root) const;
    void appendLightSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const;
    void appendMeteoSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const;
    void appendThermoSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const;
    void appendTankSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const;
    void appendLeakSnapshotPage_(JsonObject root, uint16_t offset, uint16_t limit) const;
    void appendControllerSnapshotSummary_(JsonObject root) const;

    void handleWateringFrame_(uint32_t node_id, const String &action, JsonVariantConst params);

    void updateSecurityNotifyMode_();

    void updateSepticNotifyMode_();

    void updateTanksNotifyMode_();

    void updateDisplayLayout_();
    void logStackSendFailDiag_(uint32_t node_id, const char *feature, uint16_t offset, uint16_t range_end);

    void flushPendingSecurityDetect_();

    void flushPendingSepticDetect_();

    void flushPendingTankEmpty_();

    void flushPendingWateringEvent_();
    void flushPendingStackSocketsResponse_();
    void flushPendingStackSocketsPage_();
    void flushPendingStackLightsResponse_();
    void flushPendingStackLightsPage_();
    void flushPendingStackMeteoPage_();
    void flushPendingStackThermoPage_();
    void flushPendingStackTanksPage_();
    void flushPendingStackLeakPage_();

    void flushPendingRfid_();

    void flushPendingIButton_();

    void flushPendingRingButton_();

    bool handleSecurityRfidUid_(const String &uid_str);

    bool handleSecurityIButtonSerial_(const String &serial);

    bool sendRingButtonToMaster_(bool pressed);

    bool renderDisplaySlot_(const DisplaySlotConfig &slot, char out[5]);

    static void formatTemp3_(char out[5], int t);

    static bool displaySlotEqual_(const DisplaySlotConfig &a, const DisplaySlotConfig &b);

    bool sendRfidToMaster_(const String &uid, const String &name);

    bool sendIButtonToMaster_(const String &serial, const String &name);

    void sendRfidResultToNode_(uint32_t node_id, const String &uid, bool matched,
                               const String &result, bool armed);

    void sendIButtonResultToNode_(uint32_t node_id, const String &serial, bool matched,
                                  const String &result, bool armed);

    void pollSecurityStatusFromMaster_();

    bool collectRemoteSecurityDetections_(String &out, String *plain_out);
    void syncRemoteSecurityAlarmFromSummary_(uint32_t node_id);

    String stackNodeLabel_(uint32_t node_id) const;

    static String escapeHtml_(const String &in);

    int stackNodeIndex_(uint32_t node_id) const;

    void logStackNodeInventory_(uint32_t node_id);

    struct StackInventoryLogState
    {
        uint32_t node_id = 0;
        uint16_t logged_mask = 0;
        bool sync_complete_logged = false;
    };

    StackInventoryLogState *inventoryLogState_(uint32_t node_id, bool create);

    void clearInventoryLogState_(uint32_t node_id);

    static constexpr uint32_t kPreArmFreshMs = 8000;
    static constexpr uint32_t kPreArmWaitMs = 2500;
    static constexpr uint32_t kPreArmPollMs = 1000;
    static constexpr uint32_t kStackNodeStaleMs = 15000;
    static constexpr uint32_t kDisplayNoDataErrMs = 30000;
    static constexpr uint16_t kInvSockets = 1u << 0;
    static constexpr uint16_t kInvLights = 1u << 1;
    static constexpr uint16_t kInvMeteo = 1u << 2;
    static constexpr uint16_t kInvThermo = 1u << 3;
    static constexpr uint16_t kInvTanks = 1u << 4;
    static constexpr uint16_t kInvSeptic = 1u << 5;
    static constexpr uint16_t kInvSecurity = 1u << 6;
    static constexpr uint16_t kInvWatering = 1u << 7;
    static constexpr uint16_t kInvLeak = 1u << 8;
    static constexpr uint16_t kInvAvr = 1u << 9;
    static constexpr uint16_t kInvAll = kInvSockets | kInvLights;

    void broadcastSecurityAlarm_(bool alarm_on);

    void sendSecurityAlarmToNode_(uint32_t node_id, bool alarm_on);

    void sendSecurityBeepToNode_(uint32_t node_id, const char *kind);

    void pollSecurityPrearmWarmup_();

    void broadcastSecurityClear_();

    void sendSecurityClearToNode_(uint32_t node_id);

    static constexpr uint32_t kStackPollMs = 2000;
    static constexpr uint32_t kStackBootstrapPollMs = 250;
    static constexpr uint32_t kStackBootstrapTimeoutMs = 25000;
    static constexpr uint8_t kStackBootstrapPasses = 2;
    static constexpr uint8_t kStackPollFeatureCount = 8;
    static constexpr uint8_t kStackBackgroundPollFeatureCount = 6;

    CoreContext &core;
    HardwareContext &hw;
    CommsContext &comms;
    ControlContext &control;
    UiContext &ui;
    NetworkContext &net;
    ConfigContext &cfg;

    TaskPhase _task_phase = TaskPhase::Idle;

    bool _pending_detect = false;
    uint8_t _pending_sensor_id = 0;
    String _pending_sensor_name;
    bool _pending_sensor_silent = false;
    bool _pending_septic_detect = false;
    uint8_t _pending_septic_id = 0;
    String _pending_septic_name;
    bool _pending_septic_alarm = false;
    bool _pending_tank_empty = false;
    uint8_t _pending_tank_id = 0;
    String _pending_tank_name;
    bool _pending_watering_event = false;
    WateringController::Event _pending_watering_event_type = WateringController::Event::Stop;
    WateringController::RuleConfig _pending_watering_event_cfg{};
    WateringController::RuleState _pending_watering_event_state{};
    bool _pending_stack_sockets_response = false;
    uint32_t _pending_stack_sockets_target_node = 0;
    uint32_t _pending_stack_sockets_reply_to = 0;
    uint16_t _pending_stack_sockets_response_offset = 0;
    uint16_t _pending_stack_sockets_response_limit = 0;
    bool _pending_stack_sockets_page = false;
    uint32_t _pending_stack_sockets_node_id = 0;
    uint16_t _pending_stack_sockets_offset = 0;
    uint16_t _pending_stack_sockets_limit = 0;
    bool _pending_stack_sockets_log = false;
    bool _pending_display_stack_sockets_page = false;
    uint32_t _pending_display_stack_sockets_node_id = 0;
    uint16_t _pending_display_stack_sockets_offset = 0;
    bool _pending_stack_lights_response = false;
    uint32_t _pending_stack_lights_target_node = 0;
    uint32_t _pending_stack_lights_reply_to = 0;
    uint16_t _pending_stack_lights_response_offset = 0;
    uint16_t _pending_stack_lights_response_limit = 0;
    bool _pending_stack_lights_page = false;
    uint32_t _pending_stack_lights_node_id = 0;
    uint16_t _pending_stack_lights_offset = 0;
    uint16_t _pending_stack_lights_limit = 0;
    bool _pending_stack_lights_log = false;
    bool _pending_stack_meteo_page = false;
    uint32_t _pending_stack_meteo_node_id = 0;
    uint16_t _pending_stack_meteo_offset = 0;
    uint16_t _pending_stack_meteo_limit = 0;
    bool _pending_stack_meteo_log = false;
    bool _pending_stack_thermo_page = false;
    uint32_t _pending_stack_thermo_node_id = 0;
    uint16_t _pending_stack_thermo_offset = 0;
    uint16_t _pending_stack_thermo_limit = 0;
    bool _pending_stack_thermo_log = false;
    bool _pending_stack_tanks_page = false;
    uint32_t _pending_stack_tanks_node_id = 0;
    uint16_t _pending_stack_tanks_offset = 0;
    uint16_t _pending_stack_tanks_limit = 0;
    bool _pending_stack_tanks_log = false;
    bool _pending_stack_leak_page = false;
    uint32_t _pending_stack_leak_node_id = 0;
    uint16_t _pending_stack_leak_offset = 0;
    uint16_t _pending_stack_leak_limit = 0;
    bool _pending_stack_leak_log = false;
    bool _pending_display_stack_lights_page = false;
    uint32_t _pending_display_stack_lights_node_id = 0;
    uint16_t _pending_display_stack_lights_offset = 0;
    bool _pending_display_stack_meteo_page = false;
    uint32_t _pending_display_stack_meteo_node_id = 0;
    uint16_t _pending_display_stack_meteo_offset = 0;
    bool _pending_display_stack_thermo_page = false;
    uint32_t _pending_display_stack_thermo_node_id = 0;
    uint16_t _pending_display_stack_thermo_offset = 0;
    bool _pending_display_stack_tanks_page = false;
    uint32_t _pending_display_stack_tanks_node_id = 0;
    uint16_t _pending_display_stack_tanks_offset = 0;
    bool _pending_display_stack_leak_page = false;
    uint32_t _pending_display_stack_leak_node_id = 0;
    uint16_t _pending_display_stack_leak_offset = 0;
    bool _pending_rfid = false;
    String _pending_rfid_uid;
    bool _pending_ibutton = false;
    String _pending_ibutton_serial;
    uint32_t _last_rfid_status_ms = 0;
    uint32_t _last_prearm_poll_ms = 0;
    bool _pending_ring_button = false;
    bool _pending_ring_button_pressed = false;
    bool _stack_master_effective = false;
    bool _master_led_initialized = false;
    bool _master_led_state = false;
    bool _local_inventory_logged = false;
    uint32_t _last_stack_poll_ms = 0;
    size_t _stack_poll_index = 0;
    uint8_t _stack_poll_feature_index = 0;
    uint32_t _stack_bootstrap_node_id = 0;
    uint32_t _stack_bootstrap_started_ms = 0;
    uint8_t _stack_bootstrap_feature_index = 0;
    uint8_t _stack_bootstrap_pass = 0;
    uint16_t _stack_bootstrap_feature_sent_mask = 0;
    uint32_t _stack_bootstrap_logged_sockets_node_id = 0;
    uint32_t _stack_bootstrap_logged_lights_node_id = 0;
    uint32_t _stack_bootstrap_logged_meteo_node_id = 0;
    uint32_t _stack_bootstrap_logged_thermo_node_id = 0;
    uint32_t _stack_bootstrap_logged_tanks_node_id = 0;
    uint32_t _stack_bootstrap_logged_leak_node_id = 0;
    uint16_t _stack_bootstrap_logged_sockets_offset = 0xFFFF;
    uint16_t _stack_bootstrap_logged_lights_offset = 0xFFFF;
    uint16_t _stack_bootstrap_logged_meteo_offset = 0xFFFF;
    uint16_t _stack_bootstrap_logged_thermo_offset = 0xFFFF;
    uint16_t _stack_bootstrap_logged_tanks_offset = 0xFFFF;
    uint16_t _stack_bootstrap_logged_leak_offset = 0xFFFF;
    uint32_t _stack_bootstrap_queue[StackDeviceRegistry::kMaxDevices]{};
    uint8_t _stack_bootstrap_queue_count = 0;
    StackInventoryLogState _stack_inventory_log[StackDeviceRegistry::kMaxDevices]{};
    DisplaySlotConfig _display_slots[Display::kSlotCount]{};
    bool _display_remote_slot_online[Display::kSlotCount]{};
    bool _display_rtc_cache_valid = false;
    Ds3231Mz::DateTime _display_rtc_cache{};
    uint32_t _display_rtc_cache_ms = 0;
    StackUnitSnapshot::SocketItem _stack_socket_page_items[StackUnitSnapshot::kPageSize]{};
    StackUnitSnapshot::SocketItem _stack_light_page_items[StackUnitSnapshot::kPageSize]{};
    StackUnitSnapshot::MeteoItem _stack_meteo_page_items[StackUnitSnapshot::kPageSize]{};
    StackUnitSnapshot::ThermoItem _stack_thermo_page_items[StackUnitSnapshot::kPageSize]{};
    StackUnitSnapshot::TankItem _stack_tank_page_items[StackUnitSnapshot::kPageSize]{};
    StackUnitSnapshot::LeakItem _stack_leak_page_items[StackUnitSnapshot::kPageSize]{};
};
