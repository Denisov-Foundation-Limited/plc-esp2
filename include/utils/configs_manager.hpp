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
#include <vector>

#include "core/cli/cli_console.hpp"
#include "core/network/network.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "core/network/wifi_manager.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"

class ConfigsManager : public ConfigsManagerIface
{
public:
    ConfigsManager(Configs &configs, WifiManager &wifi, TelegramClient &telegram,
                   Network &network, CliConsole &console, TelegramMenu &telegram_menu)
        : _configs(configs),
          _wifi(wifi),
          _telegram(telegram),
          _network(network),
          _console(console),
          _telegram_menu(telegram_menu)
    {
    }

    bool loadConfigs()
    {
        JsonDocument doc;
        if (!_configs.load(doc))
        {
            if (_configs.lastError() == Configs::Error::OpenRead)
                return true;
            return false;
        }
        applyConfig_(doc);
        return true;
    }

    bool save() override
    {
        JsonDocument doc;
        JsonObject w = doc["wifi"].to<JsonObject>();
        w["ssid"] = _wifi.ssid();
        w["password"] = _wifi.password();
        w["ap"] = _wifi.ap();
        w["ap_ssid"] = _wifi.apSsid();
        w["ap_password"] = _wifi.apPassword();

        JsonObject t = doc["telegram"].to<JsonObject>();
        t["token"] = _telegram.token();
        t["chat_id"] = (long long)_telegram.chatId();
        t["insecure"] = _telegram.insecure();
        t["client"] = _telegram.clientKindName();
        t["use_proxy"] = _telegram.useProxy();
        t["proxy_host"] = _telegram.proxyHost();
        t["proxy_port"] = (unsigned)_telegram.proxyPort();
        t["proxy_path"] = _telegram.proxyPath();
        JsonArray allowed = t["allowed_users"].to<JsonArray>();
        for (const auto &name : _telegram_menu.allowedUsers())
            allowed.add(name);

        if (_console.adminPasswordSet())
        {
            JsonObject a = doc["admin"].to<JsonObject>();
            a["password"] = _console.adminPassword();
        }

        return _configs.save(doc);
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
                if (use_proxy)
                    _network.setTelegramProxy(host, port, path);
                else
                    _network.disableTelegramProxy();
            }

            if (t["allowed_users"].is<JsonArrayConst>())
            {
                std::vector<String> users;
                JsonArrayConst arr = t["allowed_users"].as<JsonArrayConst>();
                for (JsonVariantConst v : arr)
                {
                    if (v.is<const char *>())
                        users.push_back(v.as<const char *>());
                }
                _telegram_menu.setAllowedUsers(users);
            }
        }

        if (doc["admin"].is<JsonObjectConst>())
        {
            JsonObjectConst a = doc["admin"].as<JsonObjectConst>();
            if (a["password"].is<const char *>())
                _console.setAdminPassword_(a["password"].as<const char *>());
        }
    }

    Configs &_configs;
    WifiManager &_wifi;
    TelegramClient &_telegram;
    Network &_network;
    CliConsole &_console;
    TelegramMenu &_telegram_menu;
};
