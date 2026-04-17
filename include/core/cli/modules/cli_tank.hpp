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

#include "controllers/tank_controller.hpp"

template <typename ConsoleT>
class CLITankT
{
public:
    CLITankT(ConsoleT &console, TankController &tanks)
        : _c(console), _tanks(tanks) {}

    void printIdRangeInline() const
    {
        printIdRangeInline_();
    }

    String idRangeString() const
    {
        return idRangeString_();
    }

    void printHelpEnable()
    {
        _c._io->println(F("    show tanks     - list tanks"));
        _c._io->println(String(F("    show tank <id>")) + idRangeString_() + F(" - tank details"));
    }

    void printHelpConfigLines()
    {
        _c._io->println(F("  Tanks:"));
        _c._io->println(F("    tank                   - enter Tank context"));
    }

    void printHelpContextLines()
    {
        _c._io->println(F("  Tanks:"));
        _c._io->println(F("    show                     - list tanks"));
        const String range = idRangeString_();
        _c._io->println(String(F("    show <id>")) + range + F("                - tank details"));
        _c._io->println(String(F("    name <id>")) + range + F(" <text>          - set tank name"));
        _c._io->println(String(F("    enable <id>")) + range + F("              - enable tank"));
        _c._io->println(String(F("    disable <id>")) + range + F("             - disable tank"));
        _c._io->println(String(F("    power <id>")) + range + F(" <0|1>           - power on/off"));
        _c._io->println(String(F("    low <id>")) + range + F(" <port|none>   - set low level input"));
        _c._io->println(String(F("    mid <id>")) + range + F(" <port|none>   - set mid level input"));
        _c._io->println(String(F("    full <id>")) + range + F(" <port|none>  - set full level input"));
        _c._io->println(String(F("    valve <id>")) + range + F(" <port|none> - set valve relay"));
        _c._io->println(String(F("    pump <id>")) + range + F(" <port|none>  - set pump relay"));
        _c._io->println(String(F("    alarm <id>")) + range + F(" <port|none> - set alarm relay"));
    }

    void showTanks()
    {
        bool any = false;
        printHeader_();
        auto guard = _tanks.lockGuard();
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = _tanks.configByIndex(i);
            const auto *st = _tanks.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            printRow_(*cfg, *st);
        }
        if (!any)
            _c._io->println(F("  none"));
    }

    void showTank(size_t id)
    {
        auto guard = _tanks.lockGuard();
        const auto *cfg = _tanks.config(id);
        const auto *st = _tanks.state(id);
        if (!cfg || !st)
        {
            printInvalidId_();
            return;
        }
        _c._io->println(F("Tank:"));
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
            showTanks();
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
            showTank(id);
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
            if (!_tanks.setEnabled(id, enable))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("power "))
        {
            String rest = cmd.substring(6);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: power <id> <0|1>"));
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
            bool value = false;
            if (!parseBool_(val_str, value))
            {
                _c._io->println(F("Invalid value"));
                _c.printPrompt_();
                return true;
            }
            if (!_tanks.setPower(id, value))
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
            if (!_tanks.setName(id, name_str))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (handlePortCmd_(cmd, lower, "low", &TankController::setLevelLow) ||
            handlePortCmd_(cmd, lower, "mid", &TankController::setLevelMid) ||
            handlePortCmd_(cmd, lower, "full", &TankController::setLevelFull) ||
            handlePortCmd_(cmd, lower, "valve", &TankController::setValveRelay) ||
            handlePortCmd_(cmd, lower, "pump", &TankController::setPumpRelay) ||
            handlePortCmd_(cmd, lower, "alarm", &TankController::setAlarmRelay))
        {
            _c.printPrompt_();
            return true;
        }

        return false;
    }

private:
    ConsoleT &_c;
    TankController &_tanks;

    void printHeader_()
    {
        _c._io->println(F("Tanks:"));
        _c._io->println(F("  Unit      ID  En  Name             Low Mid Full Valve Pump Alarm Empty Pwr"));
        _c._io->println(F("  --------  --  --  ---------------- --- --- ---- ----- ---- ----- ----- ---"));
    }

    void printRow_(const TankController::TankConfig &cfg, const TankController::TankState &st)
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
        const char *name_ptr = cfg.name.length() ? cfg.name.c_str() : "-";
        _c.printPadStr_(name_ptr, 16);
        _c._io->print(F("  "));
        printPort_(cfg.level_low, 3);
        _c._io->print(F("  "));
        printPort_(cfg.level_mid, 3);
        _c._io->print(F("  "));
        printPort_(cfg.level_full, 4);
        _c._io->print(F("  "));
        printPort_(cfg.relay_valve, 5);
        _c._io->print(F("  "));
        printPort_(cfg.relay_pump, 4);
        _c._io->print(F("  "));
        printPort_(cfg.relay_alarm, 5);
        const bool empty = !(st.level_low || st.level_mid || st.level_full);
        _c.printPadStr_(empty ? F("yes") : F("no"), 5);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.power_on ? F("on") : F("off"), 3);
        _c._io->println();
    }

    void printPort_(uint8_t port, uint8_t width)
    {
        if (port == TankController::kInvalidPort)
        {
            _c.printPadStr_(F("--"), width);
            return;
        }
        _c.printPad_(port, width);
    }

    void printIdRangeInline_() const
    {
        _c._io->print(idRangeString_());
    }

    String idRangeString_() const
    {
        return String(F(" (1..")) + TankController::kTankCount + F(")");
    }

    void printInvalidId_() const
    {
        _c._io->print(F("Invalid tank id (1.."));
        _c._io->print(TankController::kTankCount);
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
        return out >= 1 && out <= TankController::kTankCount;
    }

    static bool parsePort_(const String &s, uint8_t &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "none" || t == "-")
        {
            out = TankController::kInvalidPort;
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
                        bool (TankController::*setter)(size_t, uint8_t))
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
        uint8_t port = TankController::kInvalidPort;
        if (!parsePort_(port_str, port))
        {
            _c._io->println(F("Invalid port"));
            return true;
        }
        if (!(_tanks.*setter)(id, port))
            _c._io->println(F("Failed"));
        else
            _c._io->println(F("OK"));
        return true;
    }

    
};
