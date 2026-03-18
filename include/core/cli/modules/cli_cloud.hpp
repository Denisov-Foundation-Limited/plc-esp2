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
class CLICloudT
{
public:
    explicit CLICloudT(ConsoleT &console) : _c(console) {}

    void printHelpConfigLines() const
    {
        _c._io->println(F("    cloud                  - enter Cloud context"));
    }

    void printHelpContextLines() const
    {
        _c._io->println(F("    enable on|off           - enable/disable Cloud"));
        _c._io->println(F("    transport ws|http       - set transport"));
        _c._io->println(F("    host <value>            - set host"));
        _c._io->println(F("    port <num>              - set port"));
        _c._io->println(F("    path <value>            - set path"));
        _c._io->println(F("    ssl on|off              - enable SSL"));
        _c._io->println(F("    reconnect <ms>          - set reconnect interval"));
        _c._io->println(F("    event <ms>              - set event interval"));
        _c._io->println(F("    api_key <value|clear>   - set/clear api_key"));
        _c._io->println(F("    show                    - show settings"));
    }

    void printHelpTopic() const
    {
        _c._io->println(F("Cloud commands:"));
        _c._io->println(F("  cloud                  - enter Cloud context"));
        printHelpContextLines();
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            _c.cmdShowCloud_();
            _c.printPrompt_();
            return true;
        }

        if (!_c._configs_manager)
        {
            _c._io->println(F("Config manager missing"));
            _c.printPrompt_();
            return true;
        }

        if (lower.startsWith("enable "))
        {
            String v = cmd.substring(7);
            v.trim();
            String vlow = v;
            vlow.toLowerCase();
            bool on = (vlow == "on" || vlow == "1" || vlow == "true");
            bool off = (vlow == "off" || vlow == "0" || vlow == "false");
            if (!on && !off)
            {
                _c._io->println(F("Invalid enable value"));
                _c.printPrompt_();
                return true;
            }
            _c._configs_manager->setCloudEnabled(on);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (lower.startsWith("host "))
        {
            String v = cmd.substring(5);
            v.trim();
            _c._configs_manager->setCloudHost(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (lower.startsWith("transport "))
        {
            String v = cmd.substring(10);
            v.trim();
            v.toLowerCase();
            if (v == "ws" || v == "websocket")
                _c._configs_manager->setCloudTransport(CloudTransportKind::WebSocket);
            else if (v == "http")
                _c._configs_manager->setCloudTransport(CloudTransportKind::Http);
            else
            {
                _c._io->println(F("Invalid transport value"));
                _c.printPrompt_();
                return true;
            }
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (lower.startsWith("port "))
        {
            String v = cmd.substring(5);
            v.trim();
            uint16_t port = (uint16_t)strtoul(v.c_str(), nullptr, 10);
            _c._configs_manager->setCloudPort(port);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (lower.startsWith("path "))
        {
            String v = cmd.substring(5);
            v.trim();
            if (v.length() == 0)
                v = "/";
            _c._configs_manager->setCloudPath(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (lower.startsWith("ssl "))
        {
            String v = cmd.substring(4);
            v.trim();
            String vlow = v;
            vlow.toLowerCase();
            bool on = (vlow == "on" || vlow == "1" || vlow == "true");
            bool off = (vlow == "off" || vlow == "0" || vlow == "false");
            if (!on && !off)
            {
                _c._io->println(F("Invalid ssl value"));
                _c.printPrompt_();
                return true;
            }
            _c._configs_manager->setCloudUseSsl(on);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (lower.startsWith("reconnect "))
        {
            String v = cmd.substring(10);
            v.trim();
            uint32_t ms = (uint32_t)strtoul(v.c_str(), nullptr, 10);
            _c._configs_manager->setCloudReconnectMs(ms);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (lower.startsWith("event "))
        {
            String v = cmd.substring(6);
            v.trim();
            uint32_t ms = (uint32_t)strtoul(v.c_str(), nullptr, 10);
            _c._configs_manager->setCloudEventIntervalMs(ms);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        if (lower.startsWith("api_key "))
        {
            String v = cmd.substring(8);
            v.trim();
            if (v == "clear")
                v = "";
            _c._configs_manager->setCloudApiKey(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        return false;
    }

private:
    ConsoleT &_c;
};
