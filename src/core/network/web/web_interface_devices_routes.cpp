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

#include "core/network/web/web_interface_devices_routes.hpp"

#include "core/network/web/web_interface.hpp"
#include "core/network/web/interfaces/web_interface_handlers.hpp"

void WebInterfaceDevicesRoutes::registerRoutes(WebInterface &web, AsyncWebServer &server)
{
    SocketsHandler::registerRoutes(web, server);
    LightsHandler::registerRoutes(web, server);
    ThermoHandler::registerRoutes(web, server);
    WateringHandler::registerRoutes(web, server);
    MeteoHandler::registerRoutes(web, server);
    TankHandler::registerRoutes(web, server);
    AvrHandler::registerRoutes(web, server);
    LeakHandler::registerRoutes(web, server);
    RulesHandler::registerRoutes(web, server);
    SepticHandler::registerRoutes(web, server);
    RingHandler::registerRoutes(web, server);
    SecurityHandler::registerRoutes(web, server);
    server.on("/device", HTTP_POST, [&web](AsyncWebServerRequest *request) { web.handleDeviceSave_(request); });
}
