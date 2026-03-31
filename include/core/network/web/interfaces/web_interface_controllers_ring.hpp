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

class WebInterfaceControllersRingHelper
{
public:
    static String ringDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);
    static bool isStackRingView_(const WebInterface &web, uint32_t node_id);
    static bool sendStackRingCmd_(WebInterface &web, uint32_t node_id, bool set_state, bool state);
    static bool sendStackRingCmdAll_(WebInterface &web, bool set_state, bool state);
};
