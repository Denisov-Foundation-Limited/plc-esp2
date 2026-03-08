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


class WateringHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleWateringState(WebInterface &web, AsyncWebServerRequest *request);

    static void handleWatering(WebInterface &web, AsyncWebServerRequest *request);

    static void handleWateringSave(WebInterface &web, AsyncWebServerRequest *request);

private:
    static bool paramChecked_(AsyncWebServerRequest *request, const String &name);
    static bool parsePort_(const String &s, uint8_t &out);

    static bool parseTank_(const String &s, uint8_t &out);

    static bool parseTime_(const String &s, uint8_t &hour, uint8_t &minute);

    static bool parseDuration_(const String &s, uint32_t &out);

    static bool parseResumeLevel_(const String &s, uint8_t &out);
};


