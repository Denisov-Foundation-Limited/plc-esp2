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

#include "controllers/meteo_controller.hpp"
#include "hal/gpio/gpio.hpp"
#include "utils/logger.hpp"

class ThermoController
{
public:
    static constexpr size_t kDeviceCount = 20;
    static constexpr uint8_t kInvalidPort = 0xFF;
    static constexpr uint8_t kInvalidSensor = 0;
    static constexpr size_t kMaskBytes = (kDeviceCount + 7) / 8;

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
        uint8_t sensor_id = kInvalidSensor;
        uint8_t heat_port = kInvalidPort;
        uint8_t cool_port = kInvalidPort;
        uint8_t button_port = kInvalidPort;
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

    ThermoController(Gpio &gpio, MeteoController &meteo, Logger *logs = nullptr)
        : _gpio(gpio), _meteo(meteo), _logs(logs)
    {
        reset_();
    }

    bool begin()
    {
        if (!_controller_enabled)
            return true;
        for (size_t i = 0; i < kDeviceCount; ++i)
        {
            DeviceConfig &cfg = _cfg[i];
            DeviceState &st = _state[i];
            if (!cfg.enabled)
                continue;
            st.has_button = setupButton_(cfg, st);
            setupRelay_(cfg, st);
        }
        return true;
    }

    void task()
    {
        if (!_controller_enabled)
            return;
        for (size_t i = 0; i < kDeviceCount; ++i)
        {
            DeviceConfig &cfg = _cfg[i];
            DeviceState &st = _state[i];
            if (!cfg.enabled)
                continue;
            if (st.has_button)
                handleButton_(cfg, st);
            updateControl_(cfg, st);
        }
    }

    void applyConfig(JsonArrayConst devices)
    {
        reset_();
        size_t idx = 0;
        for (JsonVariantConst v : devices)
        {
            if (idx >= kDeviceCount)
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
            if (!indexById_(id, dst))
            {
                ++idx;
                continue;
            }
            DeviceConfig &cfg = _cfg[dst];
            cfg.id = id;
            bool enabled_set = false;
            if (obj["enabled"].is<bool>())
            {
                cfg.enabled = obj["enabled"].as<bool>();
                enabled_set = true;
            }
            if (obj["sensor"].is<unsigned>())
            {
                const unsigned sid = obj["sensor"].as<unsigned>();
                if (sid > 0 && sid <= MeteoController::kSensorCount)
                    cfg.sensor_id = (uint8_t)sid;
            }
            parsePort_(obj["heat"], cfg.heat_port);
            parsePort_(obj["cool"], cfg.cool_port);
            parsePort_(obj["button"], cfg.button_port);
            parseFloat_(obj["target"], cfg.target_c);
            parseFloat_(obj["hyst"], cfg.hysteresis);
            if (obj["mode"].is<const char *>() || obj["mode"].is<unsigned>())
                cfg.mode = parseMode_(obj["mode"]);
            if (!enabled_set)
                cfg.enabled = true;
            ++idx;
        }
    }

    void serialize(JsonArray out) const
    {
        for (size_t i = 0; i < kDeviceCount; ++i)
        {
            const DeviceConfig &cfg = _cfg[i];
            if (!cfg.enabled)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = cfg.id;
            obj["enabled"] = cfg.enabled;
            if (cfg.sensor_id != kInvalidSensor)
                obj["sensor"] = cfg.sensor_id;
            obj["mode"] = modeName_(cfg.mode);
            obj["target"] = cfg.target_c;
            obj["hyst"] = cfg.hysteresis;
            if (cfg.heat_port != kInvalidPort)
                obj["heat"] = cfg.heat_port;
            if (cfg.cool_port != kInvalidPort)
                obj["cool"] = cfg.cool_port;
            if (cfg.button_port != kInvalidPort)
                obj["button"] = cfg.button_port;
        }
    }

    bool controllerEnabled() const { return _controller_enabled; }
    void setControllerEnabled(bool enabled)
    {
        if (_controller_enabled == enabled)
            return;
        _controller_enabled = enabled;
        if (!_controller_enabled)
        {
            for (size_t i = 0; i < kDeviceCount; ++i)
                writeOff_(_cfg[i], _state[i]);
            return;
        }
        for (size_t i = 0; i < kDeviceCount; ++i)
        {
            DeviceConfig &cfg = _cfg[i];
            DeviceState &st = _state[i];
            if (!cfg.enabled)
                continue;
            st = DeviceState{};
            st.has_button = setupButton_(cfg, st);
            setupRelay_(cfg, st);
        }
    }

    const DeviceConfig *config(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_cfg[idx];
    }

    const DeviceState *state(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_state[idx];
    }

    const DeviceConfig *configByIndex(size_t idx) const
    {
        if (idx >= kDeviceCount)
            return nullptr;
        return &_cfg[idx];
    }

    const DeviceState *stateByIndex(size_t idx) const
    {
        if (idx >= kDeviceCount)
            return nullptr;
        return &_state[idx];
    }

    void buildSnapshot(uint8_t *power_mask, size_t bytes) const
    {
        if (!power_mask)
            return;
        memset(power_mask, 0, bytes);
        for (size_t i = 0; i < kDeviceCount; ++i)
        {
            const DeviceConfig &cfg = _cfg[i];
            const DeviceState &st = _state[i];
            if (!cfg.enabled)
                continue;
            const size_t byte = i / 8;
            const uint8_t bit = (uint8_t)(1u << (i % 8));
            if (byte >= bytes)
                break;
            if (st.power_on)
                power_mask[byte] |= bit;
        }
    }

    void applySnapshot(const uint8_t *power_mask, size_t bytes)
    {
        if (!power_mask)
            return;
        for (size_t i = 0; i < kDeviceCount; ++i)
        {
            DeviceConfig &cfg = _cfg[i];
            DeviceState &st = _state[i];
            if (!cfg.enabled)
                continue;
            const size_t byte = i / 8;
            const uint8_t bit = (uint8_t)(1u << (i % 8));
            if (byte >= bytes)
                break;
            const bool on = (power_mask[byte] & bit) != 0;
            if (st.power_on != on)
            {
                st.power_on = on;
                if (!st.power_on)
                    writeOff_(cfg, st);
            }
        }
    }

    bool takeDirty()
    {
        if (!_dirty)
            return false;
        _dirty = false;
        return true;
    }

    bool setEnabled(size_t id, bool enable)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        DeviceConfig &cfg = _cfg[idx];
        DeviceState &st = _state[idx];
        cfg.enabled = enable;
        if (!enable)
        {
            writeOff_(cfg, st);
            st = DeviceState{};
            _dirty = true;
            if (_logs)
                _logs->info(F("THERMO"), F("id=%u enabled=0"), (unsigned)cfg.id);
            return true;
        }
        st = DeviceState{};
        st.has_button = setupButton_(cfg, st);
        setupRelay_(cfg, st);
        _dirty = true;
        if (_logs)
            _logs->info(F("THERMO"), F("id=%u enabled=1"), (unsigned)cfg.id);
        return true;
    }

    bool setSensor(size_t id, uint8_t sensor_id)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        if (sensor_id > MeteoController::kSensorCount)
            return false;
        _cfg[idx].sensor_id = sensor_id;
        return true;
    }

    bool setMode(size_t id, Mode mode)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].mode = mode;
        return true;
    }

    bool setTarget(size_t id, float target_c)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].target_c = target_c;
        return true;
    }

    bool setHysteresis(size_t id, float hyst)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        if (hyst < 0.0f)
            hyst = -hyst;
        _cfg[idx].hysteresis = hyst;
        return true;
    }

    bool setPower(size_t id, bool on, const char *src = nullptr)
    {
        if (!_controller_enabled)
            return false;
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        DeviceConfig &cfg = _cfg[idx];
        DeviceState &st = _state[idx];
        if (!cfg.enabled)
            return false;
        if (st.power_on == on)
            return true;
        st.power_on = on;
        if (!st.power_on)
            writeOff_(cfg, st);
        else
            updateControl_(cfg, st);
        _dirty = true;
        if (_logs)
        {
            if (src && src[0] != '\0')
                _logs->info(F("THERMO"), F("id=%u power=%s src=%s"),
                            (unsigned)cfg.id, st.power_on ? "on" : "off", src);
            else
                _logs->info(F("THERMO"), F("id=%u power=%s"),
                            (unsigned)cfg.id, st.power_on ? "on" : "off");
        }
        return true;
    }

    bool togglePower(size_t id, const char *src = nullptr)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        DeviceState &st = _state[idx];
        return setPower(id, !st.power_on, src);
    }

    bool setHeatPort(size_t id, uint8_t port)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        DeviceConfig &cfg = _cfg[idx];
        DeviceState &st = _state[idx];
        cfg.heat_port = port;
        if (!cfg.enabled)
            return true;
        setupRelay_(cfg, st);
        return true;
    }

    bool setCoolPort(size_t id, uint8_t port)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        DeviceConfig &cfg = _cfg[idx];
        DeviceState &st = _state[idx];
        cfg.cool_port = port;
        if (!cfg.enabled)
            return true;
        setupRelay_(cfg, st);
        return true;
    }

    bool setButtonPort(size_t id, uint8_t port)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        DeviceConfig &cfg = _cfg[idx];
        DeviceState &st = _state[idx];
        cfg.button_port = port;
        if (!cfg.enabled)
            return true;
        st.has_button = setupButton_(cfg, st);
        return true;
    }

    static const char *modeName(Mode mode) { return modeName_(mode); }

private:
    Gpio &_gpio;
    MeteoController &_meteo;
    Logger *_logs = nullptr;
    DeviceConfig _cfg[kDeviceCount];
    DeviceState _state[kDeviceCount];
    bool _controller_enabled = false;
    bool _dirty = false;

    static constexpr bool kButtonInvert = true;
    static constexpr bool kButtonPullup = true;
    static constexpr bool kHeatInvert = false;
    static constexpr bool kCoolInvert = false;

    void reset_()
    {
        for (size_t i = 0; i < kDeviceCount; ++i)
        {
            _cfg[i] = DeviceConfig{};
            _state[i] = DeviceState{};
            _cfg[i].id = (uint8_t)(i + 1);
        }
    }

    bool indexById_(uint8_t id, size_t &out) const
    {
        if (id == 0 || id > kDeviceCount)
            return false;
        const size_t direct = static_cast<size_t>(id - 1);
        if (_cfg[direct].id == id)
        {
            out = direct;
            return true;
        }
        for (size_t i = 0; i < kDeviceCount; ++i)
        {
            if (_cfg[i].id == id)
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

    static bool parseFloat_(JsonVariantConst v, float &out)
    {
        if (v.is<float>() || v.is<double>() || v.is<long>() || v.is<int>())
        {
            out = v.as<float>();
            return true;
        }
        return false;
    }

    static Mode parseMode_(JsonVariantConst v)
    {
        if (v.is<unsigned>())
        {
            const unsigned raw = v.as<unsigned>();
            if (raw <= (unsigned)Mode::Auto)
                return (Mode)raw;
            return Mode::Off;
        }
        if (v.is<const char *>())
        {
            String t = v.as<const char *>();
            t.toLowerCase();
            if (t == "heat" || t == "heat_only" || t == "only_heat")
                return Mode::Heat;
            if (t == "cool" || t == "cool_only" || t == "only_cool")
                return Mode::Cool;
            if (t == "auto")
                return Mode::Auto;
            if (t == "off" || t == "none")
                return Mode::Off;
        }
        return Mode::Off;
    }

    static const char *modeName_(Mode mode)
    {
        switch (mode)
        {
        case Mode::Heat:
            return "heat_only";
        case Mode::Cool:
            return "cool_only";
        case Mode::Auto:
            return "auto";
        case Mode::Off:
        default:
            return "off";
        }
    }

    bool setupButton_(const DeviceConfig &cfg, DeviceState &st)
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

    bool setupRelay_(const DeviceConfig &cfg, DeviceState &st)
    {
        bool ok = true;
        if (cfg.heat_port != kInvalidPort)
        {
            if (!_gpio.pinModeDyn(cfg.heat_port, PortIO::PortMode::Output))
                ok = false;
        }
        if (cfg.cool_port != kInvalidPort)
        {
            if (!_gpio.pinModeDyn(cfg.cool_port, PortIO::PortMode::Output))
                ok = false;
        }
        writeOff_(cfg, st);
        return ok;
    }

    void handleButton_(const DeviceConfig &cfg, DeviceState &st)
    {
        bool raw = false;
        if (!_gpio.readDyn(cfg.button_port, raw))
            return;
        const bool pressed = kButtonInvert ? !raw : raw;
        if (pressed && !st.last_button)
        {
            st.power_on = !st.power_on;
            if (!st.power_on)
                writeOff_(cfg, st);
            _dirty = true;
            if (_logs)
                _logs->info(F("THERMO"), F("id=%u power=%s src=button"),
                            (unsigned)cfg.id, st.power_on ? "on" : "off");
        }
        st.last_button = pressed;
    }

    void updateControl_(const DeviceConfig &cfg, DeviceState &st)
    {
        if (!st.power_on || cfg.mode == Mode::Off)
        {
            const bool changed = writeOff_(cfg, st);
            if (changed && _logs)
                _logs->info(F("THERMO"), F("id=%u power=%s heat=%s cool=%s"),
                            (unsigned)cfg.id, st.power_on ? "on" : "off",
                            st.heat_on ? "on" : "off", st.cool_on ? "on" : "off");
            return;
        }
        if (cfg.sensor_id == kInvalidSensor)
        {
            const bool changed = writeOff_(cfg, st);
            if (changed && _logs)
                _logs->info(F("THERMO"), F("id=%u sensor=none heat=%s cool=%s"),
                            (unsigned)cfg.id, st.heat_on ? "on" : "off", st.cool_on ? "on" : "off");
            return;
        }
        const auto *sensor = _meteo.state(cfg.sensor_id);
        if (!sensor || !sensor->has_temp)
        {
            const bool changed = writeOff_(cfg, st);
            if (changed && _logs)
                _logs->info(F("THERMO"), F("id=%u temp=na heat=%s cool=%s"),
                            (unsigned)cfg.id, st.heat_on ? "on" : "off", st.cool_on ? "on" : "off");
            return;
        }
        const float t = sensor->temp_c;
        const float lo = cfg.target_c - cfg.hysteresis;
        const float hi = cfg.target_c + cfg.hysteresis;

        bool heat = st.heat_on;
        bool cool = st.cool_on;

        if (cfg.mode == Mode::Heat)
        {
            if (t <= lo)
                heat = true;
            else if (t >= hi)
                heat = false;
            cool = false;
        }
        else if (cfg.mode == Mode::Cool)
        {
            if (t >= hi)
                cool = true;
            else if (t <= lo)
                cool = false;
            heat = false;
        }
        else if (cfg.mode == Mode::Auto)
        {
            if (t < lo)
            {
                heat = true;
                cool = false;
            }
            else if (t > hi)
            {
                heat = false;
                cool = true;
            }
            else
            {
                heat = false;
                cool = false;
            }
        }

        const bool changed = writeOutputs_(cfg, st, heat, cool);
        if (changed && _logs)
            _logs->info(F("THERMO"), F("id=%u mode=%s temp=%.2f heat=%s cool=%s"),
                        (unsigned)cfg.id, modeName_(cfg.mode), t,
                        st.heat_on ? "on" : "off", st.cool_on ? "on" : "off");
    }

    bool writeOff_(const DeviceConfig &cfg, DeviceState &st)
    {
        return writeOutputs_(cfg, st, false, false);
    }

    bool writeOutputs_(const DeviceConfig &cfg, DeviceState &st, bool heat, bool cool)
    {
        bool changed = false;
        if (heat && cool)
            cool = false;
        if (cfg.heat_port != kInvalidPort && st.heat_on != heat)
        {
            const bool out = kHeatInvert ? !heat : heat;
            _gpio.writeDyn(cfg.heat_port, out);
            st.heat_on = heat;
            changed = true;
        }
        if (cfg.cool_port != kInvalidPort && st.cool_on != cool)
        {
            const bool out = kCoolInvert ? !cool : cool;
            _gpio.writeDyn(cfg.cool_port, out);
            st.cool_on = cool;
            changed = true;
        }
        if (cfg.heat_port == kInvalidPort)
        {
            if (st.heat_on)
                changed = true;
            st.heat_on = false;
        }
        if (cfg.cool_port == kInvalidPort)
        {
            if (st.cool_on)
                changed = true;
            st.cool_on = false;
        }
        return changed;
    }
};
