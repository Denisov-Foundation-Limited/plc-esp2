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

#include "core/plc_scan.hpp"

#include <Arduino.h>

#include "hal/io_stack.hpp"

PlcScanLoop::PlcScanLoop(IoStack &io) : _io(io) {}

void PlcScanLoop::begin(uint32_t cycle_us)
{
    _cycle_us = (cycle_us == 0) ? kCycleUs : cycle_us;
    _next_us = micros_();
    _missed = 0;
    _io.initImages();
}

void PlcScanLoop::tick()
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

uint32_t PlcScanLoop::timeToNextUs() const
{
    const uint32_t now = micros_();
    if ((int32_t)(_next_us - now) <= 0)
        return 0;
    return _next_us - now;
}

uint32_t PlcScanLoop::missedCycles() const
{
    return _missed;
}

void PlcScanLoop::runCycle_()
{
    _io.scanInputs();
    _io.applyOutputs();
}

uint32_t PlcScanLoop::micros_()
{
#if defined(ARDUINO)
    return (uint32_t)::micros();
#else
    return 0;
#endif
}
