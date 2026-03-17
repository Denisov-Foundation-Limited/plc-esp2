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

#include "core/network/telegram/telegram_allowed_users.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "hal/gpio/gpio.hpp"
#include "utils/logger.hpp"
#include "utils/rtos_lock.hpp"

class AvrController
{
public:
    static constexpr uint8_t kInvalidPort = 0xFF;
    using LockGuard = RtosRecursiveLock::Guard;

    enum class Source : uint8_t
    {
        Off = 0,
        Main = 1,
        Reserve = 2
    };

    enum class Fault : uint8_t
    {
        None = 0,
        NoSource,
        TransferTimeout,
        Interlock,
        FeedbackMismatch
    };

    enum class TransferState : uint8_t
    {
        Idle = 0,
        Break,
        Warmup,
        Apply
    };

    struct Config
    {
        bool enabled = false;
        bool auto_mode = true;
        bool prefer_main = true;
        bool auto_return_main = true;

        uint8_t main_ok_port = kInvalidPort;
        uint8_t reserve_ok_port = kInvalidPort;
        uint8_t relay_main_port = kInvalidPort;
        uint8_t relay_reserve_port = kInvalidPort;
        uint8_t feedback_main_port = kInvalidPort;
        uint8_t feedback_reserve_port = kInvalidPort;

        bool main_ok_active_low = true;
        bool reserve_ok_active_low = true;
        bool feedback_main_active_low = true;
        bool feedback_reserve_active_low = true;
        bool relay_main_invert = false;
        bool relay_reserve_invert = false;

        uint32_t debounce_ms = 500;
        uint32_t loss_delay_ms = 1500;
        uint32_t return_delay_ms = 5000;
        uint32_t break_ms = 250;
        uint32_t warmup_ms = 1500;
        uint32_t transfer_timeout_ms = 15000;
    };

    struct State
    {
        bool main_ok = false;
        bool reserve_ok = false;
        bool fb_main_on = false;
        bool fb_reserve_on = false;
        bool relay_main_on = false;
        bool relay_reserve_on = false;

        bool transfer_in_progress = false;
        Source active_source = Source::Off;
        Source target_source = Source::Off;
        Source manual_source = Source::Off;
        TransferState transfer_state = TransferState::Idle;

        Fault fault = Fault::None;
        uint32_t fault_ms = 0;
        uint32_t transfer_start_ms = 0;
        uint32_t transfer_step_ms = 0;
        uint32_t source_since_ms = 0;
        uint32_t switch_count = 0;
    };

    AvrController(Gpio &gpio, Logger &logs, TelegramBot &bot, TelegramAllowedUsersProvider &users)
        ;bool begin();void task();void applyConfig(JsonObjectConst obj);void serialize(JsonObject out) const;bool setControllerEnabled(bool enabled);bool controllerEnabled() const;bool setAutoMode(bool auto_mode);bool autoMode() const;bool setPreferMain(bool prefer_main);bool preferMain() const;bool setAutoReturnMain(bool auto_return);bool autoReturnMain() const;bool setManualSource(Source src);Source manualSource() const;Source activeSource() const;const Config &config() const;const State &state() const;bool setMainOkPort(uint8_t port);bool setReserveOkPort(uint8_t port);bool setRelayMainPort(uint8_t port);bool setRelayReservePort(uint8_t port);bool setFeedbackMainPort(uint8_t port);bool setFeedbackReservePort(uint8_t port);bool transferInProgress() const;Fault fault() const;void clearFault();static const char *sourceName(Source s);static const char *faultName(Fault f);LockGuard lockGuard(uint32_t timeout_ms = 0xFFFFFFFFu) const { return _lock.guard(timeout_ms); }
#if RTOS_LOCK_DIAG
    const char *lockOwnerName() const { return _lock.ownerName(); }
    uint32_t lockHeldMs() const { return _lock.heldMs(); }
#endif
private:
    struct InputSample
    {
        bool has_main_ok = false;
        bool main_ok = false;
        bool has_reserve_ok = false;
        bool reserve_ok = false;
        bool has_fb_main = false;
        bool fb_main = false;
        bool has_fb_reserve = false;
        bool fb_reserve = false;
    };

    struct InputDebounce
    {
        bool stable = false;
        bool raw = false;
        uint32_t raw_since_ms = 0;
        bool initialized = false;
    };

    Gpio &_gpio;
    Logger &_logs;
    TelegramBot &_tgbot;
    TelegramAllowedUsersProvider &_tgusers;
    Config _cfg{};
    State _st{};
    InputDebounce _main_ok_db{};
    InputDebounce _reserve_ok_db{};
    InputDebounce _fb_main_db{};
    InputDebounce _fb_reserve_db{};
    uint32_t _main_lost_since_ms = 0;
    uint32_t _main_ok_since_ms = 0;
    bool _main_state_known = false;
    bool _last_main_ok = false;
    String _pending_main_notify;
    String _pending_source_notify;
    mutable RtosRecursiveLock _lock;

    void setupHardware_();void setupInputPort_(uint8_t port);void setupRelayPort_(uint8_t port);static bool parsePort_(JsonVariantConst v, uint8_t &out);static void parseMs_(JsonVariantConst v, uint32_t &out);bool readInput_(uint8_t port, bool active_low, bool &out) const;bool updateDebounce_(InputDebounce &db, bool value, uint32_t now);void sampleInputs_(const Config &cfg, InputSample &sample) const;void applyInputSample_(const InputSample &sample, uint32_t now);void setRelays_(bool main_on, bool reserve_on);void setFault_(Fault f);Source decideAutoSource_(uint32_t now);void startTransfer_(Source target, uint32_t now);void processTransfer_(uint32_t now);void notifyMainStateIfChanged_();void notifySourceSwitched_(Source from, Source to);void sendTgNotify_(const String &msg);};
