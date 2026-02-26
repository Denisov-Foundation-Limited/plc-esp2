#pragma once

class AsyncWebServer;
class WebInterface;

class WebInterfaceStackRoutes
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);
};
