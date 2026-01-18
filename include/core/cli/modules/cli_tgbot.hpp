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
#include "core/network/telegram/telegram_menu.hpp"

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
        _c._io->println(F("    allow list              - show allowed users"));
        _c._io->println(F("    allow add <username> [chat_id] [admin] [notify] [off] - add allowed user"));
        _c._io->println(F("    allow del <username>    - remove allowed user"));
        _c._io->println(F("    allow clear             - clear allowed list"));
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
        _c._io->println(F("  allow list              - show allowed users"));
        _c._io->println(F("  allow add <username> [chat_id] [admin] [notify] [off] - add allowed user"));
        _c._io->println(F("  allow del <username>    - remove allowed user"));
        _c._io->println(F("  allow clear             - clear allowed list"));
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
        if (lower == "allow list")
        {
            const auto &users = _c._tgbot_menu.allowedUsers();
            if (users.empty())
                _c._io->println(F("Allowed list empty"));
            else
            {
                _c._io->println(F("ID  Username           ChatID       Admin Notify Enabled"));
                _c._io->println(F("--  -----------------  -----------  ----- ------ -------"));
                for (size_t i = 0; i < users.size(); ++i)
                {
                    const auto &u = users[i];
                    _c._io->print(String((unsigned)(i + 1)));
                    _c._io->print(F("  "));
                    _c._io->print(u.username.length() ? u.username : String("-"));
                    _c._io->print(F("  "));
                    _c._io->print(u.chat_id ? String((long long)u.chat_id) : String("-"));
                    _c._io->print(F("  "));
                    _c._io->print(u.is_admin ? F("yes") : F("no"));
                    _c._io->print(F("   "));
                    _c._io->print(u.is_notify ? F("yes") : F("no"));
                    _c._io->print(F("   "));
                    _c._io->println(u.enabled ? F("yes") : F("no"));
                }
            }
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("allow add "))
        {
            String v = cmd.substring(10);
            v.trim();
            TelegramMenu::AllowedUser u{};
            if (!parseAllowedUser_(v, u))
            {
                _c._io->println(F("Invalid user spec"));
                _c.printPrompt_();
                return true;
            }
            auto res = _c._tgbot_menu.addAllowedUser(u);
            const uint8_t code = static_cast<uint8_t>(res);
            if (code == 0)
                _c._io->println(F("OK"));
            else if (code == 2)
                _c._io->println(F("Already exists"));
            else if (code == 3)
                _c._io->println(F("List full"));
            else
                _c._io->println(F("Invalid id"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("allow del "))
        {
            String v = cmd.substring(10);
            v.trim();
            if (_c._tgbot_menu.removeAllowedUser(v))
                _c._io->println(F("OK"));
            else
                _c._io->println(F("Not found"));
            _c.printPrompt_();
            return true;
        }
        if (lower == "allow clear")
        {
            _c._tgbot_menu.clearAllowedUsers();
            _c._io->println(F("OK"));
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

    static bool parseAllowedUser_(const String &input, TelegramMenu::AllowedUser &out)
    {
        out = TelegramMenu::AllowedUser{};
        String s = input;
        s.trim();
        if (s.length() == 0)
            return false;
        int start = 0;
        int part = 0;
        while (start < (int)s.length())
        {
            int space = s.indexOf(' ', start);
            if (space < 0)
                space = s.length();
            String token = s.substring(start, (size_t)space);
            token.trim();
            if (token.length())
            {
                if (part == 0)
                {
                    out.username = token;
                }
                else if (token == "admin")
                {
                    out.is_admin = true;
                }
                else if (token == "notify")
                {
                    out.is_notify = true;
                }
                else
                {
                    const char *c = token.c_str();
                    bool numeric = true;
                    for (size_t i = 0; c[i]; ++i)
                    {
                        if (c[i] < '0' || c[i] > '9')
                        {
                            numeric = false;
                            break;
                        }
                    }
                    if (numeric)
                        out.chat_id = (int64_t)strtoll(c, nullptr, 10);
                }
            }
            start = space + 1;
            ++part;
        }
        return out.username.length() > 0;
    }
};
