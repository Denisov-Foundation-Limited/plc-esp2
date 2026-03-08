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

#include <Arduino.h>
#include "hal/gpio/portio.hpp"

class WebInterface;
class AsyncWebServerRequest;
class WebInterfaceControllersSocketsHelper;

class WebInterfaceControllersSocketsHelper
{
public:
    static size_t socketsLocalRenderCount_(const WebInterface &web);

    static String listSocketsHtml_(WebInterface &web, uint8_t start_id, uint8_t end_id);

    static size_t stackSocketsVisibleCount_(const WebInterface &web, uint32_t node_id);

    static String listStackSocketsHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit);

    static String socketsDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static String stackSocketsStatusText_(const WebInterface &web, uint32_t node_id);

    static bool isStackSocketsView_(const WebInterface &web, uint32_t node_id);

    static void handleStackSocketsToggle_(WebInterface &web, AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie);

    static void handleStackSocketsEnable_(WebInterface &web, AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie);

    static bool requestStackSockets_(WebInterface &web, uint32_t node_id);

    static String socketPortOptionsJson_(const WebInterface &web, PortIO::PinType type);

    static String socketUsedPortsJson_(const WebInterface &web, PortIO::PinType type);
};
