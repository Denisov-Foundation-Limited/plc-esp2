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
class WebInterfaceControllersTanksHelper;

class WebInterfaceControllersTanksHelper
{
public:
    static size_t tanksLocalRenderCount_(const WebInterface &web);

    static String tanksDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static String stackTanksStatusText_(const WebInterface &web, uint32_t node_id);

    static bool isStackTanksView_(const WebInterface &web, uint32_t node_id);

    static bool requestStackTanks_(WebInterface &web, uint32_t node_id);

    static size_t stackTanksVisibleCount_(const WebInterface &web, uint32_t node_id);

    static String listStackTanksHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit);

    static String listTanksHtml_(WebInterface &web, size_t offset, size_t limit);

    static String listTanksHtml_(WebInterface &web);

    static String tankPortOptionsJson_(const WebInterface &web, PortIO::PinType type);

    static String tankUsedPortsJson_(const WebInterface &web, PortIO::PinType type);
};
