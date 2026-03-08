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


class SecurityHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleSecurityState(WebInterface &web, AsyncWebServerRequest *request);

    static void handleSecurity(WebInterface &web, AsyncWebServerRequest *request);

    static void handleSecuritySave(WebInterface &web, AsyncWebServerRequest *request);

    static void handleSecurityArm(WebInterface &web, AsyncWebServerRequest *request);
};

