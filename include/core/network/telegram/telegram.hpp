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
#include <array>
#include <vector>
#include <FS.h>

class Client;
class WiFiClientSecure;
class Logger;
#if defined(ESP8266) || defined(ESP32)
class WiFiClient;
#endif

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

    enum class PollMode : uint8_t
    {
        Short = 0,
        Long
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
    explicit TelegramClient(Logger &log);

    void setClient(Client &client, ClientKind kind, bool secure, WiFiClientSecure *secure_client = nullptr);

    void setClientSecure(WiFiClientSecure &client);

    void setUpdateHandler(UpdatesHandler handler, void *ctx);

    ClientKind clientKind() const;
    const char *clientKindName() const;
    void setClientKindHint(ClientKind kind);

    void setToken(const String &token);
    const String &token() const;

    void setChatId(int64_t chat_id);
    int64_t chatId() const;

    void setInsecure(bool insecure);
    bool insecure() const;

    void setProxy(const String &host, uint16_t port, const String &path_prefix = "");

    void clearProxy();

    bool useProxy() const;
    const String &proxyHost() const;
    uint16_t proxyPort() const;
    const String &proxyPath() const;

    const String &lastError() const;
    Logger *logger() const;
    bool isPolling() const;
    bool hasPollUpdates() const;
    bool takePollUpdates(std::vector<Update> &out);

    void enableAutoPoll(bool on, uint16_t timeout_s = 20);
    void setPollMode(PollMode mode);
    PollMode pollMode() const;
    void setAutoPollIntervalMs(uint32_t interval_ms);
    bool autoPollEnabled() const;
    uint16_t autoPollTimeoutSec() const;
    bool pollBackoffActive() const;
    uint8_t pollFailStreak() const;
    bool canRequestNow() const;

    uint32_t lastUpdateId() const;
    int64_t lastIncomingChatId() const;

    void task();

    bool sendMessage(const String &text);

    bool sendMessageRaw(const String &payload);

    bool downloadFileToString(const String &file_id, String &out, size_t max_bytes = 0);
    bool downloadFileToFs(const String &file_id, const String &path, bool overwrite = true, size_t max_bytes = 0);

#if defined(ESP8266) || defined(ESP32)
    bool sendDocumentFromBuffer(const uint8_t *data, size_t length,
                                const String &filename = String("snapshot.jpg"),
                                const String &caption = String(),
                                int64_t chat_id = 0);

    bool sendPhotoFromBuffer(const uint8_t *data, size_t length,
                             const String &filename = String("snapshot.jpg"),
                             const String &caption = String(),
                             int64_t chat_id = 0);

    bool sendDocumentFromFile(File &file,
                              const String &filename = String("snapshot.jpg"),
                              const String &caption = String(),
                              int64_t chat_id = 0);

    bool sendPhotoFromFile(File &file,
                           const String &filename = String("snapshot.jpg"),
                           const String &caption = String(),
                           int64_t chat_id = 0);

    bool sendDocumentFromFs(const String &path,
                            const String &filename = String(),
                            const String &caption = String(),
                            int64_t chat_id = 0);

    bool sendPhotoFromFs(const String &path,
                         const String &filename = String(),
                         const String &caption = String(),
                         int64_t chat_id = 0);
#endif

    bool startLongPoll(uint16_t timeout_s, uint32_t offset = 0, uint16_t limit = 0);

private:
    Client *_client = nullptr;
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
    bool _poll_has_updates = false;
    std::vector<Update> _task_updates_tmp;
    static constexpr size_t kMaxPollUpdates = 32;
    std::array<Update, kMaxPollUpdates> _poll_updates = {};
    size_t _poll_head = 0;
    size_t _poll_count = 0;
    bool _auto_poll = false;
    PollMode _poll_mode = PollMode::Long;
    uint16_t _auto_poll_timeout_s = 20;
    uint32_t _auto_poll_interval_ms = 5000;
    uint32_t _last_update_id = 0;
    int64_t _last_incoming_chat_id = 0;
    uint32_t _log_next_ms = 0;
    String _last_log_msg;
    uint8_t _poll_fail_streak = 0;
    uint32_t _poll_backoff_until_ms = 0;
    uint32_t _poll_resume_ms = 0;
    uint32_t _poll_next_attempt_ms = 0;
    bool _poll_online = false;
    bool _poll_inflight = false;

    void restartPollingClient_();

    void logError_(const String &msg);
    void registerPollError_(const String &msg);
    void clearPollBackoff_();
    static uint32_t pollBackoffMs_(uint8_t streak);

    bool sendCommand_(const __FlashStringHelper *cmd, const String &payload);
    bool pollUpdatesOnce_();
    bool fetchUpdatesJson_(String &body);
    bool apiPostJson_(const String &method, const String &payload, String *response);
    bool apiGetJson_(const String &path, String &body);
    bool apiDownload_(const String &path, String &body, size_t max_bytes);
    bool apiDownloadToFile_(const String &path, File &file, size_t max_bytes);
    bool performRequest_(const String &method, const String &target_path,
                         const char *content_type, const String *payload,
                         String &body, int &status_code, size_t max_body = 0);
    bool sendMultipartBuffer_(const String &method, const char *field_name,
                              const uint8_t *data, size_t length, const String &filename,
                              const String &caption, int64_t chat_id, const char *mime_type);
    bool sendMultipartFile_(const String &method, const char *field_name,
                            File &file, const String &filename, const String &caption,
                            int64_t chat_id, const char *mime_type);
    Client *openTransport_(bool via_proxy, uint32_t timeout_ms);
    bool parseUpdates_(const String &body);
    bool parseTelegramOk_(const String &body);
    bool storeIncomingUpdate_(const Update &upd);
    void stopTransport_();
    String makeApiPath_(const String &suffix) const;
    String makeFilePath_(const String &suffix) const;
    static String fallbackFilename_(const String &path, const String &fallback);
    static const char *guessMimeType_(const String &filename, bool photo);

#if defined(ESP8266) || defined(ESP32)
    static constexpr size_t kUploadChunkSize = 2048;
#endif

    static constexpr uint16_t kSendTimeoutMs = 1500;
    static constexpr uint32_t kPollFailDecayMs = 30000;
    static constexpr uint8_t kPollClientRestartStreak = 4;
    static constexpr size_t kDefaultBodyLimit = 24 * 1024;
    static constexpr size_t kMaxUpdateBody = 32 * 1024;
    static constexpr size_t kMaxFileMetaBody = 8 * 1024;
#if defined(ESP8266) || defined(ESP32)
    WiFiClient *_proxy_wifi_client = nullptr;
#endif
};
