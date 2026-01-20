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

#include "controllers/meteo_controller.hpp"

template <typename ConsoleT>
class CLIMeteoT
{
public:
    CLIMeteoT(ConsoleT &console, MeteoController &meteo)
        : _c(console), _meteo(meteo) {}

    void printHelpEnable()
    {
        _c._io->println(F("    show meteo       - list meteo sensors"));
        _c._io->print(F("    show meteo <id>"));
        printIdRangeInline_();
        _c._io->println(F(" - sensor details"));
    }

    void printHelpConfigLines()
    {
        _c._io->println(F("  Meteo:"));
        _c._io->println(F("    meteo                    - enter Meteo context"));
    }

    void printHelpContextLines()
    {
        _c._io->println(F("  Meteo:"));
        _c._io->println(F("    show                     - list meteo sensors"));
        _c._io->print(F("    show <id>"));
        printIdRangeInline_();
        _c._io->println(F("                - sensor details"));
        _c._io->print(F("    enable <id>"));
        printIdRangeInline_();
        _c._io->println(F("              - enable sensor"));
        _c._io->print(F("    disable <id>"));
        printIdRangeInline_();
        _c._io->println(F("             - disable sensor"));
        _c._io->print(F("    type <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <none|ds18b20|dht22> - set sensor type"));
        _c._io->print(F("    addr <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <hex|none>          - set DS18B20 address"));
        _c._io->print(F("    pin <id>"));
        printIdRangeInline_();
        _c._io->println(F(" <pin|none>          - set DHT22 pin"));
    }

    void showSensors()
    {
        bool any = false;
        _c.printMeteoHeader_();
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = _meteo.configByIndex(i);
            const auto *st = _meteo.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            any = true;
            printRow_(*cfg, *st);
        }
        if (!any)
            _c._io->println(F("  none"));
    }

    void showSensor(size_t id)
    {
        const auto *cfg = _meteo.config(id);
        const auto *st = _meteo.state(id);
        if (!cfg || !st)
        {
            printInvalidSensorId_();
            return;
        }
        _c._io->println(F("Meteo sensor:"));
        _c.printMeteoHeader_();
        printRow_(*cfg, *st);
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showSensors();
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("show "))
        {
            String tail = cmd.substring(5);
            tail.trim();
            uint16_t id = 0;
            if (!parseId_(tail, id))
            {
                printInvalidSensorId_();
                _c.printPrompt_();
                return true;
            }
            showSensor(id);
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("enable ") || lower.startsWith("disable "))
        {
            const bool enable = lower.startsWith("enable ");
            String tail = cmd.substring(enable ? 7 : 8);
            tail.trim();
            uint16_t id = 0;
            if (!parseId_(tail, id))
            {
                printInvalidSensorId_();
                _c.printPrompt_();
                return true;
            }
            if (!_meteo.setEnabled(id, enable))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("type "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: type <id> <none|ds18b20|dht22>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String type_str = rest.substring(space + 1);
            id_str.trim();
            type_str.trim();
            uint16_t id = 0;
            if (!parseId_(id_str, id))
            {
                printInvalidSensorId_();
                _c.printPrompt_();
                return true;
            }
            MeteoController::SensorType type = MeteoController::SensorType::None;
            if (!parseType_(type_str, type))
            {
                _c._io->println(F("Invalid type"));
                _c.printPrompt_();
                return true;
            }
            if (!_meteo.setType(id, type))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("addr "))
        {
            String rest = cmd.substring(5);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: addr <id> <hex|none>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String hex = rest.substring(space + 1);
            id_str.trim();
            hex.trim();
            uint16_t id = 0;
            if (!parseId_(id_str, id))
            {
                printInvalidSensorId_();
                _c.printPrompt_();
                return true;
            }
            bool clear = false;
            uint8_t addr[MeteoController::kAddrLen] = {};
            if (!parseAddr_(hex, addr, clear))
            {
                _c._io->println(F("Invalid address"));
                _c.printPrompt_();
                return true;
            }
            if (!_meteo.setDs18b20Addr(id, addr, !clear))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("pin "))
        {
            String rest = cmd.substring(4);
            rest.trim();
            const int space = rest.indexOf(' ');
            if (space <= 0)
            {
                _c._io->println(F("Usage: pin <id> <pin|none>"));
                _c.printPrompt_();
                return true;
            }
            String id_str = rest.substring(0, space);
            String pin_str = rest.substring(space + 1);
            id_str.trim();
            pin_str.trim();
            uint16_t id = 0;
            if (!parseId_(id_str, id))
            {
                printInvalidSensorId_();
                _c.printPrompt_();
                return true;
            }
            uint8_t pin = MeteoController::kInvalidPin;
            if (!parsePin_(pin_str, pin))
            {
                _c._io->println(F("Invalid pin"));
                _c.printPrompt_();
                return true;
            }
            if (!_meteo.setDht22Pin(id, pin))
                _c._io->println(F("Failed"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }

        return false;
    }

private:
    ConsoleT &_c;
    MeteoController &_meteo;

    static bool parseId_(const String &s, uint16_t &out)
    {
        if (s.length() == 0)
            return false;
        for (size_t i = 0; i < s.length(); ++i)
            if (s[i] < '0' || s[i] > '9')
                return false;
        const int v = s.toInt();
        if (v < 1 || v > (int)MeteoController::kSensorCount)
            return false;
        out = (uint16_t)v;
        return true;
    }

    static bool parseType_(const String &s, MeteoController::SensorType &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "ds18b20")
        {
            out = MeteoController::SensorType::Ds18b20;
            return true;
        }
        if (t == "dht22")
        {
            out = MeteoController::SensorType::Dht22;
            return true;
        }
        if (t == "none")
        {
            out = MeteoController::SensorType::None;
            return true;
        }
        return false;
    }

    static bool parseAddr_(const String &s, uint8_t out[MeteoController::kAddrLen], bool &clear)
    {
        String t = s;
        t.toLowerCase();
        if (t == "none" || t == "-")
        {
            clear = true;
            return true;
        }
        clear = false;
        return MeteoController::parseHexAddr(s.c_str(), out);
    }

    static bool parsePin_(const String &s, uint8_t &out)
    {
        String t = s;
        t.toLowerCase();
        if (t == "none" || t == "-")
        {
            out = MeteoController::kInvalidPin;
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

    void printRow_(const MeteoController::SensorConfig &cfg, const MeteoController::SensorState &st)
    {
        char temp_buf[10] = {};
        char hum_buf[10] = {};
        const char *temp = "--";
        const char *hum = "--";
        if (st.has_temp)
        {
            dtostrf(st.temp_c, 0, 2, temp_buf);
            temp = temp_buf;
        }
        if (st.has_humidity)
        {
            dtostrf(st.humidity, 0, 1, hum_buf);
            hum = hum_buf;
        }

        char info_buf[24] = {};
        const char *info = "-";
        if (cfg.type == MeteoController::SensorType::Dht22)
        {
            if (cfg.dht_pin != MeteoController::kInvalidPin)
            {
                snprintf(info_buf, sizeof(info_buf), "pin=%u", (unsigned)cfg.dht_pin);
                info = info_buf;
            }
        }
        else if (cfg.type == MeteoController::SensorType::Ds18b20)
        {
            if (cfg.ds18_addr_set)
            {
                char hex[17] = {};
                MeteoController::formatHexAddr(cfg.ds18_addr, hex);
                snprintf(info_buf, sizeof(info_buf), "%s", hex);
                info = info_buf;
            }
        }

        _c.printMeteoRow_("CPU", cfg.id, cfg.enabled, MeteoController::typeName(cfg.type), temp, hum, info);
    }

    void printIdRangeInline_() const
    {
        _c._io->print(F(" (1.."));
        _c._io->print(MeteoController::kSensorCount);
        _c._io->print(F(")"));
    }

    void printInvalidSensorId_() const
    {
        _c._io->print(F("Invalid sensor id (1.."));
        _c._io->print(MeteoController::kSensorCount);
        _c._io->println(F(")"));
    }
};
