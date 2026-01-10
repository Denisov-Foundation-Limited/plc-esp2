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
#include <stdlib.h>
#include <vector>

#include "core/network/telegram/telegram.hpp"

template <typename ConsoleT>
class CLITgbotT
{
public:
    explicit CLITgbotT(ConsoleT &console) : _c(console) {}

    void printHelpConfigLines() const
    {
        _c._io->println(F("    tgbot                   - enter Telegram context"));
    }

    void printHelpContextLines() const
    {
        _c._io->println(F("    token <value>           - set bot token"));
        _c._io->println(F("    chat <id>               - set chat id"));
        _c._io->println(F("    insecure on|off         - TLS check"));
        _c._io->println(F("    send <text>             - send message"));
        _c._io->println(F("    poll                    - poll commands"));
        _c._io->println(F("    show                    - show settings"));
    }

    void printHelpTopic() const
    {
        _c._io->println(F("Telegram commands:"));
        _c._io->println(F("  tgbot                   - enter Telegram context"));
        _c._io->println(F("  token <value>           - set bot token"));
        _c._io->println(F("  chat <id>               - set chat id"));
        _c._io->println(F("  insecure on|off         - TLS check"));
        _c._io->println(F("  send <text>             - send message"));
        _c._io->println(F("  poll                    - poll commands"));
        _c._io->println(F("  show                    - show settings"));
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            _c.cmdShowTelegram_();
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("token "))
        {
            String v = cmd.substring(6);
            v.trim();
            _c._tgbot.setToken(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("chat "))
        {
            String v = cmd.substring(5);
            v.trim();
            int64_t chat_id = (int64_t)strtoll(v.c_str(), nullptr, 10);
            _c._tgbot.setChatId(chat_id);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("insecure "))
        {
            String v = cmd.substring(9);
            v.trim();
            String vlow = v;
            vlow.toLowerCase();
            bool on = (vlow == "on" || vlow == "1" || vlow == "true");
            bool off = (vlow == "off" || vlow == "0" || vlow == "false");
            if (!on && !off)
            {
                _c._io->println(F("Invalid insecure value"));
                _c.printPrompt_();
                return true;
            }
            _c._tgbot.setInsecure(on);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("send "))
        {
            String v = cmd.substring(5);
            v.trim();
            if (_c._tgbot.sendMessage(v))
                _c._io->println(F("OK"));
            else
                _c._io->println(_c._tgbot.lastError());
            _c.printPrompt_();
            return true;
        }
        if (lower == "poll")
        {
            std::vector<TelegramClient::Update> updates;
            if (_c._tgbot.takePollUpdates(updates))
            {
                if (updates.empty())
                {
                    _c._io->println(F("No commands"));
                }
                else
                {
                    for (const auto &u : updates)
                    {
                        _c._io->print(F("chat "));
                        _c._io->print((long long)u.chat_id);
                        _c._io->print(F(": "));
                        _c._io->println(u.text);
                        if (u.update_id > _c._tgbot_last_update_id)
                            _c._tgbot_last_update_id = u.update_id;
                    }
                }
                _c.printPrompt_();
                return true;
            }

            if (!_c._tgbot.isPolling())
            {
                if (_c._tgbot.startLongPoll(20, _c._tgbot_last_update_id + 1))
                    _c._io->println(F("Polling started"));
                else
                    _c._io->println(_c._tgbot.lastError());
                _c.printPrompt_();
                return true;
            }

            _c._io->println(F("Polling..."));
            _c.printPrompt_();
            return true;
        }
        return false;
    }

private:
    ConsoleT &_c;
};
