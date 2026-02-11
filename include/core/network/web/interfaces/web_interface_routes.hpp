#pragma once

inline void WebInterface::registerRoutes()
{
    IndexHandler::registerRoutes(*this, _server);
    WifiHandler::registerRoutes(*this, _server);
    ManageHandler::registerRoutes(*this, _server);
    PortsHandler::registerRoutes(*this, _server);
    BusesHandler::registerRoutes(*this, _server);
    _server.on("/stack/gen_key", HTTP_POST, [this](AsyncWebServerRequest *request) { handleStackGenKey_(request); });
    StackHandler::registerRoutes(*this, _server);
    ControllersHandler::registerRoutes(*this, _server);
    UsersHandler::registerRoutes(*this, _server);
    DisplayHandler::registerRoutes(*this, _server);
    SocketsHandler::registerRoutes(*this, _server);
    LightsHandler::registerRoutes(*this, _server);
    ThermoHandler::registerRoutes(*this, _server);
    WateringHandler::registerRoutes(*this, _server);
    MeteoHandler::registerRoutes(*this, _server);
    TankHandler::registerRoutes(*this, _server);
    AvrHandler::registerRoutes(*this, _server);
    LeakHandler::registerRoutes(*this, _server);
    RulesHandler::registerRoutes(*this, _server);
    SepticHandler::registerRoutes(*this, _server);
    RingHandler::registerRoutes(*this, _server);
    SecurityHandler::registerRoutes(*this, _server);
    TelegramHandler::registerRoutes(*this, _server);
    CloudHandler::registerRoutes(*this, _server);
    _server.on(
        "/upload", HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleUploadDone_(request); },
        [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
               bool final) { handleUpload_(request, filename, index, data, len, final); });
    _server.on(
        "/ota", HTTP_POST,
        [this](AsyncWebServerRequest *request) { handleOtaDone_(request); },
        [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
               bool final) { handleOta_(request, filename, index, data, len, final); });
    _server.on("/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) { handleWifiSave_(request); });
    _server.on("/stack", HTTP_POST, [this](AsyncWebServerRequest *request) { handleStackSave_(request); });
    _server.on("/device", HTTP_POST, [this](AsyncWebServerRequest *request) { handleDeviceSave_(request); });
    AdminHandler::registerRoutes(*this, _server);
    _server.on("/admin", HTTP_POST, [this](AsyncWebServerRequest *request) { handleAdminSave_(request); });
    LogsHandler::registerRoutes(*this, _server);
    _server.on("/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) { handleReboot_(request); });
    _server.on("/files", HTTP_GET, [this](AsyncWebServerRequest *request) { handleFileDownload_(request); });
    _server.on("/delete", HTTP_GET, [this](AsyncWebServerRequest *request) { handleDelete_(request); });
    StatusHandler::registerRoutes(*this, _server);
    _server.on("/ui/hash", HTTP_GET, [this](AsyncWebServerRequest *request) { handleUiHash_(request); });
    _server.onNotFound([this](AsyncWebServerRequest *request) {
        const String uri = request->url();
        if (uri.startsWith("/files/"))
        {
            handleFileDownload_(request);
            return;
        }
        request->send(404, "text/plain", String("Not found: ") + uri);
    });
}
