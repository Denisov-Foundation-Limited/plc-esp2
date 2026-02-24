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
#include <ArduinoJson.h>

#include "utils/fs_config.hpp"

class Configs
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        FsMount,
        OpenRead,
        OpenWrite,
        JsonParse,
        JsonSerialize
    };

    static constexpr const char *kPath = "/startup-config.json";

    bool begin(bool format_on_fail = false);bool load(JsonDocument &doc);bool save(const JsonDocument &doc);bool load(String &out);bool save(const String &json);Error lastError() const;bool erase();private:
    Error _err = Error::Ok;
};
