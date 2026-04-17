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
#include "core/cli/modules/cli_socket.hpp"
#include "core/cli/modules/cli_meteo.hpp"
#include "core/cli/modules/cli_thermo.hpp"
#include "core/cli/modules/cli_tank.hpp"
#include "core/cli/modules/cli_septic.hpp"
#include "core/cli/modules/cli_security.hpp"
#include "core/cli/modules/cli_ring.hpp"
#include "core/cli/modules/cli_avr.hpp"
#include "core/cli/modules/cli_leak.hpp"
#include "core/cli/modules/cli_watering.hpp"
#include "core/cli/modules/cli_cloud.hpp"
#include "utils/configs_manager_iface.hpp"

template <typename ConsoleT>
class CLIConfigT
{
public:
    CLIConfigT(ConsoleT &console, CLIWifiT<ConsoleT> &wifi,
               CLISocketT<ConsoleT> &socket, CLIMeteoT<ConsoleT> &meteo, CLIThermoT<ConsoleT> &thermo,
               CLITankT<ConsoleT> &tank, CLISepticT<ConsoleT> &septic, CLISecurityT<ConsoleT> &security,
               CLIRingT<ConsoleT> &ring, CLIAvrT<ConsoleT> &avr, CLILeakT<ConsoleT> &leak,
               CLIWateringT<ConsoleT> &watering, CLICloudT<ConsoleT> &cloud)
        : _c(console),
          _wifi(wifi),
          _socket(socket),
          _meteo(meteo),
          _thermo(thermo),
          _tank(tank),
          _septic(septic),
          _security(security),
          _ring(ring),
          _avr(avr),
          _leak(leak),
          _watering(watering),
          _cloud(cloud)
    {
    }

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
            _c._io->println(F("  EEPROM:"));
            _c._io->println(F("    eeprom show             - show EEPROM save/load flags"));
            _c._io->println(F("    eeprom save <on|off>    - enable/disable EEPROM periodic save"));
            _c._io->println(F("    eeprom load <on|off>    - enable/disable EEPROM load on boot"));
            _c._io->println(F("  Wi-Fi:"));
            _c._io->println(F("    wifi                     - enter Wi-Fi context"));
            _c._io->println(F("  Time:"));
            _c._io->println(F("    time                     - enter Time context"));
            _c._io->println(F("  Cloud:"));
            _cloud.printHelpConfigLines();
            _socket.printHelpConfigLines();
            _meteo.printHelpConfigLines();
            _thermo.printHelpConfigLines();
            _tank.printHelpConfigLines();
            _septic.printHelpConfigLines();
            _security.printHelpConfigLines();
            _ring.printHelpConfigLines();
            _avr.printHelpConfigLines();
            _leak.printHelpConfigLines();
            _watering.printHelpConfigLines();
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
        if (lower == "time")
        {
            _c.enterConfigTime();
            return;
        }
        if (lower == "socket")
        {
            _c.enterConfigSocket();
            return;
        }
        if (lower == "meteo")
        {
            _c.enterConfigMeteo();
            return;
        }
        if (lower == "thermo")
        {
            _c.enterConfigThermo();
            return;
        }
        if (lower == "tank")
        {
            _c.enterConfigTank();
            return;
        }
        if (lower == "septic")
        {
            _c.enterConfigSeptic();
            return;
        }
        if (lower == "security")
        {
            _c.enterConfigSecurity();
            return;
        }
        if (lower == "ring")
        {
            _c.enterConfigRing();
            return;
        }
        if (lower == "avr")
        {
            _c.enterConfigAvr();
            return;
        }
        if (lower == "leak")
        {
            _c.enterConfigLeak();
            return;
        }
        if (lower == "watering")
        {
            _c.enterConfigWatering();
            return;
        }
        if (lower == "cloud")
        {
            _c.enterConfigCloud();
            return;
        }
        if (lower.startsWith("stack "))
        {
            handleStack_(cmd, lower);
            return;
        }
        if (lower.startsWith("eeprom "))
        {
            handleEeprom_(cmd, lower);
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

    void handleSocketContext(const String &line)
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
            _c._io->println(F("Commands (config-socket):"));
            _socket.printHelpContextLines();
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
        if (_socket.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleMeteoContext(const String &line)
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
            _c._io->println(F("Commands (config-meteo):"));
            _meteo.printHelpContextLines();
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
        if (_meteo.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleThermoContext(const String &line)
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
            _c._io->println(F("Commands (config-thermo):"));
            _thermo.printHelpContextLines();
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
        if (_thermo.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleTankContext(const String &line)
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
            _c._io->println(F("Commands (config-tank):"));
            _tank.printHelpContextLines();
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
        if (_tank.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleSepticContext(const String &line)
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
            _c._io->println(F("Commands (config-septic):"));
            _septic.printHelpContextLines();
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
        if (_septic.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleSecurityContext(const String &line)
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
            _c._io->println(F("Commands (config-security):"));
            _security.printHelpContextLines();
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
        if (_security.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleRingContext(const String &line)
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
            _c._io->println(F("Commands (config-ring):"));
            _ring.printHelpContextLines();
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
        if (_ring.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleWateringContext(const String &line)
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
            _c._io->println(F("Commands (config-watering):"));
            _watering.printHelpContextLines();
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
        if (_watering.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleAvrContext(const String &line)
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
            _c._io->println(F("Commands (config-avr):"));
            _avr.printHelpContextLines();
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
        if (_avr.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleCloudContext(const String &line)
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
            _c._io->println(F("Commands (config-cloud):"));
            _c._io->println(F("  Cloud:"));
            _cloud.printHelpContextLines();
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
        if (_cloud.handleContext(cmd))
            return;

        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
    }

    void handleLeakContext(const String &line)
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
            _c._io->println(F("Commands (config-leak):"));
            _leak.printHelpContextLines();
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
        if (_leak.handleContext(cmd))
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

    void handleEeprom_(const String &cmd, const String &lower)
    {
        if (!_c._configs_manager)
        {
            _c._io->println(F("Config manager missing"));
            _c.printPrompt_();
            return;
        }
        if (lower == "eeprom show")
        {
            _c._io->println(F("EEPROM:"));
            _c.printKeyValue_(F("save"), _c._configs_manager->eepromSaveEnabled() ? F("true") : F("false"), 4);
            _c.printKeyValue_(F("load"), _c._configs_manager->eepromLoadEnabled() ? F("true") : F("false"), 4);
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("eeprom save "))
        {
            const String value = cmd.substring(12);
            bool enabled = false;
            if (!parseOnOff_(value, enabled))
            {
                _c._io->println(F("Invalid save value"));
                _c.printPrompt_();
                return;
            }
            _c._configs_manager->setEepromSaveEnabled(enabled);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("eeprom load "))
        {
            const String value = cmd.substring(12);
            bool enabled = false;
            if (!parseOnOff_(value, enabled))
            {
                _c._io->println(F("Invalid load value"));
                _c.printPrompt_();
                return;
            }
            _c._configs_manager->setEepromLoadEnabled(enabled);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        _c._io->println(F("Unknown command"));
        _c.printPrompt_();
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
        if (lower.startsWith("stack policy "))
        {
            String value = cmd.substring(13);
            value.trim();
            value.toLowerCase();
            ConfigsManagerIface::StackExchangePolicy policy = ConfigsManagerIface::StackExchangePolicy::Direct;
            if (value == "direct")
                policy = ConfigsManagerIface::StackExchangePolicy::Direct;
            else if (value == "poll")
                policy = ConfigsManagerIface::StackExchangePolicy::Poll;
            else
            {
                _c._io->println(F("Invalid policy"));
                _c.printPrompt_();
                return;
            }
            if (!_c.setStackExchangePolicy_(policy))
                _c._io->println(F("Config manager missing"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("stack transport "))
        {
            String value = cmd.substring(16);
            value.trim();
            value.toLowerCase();
            ConfigsManagerIface::StackTransportKind kind = ConfigsManagerIface::StackTransportKind::WebSocket;
            if (value == "rs485")
                kind = ConfigsManagerIface::StackTransportKind::Rs485;
            else if (value != "websocket" && value != "ws")
            {
                _c._io->println(F("Invalid transport"));
                _c.printPrompt_();
                return;
            }
            if (!_c.setStackTransport_(kind))
                _c._io->println(F("Config manager missing"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("stack payload "))
        {
            String value = cmd.substring(14);
            value.trim();
            value.toLowerCase();
            ConfigsManagerIface::StackPayloadMode mode = ConfigsManagerIface::StackPayloadMode::Json;
            if (value == "binary" || value == "bin")
                mode = ConfigsManagerIface::StackPayloadMode::Binary;
            else if (value != "json")
            {
                _c._io->println(F("Invalid payload"));
                _c.printPrompt_();
                return;
            }
            if (!_c.setStackPayloadMode_(mode))
                _c._io->println(F("Config manager missing"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("stack fallback "))
        {
            String value = cmd.substring(15);
            value.trim();
            value.toLowerCase();
            bool enabled = false;
            if (value == "on" || value == "1" || value == "true" || value == "yes")
                enabled = true;
            else if (value == "off" || value == "0" || value == "false" || value == "no")
                enabled = false;
            else
            {
                _c._io->println(F("Invalid fallback value"));
                _c.printPrompt_();
                return;
            }
            if (!_c.setStackFallbackEnabled_(enabled))
                _c._io->println(F("Config manager missing"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("stack fallback_host "))
        {
            String host = cmd.substring(20);
            host.trim();
            if (!_c.setStackFallbackHost_(host))
                _c._io->println(F("Config manager missing"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("stack slave_controller "))
        {
            String value = cmd.substring(23);
            value.trim();
            value.toLowerCase();
            bool enabled = false;
            if (value == "on" || value == "1" || value == "true" || value == "yes")
                enabled = true;
            else if (value == "off" || value == "0" || value == "false" || value == "no")
                enabled = false;
            else
            {
                _c._io->println(F("Invalid slave_controller value"));
                _c.printPrompt_();
                return;
            }
            if (!_c.setStackSlaveController_(enabled))
                _c._io->println(F("Config manager missing"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return;
        }
        if (lower.startsWith("stack api_key "))
        {
            String key = cmd.substring(14);
            key.trim();
            if (key == "clear")
                key = "";
            if (key == "gen")
            {
                key = genApiKey_();
            }
            if (!_c.setStackApiKey_(key))
                _c._io->println(F("Config manager missing"));
            else
            {
                if (key.length() > 0)
                {
                    _c._io->print(F("API key: "));
                    _c._io->println(key);
                }
                _c._io->println(F("OK"));
            }
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

    static String genApiKey_()
    {
        char buf[33] = {};
        static const char kHex[] = "0123456789abcdef";
        for (size_t i = 0; i < 16; ++i)
        {
            const uint8_t v = (uint8_t)random(0, 256);
            buf[i * 2] = kHex[(v >> 4) & 0x0F];
            buf[i * 2 + 1] = kHex[v & 0x0F];
        }
        buf[32] = '\0';
        return String(buf);
    }

    static bool parseOnOff_(String value, bool &out)
    {
        value.trim();
        value.toLowerCase();
        if (value == "on" || value == "1" || value == "true" || value == "yes")
        {
            out = true;
            return true;
        }
        if (value == "off" || value == "0" || value == "false" || value == "no")
        {
            out = false;
            return true;
        }
        return false;
    }

    ConsoleT &_c;
    CLIWifiT<ConsoleT> &_wifi;
    CLISocketT<ConsoleT> &_socket;
    CLIMeteoT<ConsoleT> &_meteo;
    CLIThermoT<ConsoleT> &_thermo;
    CLITankT<ConsoleT> &_tank;
    CLISepticT<ConsoleT> &_septic;
    CLISecurityT<ConsoleT> &_security;
    CLIRingT<ConsoleT> &_ring;
    CLIAvrT<ConsoleT> &_avr;
    CLILeakT<ConsoleT> &_leak;
    CLIWateringT<ConsoleT> &_watering;
    CLICloudT<ConsoleT> &_cloud;
};
