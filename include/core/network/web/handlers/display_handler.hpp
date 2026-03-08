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

#include <string.h>

#include "core/display.hpp"


class DisplayHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleDisplay(WebInterface &web, AsyncWebServerRequest *request);

    static void handleDisplaySave(WebInterface &web, AsyncWebServerRequest *request);

private:
    static bool displaySlotEqual_(const DisplaySlotConfig &a, const DisplaySlotConfig &b);
};
