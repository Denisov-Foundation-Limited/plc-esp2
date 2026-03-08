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
class WebInterfaceControllersThermoHelper;

class WebInterfaceControllersThermoHelper
{
public:
    static size_t thermoLocalRenderCount_(const WebInterface &web);

    static String thermoDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);

    static String stackThermoStatusText_(const WebInterface &web, uint32_t node_id);

    static bool isStackThermoView_(const WebInterface &web, uint32_t node_id);

    static bool requestStackThermo_(WebInterface &web, uint32_t node_id);

    static size_t stackThermoVisibleCount_(const WebInterface &web, uint32_t node_id);

    static String listStackThermoHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit);

    static String listThermoHtml_(WebInterface &web, size_t offset, size_t limit);

    static String listThermoHtml_(WebInterface &web);

    static String thermoPortOptionsJson_(const WebInterface &web, PortIO::PinType type);

    static String thermoUsedPortsJson_(const WebInterface &web, PortIO::PinType type);
};
