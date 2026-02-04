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

enum class DisplaySlotKind : uint8_t
{
    None = 0,
    Time,
    Socket,
    Light,
    Meteo,
    Tank,
    Septic,
    Security,
    Text
};

enum class DisplaySlotField : uint8_t
{
    None = 0,
    TimeHm,
    SocketState,
    LightState,
    MeteoTemp,
    MeteoHum,
    TankLevel,
    SepticLevel,
    SecurityArmed,
    Text
};

struct DisplaySlotConfig
{
    DisplaySlotKind kind = DisplaySlotKind::None;
    uint8_t index = 0;
    DisplaySlotField field = DisplaySlotField::None;
    char text[5] = {};
};
