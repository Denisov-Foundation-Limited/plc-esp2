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

#include "controllers/septic_controller.hpp"

template <typename ConsoleT>
class CLISepticT
{
public:
    CLISepticT(ConsoleT &console, SepticController &septic)
        : _c(console), _septic(septic) {}

    void printIdRangeInline() const
    {
        printIdRangeInline_();
    }

    void printHelpEnable()
    {
        _c._io->println(F("    show septic     - list septic"));
        _c._io->print(F("    show septic <id>"));
        printIdRangeInline_();
        _c._io->println(F(" - septic details"));
    }

    void printHelpConfigLines()
    {
        _c._io->println(F("  Septic:"));
        _c._io->println(F("    septic                 - enter Septic context"));
    }

    void printHelpContextLines()
    {
        _c._io->println(F("  Septic:"));
        _c._io->println(F("    show                     - list septic"));
        _c._io->print(F("    show <id>"));
        printIdRangeInline_();
        _c._io->println(F("                - septic details"));
        _c._io->print(F("    name <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <text>          - set septic name"));
        _c._io->print(F("    enable <id>"));
        printIdRangeInline_();
        _c._io->println(F("              - enable septic"));
        _c._io->print(F("    disable <id>"));
        printIdRangeInline_();
        _c._io->println(F("             - disable septic"));
        _c._io->print(F("    warning <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <port|none> - set warning input"));
        _c._io->print(F("    alarm <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <port|none>   - set alarm input"));
        _c._io->print(F("    relay_warn <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <port|none> - set warning relay"));
        _c._io->print(F("    relay_alarm <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <port|none>- set alarm relay"));
        _c._io->print(F("    monitor <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <on|off>     - set monitoring"));
    }

    void showSeptic()
    {
        bool any = false;
        printHeader_();
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = _septic.configByIndex(i);
            const auto *st = _septic.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            printRow_(*cfg, *st);
        }
        if (!any)
            _c._io->println(F("  none"));
    }

    void showSeptic(size_t id)
    {
        const auto *cfg = _septic.configByIndex(id - 1);
        const auto *st = _septic.stateByIndex(id - 1);
        if (!cfg || !st)
        {
            printInvalidId_();
            return;
        }
        _c._io->println(F("Septic:"));
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
            showSeptic();
            _c.printPrompt_();
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
                _c.printPrompt_();
                return true;
            }
            showSeptic(id);
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("enable ") || lower.startsWith("disable "))
        {
            const bool enable = lower.startsWith("enable ");
            String tail = cmd.substring(enable ? 7 : 8);
            tail.trim();
            uint16_t id = 0;
            if (!parseId_(tail, id))
            {
                printInvalidId_();
                _c.printPrompt_();
                return true;
            }
            if (!_septic.setEnabled(id, enable))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("name "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: name <id> <text>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String name_str = rest.substring(space + 1);
            id_str.trim();
            name_str.trim();
            uint16_t id = 0;
            if (!parseId_(id_str, id))
            {
                printInvalidId_();
                _c.printPrompt_();
                return true;
            }
            if (!_septic.setName(id, name_str))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("monitor "))
        {
            String rest = cmd.substring(8);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: monitor <id> <on|off>"));
                _c.printPrompt_();
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
                _c.printPrompt_();
                return true;
            }
            bool on = false;
            if (!parseBool_(val_str, on))
            {
                _c._io->println(F("Invalid value"));
                _c.printPrompt_();
                return true;
            }
            if (!_septic.setMonitoring(id, on))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (handlePortCmd_(cmd, lower, "warning", &SepticController::setWarningPort) ||
            handlePortCmd_(cmd, lower, "alarm", &SepticController::setAlarmPort) ||
            handlePortCmd_(cmd, lower, "relay_warn", &SepticController::setWarningRelay) ||
            handlePortCmd_(cmd, lower, "relay_alarm", &SepticController::setAlarmRelay))
        {
            _c.printPrompt_();
            return true;
        }

        return false;
    }

private:
    ConsoleT &_c;
    SepticController &_septic;

    void printHeader_()
    {
        _c._io->println(F("Septic:"));
        _c._io->println(F("  Unit      ID  En  Mon Name             Warn Alarm RWarn RAlarm W  A"));
        _c._io->println(F("  --------  --  --  --- ---------------- ---- ----- ----- ------ -- --"));
    }

    void printRow_(const SepticController::SepticConfig &cfg, const SepticController::SepticState &st)
    {
        if (!_c._io)
            return;
        _c._io->print(F("  "));
        _c.printPadStr_(F("CPU"), 8);
        _c._io->print(F("  "));
        _c.printPad_(cfg.id, 2);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.enabled ? F("on") : F("off"), 2);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.monitoring_on ? F("on") : F("off"), 3);
        _c._io->print(F("  "));
        const char *name_ptr = cfg.name.length() ? cfg.name.c_str() : "-";
        _c.printPadStr_(name_ptr, 16);
        _c._io->print(F("  "));
        printPort_(cfg.warning_port, 4);
        _c._io->print(F("  "));
        printPort_(cfg.alarm_port, 5);
        _c._io->print(F("  "));
        printPort_(cfg.relay_warning, 5);
        _c._io->print(F("  "));
        printPort_(cfg.relay_alarm, 6);
        _c._io->print(F("  "));
        _c.printPadStr_(st.warning ? F("1") : F("0"), 2);
        _c._io->print(F(" "));
        _c.printPadStr_(st.alarm ? F("1") : F("0"), 2);
        _c._io->println();
    }

    void printPort_(uint8_t port, uint8_t width)
    {
        if (port == SepticController::kInvalidPort)
        {
            _c.printPadStr_(F("--"), width);
            return;
        }
        _c.printPad_(port, width);
    }

    void printIdRangeInline_() const
    {
        _c._io->print(F(" (1.."));
        _c._io->print(SepticController::kSepticCount);
        _c._io->print(F(")"));
    }

    void printInvalidId_() const
    {
        _c._io->print(F("Invalid septic id (1.."));
        _c._io->print(SepticController::kSepticCount);
        _c._io->println(F(")"));
    }

    static bool parseId_(const String &s, uint16_t &out)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
        {
            const char c = s[i];
            if (c < '0' || c > '9')
                return false;
        }
        out = (uint16_t)s.toInt();
        return out >= 1 && out <= SepticController::kSepticCount;
    }

    static bool parsePort_(const String &s, uint8_t &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "none" || t == "-")
        {
            out = SepticController::kInvalidPort;
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

    bool handlePortCmd_(const String &cmd, const String &lower, const char *name,
                        bool (SepticController::*setter)(size_t, uint8_t))
    {
        const String prefix = String(name) + " ";
        if (!lower.startsWith(prefix))
            return false;
        String rest = cmd.substring(prefix.length());
        rest.trim();
        const int space = rest.indexOf(' ');
        if (space <= 0)
        {
            _c._io->print(F("Usage: "));
            _c._io->print(name);
            _c._io->print(F(" <id> <port|none>"));
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
        uint8_t port = SepticController::kInvalidPort;
        if (!parsePort_(port_str, port))
        {
            _c._io->println(F("Invalid port"));
            return true;
        }
        if (!(_septic.*setter)(id, port))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }
};
