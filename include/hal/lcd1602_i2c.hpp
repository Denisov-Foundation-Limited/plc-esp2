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
#include <Wire.h>

class Lcd1602I2c
{
public:
    static constexpr uint8_t kDefaultAddr = 0x3F;

    Lcd1602I2c() = default;
    explicit Lcd1602I2c(TwoWire &wire) : _wire(&wire) {}

    bool begin(TwoWire &wire, uint8_t addr = kDefaultAddr)
    {
        _wire = &wire;
        _addr = addr;
        if (!_wire)
            return false;

        delay(50);
        setBacklight(true);
        write4bits_(0x03);
        delayMicroseconds(4500);
        write4bits_(0x03);
        delayMicroseconds(4500);
        write4bits_(0x03);
        delayMicroseconds(150);
        write4bits_(0x02);

        command_(0x28); // 4-bit, 2-line, 5x8
        command_(0x0C); // display on, cursor off, blink off
        command_(0x06); // entry mode
        clear();
        return true;
    }

    void clear()
    {
        command_(0x01);
        delayMicroseconds(2000);
    }

    void home()
    {
        command_(0x02);
        delayMicroseconds(2000);
    }

    void setCursor(uint8_t col, uint8_t row)
    {
        static const uint8_t kRowOffsets[] = {0x00, 0x40};
        if (row > 1)
            row = 1;
        if (col > 15)
            col = 15;
        command_(0x80 | (col + kRowOffsets[row]));
    }

    void print(const String &text)
    {
        for (size_t i = 0; i < text.length(); ++i)
            writeChar_(text[i]);
    }
    void print(const __FlashStringHelper *text)
    {
        const char *p = (const char *)text;
        char c = 0;
        while ((c = pgm_read_byte(p++)))
            writeChar_(c);
    }

    void printAt(uint8_t col, uint8_t row, const String &text)
    {
        setCursor(col, row);
        print(text);
    }

    void printAt(uint8_t col, uint8_t row, const __FlashStringHelper *text)
    {
        setCursor(col, row);
        print(text);
    }

    void setBacklight(bool on)
    {
        _backlight = on;
        updateBacklight_();
    }

    void backlightOn() { setBacklight(true); }
    void backlightOff() { setBacklight(false); }
    bool backlight() const { return _backlight; }
    void toggleBacklight() { setBacklight(!_backlight); }

    void scrollDisplayLeft() { command_(0x18); }
    void scrollDisplayRight() { command_(0x1C); }

    void createChar(uint8_t location, const uint8_t charmap[8])
    {
        location &= 0x07;
        command_(0x40 | (location << 3));
        for (uint8_t i = 0; i < 8; ++i)
            writeChar_((char)charmap[i]);
    }

private:
    static constexpr uint8_t kPinRs = 0;
    static constexpr uint8_t kPinRw = 1;
    static constexpr uint8_t kPinEn = 2;
    static constexpr uint8_t kPinBl = 3;
    static constexpr uint8_t kPinD4 = 4;
    static constexpr uint8_t kPinD5 = 5;
    static constexpr uint8_t kPinD6 = 6;
    static constexpr uint8_t kPinD7 = 7;

    void command_(uint8_t value) { send_(value, false); }
    void writeChar_(char value) { send_((uint8_t)value, true); }

    void send_(uint8_t value, bool rs)
    {
        uint8_t high = (value >> 4) & 0x0F;
        uint8_t low = value & 0x0F;
        write4bits_(high, rs);
        write4bits_(low, rs);
    }

    void write4bits_(uint8_t value, bool rs = false)
    {
        uint8_t data = 0;
        if (rs)
            data |= (1 << kPinRs);
        if (_backlight)
            data |= (1 << kPinBl);
        data |= ((value & 0x01) << kPinD4);
        data |= ((value & 0x02) << (kPinD5 - 1));
        data |= ((value & 0x04) << (kPinD6 - 2));
        data |= ((value & 0x08) << (kPinD7 - 3));
        pulseEnable_(data);
    }

    void pulseEnable_(uint8_t data)
    {
        writeI2c_(data | (1 << kPinEn));
        delayMicroseconds(1);
        writeI2c_(data & ~(1 << kPinEn));
        delayMicroseconds(50);
    }

    bool writeI2c_(uint8_t data)
    {
        if (!_wire)
            return false;
        _last_port = data;
        _wire->beginTransmission(_addr);
        _wire->write(data);
        const uint8_t res = _wire->endTransmission();
        if (res != 0)
            _i2c_error = true;
        return res == 0;
    }

    void updateBacklight_()
    {
        uint8_t data = _last_port & ~(1 << kPinBl);
        if (_backlight)
            data |= (1 << kPinBl);
        writeI2c_(data);
    }

    TwoWire *_wire = nullptr;
    uint8_t _addr = kDefaultAddr;
    bool _backlight = true;
    uint8_t _last_port = 0;
    bool _i2c_error = false;
};
