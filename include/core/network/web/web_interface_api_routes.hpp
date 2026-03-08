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

class WebInterfaceApiRoutes
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);
    static void registerNotFound(WebInterface &web, AsyncWebServer &server);
};
