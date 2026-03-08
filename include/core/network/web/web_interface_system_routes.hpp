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

class AsyncWebServer;
class WebInterface;

class WebInterfaceSystemRoutes
{
public:
    static void registerPrimary(WebInterface &web, AsyncWebServer &server);
    static void registerSecondary(WebInterface &web, AsyncWebServer &server);
    static void registerTail(WebInterface &web, AsyncWebServer &server);
    static void registerDisplay(WebInterface &web, AsyncWebServer &server);
};
