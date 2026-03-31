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

class WebInterface;
class AsyncWebServerRequest;
class WebInterfaceControllersLightsHelper;

class WebInterfaceControllersLightsHelper
{
public:
    static size_t lightsLocalRenderCount_(const WebInterface &web);

    static String lightsDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static String stackLightsStatusText_(const WebInterface &web, uint32_t node_id);

    static bool isStackLightsView_(const WebInterface &web, uint32_t node_id);

    static void handleStackLightsToggle_(WebInterface &web, AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie);
    static void handleStackLightsEnable_(WebInterface &web, AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie);

    static bool requestStackLights_(WebInterface &web, uint32_t node_id);

    static String listLightsHtml_(WebInterface &web, uint8_t start_id, uint8_t end_id);

    static size_t stackLightsVisibleCount_(const WebInterface &web, uint32_t node_id);

    static String listStackLightsHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit);
};
