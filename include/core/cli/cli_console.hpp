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
#include <stdint.h>
#include <string.h>

#if defined(ESP32)
#include "mbedtls/sha256.h"
#endif

#include "core/rtc.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "ftest.hpp"
#include "plc/plc_control.hpp"
#include "core/cli/cli_config.hpp"
#include "core/cli/cli_enable.hpp"
#include "core/cli/modules/cli_tgbot.hpp"
#include "hal/bus/i2c.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"

class CliConsole
{
public:
    using CLIEnable = CLIEnableT<CliConsole>;
    using CLIConfig = CLIConfigT<CliConsole>;
    using CLIWifi = CLIWifiT<CliConsole>;
    using CLITgbot = CLITgbotT<CliConsole>;
    static constexpr const char kAdminUser[] = "admin";

    CliConsole(PlcControl &plc, WifiManager &wifi, RTC &rtc, Ftest &ftest, I2CManager &i2c,
               TelegramClient &tgbot, TelegramMenu &tgbot_menu, Configs &configs)
        : _plc(plc),
          _wifi(wifi),
          _rtc(rtc),
          _ftest(ftest),
          _i2c(i2c),
          _tgbot(tgbot),
          _tgbot_menu(tgbot_menu),
          _configs(configs),
          _wifi_cli(*this),
          _tgbot_cli(*this),
          _enable(*this, _wifi_cli),
          _config(*this, _wifi_cli, _tgbot_cli)
    {
    }

    void begin(Stream &io)
    {
        _io = &io;
        _state = State::NeedUser;
        _mode = Mode::Enable;
        _line = "";
        _user_input = "";
        printPrompt_();
    }

    void loop()
    {
        if (!_io)
            return;
        while (_io->available())
        {
            char c = (char)_io->read();
            if (c == '\r')
                continue;
            if (c == '\n')
            {
                handleLine_(_line);
                _line = "";
                continue;
            }
            if (c == 0x7F || c == 0x08)
            {
                if (_line.length() > 0)
                    _line.remove(_line.length() - 1);
                continue;
            }
            if (c == '\t')
            {
                handleTab_();
                continue;
            }
            if (_state != State::NeedPass)
                _io->print(c);
            if (_line.length() < kMaxLine)
                _line += c;
        }
    }

    bool setAdminPassword_(const String &pass)
    {
        if (pass.length() == 0)
            return false;
        uint8_t hash[32] = {};
        sha256_(pass.c_str(), hash);
        memcpy(_admin_hash, hash, sizeof(_admin_hash));
        _admin_password = pass;
        _admin_set = true;
        return true;
    }

    const String &adminPassword() const { return _admin_password; }
    bool adminPasswordSet() const { return _admin_set; }

    void enterUser() { _mode = Mode::Enable; printPrompt_(); }
    void enterEnable() { _mode = Mode::Enable; printPrompt_(); }
    void enterConfig() { _mode = Mode::Config; printPrompt_(); }
    void enterConfigWifi() { _mode = Mode::ConfigWifi; printPrompt_(); }
    void enterConfigTgbot() { _mode = Mode::ConfigTgbot; printPrompt_(); }
    void logout()
    {
        _state = State::NeedUser;
        _mode = Mode::Enable;
        _user_input = "";
        printPrompt_();
    }

    void cmdShowPlc_()
    {
        const float t = _plc.boardTemp();
        const bool fan = _plc.fanStatus();
        _io->println(F("PLC status:"));
        const size_t key_w = 6; // temp_c
        printKeyValue_(F("fan"), fan ? F("on") : F("off"), key_w);
        char buf[16] = {};
        dtostrf(t, 0, 2, buf);
        printKeyValue_(F("temp_c"), buf, key_w);
    }

    void cmdShowWifi_()
    {
        _io->println(F("Wi-Fi configurations:"));
        const size_t key_w = 11; // ap_password
        printKeyValue_(F("mode"), _wifi.ap() ? F("AP") : F("STA"), key_w);
        printKeyValue_(F("ssid"), _wifi.ssid(), key_w);
        printKeyValue_(F("password"), _wifi.password(), key_w);
        printKeyValue_(F("ap_ssid"), _wifi.apSsid(), key_w);
        printKeyValue_(F("ap_password"), _wifi.apPassword(), key_w);
    }

    void cmdShowTime_()
    {
        Ds3231Mz::DateTime dt{};
        if (!_rtc.Time(dt))
        {
            _io->println(F("RTC error"));
            return;
        }
        _io->println(F("RTC time:"));
        const size_t key_w = 8; // weekday
        char date_buf[16] = {};
        char time_buf[16] = {};
        snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
                 (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
                 (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
        printKeyValue_(F("date"), date_buf, key_w);
        printKeyValue_(F("time"), time_buf, key_w);
        printKeyValue_(F("weekday"), String((unsigned)dt.day_of_week), key_w);
    }

    void cmdShowTelegram_()
    {
        _io->println(F("Telegram:"));
        const size_t key_w = 10; // proxy_host
        printKeyValue_(F("token"), _tgbot.token(), key_w);
        printKeyValue_(F("chat_id"), String((long long)_tgbot.chatId()), key_w);
        printKeyValue_(F("insecure"), _tgbot.insecure() ? F("true") : F("false"), key_w);
        printKeyValue_(F("client"), _tgbot.clientKindName(), key_w);
        printKeyValue_(F("proxy"), _tgbot.useProxy() ? F("true") : F("false"), key_w);
        printKeyValue_(F("proxy_host"), _tgbot.proxyHost(), key_w);
        printKeyValue_(F("proxy_port"), String((unsigned)_tgbot.proxyPort()), key_w);
        printKeyValue_(F("proxy_path"), _tgbot.proxyPath(), key_w);
    }

    void cmdShowI2c_()
    {
        _io->println(F("I2C devices:"));
        _io->println(F("    Bus  Addr"));
        _io->println(F("    --- -----"));
        bool scanned[3] = {false, false, false};
        for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
        {
            const uint8_t bus = ActiveBoardProfile::I2CS[i].bus_num;
            if (bus < 3 && scanned[bus])
                continue;
            if (bus < 3)
                scanned[bus] = true;
            std::vector<uint8_t> addrs;
            if (!_i2c.scanDevices(bus, addrs))
                continue;
            for (size_t a = 0; a < addrs.size(); ++a)
            {
                char addr_buf[8] = {};
                snprintf(addr_buf, sizeof(addr_buf), "0x%02X", addrs[a]);
                char line[20] = {};
                snprintf(line, sizeof(line), "    %3u  %s", (unsigned)bus, addr_buf);
                _io->println(line);
            }
        }
    }

    void cmdShowConfig_()
    {
        JsonDocument doc;
        if (!_configs.load(doc))
        {
            const char *err = "Unknown error";
            switch (_configs.lastError())
            {
            case Configs::Error::FsMount:
                err = "FS mount failed";
                break;
            case Configs::Error::OpenRead:
                err = "Open read failed";
                break;
            case Configs::Error::OpenWrite:
                err = "Open write failed";
                break;
            case Configs::Error::JsonParse:
                err = "JSON parse failed";
                break;
            case Configs::Error::JsonSerialize:
                err = "JSON serialize failed";
                break;
            default:
                break;
            }
            _io->print(F("Show config failed: "));
            _io->println(err);
            return;
        }
        _io->println(F("Config file:"));
        String pretty;
        if (serializeJsonPretty(doc, pretty) == 0)
            _io->println(F("{}"));
        else
            _io->println(pretty);
    }

    void cmdFtest_()
    {
        _ftest.start();
        _io->println(F("ftest started"));
    }

    void cmdWifiRestart_()
    {
        if (_wifi.begin())
            _io->println(F("Wi-Fi restarted"));
        else
            _io->println(F("Wi-Fi restart failed"));
    }

    void cmdRestart_()
    {
#if defined(ESP32)
        _io->println(F("Restarting..."));
        _io->flush();
        ESP.restart();
#else
        _io->println(F("Restart not supported"));
#endif
    }

    void cmdWriteConfig_()
    {
        const bool saved = _configs_manager && _configs_manager->save();
        if (saved)
        {
            _io->println(F("OK"));
            return;
        }

        const char *err = _configs_manager ? "Unknown error" : "Config manager missing";
        switch (_configs.lastError())
        {
        case Configs::Error::FsMount:
            err = "FS mount failed";
            break;
        case Configs::Error::OpenRead:
            err = "Open read failed";
            break;
        case Configs::Error::OpenWrite:
            err = "Open write failed";
            break;
        case Configs::Error::JsonParse:
            err = "JSON parse failed";
            break;
        case Configs::Error::JsonSerialize:
            err = "JSON serialize failed";
            break;
        default:
            break;
        }
        _io->print(F("Write failed: "));
        _io->println(err);
    }

    void cmdEraseConfig_()
    {
        if (_configs.erase())
        {
            _io->println(F("OK"));
            return;
        }
        const char *err = "Unknown error";
        switch (_configs.lastError())
        {
        case Configs::Error::FsMount:
            err = "FS mount failed";
            break;
        case Configs::Error::OpenRead:
            err = "Open read failed";
            break;
        case Configs::Error::OpenWrite:
            err = "Open write failed";
            break;
        case Configs::Error::JsonParse:
            err = "JSON parse failed";
            break;
        case Configs::Error::JsonSerialize:
            err = "JSON serialize failed";
            break;
        default:
            break;
        }
        _io->print(F("Erase failed: "));
        _io->println(err);
    }

private:
    enum class Mode : uint8_t
    {
        User,
        Enable,
        Config,
        ConfigWifi,
        ConfigTgbot
    };

    enum class State : uint8_t
    {
        NeedUser,
        NeedPass,
        LoggedIn
    };

    static constexpr size_t kMaxLine = 96;
    void showHelpTopic_(const String &topic)
    {
        String t = topic;
        t.toLowerCase();
        if (t == "show")
        {
            _io->println(F("Show commands:"));
            _io->println(F("  show plc        - fan state and board temperature"));
            _io->println(F("  show wifi       - Wi-Fi configuration"));
            _io->println(F("  show time       - RTC date/time"));
            _io->println(F("  show i2c        - I2C device list"));
            _io->println(F("  show telegram   - Telegram settings"));
            _io->println(F("  show config     - configuration file contents"));
            return;
        }
        if (t == "wifi")
        {
            _wifi_cli.printHelpTopic();
            return;
        }
        if (t == "user")
        {
            _io->println(F("Admin commands:"));
            _io->println(F("  password <pass>         - set admin password"));
            _io->println(F("  admin password <pass>   - set admin password"));
            return;
        }
        if (t == "tgbot")
        {
            _tgbot_cli.printHelpTopic();
            return;
        }
        if (t == "system")
        {
            _io->println(F("System commands:"));
            _io->println(F("  ftest   - start functional test task"));
            _io->println(F("  reload  - restart controller"));
            _io->println(F("  reset   - restart controller"));
            _io->println(F("  write   - save configuration"));
            _io->println(F("  erase   - delete configuration"));
            return;
        }
        _io->println(F("Unknown topic"));
    }

    void handleTab_()
    {
        if (!_io || _state != State::LoggedIn)
            return;
        static const char *const kEnableCmds[] = {
            "show plc",
            "show wifi",
            "show time",
            "show i2c",
            "show telegram",
            "show config",
            "ftest",
            "wifi restart",
            "reload",
            "reset",
            "write",
            "erase",
            "configure terminal",
            "conf t",
            "disable",
            "logout",
            "exit",
            "help",
            "help show",
            "help wifi",
            "help user",
            "help system",
            "help tgbot"};
        static const size_t kEnableCmdsCount = sizeof(kEnableCmds) / sizeof(kEnableCmds[0]);

        static const char *const kConfigCmds[] = {
            "password <pass>",
            "admin password <pass>",
            "wifi",
            "tgbot",
            "wifi ssid <value>",
            "wifi password <value>",
            "wifi ap on",
            "wifi ap off",
            "wifi ap_ssid <value>",
            "wifi ap_password <value>",
            "wifi restart",
            "exit",
            "end",
            "help",
            "help show",
            "help wifi",
            "help user",
            "help system",
            "help tgbot"};
        static const size_t kConfigCmdsCount = sizeof(kConfigCmds) / sizeof(kConfigCmds[0]);

        static const char *const kConfigWifiCmds[] = {
            "ssid <value>",
            "password <value>",
            "ap on",
            "ap off",
            "ap_ssid <value>",
            "ap_password <value>",
            "restart",
            "show",
            "exit",
            "end",
            "help",
            "help show",
            "help wifi",
            "help user",
            "help system"};
        static const size_t kConfigWifiCmdsCount = sizeof(kConfigWifiCmds) / sizeof(kConfigWifiCmds[0]);

        static const char *const kConfigTgbotCmds[] = {
            "token <value>",
            "chat <id>",
            "insecure on",
            "insecure off",
            "allow list",
            "allow add <username>",
            "allow del <username>",
            "allow clear",
            "send <text>",
            "poll",
            "show",
            "exit",
            "end",
            "help",
            "help show",
            "help wifi",
            "help user",
            "help system",
            "help tgbot"};
        static const size_t kConfigTgbotCmdsCount = sizeof(kConfigTgbotCmds) / sizeof(kConfigTgbotCmds[0]);

        const char *const *cmds = nullptr;
        size_t count = 0;
        switch (_mode)
        {
        case Mode::Enable:
            cmds = kEnableCmds;
            count = kEnableCmdsCount;
            break;
        case Mode::Config:
            cmds = kConfigCmds;
            count = kConfigCmdsCount;
            break;
        case Mode::ConfigWifi:
            cmds = kConfigWifiCmds;
            count = kConfigWifiCmdsCount;
            break;
        case Mode::ConfigTgbot:
            cmds = kConfigTgbotCmds;
            count = kConfigTgbotCmdsCount;
            break;
        case Mode::User:
            cmds = kEnableCmds;
            count = kEnableCmdsCount;
            break;
        }

        auto isPlaceholder = [](const String &tok) -> bool
        {
            return tok.length() >= 3 && tok[0] == '<' && tok[tok.length() - 1] == '>';
        };
        auto tokenize = [](const String &s, String *out, size_t max, bool &ends_space) -> size_t
        {
            ends_space = (s.length() > 0 && s[s.length() - 1] == ' ');
            size_t count_out = 0;
            String cur;
            for (size_t i = 0; i < s.length(); ++i)
            {
                char ch = s[i];
                if (ch == ' ')
                {
                    if (cur.length() > 0)
                    {
                        if (count_out < max)
                            out[count_out++] = cur;
                        cur = "";
                    }
                }
                else
                {
                    cur += ch;
                }
            }
            if (cur.length() > 0 && count_out < max)
                out[count_out++] = cur;
            if (ends_space && count_out < max)
                out[count_out++] = "";
            return count_out;
        };
        auto eqTok = [](const String &a, const String &b) -> bool
        {
            String la = a;
            String lb = b;
            la.toLowerCase();
            lb.toLowerCase();
            return la == lb;
        };
        auto startsWithTok = [](const String &a, const String &b) -> bool
        {
            String la = a;
            String lb = b;
            la.toLowerCase();
            lb.toLowerCase();
            return la.startsWith(lb);
        };

        String in_tokens[6];
        bool ends_space = false;
        size_t in_count = tokenize(_line, in_tokens, 6, ends_space);

        int match_idx = -1;
        size_t matches = 0;
        bool single_placeholder = false;

        for (size_t i = 0; i < count; ++i)
        {
            String cmd = cmds[i];
            String cmd_tokens[6];
            bool cmd_space = false;
            size_t cmd_count = tokenize(cmd, cmd_tokens, 6, cmd_space);
            if (in_count > cmd_count)
                continue;

            bool ok = true;
            for (size_t t = 0; t < in_count; ++t)
            {
                const String &in_tok = in_tokens[t];
                const String &cmd_tok = cmd_tokens[t];
                const bool is_last = (t == in_count - 1);
                if (is_last)
                {
                    if (isPlaceholder(cmd_tok))
                    {
                        ok = false;
                        break;
                    }
                    if (in_tok.length() > 0 && !startsWithTok(cmd_tok, in_tok))
                    {
                        ok = false;
                        break;
                    }
                }
                else
                {
                    if (!eqTok(cmd_tok, in_tok))
                    {
                        ok = false;
                        break;
                    }
                }
            }
            if (!ok)
                continue;
            match_idx = (int)i;
            ++matches;
            if (in_count > 0 && cmd_count >= in_count)
            {
                String tok = cmd_tokens[in_count - 1];
                if (isPlaceholder(tok))
                    single_placeholder = true;
            }
        }

        if (matches == 0)
            return;
        if (matches == 1 && match_idx >= 0 && !single_placeholder)
        {
            String cmd = cmds[match_idx];
            String cmd_tokens[6];
            bool cmd_space = false;
            size_t cmd_count = tokenize(cmd, cmd_tokens, 6, cmd_space);

            if (in_count == 0)
                return;
            const String &cmd_tok = cmd_tokens[in_count - 1];
            const String &in_tok = in_tokens[in_count - 1];
            if (cmd_tok.length() > in_tok.length())
            {
                String suffix = cmd_tok.substring(in_tok.length());
                _io->print(suffix);
                _line += suffix;
            }
            if (cmd_count > in_count)
            {
                _io->print(' ');
                _line += ' ';
            }
            return;
        }

        _io->println();
        for (size_t i = 0; i < count; ++i)
        {
            String cmd = cmds[i];
            String cmd_tokens[6];
            bool cmd_space = false;
            size_t cmd_count = tokenize(cmd, cmd_tokens, 6, cmd_space);
            if (in_count > cmd_count)
                continue;
            bool ok = true;
            for (size_t t = 0; t < in_count; ++t)
            {
                const String &in_tok = in_tokens[t];
                const String &cmd_tok = cmd_tokens[t];
                const bool is_last = (t == in_count - 1);
                if (is_last)
                {
                    if (isPlaceholder(cmd_tok))
                    {
                        ok = false;
                        break;
                    }
                    if (in_tok.length() > 0 && !startsWithTok(cmd_tok, in_tok))
                    {
                        ok = false;
                        break;
                    }
                }
                else if (!eqTok(cmd_tok, in_tok))
                {
                    ok = false;
                    break;
                }
            }
            if (!ok)
                continue;
            _io->print(F("  "));
            _io->println(cmd);
        }
        printPrompt_();
        _io->print(_line);
    }


    void handleShow_(String what)
    {
        what.trim();
        if (eq_(what, "plc"))
            cmdShowPlc_();
        else if (eq_(what, "wifi"))
            cmdShowWifi_();
        else if (eq_(what, "time"))
            cmdShowTime_();
        else if (eq_(what, "i2c"))
            cmdShowI2c_();
        else if (eq_(what, "telegram"))
            cmdShowTelegram_();
        else if (eq_(what, "config"))
            cmdShowConfig_();
        else
            _io->println(F("Unknown show"));
        printPrompt_();
    }

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

    void handleLine_(String line)
    {
        line.trim();
        if (_state != State::LoggedIn)
        {
            handleLogin_(line);
            return;
        }
        if (line.length() == 0)
        {
            printPrompt_();
            return;
        }

        switch (_mode)
        {
        case Mode::Enable:
            _enable.handle(line);
            break;
        case Mode::Config:
            _config.handle(line);
            break;
        case Mode::ConfigWifi:
            _config.handleWifiContext(line);
            break;
        case Mode::ConfigTgbot:
            _config.handleTgbotContext(line);
            break;
        case Mode::User:
            _enable.handle(line);
            break;
        }
    }

    void handleLogin_(const String &line)
    {
        if (_state == State::NeedUser)
        {
            if (line.length() == 0)
            {
                printPrompt_();
                return;
            }
            _user_input = line;
            _state = State::NeedPass;
            printPrompt_();
            return;
        }

        if (_state == State::NeedPass)
        {
            if (!isAdminUser_(_user_input))
            {
                printLine_(F("Login invalid"));
                _state = State::NeedUser;
                _user_input = "";
                printPrompt_();
                return;
            }

            if (!_admin_set)
            {
                _state = State::LoggedIn;
                _mode = Mode::Enable;
                printPrompt_();
                return;
            }

            if (checkAdmin_(line.c_str()))
            {
                _state = State::LoggedIn;
                _mode = Mode::Enable;
                printPrompt_();
            }
            else
            {
                printLine_(F("Login invalid"));
                _state = State::NeedUser;
                _user_input = "";
                printPrompt_();
            }
        }
    }

    void printPrompt_()
    {
        if (!_io)
            return;
        if (_state == State::NeedUser)
        {
            _io->print(F("login: "));
            return;
        }
        if (_state == State::NeedPass)
        {
            _io->print(F("password: "));
            return;
        }

        switch (_mode)
        {
        case Mode::User:
            _io->print(F("plc> "));
            break;
        case Mode::Enable:
            _io->print(F("plc# "));
            break;
        case Mode::Config:
            _io->print(F("plc(config)# "));
            break;
        case Mode::ConfigWifi:
            _io->print(F("plc(config-wifi)# "));
            break;
        case Mode::ConfigTgbot:
            _io->print(F("plc(config-tgbot)# "));
            break;
        }
    }

    void printLine_(const __FlashStringHelper *s)
    {
        if (_io)
            _io->println(s);
    }

    void printKeyValue_(const __FlashStringHelper *key, const __FlashStringHelper *value, size_t key_w)
    {
        if (!_io)
            return;
        _io->print(F("    "));
        _io->print(key);
        size_t len = strlen_P(reinterpret_cast<const char *>(key));
        if (len < key_w)
        {
            for (size_t i = 0; i < (key_w - len); ++i)
                _io->print(F(" "));
        }
        _io->print(F(" : "));
        _io->println(value);
    }

    void printKeyValue_(const __FlashStringHelper *key, const String &value, size_t key_w)
    {
        if (!_io)
            return;
        _io->print(F("    "));
        _io->print(key);
        size_t len = strlen_P(reinterpret_cast<const char *>(key));
        if (len < key_w)
        {
            for (size_t i = 0; i < (key_w - len); ++i)
                _io->print(F(" "));
        }
        _io->print(F(" : "));
        _io->println(value);
    }

    static void sha256_(const char *input, uint8_t out[32])
    {
        if (!input)
            return;
#if defined(ESP32)
        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts_ret(&ctx, 0);
        mbedtls_sha256_update_ret(&ctx, (const unsigned char *)input, strlen(input));
        mbedtls_sha256_finish_ret(&ctx, out);
        mbedtls_sha256_free(&ctx);
#else
        (void)input;
        for (uint8_t i = 0; i < 32; ++i)
            out[i] = 0;
#endif
    }

    bool isAdminUser_(const String &user) const
    {
        String u = user;
        u.toLowerCase();
        return u == kAdminUser;
    }

    bool checkAdmin_(const char *pass) const
    {
        if (!pass || !_admin_set)
            return false;
        uint8_t hash[32] = {};
        sha256_(pass, hash);
        return memcmp(_admin_hash, hash, sizeof(hash)) == 0;
    }

    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    Ftest &_ftest;
    I2CManager &_i2c;
    TelegramClient &_tgbot;
    TelegramMenu &_tgbot_menu;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;

    Stream *_io = nullptr;
    String _line;
    String _user_input;
    State _state = State::NeedUser;
    Mode _mode = Mode::Enable;
    uint8_t _admin_hash[32] = {};
    bool _admin_set = false;
    String _admin_password;

    CLIWifi _wifi_cli;
    CLITgbot _tgbot_cli;
    CLIEnable _enable;
    CLIConfig _config;
    uint32_t _tgbot_last_update_id = 0;

    template <typename>
    friend class CLIEnableT;
    template <typename>
    friend class CLIConfigT;
    template <typename>
    friend class CLIWifiT;
    template <typename>
    friend class CLITgbotT;

public:
    void setConfigsManager(ConfigsManagerIface &mgr) { _configs_manager = &mgr; }
};
