#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>

#include "boards/board_profile.hpp"

#include "core/task_binder.hpp"
#include "core/task_manager.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/gsm_modem.hpp"
#include "core/rtc.hpp"
#include "core/display.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "core/eeprom_storage.hpp"
#include "core/network/network.hpp"
#include "core/network/stack/stack_slave_handler.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "core/cli/cli_console.hpp"
#include "core/network/web/web_interface.hpp"
#include "core/plc_scan.hpp"
#include "core/rules_controller.hpp"

#include "hal/at24lc512.hpp"
#include "hal/dht22.hpp"
#include "hal/ds18b20.hpp"
#include "hal/ds3231mz.hpp"
#include "hal/lcd1602_i2c.hpp"
#include "hal/lm75ad.hpp"
#include "hal/sim800l.hpp"
#include "hal/ibutton.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/gpio/portio.hpp"
#include "hal/gpio/gpio.hpp"
#include "hal/io_stack.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/bus/spi.hpp"
#include "hal/bus/uart.hpp"
#include "hal/hal.hpp"

#include "plc/plc_control.hpp"
#include "controllers/controllers.hpp"
#include "ftest.hpp"

#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager.hpp"
#include "utils/meteo_history.hpp"
#include "utils/users_registry.hpp"

struct CoreContext
{
    UartManager uart;
    Logger logs;
    TaskManager<TASK_MGR_TSK_COUNT> tm;
    Configs configs;

    CoreContext();
};

struct HardwareContext
{
    I2CManager i2c;
    SPIManager spi;
    OneWireManager ow;
    IButton ibutton;

    Ds18b20 ds18b20;
    DHT22 dht22;
    Ds3231Mz ds3231;
    RTC rtc;

    At24lc512 eeprom;
    EepromStorage eeprom_storage;

    Lcd1602I2c lcd_hal;
    Display display;

    Lm75ad lm75ad;

    Extender ext;
    PortIO portio;
    IoStack io;
    Gpio gpio;

    Hal hal;
    PlcControl plc;

    HardwareContext(Logger &logs, UartManager &uart);
};

struct CommsContext
{
    WifiManager wifi;
    Sim800l sim800l;
    GsmModem gsm;

    WiFiClientSecure telegram_wifi_client;
    TelegramClient telegram;
    TelegramBot telegram_bot;

    CommsContext(Logger &logs, UartManager &uart);
};

struct ControlContext
{
    UsersRegistry users;
    TelegramMenu telegram_menu;
    Controllers controllers;
    RulesController rules;
    MeteoHistory meteo_history;

    TaskBinder<TASK_MGR_TSK_COUNT> task_binder;
    Ftest ftest;
    PlcScanLoop plc_scan;

    ControlContext(CoreContext &core, HardwareContext &hw, CommsContext &comms);
};

struct UiContext
{
    CliConsole console;

    UiContext(CoreContext &core, HardwareContext &hw, CommsContext &comms, ControlContext &control);
};

struct NetworkContext
{
    AsyncWebServer web;
    WebInterface fw_upgrade;
    Network network;
    StackSlaveHandler stack_slave;

    NetworkContext(CoreContext &core, HardwareContext &hw, CommsContext &comms, ControlContext &control, UiContext &ui);
};

struct ConfigContext
{
    ConfigsManager configs_manager;

    ConfigContext(CoreContext &core, HardwareContext &hw, CommsContext &comms,
                  ControlContext &control, UiContext &ui, NetworkContext &network);
};

struct App
{
    CoreContext core;
    HardwareContext hw;
    CommsContext comms;
    ControlContext control;
    UiContext ui;
    StackCache stack_cache;
    NetworkContext net;
    ConfigContext cfg;

    App();


    bool begin();


    void loop();


private:
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

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame);

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

    void sendTelegramNotify_(const String &msg);

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame);

    void handleSepticFrame_(uint32_t node_id, const String &action, JsonVariantConst params);

    void handleTankFrame_(uint32_t node_id, const String &action, JsonVariantConst params);

    void handleWateringFrame_(uint32_t node_id, const String &action, JsonVariantConst params);

    void updateSecurityNotifyMode_();

    void updateSepticNotifyMode_();

    void updateTanksNotifyMode_();

    void updateDisplayLayout_();

    void flushPendingSecurityDetect_();

    void flushPendingSepticDetect_();

    void flushPendingTankEmpty_();

    void flushPendingWateringEvent_();

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
    static constexpr uint16_t kInvAll =
        kInvSockets | kInvLights | kInvMeteo | kInvThermo | kInvTanks |
        kInvSeptic | kInvSecurity | kInvWatering | kInvLeak;

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
    static constexpr uint8_t kStackPollFeatureCount = 13;

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
    uint32_t _stack_bootstrap_queue[StackMaster::MAX_SESSIONS]{};
    uint8_t _stack_bootstrap_queue_count = 0;
    StackInventoryLogState _stack_inventory_log[StackMaster::MAX_SESSIONS]{};
    DisplaySlotConfig _display_slots[Display::kSlotCount]{};
};
