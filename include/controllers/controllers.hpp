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

#include "controllers/socket/socket_controller.hpp"
#include "core/eeprom_storage.hpp"
#include "hal/gpio/gpio.hpp"
#include "utils/logger.hpp"

class Controllers
{
public:
    explicit Controllers(Gpio &gpio, EepromStorage &storage, Logger &logs)
        : _sockets(gpio), _storage(storage), _logs(logs) {}

    bool begin()
    {
        _logs.info(F("CTRL"), F("Sockets init"));
        if (!_sockets.begin())
        {
            _logs.error(F("CTRL"), F("Sockets init failed"));
            return false;
        }
        loadFromStorage_();
        return true;
    }

    void task()
    {
        _sockets.task();
        saveIfNeeded_();
    }

    void applyConfig(JsonObjectConst cfg)
    {
        if (cfg["sockets_enabled"].is<bool>())
            _sockets.setControllerEnabled(cfg["sockets_enabled"].as<bool>());
        if (!cfg["sockets"].is<JsonArrayConst>())
            return;
        _sockets.applyConfig(cfg["sockets"].as<JsonArrayConst>());
    }

    void serialize(JsonObject out) const
    {
        out["sockets_enabled"] = _sockets.controllerEnabled();
        JsonArray arr = out["sockets"].to<JsonArray>();
        _sockets.serialize(arr);
    }

    SocketController &sockets() { return _sockets; }
    const SocketController &sockets() const { return _sockets; }
    void setSaveIntervalMs(uint32_t ms) { _save_interval_ms = ms; }

private:
    SocketController _sockets;
    EepromStorage &_storage;
    Logger &_logs;
    uint32_t _last_save_ms = 0;
    uint32_t _save_interval_ms = 2000;

    void loadFromStorage_()
    {
        if (!_storage.isReady())
            return;
        EepromStorage::SocketSnapshot snap;
        if (_storage.loadSockets(snap))
            _sockets.applySnapshot(snap.enabled_mask, snap.state_mask, EepromStorage::kSocketMaskBytes);
    }

    void saveIfNeeded_()
    {
        if (!_storage.isReady())
            return;
        if (!_sockets.takeDirty())
            return;
        const uint32_t now = millis();
        if (_save_interval_ms && (uint32_t)(now - _last_save_ms) < _save_interval_ms)
            return;
        EepromStorage::SocketSnapshot snap;
        _sockets.buildSnapshot(snap.enabled_mask, snap.state_mask, EepromStorage::kSocketMaskBytes);
        if (_storage.saveSockets(snap))
            _last_save_ms = now;
    }
};
