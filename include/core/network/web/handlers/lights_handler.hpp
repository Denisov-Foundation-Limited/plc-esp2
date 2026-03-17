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


class LightsHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleLights(WebInterface &web, AsyncWebServerRequest *request);
    static void handleLightsList(WebInterface &web, AsyncWebServerRequest *request);

    static void handleLightsSave(WebInterface &web, AsyncWebServerRequest *request, const char *redirect);

    static void handleLightsToggle(WebInterface &web, AsyncWebServerRequest *request);

    static void handleLightsPortsOptions(WebInterface &web, AsyncWebServerRequest *request);

    static void handleLightsEnable(WebInterface &web, AsyncWebServerRequest *request);
};
