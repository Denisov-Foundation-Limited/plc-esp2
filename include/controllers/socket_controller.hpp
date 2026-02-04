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
#include <string.h>

#include "hal/gpio/gpio.hpp"
#include "hal/gpio/gpio_caps.hpp"
#include "utils/logger.hpp"

class SocketController
{
public:
    static constexpr size_t kSocketCount = 72;
    static constexpr size_t kLightCount = 72;
    static constexpr uint8_t kInvalidPort = 0xFF;

    struct SocketConfig
    {
        uint8_t id = 0;
        bool enabled = false;
        uint8_t button_port = kInvalidPort;
        uint8_t relay_port = kInvalidPort;
        String name;
    };

    struct SocketState
    {
        bool relay_on = false;
        bool last_button = false;
        bool has_button = false;
    };

    using LightConfig = SocketConfig;
    using LightState = SocketState;

    SocketController(Gpio &gpio, Logger &logs) : _gpio(gpio), _logs(logs)
    {
        resetSockets_();
        resetLights_();
    }

    void applyConfig(JsonArrayConst sockets, bool legacy_lights = false)
    {
        resetSockets_();
        if (legacy_lights)
            resetLights_();
        size_t idx = 0;
        for (JsonVariantConst v : sockets)
        {
            if (idx >= kSocketCount)
                break;
            if (!v.is<JsonObjectConst>())
            {
                ++idx;
                continue;
            }
            JsonObjectConst obj = v.as<JsonObjectConst>();
            uint8_t id = (uint8_t)(idx + 1);
            if (obj["id"].is<unsigned>())
            {
                const unsigned raw = obj["id"].as<unsigned>();
                if (raw <= 0xFFu)
                    id = (uint8_t)raw;
            }
            bool use_lights = false;
            if (legacy_lights && obj["type"].is<const char *>())
            {
                const char *type = obj["type"].as<const char *>();
                if (type && strcmp(type, "light") == 0)
                    use_lights = true;
            }
            size_t dst = 0;
            if (use_lights)
            {
                if (!lightIndexById_(id, dst))
                {
                    ++idx;
                    continue;
                }
            }
            else
            {
                if (!indexById_(id, dst))
                {
                    ++idx;
                    continue;
                }
            }
            SocketConfig &cfg = use_lights ? _light_cfg[dst] : _cfg[dst];
            cfg.id = id;
            bool enabled_set = false;
            if (obj["enabled"].is<bool>())
            {
                cfg.enabled = obj["enabled"].as<bool>();
                enabled_set = true;
            }
            if (obj["name"].is<const char *>())
                cfg.name = obj["name"].as<const char *>();
            parsePort_(obj["button"], cfg.button_port);
            parsePort_(obj["relay"], cfg.relay_port);
            if (!enabled_set)
                cfg.enabled = true;
            ++idx;
        }
    }

    void applyLightsConfig(JsonArrayConst lights)
    {
        resetLights_();
        size_t idx = 0;
        for (JsonVariantConst v : lights)
        {
            if (idx >= kLightCount)
                break;
            if (!v.is<JsonObjectConst>())
            {
                ++idx;
                continue;
            }
            JsonObjectConst obj = v.as<JsonObjectConst>();
            uint8_t id = (uint8_t)(idx + 1);
            if (obj["id"].is<unsigned>())
            {
                const unsigned raw = obj["id"].as<unsigned>();
                if (raw <= 0xFFu)
                    id = (uint8_t)raw;
            }
            size_t dst = 0;
            if (!lightIndexById_(id, dst))
            {
                ++idx;
                continue;
            }
            LightConfig &cfg = _light_cfg[dst];
            cfg.id = id;
            bool enabled_set = false;
            if (obj["enabled"].is<bool>())
            {
                cfg.enabled = obj["enabled"].as<bool>();
                enabled_set = true;
            }
            if (obj["name"].is<const char *>())
                cfg.name = obj["name"].as<const char *>();
            parsePort_(obj["button"], cfg.button_port);
            parsePort_(obj["relay"], cfg.relay_port);
            if (!enabled_set)
                cfg.enabled = true;
            ++idx;
        }
    }

    bool begin()
    {
        if (!_controller_enabled && !_lights_enabled)
        {
            _logs.info(F("SOCKET"), F("Controller disabled"));
            return true;
        }
        if (_controller_enabled)
        {
            for (size_t i = 0; i < kSocketCount; ++i)
            {
                SocketConfig &cfg = _cfg[i];
                SocketState &st = _state[i];
                if (!cfg.enabled)
                    continue;

                st.has_button = setupButton_(cfg, st);
                setupRelay_(cfg, st);
            }
        }
        if (_lights_enabled)
        {
            for (size_t i = 0; i < kLightCount; ++i)
            {
                LightConfig &cfg = _light_cfg[i];
                LightState &st = _light_state[i];
                if (!cfg.enabled)
                    continue;

                st.has_button = setupButton_(cfg, st);
                setupRelay_(cfg, st);
            }
        }
        _logs.info(F("SOCKET"), F("Init done"));
        return true;
    }

    void task()
    {
        if (_controller_enabled)
        {
            for (size_t i = 0; i < kSocketCount; ++i)
            {
                SocketConfig &cfg = _cfg[i];
                SocketState &st = _state[i];
                if (!cfg.enabled || !st.has_button || cfg.relay_port == kInvalidPort)
                    continue;

                bool raw = false;
                if (!_gpio.readDyn(cfg.button_port, raw))
                    continue;
                bool pressed = kButtonInvert ? !raw : raw;
                if (pressed && !st.last_button)
                {
                    st.relay_on = !st.relay_on;
                    writeRelay_(cfg, st.relay_on);
                    _dirty_sockets = true;
                    _logs.info(F("SOCKET"), F("id: %u state: %s src: button"),
                               (unsigned)cfg.id, st.relay_on ? "on" : "off");
                }
                st.last_button = pressed;
            }
        }
        if (_lights_enabled)
        {
            for (size_t i = 0; i < kLightCount; ++i)
            {
                LightConfig &cfg = _light_cfg[i];
                LightState &st = _light_state[i];
                if (!cfg.enabled || !st.has_button || cfg.relay_port == kInvalidPort)
                    continue;

                bool raw = false;
                if (!_gpio.readDyn(cfg.button_port, raw))
                    continue;
                bool pressed = kButtonInvert ? !raw : raw;
                if (pressed && !st.last_button)
                {
                    st.relay_on = !st.relay_on;
                    writeRelay_(cfg, st.relay_on);
                    _dirty_lights = true;
                    _logs.info(F("LIGHT"), F("id: %u state: %s src: button"),
                               (unsigned)cfg.id, st.relay_on ? "on" : "off");
                }
                st.last_button = pressed;
            }
        }
    }

    bool setRelay(size_t id, bool on)
    {
        if (!_controller_enabled)
            return false;
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        SocketConfig &cfg = _cfg[idx];
        SocketState &st = _state[idx];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            return false;
        if (st.relay_on == on)
            return true;
        st.relay_on = on;
        writeRelay_(cfg, st.relay_on);
        syncButtonState_(cfg, st);
        _dirty_sockets = true;
        _logs.info(F("SOCKET"), F("id: %u state: %s"),
                   (unsigned)cfg.id, st.relay_on ? "on" : "off");
        return true;
    }

    bool toggleRelay(size_t id)
    {
        if (!_controller_enabled)
            return false;
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        SocketConfig &cfg = _cfg[idx];
        SocketState &st = _state[idx];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            return false;
        st.relay_on = !st.relay_on;
        writeRelay_(cfg, st.relay_on);
        syncButtonState_(cfg, st);
        _dirty_sockets = true;
        _logs.info(F("SOCKET"), F("id: %u state: %s src: toggle"),
                   (unsigned)cfg.id, st.relay_on ? "on" : "off");
        return true;
    }

    bool relayState(size_t id, bool &out) const
    {
        if (!_controller_enabled)
            return false;
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        const SocketConfig &cfg = _cfg[idx];
        const SocketState &st = _state[idx];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            return false;
        out = st.relay_on;
        return true;
    }

    bool setRelayById(uint8_t id, bool on) { return setRelay(id, on); }
    bool toggleRelayById(uint8_t id) { return toggleRelay(id); }
    bool relayStateById(uint8_t id, bool &out) const { return relayState(id, out); }

    bool setLightRelay(size_t id, bool on)
    {
        if (!_lights_enabled)
            return false;
        size_t idx = 0;
        if (!lightIndexById_(id, idx))
            return false;
        LightConfig &cfg = _light_cfg[idx];
        LightState &st = _light_state[idx];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            return false;
        if (st.relay_on == on)
            return true;
        st.relay_on = on;
        writeRelay_(cfg, st.relay_on);
        syncButtonState_(cfg, st);
        _dirty_lights = true;
        _logs.info(F("LIGHT"), F("id: %u state: %s"),
                   (unsigned)cfg.id, st.relay_on ? "on" : "off");
        return true;
    }

    bool toggleLightRelay(size_t id)
    {
        if (!_lights_enabled)
            return false;
        size_t idx = 0;
        if (!lightIndexById_(id, idx))
            return false;
        LightConfig &cfg = _light_cfg[idx];
        LightState &st = _light_state[idx];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            return false;
        st.relay_on = !st.relay_on;
        writeRelay_(cfg, st.relay_on);
        syncButtonState_(cfg, st);
        _dirty_lights = true;
        _logs.info(F("LIGHT"), F("id: %u state: %s src: toggle"),
                   (unsigned)cfg.id, st.relay_on ? "on" : "off");
        return true;
    }

    bool lightRelayState(size_t id, bool &out) const
    {
        if (!_lights_enabled)
            return false;
        size_t idx = 0;
        if (!lightIndexById_(id, idx))
            return false;
        const LightConfig &cfg = _light_cfg[idx];
        const LightState &st = _light_state[idx];
        if (!cfg.enabled || cfg.relay_port == kInvalidPort)
            return false;
        out = st.relay_on;
        return true;
    }

    bool setLightRelayById(uint8_t id, bool on) { return setLightRelay(id, on); }
    bool toggleLightRelayById(uint8_t id) { return toggleLightRelay(id); }
    bool lightRelayStateById(uint8_t id, bool &out) const { return lightRelayState(id, out); }

    bool setEnabled(size_t id, bool enable)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        SocketConfig &cfg = _cfg[idx];
        SocketState &st = _state[idx];
        cfg.enabled = enable;
        if (!enable)
        {
            st = SocketState{};
            return true;
        }
        st = SocketState{};
        st.has_button = setupButton_(cfg, st);
        setupRelay_(cfg, st);
        _dirty_sockets = true;
        _logs.info(F("SOCKET"), F("id: %u enabled: 1"), (unsigned)cfg.id);
        return true;
    }

    bool setButtonPort(size_t id, uint8_t port)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        SocketConfig &cfg = _cfg[idx];
        SocketState &st = _state[idx];
        cfg.button_port = port;
        if (!cfg.enabled)
            return true;
        st.has_button = setupButton_(cfg, st);
        _dirty_sockets = true;
        return true;
    }

    bool setRelayPort(size_t id, uint8_t port)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        SocketConfig &cfg = _cfg[idx];
        SocketState &st = _state[idx];
        cfg.relay_port = port;
        if (!cfg.enabled)
            return true;
        setupRelay_(cfg, st);
        _dirty_sockets = true;
        return true;
    }

    bool setName(size_t id, const String &name)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].name = name;
        _dirty_sockets = true;
        return true;
    }

    bool setLightEnabled(size_t id, bool enable)
    {
        size_t idx = 0;
        if (!lightIndexById_(id, idx))
            return false;
        LightConfig &cfg = _light_cfg[idx];
        LightState &st = _light_state[idx];
        cfg.enabled = enable;
        if (!enable)
        {
            st = LightState{};
            return true;
        }
        st = LightState{};
        st.has_button = setupButton_(cfg, st);
        setupRelay_(cfg, st);
        _dirty_lights = true;
        _logs.info(F("LIGHT"), F("id: %u enabled: 1"), (unsigned)cfg.id);
        return true;
    }

    bool setLightButtonPort(size_t id, uint8_t port)
    {
        size_t idx = 0;
        if (!lightIndexById_(id, idx))
            return false;
        LightConfig &cfg = _light_cfg[idx];
        LightState &st = _light_state[idx];
        cfg.button_port = port;
        if (!cfg.enabled)
            return true;
        st.has_button = setupButton_(cfg, st);
        _dirty_lights = true;
        return true;
    }

    bool setLightRelayPort(size_t id, uint8_t port)
    {
        size_t idx = 0;
        if (!lightIndexById_(id, idx))
            return false;
        LightConfig &cfg = _light_cfg[idx];
        LightState &st = _light_state[idx];
        cfg.relay_port = port;
        if (!cfg.enabled)
            return true;
        setupRelay_(cfg, st);
        _dirty_lights = true;
        return true;
    }

    bool setLightName(size_t id, const String &name)
    {
        size_t idx = 0;
        if (!lightIndexById_(id, idx))
            return false;
        _light_cfg[idx].name = name;
        _dirty_lights = true;
        return true;
    }

    const SocketConfig *config(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_cfg[idx];
    }

    const SocketState *state(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_state[idx];
    }

    const SocketConfig *configByIndex(size_t idx) const
    {
        if (idx >= kSocketCount)
            return nullptr;
        return &_cfg[idx];
    }

    const SocketState *stateByIndex(size_t idx) const
    {
        if (idx >= kSocketCount)
            return nullptr;
        return &_state[idx];
    }

    const LightConfig *lightConfig(size_t id) const
    {
        size_t idx = 0;
        if (!lightIndexById_(id, idx))
            return nullptr;
        return &_light_cfg[idx];
    }

    const LightState *lightState(size_t id) const
    {
        size_t idx = 0;
        if (!lightIndexById_(id, idx))
            return nullptr;
        return &_light_state[idx];
    }

    const LightConfig *lightConfigByIndex(size_t idx) const
    {
        if (idx >= kLightCount)
            return nullptr;
        return &_light_cfg[idx];
    }

    const LightState *lightStateByIndex(size_t idx) const
    {
        if (idx >= kLightCount)
            return nullptr;
        return &_light_state[idx];
    }

    void serialize(JsonArray out) const
    {
        for (size_t i = 0; i < kSocketCount; ++i)
        {
            const SocketConfig &cfg = _cfg[i];
            if (!cfg.enabled)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = cfg.id;
            obj["enabled"] = cfg.enabled;
            if (cfg.name.length())
                obj["name"] = cfg.name;
            if (cfg.button_port != kInvalidPort)
                obj["button"] = cfg.button_port;
            if (cfg.relay_port != kInvalidPort)
                obj["relay"] = cfg.relay_port;
        }
    }

    void serializeLights(JsonArray out) const
    {
        for (size_t i = 0; i < kLightCount; ++i)
        {
            const LightConfig &cfg = _light_cfg[i];
            if (!cfg.enabled)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = cfg.id;
            obj["enabled"] = cfg.enabled;
            if (cfg.name.length())
                obj["name"] = cfg.name;
            if (cfg.button_port != kInvalidPort)
                obj["button"] = cfg.button_port;
            if (cfg.relay_port != kInvalidPort)
                obj["relay"] = cfg.relay_port;
        }
    }

    void buildSnapshot(uint8_t *enabled_mask, uint8_t *state_mask, size_t bytes) const
    {
        if (!enabled_mask || !state_mask)
            return;
        memset(enabled_mask, 0, bytes);
        memset(state_mask, 0, bytes);
        for (size_t i = 0; i < kSocketCount; ++i)
        {
            const SocketConfig &cfg = _cfg[i];
            const SocketState &st = _state[i];
            if (!cfg.enabled || cfg.relay_port == kInvalidPort)
                continue;
            const size_t byte = i / 8;
            const uint8_t bit = (uint8_t)(1u << (i % 8));
            if (byte >= bytes)
                break;
            enabled_mask[byte] |= bit;
            if (st.relay_on)
                state_mask[byte] |= bit;
        }
    }

    void applySnapshot(const uint8_t *enabled_mask, const uint8_t *state_mask, size_t bytes)
    {
        if (!enabled_mask || !state_mask)
            return;
        for (size_t i = 0; i < kSocketCount; ++i)
        {
            SocketConfig &cfg = _cfg[i];
            SocketState &st = _state[i];
            if (!cfg.enabled || cfg.relay_port == kInvalidPort)
                continue;
            const size_t byte = i / 8;
            const uint8_t bit = (uint8_t)(1u << (i % 8));
            if (byte >= bytes)
                break;
            if ((enabled_mask[byte] & bit) == 0)
                continue;
            const bool on = (state_mask[byte] & bit) != 0;
            if (st.relay_on != on)
            {
                st.relay_on = on;
                writeRelay_(cfg, st.relay_on);
            }
        }
    }

    void buildLightsSnapshot(uint8_t *enabled_mask, uint8_t *state_mask, size_t bytes) const
    {
        if (!enabled_mask || !state_mask)
            return;
        memset(enabled_mask, 0, bytes);
        memset(state_mask, 0, bytes);
        for (size_t i = 0; i < kLightCount; ++i)
        {
            const LightConfig &cfg = _light_cfg[i];
            const LightState &st = _light_state[i];
            if (!cfg.enabled || cfg.relay_port == kInvalidPort)
                continue;
            const size_t byte = i / 8;
            const uint8_t bit = (uint8_t)(1u << (i % 8));
            if (byte >= bytes)
                break;
            enabled_mask[byte] |= bit;
            if (st.relay_on)
                state_mask[byte] |= bit;
        }
    }

    void applyLightsSnapshot(const uint8_t *enabled_mask, const uint8_t *state_mask, size_t bytes)
    {
        if (!enabled_mask || !state_mask)
            return;
        for (size_t i = 0; i < kLightCount; ++i)
        {
            LightConfig &cfg = _light_cfg[i];
            LightState &st = _light_state[i];
            if (!cfg.enabled || cfg.relay_port == kInvalidPort)
                continue;
            const size_t byte = i / 8;
            const uint8_t bit = (uint8_t)(1u << (i % 8));
            if (byte >= bytes)
                break;
            if ((enabled_mask[byte] & bit) == 0)
                continue;
            const bool on = (state_mask[byte] & bit) != 0;
            if (st.relay_on != on)
            {
                st.relay_on = on;
                writeRelay_(cfg, st.relay_on);
            }
        }
    }

    bool takeDirty()
    {
        if (!_dirty_sockets)
            return false;
        _dirty_sockets = false;
        return true;
    }

    bool controllerEnabled() const { return _controller_enabled; }
    bool lightsEnabled() const { return _lights_enabled; }

    void setControllerEnabled(bool enabled)
    {
        if (_controller_enabled == enabled)
            return;
        _controller_enabled = enabled;
        if (!_controller_enabled)
            return;
        for (size_t i = 0; i < kSocketCount; ++i)
        {
            SocketConfig &cfg = _cfg[i];
            SocketState &st = _state[i];
            if (!cfg.enabled)
                continue;
            st = SocketState{};
            st.has_button = setupButton_(cfg, st);
            setupRelay_(cfg, st);
        }
    }

    void setLightsEnabled(bool enabled)
    {
        if (_lights_enabled == enabled)
            return;
        _lights_enabled = enabled;
        if (!_lights_enabled)
            return;
        for (size_t i = 0; i < kLightCount; ++i)
        {
            LightConfig &cfg = _light_cfg[i];
            LightState &st = _light_state[i];
            if (!cfg.enabled)
                continue;
            st = LightState{};
            st.has_button = setupButton_(cfg, st);
            setupRelay_(cfg, st);
        }
    }

    bool takeLightsDirty()
    {
        if (!_dirty_lights)
            return false;
        _dirty_lights = false;
        return true;
    }

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

    void resetSockets_()
    {
        for (size_t i = 0; i < kSocketCount; ++i)
        {
            _cfg[i] = SocketConfig{};
            _state[i] = SocketState{};
            _cfg[i].id = (uint8_t)(i + 1);
        }
    }

    void resetLights_()
    {
        for (size_t i = 0; i < kLightCount; ++i)
        {
            _light_cfg[i] = LightConfig{};
            _light_state[i] = LightState{};
            _light_cfg[i].id = (uint8_t)(i + 1);
        }
    }

    bool indexById_(uint8_t id, size_t &out) const
    {
        if (id == 0 || id > kSocketCount)
            return false;
        const size_t direct = static_cast<size_t>(id - 1);
        if (_cfg[direct].id == id)
        {
            out = direct;
            return true;
        }
        for (size_t i = 0; i < kSocketCount; ++i)
        {
            if (_cfg[i].id == id)
            {
                out = i;
                return true;
            }
        }
        return false;
    }

    bool lightIndexById_(uint8_t id, size_t &out) const
    {
        if (id == 0 || id > kLightCount)
            return false;
        const size_t direct = static_cast<size_t>(id - 1);
        if (_light_cfg[direct].id == id)
        {
            out = direct;
            return true;
        }
        for (size_t i = 0; i < kLightCount; ++i)
        {
            if (_light_cfg[i].id == id)
            {
                out = i;
                return true;
            }
        }
        return false;
    }

    static bool parsePort_(JsonVariantConst v, uint8_t &out)
    {
        if (v.is<unsigned>())
        {
            const unsigned val = v.as<unsigned>();
            if (val <= 0xFFu)
            {
                out = static_cast<uint8_t>(val);
                return true;
            }
        }
        return false;
    }

    bool setupButton_(const SocketConfig &cfg, SocketState &st)
    {
        if (cfg.button_port == kInvalidPort)
            return false;
        const PortIO::PortMode mode = kButtonPullup ? PortIO::PortMode::InputPullUp : PortIO::PortMode::Input;
        if (!_gpio.pinModeDyn(cfg.button_port, mode))
            return false;
        bool raw = false;
        if (!_gpio.readDyn(cfg.button_port, raw))
            return false;
        st.last_button = kButtonInvert ? !raw : raw;
        return true;
    }

    bool setupRelay_(const SocketConfig &cfg, SocketState &st)
    {
        if (cfg.relay_port == kInvalidPort)
            return false;
        if (!_gpio.pinModeDyn(cfg.relay_port, PortIO::PortMode::Output))
            return false;
        st.relay_on = false;
        writeRelay_(cfg, st.relay_on);
        return true;
    }

    bool syncButtonState_(const SocketConfig &cfg, SocketState &st)
    {
        if (!st.has_button || cfg.button_port == kInvalidPort)
            return false;
        bool raw = false;
        if (!_gpio.readDyn(cfg.button_port, raw))
            return false;
        st.last_button = kButtonInvert ? !raw : raw;
        return true;
    }

    void writeRelay_(const SocketConfig &cfg, bool on)
    {
        const bool out = kRelayInvert ? !on : on;
        _gpio.writeDyn(cfg.relay_port, out);
    }

    static constexpr bool kButtonInvert = true;
    static constexpr bool kRelayInvert = false;
    static constexpr bool kButtonPullup = true;
};
