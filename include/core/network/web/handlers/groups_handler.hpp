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

#include "core/compat/stack_stub.hpp"

class GroupsHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleGroups(WebInterface &web, AsyncWebServerRequest *request);

    static void handleGroupsSave(WebInterface &web, AsyncWebServerRequest *request);

private:
    static bool isStackGroupsView_(WebInterface &web, uint32_t node_id);

    static String groupsPath_(uint32_t node_id, bool stack_view);

    static void appendGroupRow_(WebInterface &web, String &rows, uint8_t id, const char *name, uint16_t sort);

    static void appendRowsFromLocalConfig_(WebInterface &web, String &rows);

    template <typename TCache>
    static void appendRowsFromStackCache_(WebInterface &web, String &rows, const TCache *cache);

    static void handleGroupsSaveStack_(WebInterface &web, AsyncWebServerRequest *request,
                                       uint32_t node_id, const String &back, bool set_cookie);
};
