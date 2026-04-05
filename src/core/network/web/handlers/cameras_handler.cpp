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

#include "core/network/web/handlers/cameras_handler.hpp"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "hal/camera_store.hpp"
#include "core/network/web/pages/web_page_cameras.hpp"
#include "core/network/web/web_interface.hpp"

namespace
{
String cameraImagePath_(uint8_t id)
{
    return String(F("/camera_")) + String((unsigned)id) + F(".jpg");
}

String cameraImageTempPath_(uint8_t id)
{
    return String(F("/camera_")) + String((unsigned)id) + F(".tmp");
}

void appendHtmlEscapedLocal_(String &out, const String &in)
{
    for (size_t i = 0; i < (size_t)in.length(); ++i)
    {
        switch (in[i])
        {
        case '&':
            out += F("&amp;");
            break;
        case '<':
            out += F("&lt;");
            break;
        case '>':
            out += F("&gt;");
            break;
        case '"':
            out += F("&quot;");
            break;
        case '\'':
            out += F("&#39;");
            break;
        default:
            out += in[i];
            break;
        }
    }
}

String htmlVal_(const String &in)
{
    String out;
    out.reserve(in.length() + 8);
    appendHtmlEscapedLocal_(out, in);
    return out;
}

String cameraTilesHtml_(const CameraConfigEntry cfgs[CameraStore::kCameraCount], uint8_t active_id, const String &active_status)
{
    String html;
    html.reserve(8192);
    for (size_t i = 0; i < CameraStore::kCameraCount; ++i)
    {
        const auto &cfg = cfgs[i];
        html += "<div class=\"tile";
        if (!cfg.enabled)
            html += " disabled";
        html += "\">";
        html += "<div class=\"preview\">";
        html += "<div class=\"preview-badge\"><span class=\"dot ";
        html += cfg.enabled ? "ok" : "off";
        html += "\"></span><span>#";
        html += String((unsigned)cfg.id);
        html += "</span></div>";
        html += "<div class=\"preview-empty\">Нет снимка</div>";
        html += "<img data-preview=\"/cameras/image?id=";
        html += String((unsigned)cfg.id);
        html += "\" src=\"/cameras/image?id=";
        html += String((unsigned)cfg.id);
        html += "\" alt=\"camera\" loading=\"lazy\">";
        html += "</div>";
        html += "<form method=\"POST\" action=\"/cameras\">";
        html += "<input type=\"hidden\" name=\"id\" value=\"";
        html += String((unsigned)cfg.id);
        html += "\">";
        html += "<div class=\"form-head\"><div class=\"title\">";
        appendHtmlEscapedLocal_(html, cfg.name.length() ? cfg.name : (String(F("Камера #")) + String((unsigned)cfg.id)));
        html += "</div><label class=\"switch\"><input type=\"checkbox\" name=\"enabled\" ";
        if (cfg.enabled)
            html += "checked";
        html += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
        html += "<div class=\"form-grid\">";
        html += "<div class=\"form-row\"><label>Имя</label><input class=\"field\" type=\"text\" name=\"name\" value=\"";
        html += htmlVal_(cfg.name);
        html += "\"></div>";
        html += "<div class=\"form-row\"><label>Логин</label><input class=\"field\" type=\"text\" name=\"username\" value=\"";
        html += htmlVal_(cfg.username);
        html += "\"></div>";
        html += "<div class=\"form-row full\"><label>Snapshot URL</label><input class=\"field\" type=\"text\" name=\"snapshot_url\" value=\"";
        html += htmlVal_(cfg.snapshot_url);
        html += "\"></div>";
        html += "<div class=\"form-row full\"><label>Пароль</label><input class=\"field\" type=\"password\" name=\"password\" value=\"";
        html += htmlVal_(cfg.password);
        html += "\"></div>";
        html += "</div>";
        html += "<div class=\"actions\"><button class=\"btn\" type=\"submit\">Сохранить</button>";
        html += "<button class=\"btn secondary js-snapshot-btn\" type=\"button\" data-id=\"";
        html += String((unsigned)cfg.id);
        html += "\">Получить фото</button></div>";
        html += "<div class=\"tile-status\">";
        if (active_id == cfg.id && active_status.length())
            appendHtmlEscapedLocal_(html, active_status);
        html += "</div>";
        html += "</form>";
        html += "</div>";
    }
    return html;
}

String placeholderSvg_(uint8_t id)
{
    String svg;
    svg.reserve(256);
    svg += F("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 640 360'>");
    svg += F("<defs><linearGradient id='g' x1='0' x2='1' y1='0' y2='1'><stop stop-color='#0b1220'/><stop offset='1' stop-color='#172033'/></linearGradient></defs>");
    svg += F("<rect width='640' height='360' rx='20' fill='url(#g)'/>");
    svg += F("<circle cx='320' cy='152' r='46' fill='#243148'/><rect x='228' y='208' width='184' height='16' rx='8' fill='#243148'/>");
    svg += F("<text x='320' y='292' text-anchor='middle' fill='#94a3b8' font-family='Segoe UI, Arial, sans-serif' font-size='28'>Камера #");
    svg += String((unsigned)id);
    svg += F("</text><text x='320' y='326' text-anchor='middle' fill='#64748b' font-family='Segoe UI, Arial, sans-serif' font-size='20'>Снимок еще не получен</text></svg>");
    return svg;
}

void sendJson_(AsyncWebServerRequest *request, const JsonDocument &doc)
{
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json; charset=utf-8", out);
}
} // namespace

void CamerasHandler::registerRoutes(WebInterface &web, AsyncWebServer &server)
{
    server.on("/cameras/snapshot", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleSnapshot(web, request); });
    server.on("/cameras/task", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleTask(web, request); });
    server.on("/cameras/image", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleImage(web, request); });
    server.on("/cameras", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleSave(web, request); });
    server.on("/cameras", HTTP_GET, [&web](AsyncWebServerRequest *request) { handlePage(web, request); });
}

void CamerasHandler::handlePage(WebInterface &web, AsyncWebServerRequest *request)
{
    bool set_cookie = false;
    if (!web.checkAuth_(request, &set_cookie))
        return;
    if (!web.requireWebAdmin_(request, &set_cookie))
        return;
    CameraConfigEntry cfgs[CameraStore::kCameraCount];
    if (!CameraStore::load(LittleFS, cfgs))
        web._camera_status = F("Не удалось прочитать конфиг камер");
    String page = FPSTR(kWebInterfaceCamerasHtml);
    page.reserve(page.length() + 12288);
    page.replace("%NAV%", web.navHtml_());
    page.replace("%CAMERA_STATUS%", web._camera_status.length() ? web._camera_status : String(F("Настройте Snapshot URL и нажмите «Получить фото».")));
    page.replace("%CAMERA_TILES%", cameraTilesHtml_(cfgs, web._camera_preview_id, web._camera_status));
    web.sendHtml_(request, page, set_cookie);
}

void CamerasHandler::handleSave(WebInterface &web, AsyncWebServerRequest *request)
{
    bool set_cookie = false;
    if (!web.checkAuth_(request, &set_cookie))
        return;
    if (!web.requireWebAdmin_(request, &set_cookie))
        return;
    const String id_str = web.paramValue_(request, "id");
    const int id = id_str.toInt();
    if (id <= 0 || id > (int)CameraStore::kCameraCount)
    {
        web._camera_status = F("Некорректный id камеры");
        web.sendRedirect_(request, "/cameras", set_cookie);
        return;
    }
    CameraConfigEntry cfgs[CameraStore::kCameraCount];
    if (!CameraStore::load(LittleFS, cfgs))
        CameraStore::setDefaults(cfgs);
    CameraConfigEntry *cfg = nullptr;
    if (!CameraStore::find(cfgs, (uint8_t)id, cfg) || !cfg)
    {
        web._camera_status = F("Камера не найдена");
        web.sendRedirect_(request, "/cameras", set_cookie);
        return;
    }
    cfg->enabled = request->hasParam("enabled", true);
    cfg->name = web.paramValue_(request, "name");
    cfg->snapshot_url = web.paramValue_(request, "snapshot_url");
    cfg->username = web.paramValue_(request, "username");
    cfg->password = web.paramValue_(request, "password");
    if (!cfg->name.length())
        cfg->name = String(F("Камера #")) + String((unsigned)cfg->id);
    if (!CameraStore::save(LittleFS, cfgs))
        web._camera_status = F("Не удалось сохранить камеры");
    else
        web._camera_status = String(F("Камера #")) + String((unsigned)cfg->id) + F(" сохранена");
    web.sendRedirect_(request, "/cameras", set_cookie);
}

void CamerasHandler::handleSnapshot(WebInterface &web, AsyncWebServerRequest *request)
{
    bool set_cookie = false;
    if (!web.checkAuthApi_(request, &set_cookie))
        return;
    if (!web.webSessionIsAdmin_())
    {
        StaticJsonDocument<128> doc;
        doc["ok"] = false;
        doc["error"] = "Admin only";
        sendJson_(request, doc);
        return;
    }
    StaticJsonDocument<256> doc;
    if (!web._camera)
    {
        doc["ok"] = false;
        doc["error"] = "Camera unavailable";
        sendJson_(request, doc);
        return;
    }
    const int id = web.paramValueAny_(request, "id").toInt();
    CameraConfigEntry cfgs[CameraStore::kCameraCount];
    CameraConfigEntry *cfg = nullptr;
    if (id <= 0 || id > (int)CameraStore::kCameraCount || !CameraStore::load(LittleFS, cfgs) || !CameraStore::find(cfgs, (uint8_t)id, cfg) || !cfg)
    {
        doc["ok"] = false;
        doc["error"] = "Invalid camera id";
        sendJson_(request, doc);
        return;
    }
    if (!cfg->enabled)
    {
        doc["ok"] = false;
        doc["error"] = "Camera disabled";
        sendJson_(request, doc);
        return;
    }
    const String url = CameraStore::buildEffectiveUrl(*cfg);
    if (!url.length())
    {
        doc["ok"] = false;
        doc["error"] = "Empty snapshot URL";
        sendJson_(request, doc);
        return;
    }
    if (!web._camera->startDownload(url))
    {
        doc["ok"] = false;
        doc["error"] = web._camera->lastErrorText();
        sendJson_(request, doc);
        return;
    }
    Camera::Snapshot snap{};
    web._camera->snapshot(snap);
    web._camera_preview_id = (uint8_t)id;
    web._camera_preview_ver = 0;
    web._camera_request_started_ms = snap.started_ms ? snap.started_ms : millis();
    const String tmp_path = cameraImageTempPath_((uint8_t)id);
    if (LittleFS.exists(tmp_path))
        LittleFS.remove(tmp_path);
    web._camera_status = String(F("Камера #")) + String((unsigned)id) + F(": получаем фото...");
    doc["ok"] = true;
    doc["status"] = web._camera_status;
    doc["ver"] = (unsigned long)web._camera_request_started_ms;
    sendJson_(request, doc);
}

void CamerasHandler::handleTask(WebInterface &web, AsyncWebServerRequest *request)
{
    bool set_cookie = false;
    if (!web.checkAuthApi_(request, &set_cookie))
        return;
    if (!web.webSessionIsAdmin_())
    {
        StaticJsonDocument<128> doc;
        doc["ok"] = false;
        doc["busy"] = false;
        doc["error"] = "Admin only";
        sendJson_(request, doc);
        return;
    }
    StaticJsonDocument<512> doc;
    if (!web._camera)
    {
        doc["ok"] = false;
        doc["busy"] = false;
        doc["status"] = "Camera unavailable";
        sendJson_(request, doc);
        return;
    }
    const int id = web.paramValueAny_(request, "id").toInt();
    Camera::Snapshot snap{};
    web._camera->snapshot(snap);
    bool preview_ready = false;
    if (!snap.busy && snap.ok && id > 0 && (uint8_t)id == web._camera_preview_id && snap.started_ms >= web._camera_request_started_ms &&
        web._camera_preview_ver != snap.finished_ms)
    {
        const String path = cameraImagePath_((uint8_t)id);
        const String tmp_path = cameraImageTempPath_((uint8_t)id);
        if (LittleFS.exists(tmp_path))
            LittleFS.remove(tmp_path);
        if (web._camera->saveToFs(LittleFS, tmp_path.c_str()))
        {
            if (LittleFS.exists(path))
                LittleFS.remove(path);
            if (LittleFS.rename(tmp_path, path))
            {
                web._camera_preview_ver = snap.finished_ms;
                web._camera_status = String(F("Камера #")) + String((unsigned)id) + F(": фото обновлено");
                preview_ready = true;
            }
            else
            {
                web._camera_status = String(F("Камера #")) + String((unsigned)id) + F(": ошибка подмены latest.jpg");
            }
        }
        else
        {
            web._camera_status = String(F("Камера #")) + String((unsigned)id) + F(": ошибка записи latest.jpg");
        }
    }
    else if (!snap.busy && snap.ok && id > 0 && (uint8_t)id == web._camera_preview_id && web._camera_preview_ver == snap.finished_ms && snap.finished_ms != 0)
    {
        preview_ready = true;
    }
    doc["busy"] = snap.busy;
    doc["ok"] = (!snap.busy && snap.ok && snap.error == Camera::Error::Ok && (id <= 0 || (uint8_t)id != web._camera_preview_id || preview_ready));
    doc["size"] = (unsigned)snap.size;
    doc["http"] = snap.http_code;
    doc["ver"] = (unsigned long)web._camera_preview_ver;
    doc["status"] = web._camera_status;
    if (!snap.busy && !snap.ok && snap.error_text.length())
        doc["error"] = snap.error_text;
    sendJson_(request, doc);
}

void CamerasHandler::handleImage(WebInterface &web, AsyncWebServerRequest *request)
{
    bool set_cookie = false;
    if (!web.checkAuth_(request, &set_cookie))
        return;
    if (!web.requireWebAdmin_(request, &set_cookie))
        return;
    const int id = web.paramValueAny_(request, "id").toInt();
    if (id <= 0 || id > (int)CameraStore::kCameraCount)
    {
        web.sendText_(request, 400, "text/plain; charset=utf-8", "Invalid camera id", set_cookie);
        return;
    }
    const String path = cameraImagePath_((uint8_t)id);
    if (!LittleFS.exists(path) && web._camera && (uint8_t)id == web._camera_preview_id)
    {
        Camera::Snapshot snap{};
        web._camera->snapshot(snap);
        if (!snap.busy && snap.ok && snap.started_ms >= web._camera_request_started_ms)
        {
            const String tmp_path = cameraImageTempPath_((uint8_t)id);
            if (LittleFS.exists(tmp_path))
                LittleFS.remove(tmp_path);
            if (web._camera->saveToFs(LittleFS, tmp_path.c_str()))
            {
                if (LittleFS.exists(path))
                    LittleFS.remove(path);
                if (LittleFS.rename(tmp_path, path))
                    web._camera_preview_ver = snap.finished_ms;
            }
        }
    }
    if (LittleFS.exists(path))
    {
        auto *response = request->beginResponse(LittleFS, path, "image/jpeg");
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
        return;
    }
    web.sendText_(request, 200, "image/svg+xml; charset=utf-8", placeholderSvg_((uint8_t)id), set_cookie);
}
