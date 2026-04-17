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

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/task.h>

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
#define LOGGER_USE_COLOR 0
#endif

class Logger
{
public:
    using OutputObserver = void (*)(void *ctx);
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

    class OutputGuard
    {
    public:
        OutputGuard() { Logger::lockOutput_(); }
        ~OutputGuard() { Logger::unlockOutput_(); }
    };

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
        notifyObserver_();
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
    void setOutputObserver(OutputObserver cb, void *ctx);
    static void setInteractiveOpen(bool open);
    static bool interactiveOpen();

private:
    Stream *_out = nullptr;
    UartManager &uart_;
    RTC *_rtc = nullptr;
    mutable bool _has_last_rtc = false;
    mutable Ds3231Mz::DateTime _last_rtc = {};
    mutable uint32_t _last_rtc_ms = 0;
    OutputObserver _observer = nullptr;
    void *_observer_ctx = nullptr;
    static SemaphoreHandle_t _output_lock;
    static portMUX_TYPE _output_lock_init_mux;
    static bool _interactive_open;
    static constexpr size_t kQueueLineSize = LOGGER_BUFFER_SIZE + 52;
    static constexpr size_t kQueueDepth = 64;
    struct QueueItem
    {
        uint16_t len = 0;
        char data[kQueueLineSize] = {};
    };
    static constexpr size_t kRecentMax = 30;
    static constexpr size_t kRecentLineSize = LOGGER_BUFFER_SIZE + 48;
    char _recent[kRecentMax][kRecentLineSize] = {};
    uint8_t _recent_head = 0;
    uint8_t _recent_count = 0;
    QueueHandle_t _queue = nullptr;
    TaskHandle_t _task = nullptr;

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
        if (_interactive_open)
            _interactive_open = false;
#if LOGGER_USE_COLOR
        char framed[kQueueLineSize] = {};
        const char *color = reinterpret_cast<const char *>(color_<L>());
        const size_t color_len = strlen_P(color);
        const size_t line_len = strnlen(line, sizeof(line));
        size_t pos = 0;
        if (color_len < sizeof(framed))
        {
            memcpy(framed + pos, color, color_len);
            pos += color_len;
        }
        const size_t copy_len = (pos + line_len + 4 <= sizeof(framed)) ? line_len : (sizeof(framed) - pos - 4);
        memcpy(framed + pos, line, copy_len);
        pos += copy_len;
        memcpy(framed + pos, "\r\n\x1b[0m", 4);
        pos += 4;
        enqueueLine_(framed, pos);
#else
        char framed[LOGGER_BUFFER_SIZE + 52] = {};
        const size_t line_len = strnlen(line, sizeof(line));
        memcpy(framed, line, line_len);
        framed[line_len] = '\r';
        framed[line_len + 1] = '\n';
        enqueueLine_(framed, line_len + 2);
#endif

        storeLine_(line);
    }

    template <Level L>
    void writeJson_(const __FlashStringHelper *tag, const char *msg)
    {
        StaticJsonDocument<512> doc;
        if (_interactive_open)
        {
            _out->print('\r');
            _out->println();
            _interactive_open = false;
        }
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

        char framed[kQueueLineSize] = {};
        size_t len = serializeJson(doc, framed, sizeof(framed) - 2);
        if (len == 0 || len > sizeof(framed) - 2)
            len = 0;
        framed[len] = '\r';
        framed[len + 1] = '\n';
        enqueueLine_(framed, len + 2);

        char line[LOGGER_BUFFER_SIZE] = {};
        if (serializeJson(doc, line, sizeof(line)) == 0)
            strncpy(line, "{}", sizeof(line) - 1);
        storeLine_(line);
    }

    void storeLine_(const char *line);void buildTextLine_(char *out, size_t cap, const __FlashStringHelper *tag,
                        const char *level, const char *msg);bool formatTimestamp_(char *out, size_t cap);void notifyObserver_();static void ensureLock_();static void lockOutput_();static void unlockOutput_();void lock_();void unlock_();void ensureQueue_();void enqueueLine_(const char *data, size_t len);static void loggerTaskEntry_(void *ctx);void loggerTask_();};
