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

#include "boards/board_profile.hpp"
#include "core/display_slots.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/lcd1602_i2c.hpp"

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

    Display(I2CManager &i2c, Lcd1602I2c &lcd) : _i2c(i2c), _lcd(lcd) {}

    bool begin()
    {
        const uint8_t bus_num = ActiveBoardProfile::LCD.bus_num;
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

        if (!_lcd.begin(*wire, addr))
        {
            _err = Error::I2c;
            return false;
        }

        _lcd.createChar(kDegreeChar, kDegreeCharMap_);
        _err = Error::Ok;
        _ready = true;

        _lcd.setBacklight(true);

        clear();
        showStr(0, F("      FCPLC     "));
        showStr(1, F("Denisov Fnd Ltd."));
        return true;
    }

    bool showStr(uint8_t str, const String &text) {
        if (str > 0 || str < 2) {
            if (text.length() > 16) {
                return false;
            }
            _lcd.setCursor(0, str);
            _lcd.print(text);
        }
        return false;
    }

    void clear() {
        _lcd.clear();
    }

    void task()
    {
        if (!_ready)
            return;
        if (_slot_provider)
        {
            char line0[17] = {};
            char line1[17] = {};
            renderSlots_(line0, line1);
            writeLine_(0, line0);
            writeLine_(1, line1);
            return;
        }
        writeLine_(0, _line0);
        writeLine_(1, _line1);
    }

    bool setLine(uint8_t line, const String &text)
    {
        if (line > 1 || text.length() > 16)
            return false;
        if (line == 0)
            _line0 = text;
        else
            _line1 = text;
        return true;
    }

    bool setText(const String &line0, const String &line1)
    {
        if (line0.length() > 16 || line1.length() > 16)
            return false;
        _line0 = line0;
        _line1 = line1;
        return true;
    }

    void setSlotProvider(SlotProvider cb, void *ctx)
    {
        _slot_provider = cb;
        _slot_ctx = ctx;
    }

    bool setSlot(size_t idx, const DisplaySlotConfig &slot)
    {
        if (idx >= kSlotCount)
            return false;
        _slots[idx] = slot;
        return true;
    }

    const DisplaySlotConfig *slot(size_t idx) const
    {
        if (idx >= kSlotCount)
            return nullptr;
        return &_slots[idx];
    }

    Error lastError() const { return _err; }

private:
    static constexpr bool busExists_(uint8_t bus_num)
    {
        for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
        {
            if (ActiveBoardProfile::I2CS[i].bus_num == bus_num)
                return true;
        }
        return false;
    }

    I2CManager &_i2c;
    Lcd1602I2c &_lcd;
    Error _err = Error::Ok;
    bool _ready = false;
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

    void writeLine_(uint8_t line, const String &text)
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

    void writeLine_(uint8_t line, const char *text)
    {
        if (!text)
            return;
        writeLine_(line, String(text));
    }

    void renderSlots_(char out0[17], char out1[17])
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
};
