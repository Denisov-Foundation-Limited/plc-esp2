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
#include <string.h>

#include "controllers/watering_controller.hpp"

template <typename ConsoleT>
class CLIWateringT
{
public:
    CLIWateringT(ConsoleT &console, WateringController &watering)
        : _c(console), _watering(watering) {}

    void printIdRangeInline() const
    {
        printIdRangeInline_();
    }

    void printHelpEnable()
    {
        _c._io->println(F("    show watering  - list watering rules"));
        _c._io->print(F("    show watering <id>"));
        printIdRangeInline_();
        _c._io->println(F(" - rule details"));
    }

    void printHelpConfigLines()
    {
        _c._io->println(F("  Watering:"));
        _c._io->println(F("    watering               - enter Watering context"));
    }

    void printHelpContextLines()
    {
        _c._io->println(F("  Watering:"));
        _c._io->println(F("    show                     - list rules"));
        _c._io->print(F("    show <id>"));
        printIdRangeInline_();
        _c._io->println(F("              - rule details"));
        _c._io->print(F("    name <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <text>          - set rule name"));
        _c._io->print(F("    enable <id>"));
        printIdRangeInline_();
        _c._io->println(F("              - enable rule"));
        _c._io->print(F("    disable <id>"));
        printIdRangeInline_();
        _c._io->println(F("             - disable rule"));
        _c._io->print(F("    status <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <on|off>       - monitor time"));
        _c._io->print(F("    port <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <port|none>   - set GPIO port"));
        _c._io->print(F("    tank <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <tank_id|none> - bind tank"));
        _c._io->print(F("    days <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <mon,tue,...|all|none> - set weekdays"));
        _c._io->print(F("    time <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <HH:MM>       - set start time"));
        _c._io->print(F("    duration <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <min>        - set duration (minutes)"));
        _c._io->print(F("    resume <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <on|off>      - resume after refill"));
        _c._io->print(F("    resume_level <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <low|mid|full> - resume at >= level"));
    }

    void showRules()
    {
        bool any = false;
        printHeader_();
        for (size_t i = 0; i < WateringController::kRuleCount; ++i)
        {
            const auto *cfg = _watering.configByIndex(i);
            const auto *st = _watering.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            printRow_(*cfg, *st);
        }
        if (!any)
            _c._io->println(F("  none"));
    }

    void showRule(size_t id)
    {
        const auto *cfg = _watering.config(id);
        const auto *st = _watering.state(id);
        if (!cfg || !st)
        {
            printInvalidId_();
            return;
        }
        _c._io->println(F("Watering rule:"));
        printHeader_();
        printRow_(*cfg, *st);
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showRules();
            return true;
        }
        if (lower.startsWith("show "))
        {
            String tail = cmd.substring(5);
            tail.trim();
            uint16_t id = 0;
            if (!parseId_(tail, id))
            {
                printInvalidId_();
                return true;
            }
            showRule(id);
            return true;
        }
        if (lower.startsWith("name "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->print(F("Usage: name <id> <text>"));
                return true;
            }
            String id_str = rest.substring(0, space);
            String text = rest.substring(space + 1);
            id_str.trim();
            text.trim();
            uint16_t id = 0;
            if (!parseId_(id_str, id))
            {
                printInvalidId_();
                return true;
            }
            if (!_watering.setName(id, text))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            return true;
        }
        if (handleSimpleEnable_(cmd, lower, true))
            return true;
        if (handleSimpleEnable_(cmd, lower, false))
            return true;
        if (handleStatus_(cmd, lower))
            return true;
        if (handlePort_(cmd, lower))
            return true;
        if (handleTank_(cmd, lower))
            return true;
        if (handleDays_(cmd, lower))
            return true;
        if (handleTime_(cmd, lower))
            return true;
        if (handleDuration_(cmd, lower))
            return true;
        if (handleResume_(cmd, lower))
            return true;
        if (handleResumeLevel_(cmd, lower))
            return true;
        return false;
    }

private:
    ConsoleT &_c;
    WateringController &_watering;

    static bool parseId_(const String &s, uint16_t &out)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
            if (s[i] < '0' || s[i] > '9')
                return false;
        const int v = s.toInt();
        if (v < 1 || v > (int)WateringController::kRuleCount)
            return false;
        out = (uint16_t)v;
        return true;
    }

    static bool parsePort_(const String &s, uint8_t &out)
    {
        String t = s;
        t.trim();
        t.toLowerCase();
        if (t == "none")
        {
            out = WateringController::kInvalidPort;
            return true;
        }
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const int v = t.toInt();
        if (v < 0 || v > 255)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseBool_(const String &s, bool &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "1" || t == "on" || t == "true" || t == "yes")
        {
            out = true;
            return true;
        }
        if (t == "0" || t == "off" || t == "false" || t == "no")
        {
            out = false;
            return true;
        }
        return false;
    }

    static bool parseDays_(const String &s, uint8_t &mask)
    {
        String t = s;
        t.trim();
        t.toLowerCase();
        if (t.length() == 0)
            return false;
        if (t == "all")
        {
            mask = 0x7F;
            return true;
        }
        if (t == "none")
        {
            mask = 0;
            return true;
        }
        t.replace(',', ' ');
        t.replace(';', ' ');
        uint8_t out = 0;
        int pos = 0;
        while (pos < (int)t.length())
        {
            while (pos < (int)t.length() && t[pos] == ' ')
                ++pos;
            if (pos >= (int)t.length())
                break;
            int end = t.indexOf(' ', pos);
            if (end < 0)
                end = t.length();
            String tok = t.substring(pos, end);
            tok.trim();
            uint8_t day = 0;
            if (tok.length() == 0)
            {
                pos = end + 1;
                continue;
            }
            bool is_num = true;
            for (size_t i = 0; i < tok.length(); ++i)
            {
                if (tok[i] < '0' || tok[i] > '9')
                {
                    is_num = false;
                    break;
                }
            }
            if (is_num)
            {
                const int v = tok.toInt();
                if (v < 1 || v > 7)
                    return false;
                day = (uint8_t)v;
            }
            else if (tok == "mon" || tok == "monday" || tok == "mo" || tok == "пн")
                day = 2;
            else if (tok == "tue" || tok == "tuesday" || tok == "tu" || tok == "вт")
                day = 3;
            else if (tok == "wed" || tok == "wednesday" || tok == "we" || tok == "ср")
                day = 4;
            else if (tok == "thu" || tok == "thursday" || tok == "th" || tok == "чт")
                day = 5;
            else if (tok == "fri" || tok == "friday" || tok == "fr" || tok == "пт")
                day = 6;
            else if (tok == "sat" || tok == "saturday" || tok == "sa" || tok == "сб")
                day = 7;
            else if (tok == "sun" || tok == "sunday" || tok == "su" || tok == "вс")
                day = 1;
            else
                return false;
            out |= (uint8_t)(1u << (day - 1u));
            pos = end + 1;
        }
        mask = out;
        return true;
    }

    static bool parseTime_(const String &s, uint8_t &hour, uint8_t &minute)
    {
        const int p1 = s.indexOf(':');
        if (p1 <= 0)
            return false;
        const int h = s.substring(0, p1).toInt();
        const int m = s.substring(p1 + 1).toInt();
        if (h < 0 || h > 23)
            return false;
        if (m < 0 || m > 59)
            return false;
        hour = (uint8_t)h;
        minute = (uint8_t)m;
        return true;
    }

    static bool parseDuration_(const String &s, uint32_t &out)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
            if (s[i] < '0' || s[i] > '9')
                return false;
        const uint32_t v = (uint32_t)s.toInt();
        out = v;
        return true;
    }

    void printHeader_()
    {
        _c._io->println(F("  id  en  mon  port  tank  days           time   dur_m  act  res  lvl  name"));
    }

    void printRow_(const WateringController::RuleConfig &cfg, const WateringController::RuleState &st)
    {
        _c._io->print(F("  "));
        char buf[24] = {};
        snprintf(buf, sizeof(buf), "%u", (unsigned)cfg.id);
        _c.printPadStr_(buf, 2);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.enabled ? F("on") : F("off"), 3);
        _c._io->print(F(" "));
        _c.printPadStr_(st.status ? F("on") : F("off"), 3);
        _c._io->print(F("   "));
        if (cfg.port == WateringController::kInvalidPort)
            _c.printPadStr_(F("--"), 4);
        else
        {
            snprintf(buf, sizeof(buf), "%u", (unsigned)cfg.port);
            _c.printPadStr_(buf, 4);
        }
        _c._io->print(F("  "));
        if (cfg.tank_id == 0)
            _c.printPadStr_(F("--"), 4);
        else
        {
            snprintf(buf, sizeof(buf), "%u", (unsigned)cfg.tank_id);
            _c.printPadStr_(buf, 4);
        }
        _c._io->print(F("  "));
        char days[24] = {};
        formatWeekdays_(cfg.weekdays_mask, days, sizeof(days));
        _c.printPadStr_(days, 14);
        _c._io->print(F("  "));
        if (cfg.weekdays_mask)
            snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)cfg.hour, (unsigned)cfg.minute);
        else
            strncpy(buf, "--:--", sizeof(buf) - 1);
        _c.printPadStr_(buf, 5);
        _c._io->print(F("  "));
        if (cfg.duration_sec)
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)((cfg.duration_sec + 59) / 60));
        else
            strncpy(buf, "--", sizeof(buf) - 1);
        _c.printPadStr_(buf, 6);
        _c._io->print(F("  "));
        _c.printPadStr_(st.active ? F("on") : F("off"), 3);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.resume_after_refill ? F("on") : F("off"), 3);
        _c._io->print(F("  "));
        const __FlashStringHelper *lvl = F("low");
        if (cfg.resume_level == 1)
            lvl = F("mid");
        else if (cfg.resume_level == 2)
            lvl = F("full");
        _c.printPadStr_(lvl, 4);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.name.length() ? cfg.name.c_str() : "-", 16);
        _c._io->println();
    }

    void printInvalidId_() const
    {
        _c._io->print(F("Invalid id. Range: 1.."));
        _c._io->println(WateringController::kRuleCount);
    }

    void printIdRangeInline_() const
    {
        _c._io->print(F(" (1-"));
        _c._io->print(WateringController::kRuleCount);
        _c._io->print(F(")"));
    }

    static void formatWeekdays_(uint8_t mask, char *out, size_t cap)
    {
        if (!out || cap == 0)
            return;
        out[0] = '\0';
        if (mask == 0)
        {
            strncpy(out, "--", cap - 1);
            out[cap - 1] = '\0';
            return;
        }
        static const char *labels[7] = {"Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};
        static const uint8_t map[7] = {2, 3, 4, 5, 6, 7, 1};
        bool first = true;
        for (size_t i = 0; i < 7; ++i)
        {
            const uint8_t day = map[i];
            if ((mask & (uint8_t)(1u << (day - 1u))) == 0)
                continue;
            if (!first)
                strncat(out, " ", cap - strlen(out) - 1);
            strncat(out, labels[i], cap - strlen(out) - 1);
            first = false;
        }
    }

    bool handleSimpleEnable_(const String &cmd, const String &lower, bool enable)
    {
        const String prefix = enable ? "enable " : "disable ";
        if (!lower.startsWith(prefix))
            return false;
        String tail = cmd.substring(prefix.length());
        tail.trim();
        uint16_t id = 0;
        if (!parseId_(tail, id))
        {
            printInvalidId_();
            return true;
        }
        if (!_watering.setEnabled(id, enable))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }

    bool handleStatus_(const String &cmd, const String &lower)
    {
        if (!lower.startsWith("status "))
            return false;
        String rest = cmd.substring(7);
        rest.trim();
        const int space = rest.indexOf(' ');
        if (space <= 0)
        {
            _c._io->print(F("Usage: status <id> <on|off>"));
            return true;
        }
        String id_str = rest.substring(0, space);
        String val_str = rest.substring(space + 1);
        id_str.trim();
        val_str.trim();
        uint16_t id = 0;
        if (!parseId_(id_str, id))
        {
            printInvalidId_();
            return true;
        }
        bool enabled = false;
        if (!parseBool_(val_str, enabled))
        {
            _c._io->println(F("Invalid value"));
            return true;
        }
        if (!_watering.setStatus(id, enabled))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }

    bool handlePort_(const String &cmd, const String &lower)
    {
        if (!lower.startsWith("port "))
            return false;
        String rest = cmd.substring(5);
        rest.trim();
        const int space = rest.indexOf(' ');
        if (space <= 0)
        {
            _c._io->print(F("Usage: port <id> <port|none>"));
            return true;
        }
        String id_str = rest.substring(0, space);
        String port_str = rest.substring(space + 1);
        id_str.trim();
        port_str.trim();
        uint16_t id = 0;
        if (!parseId_(id_str, id))
        {
            printInvalidId_();
            return true;
        }
        uint8_t port = WateringController::kInvalidPort;
        if (!parsePort_(port_str, port))
        {
            _c._io->println(F("Invalid port"));
            return true;
        }
        if (!_watering.setPort(id, port))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }

    bool handleTank_(const String &cmd, const String &lower)
    {
        if (!lower.startsWith("tank "))
            return false;
        String rest = cmd.substring(5);
        rest.trim();
        const int space = rest.indexOf(' ');
        if (space <= 0)
        {
            _c._io->print(F("Usage: tank <id> <tank_id|none>"));
            return true;
        }
        String id_str = rest.substring(0, space);
        String tank_str = rest.substring(space + 1);
        id_str.trim();
        tank_str.trim();
        uint16_t id = 0;
        if (!parseId_(id_str, id))
        {
            printInvalidId_();
            return true;
        }
        uint8_t tank_id = 0;
        if (tank_str.length() == 0)
        {
            _c._io->println(F("Invalid tank"));
            return true;
        }
        String t = tank_str;
        t.toLowerCase();
        if (t == "none")
        {
            tank_id = 0;
        }
        else
        {
            for (size_t i = 0; i < t.length(); ++i)
                if (t[i] < '0' || t[i] > '9')
                {
                    _c._io->println(F("Invalid tank"));
                    return true;
                }
            const int v = t.toInt();
            if (v < 0 || v > 255)
            {
                _c._io->println(F("Invalid tank"));
                return true;
            }
            tank_id = (uint8_t)v;
        }
        if (!_watering.setTankId(id, tank_id))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }

    bool handleDays_(const String &cmd, const String &lower)
    {
        if (!lower.startsWith("days "))
            return false;
        String rest = cmd.substring(5);
        rest.trim();
        const int space = rest.indexOf(' ');
        if (space <= 0)
        {
            _c._io->print(F("Usage: days <id> <mon,tue,...|all|none>"));
            return true;
        }
        String id_str = rest.substring(0, space);
        String days_str = rest.substring(space + 1);
        id_str.trim();
        days_str.trim();
        uint16_t id = 0;
        if (!parseId_(id_str, id))
        {
            printInvalidId_();
            return true;
        }
        uint8_t mask = 0;
        if (!parseDays_(days_str, mask))
        {
            _c._io->println(F("Invalid days"));
            return true;
        }
        if (!_watering.setWeekdaysMask(id, mask))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }

    bool handleTime_(const String &cmd, const String &lower)
    {
        if (!lower.startsWith("time "))
            return false;
        String rest = cmd.substring(5);
        rest.trim();
        const int space = rest.indexOf(' ');
        if (space <= 0)
        {
            _c._io->print(F("Usage: time <id> <HH:MM>"));
            return true;
        }
        String id_str = rest.substring(0, space);
        String time_str = rest.substring(space + 1);
        id_str.trim();
        time_str.trim();
        uint16_t id = 0;
        if (!parseId_(id_str, id))
        {
            printInvalidId_();
            return true;
        }
        uint8_t h = 0;
        uint8_t m = 0;
        if (!parseTime_(time_str, h, m))
        {
            _c._io->println(F("Invalid time"));
            return true;
        }
        if (!_watering.setStartTime(id, h, m))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }

    bool handleDuration_(const String &cmd, const String &lower)
    {
        if (!lower.startsWith("duration "))
            return false;
        String rest = cmd.substring(9);
        rest.trim();
        const int space = rest.indexOf(' ');
        if (space <= 0)
        {
            _c._io->print(F("Usage: duration <id> <min>"));
            return true;
        }
        String id_str = rest.substring(0, space);
        String dur_str = rest.substring(space + 1);
        id_str.trim();
        dur_str.trim();
        uint16_t id = 0;
        if (!parseId_(id_str, id))
        {
            printInvalidId_();
            return true;
        }
        uint32_t dur_min = 0;
        if (!parseDuration_(dur_str, dur_min))
        {
            _c._io->println(F("Invalid duration"));
            return true;
        }
        if (!_watering.setDuration(id, dur_min * 60u))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }

    bool handleResume_(const String &cmd, const String &lower)
    {
        if (!lower.startsWith("resume "))
            return false;
        String rest = cmd.substring(7);
        rest.trim();
        const int space = rest.indexOf(' ');
        if (space <= 0)
        {
            _c._io->print(F("Usage: resume <id> <on|off>"));
            return true;
        }
        String id_str = rest.substring(0, space);
        String val_str = rest.substring(space + 1);
        id_str.trim();
        val_str.trim();
        uint16_t id = 0;
        if (!parseId_(id_str, id))
        {
            printInvalidId_();
            return true;
        }
        bool enabled = false;
        if (!parseBool_(val_str, enabled))
        {
            _c._io->println(F("Invalid value"));
            return true;
        }
        if (!_watering.setResumeAfterRefill(id, enabled))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }

    bool handleResumeLevel_(const String &cmd, const String &lower)
    {
        if (!lower.startsWith("resume_level "))
            return false;
        String rest = cmd.substring(13);
        rest.trim();
        const int space = rest.indexOf(' ');
        if (space <= 0)
        {
        _c._io->print(F("Usage: resume_level <id> <low|mid|full>"));
        return true;
        }
        String id_str = rest.substring(0, space);
        String lvl_str = rest.substring(space + 1);
        id_str.trim();
        lvl_str.trim();
        uint16_t id = 0;
        if (!parseId_(id_str, id))
        {
            printInvalidId_();
            return true;
        }
        String t = lvl_str;
        t.toLowerCase();
        uint8_t lvl = 0;
        if (t == "low")
            lvl = 0;
        else if (t == "mid")
            lvl = 1;
        else if (t == "full")
            lvl = 2;
        else
        {
            _c._io->println(F("Invalid level"));
            return true;
        }
        if (!_watering.setResumeLevel(id, lvl))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }
};
