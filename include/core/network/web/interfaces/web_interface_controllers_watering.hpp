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
class WebInterfaceControllersWateringHelper;

class WebInterfaceControllersWateringHelper
{
public:
    static size_t wateringLocalRenderCount_(const WebInterface &web);

    static String wateringDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static String stackWateringStatusText_(const WebInterface &web, uint32_t node_id);

    static bool isStackWateringView_(const WebInterface &web, uint32_t node_id);

    static bool requestStackWatering_(WebInterface &web, uint32_t node_id);

    static size_t stackWateringVisibleCount_(const WebInterface &web, uint32_t node_id);

    static String listStackWateringHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit);

    static String listWateringHtml_(WebInterface &web, size_t offset, size_t limit);

    static String listWateringHtml_(WebInterface &web);

    static String wateringPortOptionsJson_(const WebInterface &web);

    static String wateringTankOptionsJson_(const WebInterface &web);

    static String stackWateringTankOptionsJson_(const WebInterface &web, uint32_t node_id);
};
