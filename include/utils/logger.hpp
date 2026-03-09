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

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

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

    Logger(UartManager &uart);

    void begin(Stream &out);bool ready() const;void setRtc(RTC &rtc);size_t recentCount() const;bool getRecentLine(size_t idx, char *out, size_t cap) const;bool beginAuto();template <Level L>
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

        lock_();
#if LOGGER_FORMAT_JSON
        writeJson_<L>(tag, msg);
#else
        writeText_<L>(tag, msg);
#endif
        unlock_();
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

        char tag_buf[32] = {};
        char msg_buf[LOGGER_BUFFER_SIZE] = {};
        char line[LOGGER_BUFFER_SIZE + 48] = {};
        if (tag)
            strncpy_P(tag_buf, reinterpret_cast<const char *>(tag), sizeof(tag_buf) - 1);
        if (msg)
            strncpy_P(msg_buf, reinterpret_cast<const char *>(msg), sizeof(msg_buf) - 1);
        snprintf(line, sizeof(line), "[%s][%s] %s", levelName_(L), tag_buf, msg_buf);
        _out->println(line);
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
#if defined(ESP32)
    SemaphoreHandle_t _lock = nullptr;
    portMUX_TYPE _lock_init_mux = portMUX_INITIALIZER_UNLOCKED;
#endif
    static constexpr size_t kRecentMax = 30;
    static constexpr size_t kRecentLineSize = LOGGER_BUFFER_SIZE + 48;
    char _recent[kRecentMax][kRecentLineSize] = {};
    uint8_t _recent_head = 0;
    uint8_t _recent_count = 0;

    template <Level L>
    static constexpr bool enabled() { return (uint8_t)L <= LOGGER_LEVEL; }

    static char levelChar_(Level l);static const char *levelName_(Level l);template <Level L>
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
        char line[LOGGER_BUFFER_SIZE + 48] = {};
        buildTextLine_(line, sizeof(line), tag, levelName_(L), msg);
#if LOGGER_USE_COLOR
        _out->print(color_<L>());
#endif
        _out->println(line);
#if LOGGER_USE_COLOR
        _out->print(F("\x1b[0m"));
#endif

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

    void storeLine_(const char *line);void buildTextLine_(char *out, size_t cap, const __FlashStringHelper *tag,
                        const char *level, const char *msg);bool formatTimestamp_(char *out, size_t cap);void ensureLock_();void lock_();void unlock_();};
