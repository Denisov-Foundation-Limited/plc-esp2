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

#include "controllers/security_controller.hpp"
#include "hal/ibutton.hpp"

template <typename ConsoleT>
class CLISecurityT
{
public:
    CLISecurityT(ConsoleT &console, SecurityController &security)
        : _c(console), _security(security) {}

    String idRangeString() const
    {
        return idRangeString_();
    }

    void printHelpEnable()
    {
        _c._io->println(F("    show security       - list security sensors"));
        _c._io->println(String(F("    show security <id>")) + idRangeString_() + F(" - sensor details"));
        _c._io->println(F("    security status     - show security status"));
        _c._io->println(F("    security arm        - arm security"));
        _c._io->println(F("    security disarm     - disarm security"));
    }

    void printHelpConfigLines()
    {
        _c._io->println(F("  Security:"));
        _c._io->println(F("    security                - enter Security context"));
    }

    void printHelpContextLines()
    {
        _c._io->println(F("  Security:"));
        _c._io->println(F("    show                    - list sensors"));
        const String range = idRangeString_();
        _c._io->println(String(F("    show <id>")) + range + F("                - sensor details"));
        _c._io->println(String(F("    enable <id>")) + range + F("              - enable sensor"));
        _c._io->println(String(F("    disable <id>")) + range + F("             - disable sensor"));
        _c._io->println(String(F("    type <id>")) + range + F(" <pir|reed>     - set sensor type"));
        _c._io->println(String(F("    port <id>")) + range + F(" <port|none>   - set sensor port"));
        _c._io->println(String(F("    name <id>")) + range + F(" <text>        - set sensor name"));
        _c._io->println(String(F("    silent <id>")) + range + F(" <on|off>     - set silent flag"));
        _c._io->println(F("    siren <port|none>      - set siren port"));
        _c._io->println(F("    keys list              - list iButton keys"));
        _c._io->println(F("    key add <hex16> [name] - add iButton key"));
        _c._io->println(F("    key name <hex16> <text> - set key name"));
        _c._io->println(F("    key del <hex16>        - remove iButton key"));
        _c._io->println(F("    key clear              - clear iButton keys"));
        _c._io->println(F("    phones list            - list GSM phones"));
        _c._io->println(F("    phone set <id> <num|none> [name] - set phone"));
        _c._io->println(F("    phone name <id> <text> - set phone name"));
        _c._io->println(F("    phone notify <id> <on|off> - set phone notify"));
        _c._io->println(F("    phone call <id> <on|off> - set phone call"));
        _c._io->println(F("    phone enable <id> <on|off> - enable phone"));
        _c._io->println(F("    phone clear            - clear GSM phones"));
    }

    void showSensors()
    {
        _c._io->println(F("Security sensors:"));
        _c._io->println(F("  ID  En  Type  Port  Silent  Detect  Name"));
        _c._io->println(F("  --  --  ----  ----  ------  ------  ----------------"));
        bool any = false;
        auto guard = _security.lockGuard();
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = _security.configByIndex(i);
            const auto *st = _security.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            printSensorRow_(*cfg, *st);
        }
        if (!any)
            _c._io->println(F("  none"));
    }

    void showSensor(size_t id)
    {
        auto guard = _security.lockGuard();
        const auto *cfg = _security.config(id);
        const auto *st = _security.state(id);
        if (!cfg || !st)
        {
            printInvalidId_();
            return;
        }
        _c._io->println(F("Security sensor:"));
        _c._io->println(F("  ID  En  Type  Port  Silent  Detect  Name"));
        _c._io->println(F("  --  --  ----  ----  ------  ------  ----------------"));
        printSensorRow_(*cfg, *st);
    }

    void printStatus()
    {
        _c._io->println(F("Security status:"));
        _c.printKeyValueTab_(F("enabled"), _security.controllerEnabled() ? F("yes") : F("no"), 7);
        _c.printKeyValueTab_(F("armed"), _security.armed() ? F("yes") : F("no"), 7);
        _c.printKeyValueTab_(F("alarm"), _security.alarmOn() ? F("on") : F("off"), 7);
        if (_security.sirenPort() != SecurityController::kInvalidPort)
            _c.printKeyValueTab_(F("siren"), String((unsigned)_security.sirenPort()), 7);
        else
            _c.printKeyValueTab_(F("siren"), F("none"), 7);
    }

    bool handleEnable(const String &line)
    {
        String cmd = line;
        cmd.trim();
        cmd.toLowerCase();
        if (cmd == "security status")
        {
            printStatus();
            _c.printPrompt_();
            return true;
        }
        if (cmd == "security arm")
        {
            if (_security.armFrom("cli", "admin"))
                _c._io->println(F("OK"));
            else
                _c._io->println(F("Security disabled"));
            _c.printPrompt_();
            return true;
        }
        if (cmd == "security disarm")
        {
            if (_security.disarmFrom("cli", "admin"))
                _c._io->println(F("OK"));
            else
                _c._io->println(F("Security disabled"));
            _c.printPrompt_();
            return true;
        }
        return false;
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showSensors();
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
            showSensor(id);
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
            if (!_security.setEnabled(id, enable))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("type "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: type <id> <pir|reed>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String type_str = rest.substring(space + 1);
            id_str.trim();
            type_str.trim();
            uint16_t id = 0;
            if (!parseId_(id_str, id))
            {
                printInvalidId_();
                _c.printPrompt_();
                return true;
            }
            SecurityController::SensorType type = SecurityController::SensorType::Pir;
            if (!parseType_(type_str, type))
            {
                _c._io->println(F("Invalid type"));
                _c.printPrompt_();
                return true;
            }
            if (!_security.setType(id, type))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("port "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: port <id> <port|none>"));
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
            uint8_t port = SecurityController::kInvalidPort;
            if (!parsePort_(port_str, port))
            {
                _c._io->println(F("Invalid port"));
                _c.printPrompt_();
                return true;
            }
            if (!_security.setPort(id, port))
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
            if (!_security.setName(id, name_str))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("silent "))
        {
            String rest = cmd.substring(7);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: silent <id> <on|off>"));
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
            bool silent = false;
            if (!parseBool_(val_str, silent))
            {
                _c._io->println(F("Invalid value"));
                _c.printPrompt_();
                return true;
            }
            if (!_security.setSilent(id, silent))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("siren "))
        {
            String v = cmd.substring(6);
            v.trim();
            uint8_t port = SecurityController::kInvalidPort;
            if (!parsePort_(v, port))
            {
                _c._io->println(F("Invalid siren port"));
                _c.printPrompt_();
                return true;
            }
            _security.setSirenPort(port);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower == "keys list")
        {
            printKeyList_();
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("key add "))
        {
            String rest = cmd.substring(8);
            rest.trim();
            String hex = rest;
            String name;
            const int sp = rest.indexOf(' ');
            if (sp > 0)
            {
                hex = rest.substring(0, sp);
                name = rest.substring(sp + 1);
                name.trim();
            }
            uint8_t addr[8] = {};
            if (!parseHex_(hex, addr))
            {
                _c._io->println(F("Invalid key"));
                _c.printPrompt_();
                return true;
            }
            if (_security.addKey(addr, name))
                _c._io->println(F("OK"));
            else
                _c._io->println(F("Key list full"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("key name "))
        {
            String rest = cmd.substring(9);
            rest.trim();
            const int sp = rest.indexOf(' ');
            if (sp <= 0)
            {
                _c._io->println(F("Usage: key name <hex16> <text>"));
                _c.printPrompt_();
                return true;
            }
            String hex = rest.substring(0, sp);
            String name = rest.substring(sp + 1);
            hex.trim();
            name.trim();
            uint8_t addr[8] = {};
            if (!parseHex_(hex, addr))
            {
                _c._io->println(F("Invalid key"));
                _c.printPrompt_();
                return true;
            }
            if (_security.setKeyNameByAddr(addr, name))
                _c._io->println(F("OK"));
            else
                _c._io->println(F("Not found"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("key del "))
        {
            String hex = cmd.substring(8);
            hex.trim();
            uint8_t addr[8] = {};
            if (!parseHex_(hex, addr))
            {
                _c._io->println(F("Invalid key"));
                _c.printPrompt_();
                return true;
            }
            if (_security.removeKey(addr))
                _c._io->println(F("OK"));
            else
                _c._io->println(F("Not found"));
            _c.printPrompt_();
            return true;
        }
        if (lower == "key clear")
        {
            _security.clearKeys();
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower == "phones list")
        {
            printPhoneList_();
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("phone set "))
        {
            String rest = cmd.substring(10);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: phone set <id> <num|none> [name]"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String num_and_name = rest.substring(space + 1);
            id_str.trim();
            num_and_name.trim();
            String num_str = num_and_name;
            String name;
            const int sp = num_and_name.indexOf(' ');
            if (sp > 0)
            {
                num_str = num_and_name.substring(0, sp);
                name = num_and_name.substring(sp + 1);
                name.trim();
            }
            uint16_t id = 0;
            if (!parsePhoneId_(id_str, id))
            {
                printInvalidPhoneId_();
                _c.printPrompt_();
                return true;
            }
            const size_t idx = (size_t)(id - 1);
            if (num_str == "none" || num_str == "-")
            {
                _security.setPhone(idx, "");
                _security.setPhoneEnabled(idx, false);
                _security.setPhoneName(idx, "");
                _security.setPhoneNotify(idx, false);
                _security.setPhoneCall(idx, false);
            }
            else
            {
                _security.setPhone(idx, num_str);
                _security.setPhoneEnabled(idx, true);
                _security.setPhoneName(idx, name);
            }
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("phone name "))
        {
            String rest = cmd.substring(11);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: phone name <id> <text>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String name = rest.substring(space + 1);
            id_str.trim();
            name.trim();
            uint16_t id = 0;
            if (!parsePhoneId_(id_str, id))
            {
                printInvalidPhoneId_();
                _c.printPrompt_();
                return true;
            }
            _security.setPhoneName((size_t)(id - 1), name);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("phone enable "))
        {
            String rest = cmd.substring(13);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: phone enable <id> <on|off>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String val_str = rest.substring(space + 1);
            id_str.trim();
            val_str.trim();
            uint16_t id = 0;
            if (!parsePhoneId_(id_str, id))
            {
                printInvalidPhoneId_();
                _c.printPrompt_();
                return true;
            }
            bool enabled = false;
            if (!parseBool_(val_str, enabled))
            {
                _c._io->println(F("Invalid value"));
                _c.printPrompt_();
                return true;
            }
            _security.setPhoneEnabled((size_t)(id - 1), enabled);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("phone notify "))
        {
            String rest = cmd.substring(13);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: phone notify <id> <on|off>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String val_str = rest.substring(space + 1);
            id_str.trim();
            val_str.trim();
            uint16_t id = 0;
            if (!parsePhoneId_(id_str, id))
            {
                printInvalidPhoneId_();
                _c.printPrompt_();
                return true;
            }
            bool notify = false;
            if (!parseBool_(val_str, notify))
            {
                _c._io->println(F("Invalid value"));
                _c.printPrompt_();
                return true;
            }
            _security.setPhoneNotify((size_t)(id - 1), notify);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("phone call "))
        {
            String rest = cmd.substring(11);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: phone call <id> <on|off>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String val_str = rest.substring(space + 1);
            id_str.trim();
            val_str.trim();
            uint16_t id = 0;
            if (!parsePhoneId_(id_str, id))
            {
                printInvalidPhoneId_();
                _c.printPrompt_();
                return true;
            }
            bool call = false;
            if (!parseBool_(val_str, call))
            {
                _c._io->println(F("Invalid value"));
                _c.printPrompt_();
                return true;
            }
            _security.setPhoneCall((size_t)(id - 1), call);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower == "phone clear")
        {
            _security.clearPhones();
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        return false;
    }

    void printIdRangeInline() const
    {
        printIdRangeInline_();
    }

private:
    ConsoleT &_c;
    SecurityController &_security;

    static bool parseId_(const String &s, uint16_t &out)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
            if (s[i] < '0' || s[i] > '9')
                return false;
        const int v = s.toInt();
        if (v < 1 || v > (int)SecurityController::kSensorCount)
            return false;
        out = (uint16_t)v;
        return true;
    }
    static bool parsePhoneId_(const String &s, uint16_t &out)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
            if (s[i] < '0' || s[i] > '9')
                return false;
        const int v = s.toInt();
        if (v < 1 || v > (int)SecurityController::kPhoneCount)
            return false;
        out = (uint16_t)v;
        return true;
    }

    static bool parseType_(const String &s, SecurityController::SensorType &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "pir")
        {
            out = SecurityController::SensorType::Pir;
            return true;
        }
        if (t == "reed")
        {
            out = SecurityController::SensorType::Reed;
            return true;
        }
        return false;
    }

    static bool parsePort_(const String &s, uint8_t &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "none" || t == "-")
        {
            out = SecurityController::kInvalidPort;
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
        if (t == "1" || t == "on" || t == "yes" || t == "true")
        {
            out = true;
            return true;
        }
        if (t == "0" || t == "off" || t == "no" || t == "false")
        {
            out = false;
            return true;
        }
        return false;
    }

    static int hexNibble_(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        return -1;
    }

    static bool parseHex_(const String &s, uint8_t out[8])
    {
        if (s.length() != 16)
            return false;
        for (uint8_t i = 0; i < 8; ++i)
        {
            const int hi = hexNibble_(s[i * 2]);
            const int lo = hexNibble_(s[i * 2 + 1]);
            if (hi < 0 || lo < 0)
                return false;
            out[i] = (uint8_t)((hi << 4) | lo);
        }
        return true;
    }

    void printKeyList_()
    {
        _c._io->println(F("iButton keys:"));
        const size_t count = _security.keyCount();
        if (count == 0)
        {
            _c._io->println(F("  none"));
            return;
        }
        _c._io->println(F("  Serial            Name"));
        _c._io->println(F("  ----------------  ----------------"));
        for (size_t i = 0; i < count; ++i)
        {
            uint8_t addr[8] = {};
            if (!_security.keyByIndex(i, addr))
                continue;
            char hex[17] = {};
            IButton::toHex(addr, hex);
            _c._io->print(F("  "));
            _c.printPadStr_(hex, 16);
            _c._io->print(F("  "));
            const String &name = _security.keyNameByIndex(i);
            _c._io->println(name.length() ? name : "-");
        }
    }

    void printIdRangeInline_() const
    {
        _c._io->print(idRangeString_());
    }

    String idRangeString_() const
    {
        return String(F(" (1..")) + SecurityController::kSensorCount + F(")");
    }

    void printInvalidId_() const
    {
        _c._io->print(F("Invalid sensor id (1.."));
        _c._io->print(SecurityController::kSensorCount);
        _c._io->println(F(")"));
    }
    void printInvalidPhoneId_() const
    {
        _c._io->print(F("Invalid phone id (1.."));
        _c._io->print(SecurityController::kPhoneCount);
        _c._io->println(F(")"));
    }
    void printPhoneList_()
    {
        _c._io->println(F("GSM phones:"));
        _c._io->println(F("  ID  En  Nfy Call Number           Name"));
        _c._io->println(F("  --  --  --- ---- ---------------- ----------------"));
        bool any = false;
        for (size_t i = 0; i < SecurityController::kPhoneCount; ++i)
        {
            String number;
            bool enabled = false;
            if (!_security.phoneSlot(i, number, enabled))
                continue;
            if (number.length() == 0 && !enabled)
                continue;
            any = true;
            _c._io->print(F("  "));
            _c.printPad_((uint8_t)(i + 1), 2);
            _c._io->print(F("  "));
            _c.printPadStr_(enabled ? F("on") : F("off"), 2);
            _c._io->print(F("  "));
            _c.printPadStr_(_security.phoneNotifyByIndex(i) ? F("on") : F("off"), 3);
            _c._io->print(F(" "));
            _c.printPadStr_(_security.phoneCallByIndex(i) ? F("on") : F("off"), 4);
            _c._io->print(F(" "));
            _c.printPadStr_(number.length() ? number.c_str() : "-", 16);
            _c._io->print(F(" "));
            const String &name = _security.phoneNameByIndex(i);
            _c._io->println(name.length() ? name : "-");
        }
        if (!any)
            _c._io->println(F("  none"));
    }

    void printSensorRow_(const SecurityController::SensorConfig &cfg,
                         const SecurityController::SensorState &st)
    {
        _c._io->print(F("  "));
        _c.printPad_(cfg.id, 2);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.enabled ? F("on") : F("off"), 2);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.type == SecurityController::SensorType::Reed ? F("reed") : F("pir"), 4);
        _c._io->print(F("  "));
        if (cfg.port != SecurityController::kInvalidPort)
            _c.printPad_(cfg.port, 4);
        else
            _c.printPadStr_(F("--"), 4);
        _c._io->print(F("  "));
        _c.printPadStr_(cfg.silent ? F("yes") : F("no"), 6);
        _c._io->print(F("  "));
        _c.printPadStr_(st.is_detect ? F("yes") : F("no"), 6);
        _c._io->print(F("  "));
        const char *name = cfg.name.length() ? cfg.name.c_str() : "-";
        _c._io->println(name);
    }
};
