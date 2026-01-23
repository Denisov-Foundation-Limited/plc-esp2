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
#include <array>
#include <vector>

#include "core/cli/cli_console.hpp"
#include "core/network/network.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/gsm_modem.hpp"
#include "controllers/controllers.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"
#include "plc/plc_control.hpp"

class ConfigsManager : public ConfigsManagerIface
{
public:
    static constexpr size_t kConfigDocCapacity = 12288;

    ConfigsManager(Configs &configs, WifiManager &wifi, TelegramClient &telegram,
                   Network &network, CliConsole &console, TelegramMenu &telegram_menu, PlcControl &plc,
                   Controllers &controllers, GsmModem &gsm)
        : _configs(configs),
          _wifi(wifi),
          _telegram(telegram),
          _network(network),
          _console(console),
          _telegram_menu(telegram_menu),
          _plc(plc),
          _controllers(controllers),
          _gsm(gsm)
    {
    }

    StackRole stackRole() const override { return _stack_role; }
    String stackMasterHost() const override { return _stack_master_host; }
    String stackApiKey() const override { return _stack_api_key; }
    void setStackRole(StackRole role) override { _stack_role = role; }
    void setStackMasterHost(const String &host) override { _stack_master_host = host; }
    void setStackApiKey(const String &key) override { _stack_api_key = key; }

    bool loadConfigs()
    {
        _doc.clear();
        if (!_configs.load(_doc))
        {
            if (_configs.lastError() == Configs::Error::OpenRead)
                return true;
            return false;
        }
        applyConfig_(_doc);
        return true;
    }

    bool save() override
    {
        _doc.clear();
        JsonObject w = _doc["wifi"].to<JsonObject>();
        w["ssid"] = _wifi.ssid();
        w["password"] = _wifi.password();
        w["ap"] = _wifi.ap();
        w["ap_ssid"] = _wifi.apSsid();
        w["ap_password"] = _wifi.apPassword();

        JsonObject t = _doc["telegram"].to<JsonObject>();
        t["token"] = _telegram.token();
        t["chat_id"] = (long long)_telegram.chatId();
        t["insecure"] = _telegram.insecure();
        t["client"] = _telegram.clientKindName();
        t["use_proxy"] = _telegram.useProxy();
        t["proxy_host"] = _telegram.proxyHost();
        t["proxy_port"] = (unsigned)_telegram.proxyPort();
        t["proxy_path"] = _telegram.proxyPath();
        JsonArray allowed = t["allowed_users"].to<JsonArray>();
        size_t allow_idx = 0;
        const auto users = _telegram_menu.allowedUsers();
        for (size_t i = 0; i < users.size; ++i)
        {
            const auto &user = users[i];
            if (!user.enabled)
                continue;
            JsonObject u = allowed.add<JsonObject>();
            u["id"] = (unsigned)(allow_idx + 1);
            if (user.username.length())
                u["username"] = user.username;
            if (user.chat_id)
                u["chat_id"] = (long long)user.chat_id;
            u["is_admin"] = user.is_admin;
            u["is_notify"] = user.is_notify;
            u["enabled"] = user.enabled;
            ++allow_idx;
        }

        if (_console.adminPasswordSet())
        {
            JsonObject a = _doc["admin"].to<JsonObject>();
            a["password_hash"] = _console.adminPasswordHashHex();
        }

        JsonObject plc = _doc["plc"].to<JsonObject>();
        plc["device_name"] = _plc.deviceName();

        JsonObject s = _doc["stack"].to<JsonObject>();
        s["role"] = (_stack_role == StackRole::Master) ? "master" : "slave";
        s["master_host"] = _stack_master_host;
        s["api_key"] = _stack_api_key;

        JsonObject ctrl = _doc["controllers"].to<JsonObject>();
        _controllers.serialize(ctrl);

        JsonObject g = _doc["gsm"].to<JsonObject>();
        g["enabled"] = _gsm.enabled();

        return _configs.save(_doc);
    }

    bool save(const JsonDocument &doc) override
    {
        return _configs.save(doc);
    }

private:
    void applyConfig_(const JsonDocument &doc)
    {
        if (doc["wifi"].is<JsonObjectConst>())
        {
            JsonObjectConst w = doc["wifi"].as<JsonObjectConst>();
            if (w["ssid"].is<const char *>())
                _wifi.setSsid(w["ssid"].as<const char *>());
            if (w["password"].is<const char *>())
                _wifi.setPassword(w["password"].as<const char *>());
            if (w["ap"].is<bool>())
                _wifi.setAp(w["ap"].as<bool>());
            if (w["ap_ssid"].is<const char *>())
                _wifi.setApSsid(w["ap_ssid"].as<const char *>());
            if (w["ap_password"].is<const char *>())
                _wifi.setApPassword(w["ap_password"].as<const char *>());
        }

        if (doc["telegram"].is<JsonObjectConst>())
        {
            JsonObjectConst t = doc["telegram"].as<JsonObjectConst>();
            if (t["token"].is<const char *>())
                _telegram.setToken(t["token"].as<const char *>());
            if (t["chat_id"].is<long long>())
                _telegram.setChatId((int64_t)t["chat_id"].as<long long>());
            if (t["insecure"].is<bool>())
                _telegram.setInsecure(t["insecure"].as<bool>());

            if (t["client"].is<const char *>())
            {
                String c = t["client"].as<const char *>();
                c.toLowerCase();
                if (c == "tinygsm")
                    _network.setTelegramClientKind(TelegramNetCfg::ClientKind::TinyGsm);
                else if (c == "wifi" || c == "wifi_secure")
                    _network.setTelegramClientKind(TelegramNetCfg::ClientKind::WifiSecure);
            }

            bool proxy_override = false;
            bool use_proxy = false;
            String host;
            uint16_t port = 0;
            String path;

            if (t["use_proxy"].is<bool>())
            {
                proxy_override = true;
                use_proxy = t["use_proxy"].as<bool>();
            }
            if (t["proxy_host"].is<const char *>())
            {
                proxy_override = true;
                host = t["proxy_host"].as<const char *>();
                if (!t["use_proxy"].is<bool>())
                    use_proxy = true;
            }
            if (t["proxy_port"].is<unsigned>())
            {
                proxy_override = true;
                port = (uint16_t)t["proxy_port"].as<unsigned>();
                if (!t["use_proxy"].is<bool>())
                    use_proxy = true;
            }
            if (t["proxy_path"].is<const char *>())
            {
                proxy_override = true;
                path = t["proxy_path"].as<const char *>();
                if (!t["use_proxy"].is<bool>())
                    use_proxy = true;
            }

            if (proxy_override)
            {
                if (use_proxy && host.length() > 0)
                    _network.setTelegramProxy(host, port, path);
                else
                    _network.disableTelegramProxy();
            }

            if (t["allowed_users"].is<JsonArrayConst>())
            {
                std::array<TelegramMenu::AllowedUser, TelegramMenu::kMaxAllowedUsers> ordered{};
                std::array<bool, TelegramMenu::kMaxAllowedUsers> used{};
                std::vector<TelegramMenu::AllowedUser> tail;
                JsonArrayConst arr = t["allowed_users"].as<JsonArrayConst>();
                for (JsonVariantConst v : arr)
                {
                    if (v.is<const char *>())
                    {
                        TelegramMenu::AllowedUser u{};
                        u.username = v.as<const char *>();
                        u.is_admin = true;
                        tail.push_back(u);
                        continue;
                    }
                    if (!v.is<JsonObjectConst>())
                        continue;
                    JsonObjectConst obj = v.as<JsonObjectConst>();
                    TelegramMenu::AllowedUser u{};
                    uint8_t id = 0;
                    if (obj["id"].is<unsigned>())
                    {
                        const unsigned raw = obj["id"].as<unsigned>();
                        if (raw >= 1 && raw <= TelegramMenu::kMaxAllowedUsers)
                            id = (uint8_t)raw;
                    }
                    if (obj["username"].is<const char *>())
                        u.username = obj["username"].as<const char *>();
                    if (obj["chat_id"].is<long long>())
                        u.chat_id = (int64_t)obj["chat_id"].as<long long>();
                    if (obj["is_admin"].is<bool>())
                        u.is_admin = obj["is_admin"].as<bool>();
                    if (obj["is_notify"].is<bool>())
                        u.is_notify = obj["is_notify"].as<bool>();
                    if (obj["enabled"].is<bool>())
                        u.enabled = obj["enabled"].as<bool>();
                    if (id > 0 && !used[id - 1])
                    {
                        ordered[id - 1] = u;
                        used[id - 1] = true;
                    }
                    else
                    {
                        tail.push_back(u);
                    }
                }
                std::vector<TelegramMenu::AllowedUser> users;
                for (size_t i = 0; i < TelegramMenu::kMaxAllowedUsers; ++i)
                {
                    if (used[i])
                        users.push_back(ordered[i]);
                }
                users.insert(users.end(), tail.begin(), tail.end());
                _telegram_menu.setAllowedUsers(users);
            }
        }

        if (doc["gsm"].is<JsonObjectConst>())
        {
            JsonObjectConst g = doc["gsm"].as<JsonObjectConst>();
            if (g["enabled"].is<bool>())
                _gsm.setEnabled(g["enabled"].as<bool>());
        }

        if (doc["admin"].is<JsonObjectConst>())
        {
            JsonObjectConst a = doc["admin"].as<JsonObjectConst>();
            if (a["password_hash"].is<const char *>())
            {
                _console.setAdminPasswordHashHex_(a["password_hash"].as<const char *>());
            }
            else if (a["password"].is<const char *>())
            {
                _console.setAdminPassword_(a["password"].as<const char *>());
            }
        }

        if (doc["plc"].is<JsonObjectConst>())
        {
            JsonObjectConst p = doc["plc"].as<JsonObjectConst>();
            if (p["device_name"].is<const char *>())
            {
                String name = p["device_name"].as<const char *>();
                name = sanitizeUtf8_(name);
                _plc.setDeviceName(name);
            }
        }

        if (doc["stack"].is<JsonObjectConst>())
        {
            JsonObjectConst s = doc["stack"].as<JsonObjectConst>();
            if (s["role"].is<const char *>())
            {
                String role = s["role"].as<const char *>();
                role.toLowerCase();
                _stack_role = (role == "slave") ? StackRole::Slave : StackRole::Master;
            }
            if (s["master_host"].is<const char *>())
                _stack_master_host = s["master_host"].as<const char *>();
            if (s["api_key"].is<const char *>())
                _stack_api_key = s["api_key"].as<const char *>();
        }

        if (doc["controllers"].is<JsonObjectConst>())
        {
            _controllers.applyConfig(doc["controllers"].as<JsonObjectConst>());
        }
    }

    static String sanitizeUtf8_(const String &in)
    {
        if (isValidUtf8_(in))
            return in;
        return cp1251ToUtf8_(in);
    }

    static bool isValidUtf8_(const String &in)
    {
        size_t i = 0;
        while (i < (size_t)in.length())
        {
            const uint8_t c = (uint8_t)in[i];
            if (c < 0x80)
            {
                ++i;
                continue;
            }
            size_t need = 0;
            if ((c & 0xE0) == 0xC0)
            {
                if (c < 0xC2)
                    return false;
                need = 1;
            }
            else if ((c & 0xF0) == 0xE0)
            {
                need = 2;
            }
            else if ((c & 0xF8) == 0xF0)
            {
                if (c > 0xF4)
                    return false;
                need = 3;
            }
            else
            {
                return false;
            }

            if (i + need >= (size_t)in.length())
                return false;

            for (size_t j = 1; j <= need; ++j)
            {
                const uint8_t cc = (uint8_t)in[i + j];
                if ((cc & 0xC0) != 0x80)
                    return false;
            }
            i += need + 1;
        }
        return true;
    }

    static void appendUtf8_(String &out, uint16_t code)
    {
        if (code < 0x80)
        {
            out += (char)code;
            return;
        }
        if (code < 0x800)
        {
            out += (char)(0xC0 | (code >> 6));
            out += (char)(0x80 | (code & 0x3F));
            return;
        }
        out += (char)(0xE0 | (code >> 12));
        out += (char)(0x80 | ((code >> 6) & 0x3F));
        out += (char)(0x80 | (code & 0x3F));
    }

    static String cp1251ToUtf8_(const String &in)
    {
        String out;
        out.reserve(in.length() * 2);
        for (size_t i = 0; i < (size_t)in.length(); ++i)
        {
            const uint8_t c = (uint8_t)in[i];
            if (c < 0x80)
            {
                out += (char)c;
                continue;
            }
            uint16_t code = '?';
            if (c == 0xA8)
                code = 0x0401;
            else if (c == 0xB8)
                code = 0x0451;
            else if (c >= 0xC0 && c <= 0xFF)
                code = (uint16_t)(0x0410 + (c - 0xC0));
            else
                code = '?';
            appendUtf8_(out, code);
        }
        return out;
    }

    Configs &_configs;
    WifiManager &_wifi;
    TelegramClient &_telegram;
    Network &_network;
    CliConsole &_console;
    TelegramMenu &_telegram_menu;
    PlcControl &_plc;
    Controllers &_controllers;
    GsmModem &_gsm;
    StackRole _stack_role = StackRole::Master;
    String _stack_master_host;
    String _stack_api_key;
    DynamicJsonDocument _doc{kConfigDocCapacity};
};
