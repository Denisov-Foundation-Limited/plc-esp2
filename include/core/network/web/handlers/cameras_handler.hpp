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

class CamerasHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);
    static void handlePage(WebInterface &web, AsyncWebServerRequest *request);
    static void handleSave(WebInterface &web, AsyncWebServerRequest *request);
    static void handleSnapshot(WebInterface &web, AsyncWebServerRequest *request);
    static void handleTask(WebInterface &web, AsyncWebServerRequest *request);
    static void handleImage(WebInterface &web, AsyncWebServerRequest *request);
};
