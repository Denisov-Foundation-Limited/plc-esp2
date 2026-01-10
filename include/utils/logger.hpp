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

#include "boards/board_profile.hpp"
#include "boards/board_profile_base.hpp"

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
        _out->print(F("["));
        _out->print((uint32_t)millis());
        _out->print(F("]"));
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
    }

    template <Level L>
    void writeJson_(const __FlashStringHelper *tag, const char *msg)
    {
        JsonDocument doc;
#if LOGGER_USE_TIMESTAMP
        doc["ts"] = (uint32_t)millis();
#endif
        doc["lvl"] = levelName_(L);
        doc["tag"] = tag;
        doc["msg"] = msg;

        serializeJson(doc, *_out);
        _out->println();
    }
};
