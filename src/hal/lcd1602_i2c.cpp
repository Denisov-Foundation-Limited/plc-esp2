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

#include "hal/lcd1602_i2c.hpp"
#include <Wire.h>

Lcd1602I2c::Lcd1602I2c(TwoWire &wire)
    : _wire(&wire)
{
}

bool Lcd1602I2c::begin(TwoWire &wire, uint8_t addr)
{
    _wire = &wire;
    _addr = addr;
    if (!_wire)
        return false;

    delay(50);
    setBacklight(true);
    mirrorClear_();
    init4bit_();
    clear();
    return true;
}

bool Lcd1602I2c::resync()
{
    if (!_wire)
        return false;
    _i2c_error = false;
    delay(2);
    init4bit_();
    if (!redrawShadow_())
        return false;
    if (_cursor_valid)
        setCursor(_cursor_col, _cursor_row);
    return !_i2c_error;
}

void Lcd1602I2c::clear()
{
    ensureReady_();
    command_(0x01);
    command_(0x01);
    delayMicroseconds(2000);
    _cursor_col = 0;
    _cursor_row = 0;
    _cursor_valid = true;
    mirrorClear_();
}

void Lcd1602I2c::home()
{
    ensureReady_();
    command_(0x02);
    command_(0x02);
    delayMicroseconds(2000);
    _cursor_col = 0;
    _cursor_row = 0;
    _cursor_valid = true;
}

void Lcd1602I2c::setCursor(uint8_t col, uint8_t row)
{
    static const uint8_t kRowOffsets[] = {0x00, 0x40};
    if (row > 1)
        row = 1;
    if (col > 15)
        col = 15;
    ensureReady_();
    command_(0x80 | (col + kRowOffsets[row]));
    command_(0x80 | (col + kRowOffsets[row]));
    _cursor_col = col;
    _cursor_row = row;
    _cursor_valid = true;
}

void Lcd1602I2c::print(const String &text)
{
    for (size_t i = 0; i < text.length(); ++i)
        writeChar_(text[i]);
}

void Lcd1602I2c::print(const __FlashStringHelper *text)
{
    const char *p = (const char *)text;
    char c = 0;
    while ((c = pgm_read_byte(p++)))
        writeChar_(c);
}

void Lcd1602I2c::printAt(uint8_t col, uint8_t row, const String &text)
{
    setCursor(col, row);
    print(text);
}

void Lcd1602I2c::printAt(uint8_t col, uint8_t row, const __FlashStringHelper *text)
{
    setCursor(col, row);
    print(text);
}

void Lcd1602I2c::setBacklight(bool on)
{
    _backlight = on;
    updateBacklight_();
}

void Lcd1602I2c::backlightOn()
{
    setBacklight(true);
}

void Lcd1602I2c::backlightOff()
{
    setBacklight(false);
}

bool Lcd1602I2c::backlight() const
{
    return _backlight;
}

void Lcd1602I2c::toggleBacklight()
{
    setBacklight(!_backlight);
}

void Lcd1602I2c::scrollDisplayLeft()
{
    command_(0x18);
}

void Lcd1602I2c::scrollDisplayRight()
{
    command_(0x1C);
}

void Lcd1602I2c::createChar(uint8_t location, const uint8_t charmap[8])
{
    ensureReady_();
    const uint8_t saved_col = _cursor_col;
    const uint8_t saved_row = _cursor_row;
    const bool saved_valid = _cursor_valid;
    location &= 0x07;
    command_(0x40 | (location << 3));
    for (uint8_t i = 0; i < 8; ++i)
        send_((uint8_t)charmap[i], true);
    _cursor_col = saved_col;
    _cursor_row = saved_row;
    _cursor_valid = saved_valid;
    if (_cursor_valid)
        setCursor(_cursor_col, _cursor_row);
}

void Lcd1602I2c::ensureReady_()
{
    if (_i2c_error)
        resync();
}

void Lcd1602I2c::command_(uint8_t value)
{
    send_(value, false);
}

void Lcd1602I2c::writeChar_(char value)
{
    send_((uint8_t)value, true);
    if (_cursor_valid)
        mirrorWriteChar_(value);
}

void Lcd1602I2c::send_(uint8_t value, bool rs)
{
    const uint8_t high = (value >> 4) & 0x0F;
    const uint8_t low = value & 0x0F;
    write4bits_(high, rs);
    write4bits_(low, rs);
}

void Lcd1602I2c::write4bits_(uint8_t value, bool rs)
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

void Lcd1602I2c::pulseEnable_(uint8_t data)
{
    writeI2c_(data | (1 << kPinEn));
    delayMicroseconds(1);
    writeI2c_(data & ~(1 << kPinEn));
    delayMicroseconds(50);
}

bool Lcd1602I2c::writeI2c_(uint8_t data)
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

void Lcd1602I2c::init4bit_()
{
    write4bits_(0x03);
    delayMicroseconds(4500);
    write4bits_(0x03);
    delayMicroseconds(4500);
    write4bits_(0x03);
    delayMicroseconds(150);
    write4bits_(0x02);

    command_(0x28);
    command_(0x0C);
    command_(0x06);
}

void Lcd1602I2c::updateBacklight_()
{
    uint8_t data = _last_port & ~(1 << kPinBl);
    if (_backlight)
        data |= (1 << kPinBl);
    writeI2c_(data);
}

void Lcd1602I2c::mirrorClear_()
{
    for (uint8_t row = 0; row < kRows; ++row)
    {
        for (uint8_t col = 0; col < kCols; ++col)
            _shadow[row][col] = ' ';
    }
}

void Lcd1602I2c::mirrorWriteChar_(char value)
{
    if (_cursor_row >= kRows || _cursor_col >= kCols)
        return;
    _shadow[_cursor_row][_cursor_col] = value;
    ++_cursor_col;
    if (_cursor_col >= kCols)
    {
        _cursor_col = 0;
        _cursor_row = (uint8_t)((_cursor_row + 1) % kRows);
    }
}

bool Lcd1602I2c::redrawShadow_()
{
    static const uint8_t kRowOffsets[] = {0x00, 0x40};
    command_(0x0C);
    command_(0x06);
    for (uint8_t row = 0; row < kRows; ++row)
    {
        command_(0x80 | kRowOffsets[row]);
        for (uint8_t col = 0; col < kCols; ++col)
            send_((uint8_t)_shadow[row][col], true);
    }
    return !_i2c_error;
}
