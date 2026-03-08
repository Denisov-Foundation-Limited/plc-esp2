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

class WifiHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleWifi(WebInterface &web, AsyncWebServerRequest *request);

private:
    static const char *signalLabel_(int rssi);

    static String formatSignalForWeb_(const String &signal);
};
