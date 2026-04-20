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

#include "utils/users_registry.hpp"

template <typename ConsoleT>
class CLIUserT
{
public:
    explicit CLIUserT(ConsoleT &console) : _c(console) {}

    void printHelpEnable() const
    {
        _c._io->println(F("    show users     - list users"));
        _c._io->println(F("    show user <id> - user details"));
    }

    void printHelpConfigLines() const
    {
        _c._io->println(F("  Users:"));
        _c._io->println(F("    user                   - enter User context"));
    }

    void printHelpContextLines() const
    {
        _c._io->println(F("  Users:"));
        _c._io->println(F("    show                    - list users"));
        _c._io->println(F("    show <id>               - user details"));
        _c._io->println(F("    enable <id>             - enable user"));
        _c._io->println(F("    disable <id>            - disable user"));
        _c._io->println(F("    username <id> <text>    - set username"));
        _c._io->println(F("    password <id> <text|clear> - set/clear web password"));
        _c._io->println(F("    phone <id> <num|none>   - set GSM phone"));
        _c._io->println(F("    sms <id> <on|off>       - set SMS notify"));
        _c._io->println(F("    call <id> <on|off>      - set call notify"));
        _c._io->println(F("    ibutton <id> <hex|clear> - set iButton key"));
        _c._io->println(F("    rfid <id> <hex|clear>   - set RFID key"));
        _c._io->println(F("  ACL:"));
        _c._io->println(F("    acl show <id> [unit]"));
        _c._io->println(F("    acl controller <id> <unit> <ctrl> <on|off>"));
        _c._io->println(F("    acl view <id> <unit> <ctrl> <item> <on|off>"));
        _c._io->println(F("    acl control <id> <unit> <ctrl> <item> <on|off>"));
        _c._io->println(F("    acl clear <id> <unit>   - clear all ACL bits for unit"));
        _c._io->println(F("    acl grant <id> <unit>   - grant all ACL bits for unit"));
    }

    void printHelpTopic() const
    {
        _c._io->println(F("User commands:"));
        _c._io->println(F("  user                   - enter User context"));
        printHelpContextLines();
    }

    void showUsers() const
    {
        _c._io->println(F("Users:"));
        _c._io->println(F("  id  en  username            phone"));
        for (size_t i = 0; i < _c._users.size(); ++i)
        {
            const auto &u = _c._users.user(i);
            _c._io->print(F("  "));
            _c._io->print(String((unsigned)(i + 1)));
            if (i + 1 < 10)
                _c._io->print(F("   "));
            else
                _c._io->print(F("  "));
            _c._io->print(u.enabled ? F("on  ") : F("off "));
            String username = u.username;
            while (username.length() < 18)
                username += ' ';
            _c._io->print(username);
            _c._io->println(u.gsm_phone);
        }
    }

    void showUser(uint8_t id) const
    {
        const auto *u = userById_(id);
        if (!u)
        {
            _c._io->println(F("Invalid user id"));
            return;
        }
        _c._io->println(F("User:"));
        _c._io->println(String(F("  id: ")) + String((unsigned)id));
        _c._io->println(String(F("  enabled: ")) + (u->enabled ? F("true") : F("false")));
        _c._io->println(String(F("  username: ")) + u->username);
        _c._io->println(String(F("  web_password: ")) + (u->hasWebPassword() ? F("***") : F("")));
        _c._io->println(String(F("  gsm_phone: ")) + u->gsm_phone);
        _c._io->println(String(F("  gsm_sms: ")) + (u->gsm_sms ? F("true") : F("false")));
        _c._io->println(String(F("  gsm_call: ")) + (u->gsm_call ? F("true") : F("false")));
        _c._io->println(String(F("  ibutton: ")) + u->ibutton_key);
        _c._io->println(String(F("  rfid: ")) + u->rfid_key);
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showUsers();
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("show "))
        {
            String tail = cmd.substring(5);
            tail.trim();
            uint16_t id = 0;
            if (!ConsoleT::parseUint_(tail, id) || !validUserId_(id))
                _c._io->println(F("Usage: show <id>"));
            else
                showUser((uint8_t)id);
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("enable ") || lower.startsWith("disable "))
        {
            const bool enabled = lower.startsWith("enable ");
            String tail = cmd.substring(enabled ? 7 : 8);
            tail.trim();
            uint16_t id = 0;
            if (!ConsoleT::parseUint_(tail, id) || !validUserId_(id))
                _c._io->println(F("Invalid user id"));
            else
            {
                _c._users.user((size_t)(id - 1)).enabled = enabled;
                _c._io->println(F("OK"));
            }
            _c.printPrompt_();
            return true;
        }
        if (handleTextCmd_(cmd, lower, "username", [](UsersRegistry::User &u, const String &v) { u.username = v; }) ||
            handleTextCmd_(cmd, lower, "phone", [](UsersRegistry::User &u, const String &v) { u.gsm_phone = (v == "none" || v == "clear") ? String() : UsersRegistry::normalizePhone(v); }) ||
            handleTextCmd_(cmd, lower, "ibutton", [](UsersRegistry::User &u, const String &v) { u.ibutton_key = (v == "clear" || v == "none") ? String() : UsersRegistry::normalizeHex(v, 16); }) ||
            handleTextCmd_(cmd, lower, "rfid", [](UsersRegistry::User &u, const String &v) { u.rfid_key = (v == "clear" || v == "none") ? String() : UsersRegistry::normalizeHex(v, 20); }))
        {
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("password "))
        {
            handlePassword_(cmd.substring(9));
            _c.printPrompt_();
            return true;
        }
        if (handleBoolCmd_(cmd, lower, "sms", [](UsersRegistry::User &u, bool on) { u.gsm_sms = on; }) ||
            handleBoolCmd_(cmd, lower, "call", [](UsersRegistry::User &u, bool on) { u.gsm_call = on; }))
        {
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("acl "))
        {
            handleAcl_(cmd.substring(4));
            _c.printPrompt_();
            return true;
        }
        return false;
    }

private:
    template <typename FnT>
    bool handleTextCmd_(const String &cmd, const String &lower, const char *name, FnT fn)
    {
        String prefix = String(name) + " ";
        if (!lower.startsWith(prefix))
            return false;
        String rest = cmd.substring(prefix.length());
        rest.trim();
        const int sp = rest.indexOf(' ');
        if (sp <= 0)
        {
            _c._io->print(F("Usage: "));
            _c._io->print(name);
            _c._io->println(F(" <id> <value>"));
            return true;
        }
        String id_str = rest.substring(0, sp);
        String value = rest.substring(sp + 1);
        id_str.trim();
        value.trim();
        uint16_t id = 0;
        if (!ConsoleT::parseUint_(id_str, id) || !validUserId_(id))
        {
            _c._io->println(F("Invalid user id"));
            return true;
        }
        fn(_c._users.user((size_t)(id - 1)), value);
        _c._io->println(F("OK"));
        return true;
    }

    template <typename FnT>
    bool handleBoolCmd_(const String &cmd, const String &lower, const char *name, FnT fn)
    {
        String prefix = String(name) + " ";
        if (!lower.startsWith(prefix))
            return false;
        String rest = cmd.substring(prefix.length());
        rest.trim();
        const int sp = rest.indexOf(' ');
        if (sp <= 0)
        {
            _c._io->print(F("Usage: "));
            _c._io->print(name);
            _c._io->println(F(" <id> <on|off>"));
            return true;
        }
        String id_str = rest.substring(0, sp);
        String value = rest.substring(sp + 1);
        id_str.trim();
        value.trim();
        uint16_t id = 0;
        bool on = false;
        if (!ConsoleT::parseUint_(id_str, id) || !validUserId_(id))
            _c._io->println(F("Invalid user id"));
        else if (!parseBool_(value, on))
            _c._io->println(F("Invalid value"));
        else
        {
            fn(_c._users.user((size_t)(id - 1)), on);
            _c._io->println(F("OK"));
        }
        return true;
    }

    void handlePassword_(String rest)
    {
        rest.trim();
        const int sp = rest.indexOf(' ');
        if (sp <= 0)
        {
            _c._io->println(F("Usage: password <id> <text|clear>"));
            return;
        }
        String id_str = rest.substring(0, sp);
        String value = rest.substring(sp + 1);
        id_str.trim();
        value.trim();
        uint16_t id = 0;
        if (!ConsoleT::parseUint_(id_str, id) || !validUserId_(id))
        {
            _c._io->println(F("Invalid user id"));
            return;
        }
        auto &u = _c._users.user((size_t)(id - 1));
        if (value == "clear" || value == "none")
            u.clearWebPassword();
        else if (!u.setWebPassword(value))
        {
            _c._io->println(F("Failed"));
            return;
        }
        _c._io->println(F("OK"));
    }

    void handleAcl_(String rest)
    {
        rest.trim();
        String parts[6];
        const size_t count = split_(rest, parts, 6);
        if (count == 0)
        {
            _c._io->println(F("Usage: acl <show|controller|view|control|clear|grant> ..."));
            return;
        }
        String op = parts[0];
        op.toLowerCase();
        if (op == "show")
        {
            if (count < 2)
            {
                _c._io->println(F("Usage: acl show <id> [unit]"));
                return;
            }
            uint16_t id = 0;
            uint16_t unit = 1;
            if (!ConsoleT::parseUint_(parts[1], id) || !validUserId_(id))
            {
                _c._io->println(F("Invalid user id"));
                return;
            }
            if (count >= 3 && (!ConsoleT::parseUint_(parts[2], unit) || !validUnit_(unit)))
            {
                _c._io->println(F("Invalid unit"));
                return;
            }
            showAcl_((uint8_t)id, (uint8_t)unit);
            return;
        }
        if (op == "clear" || op == "grant")
        {
            if (count < 3)
            {
                _c._io->println(op == "clear" ? F("Usage: acl clear <id> <unit>") : F("Usage: acl grant <id> <unit>"));
                return;
            }
            uint16_t id = 0, unit = 1;
            if (!ConsoleT::parseUint_(parts[1], id) || !validUserId_(id))
            {
                _c._io->println(F("Invalid user id"));
                return;
            }
            if (!ConsoleT::parseUint_(parts[2], unit) || !validUnit_(unit))
            {
                _c._io->println(F("Invalid unit"));
                return;
            }
            auto &u = _c._users.user((size_t)(id - 1));
            const uint8_t unit_idx = (uint8_t)(unit - 1);
            for (uint8_t ctrl_i = 0; ctrl_i < (uint8_t)UsersRegistry::kAclControllerCount; ++ctrl_i)
            {
                const auto ctrl = (UsersRegistry::AclController)ctrl_i;
                const bool allow = (op == "grant");
                u.setControllerAllowed(unit_idx, ctrl, allow);
                const uint16_t max_id = UsersRegistry::kAclItemsPerController[ctrl_i];
                for (uint16_t item = 1; item <= max_id; ++item)
                {
                    u.setItemView(unit_idx, ctrl, item, allow);
                    u.setItemControl(unit_idx, ctrl, item, allow);
                }
            }
            _c._io->println(F("OK"));
            return;
        }
        if (count < 5)
        {
            _c._io->println(F("Usage: acl controller <id> <unit> <ctrl> <on|off>"));
            _c._io->println(F("   or: acl view <id> <unit> <ctrl> <item> <on|off>"));
            _c._io->println(F("   or: acl control <id> <unit> <ctrl> <item> <on|off>"));
            return;
        }
        uint16_t id = 0, unit = 1;
        if (!ConsoleT::parseUint_(parts[1], id) || !validUserId_(id))
        {
            _c._io->println(F("Invalid user id"));
            return;
        }
        if (!ConsoleT::parseUint_(parts[2], unit) || !validUnit_(unit))
        {
            _c._io->println(F("Invalid unit"));
            return;
        }
        UsersRegistry::AclController ctrl;
        if (!parseCtrl_(parts[3], ctrl))
        {
            _c._io->println(F("Invalid controller"));
            return;
        }
        auto &u = _c._users.user((size_t)(id - 1));
        const uint8_t unit_idx = (uint8_t)(unit - 1);
        if (op == "controller")
        {
            bool on = false;
            if (!parseBool_(parts[4], on))
            {
                _c._io->println(F("Invalid value"));
                return;
            }
            u.setControllerAllowed(unit_idx, ctrl, on);
            _c._io->println(F("OK"));
            return;
        }
        if (count < 6)
        {
            _c._io->println(F("Usage: acl view|control <id> <unit> <ctrl> <item> <on|off>"));
            return;
        }
        uint16_t item = 0;
        bool on = false;
        if (!ConsoleT::parseUint_(parts[4], item) || item == 0)
        {
            _c._io->println(F("Invalid item id"));
            return;
        }
        if (!parseBool_(parts[5], on))
        {
            _c._io->println(F("Invalid value"));
            return;
        }
        if (op == "view")
            u.setItemView(unit_idx, ctrl, item, on);
        else if (op == "control")
            u.setItemControl(unit_idx, ctrl, item, on);
        else
        {
            _c._io->println(F("Unknown acl op"));
            return;
        }
        _c._io->println(F("OK"));
    }

    void showAcl_(uint8_t user_id, uint8_t unit_no) const
    {
        const auto &u = _c._users.user((size_t)(user_id - 1));
        const uint8_t unit_idx = (uint8_t)(unit_no - 1);
        _c._io->println(String(F("ACL user: ")) + String((unsigned)user_id) + F(" unit: ") + String((unsigned)unit_no));
        for (uint8_t ctrl_i = 0; ctrl_i < (uint8_t)UsersRegistry::kAclControllerCount; ++ctrl_i)
        {
            const auto ctrl = (UsersRegistry::AclController)ctrl_i;
            _c._io->print(F("  "));
            _c._io->print(ctrlName_(ctrl));
            _c._io->print(F(": ctrl="));
            _c._io->print(u.controllerAllowed(unit_idx, ctrl) ? F("on") : F("off"));
            _c._io->print(F(" view="));
            printItemList_(u, unit_idx, ctrl, true);
            _c._io->print(F(" control="));
            printItemList_(u, unit_idx, ctrl, false);
            _c._io->println();
        }
    }

    void printItemList_(const UsersRegistry::User &u, uint8_t unit_idx, UsersRegistry::AclController ctrl, bool view) const
    {
        bool first = true;
        const uint16_t max_id = UsersRegistry::kAclItemsPerController[(size_t)ctrl];
        for (uint16_t item = 1; item <= max_id; ++item)
        {
            const bool allowed = view ? u.itemViewAllowedRaw(unit_idx, ctrl, item)
                                      : u.itemControlAllowedRaw(unit_idx, ctrl, item);
            if (!allowed)
                continue;
            if (!first)
                _c._io->print(',');
            _c._io->print(String((unsigned)item));
            first = false;
        }
        if (first)
            _c._io->print('-');
    }

    static const char *ctrlName_(UsersRegistry::AclController ctrl)
    {
        switch (ctrl)
        {
        case UsersRegistry::AclController::Sockets: return "sockets";
        case UsersRegistry::AclController::Lights: return "lights";
        case UsersRegistry::AclController::Meteo: return "meteo";
        case UsersRegistry::AclController::Thermo: return "thermo";
        case UsersRegistry::AclController::Tanks: return "tanks";
        case UsersRegistry::AclController::Septic: return "septic";
        case UsersRegistry::AclController::Security: return "security";
        case UsersRegistry::AclController::Watering: return "watering";
        case UsersRegistry::AclController::Leak: return "leak";
        case UsersRegistry::AclController::Avr: return "avr";
        case UsersRegistry::AclController::Ring: return "ring";
        default: return "unknown";
        }
    }

    static bool parseCtrl_(String input, UsersRegistry::AclController &out)
    {
        input.trim();
        input.toLowerCase();
        if (input == "sockets") out = UsersRegistry::AclController::Sockets;
        else if (input == "lights") out = UsersRegistry::AclController::Lights;
        else if (input == "meteo") out = UsersRegistry::AclController::Meteo;
        else if (input == "thermo") out = UsersRegistry::AclController::Thermo;
        else if (input == "tanks") out = UsersRegistry::AclController::Tanks;
        else if (input == "septic") out = UsersRegistry::AclController::Septic;
        else if (input == "security") out = UsersRegistry::AclController::Security;
        else if (input == "watering") out = UsersRegistry::AclController::Watering;
        else if (input == "leak") out = UsersRegistry::AclController::Leak;
        else if (input == "avr") out = UsersRegistry::AclController::Avr;
        else if (input == "ring") out = UsersRegistry::AclController::Ring;
        else return false;
        return true;
    }

    static bool parseBool_(String input, bool &out)
    {
        input.trim();
        input.toLowerCase();
        if (input == "on" || input == "1" || input == "true")
        {
            out = true;
            return true;
        }
        if (input == "off" || input == "0" || input == "false")
        {
            out = false;
            return true;
        }
        return false;
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

    static bool validUserId_(uint16_t id)
    {
        return id >= 1 && id <= UsersRegistry::kMaxUsers;
    }

    static bool validUnit_(uint16_t unit)
    {
        return unit >= 1 && unit <= UsersRegistry::kAclUnitCount;
    }

    const UsersRegistry::User *userById_(uint8_t id) const
    {
        if (!validUserId_(id))
            return nullptr;
        return &_c._users.user((size_t)(id - 1));
    }

    ConsoleT &_c;
};
