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

#include "core/network/gsm_modem.hpp"

#include "hal/bus/uart.hpp"
#include "hal/sim800l.hpp"
#include "utils/logger.hpp"

GsmModem::GsmModem(UartManager &uart, Sim800l &modem, Logger &log)
    : _uart(uart), _modem(modem), _log(log)
{
}
bool GsmModem::begin(uint8_t uart_index, uint32_t config)
{
    if (!_enabled)
        return false;
    HardwareSerial *ser = _uart.beginSerialForIndex(uart_index, config);
    if (!ser)
    {
        if (_log.ready())
            _log.error(F("GSM"), F("UART init failed (idx=%u)"), uart_index);
        return false;
    }
    _serial = ser;
    _modem.begin(*ser);
    bindCallbacks_();
    if (_log.ready())
        _log.info(F("GSM"), F("UART ready (idx=%u)"), uart_index);
    initSequence_();
    _started = true;
    return true;
}
void GsmModem::loop()
{
    if (!_enabled)
        return;
    _modem.tick();
    checkInitWatchdog_();
    checkInitRetry_();
}
Sim800l &GsmModem::driver()
{ return _modem; }
const Sim800l &GsmModem::driver() const
{ return _modem; }
const String &GsmModem::lastUrc() const
{ return _last_urc; }
uint16_t GsmModem::lastSmsIndex() const
{ return _last_sms_index; }
const String &GsmModem::lastCallNumber() const
{ return _last_call; }
const String &GsmModem::lastUssd() const
{ return _last_ussd; }
int GsmModem::lastHttpStatus() const
{ return _last_http_status; }
int GsmModem::lastHttpLen() const
{ return _last_http_len; }
const String &GsmModem::lastError() const
{ return _last_error; }
const String &GsmModem::imei() const
{ return _imei; }
const String &GsmModem::imsi() const
{ return _imsi; }
const String &GsmModem::operatorName() const
{ return _operator_name; }
const String &GsmModem::signalQuality() const
{ return _signal_quality; }
const String &GsmModem::regStatus() const
{ return _reg_status; }
bool GsmModem::enabled() const
{ return _enabled; }
bool GsmModem::started() const
{ return _started; }
void GsmModem::setEnabled(bool enabled)
{
    _enabled = enabled;
    if (!enabled)
    {
        _started = false;
        _last_error = "";
    }
}
bool GsmModem::sendSms(const String &number, const String &text)
{
    if (number.length() == 0)
        return false;
    return _modem.sendSms(number, text, &GsmModem::onCmdLog_, nullptr);
}
bool GsmModem::takeLastCall(String &out)
{
    if (_call_count == 0)
        return false;
    out = _call_queue[_call_head];
    _call_head = (uint8_t)((_call_head + 1) % kCallQueue);
    --_call_count;
    return true;
}
void GsmModem::onUrc_(void *ctx, const String &line)
{
    static_cast<GsmModem *>(ctx)->handleUrc_(line);
}
void GsmModem::onSms_(void *ctx, uint16_t index)
{
    static_cast<GsmModem *>(ctx)->handleSms_(index);
}
void GsmModem::onCall_(void *ctx, const String &number)
{
    static_cast<GsmModem *>(ctx)->handleCall_(number);
}
void GsmModem::onUssd_(void *ctx, const String &text)
{
    static_cast<GsmModem *>(ctx)->handleUssd_(text);
}
void GsmModem::onHttpAction_(void *ctx, int status, int len)
{
    static_cast<GsmModem *>(ctx)->handleHttpAction_(status, len);
}
void GsmModem::bindCallbacks_()
{
    _modem.setUrcHandler(&GsmModem::onUrc_, this);
    _modem.setSmsHandler(&GsmModem::onSms_, this);
    _modem.setCallHandler(&GsmModem::onCall_, this);
    _modem.setUssdHandler(&GsmModem::onUssd_, this);
    _modem.setHttpActionHandler(&GsmModem::onHttpAction_, this);
    _modem.setCommandDoneHandler(&GsmModem::onCmdDone_, this);
}
void GsmModem::onCmdLog_(void *ctx, bool ok, const String &response)
{
    CmdLogCtx *st = static_cast<CmdLogCtx *>(ctx);
    if (!st || !st->self)
        return;
    st->self->logCmd_(st->name, ok, response);
}
void GsmModem::onCmdDone_(void *ctx, bool ok, const String &response, const String &cmd)
{
    static_cast<GsmModem *>(ctx)->handleCmdDone_(ok, response, cmd);
}
void GsmModem::initSequence_()
{
    _init_retry_pending = false;
    if (_init_attempt < kInitMaxAttempts)
        ++_init_attempt;
    _init_logged = false;
    static const __FlashStringHelper *names[] = {
        F("AT"),
        F("ATE0"),
        F("CMGF=1"),
        F("CLIP=1"),
        F("CREG=1"),
        F("GSN"),
        F("CIMI"),
        F("COPS?"),
        F("CSQ"),
        F("CREG?")
    };
    _init_pending = true;
    _init_warned = false;
    _init_started_ms = millis();
    _init_ok = 0;
    _init_fail = 0;
    size_t idx = 0;
    auto bind = [&](size_t i) -> CmdLogCtx * {
        if (i >= kInitCmdCount)
            return nullptr;
        _init_ctx[i].self = this;
        _init_ctx[i].name = names[i];
        return &_init_ctx[i];
    };
    enqueueOrLog_(_modem.sync(&GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
    enqueueOrLog_(_modem.setEcho(false, &GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
    enqueueOrLog_(_modem.setSmsTextMode(&GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
    enqueueOrLog_(_modem.setCallerId(true, &GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
    enqueueOrLog_(_modem.enableRegUrc(&GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
    enqueueOrLog_(_modem.requestImei(&GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
    enqueueOrLog_(_modem.requestImsi(&GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
    enqueueOrLog_(_modem.requestOperator(&GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
    enqueueOrLog_(_modem.requestSignal(&GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
    enqueueOrLog_(_modem.requestRegStatus(&GsmModem::onCmdLog_, bind(idx++)), names[idx - 1]);
}
void GsmModem::logCmd_(const __FlashStringHelper *name, bool ok, const String &response)
{
    if (_init_pending && name)
    {
        if (ok)
            ++_init_ok;
        else
            ++_init_fail;
        if ((_init_ok + _init_fail) >= kInitCmdCount)
        {
            _init_pending = false;
            if (_init_ok == 0 && !_init_warned)
            {
                _init_warned = true;
                _last_error = "Modem not responding";
                if (_log.ready())
                    _log.error(F("GSM"), F("Modem not responding"));
            }
            if (_init_ok == 0)
                scheduleInitRetry_();
            if (_init_ok > 0)
                logInitSummary_();
        }
    }
    if (ok && name)
        parseInitResponse_(name, response);
}
void GsmModem::handleUrc_(const String &line)
{
    _last_urc = line;
    if (_log.ready())
        _log.info(F("GSM"), F("URC: %s"), line.c_str());
}
void GsmModem::handleSms_(uint16_t index)
{
    _last_sms_index = index;
    if (_log.ready())
        _log.info(F("GSM"), F("SMS index %u"), (unsigned)index);
}
void GsmModem::handleCall_(const String &number)
{
    _last_call = number;
    if (_call_count < kCallQueue)
    {
        _call_queue[_call_tail] = number;
        _call_tail = (uint8_t)((_call_tail + 1) % kCallQueue);
        ++_call_count;
    }
    else
    {
        _call_queue[_call_tail] = number;
        _call_tail = (uint8_t)((_call_tail + 1) % kCallQueue);
        _call_head = _call_tail;
        if (_log.ready())
            _log.warn(F("GSM"), F("Call queue overflow"));
    }
    if (_log.ready())
        _log.info(F("GSM"), F("Call from %s"), number.c_str());
}
void GsmModem::handleUssd_(const String &text)
{
    _last_ussd = text;
    if (_log.ready())
        _log.info(F("GSM"), F("USSD: %s"), text.c_str());
}
void GsmModem::handleHttpAction_(int status, int len)
{
    _last_http_status = status;
    _last_http_len = len;
    if (_log.ready())
        _log.info(F("GSM"), F("HTTP action status=%d len=%d"), status, len);
}
void GsmModem::enqueueOrLog_(bool ok, const __FlashStringHelper *name)
{
    if (ok || !_log.ready())
        return;
    if (name)
        _log.warn(F("GSM"), F("Command queue full: %s"), name);
    else
        _log.warn(F("GSM"), F("Command queue full"));
}
void GsmModem::handleCmdDone_(bool ok, const String &response, const String &cmd)
{
    if (ok)
    {
        if (_last_error.length())
            _last_error = "";
        return;
    }
    if (response.length() != 0)
        return;
    _last_error = "Modem not responding";
    if (!_log.ready())
        return;
    const String label = cmdTimeoutLabel_(cmd);
    if (label.length())
        _log.error(F("GSM"), F("Timeout: %s"), label.c_str());
    else if (cmd.length())
        _log.error(F("GSM"), F("Command timeout: %s"), cmd.c_str());
    else
        _log.error(F("GSM"), F("Command timeout"));
}
void GsmModem::checkInitWatchdog_()
{
    if (!_init_pending || _init_warned)
        return;
    const uint32_t now = millis();
    if ((uint32_t)(now - _init_started_ms) < kInitTimeoutMs)
        return;
    _init_warned = true;
    if (_init_ok == 0)
    {
        _last_error = "Modem not responding";
        if (!_log.ready())
            return;
        _log.error(F("GSM"), F("Modem not responding"));
        _init_pending = false;
        scheduleInitRetry_();
    }
}
void GsmModem::scheduleInitRetry_()
{
    if (!_enabled)
        return;
    if (_init_retry_pending)
        return;
    if (_init_attempt >= kInitMaxAttempts)
        return;
    _init_retry_pending = true;
    _init_retry_at_ms = millis() + kInitRetryDelayMs;
    if (_log.ready())
        _log.warn(F("GSM"), F("Init retry %u/%u in %u ms"),
                  (unsigned)(_init_attempt + 1), (unsigned)kInitMaxAttempts,
                  (unsigned)kInitRetryDelayMs);
}
void GsmModem::checkInitRetry_()
{
    if (!_init_retry_pending)
        return;
    const uint32_t now = millis();
    if ((int32_t)(now - _init_retry_at_ms) < 0)
        return;
    initSequence_();
}
String GsmModem::firstDataLine_(const String &response)
{
    int pos = 0;
    while (pos < response.length())
    {
        int end = response.indexOf('\n', pos);
        if (end < 0)
            end = response.length();
        String line = response.substring(pos, end);
        line.trim();
        if (line.length() && line != "OK" && line != "ERROR")
            return line;
        pos = end + 1;
    }
    return "";
}
String GsmModem::parseQuoted_(const String &line)
{
    int start = line.indexOf('\"');
    if (start < 0)
        return "";
    int end = line.indexOf('\"', start + 1);
    if (end <= start)
        return "";
    return line.substring(start + 1, end);
}
void GsmModem::parseInitResponse_(const __FlashStringHelper *name, const String &response)
{
    const String n = String(name);
    const String line = firstDataLine_(response);
    if (n == "GSN")
    {
        _imei = line;
        return;
    }
    if (n == "CIMI")
    {
        _imsi = line;
        return;
    }
    if (n == "COPS?")
    {
        _operator_name = parseQuoted_(line);
        if (_operator_name.length() == 0)
            _operator_name = line;
        return;
    }
    if (n == "CSQ")
    {
        int pos = line.indexOf(':');
        _signal_quality = (pos >= 0) ? line.substring(pos + 1) : line;
        _signal_quality.trim();
        return;
    }
    if (n == "CREG?")
    {
        int pos = line.indexOf(':');
        _reg_status = (pos >= 0) ? line.substring(pos + 1) : line;
        _reg_status.trim();
        return;
    }
}
String GsmModem::cmdTimeoutLabel_(const String &cmd)
{
    if (cmd == "AT")
        return "Modem check";
    if (cmd == "ATE0")
        return "Disable echo";
    if (cmd == "ATE1")
        return "Enable echo";
    if (cmd == "AT+CMGF=1")
        return "SMS text mode";
    if (cmd == "AT+CLIP=1")
        return "Caller ID";
    if (cmd == "AT+GSN")
        return "Read IMEI";
    if (cmd == "AT+CIMI")
        return "Read IMSI";
    if (cmd == "AT+COPS?")
        return "Network operator";
    if (cmd == "AT+CSQ")
        return "Signal quality";
    if (cmd == "AT+CREG?")
        return "Network registration";
    if (cmd == "AT+CREG=1")
        return "Enable registration URC";
    if (cmd.startsWith("AT+CMGS="))
        return "Send SMS";
    if (cmd.startsWith("AT+CMGR="))
        return "Read SMS";
    if (cmd.startsWith("AT+CMGD="))
        return "Delete SMS";
    if (cmd.startsWith("AT+CMGL="))
        return "List SMS";
    if (cmd.startsWith("ATD"))
        return "Dial";
    if (cmd == "ATA")
        return "Answer call";
    if (cmd == "ATH")
        return "Hang up";
    if (cmd.startsWith("AT+CUSD="))
        return "USSD request";
    if (cmd == "AT+SAPBR=1,1")
        return "GPRS: open bearer";
    if (cmd == "AT+SAPBR=0,1")
        return "GPRS: close bearer";
    if (cmd.startsWith("AT+SAPBR=3,1"))
        return "GPRS: config";
    if (cmd == "AT+HTTPINIT")
        return "HTTP: init";
    if (cmd == "AT+HTTPTERM")
        return "HTTP: term";
    if (cmd.startsWith("AT+HTTPPARA="))
        return "HTTP: parameter";
    if (cmd == "AT+HTTPACTION=0")
        return "HTTP: GET";
    if (cmd == "AT+HTTPREAD")
        return "HTTP: read";
    if (cmd == "AT+CPOWD=1")
        return "Power down modem";
    return "";
}
void GsmModem::logInitSummary_()
{
    if (_init_logged || !_log.ready())
        return;
    _init_logged = true;
    _log.info(F("GSM"), F("Modem connected"));
    if (_imei.length())
        _log.info(F("GSM"), F("IMEI: %s"), _imei.c_str());
    if (_signal_quality.length())
        _log.info(F("GSM"), F("Signal: %s"), _signal_quality.c_str());
    if (_reg_status.length())
        _log.info(F("GSM"), F("Registration: %s"), _reg_status.c_str());
    if (_operator_name.length())
        _log.info(F("GSM"), F("Operator: %s"), _operator_name.c_str());
    if (_imsi.length())
        _log.info(F("GSM"), F("IMSI: %s"), _imsi.c_str());
}
