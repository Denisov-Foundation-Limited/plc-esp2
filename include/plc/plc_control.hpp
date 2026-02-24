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
#include <stdint.h>
#include <math.h>

#include "boards/board_profile.hpp"
#include "core/rtc.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/io_stack.hpp"
#include "hal/lm75ad.hpp"

class PlcControl
{
public:
    enum class AlarmModule : uint8_t
    {
        Sockets = 0,
        Lights,
        Meteo,
        Thermo,
        Tanks,
        Septic,
        Security,
        Ring
    };
    static constexpr size_t kAlarmModuleCount = 8;

    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        InvalidConfig,
        I2c
    };

    PlcControl(I2CManager &i2c, IoStack &io, RTC &rtc);

    bool begin();
    void task();

    float boardTemp() const;
    float cpuTemp() const;
    bool fanStatus() const;
    const String &deviceName() const;
    void setDeviceName(const String &name);
    bool buzzerEnabled() const;
    void setBuzzerEnabled(bool enabled);
    bool fanManualMode() const;
    float fanOnC() const;
    float fanHysteresisC() const;

    bool portState(uint8_t id, bool &out) const;

    void setFanAuto();
    void setFanManual(bool on);
    void setFanThresholds(float on_c, float hyst_c);

    void setAlarmMask(uint32_t mask);
    uint32_t alarmMask() const;
    void setAlarmUnitMask(AlarmModule module, uint32_t mask);
    uint32_t alarmUnitMask(AlarmModule module) const;
    void setAlarmDetailMask(AlarmModule module, uint32_t mask);
    uint32_t alarmDetailMask(AlarmModule module) const;
    void setAlarmModule(AlarmModule module, bool has_alarm);
    void setAlarm(AlarmModule module);
    void clearAlarm(AlarmModule module);
    void setAlarmModule(uint8_t module, bool has_alarm);
    void setAlarmDetail(AlarmModule module, uint8_t bit, bool has_alarm);
    void setAlarmDetailBit(AlarmModule module, uint8_t bit);
    void clearAlarmDetailBit(AlarmModule module, uint8_t bit);
    void setAlarmUnit(AlarmModule module, uint8_t unit_bit, bool has_alarm);

    Error lastError() const;

private:
    static constexpr bool busExists_(uint8_t bus_num)
    {
        for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
        {
            if (ActiveBoardProfile::I2CS[i].bus_num == bus_num)
                return true;
        }
        return false;
    }

    void setFans_(bool on);
    static float readCpuTemp_();
    void setAlarmModule_(uint8_t module, bool has_alarm);
    void setAlarmDetail_(AlarmModule module, uint8_t bit_index, bool has_alarm);
    void ensureAlarmLed_();
    void setAlarmLed_(bool on);
    void updateAlarmLed_(uint32_t now);

    Lm75ad _lm75;
    I2CManager &_i2c;
    IoStack &_io;
    RTC &_rtc;
    Error _err = Error::Ok;
    bool _fan_on = false;
    bool _fan_manual = false;
    float _fan_on_c = 0.0f;
    float _fan_hyst_c = 0.0f;
    float _last_temp_c = 0.0f;
    float _last_cpu_temp_c = 0.0f;
    float _last_rtc_temp_c = 0.0f;
    bool _temp_valid = false;
    bool _cpu_temp_valid = false;
    bool _rtc_temp_valid = false;
    String _device_name;
    uint32_t _sample_interval_ms = 1000;
    uint32_t _next_sample_ms = 0;
    uint32_t _alarm_mask = 0;
    uint32_t _alarm_detail_mask[kAlarmModuleCount] = {};
    uint32_t _alarm_unit_mask[kAlarmModuleCount] = {};
    bool _alarm_led_ready = false;
    bool _alarm_led_state = false;
    uint32_t _alarm_blink_ms = 500;
    uint32_t _alarm_next_toggle_ms = 0;
    bool _buzzer_enabled = true;


    static bool timeDue_(uint32_t now, uint32_t at);
};
