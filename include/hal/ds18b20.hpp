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
#include <vector>

class Ds18b20
{
public:
    using BusLockCallback = bool (*)(void *ctx, uint32_t timeout_ms);
    using BusUnlockCallback = void (*)(void *ctx);

    Ds18b20() = default;
    explicit Ds18b20(OneWireBus &bus);

    bool begin(OneWireBus &bus);
    void setBusLockCallbacks(BusLockCallback lock_cb, BusUnlockCallback unlock_cb, void *ctx);
    bool readTempC(float &out_c);
    bool readTempC(uint8_t addr[8], float &out_c);
    bool readTempC(const String &hex_serial, float &out_c);
    bool readTempC(const char *hex_serial, float &out_c);
    bool startConversion();
    bool ready() const;
    bool readTempCNoWait(float &out_c);
    bool readTempCNoWait(uint8_t addr[8], float &out_c);
    bool readTempCNoWait(const String &hex_serial, float &out_c);
    bool readTempCNoWait(const char *hex_serial, float &out_c);
    void listSerials(std::vector<String> &out);
    void listSerials(char out[][17], size_t max, size_t &count);

private:
    static constexpr uint8_t kFamily = 0x28;
    static constexpr uint16_t kDefaultConvMs = 1000;
    static constexpr uint8_t kReadAttempts = 2;
    static constexpr uint16_t kRetryDelayMs = 25;

    OneWireBus *bus_();
    bool findFirst_();
    bool readTempByAddr_(const uint8_t addr[8], float &out_c);
    bool readTempByAddrNoWait_(const uint8_t addr[8], float &out_c);
    bool readScratchpadTemp_(const uint8_t addr[8], float &out_c);
    static bool parseHexSerial_(const String &hex, uint8_t out[8]);
    static bool parseHexSerial_(const char *hex, uint8_t out[8]);
    static String toString_(const uint8_t in[8]);
    static void toHex_(const uint8_t in[8], char out[17]);
    static int hexNibble_(char c);
    bool lockBus_(uint32_t timeout_ms = 1000);
    void unlockBus_();

    OneWireBus *_bus = nullptr;
    uint8_t _addr[8] = {};
    bool _has_addr = false;
    uint32_t _last_conv_ms = 0;
    uint16_t _conv_time_ms = kDefaultConvMs;
    bool _has_conv = false;
    BusLockCallback _bus_lock_cb = nullptr;
    BusUnlockCallback _bus_unlock_cb = nullptr;
    void *_bus_lock_ctx = nullptr;
};
