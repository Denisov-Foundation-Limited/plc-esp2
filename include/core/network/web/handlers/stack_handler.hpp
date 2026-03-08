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

#pragma once
#include "core/network/web/interfaces/web_interface_handler_base.hpp"


class StackHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleStack(WebInterface &web, AsyncWebServerRequest *request);

    static void handleSlaveLinkStatus(WebInterface &web, AsyncWebServerRequest *request);

    static void handleNodesTbody(WebInterface &web, AsyncWebServerRequest *request);

    static void handleOnlineSnapshot(WebInterface &web, AsyncWebServerRequest *request);
};

