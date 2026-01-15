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

enum class StackFeature : uint8_t
{
    System = 0x01,
    Ports = 0x02,
    TempSensors = 0x03,
    I2cScan = 0x04,
    OwScan = 0x05,
    Fan = 0x06,
    Rtc = 0x07,
    PlcStatus = 0x08,
    Relays = 0x09,
    DigitalInputs = 0x0A,
    Telegram = 0x0B,
    Storage = 0x0C
};
