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

#include "hal/bus/onewire.hpp"
#include "hal/dht22.hpp"
#include "hal/ds18b20.hpp"
#include "utils/logger.hpp"
#include "utils/rtos_lock.hpp"

class MeteoController
{
public:
    static constexpr size_t kSensorCount = 50;
    static constexpr uint8_t kInvalidPin = 0xFF;
    static constexpr uint8_t kAddrLen = 8;
    using LockGuard = RtosRecursiveLock::Guard;

    enum class SensorType : uint8_t
    {
        None = 0,
        Ds18b20,
        Dht22
    };

    struct SensorConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        uint8_t group_id = 0;
        SensorType type = SensorType::None;
        uint8_t dht_pin = kInvalidPin;
        uint8_t ds18_addr[kAddrLen] = {};
        bool ds18_addr_set = false;
        uint32_t source_node_id = 0;
        uint8_t source_sensor_id = 0;
        String name;
    };

    struct SensorState
    {
        float temp_c = 0.0f;
        float humidity = 0.0f;
        bool has_temp = false;
        bool has_humidity = false;
        bool ok = false;
        bool had_success = false;
        uint8_t fail_count = 0;
        uint32_t last_read_ms = 0;
    };

    MeteoController(OneWireManager &ow, Logger &logs) ;using RemoteMeteoProvider = bool (*)(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                         float &temp_c, bool &has_temp, float &hum, bool &has_hum, bool &ok);
    using RemoteNodeNameProvider = bool (*)(void *ctx, uint32_t node_id, String &out);
    using RemoteSensorNameProvider = bool (*)(void *ctx, uint32_t node_id, uint8_t sensor_id, String &out);
    using RemoteSensorTypeProvider = bool (*)(void *ctx, uint32_t node_id, uint8_t sensor_id, SensorType &out);
    using AlarmHandler = void (*)(void *ctx, uint32_t node_id, uint8_t sensor_id, bool alarm);
    void setRemoteMeteoProvider(RemoteMeteoProvider cb, void *ctx);void setRemoteNodeNameProvider(RemoteNodeNameProvider cb, void *ctx);void setRemoteSensorNameProvider(RemoteSensorNameProvider cb, void *ctx);void setRemoteSensorTypeProvider(RemoteSensorTypeProvider cb, void *ctx);void setAlarmHandler(AlarmHandler cb, void *ctx);bool begin();void task();void applyConfig(JsonArrayConst sensors);void serialize(JsonArray out) const;bool controllerEnabled() const;void setControllerEnabled(bool enabled);bool setEnabled(size_t id, bool enable);bool setType(size_t id, SensorType type);bool setDht22Pin(size_t id, uint8_t pin);bool setDs18b20Addr(size_t id, const uint8_t addr[kAddrLen], bool set);bool setName(size_t id, const String &name);bool setGroupId(size_t id, uint8_t group_id);bool setRemoteSource(size_t id, uint32_t node_id, uint8_t sensor_id);const SensorConfig *config(size_t id) const;bool displayName(uint8_t id, String &out) const;const SensorState *state(size_t id) const;const SensorConfig *configByIndex(size_t idx) const;const SensorState *stateByIndex(size_t idx) const;void listDs18b20Serials(char out[][17], size_t max, size_t &count);static bool parseHexAddr(const char *hex, uint8_t out[kAddrLen]);static void formatHexAddr(const uint8_t addr[kAddrLen], char out[17]);static const char *typeName(SensorType type);LockGuard lockGuard() const { return _lock.guard(); }
private:
    static constexpr uint32_t kDht22IntervalMs = 3000;
    static constexpr uint32_t kDs18b20IntervalMs = 1000;
    static constexpr uint32_t kRemoteIntervalMs = 2000;
    static constexpr uint8_t kFailThreshold = 10;
    static constexpr uint8_t kLocalFailThreshold = 3;
    static constexpr uint8_t kDht22LocalFailThreshold = 5;
    static constexpr uint8_t kDht22ReadAttempts = 2;
    static constexpr uint16_t kDht22RetryDelayUs = 500;

    OneWireManager &_ow;
    Logger &_logs;
    OneWireBus *_ds_bus = nullptr;
    Ds18b20 _ds18b20;
    DHT22 _dht22;
    uint8_t _dht22_pin = kInvalidPin;
    bool _ds_conv_pending = false;
    bool _ds_conv_ready = false;
    uint32_t _ds_last_conv_ms = 0;
    SensorConfig _cfg[kSensorCount];
    SensorState _state[kSensorCount];
    bool _controller_enabled = false;
    size_t _scan_index = 0;
    RemoteMeteoProvider _remote_cb = nullptr;
    void *_remote_ctx = nullptr;
    RemoteNodeNameProvider _remote_name_cb = nullptr;
    void *_remote_name_ctx = nullptr;
    RemoteSensorNameProvider _remote_sensor_name_cb = nullptr;
    void *_remote_sensor_name_ctx = nullptr;
    RemoteSensorTypeProvider _remote_sensor_type_cb = nullptr;
    void *_remote_sensor_type_ctx = nullptr;
    AlarmHandler _alarm_cb = nullptr;
    void *_alarm_ctx = nullptr;
    mutable RtosRecursiveLock _lock;

    void reset_();bool indexById_(uint8_t id, size_t &out) const;bool readIfDue_(size_t idx, uint32_t now);bool readRemoteIfDue_(const SensorConfig &cfg, SensorState &st, uint32_t now);bool readDs18b20IfDue_(const SensorConfig &cfg, SensorState &st, uint32_t now);bool readDht22_(const SensorConfig &cfg, float &out_temp, bool &out_has_temp, float &out_hum, bool &out_has_hum);static bool mapDhtPinToGpio_(uint8_t port, uint8_t &gpio);static SensorType parseType_(JsonVariantConst v);static bool parsePin_(JsonVariantConst v, uint8_t &out);static const char *typeName_(SensorType type);static const char *typeNameLog_(SensorType type);static int hexNibble_(char c);void updateDs18Conversion_(uint32_t now);void logMeteoStateChange_(const SensorConfig &cfg, const SensorState &st, bool prev_ok);static bool owLockCb_(void *ctx, uint32_t timeout_ms);static void owUnlockCb_(void *ctx);void applyReadResult_(const SensorConfig &cfg, SensorState &st, bool ok, bool has_temp, float temp_c, bool has_hum,
                          float hum);};
