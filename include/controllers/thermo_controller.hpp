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

#include "controllers/meteo_controller.hpp"
#include "hal/gpio/gpio.hpp"
#include "utils/logger.hpp"
#include "utils/rtos_lock.hpp"

class ThermoController
{
public:
    static constexpr size_t kDeviceCount = 20;
    static constexpr uint8_t kInvalidPort = 0xFF;
    static constexpr uint8_t kInvalidSensor = 0;
    static constexpr size_t kMaskBytes = (kDeviceCount + 7) / 8;
    static constexpr int16_t kInvalidTarget = 0x7FFF;
    using LockGuard = RtosRecursiveLock::Guard;

    enum class Mode : uint8_t
    {
        Off = 0,
        Heat = 1,
        Cool = 2,
        Auto = 3
    };

    struct DeviceConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        uint8_t group_id = 0;
        uint8_t sensor_id = kInvalidSensor;
        uint32_t sensor_node_id = 0;
        uint8_t heat_port = kInvalidPort;
        uint8_t cool_port = kInvalidPort;
        uint8_t button_port = kInvalidPort;
        String name;
        float target_c = 22.0f;
        float hysteresis = 0.5f;
        Mode mode = Mode::Auto;
    };

    struct DeviceState
    {
        bool heat_on = false;
        bool cool_on = false;
        bool last_button = false;
        bool has_button = false;
        bool power_on = true;
    };

    ThermoController(Gpio &gpio, MeteoController &meteo, Logger &logs)
        ;bool begin();void task();void applyConfig(JsonArrayConst devices);void serialize(JsonArray out) const;bool controllerEnabled() const;using RemoteMeteoProvider = bool (*)(void *ctx, uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp);
    void setRemoteMeteoProvider(RemoteMeteoProvider cb, void *ctx);void setControllerEnabled(bool enabled);const DeviceConfig *config(size_t id) const;const DeviceState *state(size_t id) const;const DeviceConfig *configByIndex(size_t idx) const;const DeviceState *stateByIndex(size_t idx) const;void buildSnapshot(uint8_t *power_mask, size_t bytes) const;void buildTargetSnapshot(int16_t *targets, size_t count) const;void applySnapshot(const uint8_t *power_mask, size_t bytes);void applyTargetSnapshot(const int16_t *targets, size_t count);bool takeDirty();bool setEnabled(size_t id, bool enable);bool setSensor(size_t id, uint8_t sensor_id);bool setSensorSource(size_t id, uint32_t node_id, uint8_t sensor_id);bool setMode(size_t id, Mode mode);bool setTarget(size_t id, float target_c);bool setHysteresis(size_t id, float hyst);bool setName(size_t id, const String &name);bool setGroupId(size_t id, uint8_t group_id);bool setPower(size_t id, bool on, const char *src = nullptr);bool togglePower(size_t id, const char *src = nullptr);bool setHeatPort(size_t id, uint8_t port);bool setCoolPort(size_t id, uint8_t port);bool setButtonPort(size_t id, uint8_t port);static const char *modeName(Mode mode);LockGuard lockGuard(uint32_t timeout_ms = 0xFFFFFFFFu) const { return _lock.guard(timeout_ms); }
private:
    Gpio &_gpio;
    MeteoController &_meteo;
    Logger &_logs;
    RemoteMeteoProvider _remote_meteo_cb = nullptr;
    void *_remote_meteo_ctx = nullptr;
    DeviceConfig _cfg[kDeviceCount];
    DeviceState _state[kDeviceCount];
    bool _controller_enabled = false;
    bool _dirty = false;
    mutable RtosRecursiveLock _lock;

    static constexpr bool kButtonInvert = true;
    static constexpr bool kButtonPullup = true;
    static constexpr bool kHeatInvert = false;
    static constexpr bool kCoolInvert = false;

    void reset_();bool indexById_(uint8_t id, size_t &out) const;static bool parsePort_(JsonVariantConst v, uint8_t &out);static bool parseFloat_(JsonVariantConst v, float &out);static Mode parseMode_(JsonVariantConst v);static const char *modeName_(Mode mode);bool setupButton_(const DeviceConfig &cfg, DeviceState &st);bool setupRelay_(const DeviceConfig &cfg, DeviceState &st);void handleButton_(const DeviceConfig &cfg, DeviceState &st);void updateControl_(const DeviceConfig &cfg, DeviceState &st);bool writeOff_(const DeviceConfig &cfg, DeviceState &st);bool writeOutputs_(const DeviceConfig &cfg, DeviceState &st, bool heat, bool cool);};
