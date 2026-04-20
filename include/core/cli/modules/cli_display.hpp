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

#include "core/display.hpp"
#include "core/display_slots.hpp"
#include "utils/configs_manager_iface.hpp"

template <typename ConsoleT>
class CLIDisplayT
{
public:
    explicit CLIDisplayT(ConsoleT &console) : _c(console) {}

    void printHelpEnable() const
    {
        _c._io->println(F("    show display   - list display slots"));
    }

    void printHelpConfigLines() const
    {
        _c._io->println(F("  Display:"));
        _c._io->println(F("    display                - enter Display context"));
    }

    void printHelpContextLines() const
    {
        _c._io->println(F("  Display:"));
        _c._io->println(F("    show                    - list display slots"));
        _c._io->println(F("    show <slot>             - slot details"));
        _c._io->println(F("    clear <slot>            - clear slot"));
        _c._io->println(F("    text <slot> <text>      - set text slot"));
        _c._io->println(F("    set <slot> <kind> <field> [index] [node] - set source slot"));
        _c._io->println(F("    kinds: none,time,socket,light,meteo,thermo,tank,septic,security,avr,leak,text"));
        _c._io->println(F("    fields: none,hm,min,state,temp,hum,level,armed,avr_source,avr_main_ok,avr_reserve_ok,leak_state,text"));
    }

    void printHelpTopic() const
    {
        _c._io->println(F("Display commands:"));
        _c._io->println(F("  display                - enter Display context"));
        printHelpContextLines();
    }

    void showSlots() const
    {
        if (!_c._configs_manager)
        {
            _c._io->println(F("Config manager missing"));
            return;
        }
        _c._io->println(F("Display slots:"));
        for (size_t i = 0; i < Display::kSlotCount; ++i)
        {
            DisplaySlotConfig slot{};
            _c._configs_manager->displaySlot(i, slot);
            printSlot_(i, slot);
        }
    }

    void showSlot(uint8_t slot_no) const
    {
        if (!_c._configs_manager)
        {
            _c._io->println(F("Config manager missing"));
            return;
        }
        DisplaySlotConfig slot{};
        const size_t idx = slotIndex_(slot_no);
        if (!_c._configs_manager->displaySlot(idx, slot))
        {
            _c._io->println(F("Invalid slot"));
            return;
        }
        printSlot_(idx, slot);
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showSlots();
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("show "))
        {
            String tail = cmd.substring(5);
            tail.trim();
            uint16_t slot_no = 0;
            if (!ConsoleT::parseUint_(tail, slot_no) || !validSlotNo_(slot_no))
                _c._io->println(F("Usage: show <slot>"));
            else
                showSlot((uint8_t)slot_no);
            _c.printPrompt_();
            return true;
        }
        if (!_c._configs_manager)
        {
            _c._io->println(F("Config manager missing"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("clear "))
        {
            String tail = cmd.substring(6);
            tail.trim();
            uint16_t slot_no = 0;
            if (!ConsoleT::parseUint_(tail, slot_no) || !validSlotNo_(slot_no))
                _c._io->println(F("Usage: clear <slot>"));
            else
            {
                DisplaySlotConfig slot{};
                _c._configs_manager->setDisplaySlot(slotIndex_((uint8_t)slot_no), slot);
                _c._io->println(F("OK"));
            }
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("text "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int sp = rest.indexOf(' ');
            if (sp <= 0)
            {
                _c._io->println(F("Usage: text <slot> <text>"));
                _c.printPrompt_();
                return true;
            }
            String slot_str = rest.substring(0, sp);
            String text = rest.substring(sp + 1);
            slot_str.trim();
            text.trim();
            uint16_t slot_no = 0;
            if (!ConsoleT::parseUint_(slot_str, slot_no) || !validSlotNo_(slot_no))
                _c._io->println(F("Invalid slot"));
            else
            {
                DisplaySlotConfig slot{};
                slot.kind = DisplaySlotKind::Text;
                slot.field = DisplaySlotField::Text;
                if (text.length() > 4)
                    text.remove(4);
                strncpy(slot.text, text.c_str(), sizeof(slot.text) - 1);
                slot.text[sizeof(slot.text) - 1] = '\0';
                _c._configs_manager->setDisplaySlot(slotIndex_((uint8_t)slot_no), slot);
                _c._io->println(F("OK"));
            }
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("set "))
        {
            handleSet_(cmd.substring(4));
            _c.printPrompt_();
            return true;
        }
        return false;
    }

private:
    void handleSet_(String rest)
    {
        rest.trim();
        String parts[5];
        size_t count = split_(rest, parts, 5);
        if (count < 3)
        {
            _c._io->println(F("Usage: set <slot> <kind> <field> [index] [node]"));
            return;
        }
        uint16_t slot_no = 0;
        if (!ConsoleT::parseUint_(parts[0], slot_no) || !validSlotNo_(slot_no))
        {
            _c._io->println(F("Invalid slot"));
            return;
        }

        DisplaySlotKind kind = DisplaySlotKind::None;
        DisplaySlotField field = DisplaySlotField::None;
        if (!parseKind_(parts[1], kind))
        {
            _c._io->println(F("Invalid kind"));
            return;
        }
        if (!parseField_(parts[2], field))
        {
            _c._io->println(F("Invalid field"));
            return;
        }

        DisplaySlotConfig slot{};
        slot.kind = kind;
        slot.field = adjustedField_(kind, field);

        if (count >= 4)
        {
            uint16_t idx = 0;
            if (!ConsoleT::parseUint_(parts[3], idx))
            {
                _c._io->println(F("Invalid index"));
                return;
            }
            slot.index = (uint8_t)idx;
        }
        if (count >= 5)
        {
            uint16_t node16 = 0;
            if (ConsoleT::parseUint_(parts[4], node16))
                slot.node_id = node16;
            else
                slot.node_id = (uint32_t)strtoul(parts[4].c_str(), nullptr, 10);
        }
        _c._configs_manager->setDisplaySlot(slotIndex_((uint8_t)slot_no), slot);
        _c._io->println(F("OK"));
    }

    static DisplaySlotField adjustedField_(DisplaySlotKind kind, DisplaySlotField field)
    {
        if (kind == DisplaySlotKind::Light && field == DisplaySlotField::SocketState)
            return DisplaySlotField::LightState;
        if (kind == DisplaySlotKind::Thermo && field == DisplaySlotField::SocketState)
            return DisplaySlotField::ThermoState;
        if (kind == DisplaySlotKind::Septic && field == DisplaySlotField::TankLevel)
            return DisplaySlotField::SepticLevel;
        return field;
    }

    static bool parseKind_(String input, DisplaySlotKind &out)
    {
        input.trim();
        input.toLowerCase();
        if (input == "none") out = DisplaySlotKind::None;
        else if (input == "time") out = DisplaySlotKind::Time;
        else if (input == "socket") out = DisplaySlotKind::Socket;
        else if (input == "light") out = DisplaySlotKind::Light;
        else if (input == "meteo") out = DisplaySlotKind::Meteo;
        else if (input == "thermo") out = DisplaySlotKind::Thermo;
        else if (input == "tank") out = DisplaySlotKind::Tank;
        else if (input == "septic") out = DisplaySlotKind::Septic;
        else if (input == "security") out = DisplaySlotKind::Security;
        else if (input == "avr") out = DisplaySlotKind::Avr;
        else if (input == "leak") out = DisplaySlotKind::Leak;
        else if (input == "text") out = DisplaySlotKind::Text;
        else return false;
        return true;
    }

    static bool parseField_(String input, DisplaySlotField &out)
    {
        input.trim();
        input.toLowerCase();
        if (input == "none") out = DisplaySlotField::None;
        else if (input == "hm") out = DisplaySlotField::TimeHm;
        else if (input == "min") out = DisplaySlotField::TimeMin;
        else if (input == "state") out = DisplaySlotField::SocketState;
        else if (input == "temp") out = DisplaySlotField::MeteoTemp;
        else if (input == "hum") out = DisplaySlotField::MeteoHum;
        else if (input == "level") out = DisplaySlotField::TankLevel;
        else if (input == "armed") out = DisplaySlotField::SecurityArmed;
        else if (input == "avr_source") out = DisplaySlotField::AvrSource;
        else if (input == "avr_main_ok") out = DisplaySlotField::AvrMainOk;
        else if (input == "avr_reserve_ok") out = DisplaySlotField::AvrReserveOk;
        else if (input == "leak_state") out = DisplaySlotField::LeakState;
        else if (input == "text") out = DisplaySlotField::Text;
        else return false;
        return true;
    }

    static size_t split_(const String &input, String *out, size_t max_parts)
    {
        size_t count = 0;
        String cur;
        for (size_t i = 0; i < input.length(); ++i)
        {
            const char ch = input[i];
            if (ch == ' ')
            {
                if (cur.length() && count < max_parts)
                {
                    out[count++] = cur;
                    cur = "";
                }
            }
            else
            {
                cur += ch;
            }
        }
        if (cur.length() && count < max_parts)
            out[count++] = cur;
        return count;
    }

    static bool validSlotNo_(uint16_t slot_no)
    {
        return slot_no >= 1 && slot_no <= Display::kSlotCount;
    }

    static size_t slotIndex_(uint8_t slot_no)
    {
        return (size_t)(slot_no - 1);
    }

    void printSlot_(size_t idx, const DisplaySlotConfig &slot) const
    {
        _c._io->println(String(F("Slot ")) + String((unsigned)(idx + 1)) + F(":"));
        _c._io->println(String(F("  kind: ")) + kindName_(slot.kind));
        _c._io->println(String(F("  field: ")) + fieldName_(slot.field));
        _c._io->println(String(F("  index: ")) + String((unsigned)slot.index));
        _c._io->println(String(F("  node: ")) + String((unsigned long)slot.node_id));
        _c._io->println(String(F("  text: ")) + String(slot.text));
    }

    static const char *kindName_(DisplaySlotKind kind)
    {
        switch (kind)
        {
        case DisplaySlotKind::Time: return "time";
        case DisplaySlotKind::Socket: return "socket";
        case DisplaySlotKind::Light: return "light";
        case DisplaySlotKind::Meteo: return "meteo";
        case DisplaySlotKind::Thermo: return "thermo";
        case DisplaySlotKind::Tank: return "tank";
        case DisplaySlotKind::Septic: return "septic";
        case DisplaySlotKind::Security: return "security";
        case DisplaySlotKind::Avr: return "avr";
        case DisplaySlotKind::Leak: return "leak";
        case DisplaySlotKind::Text: return "text";
        case DisplaySlotKind::None:
        default: return "none";
        }
    }

    static const char *fieldName_(DisplaySlotField field)
    {
        switch (field)
        {
        case DisplaySlotField::TimeHm: return "hm";
        case DisplaySlotField::TimeMin: return "min";
        case DisplaySlotField::SocketState: return "state";
        case DisplaySlotField::LightState: return "state";
        case DisplaySlotField::MeteoTemp: return "temp";
        case DisplaySlotField::MeteoHum: return "hum";
        case DisplaySlotField::ThermoState: return "state";
        case DisplaySlotField::TankLevel: return "level";
        case DisplaySlotField::SepticLevel: return "level";
        case DisplaySlotField::SecurityArmed: return "armed";
        case DisplaySlotField::AvrSource: return "avr_source";
        case DisplaySlotField::AvrMainOk: return "avr_main_ok";
        case DisplaySlotField::AvrReserveOk: return "avr_reserve_ok";
        case DisplaySlotField::LeakState: return "leak_state";
        case DisplaySlotField::Text: return "text";
        case DisplaySlotField::None:
        default: return "none";
        }
    }

    ConsoleT &_c;
};
