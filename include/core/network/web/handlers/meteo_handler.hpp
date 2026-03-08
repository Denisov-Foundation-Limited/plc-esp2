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

class MeteoHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleRemoteSources(WebInterface &web, AsyncWebServerRequest *request);

    static void handleMeteo(WebInterface &web, AsyncWebServerRequest *request);

    static void handleMeteoState(WebInterface &web, AsyncWebServerRequest *request);

    static void handleMeteoSave(WebInterface &web, AsyncWebServerRequest *request);
};
