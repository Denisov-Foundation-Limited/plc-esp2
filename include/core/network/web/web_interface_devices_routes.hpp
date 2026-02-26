#pragma once

class AsyncWebServer;
class WebInterface;

class WebInterfaceDevicesRoutes
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);
};
