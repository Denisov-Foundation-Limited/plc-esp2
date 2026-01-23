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

class ConfigsManagerIface
{
public:
    enum class StackRole : uint8_t
    {
        Master = 0,
        Slave
    };

    virtual ~ConfigsManagerIface() = default;
    virtual StackRole stackRole() const = 0;
    virtual String stackMasterHost() const = 0;
    virtual String stackApiKey() const = 0;
    virtual void setStackRole(StackRole role) = 0;
    virtual void setStackMasterHost(const String &host) = 0;
    virtual void setStackApiKey(const String &key) = 0;
    virtual bool save() = 0;
    virtual bool save(const JsonDocument &doc) = 0;
};
