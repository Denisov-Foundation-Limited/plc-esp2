#pragma once

class AsyncWebServer;
class WebInterface;

class WebInterfaceSystemRoutes
{
public:
    static void registerPrimary(WebInterface &web, AsyncWebServer &server);
    static void registerSecondary(WebInterface &web, AsyncWebServer &server);
    static void registerTail(WebInterface &web, AsyncWebServer &server);
    static void registerDisplay(WebInterface &web, AsyncWebServer &server);
};
