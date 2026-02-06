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

#include "hal/io_stack.hpp"

class PlcScanLoop
{
public:
    static constexpr uint32_t kCycleUs = 1000; // 1ms fixed PLC scan cycle

    explicit PlcScanLoop(IoStack &io) : _io(io) {}

    void begin(uint32_t cycle_us = kCycleUs)
    {
        _cycle_us = (cycle_us == 0) ? kCycleUs : cycle_us;
        _next_us = micros_();
        _missed = 0;
        _io.initImages();
    }

    void tick()
    {
        const uint32_t now = micros_();
        if ((int32_t)(now - _next_us) < 0)
            return;

        if (_cycle_us > 0)
        {
            const uint32_t late = (uint32_t)(now - _next_us);
            if (late >= _cycle_us)
            {
                _missed += late / _cycle_us;
                _next_us = now;
            }
        }

        runCycle_();
        _next_us += _cycle_us;
    }

    uint32_t timeToNextUs() const
    {
        const uint32_t now = micros_();
        if ((int32_t)(_next_us - now) <= 0)
            return 0;
        return _next_us - now;
    }

    uint32_t missedCycles() const { return _missed; }

private:
    void runCycle_()
    {
        _io.scanInputs();
        _io.applyOutputs();
    }

    static uint32_t micros_()
    {
#if defined(ARDUINO)
        return (uint32_t)::micros();
#else
        return 0;
#endif
    }

    IoStack &_io;
    uint32_t _cycle_us = kCycleUs;
    uint32_t _next_us = 0;
    uint32_t _missed = 0;
};
