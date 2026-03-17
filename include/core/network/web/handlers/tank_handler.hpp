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

class TankHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleTanks(WebInterface &web, AsyncWebServerRequest *request);
    static void handleTanksList(WebInterface &web, AsyncWebServerRequest *request);

    static void handleTanksSave(WebInterface &web, AsyncWebServerRequest *request);

    static void handleTanksToggle(WebInterface &web, AsyncWebServerRequest *request);
};

