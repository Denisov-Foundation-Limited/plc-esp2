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
    using BusLockCallback = bool (*)(void *ctx, uint32_t timeout_ms);
    using BusUnlockCallback = void (*)(void *ctx);

    IButton() = default;
    explicit IButton(OneWireBus &bus);

    bool begin(OneWireBus &bus);
    void setBusLockCallbacks(BusLockCallback lock_cb, BusUnlockCallback unlock_cb, void *ctx);
    bool readSerial(uint8_t out[8]);
    bool readSerial(uint64_t &out);

    static void toHex(const uint8_t in[8], char out[17]);
    static String toString(const uint8_t in[8]);

    bool readSerialHex(char out[17]);
    String readSerialString();

private:
    OneWireBus *bus_();
    bool lockBus_(uint32_t timeout_ms = 200);
    void unlockBus_();

    OneWireBus *_bus = nullptr;
    BusLockCallback _bus_lock_cb = nullptr;
    BusUnlockCallback _bus_unlock_cb = nullptr;
    void *_bus_lock_ctx = nullptr;
};
