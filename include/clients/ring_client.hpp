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

#include "hal/gpio/gpio.hpp"
#include "hal/gpio/gpio_caps.hpp"
#include "utils/logger.hpp"

class RingClient
{
public:
    static constexpr uint8_t kInvalidPort = 0xFF;

    using ButtonHandler = void (*)(void *ctx, bool pressed);

    RingClient(Gpio &gpio, Logger &logs) : _gpio(gpio), _logs(logs) {}

    void setEnabled(bool enabled) { _enabled = enabled; }
    bool enabled() const { return _enabled; }

    bool setButtonPort(uint8_t port)
    {
        if (_button_port == port)
            return false;
        _button_port = port;
        _has_button = setupButton_();
        return true;
    }

    uint8_t buttonPort() const { return _button_port; }

    void setButtonHandler(ButtonHandler cb, void *ctx)
    {
        _btn_cb = cb;
        _btn_ctx = ctx;
    }

    void task()
    {
        if (!_enabled || !_has_button)
            return;
        bool raw = false;
        if (!_gpio.readDyn(_button_port, raw))
            return;
        const bool pressed = kButtonActiveLow ? !raw : raw;
        const uint32_t now = millis();
        if (pressed)
        {
            if (_last_press_ms != 0 && (uint32_t)(now - _last_press_ms) < kPressCooldownMs)
            {
                _last_button = pressed;
                return;
            }
            if (_cooldown_until_ms != 0 && (int32_t)(now - _cooldown_until_ms) < 0)
            {
                _last_button = pressed;
                return;
            }
            if (!_last_button)
            {
                _last_press_ms = now;
                notify_(true);
            }
        }
        else if (_last_button)
        {
            notify_(false);
            _cooldown_until_ms = now + kReleaseCooldownMs;
        }
        _last_button = pressed;
    }

private:
    Gpio &_gpio;
    Logger &_logs;
    bool _enabled = false;
    uint8_t _button_port = kInvalidPort;
    bool _has_button = false;
    bool _last_button = false;
    uint32_t _last_press_ms = 0;
    uint32_t _cooldown_until_ms = 0;
    ButtonHandler _btn_cb = nullptr;
    void *_btn_ctx = nullptr;

    bool setupButton_()
    {
        if (_button_port == kInvalidPort)
            return false;
        const Cap caps = _gpio.capsDyn(_button_port);
        PortIO::PortMode mode = PortIO::PortMode::Input;
        if (kButtonPullup && has(caps, Cap::PullUp))
            mode = PortIO::PortMode::InputPullUp;
        if (!_gpio.pinModeDyn(_button_port, mode))
        {
            if (mode != PortIO::PortMode::Input)
            {
                mode = PortIO::PortMode::Input;
                if (!_gpio.pinModeDyn(_button_port, mode))
                    return false;
            }
            else
            {
                return false;
            }
        }
        bool raw = false;
        if (!_gpio.readDyn(_button_port, raw))
            return false;
        _last_button = kButtonActiveLow ? !raw : raw;
        return true;
    }

    void notify_(bool pressed)
    {
        if (pressed)
            _logs.info(F("RING"), F("client button pressed"));
        if (_btn_cb)
            _btn_cb(_btn_ctx, pressed);
    }

    static constexpr bool kButtonActiveLow = false;
    static constexpr bool kButtonPullup = true;
    static constexpr uint32_t kPressCooldownMs = 1000;
    static constexpr uint32_t kReleaseCooldownMs = 1000;
};
