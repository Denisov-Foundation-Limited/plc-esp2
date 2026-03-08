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

Logger::Logger(UartManager &uart) : uart_(uart){}

void Logger::begin(Stream &out){ _out = &out; }

bool Logger::ready() const{ return _out != nullptr; }

void Logger::setRtc(RTC &rtc){ _rtc = &rtc; }

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
    strncpy(_recent[_recent_head], line, LOGGER_BUFFER_SIZE - 1);
    _recent[_recent_head][LOGGER_BUFFER_SIZE - 1] = '\0';
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
        Ds3231Mz::DateTime dt{};
        if (_rtc->Time(dt))
        {
            char date_buf[16] = {};
            char time_buf[16] = {};
            snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
                     (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
            snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
                     (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
            snprintf(out, cap, "[%s][%s]", date_buf, time_buf);
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

void Logger::lock_(){
#if defined(ESP32)
    if (_lock == nullptr)
        _lock = xSemaphoreCreateMutex();
    if (_lock)
        xSemaphoreTake(_lock, portMAX_DELAY);
#endif
}

void Logger::unlock_(){
#if defined(ESP32)
    if (_lock)
        xSemaphoreGive(_lock);
#endif
}
