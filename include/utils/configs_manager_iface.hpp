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

#include "core/display_slots.hpp"

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
    virtual bool stackFallbackEnabled() const = 0;
    virtual String stackFallbackHost() const = 0;
    virtual bool stackSlaveController() const = 0;
    virtual bool cloudEnabled() const = 0;
    virtual String cloudHost() const = 0;
    virtual uint16_t cloudPort() const = 0;
    virtual String cloudPath() const = 0;
    virtual bool cloudUseSsl() const = 0;
    virtual uint32_t cloudReconnectMs() const = 0;
    virtual uint32_t cloudEventIntervalMs() const = 0;
    virtual String cloudApiKey() const = 0;
    virtual String cloudFirmwareVersion() const = 0;
    virtual bool rfidEnabled() const = 0;
    virtual bool ringClientEnabled() const = 0;
    virtual uint8_t ringClientButtonPort() const = 0;
    virtual size_t displaySlotCount() const = 0;
    virtual bool displaySlot(size_t idx, DisplaySlotConfig &out) const = 0;
    virtual void setStackRole(StackRole role) = 0;
    virtual void setStackMasterHost(const String &host) = 0;
    virtual void setStackApiKey(const String &key) = 0;
    virtual void setStackFallbackEnabled(bool enabled) = 0;
    virtual void setStackFallbackHost(const String &host) = 0;
    virtual void setStackSlaveController(bool controller) = 0;
    virtual void setCloudEnabled(bool enabled) = 0;
    virtual void setCloudHost(const String &host) = 0;
    virtual void setCloudPort(uint16_t port) = 0;
    virtual void setCloudPath(const String &path) = 0;
    virtual void setCloudUseSsl(bool use_ssl) = 0;
    virtual void setCloudReconnectMs(uint32_t ms) = 0;
    virtual void setCloudEventIntervalMs(uint32_t ms) = 0;
    virtual void setCloudApiKey(const String &key) = 0;
    virtual void setCloudFirmwareVersion(const String &ver) = 0;
    virtual void setRfidEnabled(bool enabled) = 0;
    virtual void setRingClientEnabled(bool enabled) = 0;
    virtual void setRingClientButtonPort(uint8_t port) = 0;
    virtual void setDisplaySlot(size_t idx, const DisplaySlotConfig &slot) = 0;
    virtual bool save() = 0;
    virtual bool save(const JsonDocument &doc) = 0;
};
