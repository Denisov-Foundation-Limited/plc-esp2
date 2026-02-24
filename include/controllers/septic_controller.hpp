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

#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/telegram/telegram_allowed_users.hpp"
#include "hal/gpio/gpio.hpp"
#include "hal/gpio/portio.hpp"
#include "utils/logger.hpp"

class SepticController
{
public:
    static constexpr size_t kSepticCount = 1;
    static constexpr uint8_t kInvalidPort = 0xFF;

    struct SepticConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        bool monitoring_on = false;
        uint8_t warning_port = kInvalidPort;
        uint8_t alarm_port = kInvalidPort;
        uint8_t relay_warning = kInvalidPort;
        uint8_t relay_alarm = kInvalidPort;
        String name;
    };

    struct SepticState
    {
        bool warning = false;
        bool alarm = false;
        bool relay_warning = false;
        bool relay_alarm = false;
        bool last_warning = false;
        bool last_alarm = false;
    };

    using DetectHandler = void (*)(void *ctx, uint8_t septic_id, const String &name, bool is_alarm);

    SepticController(Gpio &gpio, Logger &logs, TelegramBot &bot, TelegramAllowedUsersProvider &users)
        ;bool begin();void task();void applyConfig(JsonArrayConst septic);void serialize(JsonArray out) const;bool controllerEnabled() const;void setControllerEnabled(bool enabled);bool setEnabled(size_t id, bool enabled);bool setMonitoring(size_t id, bool on);bool setName(size_t id, const String &name);bool setWarningPort(size_t id, uint8_t port);bool setAlarmPort(size_t id, uint8_t port);bool setWarningRelay(size_t id, uint8_t port);bool setAlarmRelay(size_t id, uint8_t port);void setDetectHandler(DetectHandler cb, void *ctx);void setNotifyEnabled(bool enabled);void notifyRemoteLevel(const String &source, uint8_t septic_id, const String &name, bool is_alarm);const SepticConfig *configByIndex(size_t idx) const;const SepticState *stateByIndex(size_t idx) const;private:
    Gpio &_gpio;
    Logger &_logs;
    TelegramBot &_tgbot;
    TelegramAllowedUsersProvider &_tgusers;

    SepticConfig _cfg[kSepticCount]{};
    SepticState _state[kSepticCount]{};
    bool _controller_enabled = false;
    bool _notify_enabled = true;
    DetectHandler _detect_cb = nullptr;
    void *_detect_ctx = nullptr;

    void reset_();static bool indexById_(uint8_t id, size_t &out);void setupInputs_(const SepticConfig &cfg);void setupInput_(uint8_t port);void setupOutputs_(const SepticConfig &cfg, SepticState &st);void setupRelay_(uint8_t port, bool &state);void readLevels_(const SepticConfig &cfg, SepticState &st);bool setLevelPort_(size_t id, uint8_t port, uint8_t SepticConfig::*field);bool setRelayPort_(size_t id, uint8_t port, uint8_t SepticConfig::*field);bool readInput_(uint8_t port);void updateRelays_(const SepticConfig &cfg, SepticState &st);void writeRelay_(uint8_t port, bool on);void logLevelChange_(const SepticConfig &cfg, const SepticState &prev, const SepticState &curr);void logRelayChange_(const SepticConfig &cfg, const SepticState &prev, const SepticState &curr);void notifyLevel_(const SepticConfig &cfg, bool is_alarm);void sendTgNotify_(const String &msg);void notifyDetectEvent_(const SepticConfig &cfg, bool is_alarm);static bool parsePort_(JsonVariantConst v, uint8_t &out);static constexpr bool kRelayInvert = false;
    static constexpr bool kLevelPullup = true;
};

