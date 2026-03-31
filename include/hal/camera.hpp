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
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <stdint.h>

class Logger;

class Camera
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        Busy,
        InvalidArg,
        NoData,
        PsramAlloc,
        HttpBegin,
        HttpStatus,
        HttpRead,
        TooLarge,
        InvalidJpeg,
        UploadFailed,
        Internal
    };

    enum class Op : uint8_t
    {
        None = 0,
        Download,
        Upload
    };

    struct Snapshot
    {
        bool busy = false;
        bool ok = false;
        Error error = Error::Ok;
        Op op = Op::None;
        size_t size = 0;
        size_t capacity = 0;
        int http_code = 0;
        uint32_t started_ms = 0;
        uint32_t finished_ms = 0;
        String url;
        String content_type;
        String error_text;
    };

    static constexpr size_t kDefaultMaxPhotoSize = 2u * 1024u * 1024u;

    explicit Camera(Logger &logs);
    ~Camera();

    bool startDownload(const String &url, size_t max_bytes = kDefaultMaxPhotoSize);
    bool startUpload(const String &url, const String &content_type = String(), const String &api_key = String());
    bool clear();

    bool busy() const;
    bool ok() const;
    size_t size() const;
    size_t capacity() const;
    Error lastError() const;
    const String &contentType() const;
    const String &lastUrl() const;
    const String &lastErrorText() const;
    int lastHttpCode() const;
    bool snapshot(Snapshot &out) const;
    bool saveToFs(fs::FS &fs, const char *path) const;

    static const char *errorName(Error err);
    static const char *opName(Op op);

private:
    static void taskEntry_(void *arg);
    void runTask_();
    void runDownload_();
    void runUpload_();
    void completeFailure_(Error err, const String &text, int http_code = 0);
    void completeSuccess_(Op op, int http_code = 200);
    void freeBuffer_();
    bool setBusy_(Op op, const String &url, size_t max_bytes, const String &content_type);

    Logger &_logs;
    SemaphoreHandle_t _mtx = nullptr;
    TaskHandle_t _task = nullptr;
    uint8_t *_buf = nullptr;
    size_t _size = 0;
    size_t _capacity = 0;
    size_t _max_bytes = kDefaultMaxPhotoSize;
    int _http_code = 0;
    uint32_t _started_ms = 0;
    uint32_t _finished_ms = 0;
    bool _busy = false;
    bool _ok = false;
    Error _error = Error::Ok;
    Op _op = Op::None;
    String _url;
    String _content_type;
    String _api_key;
    String _error_text;
};
