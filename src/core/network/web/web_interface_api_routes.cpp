#include "core/network/web/web_interface_api_routes.hpp"

#include "core/network/web/web_interface.hpp"

void WebInterfaceApiRoutes::registerRoutes(WebInterface &web, AsyncWebServer &server)
{
    server.on(
        "/upload", HTTP_POST,
        [&web](AsyncWebServerRequest *request) { web.handleUploadDone_(request); },
        [&web](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
               bool final) { web.handleUpload_(request, filename, index, data, len, final); });
    server.on(
        "/ota", HTTP_POST,
        [&web](AsyncWebServerRequest *request) { web.handleOtaDone_(request); },
        [&web](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
               bool final) { web.handleOta_(request, filename, index, data, len, final); });
    server.on("/wifi", HTTP_POST, [&web](AsyncWebServerRequest *request) { web.handleWifiSave_(request); });
    server.on("/admin", HTTP_POST, [&web](AsyncWebServerRequest *request) { web.handleAdminSave_(request); });
    server.on("/reboot", HTTP_POST, [&web](AsyncWebServerRequest *request) { web.handleReboot_(request); });
    server.on("/files", HTTP_GET, [&web](AsyncWebServerRequest *request) { web.handleFileDownload_(request); });
    server.on("/delete", HTTP_GET, [&web](AsyncWebServerRequest *request) { web.handleDelete_(request); });
    server.on("/ui/hash", HTTP_GET, [&web](AsyncWebServerRequest *request) { web.handleUiHash_(request); });
}

void WebInterfaceApiRoutes::registerNotFound(WebInterface &web, AsyncWebServer &server)
{
    server.onNotFound([&web](AsyncWebServerRequest *request) {
        const String uri = request->url();
        if (uri.startsWith("/files/"))
        {
            web.handleFileDownload_(request);
            return;
        }
        request->send(404, "text/plain", String("Not found: ") + uri);
    });
}
