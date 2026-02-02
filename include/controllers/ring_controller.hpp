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
#include "hal/gpio/gpio_caps.hpp"
#include "utils/logger.hpp"

class RingController
{
public:
    static constexpr uint8_t kInvalidPort = 0xFF;
    struct Config
    {
        bool enabled = false;
        uint8_t button_port = kInvalidPort;
        uint8_t relay_port = kInvalidPort;
    };

    struct State
    {
        bool relay_on = false;
        bool hold_active = false;
        bool last_button = false;
        bool has_button = false;
        bool has_relay = false;
        uint32_t cooldown_until_ms = 0;
        uint8_t last_source = 0;
    };

    using HoldHandler = void (*)(void *ctx, bool on);

    enum class Source : uint8_t
    {
        Unknown = 0,
        Button = 1,
        Web = 2,
        Cli = 3,
        Stack = 4
    };

    RingController(Gpio &gpio, Logger &logs) : _gpio(gpio), _logs(logs)
    {
    }

    bool begin()
    {
        setupHardware_();
        return true;
    }

    void task()
    {
        if (!_cfg.enabled)
        {
            ensureRelayOff_();
            return;
        }
        handleButton_();
    }

    void applyConfig(JsonObjectConst obj)
    {
        Config next = _cfg;
        if (obj["enabled"].is<bool>())
            next.enabled = obj["enabled"].as<bool>();
        parsePort_(obj["button"], next.button_port);
        parsePort_(obj["relay"], next.relay_port);
        _cfg = next;
        setupHardware_();
    }

    void serialize(JsonObject out) const
    {
        out["enabled"] = _cfg.enabled;
        if (_cfg.button_port != kInvalidPort)
            out["button"] = _cfg.button_port;
        if (_cfg.relay_port != kInvalidPort)
            out["relay"] = _cfg.relay_port;
    }

    bool setControllerEnabled(bool enabled)
    {
        if (_cfg.enabled == enabled)
            return false;
        _cfg.enabled = enabled;
        if (!enabled)
            ensureRelayOff_();
        return true;
    }

    bool controllerEnabled() const { return _cfg.enabled; }

    bool setButtonPort(uint8_t port)
    {
        if (_cfg.button_port == port)
            return false;
        _cfg.button_port = port;
        _st.has_button = setupButton_();
        return true;
    }

    bool setRelayPort(uint8_t port)
    {
        if (_cfg.relay_port == port)
            return false;
        _cfg.relay_port = port;
        _st.has_relay = setupRelay_();
        return true;
    }

    bool setHoldRelay(bool on)
    {
        return setHoldRelay_(on, true, Source::Unknown);
    }

    bool setHoldRelayLocal(bool on)
    {
        return setHoldRelay_(on, false, Source::Unknown);
    }

    bool setHoldRelayWithSource(bool on, Source source)
    {
        return setHoldRelay_(on, true, source);
    }

    bool setHoldRelayLocalWithSource(bool on, Source source)
    {
        return setHoldRelay_(on, false, source);
    }

    const Config &config() const { return _cfg; }
    const State &state() const { return _st; }
    Source lastSource() const { return static_cast<Source>(_st.last_source); }

    void setHoldHandler(HoldHandler cb, void *ctx)
    {
        _hold_cb = cb;
        _hold_ctx = ctx;
    }

private:
    Gpio &_gpio;
    Logger &_logs;
    Config _cfg;
    State _st;
    HoldHandler _hold_cb = nullptr;
    void *_hold_ctx = nullptr;

    void setupHardware_()
    {
        _st.has_button = setupButton_();
        _st.has_relay = setupRelay_();
        if (!_cfg.enabled)
            ensureRelayOff_();
    }

    void ensureRelayOff_()
    {
        if (_st.hold_active)
            setHoldActive_(false, false);
        if (_st.relay_on)
        {
            _st.relay_on = false;
            writeRelay_(false);
        }
    }

    void handleButton_()
    {
        if (!_st.has_button)
            return;
        bool raw = false;
        if (!_gpio.readDyn(_cfg.button_port, raw))
            return;
        const bool pressed = kButtonActiveLow ? !raw : raw;
        const uint32_t now = millis();
        if (pressed)
        {
            if (_st.cooldown_until_ms != 0 && (int32_t)(now - _st.cooldown_until_ms) < 0)
            {
                _st.last_button = pressed;
                return;
            }
            _st.last_source = static_cast<uint8_t>(Source::Button);
            setHoldActive_(true, true);
        }
        else if (_st.last_button)
        {
            setHoldActive_(false, true);
            _st.cooldown_until_ms = now + kReleaseCooldownMs;
        }
        _st.last_button = pressed;
    }

    void setHoldActive_(bool on, bool notify)
    {
        if (_st.hold_active == on)
            return;
        _st.hold_active = on;
        _st.relay_on = on;
        writeRelay_(on);
        if (notify && _hold_cb)
            _hold_cb(_hold_ctx, on);
    }

    bool setHoldRelay_(bool on, bool notify, Source source)
    {
        if (!_cfg.enabled || _cfg.relay_port == kInvalidPort)
            return false;
        const uint32_t now = millis();
        if (on)
        {
            if (_st.cooldown_until_ms != 0 && (int32_t)(now - _st.cooldown_until_ms) < 0)
                return false;
            _st.last_source = static_cast<uint8_t>(source);
        }
        const bool was_on = _st.hold_active;
        setHoldActive_(on, notify);
        if (!on && was_on)
            _st.cooldown_until_ms = now + kReleaseCooldownMs;
        return true;
    }

    bool setupButton_()
    {
        if (_cfg.button_port == kInvalidPort)
            return false;
        const Cap caps = _gpio.capsDyn(_cfg.button_port);
        PortIO::PortMode mode = PortIO::PortMode::Input;
        if (kButtonPullup && has(caps, Cap::PullUp))
            mode = PortIO::PortMode::InputPullUp;
        if (!_gpio.pinModeDyn(_cfg.button_port, mode))
        {
            if (mode != PortIO::PortMode::Input)
            {
                mode = PortIO::PortMode::Input;
                if (!_gpio.pinModeDyn(_cfg.button_port, mode))
                    return false;
            }
            else
            {
                return false;
            }
        }
        bool raw = false;
        if (!_gpio.readDyn(_cfg.button_port, raw))
            return false;
        _st.last_button = kButtonActiveLow ? !raw : raw;
        return true;
    }

    bool setupRelay_()
    {
        if (_cfg.relay_port == kInvalidPort)
            return false;
        if (!_gpio.pinModeDyn(_cfg.relay_port, PortIO::PortMode::Output))
            return false;
        _st.relay_on = false;
        writeRelay_(false);
        return true;
    }

    void writeRelay_(bool on)
    {
        if (_cfg.relay_port == kInvalidPort)
            return;
        const bool out = kRelayInvert ? !on : on;
        _gpio.writeDyn(_cfg.relay_port, out);
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

    static constexpr bool kButtonActiveLow = false;
    static constexpr bool kRelayInvert = false;
    static constexpr bool kButtonPullup = true;
    static constexpr uint32_t kReleaseCooldownMs = 1000;
};
