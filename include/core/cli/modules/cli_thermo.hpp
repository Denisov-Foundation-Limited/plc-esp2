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
#include <math.h>
#include <stdint.h>

#include "boards/board_profile.hpp"
#include "controllers/meteo_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "hal/gpio/portio.hpp"
#include "hal/gpio/extender.hpp"

template <typename ConsoleT>
class CLIThermoT
{
public:
    CLIThermoT(ConsoleT &console, ThermoController &thermo, MeteoController &meteo)
        : _c(console), _thermo(thermo), _meteo(meteo) {}

    void printHeader()
    {
        printHeader_();
    }

    void printRow(const char *unit, const ThermoController::DeviceConfig &cfg,
                  const ThermoController::DeviceState &st)
    {
        printRow_(unit, cfg, st);
    }

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
        _c._io->println(F("    show thermo     - list thermo devices"));
        _c._io->println(String(F("    show thermo <id>")) + idRangeString_() + F(" - device details"));
    }

    void printHelpConfigLines()
    {
        _c._io->println(F("  Thermo:"));
        _c._io->println(F("    thermo                  - enter Thermo context"));
    }

    void printHelpContextLines()
    {
        _c._io->println(F("  Thermo:"));
        _c._io->println(F("    show                     - list thermo devices"));
        const String range = idRangeString_();
        _c._io->println(String(F("    show <id>")) + range + F("                - device details"));
        _c._io->println(String(F("    name <id>")) + range + F(" <text>          - set device name"));
        _c._io->println(String(F("    enable <id>")) + range + F("              - enable device"));
        _c._io->println(String(F("    disable <id>")) + range + F("             - disable device"));
        _c._io->println(String(F("    mode <id>")) + range + F(" <off|heat|cool|auto> - set mode"));
        _c._io->println(String(F("    sensor <id>")) + range + F(" <sensor|none>       - set meteo sensor"));
        _c._io->println(String(F("    target <id>")) + range + F(" <temp>              - set target temperature"));
        _c._io->println(String(F("    hyst <id>")) + range + F(" <temp>               - set hysteresis"));
        _c._io->println(String(F("    heat <id>")) + range + F(" <port|none>         - set heat relay port"));
        _c._io->println(String(F("    cool <id>")) + range + F(" <port|none>         - set cool relay port"));
        _c._io->println(String(F("    button <id>")) + range + F(" <port|none>         - set button port"));
    }

    void showDevices()
    {
        bool any = false;
        printHeader_();
        auto guard = _thermo.lockGuard();
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = _thermo.configByIndex(i);
            const auto *st = _thermo.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            printRow_("CPU", *cfg, *st);
        }
        if (!any)
            _c._io->println(F("  none"));
    }

    void showDevice(size_t id)
    {
        auto guard = _thermo.lockGuard();
        const auto *cfg = _thermo.config(id);
        const auto *st = _thermo.state(id);
        if (!cfg || !st)
        {
            printInvalidId_();
            return;
        }
        _c._io->println(F("Thermo device:"));
        printHeader_();
        printRow_("CPU", *cfg, *st);
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showDevices();
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
            showDevice(id);
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
            if (!_thermo.setEnabled(id, enable))
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
            if (!_thermo.setName(id, name_str))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("mode "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: mode <id> <off|heat|cool|auto>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String mode_str = rest.substring(space + 1);
            id_str.trim();
            mode_str.trim();
            uint16_t id = 0;
            if (!parseId_(id_str, id))
            {
                printInvalidId_();
                _c.printPrompt_();
                return true;
            }
            ThermoController::Mode mode = ThermoController::Mode::Off;
            if (!parseMode_(mode_str, mode))
            {
                _c._io->println(F("Invalid mode"));
                _c.printPrompt_();
                return true;
            }
            if (!_thermo.setMode(id, mode))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("sensor "))
        {
            String rest = cmd.substring(7);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: sensor <id> <sensor|none>"));
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
            uint8_t sensor = ThermoController::kInvalidSensor;
            if (!parseSensor_(val_str, sensor))
            {
                _c._io->println(F("Invalid sensor"));
                _c.printPrompt_();
                return true;
            }
            if (sensor != ThermoController::kInvalidSensor && !isMeteoActive_(sensor))
            {
                _c._io->println(F("Meteo sensor not active"));
                _c.printPrompt_();
                return true;
            }
            if (!_thermo.setSensor(id, sensor))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("target ") || lower.startsWith("hyst "))
        {
            const bool is_target = lower.startsWith("target ");
            String rest = cmd.substring(is_target ? 7 : 5);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: target|hyst <id> <temp>"));
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
            bool ok = false;
            if (is_target)
            {
                float parsed = 0.0f;
                if (!parseFloat_(val_str, parsed))
                {
                    _c._io->println(F("Invalid value"));
                    _c.printPrompt_();
                    return true;
                }
                ok = _thermo.setTarget(id, (int16_t)lroundf(parsed));
            }
            else
            {
                float val = 0.0f;
                if (!parseFloat_(val_str, val))
                {
                    _c._io->println(F("Invalid value"));
                    _c.printPrompt_();
                    return true;
                }
                ok = _thermo.setHysteresis(id, val);
            }
            if (!ok)
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("heat ") || lower.startsWith("cool ") || lower.startsWith("button "))
        {
            const bool is_heat = lower.startsWith("heat ");
            const bool is_cool = lower.startsWith("cool ");
            String rest = cmd.substring(is_heat ? 5 : (is_cool ? 5 : 7));
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: heat|cool|button <id> <port|none>"));
                _c.printPrompt_();
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
                _c.printPrompt_();
                return true;
            }
            uint8_t port = ThermoController::kInvalidPort;
            if (!parsePort_(port_str, port))
            {
                _c._io->println(F("Invalid port"));
                _c.printPrompt_();
                return true;
            }
            const PortIO::PinType type = is_heat || is_cool ? PortIO::PinType::Relay : PortIO::PinType::DInput;
            if (port != ThermoController::kInvalidPort && !isPortSelectable_(port, type))
            {
                _c._io->println(F("Port not available"));
                _c.printPrompt_();
                return true;
            }
            if (port != ThermoController::kInvalidPort && isPortUsedByOther_(id, port))
            {
                _c._io->println(F("Port already in use"));
                _c.printPrompt_();
                return true;
            }
            bool ok = false;
            if (is_heat)
                ok = _thermo.setHeatPort(id, port);
            else if (is_cool)
                ok = _thermo.setCoolPort(id, port);
            else
                ok = _thermo.setButtonPort(id, port);
            if (!ok)
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        return false;
    }

private:
    ConsoleT &_c;
    ThermoController &_thermo;
    MeteoController &_meteo;

    static bool parseId_(const String &s, uint16_t &out)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
            if (s[i] < '0' || s[i] > '9')
                return false;
        const int v = s.toInt();
        if (v < 1 || v > (int)ThermoController::kDeviceCount)
            return false;
        out = (uint16_t)v;
        return true;
    }

    static bool parsePort_(const String &s, uint8_t &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "none" || t == "-")
        {
            out = ThermoController::kInvalidPort;
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

    static bool parseSensor_(const String &s, uint8_t &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "none" || t == "-")
        {
            out = ThermoController::kInvalidSensor;
            return true;
        }
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const int v = t.toInt();
        if (v < 0 || v > (int)MeteoController::kSensorCount)
            return false;
        out = (uint8_t)v;
        return true;
    }

    static bool parseFloat_(const String &s, float &out)
    {
        if (s.length() == 0)
            return false;
        const char *c = s.c_str();
        char *end = nullptr;
        const float v = strtof(c, &end);
        if (end == c)
            return false;
        out = v;
        return true;
    }

    static bool parseMode_(const String &s, ThermoController::Mode &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "off" || t == "none")
        {
            out = ThermoController::Mode::Off;
            return true;
        }
        if (t == "heat" || t == "heat_only" || t == "only_heat")
        {
            out = ThermoController::Mode::Heat;
            return true;
        }
        if (t == "cool" || t == "cool_only" || t == "only_cool")
        {
            out = ThermoController::Mode::Cool;
            return true;
        }
        if (t == "auto")
        {
            out = ThermoController::Mode::Auto;
            return true;
        }
        return false;
    }

    bool isMeteoActive_(uint8_t id) const
    {
        auto guard = _meteo.lockGuard();
        const auto *cfg = _meteo.config(id);
        return cfg && cfg->enabled;
    }

    bool isPortSelectable_(uint8_t port, PortIO::PinType type) const
    {
        if (port >= PortIO::PORT_COUNT)
            return false;
        const auto &p = ActiveBoardProfile::PORTS[port];
        if (p.caps == Cap::None)
            return false;
        if (p.type != type)
            return false;
        if (p.backend != PortIO::Backend::Extender)
            return true;
        const auto *devs = _c._ext.devs();
        if (!devs || p.u.ext.dev >= _c._ext.devCount())
            return false;
        if (devs[p.u.ext.dev].type != Extender::Type::MCP23017)
            return false;
        return _c._ext.isPresent(p.u.ext.dev);
    }

    bool isPortUsedByOther_(uint16_t id, uint8_t port) const
    {
        auto guard = _thermo.lockGuard();
        const auto *self = _thermo.config(id);
        if (self && (self->heat_port == port || self->cool_port == port || self->button_port == port))
            return false;
        return _c.gpioPortUsed_(port);
    }

    void printIdRangeInline_() const
    {
        _c._io->print(idRangeString_());
    }

    String idRangeString_() const
    {
        return String(F(" (1..")) + ThermoController::kDeviceCount + F(")");
    }

    void printHeader_()
    {
        _c._io->println(F("Thermo:"));
        _c._io->println(F("  Unit      ID  En  Name             Mode        Sens  Target  Hyst  Heat  Cool  Btn  Power  State"));
        _c._io->println(F("  --------  --  --  ---------------- ----------  ----  ------  ----  ----  ----  ---  -----  -----"));
    }

    void printRow_(const char *unit, const ThermoController::DeviceConfig &cfg,
                   const ThermoController::DeviceState &st)
    {
        if (!_c._io)
            return;
        char buf[12] = {};
        _c._io->print(F("  "));
        _c.printPadStr_(unit && unit[0] ? unit : "-", 8);
        _c._io->print(F("  "));
        _c.printPad_(cfg.id, 2);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.enabled ? F("on") : F("off"), 2);
        _c._io->print(F("  "));
        const char *name_ptr = cfg.name.length() ? cfg.name.c_str() : "-";
        _c.printPadStr_(name_ptr, 16);
        _c._io->print(F("  "));
        _c.printPadStr_(ThermoController::modeName(cfg.mode), 10);
        _c._io->print(F("  "));
        if (cfg.sensor_id)
            _c.printPad_(cfg.sensor_id, 4);
        else
            _c.printPadStr_(F("--"), 4);
        _c._io->print(F("  "));
        snprintf(buf, sizeof(buf), "%d", (int)cfg.target_c);
        _c.printPadStr_(buf, 6);
        _c._io->print(F("  "));
        dtostrf(cfg.hysteresis, 0, 2, buf);
        _c.printPadStr_(buf, 4);
        _c._io->print(F("  "));
        if (cfg.heat_port != ThermoController::kInvalidPort)
            _c.printPad_(cfg.heat_port, 4);
        else
            _c.printPadStr_(F("--"), 4);
        _c._io->print(F("  "));
        if (cfg.cool_port != ThermoController::kInvalidPort)
            _c.printPad_(cfg.cool_port, 4);
        else
            _c.printPadStr_(F("--"), 4);
        _c._io->print(F("  "));
        if (cfg.button_port != ThermoController::kInvalidPort)
            _c.printPad_(cfg.button_port, 3);
        else
            _c.printPadStr_(F("---"), 3);
        _c._io->print(F("  "));
        _c.printPadStr_(st.power_on ? F("on") : F("off"), 5);
        _c._io->print(F("  "));
        const __FlashStringHelper *state = F("idle");
        if (!st.power_on || cfg.mode == ThermoController::Mode::Off)
            state = F("off");
        else if (st.heat_on)
            state = F("heat");
        else if (st.cool_on)
            state = F("cool");
        _c.printPadStr_(state, 5);
        _c._io->println();
    }

    void printInvalidId_() const
    {
        _c._io->print(F("Invalid thermo id (1.."));
        _c._io->print(ThermoController::kDeviceCount);
        _c._io->println(F(")"));
    }
};
