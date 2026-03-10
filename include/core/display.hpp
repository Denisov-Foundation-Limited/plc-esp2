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

#include "core/display_slots.hpp"

class I2CManager;
class Lcd1602I2c;

class Display
{
public:
    static constexpr size_t kSlotCount = 8;
    static constexpr char kDegreeChar = 1;

    using SlotProvider = bool (*)(void *ctx, const DisplaySlotConfig &slot, char out[5]);

    enum class Error : uint8_t
    {
        Ok = 0,
        NoBus,
        InvalidConfig,
        I2c
    };

    Display(I2CManager &i2c, Lcd1602I2c &lcd);

    bool begin();

    bool showStr(uint8_t str, const String &text);

    void clear();

    void task();

    bool setLine(uint8_t line, const String &text);

    bool setText(const String &line0, const String &line1);

    void setSlotProvider(SlotProvider cb, void *ctx);

    bool setSlot(size_t idx, const DisplaySlotConfig &slot);

    const DisplaySlotConfig *slot(size_t idx) const;

    Error lastError() const;

private:
    static constexpr bool busExists_(uint8_t bus_num);

    I2CManager &_i2c;
    Lcd1602I2c &_lcd;
    Error _err = Error::Ok;
    bool _ready = false;
    uint8_t _bus_num = 0;
    String _line0;
    String _line1;
    DisplaySlotConfig _slots[kSlotCount]{};
    SlotProvider _slot_provider = nullptr;
    void *_slot_ctx = nullptr;
    static constexpr uint8_t kDegreeCharMap_[8] = {
        0b00111,
        0b00101,
        0b00111,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
    };

    void writeLine_(uint8_t line, const String &text);
    void writeLine_(uint8_t line, const char *text);
    void renderSlots_(char out0[17], char out1[17]);
};
