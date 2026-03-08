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
class WebInterfaceControllersSepticHelper;

class WebInterfaceControllersSepticHelper
{
public:
    static size_t septicLocalRenderCount_(const WebInterface &web);

    static String septicDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static String stackSepticStatusText_(const WebInterface &web, uint32_t node_id);

    static bool isStackSepticView_(const WebInterface &web, uint32_t node_id);

    static bool requestStackSeptic_(WebInterface &web, uint32_t node_id);

    static size_t stackSepticVisibleCount_(const WebInterface &web, uint32_t node_id);

    static String listStackSepticHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit);

    static String listSepticHtml_(WebInterface &web, size_t offset, size_t limit);

    static String listSepticHtml_(WebInterface &web);

    static String septicPortOptionsJson_(const WebInterface &web, PortIO::PinType type);

    static String septicUsedPortsJson_(const WebInterface &web, PortIO::PinType type);
};
