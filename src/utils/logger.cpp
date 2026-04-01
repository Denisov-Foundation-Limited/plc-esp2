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

#include "utils/logger.hpp"

#include "boards/board_profile.hpp"
#include "boards/board_profile_base.hpp"

using LoggerLevel = Logger::Level;

SemaphoreHandle_t Logger::_output_lock = nullptr;
portMUX_TYPE Logger::_output_lock_init_mux = portMUX_INITIALIZER_UNLOCKED;
bool Logger::_interactive_open = false;

namespace
{
uint8_t daysInMonth_(uint16_t year, uint8_t month)
{
    switch (month)
    {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    case 2:
        return (year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u)) ? 29 : 28;
    default:
        return 31;
    }
}

void advanceDateTime_(Ds3231Mz::DateTime &dt, uint32_t delta_sec)
{
    uint32_t sec_of_day = (uint32_t)dt.hour * 3600u + (uint32_t)dt.minute * 60u + (uint32_t)dt.second;
    uint32_t total_sec = sec_of_day + delta_sec;
    uint32_t day_carry = total_sec / 86400u;
    total_sec %= 86400u;

    dt.hour = (uint8_t)(total_sec / 3600u);
    total_sec %= 3600u;
    dt.minute = (uint8_t)(total_sec / 60u);
    dt.second = (uint8_t)(total_sec % 60u);

    while (day_carry > 0)
    {
        const uint8_t dim = daysInMonth_(dt.year, dt.month);
        if (dt.day < dim)
            ++dt.day;
        else
        {
            dt.day = 1;
            if (dt.month < 12)
                ++dt.month;
            else
            {
                dt.month = 1;
                ++dt.year;
            }
        }
        if (dt.day_of_week >= 1 && dt.day_of_week <= 7)
            dt.day_of_week = (uint8_t)((dt.day_of_week % 7u) + 1u);
        --day_carry;
    }
}

void formatDateTime_(const Ds3231Mz::DateTime &dt, char *out, size_t cap)
{
    char date_buf[16] = {};
    char time_buf[16] = {};
    snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
             (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
    snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
             (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
    snprintf(out, cap, "[%s][%s]", date_buf, time_buf);
}
} // namespace

Logger::Logger(UartManager &uart) : uart_(uart){}

void Logger::begin(Stream &out){
    ensureLock_();
    _out = &out;
}

bool Logger::ready() const{ return _out != nullptr; }

void Logger::setRtc(RTC &rtc){
    _rtc = &rtc;
    _has_last_rtc = false;
    _last_rtc_ms = 0;
    _last_rtc = {};
}

size_t Logger::recentCount() const{
    const_cast<Logger *>(this)->lock_();
    const size_t out = _recent_count;
    const_cast<Logger *>(this)->unlock_();
    return out;
}

bool Logger::getRecentLine(size_t idx, char *out, size_t cap) const{
    const_cast<Logger *>(this)->lock_();
    if (!out || cap == 0)
    {
        const_cast<Logger *>(this)->unlock_();
        return false;
    }
    if (idx >= _recent_count)
    {
        const_cast<Logger *>(this)->unlock_();
        return false;
    }
    const size_t start = (_recent_count < kRecentMax) ? 0 : _recent_head;
    const size_t pos = (start + idx) % kRecentMax;
    strncpy(out, _recent[pos], cap - 1);
    out[cap - 1] = '\0';
    const_cast<Logger *>(this)->unlock_();
    return true;
}

bool Logger::beginAuto(){
    const LogCfg cfg = ActiveBoardProfile::LOG;
    ensureLock_();

    if (cfg.sink == LogCfg::Sink::UsbSerial)
    {
        Serial.begin(cfg.usb_baud);
        begin(Serial);
        return true;
    }

    if (cfg.sink == LogCfg::Sink::UartIndex)
    {
        HardwareSerial *ser = uart_.beginSerialForIndex(cfg.uart_index);
        if (!ser)
            return false;
        begin(*ser);
        return true;
    }

    return false;
}

void Logger::setOutputObserver(OutputObserver cb, void *ctx){
    _observer = cb;
    _observer_ctx = ctx;
}

void Logger::setInteractiveOpen(bool open){
    ensureLock_();
    lockOutput_();
    _interactive_open = open;
    unlockOutput_();
}

bool Logger::interactiveOpen(){
    ensureLock_();
    lockOutput_();
    const bool out = _interactive_open;
    unlockOutput_();
    return out;
}

void Logger::ensureLock_(){
    if (_output_lock != nullptr)
        return;
    portENTER_CRITICAL(&_output_lock_init_mux);
    if (_output_lock == nullptr)
        _output_lock = xSemaphoreCreateMutex();
    portEXIT_CRITICAL(&_output_lock_init_mux);
}

char Logger::levelChar_(LoggerLevel l){
    switch (l)
    {
    case Level::Error:
        return 'E';
    case Level::Warn:
        return 'W';
    case Level::Info:
        return 'I';
    case Level::Debug:
        return 'D';
    case Level::Trace:
        return 'T';
    default:
        return '?';
    }
}

const char *Logger::levelName_(LoggerLevel l){
    switch (l)
    {
    case Level::Error:
        return "ERROR";
    case Level::Warn:
        return "WARN";
    case Level::Info:
        return "INFO";
    case Level::Debug:
        return "DEBUG";
    case Level::Trace:
        return "TRACE";
    default:
        return "UNKNOWN";
    }
}

void Logger::storeLine_(const char *line){
    if (!line)
        return;
    strncpy(_recent[_recent_head], line, kRecentLineSize - 1);
    _recent[_recent_head][kRecentLineSize - 1] = '\0';
    _recent_head = (uint8_t)((_recent_head + 1) % kRecentMax);
    if (_recent_count < kRecentMax)
        ++_recent_count;
}

void Logger::buildTextLine_(char *out, size_t cap, const __FlashStringHelper *tag,
                    const char *level, const char *msg){
    if (!out || cap == 0)
        return;
    char tag_buf[32] = {};
    if (tag)
        strncpy_P(tag_buf, reinterpret_cast<const char *>(tag), sizeof(tag_buf) - 1);
    char ts_buf[40] = {};
    if (!formatTimestamp_(ts_buf, sizeof(ts_buf)))
    {
        snprintf(out, cap, "[%s][%s] %s", level ? level : "?", tag_buf, msg ? msg : "");
        return;
    }
    snprintf(out, cap, "%s[%s][%s] %s", ts_buf, level ? level : "?", tag_buf, msg ? msg : "");
}

bool Logger::formatTimestamp_(char *out, size_t cap){
#if LOGGER_USE_TIMESTAMP
    if (!out || cap == 0)
        return false;
    if (_rtc)
    {
        const uint32_t now_ms = millis();
        Ds3231Mz::DateTime dt{};
        const bool need_refresh = !_has_last_rtc || (uint32_t)(now_ms - _last_rtc_ms) >= 1000u;
        if (need_refresh && _rtc->Time(dt))
        {
            _last_rtc = dt;
            _last_rtc_ms = now_ms;
            _has_last_rtc = true;
            formatDateTime_(dt, out, cap);
            return true;
        }
        if (_has_last_rtc)
        {
            dt = _last_rtc;
            advanceDateTime_(dt, (uint32_t)((now_ms - _last_rtc_ms) / 1000u));
            formatDateTime_(dt, out, cap);
            return true;
        }
    }
    snprintf(out, cap, "[%lu]", (unsigned long)millis());
    return true;
#else
    (void)out;
    (void)cap;
    return false;
#endif
}

void Logger::notifyObserver_(){
    if (_observer)
        _observer(_observer_ctx);
}

void Logger::lock_(){
    lockOutput_();
}

void Logger::unlock_(){
    unlockOutput_();
}

void Logger::lockOutput_(){
    ensureLock_();
    if (_output_lock)
        xSemaphoreTake(_output_lock, portMAX_DELAY);
}

void Logger::unlockOutput_(){
    if (_output_lock)
        xSemaphoreGive(_output_lock);
}
