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

#include <ArduinoJson.h>
#include <stdint.h>
#include <vector>

class StackDataBinaryCodec
{
public:
    static bool encode(const char *feature, const char *action, JsonVariantConst value, std::vector<uint8_t> &out);
    static bool decode(const char *feature, const char *action, const uint8_t *data, size_t size, DynamicJsonDocument &out);
};
