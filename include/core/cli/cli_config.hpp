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
#include "core/cli/modules/cli_tgbot.hpp"

template <typename ConsoleT>
class CLIConfigT
{
public:
    CLIConfigT(ConsoleT &console, CLIWifiT<ConsoleT> &wifi, CLITgbotT<ConsoleT> &tgbot)
        : _c(console), _wifi(wifi), _tgbot(tgbot) {}

    void handle(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower.startsWith("help "))
        {
            String topic = cmd.substring(5);
            topic.trim();
            _c.showHelpTopic_(topic);
            _c.printPrompt_();
            return;
        }
        if (lower == "help" || lower == "?")
        {
            _c._io->println(F("Commands (config):"));
            _c._io->println(F("  Admin:"));
            _c._io->println(F("    password <pass>         - set admin password"));
            _c._io->println(F("    admin password <pass>   - set admin password"));
            _c._io->println(F("  Wi-Fi:"));
            _wifi.printHelpConfigLines();
            _c._io->println(F("  Telegram:"));
            _tgbot.printHelpConfigLines();
            _c._io->println(F("  Session:"));
            _c._io->println(F("    exit                     - return to enable"));
            _c._io->println(F("    end                      - return to enable"));
            _c.printPrompt_();
            return;
        }
        if (lower == "exit" || lower == "end")
        {
            _c.enterEnable();
            return;
        }
        if (lower == "wifi")
        {
            _c.enterConfigWifi();
            return;
        }
        if (lower == "tgbot")
        {
            _c.enterConfigTgbot();
            return;
        }
        if (handleAdminPassword_(cmd, lower))
            return;
        if (_wifi.handleConfig(cmd))
            return;
        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleWifiContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower.startsWith("help "))
        {
            String topic = cmd.substring(5);
            topic.trim();
            _c.showHelpTopic_(topic);
            _c.printPrompt_();
            return;
        }
        if (lower == "help" || lower == "?")
        {
            _c._io->println(F("Commands (config-wifi):"));
            _c._io->println(F("  Wi-Fi:"));
            _wifi.printHelpContextLines();
            _c._io->println(F("  Session:"));
            _c._io->println(F("    exit                - return to config"));
            _c._io->println(F("    end                 - return to enable"));
            _c.printPrompt_();
            return;
        }
        if (lower == "exit")
        {
            _c.enterConfig();
            return;
        }
        if (lower == "end")
        {
            _c.enterEnable();
            return;
        }
        if (_wifi.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleTgbotContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower.startsWith("help "))
        {
            String topic = cmd.substring(5);
            topic.trim();
            _c.showHelpTopic_(topic);
            _c.printPrompt_();
            return;
        }
        if (lower == "help" || lower == "?")
        {
            _c._io->println(F("Commands (config-tgbot):"));
            _c._io->println(F("  Telegram:"));
            _tgbot.printHelpContextLines();
            _c._io->println(F("  Session:"));
            _c._io->println(F("    exit                - return to config"));
            _c._io->println(F("    end                 - return to enable"));
            _c.printPrompt_();
            return;
        }
        if (lower == "exit")
        {
            _c.enterConfig();
            return;
        }
        if (lower == "end")
        {
            _c.enterEnable();
            return;
        }
        if (_tgbot.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

private:
    bool handleAdminPassword_(const String &cmd, const String &lower)
    {
        if (lower.startsWith("password "))
        {
            String pass = cmd.substring(9);
            pass.trim();
            if (!_c.setAdminPassword_(pass))
                _c._io->println(F("Invalid password"));
            else
                _c._io->println(F("Admin password updated"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("admin password "))
        {
            String pass = cmd.substring(15);
            pass.trim();
            if (!_c.setAdminPassword_(pass))
                _c._io->println(F("Invalid password"));
            else
                _c._io->println(F("Admin password updated"));
            _c.printPrompt_();
            return true;
        }
        return false;
    }

    ConsoleT &_c;
    CLIWifiT<ConsoleT> &_wifi;
    CLITgbotT<ConsoleT> &_tgbot;
};
