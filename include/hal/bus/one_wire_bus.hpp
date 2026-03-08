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

#if defined(ARDUINO_ARCH_ESP32)
#include <driver/rtc_io.h>
#include <soc/gpio_struct.h>
#include <esp_idf_version.h>
#endif

class OneWireBus
{
public:
    OneWireBus() = default;

    void begin(uint8_t pin);
    uint8_t reset();
    void write(uint8_t v);
    uint8_t read();
    void select(const uint8_t rom[8]);
    void skip();
    void reset_search();
    uint8_t search(uint8_t *new_addr);
    static uint8_t crc8(const uint8_t *addr, uint8_t len);

private:
    static constexpr uint16_t kResetLowUs = 480;
    static constexpr uint16_t kResetReleaseUs = 70;
    static constexpr uint16_t kResetRecoverUs = 410;
    static constexpr uint8_t kWrite1LowUs = 10;
    static constexpr uint8_t kWrite1SlotUs = 55;
    static constexpr uint8_t kWrite0LowUs = 65;
    static constexpr uint8_t kWrite0SlotUs = 5;
    static constexpr uint8_t kReadLowUs = 3;
    static constexpr uint8_t kReadSampleUs = 10;
    static constexpr uint8_t kReadSlotUs = 53;

    void driveLow_();
    void release_();
    void driveHigh_();
    uint8_t readPin_() const;

#if defined(ARDUINO_ARCH_ESP32)
    static uint8_t directRead_(uint8_t pin);
    static void directWriteLow_(uint8_t pin);
    static void directWriteHigh_(uint8_t pin);
    static void directModeInput_(uint8_t pin);
    static void directModeOutput_(uint8_t pin);
#endif

    static void critEnter_();
    static void critExit_();
    void writeBit_(uint8_t v);
    uint8_t readBit_();

    uint8_t _pin = 0;
    bool _inited = false;
    uint8_t _rom_no[8] = {};
    uint8_t _last_discrepancy = 0;
    uint8_t _last_family_discrepancy = 0;
    bool _last_device_flag = false;

#if defined(ARDUINO_ARCH_ESP32)
    static portMUX_TYPE mux_;
#endif
};
