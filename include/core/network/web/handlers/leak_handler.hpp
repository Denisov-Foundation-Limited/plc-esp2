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
#include <ArduinoJson.h>

class LeakHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleLeak(WebInterface &web, AsyncWebServerRequest *request);

    static void handleLeakSave(WebInterface &web, AsyncWebServerRequest *request);

private:
    static bool isStackLeakView_(WebInterface &web, uint32_t node_id);

    static String leakRedirectPath_(uint32_t node_id, bool stack_view);

    static String leakDeviceSelectHtml_(WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static String stackLeakStatusText_(WebInterface &web, uint32_t node_id);

    static bool sendStackLeakSet_(WebInterface &web, uint32_t node_id, JsonArray *zones, bool ack_all);

    static String paramName_(const char *prefix, size_t id);

    static String checked_(bool value);

    static String portValue_(uint8_t port);

    static size_t stackLeakVisibleCount_(WebInterface &web, uint32_t node_id);
    static size_t localLeakVisibleCount_(WebInterface &web, uint32_t node_id);

    static String buildRows_(WebInterface &web, uint32_t node_id, bool stack_view, size_t offset, size_t limit);
};
