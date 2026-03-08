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

#include "core/network/web/web_interface_system_routes.hpp"

#include "core/network/web/web_interface.hpp"
#include "core/network/web/interfaces/web_interface_handlers.hpp"

void WebInterfaceSystemRoutes::registerPrimary(WebInterface &web, AsyncWebServer &server)
{
    IndexHandler::registerRoutes(web, server);
    WifiHandler::registerRoutes(web, server);
    ManageHandler::registerRoutes(web, server);
    PortsHandler::registerRoutes(web, server);
    BusesHandler::registerRoutes(web, server);
}

void WebInterfaceSystemRoutes::registerSecondary(WebInterface &web, AsyncWebServer &server)
{
    ControllersHandler::registerRoutes(web, server);
    UsersHandler::registerRoutes(web, server);
    GroupsHandler::registerRoutes(web, server);
}

void WebInterfaceSystemRoutes::registerTail(WebInterface &web, AsyncWebServer &server)
{
    TelegramHandler::registerRoutes(web, server);
    CloudHandler::registerRoutes(web, server);
    AdminHandler::registerRoutes(web, server);
    LogsHandler::registerRoutes(web, server);
    StatusHandler::registerRoutes(web, server);
}

void WebInterfaceSystemRoutes::registerDisplay(WebInterface &web, AsyncWebServer &server)
{
    DisplayHandler::registerRoutes(web, server);
}
