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
#include "utils/configs_manager_iface.hpp"

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
            _c._io->println(F("  Stack:"));
            _c._io->println(F("    stack role <master|slave> - set device role"));
            _c._io->println(F("    stack master <host>       - set master host/IP"));
            _c._io->println(F("  Wi-Fi:"));
            _c._io->println(F("    wifi                     - enter Wi-Fi context"));
            _c._io->println(F("  Time:"));
            _c._io->println(F("    time                     - enter Time context"));
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
        if (lower == "time")
        {
            _c.enterConfigTime();
            return;
        }
        if (lower.startsWith("stack "))
        {
            handleStack_(cmd, lower);
            return;
        }
        if (handleAdminPassword_(cmd, lower))
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

    void handleTimeContext(const String &line)
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
            _c._io->println(F("Commands (config-time):"));
            _c._io->println(F("  Time:"));
            _c._io->println(F("    date <YYYY-MM-DD>       - set date"));
            _c._io->println(F("    time <HH:MM:SS>         - set time"));
            _c._io->println(F("    set <YYYY-MM-DD> <HH:MM:SS> - set date/time"));
            _c._io->println(F("    show                    - show RTC time"));
            _c._io->println(F("  Session:"));
            _c._io->println(F("    exit                    - return to config"));
            _c._io->println(F("    end                     - return to enable"));
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
        if (lower == "show")
        {
            _c.cmdShowTime_();
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("date "))
        {
            String val = cmd.substring(5);
            val.trim();
            uint16_t year = 0;
            uint8_t month = 0;
            uint8_t day = 0;
            if (!parseDate_(val, year, month, day))
            {
                _c._io->println(F("Invalid date"));
                _c.printPrompt_();
                return;
            }
            Ds3231Mz::DateTime dt{};
            if (!_c._rtc.Time(dt))
            {
                _c._io->println(F("RTC error"));
                _c.printPrompt_();
                return;
            }
            dt.year = year;
            dt.month = month;
            dt.day = day;
            dt.day_of_week = calcDow_(year, month, day);
            if (!_c._rtc.setTime(dt))
            {
                _c._io->println(F("RTC set failed"));
                _c.printPrompt_();
                return;
            }
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("time "))
        {
            String val = cmd.substring(5);
            val.trim();
            uint8_t hour = 0;
            uint8_t minute = 0;
            uint8_t second = 0;
            if (!parseTime_(val, hour, minute, second))
            {
                _c._io->println(F("Invalid time"));
                _c.printPrompt_();
                return;
            }
            Ds3231Mz::DateTime dt{};
            if (!_c._rtc.Time(dt))
            {
                _c._io->println(F("RTC error"));
                _c.printPrompt_();
                return;
            }
            dt.hour = hour;
            dt.minute = minute;
            dt.second = second;
            if (!_c._rtc.setTime(dt))
            {
                _c._io->println(F("RTC set failed"));
                _c.printPrompt_();
                return;
            }
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("set "))
        {
            String rest = cmd.substring(4);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space < 0)
            {
                _c._io->println(F("Invalid set syntax"));
                _c.printPrompt_();
                return;
            }
            String d = rest.substring(0, space);
            String t = rest.substring(space + 1);
            d.trim();
            t.trim();
            uint16_t year = 0;
            uint8_t month = 0;
            uint8_t day = 0;
            uint8_t hour = 0;
            uint8_t minute = 0;
            uint8_t second = 0;
            if (!parseDate_(d, year, month, day) || !parseTime_(t, hour, minute, second))
            {
                _c._io->println(F("Invalid set value"));
                _c.printPrompt_();
                return;
            }
            Ds3231Mz::DateTime dt{};
            dt.year = year;
            dt.month = month;
            dt.day = day;
            dt.day_of_week = calcDow_(year, month, day);
            dt.hour = hour;
            dt.minute = minute;
            dt.second = second;
            if (!_c._rtc.setTime(dt))
            {
                _c._io->println(F("RTC set failed"));
                _c.printPrompt_();
                return;
            }
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }

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

    void handleStack_(const String &cmd, const String &lower)
    {
        if (lower.startsWith("stack role "))
        {
            String role = cmd.substring(11);
            role.trim();
            role.toLowerCase();
            ConfigsManagerIface::StackRole r = ConfigsManagerIface::StackRole::Master;
            if (role == "slave")
                r = ConfigsManagerIface::StackRole::Slave;
            else if (role != "master")
            {
                _c._io->println(F("Invalid role"));
                _c.printPrompt_();
                return;
            }
            if (!_c.setStackRole_(r))
                _c._io->println(F("Config manager missing"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("stack master "))
        {
            String host = cmd.substring(13);
            host.trim();
            if (!_c.setStackMasterHost_(host))
                _c._io->println(F("Config manager missing"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    static bool parseUint_(const String &s, uint16_t &out)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
        {
            char c = s[i];
            if (c < '0' || c > '9')
                return false;
        }
        out = (uint16_t)s.toInt();
        return true;
    }

    static bool parseDate_(const String &s, uint16_t &year, uint8_t &month, uint8_t &day)
    {
        const int p1 = s.indexOf('-');
        const int p2 = (p1 >= 0) ? s.indexOf('-', p1 + 1) : -1;
        if (p1 <= 0 || p2 <= p1)
            return false;
        String ys = s.substring(0, p1);
        String ms = s.substring(p1 + 1, p2);
        String ds = s.substring(p2 + 1);
        uint16_t y = 0;
        uint16_t m = 0;
        uint16_t d = 0;
        if (!parseUint_(ys, y) || !parseUint_(ms, m) || !parseUint_(ds, d))
            return false;
        if (y < 2000 || y > 2099)
            return false;
        if (m < 1 || m > 12)
            return false;
        if (d < 1 || d > 31)
            return false;
        year = y;
        month = (uint8_t)m;
        day = (uint8_t)d;
        return true;
    }

    static bool parseTime_(const String &s, uint8_t &hour, uint8_t &minute, uint8_t &second)
    {
        const int p1 = s.indexOf(':');
        const int p2 = (p1 >= 0) ? s.indexOf(':', p1 + 1) : -1;
        if (p1 <= 0 || p2 <= p1)
            return false;
        String hs = s.substring(0, p1);
        String ms = s.substring(p1 + 1, p2);
        String ss = s.substring(p2 + 1);
        uint16_t h = 0;
        uint16_t m = 0;
        uint16_t sec = 0;
        if (!parseUint_(hs, h) || !parseUint_(ms, m) || !parseUint_(ss, sec))
            return false;
        if (h > 23 || m > 59 || sec > 59)
            return false;
        hour = (uint8_t)h;
        minute = (uint8_t)m;
        second = (uint8_t)sec;
        return true;
    }

    static uint8_t calcDow_(uint16_t y, uint8_t m, uint8_t d)
    {
        static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (m < 3)
            y -= 1;
        const uint8_t dow = (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
        return (uint8_t)(dow + 1);
    }

    ConsoleT &_c;
    CLIWifiT<ConsoleT> &_wifi;
    CLITgbotT<ConsoleT> &_tgbot;
};
