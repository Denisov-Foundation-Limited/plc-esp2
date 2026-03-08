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

class Sim800l
{
public:
    using CommandCallback = void (*)(void *ctx, bool ok, const String &response);
    using CommandDoneCallback = void (*)(void *ctx, bool ok, const String &response, const String &cmd);
    using LineCallback = void (*)(void *ctx, const String &line);
    using SmsIndexCallback = void (*)(void *ctx, uint16_t index);
    using CallCallback = void (*)(void *ctx, const String &number);
    using UssdCallback = void (*)(void *ctx, const String &text);
    using HttpActionCallback = void (*)(void *ctx, int status, int len);

    bool begin(Stream &ser);
    void tick();

    bool enqueueCommand(const String &cmd,
                        const char *expect = "OK",
                        uint32_t timeout_ms = 1000,
                        CommandCallback cb = nullptr,
                        void *ctx = nullptr);
    void setUrcHandler(LineCallback cb, void *ctx = nullptr);
    void setCommandDoneHandler(CommandDoneCallback cb, void *ctx = nullptr);
    void setSmsHandler(SmsIndexCallback cb, void *ctx = nullptr);
    void setCallHandler(CallCallback cb, void *ctx = nullptr);
    void setUssdHandler(UssdCallback cb, void *ctx = nullptr);
    void setHttpActionHandler(HttpActionCallback cb, void *ctx = nullptr);

    const String &lastResponse() const;

    bool sync(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool setEcho(bool on, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool setSmsTextMode(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool setCallerId(bool on, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool requestImei(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool requestImsi(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool requestOperator(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool requestSignal(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool requestRegStatus(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool enableRegUrc(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool sendUssd(const String &code, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool sendSms(const String &number, const String &text, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool dial(const String &number, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool answer(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool hangup(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool listSms(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool readSms(uint16_t index, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool deleteSms(uint16_t index, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool setApn(const String &apn,
                const String &user = "",
                const String &pass = "",
                CommandCallback cb = nullptr,
                void *ctx = nullptr);
    bool openBearer(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool closeBearer(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool getBearerIp(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool httpInit(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool httpTerm(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool httpSetCid(uint8_t cid = 1, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool httpSetUrl(const String &url, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool httpSetContentType(const String &type, CommandCallback cb = nullptr, void *ctx = nullptr);
    bool httpGet(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool httpRead(CommandCallback cb = nullptr, void *ctx = nullptr);
    bool powerDown(CommandCallback cb = nullptr, void *ctx = nullptr);

private:
    static constexpr size_t kQueueSize = 16;
    static constexpr size_t kMaxLineLen = 256;

    struct PendingCmd
    {
        String cmd;
        String expect;
        uint32_t timeout_ms = 0;
        CommandCallback cb = nullptr;
        void *ctx = nullptr;
    };

    Stream *_ser = nullptr;
    PendingCmd _queue[kQueueSize] = {};
    uint8_t _q_head = 0;
    uint8_t _q_tail = 0;
    uint8_t _q_count = 0;
    bool _waiting = false;
    uint32_t _deadline_ms = 0;
    PendingCmd _current;
    String _current_resp;
    String _last_response;
    String _line_buf;
    bool _payload_pending = false;
    uint32_t _payload_timeout_ms = 0;
    String _payload;

    LineCallback _urc_cb = nullptr;
    void *_urc_ctx = nullptr;
    CommandDoneCallback _cmd_done_cb = nullptr;
    void *_cmd_done_ctx = nullptr;
    SmsIndexCallback _sms_cb = nullptr;
    void *_sms_ctx = nullptr;
    CallCallback _call_cb = nullptr;
    void *_call_ctx = nullptr;
    UssdCallback _ussd_cb = nullptr;
    void *_ussd_ctx = nullptr;
    HttpActionCallback _http_cb = nullptr;
    void *_http_ctx = nullptr;

    static bool timePassed_(uint32_t now, uint32_t deadline);
    void startNext_();
    void finishCommand_(bool ok);
    void consumeChar_(char c);
    void handleLine_(const String &line);
    bool handleUrc_(const String &line);
    static bool isErrorLine_(const String &line);
    static bool parseIndexAfterComma_(const String &line, uint16_t &out);
    static bool parseQuoted_(const String &line, String &out);
    static bool parseHttpAction_(const String &line, int &status, int &len);
};
