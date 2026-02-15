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
#include <HTTPClient.h>

#if defined(ARDUINO_ARCH_ESP32)
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"
#endif

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
        _c._io->println(F("    user list               - show users table"));
        _c._io->println(F("    user enable <id> <on|off> - enable/disable user"));
        _c._io->println(F("    user username <id> <value|clear> - set web username"));
        _c._io->println(F("    user tg_username <id> <value|clear> - set telegram username"));
        _c._io->println(F("    user tg_chat <id> <chat_id|0> - set telegram chat id"));
        _c._io->println(F("    user is_admin <id> <on|off> - set admin flag"));
        _c._io->println(F("    user tg_notify <id> <on|off> - set telegram notify flag"));
        _c._io->println(F("    user webpass <id> <password|clear> - set/clear web password"));
        _c._io->println(F("    user acl <id> <all|none> - grant all/clear ACL"));
        _c._io->println(F("    allow list              - show allowed users"));
        _c._io->println(F("    allow add <username> [chat_id] [admin] [notify] [off] - add allowed user"));
        _c._io->println(F("    allow del <username>    - remove allowed user"));
        _c._io->println(F("    allow clear             - clear allowed list"));
        _c._io->println(F("    send <text>             - send message"));
#if !defined(FB_NO_FILE) && (defined(ESP8266) || defined(ESP32))
#endif
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
        _c._io->println(F("  user list               - show users table"));
        _c._io->println(F("  user enable <id> <on|off> - enable/disable user"));
        _c._io->println(F("  user username <id> <value|clear> - set web username"));
        _c._io->println(F("  user tg_username <id> <value|clear> - set telegram username"));
        _c._io->println(F("  user tg_chat <id> <chat_id|0> - set telegram chat id"));
        _c._io->println(F("  user is_admin <id> <on|off> - set admin flag"));
        _c._io->println(F("  user tg_notify <id> <on|off> - set telegram notify flag"));
        _c._io->println(F("  user webpass <id> <password|clear> - set/clear web password"));
        _c._io->println(F("  user acl <id> <all|none> - grant all/clear ACL"));
        _c._io->println(F("  allow list              - show allowed users"));
        _c._io->println(F("  allow add <username> [chat_id] [admin] [notify] [off] - add allowed user"));
        _c._io->println(F("  allow del <username>    - remove allowed user"));
        _c._io->println(F("  allow clear             - clear allowed list"));
        _c._io->println(F("  send <text>             - send message"));
#if !defined(FB_NO_FILE) && (defined(ESP8266) || defined(ESP32))
#endif
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
        if (lower == "user list" || lower == "user show")
        {
            _c._io->println(F("ID  En  Username         TG User          TG Chat        Admin Notify WebPass"));
            _c._io->println(F("--  --  ---------------  ---------------  -------------  ----- ------ -------"));
            for (size_t i = 0; i < _c._users.size(); ++i)
            {
                const auto &u = _c._users.user(i);
                const String id = String((unsigned)u.id);
                const String chat = (u.tg_chat_id != 0) ? String((long long)u.tg_chat_id) : String("-");
                _c._io->print(id);
                _c._io->print(F("   "));
                _c._io->print(u.enabled ? F("Y ") : F("N "));
                _c._io->print(F(" "));
                _c._io->print(u.username.length() ? u.username : String("-"));
                _c._io->print(F("   "));
                _c._io->print(u.tg_username.length() ? u.tg_username : String("-"));
                _c._io->print(F("   "));
                _c._io->print(chat);
                _c._io->print(F("   "));
                _c._io->print(u.tg_admin ? F("yes") : F("no "));
                _c._io->print(F("   "));
                _c._io->print(u.tg_notify ? F("yes") : F("no "));
                _c._io->print(F("   "));
                _c._io->println(u.hasWebPassword() ? F("yes") : F("no"));
            }
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("user enable "))
        {
            uint8_t id = 0;
            String value;
            if (!parseUserIdAndTail_(cmd.substring(12), id, value))
            {
                _c._io->println(F("Usage: user enable <id> <on|off>"));
                _c.printPrompt_();
                return true;
            }
            bool b = false;
            if (!parseBoolToken_(value, b))
            {
                _c._io->println(F("Invalid flag, use on|off"));
                _c.printPrompt_();
                return true;
            }
            _c._users.user((size_t)(id - 1)).enabled = b;
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("user username "))
        {
            uint8_t id = 0;
            String value;
            if (!parseUserIdAndTail_(cmd.substring(14), id, value))
            {
                _c._io->println(F("Usage: user username <id> <value|clear>"));
                _c.printPrompt_();
                return true;
            }
            auto &u = _c._users.user((size_t)(id - 1));
            String v = value;
            v.trim();
            if (v == "clear")
                v = "";
            u.username = UsersRegistry::normalizeUsername(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("user tg_username "))
        {
            uint8_t id = 0;
            String value;
            if (!parseUserIdAndTail_(cmd.substring(17), id, value))
            {
                _c._io->println(F("Usage: user tg_username <id> <value|clear>"));
                _c.printPrompt_();
                return true;
            }
            auto &u = _c._users.user((size_t)(id - 1));
            String v = value;
            v.trim();
            if (v == "clear")
                v = "";
            u.tg_username = UsersRegistry::normalizeTgUsername(v);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("user tg_chat "))
        {
            uint8_t id = 0;
            String value;
            if (!parseUserIdAndTail_(cmd.substring(13), id, value))
            {
                _c._io->println(F("Usage: user tg_chat <id> <chat_id|0>"));
                _c.printPrompt_();
                return true;
            }
            value.trim();
            _c._users.user((size_t)(id - 1)).tg_chat_id = (int64_t)strtoll(value.c_str(), nullptr, 10);
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("user is_admin "))
        {
            uint8_t id = 0;
            String value;
            if (!parseUserIdAndTail_(cmd.substring(14), id, value))
            {
                _c._io->println(F("Usage: user is_admin <id> <on|off>"));
                _c.printPrompt_();
                return true;
            }
            bool b = false;
            if (!parseBoolToken_(value, b))
            {
                _c._io->println(F("Invalid flag, use on|off"));
                _c.printPrompt_();
                return true;
            }
            _c._users.user((size_t)(id - 1)).tg_admin = b;
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("user tg_notify "))
        {
            uint8_t id = 0;
            String value;
            if (!parseUserIdAndTail_(cmd.substring(15), id, value))
            {
                _c._io->println(F("Usage: user tg_notify <id> <on|off>"));
                _c.printPrompt_();
                return true;
            }
            bool b = false;
            if (!parseBoolToken_(value, b))
            {
                _c._io->println(F("Invalid flag, use on|off"));
                _c.printPrompt_();
                return true;
            }
            _c._users.user((size_t)(id - 1)).tg_notify = b;
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("user webpass "))
        {
            uint8_t id = 0;
            String value;
            if (!parseUserIdAndTail_(cmd.substring(13), id, value))
            {
                _c._io->println(F("Usage: user webpass <id> <password|clear>"));
                _c.printPrompt_();
                return true;
            }
            auto &u = _c._users.user((size_t)(id - 1));
            value.trim();
            if (value == "clear")
            {
                u.clearWebPassword();
                _c._io->println(F("OK"));
                _c.printPrompt_();
                return true;
            }
            if (!u.setWebPassword(value))
            {
                _c._io->println(F("Failed to set password"));
                _c.printPrompt_();
                return true;
            }
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("user acl "))
        {
            uint8_t id = 0;
            String value;
            if (!parseUserIdAndTail_(cmd.substring(9), id, value))
            {
                _c._io->println(F("Usage: user acl <id> <all|none>"));
                _c.printPrompt_();
                return true;
            }
            value.trim();
            value.toLowerCase();
            auto &u = _c._users.user((size_t)(id - 1));
            if (value == "all")
                u.grantAllAcl();
            else if (value == "none")
                u.clearAcl();
            else
            {
                _c._io->println(F("Invalid mode, use all|none"));
                _c.printPrompt_();
                return true;
            }
            _c._io->println(F("OK"));
            _c.printPrompt_();
            return true;
        }
        if (lower == "allow list")
        {
            const auto users = _c._tgbot_menu.allowedUsers();
            if (users.empty())
                _c._io->println(F("Allowed list empty"));
            else
            {
                _c._io->println(F("ID  Username           ChatID       Admin Notify Enabled"));
                _c._io->println(F("--  -----------------  -----------  ----- ------ -------"));
                for (size_t i = 0; i < users.size; ++i)
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
#if !defined(FB_NO_FILE) && (defined(ESP8266) || defined(ESP32))
        if (lower.startsWith("snapdoc "))
        {
            _c._io->println(F("Camera commands are disabled"));
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("snapphoto "))
        {
            _c._io->println(F("Camera commands are disabled"));
            _c.printPrompt_();
            return true;
        }
#endif
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
#if !defined(FB_NO_FILE) && (defined(ESP8266) || defined(ESP32))
    static constexpr size_t kSnapshotMaxBytes = 4u * 1024u * 1024u;

    bool sendSnapshotToTelegram_(const String &url, bool as_photo, String &err)
    {
        if (url.length() == 0)
        {
            err = F("URL is empty");
            return false;
        }
        if (!url.startsWith("http://") && !url.startsWith("https://"))
        {
            err = F("Only http:// and https:// supported");
            return false;
        }

        HTTPClient http;
        if (!http.begin(url))
        {
            err = F("HTTP begin failed");
            return false;
        }
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        const int code = http.GET();
        if (code != HTTP_CODE_OK)
        {
            err = String(F("HTTP code: ")) + String(code);
            http.end();
            return false;
        }

        int content_len = http.getSize();
        if (content_len > 0 && (size_t)content_len > kSnapshotMaxBytes)
        {
            err = F("Snapshot too large");
            http.end();
            return false;
        }

        const size_t cap = (content_len > 0) ? (size_t)content_len : kSnapshotMaxBytes;
        if (cap == 0)
        {
            err = F("Invalid snapshot size");
            http.end();
            return false;
        }

        uint8_t *buf = allocSnapshotBuf_(cap);
        if (!buf)
        {
            err = F("Buffer alloc failed");
            http.end();
            return false;
        }

        WiFiClient *stream = http.getStreamPtr();
        size_t used = 0;
        uint32_t last_rx_ms = millis();
        while (http.connected())
        {
            const int avail = stream->available();
            if (avail <= 0)
            {
                delay(1);
                if (content_len >= 0 && used >= (size_t)content_len)
                    break;
                if ((uint32_t)(millis() - last_rx_ms) > 10000u)
                    break;
                continue;
            }
            size_t take = (size_t)avail;
            if (take > cap - used)
                take = cap - used;
            if (take == 0)
                break;
            const size_t read_n = stream->readBytes((char *)(buf + used), take);
            if (read_n == 0)
                break;
            used += read_n;
            last_rx_ms = millis();
            if (used >= cap)
                break;
        }
        http.end();

        bool ok = false;
        if (used > 0)
        {
            if (content_len > 0 && used != (size_t)content_len)
            {
                err = F("Incomplete snapshot data");
            }
            const String name = guessFileName_(url);
            if (err.length() == 0)
            {
                ok = as_photo ? _c._tgbot.sendPhotoFromBuffer(buf, used, name)
                              : _c._tgbot.sendDocumentFromBuffer(buf, used, name);
            }
        }
        else
        {
            err = F("No data from camera");
        }

        free(buf);
        return ok;
    }

    static String guessFileName_(const String &url)
    {
        int slash = url.lastIndexOf('/');
        String name = (slash >= 0) ? url.substring(slash + 1) : String();
        int q = name.indexOf('?');
        if (q >= 0)
            name = name.substring(0, q);
        name.trim();
        if (name.length() == 0)
            return String("snapshot.jpg");
        return name;
    }

    static uint8_t *allocSnapshotBuf_(size_t bytes)
    {
#if defined(ARDUINO_ARCH_ESP32)
        if (psramFound())
        {
            uint8_t *p = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (p)
                return p;
        }
#endif
        return (uint8_t *)malloc(bytes);
    }
#endif

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

    static bool parseBoolToken_(String token, bool &out)
    {
        token.trim();
        token.toLowerCase();
        if (token == "on" || token == "1" || token == "true" || token == "yes")
        {
            out = true;
            return true;
        }
        if (token == "off" || token == "0" || token == "false" || token == "no")
        {
            out = false;
            return true;
        }
        return false;
    }

    static bool parseUserIdAndTail_(String input, uint8_t &id_out, String &tail_out)
    {
        input.trim();
        if (input.length() == 0)
            return false;
        int sp = input.indexOf(' ');
        String id_tok = (sp < 0) ? input : input.substring(0, (size_t)sp);
        id_tok.trim();
        const long id = id_tok.toInt();
        if (id < 1 || id > (long)UsersRegistry::kMaxUsers)
            return false;
        id_out = (uint8_t)id;
        if (sp < 0)
        {
            tail_out = "";
            return false;
        }
        tail_out = input.substring((size_t)sp + 1);
        tail_out.trim();
        return tail_out.length() > 0;
    }
};
