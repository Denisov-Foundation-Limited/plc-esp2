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
class WebInterfaceControllersDisplayHelper;

class WebInterfaceControllersDisplayHelper
{
public:
    static String displaySlotsHtml_(const WebInterface &web);

    static String displayDeviceOptionsJson_(const WebInterface &web);

    static String displaySocketOptionsJson_(const WebInterface &web);

    static String displayLightOptionsJson_(const WebInterface &web);

    static String displayMeteoOptionsJson_(const WebInterface &web);

    static String displayThermoOptionsJson_(const WebInterface &web);

    static String displayTankOptionsJson_(const WebInterface &web);

    static String displaySepticOptionsJson_(const WebInterface &web);

    static String displayAvrOptionsJson_(const WebInterface &web);

    static String displayLeakOptionsJson_(const WebInterface &web);
};
