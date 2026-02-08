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

    bool begin(Stream &ser)
    {
        _ser = &ser;
        return true;
    }

    void tick()
    {
        if (!_ser)
            return;
        while (_ser->available())
        {
            char c = (char)_ser->read();
            consumeChar_(c);
        }

        const uint32_t now = millis();
        if (_waiting && timePassed_(now, _deadline_ms))
            finishCommand_(false);

        if (!_waiting && _q_count > 0)
            startNext_();
    }

    bool enqueueCommand(const String &cmd,
                        const char *expect = "OK",
                        uint32_t timeout_ms = 1000,
                        CommandCallback cb = nullptr,
                        void *ctx = nullptr)
    {
        if (_q_count >= kQueueSize)
            return false;
        PendingCmd &dst = _queue[_q_tail];
        dst.cmd = cmd;
        dst.expect = expect ? String(expect) : String("");
        dst.timeout_ms = timeout_ms;
        dst.cb = cb;
        dst.ctx = ctx;
        _q_tail = (uint8_t)((_q_tail + 1) % kQueueSize);
        ++_q_count;
        return true;
    }
    void setUrcHandler(LineCallback cb, void *ctx = nullptr)
    {
        _urc_cb = cb;
        _urc_ctx = ctx;
    }


    void setCommandDoneHandler(CommandDoneCallback cb, void *ctx = nullptr)
    {
        _cmd_done_cb = cb;
        _cmd_done_ctx = ctx;
    }

    void setSmsHandler(SmsIndexCallback cb, void *ctx = nullptr)
    {
        _sms_cb = cb;
        _sms_ctx = ctx;
    }

    void setCallHandler(CallCallback cb, void *ctx = nullptr)
    {
        _call_cb = cb;
        _call_ctx = ctx;
    }

    void setUssdHandler(UssdCallback cb, void *ctx = nullptr)
    {
        _ussd_cb = cb;
        _ussd_ctx = ctx;
    }

    void setHttpActionHandler(HttpActionCallback cb, void *ctx = nullptr)
    {
        _http_cb = cb;
        _http_ctx = ctx;
    }

    const String &lastResponse() const { return _last_response; }

    bool sync(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT", "OK", 1000, cb, ctx);
    }

    bool setEcho(bool on, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand(on ? "ATE1" : "ATE0", "OK", 1000, cb, ctx);
    }

    bool setSmsTextMode(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+CMGF=1", "OK", 1000, cb, ctx);
    }

    bool setCallerId(bool on, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand(on ? "AT+CLIP=1" : "AT+CLIP=0", "OK", 1000, cb, ctx);
    }

    bool requestImei(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+GSN", "OK", 1000, cb, ctx);
    }

    bool requestImsi(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+CIMI", "OK", 1000, cb, ctx);
    }

    bool requestOperator(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+COPS?", "OK", 1000, cb, ctx);
    }

    bool requestSignal(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+CSQ", "OK", 1000, cb, ctx);
    }

    bool requestRegStatus(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+CREG?", "OK", 1000, cb, ctx);
    }

    bool enableRegUrc(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+CREG=1", "OK", 1000, cb, ctx);
    }

    bool sendUssd(const String &code, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        const String cmd = String("AT+CUSD=1,\"") + code + "\",15";
        return enqueueCommand(cmd, "+CUSD:", 10000, cb, ctx);
    }

    bool sendSms(const String &number, const String &text, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        if (number.length() == 0)
            return false;
        _payload = text;
        _payload += "\x1A";
        _payload_pending = true;
        _payload_timeout_ms = 15000;
        const String cmd = String("AT+CMGS=\"") + number + "\"";
        return enqueueCommand(cmd, ">", 3000, cb, ctx);
    }

    bool dial(const String &number, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand(String("ATD") + number + ";", "OK", 5000, cb, ctx);
    }

    bool answer(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("ATA", "OK", 5000, cb, ctx);
    }

    bool hangup(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("ATH", "OK", 5000, cb, ctx);
    }

    bool listSms(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+CMGL=\"ALL\"", "OK", 10000, cb, ctx);
    }

    bool readSms(uint16_t index, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand(String("AT+CMGR=") + index, "OK", 5000, cb, ctx);
    }

    bool deleteSms(uint16_t index, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand(String("AT+CMGD=") + index, "OK", 5000, cb, ctx);
    }

    bool setApn(const String &apn,
                const String &user = "",
                const String &pass = "",
                CommandCallback cb = nullptr,
                void *ctx = nullptr)
    {
        bool ok = enqueueCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK", 1000, cb, ctx);
        ok = ok && enqueueCommand(String("AT+SAPBR=3,1,\"APN\",\"") + apn + "\"", "OK", 1000, cb, ctx);
        if (user.length())
            ok = ok && enqueueCommand(String("AT+SAPBR=3,1,\"USER\",\"") + user + "\"", "OK", 1000, cb, ctx);
        if (pass.length())
            ok = ok && enqueueCommand(String("AT+SAPBR=3,1,\"PWD\",\"") + pass + "\"", "OK", 1000, cb, ctx);
        return ok;
    }

    bool openBearer(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+SAPBR=1,1", "OK", 10000, cb, ctx);
    }

    bool closeBearer(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+SAPBR=0,1", "OK", 10000, cb, ctx);
    }

    bool getBearerIp(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+SAPBR=2,1", "+SAPBR:", 3000, cb, ctx);
    }

    bool httpInit(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+HTTPINIT", "OK", 1000, cb, ctx);
    }

    bool httpTerm(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+HTTPTERM", "OK", 1000, cb, ctx);
    }

    bool httpSetCid(uint8_t cid = 1, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand(String("AT+HTTPPARA=\"CID\",") + cid, "OK", 1000, cb, ctx);
    }

    bool httpSetUrl(const String &url, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand(String("AT+HTTPPARA=\"URL\",\"") + url + "\"", "OK", 1000, cb, ctx);
    }

    bool httpSetContentType(const String &type, CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand(String("AT+HTTPPARA=\"CONTENT\",\"") + type + "\"", "OK", 1000, cb, ctx);
    }

    bool httpGet(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+HTTPACTION=0", "+HTTPACTION:", 10000, cb, ctx);
    }

    bool httpRead(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+HTTPREAD", "+HTTPREAD:", 2000, cb, ctx);
    }

    bool powerDown(CommandCallback cb = nullptr, void *ctx = nullptr)
    {
        return enqueueCommand("AT+CPOWD=1", "POWER DOWN", 5000, cb, ctx);
    }

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

    static bool timePassed_(uint32_t now, uint32_t deadline)
    {
        return (int32_t)(now - deadline) >= 0;
    }

    void startNext_()
    {
        if (_q_count == 0 || !_ser)
            return;
        _current = _queue[_q_head];
        _current_resp = "";
        _waiting = true;
        _deadline_ms = millis() + _current.timeout_ms;
        _ser->print(_current.cmd);
        _ser->print("\r");
    }

    void finishCommand_(bool ok)
    {
        _last_response = _current_resp;
        if (_cmd_done_cb)
            _cmd_done_cb(_cmd_done_ctx, ok, _last_response, _current.cmd);
        if (_current.cb)
            _current.cb(_current.ctx, ok, _last_response);

        _waiting = false;
        _current = PendingCmd{};
        _current_resp = "";
        _payload_pending = false;
        _payload = "";
        _payload_timeout_ms = 0;

        if (_q_count > 0)
        {
            _q_head = (uint8_t)((_q_head + 1) % kQueueSize);
            --_q_count;
        }
    }

    void consumeChar_(char c)
    {
        if (_waiting && _current.expect == ">" && c == '>')
        {
            handleLine_(">");
            return;
        }
        if (c == '\n')
        {
            String line = _line_buf;
            _line_buf = "";
            line.trim();
            if (line.length())
                handleLine_(line);
            return;
        }
        if (c == '\r')
            return;
        if (_line_buf.length() < kMaxLineLen)
            _line_buf += c;
        else
            _line_buf = "";
    }

    void handleLine_(const String &line)
    {
        if (handleUrc_(line))
            return;

        if (!_waiting)
            return;

        if (_current.expect == ">" && line == ">")
        {
            if (_ser && _payload_pending)
            {
                _ser->print(_payload);
                _payload_pending = false;
                _payload = "";
                _current.expect = "OK";
                _deadline_ms = millis() + _payload_timeout_ms;
                return;
            }
            finishCommand_(true);
            return;
        }

        _current_resp += line;
        _current_resp += "\n";

        if (isErrorLine_(line))
        {
            finishCommand_(false);
            return;
        }

        if (_current.expect.length() && line.indexOf(_current.expect) >= 0)
        {
            finishCommand_(true);
            return;
        }
    }

    bool handleUrc_(const String &line)
    {
        if (line.startsWith("RING"))
        {
            if (_urc_cb)
                _urc_cb(_urc_ctx, line);
            return true;
        }
        if (line.startsWith("+CLIP:"))
        {
            String num;
            if (parseQuoted_(line, num) && _call_cb)
                _call_cb(_call_ctx, num);
            if (_urc_cb)
                _urc_cb(_urc_ctx, line);
            return true;
        }
        if (line.startsWith("+CMTI:"))
        {
            uint16_t idx = 0;
            if (parseIndexAfterComma_(line, idx) && _sms_cb)
                _sms_cb(_sms_ctx, idx);
            if (_urc_cb)
                _urc_cb(_urc_ctx, line);
            return true;
        }
        if (line.startsWith("+CUSD:"))
        {
            String text;
            if (parseQuoted_(line, text) && _ussd_cb)
                _ussd_cb(_ussd_ctx, text);
            if (_urc_cb)
                _urc_cb(_urc_ctx, line);
            return true;
        }
        if (line.startsWith("+HTTPACTION:"))
        {
            int status = -1;
            int len = -1;
            if (parseHttpAction_(line, status, len) && _http_cb)
                _http_cb(_http_ctx, status, len);
            if (_urc_cb)
                _urc_cb(_urc_ctx, line);
            return true;
        }
        return false;
    }

    static bool isErrorLine_(const String &line)
    {
        if (line == "ERROR")
            return true;
        if (line.startsWith("+CME ERROR"))
            return true;
        if (line.startsWith("+CMS ERROR"))
            return true;
        return false;
    }

    static bool parseIndexAfterComma_(const String &line, uint16_t &out)
    {
        int comma = line.indexOf(',');
        if (comma < 0)
            return false;
        out = (uint16_t)line.substring(comma + 1).toInt();
        return out > 0;
    }

    static bool parseQuoted_(const String &line, String &out)
    {
        int q1 = line.indexOf('\"');
        int q2 = line.indexOf('\"', q1 + 1);
        if (q1 < 0 || q2 < 0)
            return false;
        out = line.substring(q1 + 1, q2);
        return true;
    }

    static bool parseHttpAction_(const String &line, int &status, int &len)
    {
        int i1 = line.indexOf(',');
        int i2 = line.indexOf(',', i1 + 1);
        if (i1 < 0 || i2 < 0)
            return false;
        status = line.substring(i1 + 1, i2).toInt();
        len = line.substring(i2 + 1).toInt();
        return true;
    }
};
