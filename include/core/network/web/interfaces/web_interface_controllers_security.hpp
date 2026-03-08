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
class WebInterfaceControllersSecurityHelper;

class WebInterfaceControllersSecurityHelper
{
public:
    static size_t securityLocalRenderCount_(const WebInterface &web);

    static String securityDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static String stackSecurityStatusText_(const WebInterface &web, uint32_t node_id);

    static String stackSecurityTitle_(const WebInterface &web, uint32_t node_id);

    static bool isStackSecurityView_(const WebInterface &web, uint32_t node_id);

    static bool requestStackSecurity_(WebInterface &web, uint32_t node_id);

    static String listSecuritySensorsHtml_(WebInterface &web);

    static String listSecuritySensorsTiles_(WebInterface &web, uint8_t start_idx, uint8_t end_idx);

    static size_t stackSecurityVisibleCount_(const WebInterface &web, uint32_t node_id);

    static String listStackSecuritySensorsTiles_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit);

    static String securityPortOptionsJson_(const WebInterface &web);

    static String securityUsedPinsJson_(const WebInterface &web);
};
