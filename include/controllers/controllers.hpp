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

#include "controllers/meteo_controller.hpp"
#include "controllers/socket_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "core/eeprom_storage.hpp"
#include "hal/gpio/gpio.hpp"
#include "utils/logger.hpp"

class Controllers
{
public:
    explicit Controllers(Gpio &gpio, OneWireManager &ow, EepromStorage &storage, Logger &logs)
        : _sockets(gpio, &logs), _meteo(ow), _thermo(gpio, _meteo, &logs), _storage(storage), _logs(logs) {}

    bool begin()
    {
        _logs.info(F("CTRL"), F("Sockets init"));
        if (!_sockets.begin())
        {
            _logs.error(F("CTRL"), F("Sockets init failed"));
            return false;
        }
        _logs.info(F("CTRL"), F("Meteo init"));
        if (!_meteo.begin())
        {
            _logs.error(F("CTRL"), F("Meteo init failed"));
            return false;
        }
        _logs.info(F("CTRL"), F("Thermo init"));
        if (!_thermo.begin())
        {
            _logs.error(F("CTRL"), F("Thermo init failed"));
            return false;
        }
        loadFromStorage_();
        return true;
    }

    void task()
    {
        _sockets.task();
        _meteo.task();
        _thermo.task();
        saveIfNeeded_();
    }

    void applyConfig(JsonObjectConst cfg)
    {
        if (cfg["sockets_enabled"].is<bool>())
            _sockets.setControllerEnabled(cfg["sockets_enabled"].as<bool>());
        if (cfg["meteo_enabled"].is<bool>())
            _meteo.setControllerEnabled(cfg["meteo_enabled"].as<bool>());
        if (cfg["thermo_enabled"].is<bool>())
            _thermo.setControllerEnabled(cfg["thermo_enabled"].as<bool>());
        if (cfg["sockets"].is<JsonArrayConst>())
            _sockets.applyConfig(cfg["sockets"].as<JsonArrayConst>());
        if (cfg["meteo"].is<JsonArrayConst>())
            _meteo.applyConfig(cfg["meteo"].as<JsonArrayConst>());
        if (cfg["thermo"].is<JsonArrayConst>())
            _thermo.applyConfig(cfg["thermo"].as<JsonArrayConst>());
    }

    void serialize(JsonObject out) const
    {
        out["sockets_enabled"] = _sockets.controllerEnabled();
        JsonArray arr = out["sockets"].to<JsonArray>();
        _sockets.serialize(arr);
        out["meteo_enabled"] = _meteo.controllerEnabled();
        JsonArray marr = out["meteo"].to<JsonArray>();
        _meteo.serialize(marr);
        out["thermo_enabled"] = _thermo.controllerEnabled();
        JsonArray tarr = out["thermo"].to<JsonArray>();
        _thermo.serialize(tarr);
    }

    SocketController &sockets() { return _sockets; }
    const SocketController &sockets() const { return _sockets; }
    MeteoController &meteo() { return _meteo; }
    const MeteoController &meteo() const { return _meteo; }
    ThermoController &thermo() { return _thermo; }
    const ThermoController &thermo() const { return _thermo; }
    void setSaveIntervalMs(uint32_t ms) { _save_interval_ms = ms; }

private:
    SocketController _sockets;
    MeteoController _meteo;
    ThermoController _thermo;
    EepromStorage &_storage;
    Logger &_logs;
    uint32_t _last_save_ms = 0;
    uint32_t _save_interval_ms = 10000;

    void loadFromStorage_()
    {
        if (!_storage.isReady())
            return;
        EepromStorage::SocketSnapshot snap;
        if (_storage.loadSockets(snap))
            _sockets.applySnapshot(snap.enabled_mask, snap.state_mask, EepromStorage::kSocketMaskBytes);
        EepromStorage::ThermoSnapshot tsnap;
        if (_storage.loadThermo(tsnap))
            _thermo.applySnapshot(tsnap.power_mask, EepromStorage::kThermoMaskBytes);
    }

    void saveIfNeeded_()
    {
        if (!_storage.isReady())
            return;
        const uint32_t now = millis();
        if (_save_interval_ms && (uint32_t)(now - _last_save_ms) < _save_interval_ms)
            return;
        bool saved = false;
        if (_sockets.takeDirty())
        {
            EepromStorage::SocketSnapshot snap;
            _sockets.buildSnapshot(snap.enabled_mask, snap.state_mask, EepromStorage::kSocketMaskBytes);
            if (_storage.saveSockets(snap))
                saved = true;
        }
        if (_thermo.takeDirty())
        {
            EepromStorage::ThermoSnapshot tsnap;
            _thermo.buildSnapshot(tsnap.power_mask, EepromStorage::kThermoMaskBytes);
            if (_storage.saveThermo(tsnap))
                saved = true;
        }
        if (saved)
            _last_save_ms = now;
    }
};
