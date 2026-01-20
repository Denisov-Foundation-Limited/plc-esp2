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
#include <Client.h>
#include <WiFiClientSecure.h>
#include <vector>

#include <FastBot2Client.h>
#include "utils/logger.hpp"

class TelegramClient
{
public:
    enum class ClientKind : uint8_t
    {
        None = 0,
        WifiSecure,
        TinyGsm,
        Generic
    };

    struct Update
    {
        uint32_t update_id = 0;
        int64_t chat_id = 0;
        String from;
        String text;
        String document_file_id;
        String document_file_name;
        String document_mime;
        uint32_t document_size = 0;

        bool hasDocument() const { return document_file_id.length() > 0; }
    };

    using UpdatesHandler = void (*)(void *ctx, const std::vector<Update> &updates);

    TelegramClient() = default;
    explicit TelegramClient(Logger &log) : _log(&log) {}

    void setClient(Client &client, ClientKind kind, bool secure, WiFiClientSecure *secure_client = nullptr)
    {
        _client_kind = kind;
        _secure_client = secure ? secure_client : nullptr;
        initFastBot_(client);
    }

    void setClientSecure(WiFiClientSecure &client)
    {
        setClient(client, ClientKind::WifiSecure, true, &client);
    }

    void setUpdateHandler(UpdatesHandler handler, void *ctx)
    {
        _updates_handler = handler;
        _updates_ctx = ctx;
    }

    FastBot2Client *fastBot() { return _fb; }
    const FastBot2Client *fastBot() const { return _fb; }

    ClientKind clientKind() const { return _client_kind; }
    const char *clientKindName() const
    {
        switch (_client_kind)
        {
        case ClientKind::WifiSecure:
            return "wifi_secure";
        case ClientKind::TinyGsm:
            return "tinygsm";
        case ClientKind::Generic:
            return "generic";
        default:
            return "none";
        }
    }

    void setToken(const String &token)
    {
        _token = token;
        if (_fb)
            _fb->setToken(token);
    }
    const String &token() const { return _token; }

    void setChatId(int64_t chat_id) { _chat_id = chat_id; }
    int64_t chatId() const { return _chat_id; }

    void setInsecure(bool insecure)
    {
        _insecure = insecure;
        if (_secure_client && _insecure)
            _secure_client->setInsecure();
    }
    bool insecure() const { return _insecure; }

    void setProxy(const String &host, uint16_t port, const String &path_prefix = "")
    {
        _proxy_host = host;
        _proxy_port = port;
        _proxy_path_prefix = path_prefix;
        _use_proxy = _proxy_host.length() > 0;
        if (_fb && _use_proxy)
            _fb->setProxy(_proxy_host.c_str(), _proxy_port);
    }

    void clearProxy()
    {
        _proxy_host = "";
        _proxy_port = 0;
        _proxy_path_prefix = "";
        _use_proxy = false;
        if (_fb)
            _fb->clearProxy();
    }

    bool useProxy() const { return _use_proxy; }
    const String &proxyHost() const { return _proxy_host; }
    uint16_t proxyPort() const { return _proxy_port; }
    const String &proxyPath() const { return _proxy_path_prefix; }

    const String &lastError() const { return _last_error; }
    bool isPolling() const { return _fb ? _fb->isPolling() : false; }
    bool hasPollUpdates() const { return _poll_has_updates; }
    bool takePollUpdates(std::vector<Update> &out)
    {
        if (!_poll_has_updates)
            return false;
        out = _poll_updates;
        _poll_updates.clear();
        _poll_has_updates = false;
        return true;
    }

    void enableAutoPoll(bool on, uint16_t timeout_s = 20)
    {
        _auto_poll = on;
        _auto_poll_timeout_s = timeout_s;
        if (_fb)
            applyPollConfig_();
    }
    void setAutoPollIntervalMs(uint32_t interval_ms)
    {
        _auto_poll_interval_ms = interval_ms;
        if (_fb)
            applyPollConfig_();
    }

    uint32_t lastUpdateId() const { return _last_update_id; }

    void task()
    {
        if (!_auto_poll || !_fb)
            return;
        _task_updates_tmp.clear();
        _fb->tick();
        if (_fb->canReboot() && !_reboot_logged)
        {
            _reboot_logged = true;
            if (_log)
                _log->info(F("TGBOT"), F("FastBot2 OTA reboot requested"));
        }
        if (_task_updates_tmp.empty())
            return;
        if (_updates_handler)
            _updates_handler(_updates_ctx, _task_updates_tmp);
        _task_updates_tmp.clear();
    }

    bool sendMessage(const String &text)
    {
        if (_chat_id == 0)
        {
            _last_error = F("chat_id not set");
            return false;
        }
        StaticJsonDocument<256> doc;
        doc["chat_id"] = _chat_id;
        doc["text"] = text;
        String payload;
        payload.reserve(text.length() + 64);
        serializeJson(doc, payload);
        return sendCommand_(F("sendMessage"), payload);
    }

    bool sendMessageRaw(const String &payload)
    {
        return sendCommand_(F("sendMessage"), payload);
    }

    bool startLongPoll(uint16_t timeout_s, uint32_t offset = 0, uint16_t limit = 0)
    {
        if (!_fb)
        {
            _last_error = F("bot not set");
            return false;
        }
        (void)timeout_s;
        (void)offset;
        (void)limit;
        _fb->getUpdates(false, true);
        return true;
    }

private:
    ClientKind _client_kind = ClientKind::None;
    WiFiClientSecure *_secure_client = nullptr;
    UpdatesHandler _updates_handler = nullptr;
    void *_updates_ctx = nullptr;
    String _token;
    int64_t _chat_id = 0;
    bool _insecure = true;
    String _last_error;
    bool _use_proxy = false;
    String _proxy_host;
    uint16_t _proxy_port = 0;
    String _proxy_path_prefix;
    Logger *_log = nullptr;
    FastBot2Client *_fb = nullptr;
    Client *_fb_client = nullptr;
    alignas(FastBot2Client) uint8_t _fb_storage[sizeof(FastBot2Client)] = {};
    std::vector<Update> _poll_updates;
    bool _poll_has_updates = false;
    std::vector<Update> _task_updates_tmp;
    bool _auto_poll = false;
    uint16_t _auto_poll_timeout_s = 20;
    uint32_t _auto_poll_interval_ms = 5000;
    uint32_t _last_update_id = 0;
    uint32_t _log_next_ms = 0;
    String _last_log_msg;
    bool _reboot_logged = false;

    void initFastBot_(Client &client)
    {
        if (_fb_client == &client && _fb)
            return;
        if (_fb)
        {
            _fb->~FastBot2Client();
            _fb = nullptr;
        }
        _fb_client = &client;
        if (_secure_client && _insecure)
            _secure_client->setInsecure();
        _fb = new (_fb_storage) FastBot2Client(client, _token);
        _fb->onUpdate([this](fb::Update &u)
                      { onFastBotUpdate_(u); });
        _fb->attachError([this](Text err)
                         {
                             String msg;
                             err.toString(msg);
                             _last_error = msg;
                             logError_(String("Poll error: ") + msg);
                         });
        if (_use_proxy && _proxy_host.length())
            _fb->setProxy(_proxy_host.c_str(), _proxy_port);
        _fb->begin();
        applyPollConfig_();
    }

    void applyPollConfig_()
    {
        if (!_fb)
            return;
        const uint16_t prd = (uint16_t)min<uint32_t>(_auto_poll_interval_ms, 60000u);
        _fb->setPollMode(fb::Poll::Long, prd);
        const uint32_t timeout_ms = _auto_poll_timeout_s ? (_auto_poll_timeout_s * 1000u) : 2000u;
        _fb->setTimeout((uint16_t)min<uint32_t>(timeout_ms, 60000u));
        _fb->setOnline(_auto_poll);
    }

    void onFastBotUpdate_(fb::Update &upd)
    {
        if (!upd.isMessage())
            return;
        fb::MessageRead msg = upd.message();
        Update out;
        out.update_id = upd.id();
        String chat_id_str;
        msg.chat().id().toString(chat_id_str);
        out.chat_id = (int64_t)strtoll(chat_id_str.c_str(), nullptr, 10);
        msg.from().username().toString(out.from);
        msg.text().toString(out.text);
        if (msg.hasDocument())
        {
            fb::DocumentRead doc = msg.document();
            doc.id().toString(out.document_file_id);
            doc.name().toString(out.document_file_name);
            doc.type().toString(out.document_mime);
            String size_str;
            doc.size().toString(size_str);
            out.document_size = (uint32_t)strtoul(size_str.c_str(), nullptr, 10);
        }
        if (out.update_id > _last_update_id)
            _last_update_id = out.update_id;
        _task_updates_tmp.push_back(out);
        if (!_updates_handler)
        {
            _poll_updates.push_back(out);
            _poll_has_updates = true;
        }
    }

    void logError_(const String &msg)
    {
        if (!_log || !_log->ready())
            return;
        const uint32_t now = millis();
        if (msg == _last_log_msg && _log_next_ms != 0 && (int32_t)(now - _log_next_ms) < 0)
            return;
        _last_log_msg = msg;
        _log_next_ms = now + 5000;
        _log->error(F("TGBOT"), F("%s"), msg.c_str());
    }

    bool sendCommand_(const __FlashStringHelper *cmd, const String &payload)
    {
        if (!_fb)
        {
            _last_error = F("bot not set");
            logError_(String("Request failed: ") + _last_error);
            return false;
        }
        fb::Result res = _fb->sendCommand(cmd, payload, true);
        if (res.isError())
        {
            String err;
            res.getError().toString(err);
            if (!err.length())
                err = "unknown error";
            _last_error = err;
            logError_(String("API error: ") + _last_error);
            return false;
        }
        return !res.isEmpty();
    }

};
