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

#include "core/network/telegram/telegram_allowed_users.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/gsm_modem.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/gpio/gpio.hpp"
#include "hal/gpio/portio.hpp"
#include "hal/ibutton.hpp"
#include "hal/pn532.hpp"
#include "core/eeprom_storage.hpp"
#include "plc/plc_control.hpp"
#include "utils/logger.hpp"
#include "utils/users_registry.hpp"

class SecurityController
{
public:
    static constexpr size_t kSensorCount = 72;
    static constexpr size_t kKeyCount = UsersRegistry::kMaxUsers;
    static constexpr size_t kRfidKeyCount = UsersRegistry::kMaxUsers;
    static constexpr size_t kPhoneCount = UsersRegistry::kMaxUsers;
    static constexpr uint8_t kInvalidPort = 0xFF;

    enum class SensorType : uint8_t
    {
        Pir = 0,
        Reed
    };

    struct SensorConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        uint8_t group_id = 0;
        SensorType type = SensorType::Pir;
        uint8_t port = kInvalidPort;
        bool silent = false;
        String name;
    };

    struct SensorState
    {
        bool raw = false;
        bool is_detect = false;
    };

    using ArmStateHandler = void (*)(void *ctx, bool armed);
    using PreArmCheckHandler = bool (*)(void *ctx, String &out, String *plain_out);
    using AlarmStateHandler = void (*)(void *ctx, bool alarm_on);
    using ClearDetectHandler = void (*)(void *ctx);
    using DetectHandler = void (*)(void *ctx, uint8_t sensor_id, const String &name, bool silent);
    using RfidUidHandler = bool (*)(void *ctx, const String &uid);
    using IButtonSerialHandler = bool (*)(void *ctx, const String &serial);

    SecurityController(Gpio &gpio, OneWireManager &ow, Logger &logs,
                       TelegramBot &bot, TelegramAllowedUsersProvider &users)
        ;bool begin();void task();void applyConfig(JsonArrayConst sensors);void applyKeys(JsonArrayConst keys);void applyRfidKeys(JsonArrayConst keys);void applyPhones(JsonArrayConst phones);void setSirenPort(uint8_t port);void serialize(JsonArray out) const;void serializeKeys(JsonArray out) const;void serializeRfidKeys(JsonArray out) const;void serializePhones(JsonArray out) const;void buildSnapshot(uint8_t &flags) const;void applySnapshot(uint8_t flags);void setArmStateHandler(ArmStateHandler cb, void *ctx);void setPreArmCheckHandler(PreArmCheckHandler cb, void *ctx);void setAlarmStateHandler(AlarmStateHandler cb, void *ctx);void setClearDetectHandler(ClearDetectHandler cb, void *ctx);void setDetectHandler(DetectHandler cb, void *ctx);void setRfidUidHandler(RfidUidHandler cb, void *ctx);void setIButtonSerialHandler(IButtonSerialHandler cb, void *ctx);void setRfidI2c(I2CManager *i2c);void setUsersRegistry(UsersRegistry &users);void setPlcControl(PlcControl &plc);bool processRfidUid(const PN532::UID &uid, const char *src = "rfid");bool processRfidUidString(const char *uid_str, const char *src = "rfid");bool processIButtonAddr(const uint8_t addr[8], const char *src = "ibutton");bool processIButtonSerialString(const char *serial, const char *src = "ibutton");void setNotifyEnabled(bool enabled);bool takeDirty();bool takeForceSave();bool controllerEnabled() const;void setControllerEnabled(bool enabled);bool armed() const;bool alarmOn() const;uint8_t sirenPort() const;void setGsmModem(GsmModem &modem);bool arm();bool disarm();bool armFrom(const char *src, const String &user);bool armForcedFrom(const char *src, const String &user);bool disarmFrom(const char *src, const String &user, bool silent = false);void toggleFrom(const char *src, const String &user);void clearDetect();bool fillPrearmItems(JsonArray &arr, String *plain_out = nullptr);void notifyRemoteDetect(const String &source, uint8_t sensor_id, const String &name, bool silent);void setAlarmState(bool on);bool setEnabled(size_t id, bool enabled);bool setType(size_t id, SensorType type);bool setPort(size_t id, uint8_t port);bool setName(size_t id, const String &name);bool setGroupId(size_t id, uint8_t group_id);bool setSilent(size_t id, bool silent);bool addKey(const uint8_t addr[8]);bool addKey(const uint8_t addr[8], const String &name);bool removeKey(const uint8_t addr[8]);void clearKeys();void clearPhones();bool setPhone(size_t idx, const String &number);bool setPhoneName(size_t idx, const String &name);bool setPhoneNotify(size_t idx, bool notify);bool setPhoneCall(size_t idx, bool call);bool setPhoneEnabled(size_t idx, bool enabled);bool phoneSlot(size_t idx, String &number, bool &enabled) const;const String &phoneByIndex(size_t idx) const;const String &phoneNameByIndex(size_t idx) const;bool phoneNotifyByIndex(size_t idx) const;bool phoneCallByIndex(size_t idx) const;size_t keyCount() const;bool keyByIndex(size_t idx, uint8_t out[8]) const;const String &keyNameByIndex(size_t idx) const;bool setKeyNameByAddr(const uint8_t addr[8], const String &name);bool keySlot(size_t idx, uint8_t out[8], bool &enabled) const;bool lastKeyHex(char out[17]) const;bool lastRfidSerial(String &out) const;bool setKeySlot(size_t idx, const uint8_t addr[8], bool enabled, const String &name);bool rfidKeySlot(size_t idx, uint8_t out[10], uint8_t &len, bool &enabled) const;const String &rfidKeyNameByIndex(size_t idx) const;bool setRfidKeySlot(size_t idx, const uint8_t *bytes, uint8_t len, bool enabled, const String &name);static bool parseRfidSerial(const char *s, uint8_t out[10], uint8_t &len);static String rfidSerialToString(const uint8_t *bytes, uint8_t len);const SensorConfig *config(size_t id) const;const SensorState *state(size_t id) const;const SensorConfig *configByIndex(size_t idx) const;const SensorState *stateByIndex(size_t idx) const;private:
    struct TgNotifyItem
    {
        String msg;
        String parse_mode;
        uint16_t next_user = 0;
    };

    Gpio &_gpio;
    OneWireManager &_ow;
    Logger &_logs;
    TelegramBot &_tgbot;
    TelegramAllowedUsersProvider &_tgusers;
    uint32_t _tg_last_send_ms = 0;
    static constexpr uint8_t kTgQueueDepth = 8;
    TgNotifyItem _tg_queue[kTgQueueDepth]{};
    uint8_t _tg_q_head = 0;
    uint8_t _tg_q_tail = 0;
    uint8_t _tg_q_size = 0;
    GsmModem *_gsm = nullptr;
    IButton _ibutton;
    bool _ibutton_ready = false;
    I2CManager *_rfid_i2c = nullptr;
    PN532 _rfid;
    bool _rfid_ready = false;
    bool _rfid_disabled_startup_missing = false;
    UsersRegistry *_users = nullptr;
    RfidUidHandler _rfid_uid_cb = nullptr;
    void *_rfid_uid_ctx = nullptr;
    IButtonSerialHandler _ibutton_serial_cb = nullptr;
    void *_ibutton_serial_ctx = nullptr;

    SensorConfig _cfg[kSensorCount]{};
    SensorState _state[kSensorCount]{};
    uint8_t _last_key[8]{};
    uint32_t _last_key_ms = 0;
    uint8_t _last_rfid[10]{};
    uint8_t _last_rfid_len = 0;
    uint32_t _last_rfid_ms = 0;
    uint32_t _last_rfid_poll_ms = 0;
    bool _runtime_ready = false;
    bool _controller_enabled = false;
    bool _armed = false;
    bool _alarm_on = false;
    uint8_t _siren_port = kInvalidPort;
    bool _dirty = false;
    bool _force_save = false;

    uint8_t _beep_remaining = 0;
    uint16_t _beep_on_ms = 0;
    uint16_t _beep_off_ms = 0;
    bool _beep_state_on = false;
    uint32_t _beep_next_ms = 0;

    void reset_();void clearKeys_();void clearRfidKeys_();bool setRfidKeySlot_(size_t idx, const PN532::UID &uid, bool enabled, const String &name);void clearPhones_();static bool indexById_(uint8_t id, size_t &out);static const char *typeName_(SensorType t);static bool parseType_(const char *s, SensorType &out);void setupOutputs_();void setupBuzzer_();void setupAlarmLed_();void setupSiren_();void updateAlarmLed_();void updateSiren_();void setupSensorInput_(const SensorConfig &cfg);bool readRaw_(const SensorConfig &cfg);static bool isTriggered_(const SensorConfig &cfg, bool raw);void initIButton_();void initRfid_(bool startup = false);void handleRfid_();void handleIButton_();bool isAllowedKey_(const uint8_t addr[8]) const;bool isKeyRepeat_(const uint8_t addr[8]);bool isRfidRepeat_(const PN532::UID &uid);static bool portToGpio_(uint8_t port, uint8_t &out_gpio);void toggleArm_();void toggleArm_(const char *src, const String &user);void handleGsm_();void arm_();void disarm_(bool silent);void arm_(const char *src, const String &user);void armForce_(const char *src, const String &user);void disarm_(bool silent, const char *src, const String &user);void applySnapshot_(bool armed, bool alarm);bool hasTriggeredBeforeArm_(String &out, String *plain_out = nullptr);static String escapeHtml_(const char *text);static String escapeHtml_(const String &text);void clearDetect_();void startBeep_(uint8_t count, uint16_t on_ms, uint16_t off_ms);void updateBuzzer_();void updateAlarmBuzzer_();void resetAlarmBuzzer_();bool buzzerEnabled_() const;void writeBuzzer_(bool on);void logDetect_(const SensorConfig &cfg);void notifyDetect_(const SensorConfig &cfg);void notifyArmAction_(bool armed, const char *src, const String &user);void sendTgNotify_(const String &msg, const String &parse_mode = "");bool enqueueTgNotify_(const String &msg, const String &parse_mode);void popTgNotify_();void processTgNotifyQueue_();bool isAllowedPhone_(const String &number) const;bool matchPhone_(const String &number, String &user) const;void sendSmsNotify_(const SensorConfig &cfg);void sendSmsNotify_(uint8_t sensor_id, const String &name);bool matchKey_(const uint8_t addr[8], String &user) const;bool matchRfidKey_(const PN532::UID &uid, String &user) const;void logArmAction_(bool armed, const char *src, const String &user);void notifyArmState_(bool armed);void notifyAlarmState_(bool alarm_on);void notifyClearDetect_();void notifyDetectEvent_(const SensorConfig &cfg);static bool owIButtonLockCb_(void *ctx, uint32_t timeout_ms);static void owIButtonUnlockCb_(void *ctx);static int hexNibble_(char c);static bool parseHexAddr_(const char *s, uint8_t out[8]);static bool parseRfidUid_(const char *s, PN532::UID &out);static String rfidUidToString_(const uint8_t *bytes, uint8_t len);static constexpr uint32_t kKeyRepeatMs = 2000;
    static constexpr uint32_t kTgSendGapMs = 800;
    static constexpr uint16_t kBeepShortMs = 120;
    static constexpr uint16_t kBeepGapMs = 120;
    static constexpr uint16_t kBeepLongMs = 500;
    static constexpr uint8_t kBeepRejectCount = 3;
    static constexpr uint16_t kBeepRejectOnMs = 60;
    static constexpr uint16_t kBeepRejectOffMs = 80;
    static constexpr uint16_t kAlarmBuzzMs = 500;
    static constexpr uint8_t kRfidI2cAddr = 0x24;
    static constexpr uint16_t kRfidReadTimeoutMs = 50;
    static constexpr uint16_t kRfidPollMs = 250;

    bool _alarm_buzz_state = false;
    uint32_t _alarm_buzz_next_ms = 0;
    PlcControl *_plc = nullptr;
    ArmStateHandler _arm_state_cb = nullptr;
    void *_arm_state_ctx = nullptr;
    PreArmCheckHandler _pre_arm_cb = nullptr;
    void *_pre_arm_ctx = nullptr;
    AlarmStateHandler _alarm_state_cb = nullptr;
    void *_alarm_state_ctx = nullptr;
    ClearDetectHandler _clear_detect_cb = nullptr;
    void *_clear_detect_ctx = nullptr;
    DetectHandler _detect_cb = nullptr;
    void *_detect_ctx = nullptr;
    bool _notify_enabled = true;
};
