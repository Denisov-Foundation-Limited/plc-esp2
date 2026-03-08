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

#include "controllers/ring_controller.hpp"

template <typename ConsoleT>
class CLIRingT
{
public:
    CLIRingT(ConsoleT &console, RingController &ring)
        : _c(console), _ring(ring) {}

    void printHelpConfigLines()
    {
        _c._io->println(F("  Ring:"));
        _c._io->println(F("    ring                   - enter Ring context"));
    }

    void printHelpContextLines()
    {
        _c._io->println(F("  Ring:"));
        _c._io->println(F("    show                     - show ring config"));
        _c._io->println(F("    on                       - hold relay on"));
        _c._io->println(F("    off                      - release relay"));
        _c._io->println(F("    enable                   - enable ring"));
        _c._io->println(F("    disable                  - disable ring"));
        _c._io->println(F("    button <port|none>       - set button input"));
        _c._io->println(F("    relay <port|none>        - set relay output"));
    }

    void showRing()
    {
        const auto &cfg = _ring.config();
        const auto &st = _ring.state();
        _c._io->println(F("Ring:"));
        _c._io->print(F("  enabled: "));
        _c._io->println(cfg.enabled ? F("on") : F("off"));
        _c._io->print(F("  button: "));
        printPort_(cfg.button_port);
        _c._io->print(F("  relay: "));
        printPort_(cfg.relay_port);
        _c._io->print(F("  relay_on: "));
        _c._io->println(st.relay_on ? F("yes") : F("no"));
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showRing();
            _c.printPrompt_();
            return true;
        }
        if (lower == "on" || lower == "off")
        {
            const bool on = lower == "on";
            if (!_ring.setHoldRelayWithSource(on, RingController::Source::Cli))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower == "enable" || lower == "disable")
        {
            const bool enable = lower == "enable";
            if (!_ring.setControllerEnabled(enable))
                _c._io->println(F("No changes"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("button "))
        {
            String tail = cmd.substring(7);
            tail.trim();
            uint8_t port = RingController::kInvalidPort;
            if (!parsePort_(tail, port))
            {
                _c._io->println(F("Usage: button <port|none>"));
            }
            else if (!_ring.setButtonPort(port))
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
        if (lower.startsWith("relay "))
        {
            String tail = cmd.substring(6);
            tail.trim();
            uint8_t port = RingController::kInvalidPort;
            if (!parsePort_(tail, port))
            {
                _c._io->println(F("Usage: relay <port|none>"));
            }
            else if (!_ring.setRelayPort(port))
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
        return false;
    }

private:
    ConsoleT &_c;
    RingController &_ring;

    static bool parsePort_(const String &s, uint8_t &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "none" || t == "-")
        {
            out = RingController::kInvalidPort;
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

    void printPort_(uint8_t port)
    {
        if (port == RingController::kInvalidPort)
        {
            _c._io->println(F("none"));
            return;
        }
        _c._io->println(port);
    }
};
