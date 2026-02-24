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

#include "core/task_manager.hpp"
#include "core/task_binder.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/ds18b20.hpp"
#include "hal/ibutton.hpp"
#include "hal/io_stack.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/at24lc512.hpp"
#include "hal/lm75ad.hpp"
#include "core/rtc.hpp"
#include "utils/logger.hpp"

class Ftest
{
public:
    explicit Ftest(Logger &logs, IoStack &io, OneWireManager &ow, IButton &ibutton,
                   Ds18b20 &ds18b20, I2CManager &i2c, RTC &rtc, Extender &ext,
                   TaskManager<TASK_MGR_TSK_COUNT> &tm,
                   TaskBinder<TASK_MGR_TSK_COUNT> &tb);

    void start();
    void task();

private:
    void initEeprom_();
    void logEeprom_();
    void initBoardTemp_();
    void logBoardTemp_();
    void logCPUTemp_();
    void logRtc_();
    void logGpioOuts_();
    void logGpioIns_();
    void logButtons_();
    void logDs18b20_();
    void logIbutton_();
    void initOneWire_();
    static const char *pinTypeName_(PortIO::PinType t);

    static constexpr char kTypeSystem[] PROGMEM = "System";
    static constexpr char kTypeRelay[] PROGMEM = "Relay";
    static constexpr char kTypeLed[] PROGMEM = "Led";
    static constexpr char kTypeSensor[] PROGMEM = "Sensor";
    static constexpr char kTypeButton[] PROGMEM = "Button";
    static constexpr char kTypeDInput[] PROGMEM = "DInput";
    static constexpr char kTypeBuzzer[] PROGMEM = "Buzzer";
    static constexpr char kTypeFan[] PROGMEM = "Fan";
    static constexpr char kTypeUnknown[] PROGMEM = "Unknown";

    bool isPortActive_(const PortIO::PortDesc &p) const;

    Logger &_logs;
    IoStack &_io;
    OneWireManager &_ow;
    IButton &_ibutton;
    Ds18b20 &_ds18b20;
    I2CManager &_i2c;
    RTC &_rtc;
    Extender &_ext;
    Lm75ad _lm75;
    bool _lm75_ok = false;
    At24lc512 _eeprom;
    bool _eeprom_ok = false;
    bool _ds18b20_ok = false;
    TaskManager<TASK_MGR_TSK_COUNT> &_tm;
    TaskBinder<TASK_MGR_TSK_COUNT> &_tb;
    bool _state = false;
};
