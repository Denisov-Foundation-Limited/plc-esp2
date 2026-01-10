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

#include "core/cli/modules/cli_wifi.hpp"

template <typename ConsoleT>
class CLIEnableT
{
public:
    CLIEnableT(ConsoleT &console, CLIWifiT<ConsoleT> &wifi) : _c(console), _wifi(wifi) {}

    void handle(const String &line)
    {
        const String cmd = line;
        if (startsWith_(cmd, "help "))
        {
            String topic = cmd.substring(5);
            topic.trim();
            _c.showHelpTopic_(topic);
            _c.printPrompt_();
            return;
        }
        if (eq_(cmd, "help") || eq_(cmd, "?"))
        {
            _c._io->println(F("Commands (enable):"));
            _c._io->println(F("  Show:"));
            _c._io->println(F("    show plc        - fan state and board temperature"));
            _c._io->println(F("    show wifi       - Wi-Fi configuration"));
            _c._io->println(F("    show time       - RTC date/time"));
            _c._io->println(F("    show i2c        - I2C device list"));
            _c._io->println(F("    show telegram   - Telegram settings"));
            _c._io->println(F("  Actions:"));
            _c._io->println(F("    ftest           - start functional test task"));
            _wifi.printHelpEnable();
            _c._io->println(F("    reload          - restart controller"));
            _c._io->println(F("    reset           - restart controller"));
            _c._io->println(F("  Config:"));
            _c._io->println(F("    configure terminal | conf t - enter config mode"));
            _c._io->println(F("  Session:"));
            _c._io->println(F("    disable         - end session"));
            _c._io->println(F("    logout          - end session"));
            _c._io->println(F("    exit            - end session"));
            _c.printPrompt_();
            return;
        }
        if (eq_(cmd, "disable") || eq_(cmd, "exit") || eq_(cmd, "logout"))
        {
            _c.logout();
            return;
        }
        if (startsWith_(cmd, "configure terminal") || startsWith_(cmd, "conf t"))
        {
            _c.enterConfig();
            return;
        }
        if (startsWith_(cmd, "show "))
        {
            _c.handleShow_(cmd.substring(5));
            return;
        }
        if (_wifi.handleEnable(cmd))
        {
            return;
        }
        if (eq_(cmd, "reload") || eq_(cmd, "reset"))
        {
            _c.cmdRestart_();
            return;
        }
        if (eq_(cmd, "ftest"))
        {
            _c.cmdFtest_();
            _c.printPrompt_();
            return;
        }
        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

private:
    static bool eq_(const String &a, const char *b)
    {
        String t = a;
        t.toLowerCase();
        return t == b;
    }

    static bool startsWith_(const String &a, const char *b)
    {
        String t = a;
        t.toLowerCase();
        return t.startsWith(b);
    }

    ConsoleT &_c;
    CLIWifiT<ConsoleT> &_wifi;
};
