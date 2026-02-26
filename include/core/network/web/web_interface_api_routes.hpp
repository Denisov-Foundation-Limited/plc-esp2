#pragma once

class AsyncWebServer;
class WebInterface;

class WebInterfaceApiRoutes
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);
    static void registerNotFound(WebInterface &web, AsyncWebServer &server);
};
