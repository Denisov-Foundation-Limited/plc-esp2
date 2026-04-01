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

#include "hal/camera.hpp"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <memory>
#include <esp_heap_caps.h>

#include "utils/logger.hpp"

namespace
{
static constexpr size_t kChunkSize = 8192u;
static constexpr size_t kInitialCap = 64u * 1024u;
static constexpr uint32_t kReadTimeoutMs = 5000u;
static constexpr uint32_t kHttpTimeoutMs = 15000u;
}

Camera::Camera(Logger &logs) : _logs(logs)
{
    _mtx = xSemaphoreCreateMutex();
}

Camera::~Camera()
{
    if (_task)
        vTaskDelete(_task);
    freeBuffer_();
    if (_mtx)
        vSemaphoreDelete(_mtx);
}

bool Camera::startDownload(const String &url, size_t max_bytes)
{
    if (!(url.startsWith("http://") || url.startsWith("https://")) || max_bytes == 0)
    {
        if (_mtx)
            xSemaphoreTake(_mtx, portMAX_DELAY);
        _error = Error::InvalidArg;
        _error_text = F("Only http:// or https:// URLs supported");
        if (_mtx)
            xSemaphoreGive(_mtx);
        return false;
    }
    if (!setBusy_(Op::Download, url, max_bytes, String()))
        return false;
    BaseType_t ok = xTaskCreatePinnedToCore(&Camera::taskEntry_, "camera", 8192, this, 1, &_task, tskNO_AFFINITY);
    if (ok != pdPASS)
    {
        completeFailure_(Error::Internal, F("Task start failed"));
        return false;
    }
    return true;
}

bool Camera::startUpload(const String &url, const String &content_type, const String &api_key)
{
    if (!(url.startsWith("http://") || url.startsWith("https://")))
    {
        if (_mtx)
            xSemaphoreTake(_mtx, portMAX_DELAY);
        _error = Error::InvalidArg;
        _error_text = F("Only http:// or https:// URLs supported");
        if (_mtx)
            xSemaphoreGive(_mtx);
        return false;
    }
    if (_mtx)
        xSemaphoreTake(_mtx, portMAX_DELAY);
    const bool have_data = (_buf != nullptr && _size > 0);
    if (_mtx)
        xSemaphoreGive(_mtx);
    if (!have_data)
    {
        if (_mtx)
            xSemaphoreTake(_mtx, portMAX_DELAY);
        _error = Error::NoData;
        _error_text = F("No photo data");
        if (_mtx)
            xSemaphoreGive(_mtx);
        return false;
    }
    if (!setBusy_(Op::Upload, url, _max_bytes, content_type))
        return false;
    if (_mtx)
        xSemaphoreTake(_mtx, portMAX_DELAY);
    _api_key = api_key;
    if (_mtx)
        xSemaphoreGive(_mtx);
    BaseType_t ok = xTaskCreatePinnedToCore(&Camera::taskEntry_, "camera", 8192, this, 1, &_task, tskNO_AFFINITY);
    if (ok != pdPASS)
    {
        completeFailure_(Error::Internal, F("Task start failed"));
        return false;
    }
    return true;
}

bool Camera::clear()
{
    if (_mtx)
        xSemaphoreTake(_mtx, portMAX_DELAY);
    if (_busy)
    {
        if (_mtx)
            xSemaphoreGive(_mtx);
        return false;
    }
    freeBuffer_();
    _ok = false;
    _error = Error::Ok;
    _error_text = "";
    _content_type = "";
    _http_code = 0;
    _url = "";
    _op = Op::None;
    _api_key = "";
    _started_ms = 0;
    _finished_ms = 0;
    if (_mtx)
        xSemaphoreGive(_mtx);
    return true;
}

bool Camera::busy() const
{
    Snapshot snap{};
    return snapshot(snap) && snap.busy;
}

bool Camera::ok() const
{
    Snapshot snap{};
    return snapshot(snap) && snap.ok;
}

size_t Camera::size() const
{
    Snapshot snap{};
    return snapshot(snap) ? snap.size : 0u;
}

size_t Camera::capacity() const
{
    Snapshot snap{};
    return snapshot(snap) ? snap.capacity : 0u;
}

Camera::Error Camera::lastError() const
{
    Snapshot snap{};
    return snapshot(snap) ? snap.error : Error::Internal;
}

const String &Camera::contentType() const { return _content_type; }
const String &Camera::lastUrl() const { return _url; }
const String &Camera::lastErrorText() const { return _error_text; }
int Camera::lastHttpCode() const { return _http_code; }

bool Camera::snapshot(Snapshot &out) const
{
    if (!_mtx)
        return false;
    xSemaphoreTake(_mtx, portMAX_DELAY);
    out.busy = _busy;
    out.ok = _ok;
    out.error = _error;
    out.op = _op;
    out.size = _size;
    out.capacity = _capacity;
    out.http_code = _http_code;
    out.started_ms = _started_ms;
    out.finished_ms = _finished_ms;
    out.url = _url;
    out.content_type = _content_type;
    out.error_text = _error_text;
    xSemaphoreGive(_mtx);
    return true;
}

bool Camera::saveToFs(fs::FS &fs, const char *path) const
{
    if (!path || !path[0] || !_mtx)
        return false;
    xSemaphoreTake(_mtx, portMAX_DELAY);
    const uint8_t *buf = _buf;
    const size_t size = _size;
    xSemaphoreGive(_mtx);
    if (!buf || size == 0)
        return false;
    File f = fs.open(path, "w");
    if (!f)
        return false;
    const size_t written = f.write(buf, size);
    f.close();
    return written == size;
}

const char *Camera::errorName(Error err)
{
    switch (err)
    {
    case Error::Ok:
        return "ok";
    case Error::Busy:
        return "busy";
    case Error::InvalidArg:
        return "invalid_arg";
    case Error::NoData:
        return "no_data";
    case Error::PsramAlloc:
        return "psram_alloc";
    case Error::HttpBegin:
        return "http_begin";
    case Error::HttpStatus:
        return "http_status";
    case Error::HttpRead:
        return "http_read";
    case Error::TooLarge:
        return "too_large";
    case Error::InvalidJpeg:
        return "invalid_jpeg";
    case Error::UploadFailed:
        return "upload_failed";
    case Error::Internal:
        return "internal";
    }
    return "unknown";
}

const char *Camera::opName(Op op)
{
    switch (op)
    {
    case Op::None:
        return "none";
    case Op::Download:
        return "download";
    case Op::Upload:
        return "upload";
    }
    return "unknown";
}

void Camera::taskEntry_(void *arg)
{
    if (!arg)
        vTaskDelete(nullptr);
    static_cast<Camera *>(arg)->runTask_();
    vTaskDelete(nullptr);
}

void Camera::runTask_()
{
    Snapshot snap{};
    if (!snapshot(snap))
    {
        completeFailure_(Error::Internal, F("Snapshot failed"));
        return;
    }
    _logs.info(F("CAMERA"), F("Task start: op: %s url: %s"), opName(snap.op), snap.url.c_str());
    switch (snap.op)
    {
    case Op::Download:
        runDownload_();
        break;
    case Op::Upload:
        runUpload_();
        break;
    case Op::None:
    default:
        completeFailure_(Error::Internal, F("No operation"));
        break;
    }
}

void Camera::runDownload_()
{
    String url;
    size_t max_bytes = 0;
    if (_mtx)
        xSemaphoreTake(_mtx, portMAX_DELAY);
    url = _url;
    max_bytes = _max_bytes;
    if (_mtx)
        xSemaphoreGive(_mtx);

    const uint32_t t0_ms = millis();
    HTTPClient http;
    std::unique_ptr<WiFiClientSecure> secure;
    http.setTimeout(kHttpTimeoutMs);
    if (url.startsWith("https://"))
    {
        secure.reset(new WiFiClientSecure());
        if (!secure)
        {
            completeFailure_(Error::Internal, F("TLS client alloc failed"));
            return;
        }
        secure->setInsecure();
        if (!http.begin(*secure, url))
        {
            completeFailure_(Error::HttpBegin, F("HTTP begin failed"));
            return;
        }
    }
    else if (!http.begin(url))
    {
        completeFailure_(Error::HttpBegin, F("HTTP begin failed"));
        return;
    }
    const uint32_t begin_done_ms = millis();
    const int code = http.GET();
    const uint32_t get_done_ms = millis();
    if (code != HTTP_CODE_OK)
    {
        completeFailure_(Error::HttpStatus, String(F("HTTP failed: ")) + String(code), code);
        http.end();
        return;
    }

    String content_type = http.header("Content-Type");
    const int len = http.getSize();
    if (len > 0 && (size_t)len > max_bytes)
    {
        completeFailure_(Error::TooLarge, F("Photo too large"), code);
        http.end();
        return;
    }

    size_t capacity = (len > 0) ? (size_t)len : kInitialCap;
    uint8_t *buf = static_cast<uint8_t *>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buf)
    {
        completeFailure_(Error::PsramAlloc, F("PSRAM alloc failed"), code);
        http.end();
        return;
    }

    size_t size = 0;
    WiFiClient *stream = http.getStreamPtr();
    uint8_t *chunk = static_cast<uint8_t *>(heap_caps_malloc(kChunkSize, MALLOC_CAP_8BIT));
    if (!chunk)
    {
        heap_caps_free(buf);
        completeFailure_(Error::Internal, F("Chunk alloc failed"), code);
        http.end();
        return;
    }
    uint32_t last_data_ms = millis();
    uint32_t first_byte_ms = 0;
    bool got_first_byte = false;
    while (http.connected() || (stream && stream->available() > 0))
    {
        const int avail = stream ? stream->available() : 0;
        if (avail <= 0)
        {
            if ((uint32_t)(millis() - last_data_ms) > kReadTimeoutMs)
            {
                heap_caps_free(chunk);
                heap_caps_free(buf);
                completeFailure_(Error::HttpRead, F("HTTP read timeout"), code);
                http.end();
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        const size_t want = (avail > 0 && (size_t)avail < sizeof(chunk)) ? (size_t)avail : sizeof(chunk);
        const int rd = stream ? stream->read(chunk, want) : 0;
        if (rd <= 0)
        {
            if ((uint32_t)(millis() - last_data_ms) > kReadTimeoutMs)
            {
                heap_caps_free(chunk);
                heap_caps_free(buf);
                completeFailure_(Error::HttpRead, F("HTTP read stalled"), code);
                http.end();
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        last_data_ms = millis();
        if (!got_first_byte)
        {
            got_first_byte = true;
            first_byte_ms = last_data_ms;
        }
        if (size + (size_t)rd > max_bytes)
        {
            heap_caps_free(chunk);
            heap_caps_free(buf);
            completeFailure_(Error::TooLarge, F("Photo exceeds limit"), code);
            http.end();
            return;
        }
        if (size + (size_t)rd > capacity)
        {
            size_t next_cap = capacity;
            while (next_cap < size + (size_t)rd)
                next_cap *= 2u;
            if (next_cap > max_bytes)
                next_cap = max_bytes;
            uint8_t *grown = static_cast<uint8_t *>(heap_caps_realloc(buf, next_cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (!grown)
            {
                heap_caps_free(chunk);
                heap_caps_free(buf);
                completeFailure_(Error::PsramAlloc, F("PSRAM realloc failed"), code);
                http.end();
                return;
            }
            buf = grown;
            capacity = next_cap;
        }
        memcpy(buf + size, chunk, (size_t)rd);
        size += (size_t)rd;
    }
    http.end();
    heap_caps_free(chunk);

    if (size < 4 || !(buf[0] == 0xFF && buf[1] == 0xD8) || !(buf[size - 2] == 0xFF && buf[size - 1] == 0xD9))
    {
        heap_caps_free(buf);
        completeFailure_(Error::InvalidJpeg, F("Not a valid JPEG"), code);
        return;
    }

    if (_mtx)
        xSemaphoreTake(_mtx, portMAX_DELAY);
    freeBuffer_();
    _buf = buf;
    _size = size;
    _capacity = capacity;
    _content_type = content_type.length() ? content_type : String(F("image/jpeg"));
    if (_mtx)
        xSemaphoreGive(_mtx);
    const uint32_t done_ms = millis();
    _logs.info(F("CAMERA"),
               F("Download phases: begin_ms: %lu get_ms: %lu first_byte_ms: %lu body_ms: %lu total_ms: %lu bytes: %u"),
               (unsigned long)(begin_done_ms - t0_ms),
               (unsigned long)(get_done_ms - begin_done_ms),
               got_first_byte ? (unsigned long)(first_byte_ms - get_done_ms) : 0ul,
               got_first_byte ? (unsigned long)(done_ms - first_byte_ms) : 0ul,
               (unsigned long)(done_ms - t0_ms),
               (unsigned)size);
    completeSuccess_(Op::Download, code);
}

void Camera::runUpload_()
{
    String url;
    String content_type;
    String api_key;
    uint8_t *buf = nullptr;
    size_t size = 0;

    if (_mtx)
        xSemaphoreTake(_mtx, portMAX_DELAY);
    url = _url;
    content_type = _content_type;
    api_key = _api_key;
    buf = _buf;
    size = _size;
    if (_mtx)
        xSemaphoreGive(_mtx);

    if (!buf || size == 0)
    {
        completeFailure_(Error::NoData, F("No photo data"));
        return;
    }

    const uint32_t t0_ms = millis();
    HTTPClient http;
    std::unique_ptr<WiFiClientSecure> secure;
    http.setTimeout(kHttpTimeoutMs);
    if (url.startsWith("https://"))
    {
        secure.reset(new WiFiClientSecure());
        if (!secure)
        {
            completeFailure_(Error::Internal, F("TLS client alloc failed"));
            return;
        }
        secure->setInsecure();
        if (!http.begin(*secure, url))
        {
            completeFailure_(Error::HttpBegin, F("HTTP begin failed"));
            return;
        }
    }
    else if (!http.begin(url))
    {
        completeFailure_(Error::HttpBegin, F("HTTP begin failed"));
        return;
    }
    const uint32_t begin_done_ms = millis();
    http.addHeader(F("Content-Type"), content_type.length() ? content_type : String(F("image/jpeg")));
    if (api_key.length())
        http.addHeader(F("X-Api-Key"), api_key);
    const int code = http.sendRequest("POST", buf, size);
    const uint32_t send_done_ms = millis();
    http.end();
    if (code < 200 || code >= 300)
    {
        completeFailure_(Error::UploadFailed, String(F("Upload failed: ")) + String(code), code);
        return;
    }
    _logs.info(F("CAMERA"),
               F("Upload phases: begin_ms: %lu post_ms: %lu total_ms: %lu bytes: %u"),
               (unsigned long)(begin_done_ms - t0_ms),
               (unsigned long)(send_done_ms - begin_done_ms),
               (unsigned long)(send_done_ms - t0_ms),
               (unsigned)size);
    completeSuccess_(Op::Upload, code);
}

void Camera::completeFailure_(Error err, const String &text, int http_code)
{
    if (_mtx)
        xSemaphoreTake(_mtx, portMAX_DELAY);
    _busy = false;
    _ok = false;
    _error = err;
    _error_text = text;
    _http_code = http_code;
    _finished_ms = millis();
    _task = nullptr;
    if (_mtx)
        xSemaphoreGive(_mtx);
    _logs.warn(F("CAMERA"), F("Task failed: op: %s err: %s text: %s http: %d"),
               opName(_op), errorName(err), text.c_str(), http_code);
}

void Camera::completeSuccess_(Op op, int http_code)
{
    if (_mtx)
        xSemaphoreTake(_mtx, portMAX_DELAY);
    _busy = false;
    _ok = true;
    _error = Error::Ok;
    _error_text = "";
    _http_code = http_code;
    _finished_ms = millis();
    _task = nullptr;
    if (_mtx)
        xSemaphoreGive(_mtx);
    _logs.info(F("CAMERA"), F("Task done: op: %s bytes: %u http: %d time_ms: %lu"),
               opName(op), (unsigned)_size, http_code, (unsigned long)(_finished_ms - _started_ms));
}

void Camera::freeBuffer_()
{
    if (_buf)
    {
        heap_caps_free(_buf);
        _buf = nullptr;
    }
    _size = 0;
    _capacity = 0;
}

bool Camera::setBusy_(Op op, const String &url, size_t max_bytes, const String &content_type)
{
    if (!_mtx)
        return false;
    xSemaphoreTake(_mtx, portMAX_DELAY);
    if (_busy)
    {
        _error = Error::Busy;
        _error_text = F("Camera busy");
        xSemaphoreGive(_mtx);
        return false;
    }
    _busy = true;
    _ok = false;
    _error = Error::Ok;
    _error_text = "";
    _op = op;
    _url = url;
    _max_bytes = max_bytes;
    _started_ms = millis();
    _finished_ms = 0;
    _http_code = 0;
    if (content_type.length())
        _content_type = content_type;
    xSemaphoreGive(_mtx);
    return true;
}
