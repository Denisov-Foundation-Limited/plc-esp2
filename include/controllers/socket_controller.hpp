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
    using EventHandler = void (*)(void *ctx, bool lights, uint8_t id, const String &name,
                                  bool state_on, const char *source);

    SocketController(Gpio &gpio, Logger &logs);
    void applyConfig(JsonArrayConst sockets, bool legacy_lights = false);
    void applyLightsConfig(JsonArrayConst lights);
    bool begin();
    void task();
    bool setRelay(size_t id, bool on, uint32_t timeout_ms = 0xFFFFFFFFu);
    bool toggleRelay(size_t id, uint32_t timeout_ms = 0xFFFFFFFFu);
    bool relayState(size_t id, bool &out, uint32_t timeout_ms = 0xFFFFFFFFu) const;
    bool setRelayById(uint8_t id, bool on, uint32_t timeout_ms = 0xFFFFFFFFu);
    bool toggleRelayById(uint8_t id, uint32_t timeout_ms = 0xFFFFFFFFu);
    bool relayStateById(uint8_t id, bool &out, uint32_t timeout_ms = 0xFFFFFFFFu) const;
    bool setLightRelay(size_t id, bool on, uint32_t timeout_ms = 0xFFFFFFFFu);
    bool toggleLightRelay(size_t id, uint32_t timeout_ms = 0xFFFFFFFFu);
    bool lightRelayState(size_t id, bool &out, uint32_t timeout_ms = 0xFFFFFFFFu) const;
    bool setLightRelayById(uint8_t id, bool on, uint32_t timeout_ms = 0xFFFFFFFFu);
    bool toggleLightRelayById(uint8_t id, uint32_t timeout_ms = 0xFFFFFFFFu);
    bool lightRelayStateById(uint8_t id, bool &out, uint32_t timeout_ms = 0xFFFFFFFFu) const;
    bool enqueueRelayActionById(uint8_t id, uint8_t action, bool lights, bool on = false, uint32_t timeout_ms = 0xFFFFFFFFu);
    bool setEnabled(size_t id, bool enable);
    bool setButtonPort(size_t id, uint8_t port);
    bool setRelayPort(size_t id, uint8_t port);
    bool setName(size_t id, const String &name);
    bool setGroupId(size_t id, uint8_t group_id);
    bool setLightEnabled(size_t id, bool enable);
    bool setLightButtonPort(size_t id, uint8_t port);
    bool setLightRelayPort(size_t id, uint8_t port);
    bool setLightName(size_t id, const String &name);
    bool setLightGroupId(size_t id, uint8_t group_id);
    const SocketConfig *config(size_t id) const;
    const SocketState *state(size_t id) const;
    const SocketConfig *configByIndex(size_t idx) const;
    const SocketState *stateByIndex(size_t idx) const;
    const LightConfig *lightConfig(size_t id) const;
    const LightState *lightState(size_t id) const;
    const LightConfig *lightConfigByIndex(size_t idx) const;
    const LightState *lightStateByIndex(size_t idx) const;
    void serialize(JsonArray out) const;
    void serializeLights(JsonArray out) const;
    void buildSnapshot(uint8_t *enabled_mask, uint8_t *state_mask, size_t bytes) const;
    void applySnapshot(const uint8_t *enabled_mask, const uint8_t *state_mask, size_t bytes);
    void buildLightsSnapshot(uint8_t *enabled_mask, uint8_t *state_mask, size_t bytes) const;
    void applyLightsSnapshot(const uint8_t *enabled_mask, const uint8_t *state_mask, size_t bytes);
    bool takeDirty();
    uint32_t socketChangeSeq(uint32_t timeout_ms = 0xFFFFFFFFu) const;
    bool controllerEnabled() const;
    bool lightsEnabled() const;
    void setControllerEnabled(bool enabled);
    void setLightsEnabled(bool enabled);
    void reinitializeConfiguredSockets();
    void reinitializeConfiguredLights();
    bool takeLightsDirty();
    uint32_t lightChangeSeq(uint32_t timeout_ms = 0xFFFFFFFFu) const;
    void setEventHandler(EventHandler cb, void *ctx);
    LockGuard lockGuard(uint32_t timeout_ms = 0xFFFFFFFFu) const { return _lock.guard(timeout_ms); }
private:
    struct PendingAction
    {
        uint8_t id = 0;
        uint8_t action = 0;
        bool lights = false;
        bool on = false;
    };

    static constexpr size_t kPendingActionCount = 8;
    Gpio &_gpio;
    SocketConfig _cfg[kSocketCount];
    SocketState _state[kSocketCount];
    bool _controller_enabled = false;
    bool _dirty_sockets = false;
    uint32_t _socket_change_seq = 0;
    LightConfig _light_cfg[kLightCount];
    LightState _light_state[kLightCount];
    bool _lights_enabled = false;
    bool _dirty_lights = false;
    uint32_t _light_change_seq = 0;
    PendingAction _pending_actions[kPendingActionCount];
    size_t _pending_action_count = 0;
    Logger &_logs;
    EventHandler _event_cb = nullptr;
    void *_event_ctx = nullptr;
    mutable RtosRecursiveLock _lock;

    bool dequeueRelayAction_(PendingAction &out, uint32_t timeout_ms = 0xFFFFFFFFu);
    void resetSockets_();void resetLights_();bool indexById_(uint8_t id, size_t &out) const;bool lightIndexById_(uint8_t id, size_t &out) const;static bool parsePort_(JsonVariantConst v, uint8_t &out);bool setupButton_(const SocketConfig &cfg, SocketState &st);bool setupRelay_(const SocketConfig &cfg, SocketState &st);bool syncButtonState_(const SocketConfig &cfg, SocketState &st);bool writeRelay_(const SocketConfig &cfg, bool on, uint32_t timeout_ms = 0xFFFFFFFFu);static constexpr bool kButtonInvert = true;
    void notifyEvent_(bool lights, uint8_t id, const String &name, bool state_on, const char *source);
    static constexpr bool kRelayInvert = false;
    static constexpr bool kButtonPullup = true;
    static constexpr uint32_t kButtonCooldownMs = 500;
};
