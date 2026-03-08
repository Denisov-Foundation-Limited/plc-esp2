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

class IndexHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleIndex(WebInterface &web, AsyncWebServerRequest *request);

    static void handleIndexState(WebInterface &web, AsyncWebServerRequest *request);

private:
    struct IndexState
    {
        String device_name;
        String rtc_date;
        String rtc_time;
        String rtc_temp;
        String board_temp;
        String cpu_temp;
        String fan_html;
    };

    static IndexState collectIndexState_(WebInterface &web, uint32_t node_id, bool stack_view);
};
