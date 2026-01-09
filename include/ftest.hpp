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
#include "hal/gpio/gpio_caps.hpp"
#include "hal/gpio/portio.hpp"
#include "utils/logger.hpp"

class Ftest
{
public:
    explicit Ftest(Logger &logs, PortIO &portio, TaskManager<TASK_MGR_TSK_COUNT> &tm,
                   TaskBinder<TASK_MGR_TSK_COUNT> &tb)
        : _logs(logs), _portio(portio), _tm(tm), _tb(tb)
    {
    }

    void start()
    {
        const auto h = _tb.getFtestTask();
        if (h)
            _tm.enable(h, true);
    }

    void task()
    {
        _state = !_state;

        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = _portio.desc(i);
            if (p.caps == Cap::None)
                continue;

            if (p.type == PortIO::PinType::Led ||
                p.type == PortIO::PinType::Relay ||
                p.type == PortIO::PinType::Buzzer ||
                p.type == PortIO::PinType::Fan)
            {
                if (has(p.caps, Cap::Output))
                {
                    _portio.write(i, _state);
                    _logs.info(F("FTEST"), F("GPIO[%u]=%u type=%s"), i, _state ? 1u : 0u, pinTypeName_(p.type));
                }
                continue;
            }

            if (p.type == PortIO::PinType::Button)
            {
                bool v = _portio.read(i);
                _logs.info(F("FTEST"), F("BUTTON[%u]=%u"), i, v ? 1u : 0u);
            }
        }
    }

private:
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

    Logger &_logs;
    PortIO &_portio;
    TaskManager<TASK_MGR_TSK_COUNT> &_tm;
    TaskBinder<TASK_MGR_TSK_COUNT> &_tb;
    bool _state = false;
};
