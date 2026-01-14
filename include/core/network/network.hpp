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
#if defined(ESP32)
#include <ESPAsyncWebServer.h>
#endif

#include "boards/board_profile_base.hpp"
#include "utils/logger.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/web/web_interface.hpp"

class Network
{
public:
    enum class Error : uint8_t
    {
        None = 0,
        Wifi,
        TelegramClientMissing,
        TelegramProxyInvalid,
        WebInterfaceFs
    };

    Network(Logger &logs, WifiManager &wifi, TelegramClient &tgbot, TelegramBot &bot,
            TelegramMenu &menu, WebInterface &fw, AsyncWebServer &web, WiFiClientSecure &wifi_client)
        : _logs(logs), _wifi(wifi), _tgbot(tgbot), _bot(bot), _menu(menu),
          _fw_upgrade(fw), _web(web),
          _wifi_client(wifi_client) {}

    void setTelegramClient(Client &client) { _tgbot_ext_client = &client; }
    void setTelegramClientKind(TelegramNetCfg::ClientKind kind)
    {
        _client_override = kind;
        _client_override_set = true;
    }

    void setTelegramProxy(const String &host, uint16_t port, const String &path)
    {
        _proxy_override = true;
        _proxy_use = true;
        _proxy_host = host;
        _proxy_port = port;
        _proxy_path = path;
    }

    void disableTelegramProxy()
    {
        _proxy_override = true;
        _proxy_use = false;
        _proxy_host = "";
        _proxy_port = 0;
        _proxy_path = "";
    }

    bool begin()
    {
        _last_error = Error::None;
        if (!_wifi.begin())
        {
            _last_error = Error::Wifi;
            return false;
        }
        if (!_fw_upgrade.begin())
        {
            _last_error = Error::WebInterfaceFs;
            return false;
        }
        _bot.bind(_tgbot);
        _menu.begin();
        _fw_upgrade.registerRoutes();
        _web.begin();
        if (!configureTelegram_(ActiveBoardProfile::TELEGRAM_NET))
            return false;
        _tgbot.setAutoPollIntervalMs(10000);
        _tgbot.enableAutoPoll(true, 0);
        return true;
    }

    Error lastError() const { return _last_error; }

    void loop() {}

private:
    Logger &_logs;
    WifiManager &_wifi;
    TelegramClient &_tgbot;
    TelegramBot &_bot;
    TelegramMenu &_menu;
    WebInterface &_fw_upgrade;
    AsyncWebServer &_web;
    WiFiClientSecure &_wifi_client;
    Client *_tgbot_ext_client = nullptr;
    Error _last_error = Error::None;
    bool _client_override_set = false;
    TelegramNetCfg::ClientKind _client_override = TelegramNetCfg::ClientKind::WifiSecure;
    bool _proxy_override = false;
    bool _proxy_use = false;
    String _proxy_host;
    uint16_t _proxy_port = 0;
    String _proxy_path;

    bool configureTelegram_(const TelegramNetCfg &cfg)
    {
        const bool use_proxy = _proxy_override ? _proxy_use : cfg.use_proxy;
        const char *host = _proxy_override ? _proxy_host.c_str() : cfg.proxy_host;
        const uint16_t port = _proxy_override ? _proxy_port : cfg.proxy_port;
        const char *path = _proxy_override ? _proxy_path.c_str() : cfg.proxy_path;

        if (use_proxy)
        {
            if (!host || host[0] == '\0')
            {
                _last_error = Error::TelegramProxyInvalid;
                return false;
            }
            _tgbot.setProxy(host, port, path);
        }
        else
        {
            _tgbot.clearProxy();
        }

        const TelegramNetCfg::ClientKind kind = _client_override_set ? _client_override : cfg.client;
        switch (kind)
        {
        case TelegramNetCfg::ClientKind::WifiSecure:
            _tgbot.setClientSecure(_wifi_client);
            return true;
        case TelegramNetCfg::ClientKind::TinyGsm:
            if (_tgbot_ext_client)
            {
                _tgbot.setClient(*_tgbot_ext_client, TelegramClient::ClientKind::TinyGsm, false, nullptr);
                return true;
            }
            _last_error = Error::TelegramClientMissing;
            return false;
        default:
            return true;
        }
    }
};
