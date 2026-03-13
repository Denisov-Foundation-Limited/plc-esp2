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

class RingController
{
public:
    static constexpr uint8_t kInvalidPort = 0xFF;
    using LockGuard = RtosRecursiveLock::Guard;
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
        uint32_t stack_hold_until_ms = 0;
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

    RingController(Gpio &gpio, Logger &logs) ;bool begin();void task();void applyConfig(JsonObjectConst obj);void serialize(JsonObject out) const;bool setControllerEnabled(bool enabled);bool controllerEnabled() const;bool setButtonPort(uint8_t port);bool setRelayPort(uint8_t port);bool setHoldRelay(bool on);bool setHoldRelayLocal(bool on);bool setHoldRelayWithSource(bool on, Source source);bool setHoldRelayLocalWithSource(bool on, Source source);const Config &config() const;const State &state() const;Source lastSource() const;void setHoldHandler(HoldHandler cb, void *ctx);LockGuard lockGuard() const { return _lock.guard(); }
private:
    Gpio &_gpio;
    Logger &_logs;
    Config _cfg;
    State _st;
    HoldHandler _hold_cb = nullptr;
    void *_hold_ctx = nullptr;
    mutable RtosRecursiveLock _lock;

    void setupHardware_();void ensureRelayOff_();void handleButton_();void setHoldActive_(bool on, bool notify);bool setHoldRelay_(bool on, bool notify, Source source);void pollStackHoldTimeout_();bool setupButton_();bool setupRelay_();void writeRelay_(bool on);static bool parsePort_(JsonVariantConst v, uint8_t &out);static constexpr bool kButtonActiveLow = false;
    static constexpr bool kRelayInvert = false;
    static constexpr bool kButtonPullup = true;
    static constexpr uint32_t kReleaseCooldownMs = 1000;
    static constexpr uint32_t kStackHoldTimeoutMs = 10000;
};
