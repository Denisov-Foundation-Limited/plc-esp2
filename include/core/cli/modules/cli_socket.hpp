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

#include "controllers/socket/socket_controller.hpp"

template <typename ConsoleT>
class CLISocketT
{
public:
    CLISocketT(ConsoleT &console, SocketController &sockets)
        : _c(console), _sockets(sockets) {}

    void printHelpEnable()
    {
        _c._io->println(F("    show sockets   - list sockets"));
        _c._io->println(F("    show socket <id> - socket details"));
    }

    void printHelpConfigLines()
    {
        _c._io->println(F("  Sockets:"));
        _c._io->println(F("    socket                   - enter Socket context"));
    }

    void printHelpContextLines()
    {
        _c._io->println(F("  Sockets:"));
        _c._io->println(F("    show                     - list sockets"));
        _c._io->println(F("    show <id>                - socket details"));
        _c._io->println(F("    enable <id>              - enable socket"));
        _c._io->println(F("    disable <id>             - disable socket"));
        _c._io->println(F("    name <id> <value>        - set socket name"));
        _c._io->println(F("    button <id> <port|none>  - set button port"));
        _c._io->println(F("    relay <id> <port|none>   - set relay port"));
    }

    void showSockets()
    {
        bool any = false;
        _c.printSocketsHeader_();
        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const SocketController::SocketConfig *cfg = _sockets.config(i);
            const SocketController::SocketState *st = _sockets.state(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            const int button = (cfg->button_port == SocketController::kInvalidPort) ? -1 : cfg->button_port;
            const int relay = (cfg->relay_port == SocketController::kInvalidPort) ? -1 : cfg->relay_port;
            _c.printSocketRow_("CPU", (uint8_t)i, cfg->enabled,
                               cfg->name.c_str(), button, relay, st->relay_on);
        }
        const bool requested = _c._stack_cli.requestStackSockets_();
        if (!any && !requested)
            _c._io->println(F("  none"));
    }

    void showSocket(size_t idx)
    {
        const SocketController::SocketConfig *cfg = _sockets.config(idx);
        const SocketController::SocketState *st = _sockets.state(idx);
        if (!cfg || !st)
        {
            _c._io->println(F("Invalid socket id"));
            return;
        }
        _c._io->println(F("Socket:"));
        _c.printSocketsHeader_();
        const int button = (cfg->button_port == SocketController::kInvalidPort) ? -1 : cfg->button_port;
        const int relay = (cfg->relay_port == SocketController::kInvalidPort) ? -1 : cfg->relay_port;
        _c.printSocketRow_("CPU", (uint8_t)idx, cfg->enabled,
                           cfg->name.c_str(), button, relay, st->relay_on);
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showSockets();
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
                _c._io->println(F("Invalid socket id"));
                _c.printPrompt_();
                return true;
            }
            showSocket(id);
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
                _c._io->println(F("Invalid socket id"));
                _c.printPrompt_();
                return true;
            }
            if (!_sockets.setEnabled(id, enable))
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
                _c._io->println(F("Usage: name <id> <value>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String name = rest.substring(space + 1);
            name.trim();
            uint16_t id = 0;
            if (!parseId_(id_str, id))
            {
                _c._io->println(F("Invalid socket id"));
                _c.printPrompt_();
                return true;
            }
            if (!_sockets.setName(id, name))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("button ") || lower.startsWith("relay "))
        {
            const bool is_button = lower.startsWith("button ");
            String rest = cmd.substring(is_button ? 7 : 6);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: button|relay <id> <port|none>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String port_str = rest.substring(space + 1);
            port_str.trim();
            uint16_t id = 0;
            if (!parseId_(id_str, id))
            {
                _c._io->println(F("Invalid socket id"));
                _c.printPrompt_();
                return true;
            }
            uint8_t port = SocketController::kInvalidPort;
            if (!parsePort_(port_str, port))
            {
                _c._io->println(F("Invalid port"));
                _c.printPrompt_();
                return true;
            }
            const bool ok = is_button ? _sockets.setButtonPort(id, port) : _sockets.setRelayPort(id, port);
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
    SocketController &_sockets;

    static bool parseId_(const String &s, uint16_t &out)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
            if (s[i] < '0' || s[i] > '9')
                return false;
        const int v = s.toInt();
        if (v < 0 || v >= (int)SocketController::kSocketCount)
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
            out = SocketController::kInvalidPort;
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
};
