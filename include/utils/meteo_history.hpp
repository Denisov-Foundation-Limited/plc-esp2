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

#include "controllers/meteo_controller.hpp"
#include "core/rtc.hpp"

class MeteoHistory
{
public:
    static constexpr const char *kPath = "/meteo_hist.bin";

    MeteoHistory(RTC &rtc, MeteoController &meteo);void task();private:
    static constexpr uint32_t kMagic = 0x4D544831u; // "MTH1"
    static constexpr int16_t kInvalid = 0x7FFF;
    static constexpr size_t kHours = 24;

    RTC &_rtc;
    MeteoController &_meteo;
    uint32_t _date = 0;
    uint8_t _last_hour = 0xFF;

    static uint32_t dateToInt_(const Ds3231Mz::DateTime &dt);static size_t entryOffset_(uint8_t sensor_index, uint8_t hour);bool initFile_(uint32_t date);bool loadHeader_(File &f, uint32_t &date_out);bool writeHour_(uint32_t date, uint8_t hour);bool writeEntry_(File &f, size_t index, uint8_t hour);};
