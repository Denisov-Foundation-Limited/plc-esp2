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
#include <stdint.h>

#include "hal/sim800l.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class UartManager;
class Logger;

class GsmModem
{
public:
    struct CmdLogCtx
    {
        GsmModem *self = nullptr;
        const __FlashStringHelper *name = nullptr;
    };
    GsmModem(UartManager &uart, Sim800l &modem, Logger &log);

    bool begin(uint8_t uart_index = 0, uint32_t config = SERIAL_8N1);

    void loop();

    const String &lastUrc() const;
    uint16_t lastSmsIndex() const;
    const String &lastCallNumber() const;
    const String &lastUssd() const;
    int lastHttpStatus() const;
    int lastHttpLen() const;
    const String &lastError() const;
    const String &imei() const;
    const String &imsi() const;
    const String &operatorName() const;
    const String &signalQuality() const;
    const String &regStatus() const;
    bool enabled() const;
    bool started() const;
    void setEnabled(bool enabled);
    bool sendSms(const String &number, const String &text);
    bool dial(const String &number);
    bool hangup();
    bool takeLastCall(String &out);

private:
    static void onUrc_(void *ctx, const String &line);

    static void onSms_(void *ctx, uint16_t index);

    static void onCall_(void *ctx, const String &number);

    static void onUssd_(void *ctx, const String &text);

    static void onHttpAction_(void *ctx, int status, int len);

    void bindCallbacks_();

    static void onCmdLog_(void *ctx, bool ok, const String &response);

    static void onCmdDone_(void *ctx, bool ok, const String &response, const String &cmd);

    void initSequence_();

    void logCmd_(const __FlashStringHelper *name, bool ok, const String &response);

    void handleUrc_(const String &line);

    void handleSms_(uint16_t index);

    void handleCall_(const String &number);

    void handleUssd_(const String &text);

    void handleHttpAction_(int status, int len);

    UartManager &_uart;
    Sim800l &_modem;
    Logger &_log;
    HardwareSerial *_serial = nullptr;

    String _last_urc;
    uint16_t _last_sms_index = 0;
    String _last_call;
    String _last_ussd;
    String _last_error;
    String _imei;
    String _imsi;
    String _operator_name;
    String _signal_quality;
    String _last_logged_operator;
    String _last_logged_signal;
    String _reg_status;
    uint8_t _reg_state = 0xFF;
    int _last_http_status = -1;
    int _last_http_len = -1;
    bool _enabled = false;
    bool _started = false;
    static constexpr size_t kInitCmdCount = 10;
    static constexpr uint32_t kInitTimeoutMs = 35000;
    static constexpr uint32_t kInitRetryDelayMs = 3000;
    static constexpr uint8_t kInitMaxAttempts = 3;
    CmdLogCtx _init_ctx[kInitCmdCount]{};
    bool _init_pending = false;
    bool _init_warned = false;
    uint32_t _init_started_ms = 0;
    uint8_t _init_ok = 0;
    uint8_t _init_fail = 0;
    uint8_t _init_attempt = 0;
    bool _init_retry_pending = false;
    uint32_t _init_retry_at_ms = 0;
    bool _init_logged = false;
    uint32_t _next_reg_poll_ms = 0;
    uint32_t _next_info_poll_ms = 0;
    bool _caller_id_enabled = false;
    uint32_t _next_caller_id_retry_ms = 0;
    CmdLogCtx _caller_id_ctx{};
    CmdLogCtx _reg_poll_ctx{};
    CmdLogCtx _operator_poll_ctx{};
    CmdLogCtx _signal_poll_ctx{};
    static constexpr uint32_t kCallerIdRetryMs = 5000;
    static constexpr uint32_t kRegPollMs = 10000;
    static constexpr uint32_t kInfoPollMs = 30000;
    static constexpr uint8_t kCallQueue = 4;
    String _call_queue[kCallQueue];
    uint8_t _call_head = 0;
    uint8_t _call_tail = 0;
    uint8_t _call_count = 0;
    uint8_t _timeout_streak = 0;

    SemaphoreHandle_t _modem_mtx = nullptr;

    void enqueueOrLog_(bool ok, const __FlashStringHelper *name);

    void handleCmdDone_(bool ok, const String &response, const String &cmd);

    void checkInitWatchdog_();

    void scheduleInitRetry_();

    void checkInitRetry_();
    void pollNetworkState_();
    void ensureCallerId_();

    static String firstDataLine_(const String &response);

    static String parseQuoted_(const String &line);

    static bool parseRegState_(const String &line, uint8_t &out_state);

    static const char *regStateName_(uint8_t state);

    void updateRegState_(uint8_t state);
    void logOperatorIfChanged_();
    void logSignalIfChanged_();

    void parseInitResponse_(const __FlashStringHelper *name, const String &response);

    static String cmdTimeoutLabel_(const String &cmd);

    void logInitSummary_();
    void ensureModemLock_();
    bool lockModem_(uint32_t timeout_ms = 1000);
    void unlockModem_();
};
