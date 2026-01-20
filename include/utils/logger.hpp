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
#include <stdarg.h>
#include <string.h>

#include "boards/board_profile.hpp"
#include "boards/board_profile_base.hpp"
#include "core/rtc.hpp"

#include "hal/bus/uart.hpp"

// 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE
#ifndef LOGGER_LEVEL
#define LOGGER_LEVEL 3
#endif

#ifndef LOGGER_USE_TIMESTAMP
#define LOGGER_USE_TIMESTAMP 1
#endif

#ifndef LOGGER_BUFFER_SIZE
#define LOGGER_BUFFER_SIZE 160
#endif

// 0 = human text, 1 = ArduinoJson NDJSON
#ifndef LOGGER_FORMAT_JSON
#define LOGGER_FORMAT_JSON 0
#endif

#ifndef LOGGER_USE_COLOR
#define LOGGER_USE_COLOR 1
#endif

class Logger
{
public:
    enum class Level : uint8_t
    {
        Off = 0,
        Error = 1,
        Warn = 2,
        Info = 3,
        Debug = 4,
        Trace = 5
    };

    Logger(UartManager &uart) : uart_(uart) {};

    void begin(Stream &out) { _out = &out; }
    bool ready() const { return _out != nullptr; }
    void setRtc(RTC &rtc) { _rtc = &rtc; }
    size_t recentCount() const { return _recent_count; }
    bool getRecentLine(size_t idx, char *out, size_t cap) const
    {
        if (!out || cap == 0)
            return false;
        if (idx >= _recent_count)
            return false;
        const size_t start = (_recent_count < kRecentMax) ? 0 : _recent_head;
        const size_t pos = (start + idx) % kRecentMax;
        strncpy(out, _recent[pos], cap - 1);
        out[cap - 1] = '\0';
        return true;
    }

    bool beginAuto()
    {
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

    template <Level L>
    inline void log(const __FlashStringHelper *tag,
                    const __FlashStringHelper *fmt, ...)
    {
        if constexpr (!enabled<L>())
            return;
        if (!_out)
            return;

        char msg[LOGGER_BUFFER_SIZE];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf_P(msg, sizeof(msg), (const char *)fmt, ap);
        va_end(ap);

#if LOGGER_FORMAT_JSON
        writeJson_<L>(tag, msg);
#else
        writeText_<L>(tag, msg);
#endif
    }

    // ISR-safe: no ArduinoJson; minimal output
    template <Level L>
    inline void logISR(const __FlashStringHelper *tag,
                       const __FlashStringHelper *msg)
    {
        if constexpr (!enabled<L>())
            return;
        if (!_out)
            return;

        _out->print(F("["));
        _out->print(levelName_(L));
        _out->print(F("]["));
        _out->print(tag);
        _out->print(F("] "));
        _out->println(msg);
    }

    template <typename... Args>
    inline void error(const __FlashStringHelper *t, const __FlashStringHelper *f, Args... a) { log<Level::Error>(t, f, a...); }
    template <typename... Args>
    inline void warn(const __FlashStringHelper *t, const __FlashStringHelper *f, Args... a) { log<Level::Warn>(t, f, a...); }
    template <typename... Args>
    inline void info(const __FlashStringHelper *t, const __FlashStringHelper *f, Args... a) { log<Level::Info>(t, f, a...); }
    template <typename... Args>
    inline void debug(const __FlashStringHelper *t, const __FlashStringHelper *f, Args... a) { log<Level::Debug>(t, f, a...); }
    template <typename... Args>
    inline void trace(const __FlashStringHelper *t, const __FlashStringHelper *f, Args... a) { log<Level::Trace>(t, f, a...); }

private:
    Stream *_out = nullptr;
    UartManager &uart_;
    RTC *_rtc = nullptr;
    static constexpr size_t kRecentMax = 30;
    char _recent[kRecentMax][LOGGER_BUFFER_SIZE] = {};
    uint8_t _recent_head = 0;
    uint8_t _recent_count = 0;

    template <Level L>
    static constexpr bool enabled() { return (uint8_t)L <= LOGGER_LEVEL; }

    static inline char levelChar_(Level l)
    {
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

    static inline const char *levelName_(Level l)
    {
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

    template <Level L>
    static inline const __FlashStringHelper *color_()
    {
        switch (L)
        {
        case Level::Error:
            return F("\x1b[31m");
        case Level::Warn:
            return F("\x1b[33m");
        case Level::Info:
            return F("\x1b[32m");
        case Level::Debug:
            return F("\x1b[36m");
        case Level::Trace:
            return F("\x1b[90m");
        default:
            return F("");
        }
    }

    template <Level L>
    void writeText_(const __FlashStringHelper *tag, const char *msg)
    {
#if LOGGER_USE_COLOR
        _out->print(color_<L>());
#endif
#if LOGGER_USE_TIMESTAMP
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
                _out->print(F("["));
                _out->print(date_buf);
                _out->print(F("]["));
                _out->print(time_buf);
                _out->print(F("]"));
            }
            else
            {
                _out->print(F("["));
                _out->print((uint32_t)millis());
                _out->print(F("]"));
            }
        }
        else
        {
            _out->print(F("["));
            _out->print((uint32_t)millis());
            _out->print(F("]"));
        }
#endif
        _out->print(F("["));
        _out->print(levelName_(L));
        _out->print(F("]["));
        _out->print(tag);
        _out->print(F("] "));
        _out->println(msg);
#if LOGGER_USE_COLOR
        _out->print(F("\x1b[0m"));
#endif

        char line[LOGGER_BUFFER_SIZE] = {};
        buildTextLine_(line, sizeof(line), tag, levelName_(L), msg);
        storeLine_(line);
    }

    template <Level L>
    void writeJson_(const __FlashStringHelper *tag, const char *msg)
    {
        StaticJsonDocument<512> doc;
#if LOGGER_USE_TIMESTAMP
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
                doc["date"] = date_buf;
                doc["time"] = time_buf;
            }
            else
            {
                doc["ts"] = (uint32_t)millis();
            }
        }
        else
        {
            doc["ts"] = (uint32_t)millis();
        }
#endif
        doc["lvl"] = levelName_(L);
        doc["tag"] = tag;
        doc["msg"] = msg;

        serializeJson(doc, *_out);
        _out->println();

        char line[LOGGER_BUFFER_SIZE] = {};
        if (serializeJson(doc, line, sizeof(line)) == 0)
            strncpy(line, "{}", sizeof(line) - 1);
        storeLine_(line);
    }

    void storeLine_(const char *line)
    {
        if (!line)
            return;
        strncpy(_recent[_recent_head], line, LOGGER_BUFFER_SIZE - 1);
        _recent[_recent_head][LOGGER_BUFFER_SIZE - 1] = '\0';
        _recent_head = (uint8_t)((_recent_head + 1) % kRecentMax);
        if (_recent_count < kRecentMax)
            ++_recent_count;
    }

    void buildTextLine_(char *out, size_t cap, const __FlashStringHelper *tag,
                        const char *level, const char *msg)
    {
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

    bool formatTimestamp_(char *out, size_t cap)
    {
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
};
