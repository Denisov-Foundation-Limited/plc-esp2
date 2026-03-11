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

#include "utils/logger.hpp"

TelegramClient::TelegramClient(Logger &log) : _log(&log)
{}
void TelegramClient::setClient(Client &client, ClientKind kind, bool secure, WiFiClientSecure *secure_client)
{
    _client_kind = kind;
    _secure_client = secure ? secure_client : nullptr;
    initFastBot_(client);
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
FastBot2Client *TelegramClient::fastBot()
{ return _fb; }
const FastBot2Client *TelegramClient::fastBot() const
{ return _fb; }
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
void TelegramClient::setToken(const String &token)
{
    _token = token;
    if (_fb)
        _fb->setToken(token);
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
    _proxy_port = port;
    _proxy_path_prefix = path_prefix;
    _use_proxy = _proxy_host.length() > 0;
    if (_fb && _use_proxy)
        _fb->setProxy(_proxy_host.c_str(), _proxy_port);
}
void TelegramClient::clearProxy()
{
    _proxy_host = "";
    _proxy_port = 0;
    _proxy_path_prefix = "";
    _use_proxy = false;
    if (_fb)
        _fb->clearProxy();
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
bool TelegramClient::isPolling() const
{ return _fb ? _fb->isPolling() : false; }
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
    if (_fb)
        applyPollConfig_();
}
void TelegramClient::setAutoPollIntervalMs(uint32_t interval_ms)
{
    _auto_poll_interval_ms = interval_ms;
    if (_fb)
        applyPollConfig_();
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
    if (!_auto_poll || !_fb)
        return;
    const uint32_t now = millis();
    if (pollBackoffActive())
    {
        if (_poll_online)
        {
            _fb->setOnline(false);
            _poll_online = false;
        }
        return;
    }
    const bool was_polling = _fb->isPolling();
    if (!was_polling && _poll_next_attempt_ms != 0 &&
        (int32_t)(now - _poll_next_attempt_ms) < 0)
    {
        return;
    }
    if (!_poll_online)
    {
        _fb->setOnline(true);
        _poll_online = true;
        _poll_resume_ms = now;
        _poll_next_attempt_ms = 0;
    }
    _task_updates_tmp.clear();
    _fb->tick();
    const bool now_polling = _fb->isPolling();
    if (!was_polling)
    {
        if (now_polling)
        {
            _poll_next_attempt_ms = 0;
        }
        else if (_task_updates_tmp.empty())
        {
            registerPollError_(F("poll start failed"));
            return;
        }
        else
        {
            clearPollBackoff_();
            _poll_next_attempt_ms = now + _auto_poll_interval_ms;
        }
    }
    else if (!now_polling)
    {
        clearPollBackoff_();
        _poll_next_attempt_ms = now + _auto_poll_interval_ms;
    }
    if (_poll_fail_streak != 0 && _poll_resume_ms != 0 &&
        (int32_t)(now - _poll_resume_ms) >= (int32_t)kPollFailDecayMs)
    {
        _poll_fail_streak = 0;
        _poll_resume_ms = 0;
    }
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
#if !defined(FB_NO_FILE) && (defined(ESP8266) || defined(ESP32))
bool TelegramClient::sendDocumentFromBuffer(const uint8_t *data, size_t length,
                            const String &filename,
                            const String &caption,
                            int64_t chat_id)
{
    if (!_fb)
    {
        _last_error = F("bot not set");
        logError_(String("Request failed: ") + _last_error);
        return false;
    }
    if (!data || length == 0)
    {
        _last_error = F("empty buffer");
        return false;
    }
    const int64_t target_chat = chat_id ? chat_id : _chat_id;
    if (target_chat == 0)
    {
        _last_error = F("chat_id not set");
        return false;
    }
    fb::File msg(filename, fb::File::Type::document, data, length, false);
    msg.multipart.setBlockSize(kUploadBlockSize);
    msg.chatID = fb::ID((long long)target_chat);
    msg.caption = caption;
    return sendFile_(msg);
}
bool TelegramClient::sendPhotoFromBuffer(const uint8_t *data, size_t length,
                         const String &filename,
                         const String &caption,
                         int64_t chat_id)
{
    if (!_fb)
    {
        _last_error = F("bot not set");
        logError_(String("Request failed: ") + _last_error);
        return false;
    }
    if (!data || length == 0)
    {
        _last_error = F("empty buffer");
        return false;
    }
    const int64_t target_chat = chat_id ? chat_id : _chat_id;
    if (target_chat == 0)
    {
        _last_error = F("chat_id not set");
        return false;
    }
    fb::File msg(filename, fb::File::Type::photo, data, length, false);
    msg.multipart.setBlockSize(kUploadBlockSize);
    msg.chatID = fb::ID((long long)target_chat);
    msg.caption = caption;
    return sendFile_(msg);
}
bool TelegramClient::sendDocumentFromFile(File &file,
                          const String &filename,
                          const String &caption,
                          int64_t chat_id)
{
    if (!_fb)
    {
        _last_error = F("bot not set");
        logError_(String("Request failed: ") + _last_error);
        return false;
    }
    if (!file || file.size() == 0)
    {
        _last_error = F("empty file");
        return false;
    }
    const int64_t target_chat = chat_id ? chat_id : _chat_id;
    if (target_chat == 0)
    {
        _last_error = F("chat_id not set");
        return false;
    }
    fb::File msg(filename, fb::File::Type::document, file);
    msg.multipart.setBlockSize(kUploadBlockSize);
    msg.chatID = fb::ID((long long)target_chat);
    msg.caption = caption;
    return sendFile_(msg);
}
bool TelegramClient::sendPhotoFromFile(File &file,
                       const String &filename,
                       const String &caption,
                       int64_t chat_id)
{
    if (!_fb)
    {
        _last_error = F("bot not set");
        logError_(String("Request failed: ") + _last_error);
        return false;
    }
    if (!file || file.size() == 0)
    {
        _last_error = F("empty file");
        return false;
    }
    const int64_t target_chat = chat_id ? chat_id : _chat_id;
    if (target_chat == 0)
    {
        _last_error = F("chat_id not set");
        return false;
    }
    fb::File msg(filename, fb::File::Type::photo, file);
    msg.multipart.setBlockSize(kUploadBlockSize);
    msg.chatID = fb::ID((long long)target_chat);
    msg.caption = caption;
    return sendFile_(msg);
}
#endif
bool TelegramClient::startLongPoll(uint16_t timeout_s, uint32_t offset, uint16_t limit)
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
void TelegramClient::initFastBot_(Client &client)
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
                         registerPollError_(msg);
                         logError_(String("Poll error: ") + msg);
                     });
    if (_use_proxy && _proxy_host.length())
        _fb->setProxy(_proxy_host.c_str(), _proxy_port);
    _fb->begin();
    applyPollConfig_();
}
void TelegramClient::applyPollConfig_()
{
    if (!_fb)
        return;
    const uint16_t prd = (uint16_t)min<uint32_t>(_auto_poll_interval_ms, 60000u);
    _fb->setPollMode(fb::Poll::Long, prd);
    const uint32_t timeout_ms = _auto_poll_timeout_s ? (_auto_poll_timeout_s * 1000u) : 2000u;
    _fb->setTimeout((uint16_t)min<uint32_t>(timeout_ms, 60000u));
    _poll_online = _auto_poll && !pollBackoffActive();
    _fb->setOnline(_poll_online);
}
void TelegramClient::onFastBotUpdate_(fb::Update &upd)
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
    if (out.chat_id != 0)
        _last_incoming_chat_id = out.chat_id;
    clearPollBackoff_();
    _task_updates_tmp.push_back(out);
    if (!_updates_handler)
    {
        if (_poll_count < kMaxPollUpdates)
        {
            const size_t idx = (_poll_head + _poll_count) % kMaxPollUpdates;
            _poll_updates[idx] = out;
            ++_poll_count;
        }
        else
        {
            _poll_updates[_poll_head] = out;
            _poll_head = (_poll_head + 1) % kMaxPollUpdates;
        }
        _poll_has_updates = true;
    }
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
    if (_fb && _poll_online)
    {
        _fb->setOnline(false);
        _poll_online = false;
    }
    if (_secure_client)
        _secure_client->stop();
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
    _poll_next_attempt_ms = 0;
}
uint32_t TelegramClient::pollBackoffMs_(uint8_t streak)
{
    const uint8_t clamped = (streak > 6u) ? 6u : streak;
    const uint8_t shift = clamped ? (uint8_t)(clamped - 1u) : 0u;
    return 1000u << shift;
}
bool TelegramClient::sendCommand_(const __FlashStringHelper *cmd, const String &payload)
{
    if (!_fb)
    {
        _last_error = F("bot not set");
        logError_(String("Request failed: ") + _last_error);
        return false;
    }
    if (!canRequestNow())
    {
        _last_error = F("poll backoff active");
        logError_(String("Request skipped: ") + _last_error);
        return false;
    }
    // Keep synchronous API calls short to avoid blocking the main control loop.
    const uint16_t prev_timeout_ms = (uint16_t)min<uint32_t>(_auto_poll_timeout_s ? (_auto_poll_timeout_s * 1000u) : 2000u, 60000u);
    _fb->setTimeout(kSendTimeoutMs);
    fb::Result res = _fb->sendCommand(cmd, payload, true);
    _fb->setTimeout(prev_timeout_ms);
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
#if !defined(FB_NO_FILE) && (defined(ESP8266) || defined(ESP32))
bool TelegramClient::sendFile_(const fb::File &msg)
{
    if (!canRequestNow())
    {
        _last_error = F("poll backoff active");
        logError_(String("Request skipped: ") + _last_error);
        return false;
    }
    // File uploads may take significantly longer than simple JSON commands.
    const uint16_t prev_timeout_ms = (uint16_t)min<uint32_t>(_auto_poll_timeout_s ? (_auto_poll_timeout_s * 1000u) : 2000u, 60000u);
    _fb->setTimeout(kSendFileTimeoutMs);
    fb::Result res = _fb->sendFile(msg, true);
    _fb->setTimeout(prev_timeout_ms);
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
#endif
