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

class TwoWire;

class Lcd1602I2c
{
public:
    static constexpr uint8_t kDefaultAddr = 0x3F;
    static constexpr uint8_t kCols = 16;
    static constexpr uint8_t kRows = 2;

    Lcd1602I2c() = default;
    explicit Lcd1602I2c(TwoWire &wire);

    bool begin(TwoWire &wire, uint8_t addr = kDefaultAddr);
    bool resync();
    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);
    void print(const String &text);
    void print(const __FlashStringHelper *text);
    void printAt(uint8_t col, uint8_t row, const String &text);
    void printAt(uint8_t col, uint8_t row, const __FlashStringHelper *text);
    void setBacklight(bool on);
    void backlightOn();
    void backlightOff();
    bool backlight() const;
    void toggleBacklight();
    void scrollDisplayLeft();
    void scrollDisplayRight();
    void createChar(uint8_t location, const uint8_t charmap[8]);

private:
    static constexpr uint8_t kPinRs = 0;
    static constexpr uint8_t kPinRw = 1;
    static constexpr uint8_t kPinEn = 2;
    static constexpr uint8_t kPinBl = 3;
    static constexpr uint8_t kPinD4 = 4;
    static constexpr uint8_t kPinD5 = 5;
    static constexpr uint8_t kPinD6 = 6;
    static constexpr uint8_t kPinD7 = 7;

    void ensureReady_();
    void command_(uint8_t value);
    void writeChar_(char value);
    void send_(uint8_t value, bool rs);
    void write4bits_(uint8_t value, bool rs = false);
    void pulseEnable_(uint8_t data);
    bool writeI2c_(uint8_t data);
    void init4bit_();
    void updateBacklight_();
    void mirrorClear_();
    void mirrorWriteChar_(char value);
    bool redrawShadow_();

    TwoWire *_wire = nullptr;
    uint8_t _addr = kDefaultAddr;
    bool _backlight = true;
    uint8_t _last_port = 0;
    bool _i2c_error = false;
    uint8_t _cursor_col = 0;
    uint8_t _cursor_row = 0;
    bool _cursor_valid = false;
    char _shadow[kRows][kCols] = {};
};
