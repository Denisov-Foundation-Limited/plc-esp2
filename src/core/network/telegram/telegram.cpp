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

#include "core/network/telegram/telegram.hpp"

#include <ArduinoJson.h>
#include <Client.h>
#include <WiFiClientSecure.h>
#if defined(ESP8266) || defined(ESP32)
#include <WiFiClient.h>
#endif
#include <LittleFS.h>

#include "utils/logger.hpp"

namespace
{
constexpr char kTelegramHost[] = "api.telegram.org";
constexpr uint16_t kTelegramPort = 443;

String trimHttpLine_(String line)
{
    line.trim();
    return line;
}

String pathJoin_(const String &prefix, const String &suffix)
{
    if (prefix.length() == 0)
        return suffix;
    String out = prefix;
    if (!out.startsWith("/"))
        out = "/" + out;
    while (out.endsWith("/"))
        out.remove(out.length() - 1);
    if (!suffix.startsWith("/"))
        out += '/';
    out += suffix;
    return out;
}

bool readBytesExact_(Client &client, String &out, size_t length)
{
    char buf[256];
    size_t left = length;
    while (left > 0)
    {
        const size_t chunk = left > sizeof(buf) ? sizeof(buf) : left;
        const size_t got = client.readBytes(buf, chunk);
        if (got != chunk)
            return false;
        if (!out.concat(buf, got))
            return false;
        left -= got;
    }
    return true;
}

bool readHttpBody_(Client &client, bool chunked, size_t content_length, String &body, size_t max_body)
{
    body = "";
    if (chunked)
    {
        while (true)
        {
            String len_line = trimHttpLine_(client.readStringUntil('\n'));
            if (len_line.length() == 0)
                return false;
            const uint32_t chunk_len = (uint32_t)strtoul(len_line.c_str(), nullptr, 16);
            if (chunk_len == 0)
            {
                client.readStringUntil('\n');
                return true;
            }
            if (max_body && body.length() + chunk_len > max_body)
                return false;
            if (!readBytesExact_(client, body, chunk_len))
                return false;
            client.readStringUntil('\n');
        }
    }

    if (content_length > 0)
    {
        if (max_body && content_length > max_body)
            return false;
        return readBytesExact_(client, body, content_length);
    }

    const uint32_t started = millis();
    while (client.connected() || client.available())
    {
        while (client.available())
        {
            const int c = client.read();
            if (c < 0)
                break;
            if (max_body && body.length() + 1 > max_body)
                return false;
            body += (char)c;
        }
        if ((int32_t)(millis() - started) > 5000 && !client.available())
            break;
        delay(1);
    }
    return true;
}

bool readHttpResponse_(Client &client, int &status_code, String &body, size_t max_body)
{
    String status;
    for (uint8_t i = 0; i < 4; ++i)
    {
        status = trimHttpLine_(client.readStringUntil('\n'));
        if (status.length() > 0)
            break;
    }
    if (!status.startsWith("HTTP/1.1 ") && !status.startsWith("HTTP/1.0 "))
        return false;
    status_code = status.substring(9, 12).toInt();

    bool chunked = false;
    size_t content_length = 0;
    while (true)
    {
        String line = trimHttpLine_(client.readStringUntil('\n'));
        if (line.length() == 0)
            break;
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        String key = line.substring(0, colon);
        String value = line.substring(colon + 1);
        key.toLowerCase();
        value.trim();
        if (key == "content-length")
            content_length = (size_t)strtoul(value.c_str(), nullptr, 10);
        else if (key == "transfer-encoding")
        {
            value.toLowerCase();
            chunked = value.indexOf("chunked") >= 0;
        }
    }
    return readHttpBody_(client, chunked, content_length, body, max_body);
}

String parseTelegramError_(const String &body)
{
    DynamicJsonDocument doc(body.length() + 256);
    if (deserializeJson(doc, body))
        return body.length() ? body : String("request failed");
    if (doc["description"].is<const char *>())
        return String(doc["description"].as<const char *>());
    if (doc["error_code"].is<int>())
        return String("HTTP ") + String(doc["error_code"].as<int>());
    return body.length() ? body : String("request failed");
}

String jsonStringOrEmpty_(JsonVariantConst v)
{
    if (v.is<const char *>())
        return String(v.as<const char *>());
    if (v.is<String>())
        return v.as<String>();
    return String();
}

} // namespace

TelegramClient::TelegramClient(Logger &log) : _log(&log)
{}

void TelegramClient::setClient(Client &client, ClientKind kind, bool secure, WiFiClientSecure *secure_client)
{
    _client = &client;
    _client_kind = kind;
    _secure_client = secure ? secure_client : nullptr;
    if (_secure_client && _insecure)
        _secure_client->setInsecure();
}

void TelegramClient::setClientSecure(WiFiClientSecure &client)
{
    setClient(client, ClientKind::WifiSecure, true, &client);
}

void TelegramClient::setUpdateHandler(UpdatesHandler handler, void *ctx)
{
    _updates_handler = handler;
    _updates_ctx = ctx;
}

TelegramClient::ClientKind TelegramClient::clientKind() const
{ return _client_kind; }

const char *TelegramClient::clientKindName() const
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

void TelegramClient::setClientKindHint(ClientKind kind)
{
    _client_kind = kind;
}

void TelegramClient::setToken(const String &token)
{
    _token = token;
}

const String &TelegramClient::token() const
{ return _token; }

void TelegramClient::setChatId(int64_t chat_id)
{ _chat_id = chat_id; }

int64_t TelegramClient::chatId() const
{ return _chat_id; }

void TelegramClient::setInsecure(bool insecure)
{
    _insecure = insecure;
    if (_secure_client && _insecure)
        _secure_client->setInsecure();
}

bool TelegramClient::insecure() const
{ return _insecure; }

void TelegramClient::setProxy(const String &host, uint16_t port, const String &path_prefix)
{
    _proxy_host = host;
    _proxy_port = port ? port : 80;
    _proxy_path_prefix = path_prefix;
    _use_proxy = _proxy_host.length() > 0;
}

void TelegramClient::clearProxy()
{
    _proxy_host = "";
    _proxy_port = 0;
    _proxy_path_prefix = "";
    _use_proxy = false;
}

bool TelegramClient::useProxy() const
{ return _use_proxy; }

const String &TelegramClient::proxyHost() const
{ return _proxy_host; }

uint16_t TelegramClient::proxyPort() const
{ return _proxy_port; }

const String &TelegramClient::proxyPath() const
{ return _proxy_path_prefix; }

const String &TelegramClient::lastError() const
{ return _last_error; }

Logger *TelegramClient::logger() const
{ return _log; }

bool TelegramClient::isPolling() const
{ return _poll_inflight; }

bool TelegramClient::hasPollUpdates() const
{ return _poll_has_updates; }

bool TelegramClient::takePollUpdates(std::vector<TelegramClient::Update> &out)
{
    if (!_poll_has_updates)
        return false;
    out.clear();
    out.reserve(_poll_count);
    for (size_t i = 0; i < _poll_count; ++i)
        out.push_back(_poll_updates[(_poll_head + i) % kMaxPollUpdates]);
    _poll_count = 0;
    _poll_head = 0;
    _poll_has_updates = false;
    return true;
}

void TelegramClient::enableAutoPoll(bool on, uint16_t timeout_s)
{
    _auto_poll = on;
    _auto_poll_timeout_s = timeout_s;
    if (!on)
        _poll_inflight = false;
}

void TelegramClient::setPollMode(PollMode mode)
{
    _poll_mode = mode;
}

TelegramClient::PollMode TelegramClient::pollMode() const
{
    return _poll_mode;
}

void TelegramClient::setAutoPollIntervalMs(uint32_t interval_ms)
{
    _auto_poll_interval_ms = interval_ms;
}

bool TelegramClient::autoPollEnabled() const
{ return _auto_poll; }

uint16_t TelegramClient::autoPollTimeoutSec() const
{ return _auto_poll_timeout_s; }

bool TelegramClient::pollBackoffActive() const
{
    return _poll_backoff_until_ms != 0 &&
           (int32_t)(millis() - _poll_backoff_until_ms) < 0;
}

uint8_t TelegramClient::pollFailStreak() const
{ return _poll_fail_streak; }

bool TelegramClient::canRequestNow() const
{
    return !_auto_poll || !pollBackoffActive();
}

uint32_t TelegramClient::lastUpdateId() const
{ return _last_update_id; }

int64_t TelegramClient::lastIncomingChatId() const
{ return _last_incoming_chat_id; }

void TelegramClient::task()
{
    if (!_auto_poll)
        return;
    const uint32_t now = millis();
    if (pollBackoffActive())
        return;
    if (_poll_next_attempt_ms != 0 && (int32_t)(now - _poll_next_attempt_ms) < 0)
        return;

    _poll_inflight = true;
    const bool ok = pollUpdatesOnce_();
    _poll_inflight = false;
    if (!ok)
        return;

    clearPollBackoff_();
    _poll_online = true;
    _poll_resume_ms = now;
    _poll_next_attempt_ms = now + ((_poll_mode == PollMode::Long) ? 0u : _auto_poll_interval_ms);
    if (_poll_fail_streak != 0 && _poll_resume_ms != 0 &&
        (int32_t)(now - _poll_resume_ms) >= (int32_t)kPollFailDecayMs)
    {
        _poll_fail_streak = 0;
        _poll_resume_ms = 0;
    }

    if (_task_updates_tmp.empty())
        return;
    if (_updates_handler)
        _updates_handler(_updates_ctx, _task_updates_tmp);
    _task_updates_tmp.clear();
}

bool TelegramClient::sendMessage(const String &text)
{
    if (_chat_id == 0)
    {
        _last_error = F("chat_id not set");
        return false;
    }
    DynamicJsonDocument doc(text.length() + 128);
    doc["chat_id"] = _chat_id;
    doc["text"] = text;
    String payload;
    payload.reserve(text.length() + 64);
    if (serializeJson(doc, payload) == 0)
    {
        _last_error = F("payload too large");
        return false;
    }
    return sendCommand_(F("sendMessage"), payload);
}

bool TelegramClient::sendMessageRaw(const String &payload)
{
    return sendCommand_(F("sendMessage"), payload);
}

bool TelegramClient::downloadFileToString(const String &file_id, String &out, size_t max_bytes)
{
    out = "";
    if (_token.length() == 0)
    {
        _last_error = F("token not set");
        return false;
    }
    if (file_id.length() == 0)
    {
        _last_error = F("file_id empty");
        return false;
    }

    DynamicJsonDocument req(file_id.length() + 64);
    req["file_id"] = file_id;
    String payload;
    serializeJson(req, payload);

    String meta;
    if (!apiPostJson_(String("getFile"), payload, &meta))
        return false;

    DynamicJsonDocument doc(meta.length() + 512);
    if (deserializeJson(doc, meta))
    {
        _last_error = F("getFile parse failed");
        return false;
    }
    const char *file_path = doc["result"]["file_path"];
    if (!file_path || file_path[0] == '\0')
    {
        _last_error = F("file_path missing");
        return false;
    }
    return apiDownload_(makeFilePath_(String(file_path)), out, max_bytes ? max_bytes : kDefaultBodyLimit);
}

bool TelegramClient::downloadFileToFs(const String &file_id, const String &path, bool overwrite, size_t max_bytes)
{
    if (file_id.length() == 0)
    {
        _last_error = F("file_id empty");
        return false;
    }
    if (path.length() == 0 || path.charAt(0) != '/')
    {
        _last_error = F("invalid fs path");
        return false;
    }
    if (!overwrite && LittleFS.exists(path))
    {
        _last_error = F("file already exists");
        return false;
    }

    DynamicJsonDocument req(file_id.length() + 64);
    req["file_id"] = file_id;
    String payload;
    serializeJson(req, payload);

    String meta;
    if (!apiPostJson_(String("getFile"), payload, &meta))
        return false;

    DynamicJsonDocument doc(meta.length() + 512);
    if (deserializeJson(doc, meta))
    {
        _last_error = F("getFile parse failed");
        return false;
    }
    const char *file_path = doc["result"]["file_path"];
    if (!file_path || file_path[0] == '\0')
    {
        _last_error = F("file_path missing");
        return false;
    }

    File out = LittleFS.open(path, "w");
    if (!out)
    {
        _last_error = F("LittleFS open failed");
        return false;
    }
    const bool ok = apiDownloadToFile_(makeFilePath_(String(file_path)), out, max_bytes ? max_bytes : kDefaultBodyLimit);
    out.close();
    if (!ok)
        LittleFS.remove(path);
    return ok;
}

#if defined(ESP8266) || defined(ESP32)
bool TelegramClient::sendDocumentFromBuffer(const uint8_t *data, size_t length,
                                            const String &filename,
                                            const String &caption,
                                            int64_t chat_id)
{
    return sendMultipartBuffer_(String("sendDocument"), "document", data, length,
                                filename.length() ? filename : String("snapshot.jpg"),
                                caption, chat_id, guessMimeType_(filename, false));
}

bool TelegramClient::sendPhotoFromBuffer(const uint8_t *data, size_t length,
                                         const String &filename,
                                         const String &caption,
                                         int64_t chat_id)
{
    return sendMultipartBuffer_(String("sendPhoto"), "photo", data, length,
                                filename.length() ? filename : String("snapshot.jpg"),
                                caption, chat_id, guessMimeType_(filename, true));
}

bool TelegramClient::sendDocumentFromFile(File &file,
                                          const String &filename,
                                          const String &caption,
                                          int64_t chat_id)
{
    return sendMultipartFile_(String("sendDocument"), "document", file,
                              filename.length() ? filename : String("snapshot.jpg"),
                              caption, chat_id, guessMimeType_(filename, false));
}

bool TelegramClient::sendPhotoFromFile(File &file,
                                       const String &filename,
                                       const String &caption,
                                       int64_t chat_id)
{
    return sendMultipartFile_(String("sendPhoto"), "photo", file,
                              filename.length() ? filename : String("snapshot.jpg"),
                              caption, chat_id, guessMimeType_(filename, true));
}

bool TelegramClient::sendDocumentFromFs(const String &path,
                                        const String &filename,
                                        const String &caption,
                                        int64_t chat_id)
{
    File file = LittleFS.open(path, "r");
    if (!file)
    {
        _last_error = F("LittleFS open failed");
        return false;
    }
    const String name = fallbackFilename_(path, filename.length() ? filename : String("document.bin"));
    const bool ok = sendDocumentFromFile(file, name, caption, chat_id);
    file.close();
    return ok;
}

bool TelegramClient::sendPhotoFromFs(const String &path,
                                     const String &filename,
                                     const String &caption,
                                     int64_t chat_id)
{
    File file = LittleFS.open(path, "r");
    if (!file)
    {
        _last_error = F("LittleFS open failed");
        return false;
    }
    const String name = fallbackFilename_(path, filename.length() ? filename : String("photo.jpg"));
    const bool ok = sendPhotoFromFile(file, name, caption, chat_id);
    file.close();
    return ok;
}
#endif

bool TelegramClient::startLongPoll(uint16_t timeout_s, uint32_t offset, uint16_t limit)
{
    (void)offset;
    (void)limit;
    _auto_poll_timeout_s = timeout_s;
    _poll_next_attempt_ms = 0;
    return true;
}

void TelegramClient::restartPollingClient_()
{
    stopTransport_();
    _poll_online = false;
    _poll_inflight = false;
}

void TelegramClient::logError_(const String &msg)
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

void TelegramClient::registerPollError_(const String &msg)
{
    const uint32_t now = millis();
    if (_poll_fail_streak < 0xFF)
        ++_poll_fail_streak;
    const uint32_t wait_ms = pollBackoffMs_(_poll_fail_streak);
    _poll_backoff_until_ms = now + wait_ms;
    _poll_resume_ms = 0;
    _poll_next_attempt_ms = _poll_backoff_until_ms;
    _poll_online = false;
    stopTransport_();
    if (_poll_fail_streak >= kPollClientRestartStreak)
    {
        if (_log && _log->ready())
            _log->warn(F("TGBOT"), F("Poll client restart: fail_streak: %u"), (unsigned)_poll_fail_streak);
        restartPollingClient_();
    }
    if (_log && _log->ready())
    {
        _log->warn(F("TGBOT"), F("Poll backoff: %lu ms fail_streak: %u error: %s"),
                   (unsigned long)wait_ms, (unsigned)_poll_fail_streak, msg.c_str());
    }
}

void TelegramClient::clearPollBackoff_()
{
    _poll_fail_streak = 0;
    _poll_backoff_until_ms = 0;
    _poll_resume_ms = 0;
}

uint32_t TelegramClient::pollBackoffMs_(uint8_t streak)
{
    const uint8_t clamped = (streak > 6u) ? 6u : streak;
    const uint8_t shift = clamped ? (uint8_t)(clamped - 1u) : 0u;
    return 1000u << shift;
}

bool TelegramClient::sendCommand_(const __FlashStringHelper *cmd, const String &payload)
{
    if (!canRequestNow())
    {
        _last_error = F("poll backoff active");
        logError_(String("Request skipped: ") + _last_error);
        return false;
    }
    if (!cmd)
    {
        _last_error = F("command empty");
        return false;
    }
    String method(cmd);
    String response;
    if (!apiPostJson_(method, payload, &response))
    {
        logError_(String("API error: ") + _last_error);
        return false;
    }
    return true;
}

bool TelegramClient::pollUpdatesOnce_()
{
    _task_updates_tmp.clear();
    String body;
    if (!fetchUpdatesJson_(body))
    {
        const String first_err = _last_error;
        if (first_err == "bad http response" || first_err == "connection refused")
        {
            stopTransport_();
            delay(25);
            _last_error = "";
            body = "";
            if (fetchUpdatesJson_(body))
                return parseUpdates_(body);
            if (_last_error.length() == 0)
                _last_error = first_err;
        }
        registerPollError_(_last_error.length() ? _last_error : String("poll failed"));
        return false;
    }
    if (!parseUpdates_(body))
    {
        registerPollError_(_last_error.length() ? _last_error : String("poll parse failed"));
        return false;
    }
    return true;
}

bool TelegramClient::fetchUpdatesJson_(String &body)
{
    if (_token.length() == 0)
    {
        _last_error = F("token not set");
        return false;
    }
    DynamicJsonDocument doc(256);
    doc["offset"] = _last_update_id ? (_last_update_id + 1u) : 0u;
    doc["limit"] = 10;
    doc["timeout"] = (_poll_mode == PollMode::Long)
                         ? (_auto_poll_timeout_s ? _auto_poll_timeout_s : 20)
                         : 0;
    JsonArray allowed = doc.createNestedArray("allowed_updates");
    allowed.add("message");
    String payload;
    serializeJson(doc, payload);
    return apiPostJson_(String("getUpdates"), payload, &body);
}

bool TelegramClient::apiPostJson_(const String &method, const String &payload, String *response)
{
    String body;
    int status_code = 0;
    const size_t body_limit = (method == "getFile") ? kMaxFileMetaBody : kMaxUpdateBody;
    if (!performRequest_(String("POST"), makeApiPath_(method), "application/json", &payload,
                         body, status_code, body_limit))
        return false;
    if (status_code != 200)
    {
        _last_error = parseTelegramError_(body);
        return false;
    }
    if (!parseTelegramOk_(body))
        return false;
    if (response)
        *response = body;
    return true;
}

bool TelegramClient::apiGetJson_(const String &path, String &body)
{
    int status_code = 0;
    if (!performRequest_(String("GET"), path, nullptr, nullptr, body, status_code, kDefaultBodyLimit))
        return false;
    if (status_code != 200)
    {
        _last_error = parseTelegramError_(body);
        return false;
    }
    return true;
}

bool TelegramClient::apiDownload_(const String &path, String &body, size_t max_bytes)
{
    int status_code = 0;
    if (!performRequest_(String("GET"), path, nullptr, nullptr, body, status_code, max_bytes))
        return false;
    if (status_code != 200)
    {
        _last_error = parseTelegramError_(body);
        return false;
    }
    return true;
}

bool TelegramClient::apiDownloadToFile_(const String &path, File &file, size_t max_bytes)
{
    String body;
    int status_code = 0;
    if (!performRequest_(String("GET"), path, nullptr, nullptr, body, status_code, max_bytes))
        return false;
    if (status_code != 200)
    {
        _last_error = parseTelegramError_(body);
        return false;
    }
    if (file.write((const uint8_t *)body.c_str(), body.length()) != body.length())
    {
        _last_error = F("LittleFS write failed");
        return false;
    }
    return true;
}

bool TelegramClient::performRequest_(const String &method, const String &target_path,
                                     const char *content_type, const String *payload,
                                     String &body, int &status_code, size_t max_body)
{
    body = "";
    status_code = 0;
    if (_token.length() == 0)
    {
        _last_error = F("token not set");
        return false;
    }

    const bool via_proxy = _use_proxy && _proxy_host.length() > 0;
    String connect_host = via_proxy ? _proxy_host : String(kTelegramHost);
    uint16_t connect_port = via_proxy ? (_proxy_port ? _proxy_port : 80) : kTelegramPort;
    String request_target;
    String host_header = via_proxy ? _proxy_host : String(kTelegramHost);

    if (via_proxy)
    {
        request_target = pathJoin_(_proxy_path_prefix, target_path);
    }
    else
    {
        request_target = target_path;
    }

    Client *transport = nullptr;
    if (!via_proxy)
    {
        transport = _secure_client;
        if (_secure_client && _insecure)
            _secure_client->setInsecure();
    }
    else
    {
#if defined(ESP8266) || defined(ESP32)
        if (_client_kind == ClientKind::WifiSecure)
        {
            if (!_proxy_wifi_client)
                _proxy_wifi_client = new WiFiClient();
            transport = _proxy_wifi_client;
        }
        else
#endif
        {
            transport = _client;
        }
    }

    if (!transport)
    {
        _last_error = via_proxy ? F("proxy transport missing") : F("secure client missing");
        return false;
    }

    stopTransport_();
    const uint32_t base_timeout_ms = (_poll_mode == PollMode::Long)
                                         ? (_auto_poll_timeout_s ? (_auto_poll_timeout_s * 1000u) : 20000u)
                                         : kSendTimeoutMs;
    const uint32_t request_timeout_ms = via_proxy ? max<uint32_t>(base_timeout_ms + 5000u, 10000u)
                                                  : max<uint32_t>(base_timeout_ms + 5000u, 10000u);
    transport->setTimeout(request_timeout_ms);
    if (!transport->connect(connect_host.c_str(), connect_port))
    {
        _last_error = F("connection refused");
        return false;
    }

    String req;
    req.reserve(256 + target_path.length() + (payload ? payload->length() : 0));
    req += method;
    req += ' ';
    req += request_target;
    req += F(" HTTP/1.1\r\nHost: ");
    req += host_header;
    req += F("\r\nConnection: close\r\nAccept: application/json\r\nUser-Agent: plc-esp2-tg/1\r\n");
    if (payload)
    {
        req += F("Content-Type: ");
        req += content_type ? content_type : "application/json";
        req += F("\r\nContent-Length: ");
        req += String(payload->length());
        req += F("\r\n");
    }
    req += F("\r\n");
    if (transport->print(req) != (int)req.length())
    {
        _last_error = F("request write failed");
        stopTransport_();
        return false;
    }
    if (payload && payload->length())
    {
        if (transport->print(*payload) != (int)payload->length())
        {
            _last_error = F("payload write failed");
            stopTransport_();
            return false;
        }
    }

    if (!readHttpResponse_(*transport, status_code, body, max_body ? max_body : kDefaultBodyLimit))
    {
        _last_error = F("bad http response");
        stopTransport_();
        return false;
    }
    stopTransport_();
    return true;
}

bool TelegramClient::sendMultipartBuffer_(const String &method, const char *field_name,
                                          const uint8_t *data, size_t length, const String &filename,
                                          const String &caption, int64_t chat_id, const char *mime_type)
{
    if (!data || length == 0)
    {
        _last_error = F("empty buffer");
        return false;
    }
    if (_token.length() == 0)
    {
        _last_error = F("token not set");
        return false;
    }
    const int64_t target_chat = chat_id ? chat_id : _chat_id;
    if (target_chat == 0)
    {
        _last_error = F("chat_id not set");
        return false;
    }

    const bool via_proxy = _use_proxy && _proxy_host.length() > 0;
    const uint32_t timeout_ms = 30000u;
    Client *transport = openTransport_(via_proxy, timeout_ms);
    if (!transport)
        return false;

    const String boundary = String("----plc-esp2-") + String((unsigned long)millis(), HEX);
    const String chat_str = String((long long)target_chat);
    const String prefix =
        String("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + chat_str + "\r\n" +
        (caption.length()
             ? String("--") + boundary + "\r\n"
               "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" + caption + "\r\n"
             : String()) +
        String("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"" + field_name + "\"; filename=\"" + filename + "\"\r\n"
        "Content-Type: " + (mime_type ? String(mime_type) : String("application/octet-stream")) + "\r\n\r\n";
    const String suffix = String("\r\n--") + boundary + "--\r\n";
    const size_t content_length = prefix.length() + length + suffix.length();

    String req;
    const String request_target = via_proxy ? pathJoin_(_proxy_path_prefix, makeApiPath_(method)) : makeApiPath_(method);
    const String host_header = via_proxy ? _proxy_host : String(kTelegramHost);
    req.reserve(256 + request_target.length());
    req += F("POST ");
    req += request_target;
    req += F(" HTTP/1.1\r\nHost: ");
    req += host_header;
    req += F("\r\nConnection: close\r\nAccept: application/json\r\nUser-Agent: plc-esp2-tg/1\r\nContent-Type: multipart/form-data; boundary=");
    req += boundary;
    req += F("\r\nContent-Length: ");
    req += String((unsigned long)content_length);
    req += F("\r\n\r\n");
    if (transport->print(req) != (int)req.length() ||
        transport->print(prefix) != (int)prefix.length() ||
        transport->write(data, length) != length ||
        transport->print(suffix) != (int)suffix.length())
    {
        _last_error = F("multipart write failed");
        stopTransport_();
        return false;
    }

    String body;
    int status_code = 0;
    if (!readHttpResponse_(*transport, status_code, body, kDefaultBodyLimit))
    {
        _last_error = F("bad http response");
        stopTransport_();
        return false;
    }
    stopTransport_();
    if (status_code != 200)
    {
        _last_error = parseTelegramError_(body);
        return false;
    }
    return parseTelegramOk_(body);
}

bool TelegramClient::sendMultipartFile_(const String &method, const char *field_name,
                                        File &file, const String &filename, const String &caption,
                                        int64_t chat_id, const char *mime_type)
{
    if (!file || file.size() == 0)
    {
        _last_error = F("empty file");
        return false;
    }
    if (_token.length() == 0)
    {
        _last_error = F("token not set");
        return false;
    }
    const int64_t target_chat = chat_id ? chat_id : _chat_id;
    if (target_chat == 0)
    {
        _last_error = F("chat_id not set");
        return false;
    }

    const bool via_proxy = _use_proxy && _proxy_host.length() > 0;
    const uint32_t timeout_ms = 30000u;
    Client *transport = openTransport_(via_proxy, timeout_ms);
    if (!transport)
        return false;

    const String boundary = String("----plc-esp2-") + String((unsigned long)millis(), HEX);
    const String chat_str = String((long long)target_chat);
    const String prefix =
        String("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + chat_str + "\r\n" +
        (caption.length()
             ? String("--") + boundary + "\r\n"
               "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" + caption + "\r\n"
             : String()) +
        String("--") + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"" + field_name + "\"; filename=\"" + filename + "\"\r\n"
        "Content-Type: " + (mime_type ? String(mime_type) : String("application/octet-stream")) + "\r\n\r\n";
    const String suffix = String("\r\n--") + boundary + "--\r\n";
    const size_t file_size = (size_t)file.size();
    const size_t content_length = prefix.length() + file_size + suffix.length();

    String req;
    const String request_target = via_proxy ? pathJoin_(_proxy_path_prefix, makeApiPath_(method)) : makeApiPath_(method);
    const String host_header = via_proxy ? _proxy_host : String(kTelegramHost);
    req.reserve(256 + request_target.length());
    req += F("POST ");
    req += request_target;
    req += F(" HTTP/1.1\r\nHost: ");
    req += host_header;
    req += F("\r\nConnection: close\r\nAccept: application/json\r\nUser-Agent: plc-esp2-tg/1\r\nContent-Type: multipart/form-data; boundary=");
    req += boundary;
    req += F("\r\nContent-Length: ");
    req += String((unsigned long)content_length);
    req += F("\r\n\r\n");
    if (transport->print(req) != (int)req.length() || transport->print(prefix) != (int)prefix.length())
    {
        _last_error = F("multipart header write failed");
        stopTransport_();
        return false;
    }

    uint8_t buf[kUploadChunkSize];
    size_t left = file_size;
    while (left > 0)
    {
        const size_t chunk = left > sizeof(buf) ? sizeof(buf) : left;
        const size_t got = file.read(buf, chunk);
        if (got == 0)
        {
            _last_error = F("file read failed");
            stopTransport_();
            return false;
        }
        if (transport->write(buf, got) != got)
        {
            _last_error = F("multipart body write failed");
            stopTransport_();
            return false;
        }
        left -= got;
    }
    if (transport->print(suffix) != (int)suffix.length())
    {
        _last_error = F("multipart tail write failed");
        stopTransport_();
        return false;
    }

    String body;
    int status_code = 0;
    if (!readHttpResponse_(*transport, status_code, body, kDefaultBodyLimit))
    {
        _last_error = F("bad http response");
        stopTransport_();
        return false;
    }
    stopTransport_();
    if (status_code != 200)
    {
        _last_error = parseTelegramError_(body);
        return false;
    }
    return parseTelegramOk_(body);
}

Client *TelegramClient::openTransport_(bool via_proxy, uint32_t timeout_ms)
{
    Client *transport = nullptr;
    if (!via_proxy)
    {
        transport = _secure_client;
        if (_secure_client && _insecure)
            _secure_client->setInsecure();
    }
    else
    {
#if defined(ESP8266) || defined(ESP32)
        if (_client_kind == ClientKind::WifiSecure)
        {
            if (!_proxy_wifi_client)
                _proxy_wifi_client = new WiFiClient();
            transport = _proxy_wifi_client;
        }
        else
#endif
        {
            transport = _client;
        }
    }

    if (!transport)
    {
        _last_error = via_proxy ? F("proxy transport missing") : F("secure client missing");
        return nullptr;
    }

    const String connect_host = via_proxy ? _proxy_host : String(kTelegramHost);
    const uint16_t connect_port = via_proxy ? (_proxy_port ? _proxy_port : 80) : kTelegramPort;
    stopTransport_();
    transport->setTimeout(timeout_ms);
    if (!transport->connect(connect_host.c_str(), connect_port))
    {
        _last_error = F("connection refused");
        return nullptr;
    }
    return transport;
}

bool TelegramClient::parseUpdates_(const String &body)
{
    DynamicJsonDocument doc(body.length() + 1024);
    DeserializationError err = deserializeJson(doc, body);
    if (err)
    {
        _last_error = F("updates json parse failed");
        return false;
    }
    if (!doc["ok"].as<bool>())
    {
        _last_error = jsonStringOrEmpty_(doc["description"]);
        if (_last_error.length() == 0)
            _last_error = F("telegram api error");
        return false;
    }

    JsonArrayConst results = doc["result"].as<JsonArrayConst>();
    for (JsonObjectConst item : results)
    {
        Update upd;
        upd.update_id = item["update_id"] | 0u;
        JsonObjectConst msg = item["message"].as<JsonObjectConst>();
        if (msg.isNull())
            continue;
        upd.chat_id = msg["chat"]["id"] | 0ll;
        upd.from = jsonStringOrEmpty_(msg["from"]["username"]);
        if (upd.from.length() == 0)
            upd.from = jsonStringOrEmpty_(msg["from"]["first_name"]);
        upd.text = jsonStringOrEmpty_(msg["text"]);
        JsonObjectConst doc_obj = msg["document"].as<JsonObjectConst>();
        if (!doc_obj.isNull())
        {
            upd.document_file_id = jsonStringOrEmpty_(doc_obj["file_id"]);
            upd.document_file_name = jsonStringOrEmpty_(doc_obj["file_name"]);
            upd.document_mime = jsonStringOrEmpty_(doc_obj["mime_type"]);
            upd.document_size = doc_obj["file_size"] | 0u;
        }
        if (!storeIncomingUpdate_(upd))
            return false;
    }
    return true;
}

bool TelegramClient::parseTelegramOk_(const String &body)
{
    DynamicJsonDocument doc(body.length() + 512);
    DeserializationError err = deserializeJson(doc, body);
    if (err)
    {
        _last_error = F("response json parse failed");
        return false;
    }
    if (!doc["ok"].as<bool>())
    {
        _last_error = jsonStringOrEmpty_(doc["description"]);
        if (_last_error.length() == 0)
            _last_error = F("telegram api error");
        return false;
    }
    return true;
}

bool TelegramClient::storeIncomingUpdate_(const Update &upd)
{
    if (upd.update_id > _last_update_id)
        _last_update_id = upd.update_id;
    if (upd.chat_id != 0)
        _last_incoming_chat_id = upd.chat_id;

    _task_updates_tmp.push_back(upd);
    if (!_updates_handler)
    {
        if (_poll_count < kMaxPollUpdates)
        {
            const size_t idx = (_poll_head + _poll_count) % kMaxPollUpdates;
            _poll_updates[idx] = upd;
            ++_poll_count;
        }
        else
        {
            _poll_updates[_poll_head] = upd;
            _poll_head = (_poll_head + 1) % kMaxPollUpdates;
        }
        _poll_has_updates = true;
    }
    return true;
}

void TelegramClient::stopTransport_()
{
    if (_secure_client)
        _secure_client->stop();
    if (_client && _client != static_cast<Client *>(_secure_client))
        _client->stop();
#if defined(ESP8266) || defined(ESP32)
    if (_proxy_wifi_client)
        _proxy_wifi_client->stop();
#endif
}

String TelegramClient::makeApiPath_(const String &suffix) const
{
    return String("/bot") + _token + "/" + suffix;
}

String TelegramClient::makeFilePath_(const String &suffix) const
{
    return String("/file/bot") + _token + "/" + suffix;
}

#if defined(ESP8266) || defined(ESP32)
String TelegramClient::fallbackFilename_(const String &path, const String &fallback)
{
    if (fallback.length())
        return fallback;
    const int slash = path.lastIndexOf('/');
    String name = (slash >= 0) ? path.substring((size_t)slash + 1) : path;
    name.trim();
    return name.length() ? name : String("file.bin");
}

const char *TelegramClient::guessMimeType_(const String &filename, bool photo)
{
    String lower = filename;
    lower.toLowerCase();
    if (lower.endsWith(".jpg") || lower.endsWith(".jpeg"))
        return "image/jpeg";
    if (lower.endsWith(".png"))
        return "image/png";
    if (lower.endsWith(".gif"))
        return "image/gif";
    if (lower.endsWith(".webp"))
        return "image/webp";
    if (lower.endsWith(".txt"))
        return "text/plain";
    if (lower.endsWith(".json"))
        return "application/json";
    if (lower.endsWith(".bin"))
        return "application/octet-stream";
    if (lower.endsWith(".pdf"))
        return "application/pdf";
    return photo ? "image/jpeg" : "application/octet-stream";
}
#endif
