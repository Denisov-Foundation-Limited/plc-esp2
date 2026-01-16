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
#include <vector>

#include "core/task_manager.hpp"
#include "core/task_binder.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/ds18b20.hpp"
#include "hal/ibutton.hpp"
#include "hal/gpio/gpio_caps.hpp"
#include "hal/io_stack.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/at24lc512.hpp"
#include "hal/lm75ad.hpp"
#include "boards/board_profile.hpp"
#include "core/rtc.hpp"
#include "utils/logger.hpp"

class Ftest
{
public:
    explicit Ftest(Logger &logs, IoStack &io, OneWireManager &ow, IButton &ibutton,
                   Ds18b20 &ds18b20, I2CManager &i2c, RTC &rtc, Extender &ext,
                   TaskManager<TASK_MGR_TSK_COUNT> &tm,
                   TaskBinder<TASK_MGR_TSK_COUNT> &tb)
        : _logs(logs), _io(io), _ow(ow), _ibutton(ibutton),
          _ds18b20(ds18b20), _i2c(i2c), _rtc(rtc), _ext(ext), _tm(tm), _tb(tb)
    {
    }

    void start()
    {
        initOneWire_();
        initBoardTemp_();
        initEeprom_();

        const auto h = _tb.getFtestTask();
        if (h)
            _tm.enable(h, true);
    }

    void task()
    {
        _state = !_state;

        _logs.info(F("FTEST"), F("===================================================="));
        _logs.info(F("FTEST"), F(""));
        _logs.info(F("FTEST"), F("[OUTPUTS]"));
        logGpioOuts_();

        _logs.info(F("FTEST"), F(""));
        _logs.info(F("FTEST"), F("[INPUTS]"));
        logGpioIns_();

        _logs.info(F("FTEST"), F(""));
        _logs.info(F("FTEST"), F("[BUTTONS]"));
        logButtons_();

        _logs.info(F("FTEST"), F(""));
        _logs.info(F("FTEST"), F("[DS18B20]"));
        logDs18b20_();

        _logs.info(F("FTEST"), F(""));
        _logs.info(F("FTEST"), F("[IBUTTON]"));
        logIbutton_();

        _logs.info(F("FTEST"), F(""));
        _logs.info(F("FTEST"), F("[BOARD_TEMP]"));
        logBoardTemp_();

        _logs.info(F("FTEST"), F(""));
        _logs.info(F("FTEST"), F("[CPU_TEMP]"));
        logCPUTemp_();

        _logs.info(F("FTEST"), F(""));
        _logs.info(F("FTEST"), F("[RTC]"));
        logRtc_();

        _logs.info(F("FTEST"), F(""));
        _logs.info(F("FTEST"), F("[EEPROM]"));
        logEeprom_();
    }

private:
    void initEeprom_()
    {
        const auto cfg = ActiveBoardProfile::EEPROM;
        TwoWire *wire = _i2c.wirePtr(cfg.bus_num);
        if (!wire || !_eeprom.begin(*wire, cfg.addr))
        {
            _eeprom_ok = false;
            _logs.error(F("FTEST"), F("EEPROM init failed"));
            return;
        }
        _eeprom_ok = true;
    }

    void logEeprom_()
    {
        if (!_eeprom_ok)
        {
            _logs.info(F("FTEST"), F("EEPROM: err"));
            return;
        }
        const uint32_t used = _eeprom.usedBytes();
        const uint32_t total = At24lc512::capacityBytes();
        const uint32_t free = At24lc512::remainingBytes(used);
        _logs.info(F("FTEST"), F("EEPROM used: %lu free: %lu total: %lu"),
                   (unsigned long)used, (unsigned long)free, (unsigned long)total);
    }
    void initBoardTemp_()
    {
        const auto cfg = ActiveBoardProfile::BOARD_TEMP;
        TwoWire *wire = _i2c.wirePtr(cfg.bus_num);
        if (!wire || !_lm75.begin(*wire, cfg.addr))
        {
            _lm75_ok = false;
            _logs.error(F("FTEST"), F("LM75 init failed"));
            return;
        }
        _lm75_ok = true;
    }

    void logBoardTemp_()
    {
        if (!_lm75_ok)
        {
            _logs.info(F("FTEST"), F("LM75: err"));
            return;
        }
        float t = 0.0f;
        if (!_lm75.readTempC(t))
        {
            _logs.info(F("FTEST"), F("BOARD: err"));
            return;
        }
        _logs.info(F("FTEST"), F("BOARD: %.2fC"), t);
    }

    void logCPUTemp_()
    {
#if defined(ESP32)
        float t = temperatureRead();
        _logs.info(F("FTEST"), F("CPU: %.2fC"), t);
#else
        _logs.info(F("FTEST"), F("TSENS: na"));
#endif
    }

    void logRtc_()
    {
        Ds3231Mz::DateTime dt{};
        if (!_rtc.Time(dt))
        {
            _logs.info(F("FTEST"), F("RTC: err"));
            return;
        }
        float rtc_t = 0.0f;
        const bool rtc_ok = _rtc.readTemp(rtc_t);
        char date_buf[16] = {};
        char time_buf[16] = {};
        snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
                 (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
                 (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
        if (rtc_ok)
            _logs.info(F("FTEST"), F("RTC: %s %s %.2fC"), date_buf, time_buf, rtc_t);
        else
            _logs.info(F("FTEST"), F("RTC: %s %s temp: err"), date_buf, time_buf);
    }

    void logGpioOuts_()
    {
        for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
        {
            const auto &p = _io.desc(i);
            if (p.caps == Cap::None)
                continue;
            if (!isPortActive_(p))
                continue;

            if (p.type == PortIO::PinType::Led ||
                p.type == PortIO::PinType::Relay ||
                p.type == PortIO::PinType::Buzzer ||
                p.type == PortIO::PinType::Fan)
            {
                if (has(p.caps, Cap::Output))
                {
                    _io.write(i, _state);
                    _logs.info(F("FTEST"), F("GPIO[%u]: State: %s type: %s"), i, _state ? "High" : "Low", pinTypeName_(p.type));
                }
                continue;
            }
        }
    }

    void logGpioIns_()
    {
        for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
        {
            const auto &p = _io.desc(i);
            if (p.caps == Cap::None)
                continue;
            if (!isPortActive_(p))
                continue;

            if (p.type == PortIO::PinType::DInput)
            {
                if (has(p.caps, Cap::Input))
                {
                    const bool v = _io.read(i);
                    _logs.info(F("FTEST"), F("GPIO[%u]: State: %s type: %s"), i, v ? "High" : "Low", pinTypeName_(p.type));
                }
                continue;
            }
        }
    }

    void logButtons_()
    {
        for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
        {
            const auto &p = _io.desc(i);
            if (p.caps == Cap::None)
                continue;
            if (!isPortActive_(p))
                continue;

            if (p.type == PortIO::PinType::Button)
            {
                bool v = _io.read(i);
                _logs.info(F("FTEST"), F("BUTTON[%u]: State: %s"), i, v ? "High" : "Low");
            }
        }
    }

    void logDs18b20_()
    {
        if (!_ds18b20_ok)
        {
            OneWireBus *temp_bus = _ow.busPtrById(OneWireManager::OwBusType::Temp);
            if (!temp_bus)
            {
                _logs.info(F("FTEST"), F("DS18B20: bus missing"));
                return;
            }
            _ds18b20.begin(*temp_bus);
            _ds18b20_ok = true;
        }
        std::vector<String> serials;
        _ds18b20.listSerials(serials);
        if (serials.empty())
        {
            _logs.info(F("FTEST"), F("DS18B20: none"));
            return;
        }
        for (size_t i = 0; i < serials.size(); ++i)
        {
            _logs.info(F("FTEST"), F("DS18B20[%u]: %s"), (unsigned)i, serials[i].c_str());
        }
    }

    void logIbutton_()
    {
        uint8_t addr[8] = {};
        if (_ibutton.readSerial(addr))
        {
            char hex[17] = {};
            IButton::toHex(addr, hex);
            _logs.info(F("FTEST"), F("IBUTTON: %s"), hex);
        }
    }

    void initOneWire_()
    {
        OneWireBus *ib_bus = _ow.busPtrById(OneWireManager::OwBusType::iButton);
        if (!ib_bus || !_ibutton.begin(*ib_bus))
        {
            _logs.error(F("FTEST"), F("OW iButton bus missing"));
        }
        OneWireBus *temp_bus = _ow.busPtrById(OneWireManager::OwBusType::Temp);
        if (!temp_bus)
        {
            _ds18b20_ok = false;
            _logs.error(F("FTEST"), F("OW DS18B20 bus missing"));
            return;
        }
        _ds18b20.begin(*temp_bus);
        _ds18b20_ok = true;
    }

    static const char *pinTypeName_(PortIO::PinType t)
    {
        switch (t)
        {
        case PortIO::PinType::System:
            return kTypeSystem;
        case PortIO::PinType::Relay:
            return kTypeRelay;
        case PortIO::PinType::Led:
            return kTypeLed;
        case PortIO::PinType::Sensor:
            return kTypeSensor;
        case PortIO::PinType::Button:
            return kTypeButton;
        case PortIO::PinType::DInput:
            return kTypeDInput;
        case PortIO::PinType::Buzzer:
            return kTypeBuzzer;
        case PortIO::PinType::Fan:
            return kTypeFan;
        default:
            return kTypeUnknown;
        }
    }

    static constexpr char kTypeSystem[] PROGMEM = "System";
    static constexpr char kTypeRelay[] PROGMEM = "Relay";
    static constexpr char kTypeLed[] PROGMEM = "Led";
    static constexpr char kTypeSensor[] PROGMEM = "Sensor";
    static constexpr char kTypeButton[] PROGMEM = "Button";
    static constexpr char kTypeDInput[] PROGMEM = "DInput";
    static constexpr char kTypeBuzzer[] PROGMEM = "Buzzer";
    static constexpr char kTypeFan[] PROGMEM = "Fan";
    static constexpr char kTypeUnknown[] PROGMEM = "Unknown";

    bool isPortActive_(const PortIO::PortDesc &p) const
    {
        if (p.backend != PortIO::Backend::Extender)
            return true;
        return _ext.isPresent(p.u.ext.dev);
    }

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
