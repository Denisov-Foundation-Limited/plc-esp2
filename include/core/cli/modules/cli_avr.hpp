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
#include <ArduinoJson.h>
#include <string.h>

#include "controllers/avr_controller.hpp"

template <typename ConsoleT>
class CLIAvrT
{
public:
    CLIAvrT(ConsoleT &console, AvrController &avr) : _c(console), _avr(avr) {}

    void printHelpConfigLines() const
    {
        _c._io->println(F("  AVR:"));
        _c._io->println(F("    avr                    - enter AVR context"));
    }

    void printHelpContextLines() const
    {
        _c._io->println(F("  AVR:"));
        _c._io->println(F("    show                     - show AVR config/state"));
        _c._io->println(F("    enable|disable           - enable/disable AVR"));
        _c._io->println(F("    mode <auto|manual>       - set mode"));
        _c._io->println(F("    source <off|main|reserve> - manual source"));
        _c._io->println(F("    prefer_main <on|off>     - source priority"));
        _c._io->println(F("    auto_return <on|off>     - return to main"));
        _c._io->println(F("    main_ok <port|none>      - set main input"));
        _c._io->println(F("    reserve_ok <port|none>   - set reserve input"));
        _c._io->println(F("    relay_main <port|none>   - set main relay"));
        _c._io->println(F("    relay_reserve <port|none> - set reserve relay"));
        _c._io->println(F("    fb_main <port|none>      - main feedback"));
        _c._io->println(F("    fb_reserve <port|none>   - reserve feedback"));
        _c._io->println(F("    debounce|loss_delay|return_delay|break|warmup|timeout <ms>"));
        _c._io->println(F("    main_ok_al|reserve_ok_al <on|off>"));
        _c._io->println(F("    fb_main_al|fb_reserve_al <on|off>"));
        _c._io->println(F("    relay_main_inv|relay_reserve_inv <on|off>"));
        _c._io->println(F("    clear_fault              - clear fault"));
    }

    void printHelpTopic() const
    {
        _c._io->println(F("AVR commands:"));
        _c._io->println(F("  avr                    - enter AVR context"));
        printHelpContextLines();
    }

    void showAvr() const
    {
        auto guard = _avr.lockGuard();
        const auto &cfg = _avr.config();
        const auto &st = _avr.state();
        _c._io->println(F("AVR:"));
        printBoolLine_("  enabled", cfg.enabled);
        printBoolLine_("  auto_mode", cfg.auto_mode);
        printBoolLine_("  prefer_main", cfg.prefer_main);
        printBoolLine_("  auto_return_main", cfg.auto_return_main);
        printSourceLine_("  active_source", st.active_source);
        printSourceLine_("  target_source", st.target_source);
        printSourceLine_("  manual_source", st.manual_source);
        _c._io->print(F("  fault: "));
        _c._io->println(AvrController::faultName(st.fault));
        printBoolLine_("  transfer", st.transfer_in_progress);
        printBoolLine_("  main_ok", st.main_ok);
        printBoolLine_("  reserve_ok", st.reserve_ok);
        printBoolLine_("  fb_main_on", st.fb_main_on);
        printBoolLine_("  fb_reserve_on", st.fb_reserve_on);
        printBoolLine_("  relay_main_on", st.relay_main_on);
        printBoolLine_("  relay_reserve_on", st.relay_reserve_on);
        printPortLine_("  main_ok_port", cfg.main_ok_port);
        printPortLine_("  reserve_ok_port", cfg.reserve_ok_port);
        printPortLine_("  relay_main_port", cfg.relay_main_port);
        printPortLine_("  relay_reserve_port", cfg.relay_reserve_port);
        printPortLine_("  feedback_main_port", cfg.feedback_main_port);
        printPortLine_("  feedback_reserve_port", cfg.feedback_reserve_port);
        printBoolLine_("  main_ok_active_low", cfg.main_ok_active_low);
        printBoolLine_("  reserve_ok_active_low", cfg.reserve_ok_active_low);
        printBoolLine_("  feedback_main_active_low", cfg.feedback_main_active_low);
        printBoolLine_("  feedback_reserve_active_low", cfg.feedback_reserve_active_low);
        printBoolLine_("  relay_main_invert", cfg.relay_main_invert);
        printBoolLine_("  relay_reserve_invert", cfg.relay_reserve_invert);
        printU32Line_("  debounce_ms", cfg.debounce_ms);
        printU32Line_("  loss_delay_ms", cfg.loss_delay_ms);
        printU32Line_("  return_delay_ms", cfg.return_delay_ms);
        printU32Line_("  break_ms", cfg.break_ms);
        printU32Line_("  warmup_ms", cfg.warmup_ms);
        printU32Line_("  transfer_timeout_ms", cfg.transfer_timeout_ms);
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showAvr();
            _c.printPrompt_();
            return true;
        }
        if (lower == "enable" || lower == "disable")
        {
            const bool enable = lower == "enable";
            if (!_avr.setControllerEnabled(enable))
                _c._io->println(F("No changes"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower == "clear_fault")
        {
            _avr.clearFault();
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("mode "))
        {
            String val = cmd.substring(5);
            val.trim();
            val.toLowerCase();
            if (val == "auto")
            {
                _avr.setAutoMode(true);
                _c._io->println(F("OK"));
            }
            else if (val == "manual")
            {
                _avr.setAutoMode(false);
                _c._io->println(F("OK"));
            }
            else
            {
                _c._io->println(F("Usage: mode <auto|manual>"));
            }
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("source "))
        {
            String val = cmd.substring(7);
            val.trim();
            AvrController::Source src = AvrController::Source::Off;
            if (!parseSource_(val, src))
                _c._io->println(F("Usage: source <off|main|reserve>"));
            else if (!_avr.setManualSource(src))
                _c._io->println(F("No changes"));
            else
                _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (handleBoolCmd_(cmd, lower, "prefer_main ", "prefer_main", &AvrController::setPreferMain))
            return true;
        if (handleBoolCmd_(cmd, lower, "auto_return ", "auto_return_main", &AvrController::setAutoReturnMain))
            return true;
        if (handlePortCmd_(cmd, lower, "main_ok ", &AvrController::setMainOkPort))
            return true;
        if (handlePortCmd_(cmd, lower, "reserve_ok ", &AvrController::setReserveOkPort))
            return true;
        if (handlePortCmd_(cmd, lower, "relay_main ", &AvrController::setRelayMainPort))
            return true;
        if (handlePortCmd_(cmd, lower, "relay_reserve ", &AvrController::setRelayReservePort))
            return true;
        if (handlePortCmd_(cmd, lower, "fb_main ", &AvrController::setFeedbackMainPort))
            return true;
        if (handlePortCmd_(cmd, lower, "fb_reserve ", &AvrController::setFeedbackReservePort))
            return true;
        if (handleApplyBoolCmd_(cmd, lower, "main_ok_al ", "main_ok_active_low"))
            return true;
        if (handleApplyBoolCmd_(cmd, lower, "reserve_ok_al ", "reserve_ok_active_low"))
            return true;
        if (handleApplyBoolCmd_(cmd, lower, "fb_main_al ", "feedback_main_active_low"))
            return true;
        if (handleApplyBoolCmd_(cmd, lower, "fb_reserve_al ", "feedback_reserve_active_low"))
            return true;
        if (handleApplyBoolCmd_(cmd, lower, "relay_main_inv ", "relay_main_invert"))
            return true;
        if (handleApplyBoolCmd_(cmd, lower, "relay_reserve_inv ", "relay_reserve_invert"))
            return true;
        if (handleApplyMsCmd_(cmd, lower, "debounce ", "debounce_ms"))
            return true;
        if (handleApplyMsCmd_(cmd, lower, "loss_delay ", "loss_delay_ms"))
            return true;
        if (handleApplyMsCmd_(cmd, lower, "return_delay ", "return_delay_ms"))
            return true;
        if (handleApplyMsCmd_(cmd, lower, "break ", "break_ms"))
            return true;
        if (handleApplyMsCmd_(cmd, lower, "warmup ", "warmup_ms"))
            return true;
        if (handleApplyMsCmd_(cmd, lower, "timeout ", "transfer_timeout_ms"))
            return true;

        return false;
    }

private:
    ConsoleT &_c;
    AvrController &_avr;

    using BoolSetter = bool (AvrController::*)(bool);
    using PortSetter = bool (AvrController::*)(uint8_t);

    static bool parseOnOff_(const String &value, bool &out)
    {
        String t = value;
        t.trim();
        t.toLowerCase();
        if (t == "on" || t == "1" || t == "true" || t == "yes")
        {
            out = true;
            return true;
        }
        if (t == "off" || t == "0" || t == "false" || t == "no")
        {
            out = false;
            return true;
        }
        return false;
    }

    static bool parsePort_(const String &value, uint8_t &out)
    {
        String t = value;
        t.trim();
        t.toLowerCase();
        if (t == "none" || t == "-" || t.length() == 0)
        {
            out = AvrController::kInvalidPort;
            return true;
        }
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const unsigned long raw = strtoul(t.c_str(), nullptr, 10);
        if (raw > 255u)
            return false;
        out = (uint8_t)raw;
        return true;
    }

    static bool parseU32_(const String &value, uint32_t &out)
    {
        String t = value;
        t.trim();
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        out = (uint32_t)strtoul(t.c_str(), nullptr, 10);
        return true;
    }

    static bool parseSource_(const String &value, AvrController::Source &out)
    {
        String t = value;
        t.trim();
        t.toLowerCase();
        if (t == "off")
        {
            out = AvrController::Source::Off;
            return true;
        }
        if (t == "main")
        {
            out = AvrController::Source::Main;
            return true;
        }
        if (t == "reserve")
        {
            out = AvrController::Source::Reserve;
            return true;
        }
        return false;
    }

    bool handleBoolCmd_(const String &cmd, const String &lower, const char *prefix,
                        const char *usage_name, BoolSetter setter)
    {
        if (!lower.startsWith(prefix))
            return false;
        String val = cmd.substring(strlen(prefix));
        val.trim();
        bool enabled = false;
        if (!parseOnOff_(val, enabled))
        {
            _c._io->print(F("Usage: "));
            _c._io->print(usage_name);
            _c._io->println(F(" <on|off>"));
        }
        else if (!(_avr.*setter)(enabled))
            _c._io->println(F("No changes"));
        else
            _c._io->println(F("OK"));
        _c.printPrompt_();
        return true;
    }

    bool handlePortCmd_(const String &cmd, const String &lower, const char *prefix, PortSetter setter)
    {
        if (!lower.startsWith(prefix))
            return false;
        String val = cmd.substring(strlen(prefix));
        val.trim();
        uint8_t port = AvrController::kInvalidPort;
        if (!parsePort_(val, port))
            _c._io->println(F("Usage: <cmd> <port|none>"));
        else if (!(_avr.*setter)(port))
            _c._io->println(F("No changes"));
        else
            _c._io->println(F("OK"));
        _c.printPrompt_();
        return true;
    }

    bool handleApplyBoolCmd_(const String &cmd, const String &lower, const char *prefix, const char *field)
    {
        if (!lower.startsWith(prefix))
            return false;
        String val = cmd.substring(strlen(prefix));
        val.trim();
        bool enabled = false;
        if (!parseOnOff_(val, enabled))
        {
            _c._io->println(F("Usage: <cmd> <on|off>"));
            _c.printPrompt_();
            return true;
        }
        DynamicJsonDocument doc(96);
        JsonObject obj = doc.to<JsonObject>();
        obj[field] = enabled;
        _avr.applyConfig(doc.as<JsonObjectConst>());
        _c._io->println(F("OK"));
        _c.printPrompt_();
        return true;
    }

    bool handleApplyMsCmd_(const String &cmd, const String &lower, const char *prefix, const char *field)
    {
        if (!lower.startsWith(prefix))
            return false;
        String val = cmd.substring(strlen(prefix));
        val.trim();
        uint32_t ms = 0;
        if (!parseU32_(val, ms))
        {
            _c._io->println(F("Usage: <cmd> <ms>"));
            _c.printPrompt_();
            return true;
        }
        DynamicJsonDocument doc(96);
        JsonObject obj = doc.to<JsonObject>();
        obj[field] = ms;
        _avr.applyConfig(doc.as<JsonObjectConst>());
        _c._io->println(F("OK"));
        _c.printPrompt_();
        return true;
    }

    void printBoolLine_(const char *name, bool value) const
    {
        _c._io->print(name);
        _c._io->print(F(": "));
        _c._io->println(value ? F("on") : F("off"));
    }

    void printU32Line_(const char *name, uint32_t value) const
    {
        _c._io->print(name);
        _c._io->print(F(": "));
        _c._io->println((unsigned long)value);
    }

    void printPortLine_(const char *name, uint8_t port) const
    {
        _c._io->print(name);
        _c._io->print(F(": "));
        if (port == AvrController::kInvalidPort)
            _c._io->println(F("none"));
        else
            _c._io->println((unsigned)port);
    }

    void printSourceLine_(const char *name, AvrController::Source src) const
    {
        _c._io->print(name);
        _c._io->print(F(": "));
        _c._io->println(AvrController::sourceName(src));
    }
};
