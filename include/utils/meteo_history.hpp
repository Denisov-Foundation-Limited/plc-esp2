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
#include <LittleFS.h>
#include <math.h>

#include "controllers/meteo_controller.hpp"
#include "core/rtc.hpp"

class MeteoHistory
{
public:
    static constexpr const char *kPath = "/meteo_hist.bin";

    MeteoHistory(RTC &rtc, MeteoController &meteo) : _rtc(rtc), _meteo(meteo) {}

    void task()
    {
        Ds3231Mz::DateTime dt{};
        if (!_rtc.Time(dt))
            return;
        const uint32_t date = dateToInt_(dt);
        const uint8_t hour = dt.hour;
        if (hour > 23)
            return;
        if (date != _date)
        {
            if (!initFile_(date))
                return;
            _date = date;
            _last_hour = 0xFF;
        }
        if (_last_hour == hour)
            return;
        if (writeHour_(date, hour))
            _last_hour = hour;
    }

private:
    static constexpr uint32_t kMagic = 0x4D544831u; // "MTH1"
    static constexpr int16_t kInvalid = 0x7FFF;
    static constexpr size_t kHours = 24;

    RTC &_rtc;
    MeteoController &_meteo;
    uint32_t _date = 0;
    uint8_t _last_hour = 0xFF;

    static uint32_t dateToInt_(const Ds3231Mz::DateTime &dt)
    {
        return (uint32_t)dt.year * 10000u + (uint32_t)dt.month * 100u + (uint32_t)dt.day;
    }

    static size_t entryOffset_(uint8_t sensor_index, uint8_t hour)
    {
        const size_t index = (size_t)sensor_index * kHours + hour;
        return sizeof(uint32_t) + sizeof(uint32_t) + index * sizeof(int16_t) * 2;
    }

    bool initFile_(uint32_t date)
    {
        File f = LittleFS.open(kPath, "w");
        if (!f)
            return false;
        const uint32_t magic = kMagic;
        if (f.write(reinterpret_cast<const uint8_t *>(&magic), sizeof(magic)) != sizeof(magic) ||
            f.write(reinterpret_cast<const uint8_t *>(&date), sizeof(date)) != sizeof(date))
        {
            f.close();
            return false;
        }
        const int16_t invalid = kInvalid;
        for (size_t i = 0; i < MeteoController::kSensorCount * kHours; ++i)
        {
            f.write(reinterpret_cast<const uint8_t *>(&invalid), sizeof(invalid));
            f.write(reinterpret_cast<const uint8_t *>(&invalid), sizeof(invalid));
        }
        f.close();
        return true;
    }

    bool loadHeader_(File &f, uint32_t &date_out)
    {
        uint32_t magic = 0;
        if (f.read(reinterpret_cast<uint8_t *>(&magic), sizeof(magic)) != sizeof(magic))
            return false;
        if (magic != kMagic)
            return false;
        if (f.read(reinterpret_cast<uint8_t *>(&date_out), sizeof(date_out)) != sizeof(date_out))
            return false;
        return true;
    }

    bool writeHour_(uint32_t date, uint8_t hour)
    {
        File f = LittleFS.open(kPath, "r+");
        if (!f)
        {
            if (!initFile_(date))
                return false;
            f = LittleFS.open(kPath, "r+");
            if (!f)
                return false;
        }
        uint32_t stored_date = 0;
        if (!loadHeader_(f, stored_date) || stored_date != date)
        {
            f.close();
            if (!initFile_(date))
                return false;
            f = LittleFS.open(kPath, "r+");
            if (!f)
                return false;
            if (!loadHeader_(f, stored_date) || stored_date != date)
            {
                f.close();
                return false;
            }
        }

        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            if (!writeEntry_(f, i, hour))
            {
                f.close();
                return false;
            }
        }
        f.close();
        return true;
    }

    bool writeEntry_(File &f, size_t index, uint8_t hour)
    {
        const auto *cfg = _meteo.configByIndex(index);
        const auto *st = _meteo.stateByIndex(index);
        int16_t t10 = kInvalid;
        int16_t h10 = kInvalid;
        if (cfg && st && cfg->enabled)
        {
            if (st->has_temp)
            {
                const float v = st->temp_c * 10.0f;
                const float clamped = min(max(v, -32766.0f), 32766.0f);
                t10 = (int16_t)lroundf(clamped);
            }
            if (st->has_humidity)
            {
                const float v = st->humidity * 10.0f;
                const float clamped = min(max(v, -32766.0f), 32766.0f);
                h10 = (int16_t)lroundf(clamped);
            }
        }
        const size_t off = entryOffset_((uint8_t)index, hour);
        if (!f.seek(off, SeekSet))
            return false;
        f.write(reinterpret_cast<const uint8_t *>(&t10), sizeof(t10));
        f.write(reinterpret_cast<const uint8_t *>(&h10), sizeof(h10));
        return true;
    }
};
