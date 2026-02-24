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
#include "hal/bus/one_wire_bus.hpp"
#include <stdint.h>

class IButton
{
public:
    IButton() = default;
    explicit IButton(OneWireBus &bus);

    bool begin(OneWireBus &bus);
    bool readSerial(uint8_t out[8]);
    bool readSerial(uint64_t &out);

    static void toHex(const uint8_t in[8], char out[17]);
    static String toString(const uint8_t in[8]);

    bool readSerialHex(char out[17]);
    String readSerialString();

private:
    OneWireBus *bus_();

    OneWireBus *_bus = nullptr;
};
