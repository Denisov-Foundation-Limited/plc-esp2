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
            _c._io->println(F("    show board      - board profile name"));
            _c._io->println(F("    show wifi       - Wi-Fi configuration"));
            _c._io->println(F("    show time       - RTC date/time"));
            _c._io->println(F("    show i2c        - I2C device list"));
            _c._io->println(F("    show ow         - OneWire device list"));
            _c._io->println(F("    show stack      - stack role settings"));
            _c._io->println(F("    show telegram   - Telegram settings"));
            _c._io->println(F("    show config     - configuration file contents"));
            _c._io->println(F("    show port <id>  - port details"));
            _c._io->println(F("    show ports      - list ports"));
            _c._io->println(F("    show sockets    - list sockets"));
            _c._io->println(F("    show socket <id> - socket details"));
            _c._io->println(F("    socket toggle <id> - toggle socket relay"));
            _c._io->println(F("    socket on <id>     - relay ON"));
            _c._io->println(F("    socket off <id>    - relay OFF"));
            _c._io->println(F("  Actions:"));
            _c._io->println(F("    ftest           - start functional test task"));
            _c._io->println(F("    copy tftp://<ip>/firmware.bin firmware - update firmware"));
            _c._io->println(F("    copy http://<ip>/firmware.bin firmware - update firmware"));
            _c._io->println(F("    stack nodes     - list stack nodes"));
            _c._io->println(F("    stack send <id> <get|set> <json> - send stack command"));
            _c._io->println(F("    stack socket <unit> <on|off|toggle> <id> - control socket"));
            _wifi.printHelpEnable();
            _c._io->println(F("    reload          - restart controller"));
            _c._io->println(F("    reset           - restart controller"));
            _c._io->println(F("    write           - save configuration"));
            _c._io->println(F("    erase           - delete configuration"));
            _c._io->println(F("    ext scan        - rescan extenders"));
            _c._io->println(F("    show ext        - list extenders"));
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
        if (startsWith_(cmd, "socket toggle "))
        {
            String tail = cmd.substring(14);
            tail.trim();
            uint16_t id = 0;
            if (!_c.parseUint_(tail, id))
            {
                _c._io->println(F("Usage: socket toggle <id>"));
                _c.printPrompt_();
                return;
            }
            if (!_c._controllers.sockets().toggleRelayById((uint8_t)id))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (startsWith_(cmd, "socket on ") || startsWith_(cmd, "socket off "))
        {
            const bool on = startsWith_(cmd, "socket on ");
            String tail = cmd.substring(on ? 10 : 11);
            tail.trim();
            uint16_t id = 0;
            if (!_c.parseUint_(tail, id))
            {
                _c._io->println(F("Usage: socket on|off <id>"));
                _c.printPrompt_();
                return;
            }
            if (!_c._controllers.sockets().setRelayById((uint8_t)id, on))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (startsWith_(cmd, "stack "))
        {
            _c.cmdStack_(cmd);
            _c.printPrompt_();
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
        if (startsWith_(cmd, "copy "))
        {
            _c.cmdCopy_(cmd);
            _c.printPrompt_();
            return;
        }
        if (eq_(cmd, "write"))
        {
            _c.cmdWriteConfig_();
            _c.printPrompt_();
            return;
        }
        if (eq_(cmd, "ext scan"))
        {
            _c.cmdExtScan_();
            _c.printPrompt_();
            return;
        }
        if (eq_(cmd, "show ext"))
        {
            _c.cmdExtList_();
            _c.printPrompt_();
            return;
        }
        if (eq_(cmd, "erase"))
        {
            _c.cmdEraseConfig_();
            _c.printPrompt_();
            return;
        }
        _c._io->println(F("\tUnknown command"));
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
