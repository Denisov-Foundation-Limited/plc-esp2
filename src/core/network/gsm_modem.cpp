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
#include <freertos/task.h>

namespace
{
bool isRegisteredRegState(uint8_t state)
{
    return state == 1 || state == 5;
}

bool hasMeaningfulSignal(const String &value)
{
    String v = value;
    v.trim();
    if (!v.length())
        return false;
    if (v == "0,0")
        return false;
    return true;
}

bool parseCsq(const String &line, int &out_rssi, int &out_ber)
{
    int pos = line.indexOf(':');
    String tail = (pos >= 0) ? line.substring(pos + 1) : line;
    tail.trim();
    const int comma = tail.indexOf(',');
    if (comma < 0)
        return false;
    const String rssi_s = tail.substring(0, comma);
    const String ber_s = tail.substring(comma + 1);
    out_rssi = rssi_s.toInt();
    out_ber = ber_s.toInt();
    return out_rssi >= 0 && out_rssi <= 99 && out_ber >= 0;
}

bool csqToDbm(int rssi, int &out_dbm)
{
    if (rssi < 0 || rssi > 31 || rssi == 99)
        return false;
    out_dbm = -113 + (2 * rssi);
    return true;
}

const char *signalQualityLabel(int rssi)
{
    if (rssi >= 20)
        return "best";
    if (rssi >= 14)
        return "medium";
    if (rssi >= 10)
        return "weak";
    return "very weak";
}
}

GsmModem::GsmModem(UartManager &uart, Sim800l &modem, Logger &log)
    : _uart(uart), _modem(modem), _log(log)
{
}
bool GsmModem::begin(uint8_t uart_index, uint32_t config)
{
    if (!_enabled)
        return false;
    if (!lockModem_())
        return false;
    HardwareSerial *ser = _uart.beginSerialForIndex(uart_index, config);
    if (!ser)
    {
        unlockModem_();
        if (_log.ready())
            _log.error(F("GSM"), F("UART init failed (idx=%u)"), uart_index);
        return false;
    }
    _serial = ser;
    _modem.begin(*ser);
    bindCallbacks_();
    if (_log.ready())
        _log.info(F("GSM"), F("UART ready: idx: %u"), uart_index);
    initSequence_();
    _started = true;
    unlockModem_();
    return true;
}
void GsmModem::loop()
{
    if (!_enabled)
        return;
    if (!lockModem_())
        return;
    _modem.tick();
    checkInitWatchdog_();
    checkInitRetry_();
    pollNetworkState_();
    ensureCallerId_();
    unlockModem_();
}
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
    if (!lockModem_())
        return false;
    const bool ok = _modem.sendSms(number, text, &GsmModem::onCmdLog_, nullptr);
    unlockModem_();
    return ok;
}

bool GsmModem::dial(const String &number)
{
    if (number.length() == 0)
        return false;
    if (!lockModem_())
        return false;
    const bool ok = _modem.dial(number, &GsmModem::onCmdLog_, nullptr);
    unlockModem_();
    return ok;
}

bool GsmModem::hangup()
{
    if (!lockModem_())
        return false;
    const bool ok = _modem.hangup(&GsmModem::onCmdLog_, nullptr);
    unlockModem_();
    return ok;
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
    _next_reg_poll_ms = millis() + kRegPollMs;
    _next_info_poll_ms = millis() + kInfoPollMs;
    _caller_id_enabled = false;
    _next_caller_id_retry_ms = millis() + kCallerIdRetryMs;
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
    const String n = name ? String(name) : String();
    if (n == "CLIP=1")
    {
        _caller_id_enabled = ok;
        if (!ok)
            _next_caller_id_retry_ms = millis() + kCallerIdRetryMs;
    }
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
    uint8_t reg_state = 0xFF;
    if (parseRegState_(line, reg_state))
        updateRegState_(reg_state);
    if (line.startsWith("+CLIP:") || line.startsWith("RING"))
        return;
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
    _caller_id_enabled = true;
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
        _log.info(F("GSM"), F("HTTP action: status: %d len: %d"), status, len);
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
    const bool is_dial_cmd = cmd.startsWith("ATD");
    if (ok)
    {
        _timeout_streak = 0;
        if (_last_error.length())
            _last_error = "";
        return;
    }
    if (response.length() != 0)
    {
        _timeout_streak = 0;
        if (is_dial_cmd && _log.ready())
        {
            String line = firstDataLine_(response);
            if (!line.length())
                line = response;
            _log.warn(F("GSM"), F("Dial command failed: %s"), line.c_str());
        }
        return;
    }
    _last_error = "Modem not responding";
    if (_timeout_streak < 0xFF)
        ++_timeout_streak;
    if (!_log.ready())
        return;
    const String label = cmdTimeoutLabel_(cmd);
    if (label.length())
        _log.error(F("GSM"), F("Timeout: %s"), label.c_str());
    else if (cmd.length())
        _log.error(F("GSM"), F("Command timeout: %s"), cmd.c_str());
    else
        _log.error(F("GSM"), F("Command timeout"));
    if (is_dial_cmd)
        _log.error(F("GSM"), F("Dial command timeout"));
    if (_timeout_streak == 3)
        _log.error(F("GSM"), F("Modem link unstable: 3 timeouts in a row"));
    if (cmd == "AT+CLIP=1")
    {
        _caller_id_enabled = false;
        _next_caller_id_retry_ms = millis() + kCallerIdRetryMs;
    }
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
void GsmModem::pollNetworkState_()
{
    if (_init_pending)
        return;
    const uint32_t now = millis();
    if ((int32_t)(now - _next_reg_poll_ms) >= 0)
    {
        _reg_poll_ctx.self = this;
        _reg_poll_ctx.name = F("CREG?");
        if (_modem.requestRegStatus(&GsmModem::onCmdLog_, &_reg_poll_ctx))
            _next_reg_poll_ms = now + kRegPollMs;
        else
            _next_reg_poll_ms = now + 2000;
    }
    if (!isRegisteredRegState(_reg_state))
        return;
    if ((int32_t)(now - _next_info_poll_ms) < 0)
        return;
    _operator_poll_ctx.self = this;
    _operator_poll_ctx.name = F("COPS?");
    _signal_poll_ctx.self = this;
    _signal_poll_ctx.name = F("CSQ");
    bool sent = false;
    if (_modem.requestOperator(&GsmModem::onCmdLog_, &_operator_poll_ctx))
        sent = true;
    if (_modem.requestSignal(&GsmModem::onCmdLog_, &_signal_poll_ctx))
        sent = true;
    _next_info_poll_ms = now + (sent ? kInfoPollMs : 2000);
}
void GsmModem::ensureCallerId_()
{
    if (_init_pending)
        return;
    if (_caller_id_enabled)
        return;
    const uint32_t now = millis();
    if ((int32_t)(now - _next_caller_id_retry_ms) < 0)
        return;
    _caller_id_ctx.self = this;
    _caller_id_ctx.name = F("CLIP=1");
    if (_modem.setCallerId(true, &GsmModem::onCmdLog_, &_caller_id_ctx))
    {
        _next_caller_id_retry_ms = now + kCallerIdRetryMs;
        if (_log.ready())
            _log.info(F("GSM"), F("Retry caller ID enable"));
    }
    else
    {
        _next_caller_id_retry_ms = now + 1000;
    }
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
bool GsmModem::parseRegState_(const String &line, uint8_t &out_state)
{
    if (line.indexOf("CREG") < 0)
        return false;
    const int colon = line.indexOf(':');
    if (colon < 0)
        return false;
    String tail = line.substring(colon + 1);
    tail.trim();
    if (!tail.length())
        return false;
    const int comma = tail.indexOf(',');
    String token = tail;
    if (comma >= 0)
    {
        token = tail.substring(comma + 1);
        const int next_comma = token.indexOf(',');
        if (next_comma >= 0)
            token = token.substring(0, next_comma);
    }
    token.trim();
    if (!token.length())
        return false;
    int end = 0;
    while (end < token.length())
    {
        const char c = token.charAt(end);
        if (c < '0' || c > '9')
            break;
        ++end;
    }
    if (end == 0)
        return false;
    const long parsed = token.substring(0, end).toInt();
    if (parsed < 0 || parsed > 255)
        return false;
    out_state = (uint8_t)parsed;
    return true;
}
const char *GsmModem::regStateName_(uint8_t state)
{
    switch (state)
    {
    case 0:
        return "not registered";
    case 1:
        return "registered (home)";
    case 2:
        return "searching";
    case 3:
        return "registration denied";
    case 4:
        return "unknown";
    case 5:
        return "registered (roaming)";
    default:
        return "invalid";
    }
}
void GsmModem::updateRegState_(uint8_t state)
{
    if (_reg_state == state)
        return;
    const uint8_t prev = _reg_state;
    _reg_state = state;
    if (!_log.ready())
        return;
    const bool was_registered = isRegisteredRegState(prev);
    const bool is_registered = isRegisteredRegState(state);
    if (is_registered && !was_registered)
    {
        _log.info(F("GSM"), F("Network registered: %s"), regStateName_(state));
        _next_info_poll_ms = millis();
        logOperatorIfChanged_();
        logSignalIfChanged_();
        return;
    }
    if (!is_registered && was_registered)
    {
        _log.warn(F("GSM"), F("Network registration lost: %s"), regStateName_(state));
        return;
    }
    if (state == 3)
    {
        _log.error(F("GSM"), F("Network registration denied"));
        return;
    }
    _log.info(F("GSM"), F("Network registration state: %s"), regStateName_(state));
}
void GsmModem::logOperatorIfChanged_()
{
    if (!_log.ready() || !isRegisteredRegState(_reg_state))
        return;
    if (_operator_name.length() == 0)
        return;
    if (_last_logged_operator == _operator_name)
        return;
    _last_logged_operator = _operator_name;
    _log.info(F("GSM"), F("Operator: %s"), _operator_name.c_str());
}
void GsmModem::logSignalIfChanged_()
{
    if (!_log.ready() || !isRegisteredRegState(_reg_state))
        return;
    if (_signal_quality.length() == 0 || !hasMeaningfulSignal(_signal_quality))
        return;
    if (_last_logged_signal == _signal_quality)
        return;
    _last_logged_signal = _signal_quality;
    _log.info(F("GSM"), F("Signal: %s"), _signal_quality.c_str());
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
        logOperatorIfChanged_();
        return;
    }
    if (n == "CSQ")
    {
        int rssi = -1;
        int ber = -1;
        int dbm = 0;
        if (!parseCsq(line, rssi, ber) || rssi == 99 || (rssi == 0 && ber == 0) || !csqToDbm(rssi, dbm))
        {
            _signal_quality = "";
            return;
        }
        _signal_quality = String(dbm);
        _signal_quality += " dBm (";
        _signal_quality += signalQualityLabel(rssi);
        _signal_quality += ")";
        logSignalIfChanged_();
        return;
    }
    if (n == "CREG?")
    {
        int pos = line.indexOf(':');
        _reg_status = (pos >= 0) ? line.substring(pos + 1) : line;
        _reg_status.trim();
        uint8_t reg_state = 0xFF;
        if (parseRegState_(line, reg_state))
            updateRegState_(reg_state);
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
    if (_reg_status.length())
        _log.info(F("GSM"), F("Registration: %s"), _reg_status.c_str());
    if (isRegisteredRegState(_reg_state))
    {
        logOperatorIfChanged_();
        logSignalIfChanged_();
    }
    else if (_reg_status.length())
    {
        _log.info(F("GSM"), F("Operator/signal pending registration"));
    }
    if (_imsi.length())
        _log.info(F("GSM"), F("IMSI: %s"), _imsi.c_str());
}

void GsmModem::ensureModemLock_()
{
    if (_modem_mtx == nullptr)
        _modem_mtx = xSemaphoreCreateRecursiveMutex();
}

bool GsmModem::lockModem_(uint32_t timeout_ms)
{
    ensureModemLock_();
    if (_modem_mtx == nullptr)
        return false;
    return xSemaphoreTakeRecursive(_modem_mtx, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void GsmModem::unlockModem_()
{
    if (_modem_mtx)
        xSemaphoreGiveRecursive(_modem_mtx);
}
