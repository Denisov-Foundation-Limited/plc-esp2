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

#include "hal/sim800l.hpp"

bool Sim800l::begin(Stream &ser)
{
    _ser = &ser;
    return true;
}

void Sim800l::tick()
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

bool Sim800l::enqueueCommand(const String &cmd, const char *expect, uint32_t timeout_ms, CommandCallback cb, void *ctx)
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

void Sim800l::setUrcHandler(LineCallback cb, void *ctx) { _urc_cb = cb; _urc_ctx = ctx; }
void Sim800l::setCommandDoneHandler(CommandDoneCallback cb, void *ctx) { _cmd_done_cb = cb; _cmd_done_ctx = ctx; }
void Sim800l::setSmsHandler(SmsIndexCallback cb, void *ctx) { _sms_cb = cb; _sms_ctx = ctx; }
void Sim800l::setCallHandler(CallCallback cb, void *ctx) { _call_cb = cb; _call_ctx = ctx; }
void Sim800l::setUssdHandler(UssdCallback cb, void *ctx) { _ussd_cb = cb; _ussd_ctx = ctx; }
void Sim800l::setHttpActionHandler(HttpActionCallback cb, void *ctx) { _http_cb = cb; _http_ctx = ctx; }

const String &Sim800l::lastResponse() const { return _last_response; }

bool Sim800l::sync(CommandCallback cb, void *ctx) { return enqueueCommand("AT", "OK", 1000, cb, ctx); }
bool Sim800l::setEcho(bool on, CommandCallback cb, void *ctx) { return enqueueCommand(on ? "ATE1" : "ATE0", "OK", 1000, cb, ctx); }
bool Sim800l::setSmsTextMode(CommandCallback cb, void *ctx) { return enqueueCommand("AT+CMGF=1", "OK", 1000, cb, ctx); }
bool Sim800l::setCallerId(bool on, CommandCallback cb, void *ctx) { return enqueueCommand(on ? "AT+CLIP=1" : "AT+CLIP=0", "OK", 1000, cb, ctx); }
bool Sim800l::requestImei(CommandCallback cb, void *ctx) { return enqueueCommand("AT+GSN", "OK", 1000, cb, ctx); }
bool Sim800l::requestImsi(CommandCallback cb, void *ctx) { return enqueueCommand("AT+CIMI", "OK", 1000, cb, ctx); }
bool Sim800l::requestOperator(CommandCallback cb, void *ctx) { return enqueueCommand("AT+COPS?", "OK", 1000, cb, ctx); }
bool Sim800l::requestSignal(CommandCallback cb, void *ctx) { return enqueueCommand("AT+CSQ", "OK", 1000, cb, ctx); }
bool Sim800l::requestRegStatus(CommandCallback cb, void *ctx) { return enqueueCommand("AT+CREG?", "OK", 1000, cb, ctx); }
bool Sim800l::enableRegUrc(CommandCallback cb, void *ctx) { return enqueueCommand("AT+CREG=1", "OK", 1000, cb, ctx); }

bool Sim800l::sendUssd(const String &code, CommandCallback cb, void *ctx)
{
    const String cmd = String("AT+CUSD=1,\"") + code + "\",15";
    return enqueueCommand(cmd, "+CUSD:", 10000, cb, ctx);
}

bool Sim800l::sendSms(const String &number, const String &text, CommandCallback cb, void *ctx)
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

bool Sim800l::dial(const String &number, CommandCallback cb, void *ctx) { return enqueueCommand(String("ATD") + number + ";", "OK", 5000, cb, ctx); }
bool Sim800l::answer(CommandCallback cb, void *ctx) { return enqueueCommand("ATA", "OK", 5000, cb, ctx); }
bool Sim800l::hangup(CommandCallback cb, void *ctx) { return enqueueCommand("ATH", "OK", 5000, cb, ctx); }
bool Sim800l::listSms(CommandCallback cb, void *ctx) { return enqueueCommand("AT+CMGL=\"ALL\"", "OK", 10000, cb, ctx); }
bool Sim800l::readSms(uint16_t index, CommandCallback cb, void *ctx) { return enqueueCommand(String("AT+CMGR=") + index, "OK", 5000, cb, ctx); }
bool Sim800l::deleteSms(uint16_t index, CommandCallback cb, void *ctx) { return enqueueCommand(String("AT+CMGD=") + index, "OK", 5000, cb, ctx); }

bool Sim800l::setApn(const String &apn, const String &user, const String &pass, CommandCallback cb, void *ctx)
{
    bool ok = enqueueCommand("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"", "OK", 1000, cb, ctx);
    ok = ok && enqueueCommand(String("AT+SAPBR=3,1,\"APN\",\"") + apn + "\"", "OK", 1000, cb, ctx);
    if (user.length())
        ok = ok && enqueueCommand(String("AT+SAPBR=3,1,\"USER\",\"") + user + "\"", "OK", 1000, cb, ctx);
    if (pass.length())
        ok = ok && enqueueCommand(String("AT+SAPBR=3,1,\"PWD\",\"") + pass + "\"", "OK", 1000, cb, ctx);
    return ok;
}

bool Sim800l::openBearer(CommandCallback cb, void *ctx) { return enqueueCommand("AT+SAPBR=1,1", "OK", 10000, cb, ctx); }
bool Sim800l::closeBearer(CommandCallback cb, void *ctx) { return enqueueCommand("AT+SAPBR=0,1", "OK", 10000, cb, ctx); }
bool Sim800l::getBearerIp(CommandCallback cb, void *ctx) { return enqueueCommand("AT+SAPBR=2,1", "+SAPBR:", 3000, cb, ctx); }
bool Sim800l::httpInit(CommandCallback cb, void *ctx) { return enqueueCommand("AT+HTTPINIT", "OK", 1000, cb, ctx); }
bool Sim800l::httpTerm(CommandCallback cb, void *ctx) { return enqueueCommand("AT+HTTPTERM", "OK", 1000, cb, ctx); }
bool Sim800l::httpSetCid(uint8_t cid, CommandCallback cb, void *ctx) { return enqueueCommand(String("AT+HTTPPARA=\"CID\",") + cid, "OK", 1000, cb, ctx); }
bool Sim800l::httpSetUrl(const String &url, CommandCallback cb, void *ctx) { return enqueueCommand(String("AT+HTTPPARA=\"URL\",\"") + url + "\"", "OK", 1000, cb, ctx); }
bool Sim800l::httpSetContentType(const String &type, CommandCallback cb, void *ctx) { return enqueueCommand(String("AT+HTTPPARA=\"CONTENT\",\"") + type + "\"", "OK", 1000, cb, ctx); }
bool Sim800l::httpGet(CommandCallback cb, void *ctx) { return enqueueCommand("AT+HTTPACTION=0", "+HTTPACTION:", 10000, cb, ctx); }
bool Sim800l::httpRead(CommandCallback cb, void *ctx) { return enqueueCommand("AT+HTTPREAD", "+HTTPREAD:", 2000, cb, ctx); }
bool Sim800l::powerDown(CommandCallback cb, void *ctx) { return enqueueCommand("AT+CPOWD=1", "POWER DOWN", 5000, cb, ctx); }

bool Sim800l::timePassed_(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

void Sim800l::startNext_()
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

void Sim800l::finishCommand_(bool ok)
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

void Sim800l::consumeChar_(char c)
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

void Sim800l::handleLine_(const String &line)
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

bool Sim800l::handleUrc_(const String &line)
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

bool Sim800l::isErrorLine_(const String &line)
{
    if (line == "ERROR")
        return true;
    if (line.startsWith("+CME ERROR"))
        return true;
    if (line.startsWith("+CMS ERROR"))
        return true;
    return false;
}

bool Sim800l::parseIndexAfterComma_(const String &line, uint16_t &out)
{
    int comma = line.indexOf(',');
    if (comma < 0)
        return false;
    out = (uint16_t)line.substring(comma + 1).toInt();
    return out > 0;
}

bool Sim800l::parseQuoted_(const String &line, String &out)
{
    int q1 = line.indexOf('"');
    int q2 = line.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 < 0)
        return false;
    out = line.substring(q1 + 1, q2);
    return true;
}

bool Sim800l::parseHttpAction_(const String &line, int &status, int &len)
{
    int i1 = line.indexOf(',');
    int i2 = line.indexOf(',', i1 + 1);
    if (i1 < 0 || i2 < 0)
        return false;
    status = line.substring(i1 + 1, i2).toInt();
    len = line.substring(i2 + 1).toInt();
    return true;
}
