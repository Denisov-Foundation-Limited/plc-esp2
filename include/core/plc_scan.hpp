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

#include <stdint.h>

class IoStack;

class PlcScanLoop
{
public:
    static constexpr uint32_t kCycleUs = 1000; // 1ms fixed PLC scan cycle

    explicit PlcScanLoop(IoStack &io);

    void begin(uint32_t cycle_us = kCycleUs);
    void tick();
    uint32_t timeToNextUs() const;
    uint32_t missedCycles() const;

private:
    void runCycle_();
    static uint32_t micros_();

    IoStack &_io;
    uint32_t _cycle_us = kCycleUs;
    uint32_t _next_us = 0;
    uint32_t _missed = 0;
};
