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
#include <LittleFS.h>

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

    bool begin(bool format_on_fail = false)
    {
        if (LittleFS.begin(format_on_fail))
        {
            _err = Error::Ok;
            return true;
        }
        _err = Error::FsMount;
        return false;
    }

    bool load(JsonDocument &doc)
    {
        File f = LittleFS.open(kPath, "r");
        if (!f)
        {
            _err = Error::OpenRead;
            return false;
        }
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        if (err)
        {
            _err = Error::JsonParse;
            return false;
        }
        _err = Error::Ok;
        return true;
    }

    bool save(const JsonDocument &doc)
    {
        File f = LittleFS.open(kPath, "w");
        if (!f)
        {
            _err = Error::OpenWrite;
            return false;
        }
        if (serializeJsonPretty(doc, f) == 0)
        {
            f.close();
            _err = Error::JsonSerialize;
            return false;
        }
        f.close();
        _err = Error::Ok;
        return true;
    }

    bool load(String &out)
    {
        File f = LittleFS.open(kPath, "r");
        if (!f)
        {
            _err = Error::OpenRead;
            return false;
        }
        out = f.readString();
        f.close();
        _err = Error::Ok;
        return true;
    }

    bool save(const String &json)
    {
        File f = LittleFS.open(kPath, "w");
        if (!f)
        {
            _err = Error::OpenWrite;
            return false;
        }
        size_t written = f.print(json);
        f.close();
        if (written == 0)
        {
            _err = Error::JsonSerialize;
            return false;
        }
        _err = Error::Ok;
        return true;
    }

    Error lastError() const { return _err; }

    bool erase()
    {
        if (!LittleFS.exists(kPath))
        {
            _err = Error::Ok;
            return true;
        }
        if (!LittleFS.remove(kPath))
        {
            _err = Error::OpenWrite;
            return false;
        }
        _err = Error::Ok;
        return true;
    }

private:
    Error _err = Error::Ok;
};
