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

template <typename ConsoleT>
class CLIWifiT
{
public:
    explicit CLIWifiT(ConsoleT &console) : _c(console) {}

    void printHelpEnable() const
    {
        _c._io->println(F("    wifi restart    - restart Wi-Fi"));
    }

    void printHelpConfigLines() const
    {
        _c._io->println(F("    wifi                     - enter Wi-Fi context"));
        _c._io->println(F("    wifi ssid <value>        - set STA SSID"));
        _c._io->println(F("    wifi password <value>    - set STA password"));
        _c._io->println(F("    wifi ap on|off           - enable/disable AP"));
        _c._io->println(F("    wifi ap_ssid <value>     - set AP SSID"));
        _c._io->println(F("    wifi ap_password <value> - set AP password"));
        _c._io->println(F("    wifi restart             - restart Wi-Fi"));
    }

    void printHelpContextLines() const
    {
        _c._io->println(F("    ssid <value>        - set STA SSID"));
        _c._io->println(F("    password <value>    - set STA password"));
        _c._io->println(F("    ap on|off           - enable/disable AP"));
        _c._io->println(F("    ap_ssid <value>     - set AP SSID"));
        _c._io->println(F("    ap_password <value> - set AP password"));
        _c._io->println(F("    restart             - restart Wi-Fi"));
        _c._io->println(F("    show                - show Wi-Fi configuration"));
    }

    void printHelpTopic() const
    {
        _c._io->println(F("Wi-Fi commands:"));
        _c._io->println(F("  wifi                     - enter Wi-Fi context"));
        _c._io->println(F("  wifi ssid <value>        - set STA SSID"));
        _c._io->println(F("  wifi password <value>    - set STA password"));
        _c._io->println(F("  wifi ap on|off           - enable/disable AP"));
        _c._io->println(F("  wifi ap_ssid <value>     - set AP SSID"));
        _c._io->println(F("  wifi ap_password <value> - set AP password"));
        _c._io->println(F("  wifi restart             - restart Wi-Fi"));
    }

    bool handleEnable(const String &line) const
    {
        String cmd = line;
        cmd.toLowerCase();
        if (cmd == "wifi restart")
        {
            _c.cmdWifiRestart_();
            _c.printPrompt_();
            return true;
        }
        return false;
    }

    bool handleConfig(const String &line) const
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();
        if (!lower.startsWith("wifi "))
            return false;

        String rest = cmd.substring(5);
        rest.trim();
        String rest_lower = rest;
        rest_lower.toLowerCase();

        if (rest_lower.startsWith("ssid "))
        {
            String v = rest.substring(5);
            v.trim();
            _c._wifi.setSsid(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (rest_lower.startsWith("password "))
        {
            String v = rest.substring(9);
            v.trim();
            _c._wifi.setPassword(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (rest_lower.startsWith("ap "))
        {
            String v = rest.substring(3);
            v.trim();
            String vlow = v;
            vlow.toLowerCase();
            bool on = (vlow == "on" || vlow == "1" || vlow == "true");
            bool off = (vlow == "off" || vlow == "0" || vlow == "false");
            if (!on && !off)
            {
                _c._io->println(F("Invalid ap value"));
                _c.printPrompt_();
                return noteHandled_();
            }
            _c._wifi.setAp(on);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (rest_lower.startsWith("ap_ssid "))
        {
            String v = rest.substring(8);
            v.trim();
            _c._wifi.setApSsid(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (rest_lower.startsWith("ap_password "))
        {
            String v = rest.substring(12);
            v.trim();
            _c._wifi.setApPassword(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (rest_lower == "restart")
        {
            _c.cmdWifiRestart_();
            _c.printPrompt_();
            return true;
        }

        _c._io->println(F("Invalid wifi syntax"));
        _c.printPrompt_();
        return true;
    }

    bool handleContext(const String &line) const
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            _c.cmdShowWifi_();
            _c.printPrompt_();
            return true;
        }
        if (lower == "restart")
        {
            _c.cmdWifiRestart_();
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("ssid "))
        {
            String v = cmd.substring(5);
            v.trim();
            _c._wifi.setSsid(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("password "))
        {
            String v = cmd.substring(9);
            v.trim();
            _c._wifi.setPassword(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("ap "))
        {
            String v = cmd.substring(3);
            v.trim();
            String vlow = v;
            vlow.toLowerCase();
            bool on = (vlow == "on" || vlow == "1" || vlow == "true");
            bool off = (vlow == "off" || vlow == "0" || vlow == "false");
            if (!on && !off)
            {
                _c._io->println(F("Invalid ap value"));
                _c.printPrompt_();
                return noteHandled_();
            }
            _c._wifi.setAp(on);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("ap_ssid "))
        {
            String v = cmd.substring(8);
            v.trim();
            _c._wifi.setApSsid(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("ap_password "))
        {
            String v = cmd.substring(12);
            v.trim();
            _c._wifi.setApPassword(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        return false;
    }

private:
    bool noteHandled_() const { return true; }
    ConsoleT &_c;
};
