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

#include "hal/gpio/gpio.hpp"
#include "utils/logger.hpp"
#include "utils/rtos_lock.hpp"

class SocketController
{
public:
    static constexpr size_t kSocketCount = 72;
    static constexpr size_t kLightCount = 72;
    static constexpr uint8_t kInvalidPort = 0xFF;
    using LockGuard = RtosRecursiveLock::Guard;

    struct SocketConfig
    {
        uint8_t id = 0;
        bool enabled = false;
        uint8_t group_id = 0;
        uint8_t button_port = kInvalidPort;
        uint8_t relay_port = kInvalidPort;
        String name;
    };

    struct SocketState
    {
        bool relay_on = false;
        bool last_button = false;
        bool button_idle = false;
        uint32_t cooldown_until_ms = 0;
        bool has_button = false;
    };

    using LightConfig = SocketConfig;
    using LightState = SocketState;

    SocketController(Gpio &gpio, Logger &logs) ;void applyConfig(JsonArrayConst sockets, bool legacy_lights = false);void applyLightsConfig(JsonArrayConst lights);bool begin();void task();bool setRelay(size_t id, bool on);bool toggleRelay(size_t id);bool relayState(size_t id, bool &out) const;bool setRelayById(uint8_t id, bool on);bool toggleRelayById(uint8_t id);bool relayStateById(uint8_t id, bool &out) const;bool setLightRelay(size_t id, bool on);bool toggleLightRelay(size_t id);bool lightRelayState(size_t id, bool &out) const;bool setLightRelayById(uint8_t id, bool on);bool toggleLightRelayById(uint8_t id);bool lightRelayStateById(uint8_t id, bool &out) const;bool setEnabled(size_t id, bool enable);bool setButtonPort(size_t id, uint8_t port);bool setRelayPort(size_t id, uint8_t port);bool setName(size_t id, const String &name);bool setGroupId(size_t id, uint8_t group_id);bool setLightEnabled(size_t id, bool enable);bool setLightButtonPort(size_t id, uint8_t port);bool setLightRelayPort(size_t id, uint8_t port);bool setLightName(size_t id, const String &name);bool setLightGroupId(size_t id, uint8_t group_id);const SocketConfig *config(size_t id) const;const SocketState *state(size_t id) const;const SocketConfig *configByIndex(size_t idx) const;const SocketState *stateByIndex(size_t idx) const;const LightConfig *lightConfig(size_t id) const;const LightState *lightState(size_t id) const;const LightConfig *lightConfigByIndex(size_t idx) const;const LightState *lightStateByIndex(size_t idx) const;void serialize(JsonArray out) const;void serializeLights(JsonArray out) const;void buildSnapshot(uint8_t *enabled_mask, uint8_t *state_mask, size_t bytes) const;void applySnapshot(const uint8_t *enabled_mask, const uint8_t *state_mask, size_t bytes);void buildLightsSnapshot(uint8_t *enabled_mask, uint8_t *state_mask, size_t bytes) const;void applyLightsSnapshot(const uint8_t *enabled_mask, const uint8_t *state_mask, size_t bytes);bool takeDirty();bool controllerEnabled() const;bool lightsEnabled() const;void setControllerEnabled(bool enabled);void setLightsEnabled(bool enabled);bool takeLightsDirty();
    LockGuard lockGuard() const { return _lock.guard(); }
private:
    Gpio &_gpio;
    SocketConfig _cfg[kSocketCount];
    SocketState _state[kSocketCount];
    bool _controller_enabled = false;
    bool _dirty_sockets = false;
    LightConfig _light_cfg[kLightCount];
    LightState _light_state[kLightCount];
    bool _lights_enabled = false;
    bool _dirty_lights = false;
    Logger &_logs;
    mutable RtosRecursiveLock _lock;

    void resetSockets_();void resetLights_();bool indexById_(uint8_t id, size_t &out) const;bool lightIndexById_(uint8_t id, size_t &out) const;static bool parsePort_(JsonVariantConst v, uint8_t &out);bool setupButton_(const SocketConfig &cfg, SocketState &st);bool setupRelay_(const SocketConfig &cfg, SocketState &st);bool syncButtonState_(const SocketConfig &cfg, SocketState &st);void writeRelay_(const SocketConfig &cfg, bool on);static constexpr bool kButtonInvert = true;
    static constexpr bool kRelayInvert = false;
    static constexpr bool kButtonPullup = true;
    static constexpr uint32_t kButtonCooldownMs = 500;
};
