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

#include "core/display.hpp"

#include "boards/board_profile.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/lcd1602_i2c.hpp"

Display::Display(I2CManager &i2c, Lcd1602I2c &lcd) : _i2c(i2c), _lcd(lcd) {}

bool Display::begin()
{
    const uint8_t bus_num = ActiveBoardProfile::LCD.bus_num;
    _bus_num = bus_num;
    const uint8_t addr = ActiveBoardProfile::LCD.addr;
    if (!busExists_(bus_num))
    {
        _err = Error::InvalidConfig;
        return false;
    }

    TwoWire *wire = _i2c.wirePtr(bus_num);
    if (!wire)
    {
        _err = Error::NoBus;
        return false;
    }

    {
        I2CManager::ScopedBusLock lk(_i2c, _bus_num);
        if (!lk.locked())
        {
            _err = Error::I2c;
            return false;
        }
        if (!_i2c.probeAddressLocked(_bus_num, addr))
        {
            _err = Error::I2c;
            return false;
        }
        if (!_lcd.begin(*wire, addr))
        {
            _err = Error::I2c;
            return false;
        }
    }
    _err = Error::Ok;
    _ready = true;
    memset(_last_hw_line0, 0, sizeof(_last_hw_line0));
    memset(_last_hw_line1, 0, sizeof(_last_hw_line1));
    _custom_chars_ready = false;
    _line0 = F("      FCPLC     ");
    _line1 = F("Denisov Fnd Ltd.");
    return true;
}

bool Display::showStr(uint8_t str, const String &text)
{
    if (str > 1 || text.length() > 16)
        return false;
    I2CManager::ScopedBusLock lk(_i2c, _bus_num);
    if (!lk.locked())
        return false;
    _lcd.setCursor(0, str);
    _lcd.print(text);
    return true;
}

void Display::clear()
{
    I2CManager::ScopedBusLock lk(_i2c, _bus_num);
    if (!lk.locked())
        return;
    _lcd.clear();
    memset(_last_hw_line0, 0, sizeof(_last_hw_line0));
    memset(_last_hw_line1, 0, sizeof(_last_hw_line1));
}

void Display::task()
{
    if (!_ready)
        return;
    char line0[17] = {};
    char line1[17] = {};
    bool have_rendered_slots = false;

    if (_slot_provider)
    {
        renderSlots_(line0, line1);
        have_rendered_slots = true;
    }

    const char *target0 = have_rendered_slots ? line0 : _line0.c_str();
    const char *target1 = have_rendered_slots ? line1 : _line1.c_str();
    const bool line0_changed = strncmp(_last_hw_line0, target0, 16) != 0;
    const bool line1_changed = strncmp(_last_hw_line1, target1, 16) != 0;
    if (_custom_chars_ready && !line0_changed && !line1_changed)
        return;

    I2CManager::ScopedBusLock lk(_i2c, _bus_num);
    if (!lk.locked())
        return;

    if (!_custom_chars_ready)
    {
        _lcd.createChar(kDegreeChar, kDegreeCharMap_);
        _custom_chars_ready = true;
    }
    if (line0_changed)
    {
        writeLine_(0, target0);
        strncpy(_last_hw_line0, target0, 16);
        _last_hw_line0[16] = '\0';
    }
    if (line1_changed)
    {
        writeLine_(1, target1);
        strncpy(_last_hw_line1, target1, 16);
        _last_hw_line1[16] = '\0';
    }
}

bool Display::setLine(uint8_t line, const String &text)
{
    if (line > 1 || text.length() > 16)
        return false;
    if (line == 0)
        _line0 = text;
    else
        _line1 = text;
    return true;
}

bool Display::setText(const String &line0, const String &line1)
{
    if (line0.length() > 16 || line1.length() > 16)
        return false;
    _line0 = line0;
    _line1 = line1;
    return true;
}

void Display::setSlotProvider(SlotProvider cb, void *ctx)
{
    _slot_provider = cb;
    _slot_ctx = ctx;
}

bool Display::setSlot(size_t idx, const DisplaySlotConfig &slot)
{
    if (idx >= kSlotCount)
        return false;
    _slots[idx] = slot;
    return true;
}

const DisplaySlotConfig *Display::slot(size_t idx) const
{
    if (idx >= kSlotCount)
        return nullptr;
    return &_slots[idx];
}

Display::Error Display::lastError() const
{
    return _err;
}

void Display::writeLine_(uint8_t line, const String &text)
{
    if (line > 1)
        return;
    char buf[17] = {};
    const size_t n = text.length() > 16 ? 16 : text.length();
    for (size_t i = 0; i < 16; ++i)
        buf[i] = (i < n) ? text[i] : ' ';
    buf[16] = '\0';
    _lcd.setCursor(0, line);
    _lcd.print(buf);
}

void Display::writeLine_(uint8_t line, const char *text)
{
    if (!text)
        return;
    writeLine_(line, String(text));
}

void Display::renderSlots_(char out0[17], char out1[17])
{
    for (size_t i = 0; i < 16; ++i)
    {
        out0[i] = ' ';
        out1[i] = ' ';
    }
    out0[16] = '\0';
    out1[16] = '\0';
    for (size_t i = 0; i < kSlotCount; ++i)
    {
        char buf[5] = {};
        if (_slot_provider)
        {
            if (!_slot_provider(_slot_ctx, _slots[i], buf))
            {
                for (size_t k = 0; k < 4; ++k)
                    buf[k] = ' ';
                buf[4] = '\0';
            }
        }
        const uint8_t row = (i < 4) ? 0 : 1;
        uint8_t col = (uint8_t)((i % 4) * 4);
        const bool has_prev = (i % 4) != 0;
        const bool has_next = (i % 4) != 3 && (i + 1) < kSlotCount;
        const bool is_time = _slots[i].kind == DisplaySlotKind::Time;
        const bool is_time_hm = is_time && _slots[i].field == DisplaySlotField::TimeHm;
        const bool is_time_min = is_time && _slots[i].field == DisplaySlotField::TimeMin;
        const bool prev_is_time_hm = has_prev &&
                                     _slots[i - 1].kind == DisplaySlotKind::Time &&
                                     _slots[i - 1].field == DisplaySlotField::TimeHm;
        const bool next_is_time_min = has_next &&
                                      _slots[i + 1].kind == DisplaySlotKind::Time &&
                                      _slots[i + 1].field == DisplaySlotField::TimeMin;
        const bool join_prev = is_time_min && prev_is_time_hm;
        const bool join_next = is_time_hm && next_is_time_min;
        if (join_prev)
        {
            col = (uint8_t)(col - 1);
        }
        for (uint8_t k = 0; k < 4; ++k)
        {
            const bool force_space = (k == 3) && !(join_prev || join_next);
            const char c = force_space ? ' ' : (buf[k] ? buf[k] : ' ');
            if (row == 0)
                out0[col + k] = c;
            else
                out1[col + k] = c;
        }
    }
}

constexpr bool Display::busExists_(uint8_t bus_num)
{
    for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
    {
        if (ActiveBoardProfile::I2CS[i].bus_num == bus_num)
            return true;
    }
    return false;
}
