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

#include "controllers/leak_controller.hpp"

template <typename ConsoleT>
class CLILeakT
{
public:
    CLILeakT(ConsoleT &console, LeakController &leak) : _c(console), _leak(leak) {}

    void printHelpConfigLines() const
    {
        _c._io->println(F("  Leak:"));
        _c._io->println(F("    leak                   - enter leak context"));
    }

    void printHelpContextLines() const
    {
        _c._io->println(F("  Leak:"));
        _c._io->println(F("    show                    - show all zones"));
        _c._io->println(F("    show <id>               - show one zone"));
        _c._io->println(F("    enable|disable          - enable/disable controller"));
        _c._io->println(F("    zone enable|disable <id> - enable/disable zone"));
        _c._io->println(F("    power <id> <on|off>     - zone power"));
        _c._io->println(F("    sensor <id> <port|none> - set sensor port"));
        _c._io->println(F("    valve <id> <port|none>  - set valve relay"));
        _c._io->println(F("    alarm <id> <port|none>  - set alarm relay"));
        _c._io->println(F("    active_low <id> <on|off> - sensor polarity"));
        _c._io->println(F("    open_on_power <id> <on|off> - valve logic"));
        _c._io->println(F("    name <id> <text>        - set zone name"));
        _c._io->println(F("    ack <id|all>            - clear latched alarm"));
    }

    void printHelpTopic() const
    {
        _c._io->println(F("Leak commands:"));
        _c._io->println(F("  leak                   - enter leak context"));
        printHelpContextLines();
    }

    void printIdRangeInline() const
    {
        _c._io->print(F(" (1.."));
        _c._io->print(LeakController::kZoneCount);
        _c._io->print(F(")"));
    }

    void showAll() const
    {
        _c._io->println(F("Leak zones:"));
        _c._io->println(F("  ID  En  Pwr  Wet  Lat  AL  VOP Sensor Valve Alarm Name"));
        _c._io->println(F("  --  --  ---  ---  ---  --  --- ------ ----- ----- ----------------"));
        for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            showZoneRow_(i + 1);
    }

    void showZone(size_t id) const
    {
        auto guard = _leak.lockGuard();
        const auto *cfg = _leak.config(id);
        const auto *st = _leak.state(id);
        if (!cfg || !st)
        {
            _c._io->println(F("Invalid leak id"));
            return;
        }
        _c._io->println(F("Leak zone:"));
        printBool_("  controller_enabled", _leak.controllerEnabled());
        printU16_("  id", (uint16_t)cfg->id);
        printBool_("  enabled", cfg->enabled);
        printBool_("  power_on", cfg->power_on);
        printBool_("  wet", st->wet);
        printBool_("  alarm_latched", st->alarm_latched);
        printBool_("  sensor_active_low", cfg->sensor_active_low);
        printBool_("  valve_open_on_power", cfg->valve_open_on_power);
        printPort_("  sensor_port", cfg->sensor_port);
        printPort_("  valve_port", cfg->valve_port);
        printPort_("  alarm_port", cfg->alarm_port);
        _c._io->print(F("  name: "));
        _c._io->println(cfg->name);
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showAll();
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
                _c._io->print(F("Usage: show <id>"));
                printIdRangeInline();
                _c._io->println();
            }
            else
            {
                showZone(id);
            }
            _c.printPrompt_();
            return true;
        }
        if (lower == "enable" || lower == "disable")
        {
            const bool on = lower == "enable";
            if (!_leak.setControllerEnabled(on))
                _c._io->println(F("No changes"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("zone enable ") || lower.startsWith("zone disable "))
        {
            const bool on = lower.startsWith("zone enable ");
            String tail = cmd.substring(on ? 12 : 13);
            tail.trim();
            uint16_t id = 0;
            if (!parseId_(tail, id))
            {
                _c._io->print(F("Usage: zone enable|disable <id>"));
                printIdRangeInline();
                _c._io->println();
            }
            else if (!_leak.setEnabled(id, on))
            {
                _c._io->println(F("No changes"));
            }
            else
            {
                _c._io->println(F("OK"));
            }
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("power "))
            return handleBoolById_(cmd, 6, "Usage: power <id> <on|off>", &CLILeakT::setPower_);
        if (lower.startsWith("active_low "))
            return handleBoolById_(cmd, 11, "Usage: active_low <id> <on|off>", &CLILeakT::setActiveLow_);
        if (lower.startsWith("open_on_power "))
            return handleBoolById_(cmd, 14, "Usage: open_on_power <id> <on|off>", &CLILeakT::setOpenOnPower_);
        if (lower.startsWith("sensor "))
            return handlePortById_(cmd, 7, "Usage: sensor <id> <port|none>", &CLILeakT::setSensor_);
        if (lower.startsWith("valve "))
            return handlePortById_(cmd, 6, "Usage: valve <id> <port|none>", &CLILeakT::setValve_);
        if (lower.startsWith("alarm "))
            return handlePortById_(cmd, 6, "Usage: alarm <id> <port|none>", &CLILeakT::setAlarm_);
        if (lower.startsWith("name "))
        {
            String tail = cmd.substring(5);
            tail.trim();
            const int sp = tail.indexOf(' ');
            if (sp <= 0)
            {
                _c._io->println(F("Usage: name <id> <text>"));
                _c.printPrompt_();
                return true;
            }
            String id_s = tail.substring(0, sp);
            String name = tail.substring(sp + 1);
            name.trim();
            uint16_t id = 0;
            if (!parseId_(id_s, id))
            {
                _c._io->print(F("Invalid id"));
                printIdRangeInline();
                _c._io->println();
            }
            else if (!_leak.setName(id, name))
            {
                _c._io->println(F("No changes"));
            }
            else
            {
                _c._io->println(F("OK"));
            }
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("ack "))
        {
            String tail = cmd.substring(4);
            tail.trim();
            tail.toLowerCase();
            bool changed = false;
            if (tail == "all")
                changed = _leak.ackAll();
            else
            {
                uint16_t id = 0;
                if (!parseId_(tail, id))
                {
                    _c._io->println(F("Usage: ack <id|all>"));
                    _c.printPrompt_();
                    return true;
                }
                changed = _leak.ack(id);
            }
            _c._io->println(changed ? F("OK") : F("No changes"));
            _c.printPrompt_();
            return true;
        }
        return false;
    }

private:
    using BoolSetter = bool (CLILeakT::*)(size_t, bool);
    using PortSetter = bool (CLILeakT::*)(size_t, uint8_t);

    ConsoleT &_c;
    LeakController &_leak;

    bool setPower_(size_t id, bool on) { return _leak.setPower(id, on); }
    bool setActiveLow_(size_t id, bool on) { return _leak.setSensorActiveLow(id, on); }
    bool setOpenOnPower_(size_t id, bool on) { return _leak.setValveOpenOnPower(id, on); }
    bool setSensor_(size_t id, uint8_t p) { return _leak.setSensorPort(id, p); }
    bool setValve_(size_t id, uint8_t p) { return _leak.setValvePort(id, p); }
    bool setAlarm_(size_t id, uint8_t p) { return _leak.setAlarmPort(id, p); }

    bool handleBoolById_(const String &cmd, size_t off, const char *usage, BoolSetter setter)
    {
        String tail = cmd.substring((unsigned)off);
        tail.trim();
        const int sp = tail.indexOf(' ');
        if (sp <= 0)
        {
            _c._io->println(usage);
            _c.printPrompt_();
            return true;
        }
        String id_s = tail.substring(0, sp);
        String val_s = tail.substring(sp + 1);
        val_s.trim();
        uint16_t id = 0;
        bool on = false;
        if (!parseId_(id_s, id) || !parseOnOff_(val_s, on))
        {
            _c._io->println(usage);
            _c.printPrompt_();
            return true;
        }
        if (!(this->*setter)(id, on))
            _c._io->println(F("No changes"));
        else
            _c._io->println(F("OK"));
        _c.printPrompt_();
        return true;
    }

    bool handlePortById_(const String &cmd, size_t off, const char *usage, PortSetter setter)
    {
        String tail = cmd.substring((unsigned)off);
        tail.trim();
        const int sp = tail.indexOf(' ');
        if (sp <= 0)
        {
            _c._io->println(usage);
            _c.printPrompt_();
            return true;
        }
        String id_s = tail.substring(0, sp);
        String val_s = tail.substring(sp + 1);
        val_s.trim();
        uint16_t id = 0;
        uint8_t port = LeakController::kInvalidPort;
        if (!parseId_(id_s, id) || !parsePort_(val_s, port))
        {
            _c._io->println(usage);
            _c.printPrompt_();
            return true;
        }
        if (!(this->*setter)(id, port))
            _c._io->println(F("No changes"));
        else
            _c._io->println(F("OK"));
        _c.printPrompt_();
        return true;
    }

    static bool parseId_(const String &s, uint16_t &id)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
            if (s[i] < '0' || s[i] > '9')
                return false;
        id = (uint16_t)strtoul(s.c_str(), nullptr, 10);
        return id >= 1 && id <= LeakController::kZoneCount;
    }

    static bool parseOnOff_(String s, bool &out)
    {
        s.trim();
        s.toLowerCase();
        if (s == "on" || s == "1" || s == "true" || s == "yes")
        {
            out = true;
            return true;
        }
        if (s == "off" || s == "0" || s == "false" || s == "no")
        {
            out = false;
            return true;
        }
        return false;
    }

    static bool parsePort_(String s, uint8_t &out)
    {
        s.trim();
        s.toLowerCase();
        if (s.length() == 0 || s == "none" || s == "-")
        {
            out = LeakController::kInvalidPort;
            return true;
        }
        for (size_t i = 0; i < s.length(); ++i)
            if (s[i] < '0' || s[i] > '9')
                return false;
        const unsigned long raw = strtoul(s.c_str(), nullptr, 10);
        if (raw > 255u)
            return false;
        out = (uint8_t)raw;
        return true;
    }

    static void printPad_(Stream *io, const char *s, uint8_t width)
    {
        if (!io)
            return;
        const char *v = s ? s : "";
        io->print(v);
        const size_t len = strlen(v);
        if (len < width)
            for (size_t i = 0; i < width - len; ++i)
                io->print(' ');
    }

    void showZoneRow_(size_t id) const
    {
        auto guard = _leak.lockGuard();
        const auto *cfg = _leak.config(id);
        const auto *st = _leak.state(id);
        if (!cfg || !st || !_c._io)
            return;
        _c._io->print(F("  "));
        if (id < 10)
            _c._io->print(' ');
        _c._io->print((unsigned)id);
        _c._io->print(F("  "));
        printPad_(_c._io, cfg->enabled ? "on" : "off", 2);
        _c._io->print(F("  "));
        printPad_(_c._io, cfg->power_on ? "on" : "off", 3);
        _c._io->print(F("  "));
        printPad_(_c._io, st->wet ? "yes" : "no", 3);
        _c._io->print(F("  "));
        printPad_(_c._io, st->alarm_latched ? "yes" : "no", 3);
        _c._io->print(F("  "));
        printPad_(_c._io, cfg->sensor_active_low ? "on" : "off", 2);
        _c._io->print(F("  "));
        printPad_(_c._io, cfg->valve_open_on_power ? "on" : "off", 3);
        _c._io->print(F("  "));
        printPortShort_(cfg->sensor_port);
        _c._io->print(F("   "));
        printPortShort_(cfg->valve_port);
        _c._io->print(F("   "));
        printPortShort_(cfg->alarm_port);
        _c._io->print(F("  "));
        _c._io->println(cfg->name);
    }

    void printPortShort_(uint8_t p) const
    {
        if (!_c._io)
            return;
        if (p == LeakController::kInvalidPort)
        {
            _c._io->print(F("-"));
            return;
        }
        if (p < 10)
            _c._io->print(' ');
        if (p < 100)
            _c._io->print(' ');
        _c._io->print((unsigned)p);
    }

    void printBool_(const char *k, bool v) const
    {
        _c._io->print(k);
        _c._io->print(F(": "));
        _c._io->println(v ? F("on") : F("off"));
    }

    void printU16_(const char *k, uint16_t v) const
    {
        _c._io->print(k);
        _c._io->print(F(": "));
        _c._io->println((unsigned)v);
    }

    void printPort_(const char *k, uint8_t p) const
    {
        _c._io->print(k);
        _c._io->print(F(": "));
        if (p == LeakController::kInvalidPort)
            _c._io->println(F("none"));
        else
            _c._io->println((unsigned)p);
    }
};
