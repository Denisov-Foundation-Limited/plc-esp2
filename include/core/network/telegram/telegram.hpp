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

    enum class PollState : uint8_t
    {
        Idle = 0,
        Connecting,
        Sending,
        Reading,
        Done,
        Error
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

    void setClient(Client &client, ClientKind kind, bool secure, WiFiClientSecure *secure_client = nullptr)
    {
        _client = &client;
        _client_kind = kind;
        _client_secure = secure;
        _secure_client = secure ? secure_client : nullptr;
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

    void setToken(const String &token) { _token = token; }
    const String &token() const { return _token; }

    void setChatId(int64_t chat_id) { _chat_id = chat_id; }
    int64_t chatId() const { return _chat_id; }

    void setInsecure(bool insecure) { _insecure = insecure; }
    bool insecure() const { return _insecure; }

    void setProxy(const String &host, uint16_t port, const String &path_prefix = "")
    {
        _proxy_host = host;
        _proxy_port = port;
        _proxy_path_prefix = path_prefix;
        _use_proxy = _proxy_host.length() > 0;
    }

    void clearProxy()
    {
        _proxy_host = "";
        _proxy_port = 0;
        _proxy_path_prefix = "";
        _use_proxy = false;
    }

    bool useProxy() const { return _use_proxy; }
    const String &proxyHost() const { return _proxy_host; }
    uint16_t proxyPort() const { return _proxy_port; }
    const String &proxyPath() const { return _proxy_path_prefix; }

    const String &lastError() const { return _last_error; }
    PollState pollState() const { return _poll_state; }
    bool isPolling() const { return _poll_state != PollState::Idle; }
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

    void task()
    {
        if (!isPolling())
            return;
        PollState st = pollTick(_task_updates_tmp);
        if (st == PollState::Done && _updates_handler && !_task_updates_tmp.empty())
            _updates_handler(_updates_ctx, _task_updates_tmp);
    }

    bool sendMessage(const String &text)
    {
        if (_chat_id == 0)
        {
            _last_error = F("chat_id not set");
            return false;
        }
        JsonDocument doc;
        doc["chat_id"] = _chat_id;
        doc["text"] = text;
        String payload;
        serializeJson(doc, payload);
        String response;
        return requestJson_("sendMessage", payload, response);
    }

    bool sendMessageRaw(const String &payload)
    {
        String response;
        return requestJson_("sendMessage", payload, response);
    }

    bool sendPhoto(const String &photo_url_or_id, const String &caption = "")
    {
        if (_chat_id == 0)
        {
            _last_error = F("chat_id not set");
            return false;
        }
        JsonDocument doc;
        doc["chat_id"] = _chat_id;
        doc["photo"] = photo_url_or_id;
        if (caption.length() > 0)
            doc["caption"] = caption;
        String payload;
        serializeJson(doc, payload);
        String response;
        return requestJson_("sendPhoto", payload, response);
    }

    bool sendDocument(const String &doc_url_or_id, const String &caption = "")
    {
        if (_chat_id == 0)
        {
            _last_error = F("chat_id not set");
            return false;
        }
        JsonDocument doc;
        doc["chat_id"] = _chat_id;
        doc["document"] = doc_url_or_id;
        if (caption.length() > 0)
            doc["caption"] = caption;
        String payload;
        serializeJson(doc, payload);
        String response;
        return requestJson_("sendDocument", payload, response);
    }

    bool getFilePath(const String &file_id, String &file_path)
    {
        String method = "getFile?file_id=" + file_id;
        String response;
        if (!request_("GET", method, "", "", response))
            return false;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, response);
        if (err)
        {
            _last_error = F("json parse failed");
            return false;
        }
        if (!doc["ok"].as<bool>())
        {
            _last_error = doc["description"].as<const char *>();
            return false;
        }
        const char *path = doc["result"]["file_path"] | "";
        if (!path || !path[0])
        {
            _last_error = F("file_path missing");
            return false;
        }
        file_path = path;
        return true;
    }

    bool downloadFile(const String &file_path, String &out)
    {
        if (_token.length() == 0)
        {
            _last_error = F("token not set");
            return false;
        }
        if (!_client)
        {
            _last_error = F("client not set");
            return false;
        }
        prepareClient_();
        const char *host = nullptr;
        uint16_t port = 0;
        if (!getTarget_(host, port))
            return false;
        if (!_client->connect(host, port))
        {
            _last_error = F("connect failed");
            return false;
        }

        String path = buildFilePath_(file_path);
        _client->print(F("GET "));
        _client->print(path);
        _client->println(F(" HTTP/1.1"));
        _client->print(F("Host: "));
        _client->println(host);
        _client->println(F("Connection: close"));
        _client->println();

        String status = _client->readStringUntil('\n');
        status.trim();
        if (!status.startsWith("HTTP/1.1 200"))
            _last_error = status;

        while (_client->connected())
        {
            String line = _client->readStringUntil('\n');
            if (line == "\r")
                break;
        }
        String response = _client->readString();
        _client->stop();

        if (!status.startsWith("HTTP/1.1 200"))
            return false;
        out = response;
        return true;
    }

    using DownloadHandler = bool (*)(void *ctx, const uint8_t *data, size_t len);

    bool downloadFile(const String &file_path, DownloadHandler handler, void *ctx, size_t &bytes)
    {
        bytes = 0;
        if (_token.length() == 0)
        {
            _last_error = F("token not set");
            return false;
        }
        if (!_client)
        {
            _last_error = F("client not set");
            return false;
        }
        prepareClient_();
        const char *host = nullptr;
        uint16_t port = 0;
        if (!getTarget_(host, port))
            return false;
        if (!_client->connect(host, port))
        {
            _last_error = F("connect failed");
            return false;
        }

        String path = buildFilePath_(file_path);
        _client->print(F("GET "));
        _client->print(path);
        _client->println(F(" HTTP/1.1"));
        _client->print(F("Host: "));
        _client->println(host);
        _client->println(F("Connection: close"));
        _client->println();

        String status = _client->readStringUntil('\n');
        status.trim();
        if (!status.startsWith("HTTP/1.1 200"))
            _last_error = status;

        while (_client->connected())
        {
            String line = _client->readStringUntil('\n');
            if (line == "\r")
                break;
        }

        uint8_t buf[512];
        while (_client->connected() || _client->available())
        {
            size_t avail = _client->available();
            if (avail == 0)
            {
                delay(1);
                continue;
            }
            size_t to_read = avail > sizeof(buf) ? sizeof(buf) : avail;
            size_t n = _client->read(buf, to_read);
            if (n == 0)
                continue;
            if (!handler(ctx, buf, n))
            {
                _last_error = F("write failed");
                _client->stop();
                return false;
            }
            bytes += n;
        }
        _client->stop();

        if (!status.startsWith("HTTP/1.1 200"))
            return false;
        return true;
    }

    bool getUpdates(std::vector<Update> &out, uint32_t offset = 0, uint16_t limit = 0)
    {
        String path = "getUpdates?timeout=0";
        if (offset > 0)
            path += "&offset=" + String(offset);
        if (limit > 0)
            path += "&limit=" + String(limit);
        String response;
        if (!request_("GET", path, "", "", response))
            return false;
        return parseUpdates_(response, out);
    }

    bool pollCommands(std::vector<Update> &out, uint32_t &last_update_id)
    {
        std::vector<Update> updates;
        if (!getUpdates(updates, last_update_id + 1))
            return false;
        out.clear();
        for (const auto &u : updates)
        {
            if (u.text.length() > 0 && u.text[0] == '/')
                out.push_back(u);
            if (u.update_id > last_update_id)
                last_update_id = u.update_id;
        }
        return true;
    }

    bool startLongPoll(uint16_t timeout_s, uint32_t offset = 0, uint16_t limit = 0)
    {
        if (_poll_state != PollState::Idle)
        {
            _last_error = F("poll busy");
            return false;
        }
        if (_token.length() == 0)
        {
            _last_error = F("token not set");
            return false;
        }
        _poll_timeout_s = timeout_s;
        _poll_offset = offset;
        _poll_limit = limit;
        _poll_response = "";
        _poll_state = PollState::Connecting;
        _poll_deadline_ms = millis() + (uint32_t)timeout_s * 1000UL + 5000UL;
        return true;
    }

    PollState pollTick(std::vector<Update> &out)
    {
        out.clear();
        if (_poll_state == PollState::Idle)
            return PollState::Idle;

        if (_poll_state == PollState::Connecting)
        {
            prepareClient_();
            const char *host = nullptr;
            uint16_t port = 0;
            if (!getTarget_(host, port))
            {
                _poll_state = PollState::Error;
                return _poll_state;
            }
            if (!_client->connect(host, port))
            {
                if (millis() > _poll_deadline_ms)
                {
                    _last_error = F("connect timeout");
                    _poll_state = PollState::Error;
                }
                return _poll_state;
            }
            _poll_state = PollState::Sending;
        }

        if (_poll_state == PollState::Sending)
        {
            String method = "getUpdates?timeout=" + String(_poll_timeout_s);
            if (_poll_offset > 0)
                method += "&offset=" + String(_poll_offset);
            if (_poll_limit > 0)
                method += "&limit=" + String(_poll_limit);

            String final_path = buildPath_(method);
            const char *host = _use_proxy ? _proxy_host.c_str() : "api.telegram.org";

            _client->print(F("GET "));
            _client->print(final_path);
            _client->println(F(" HTTP/1.1"));
            _client->print(F("Host: "));
            _client->println(host);
            _client->println(F("Connection: close"));
            _client->println();
            _poll_state = PollState::Reading;
        }

        if (_poll_state == PollState::Reading)
        {
            while (_client->available())
            {
                char c = (char)_client->read();
                _poll_response += c;
            }
            if (!_client->connected() && !_client->available())
            {
                _client->stop();
                _poll_state = PollState::Done;
            }
            else if (millis() > _poll_deadline_ms)
            {
                _client->stop();
                _last_error = F("poll timeout");
                _poll_state = PollState::Error;
            }
        }

        if (_poll_state == PollState::Done)
        {
            String body;
            if (!extractBody_(_poll_response, body))
            {
                _last_error = F("bad response");
                _poll_state = PollState::Error;
                return _poll_state;
            }
            std::vector<Update> parsed;
            bool ok = parseUpdates_(body, parsed);
            if (ok)
            {
                if (_updates_handler)
                {
                    out = parsed;
                }
                else
                {
                    _poll_updates = parsed;
                    _poll_has_updates = true;
                    out = _poll_updates;
                }
            }
            _poll_state = ok ? PollState::Idle : PollState::Error;
            return ok ? PollState::Done : PollState::Error;
        }

        return _poll_state;
    }

private:
    Client *_client = nullptr;
    ClientKind _client_kind = ClientKind::None;
    bool _client_secure = false;
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
    PollState _poll_state = PollState::Idle;
    uint32_t _poll_deadline_ms = 0;
    uint16_t _poll_timeout_s = 0;
    uint32_t _poll_offset = 0;
    uint16_t _poll_limit = 0;
    String _poll_response;
    std::vector<Update> _poll_updates;
    bool _poll_has_updates = false;
    std::vector<Update> _task_updates_tmp;

    void prepareClient_()
    {
        if (_client_secure && _secure_client && _insecure)
            _secure_client->setInsecure();
    }

    bool getTarget_(const char *&host, uint16_t &port)
    {
        if (!_client)
        {
            _last_error = F("client not set");
            return false;
        }
        if (_use_proxy)
        {
            if (_proxy_host.length() == 0)
            {
                _last_error = F("proxy host not set");
                return false;
            }
            host = _proxy_host.c_str();
            port = _proxy_port ? _proxy_port : 80;
            return true;
        }
        if (!_client_secure)
        {
            _last_error = F("client not secure (use proxy)");
            return false;
        }
        host = "api.telegram.org";
        port = 443;
        return true;
    }

    bool requestJson_(const String &method, const String &payload, String &response)
    {
        return request_("POST", method, "application/json", payload, response);
    }

    bool request_(const String &verb, const String &method, const String &content_type,
                  const String &payload, String &response)
    {
        if (_token.length() == 0)
        {
            _last_error = F("token not set");
            return false;
        }
        if (!_client)
        {
            _last_error = F("client not set");
            return false;
        }
        prepareClient_();
        const char *host = nullptr;
        uint16_t port = 0;
        if (!getTarget_(host, port))
            return false;
        if (!_client->connect(host, port))
        {
            _last_error = F("connect failed");
            return false;
        }

        String path = buildPath_(method);
        _client->print(verb);
        _client->print(' ');
        _client->print(path);
        _client->println(F(" HTTP/1.1"));
        _client->print(F("Host: "));
        _client->println(host);
        _client->println(F("Connection: close"));
        if (verb == "POST")
        {
            _client->print(F("Content-Type: "));
            _client->println(content_type);
            _client->print(F("Content-Length: "));
            _client->println(payload.length());
            _client->println();
            _client->print(payload);
        }
        else
        {
            _client->println();
        }

        String status = _client->readStringUntil('\n');
        status.trim();
        if (!status.startsWith("HTTP/1.1 200"))
        {
            _last_error = status;
        }

        while (_client->connected())
        {
            String line = _client->readStringUntil('\n');
            if (line == "\r")
                break;
        }
        response = _client->readString();
        _client->stop();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, response);
        if (!err && doc["ok"].is<bool>() && !doc["ok"].as<bool>())
        {
            _last_error = doc["description"].as<const char *>();
            return false;
        }
        return status.startsWith("HTTP/1.1 200");
    }

    String buildPath_(const String &method) const
    {
        String prefix = _proxy_path_prefix;
        if (prefix.length() > 0 && !prefix.startsWith("/"))
            prefix = "/" + prefix;
        if (prefix.endsWith("/") && prefix.length() > 1)
            prefix.remove(prefix.length() - 1);
        return prefix + "/bot" + _token + "/" + method;
    }

    String buildFilePath_(const String &file_path) const
    {
        String prefix = _proxy_path_prefix;
        if (prefix.length() > 0 && !prefix.startsWith("/"))
            prefix = "/" + prefix;
        if (prefix.endsWith("/") && prefix.length() > 1)
            prefix.remove(prefix.length() - 1);
        return prefix + "/file/bot" + _token + "/" + file_path;
    }

    bool parseUpdates_(const String &response, std::vector<Update> &out)
    {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, response);
        if (err)
        {
            _last_error = F("json parse failed");
            return false;
        }
        if (!doc["ok"].as<bool>())
        {
            _last_error = doc["description"].as<const char *>();
            return false;
        }
        out.clear();
        JsonArray arr = doc["result"].as<JsonArray>();
        for (JsonVariant v : arr)
        {
            Update u;
            u.update_id = v["update_id"] | 0;
            JsonObject msg = v["message"].as<JsonObject>();
            if (msg.isNull())
                continue;
            u.chat_id = msg["chat"]["id"] | 0;
            u.from = msg["from"]["username"] | "";
            u.text = msg["text"] | "";
            JsonObject doc_msg = msg["document"].as<JsonObject>();
            if (!doc_msg.isNull())
            {
                u.document_file_id = doc_msg["file_id"] | "";
                u.document_file_name = doc_msg["file_name"] | "";
                u.document_mime = doc_msg["mime_type"] | "";
                u.document_size = doc_msg["file_size"] | 0;
            }
            out.push_back(u);
        }
        return true;
    }

    static bool extractBody_(const String &resp, String &body)
    {
        int idx = resp.indexOf("\r\n\r\n");
        if (idx < 0)
            return false;
        body = resp.substring(idx + 4);
        return true;
    }
};
