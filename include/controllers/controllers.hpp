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
#include "controllers/ring_controller.hpp"
#include "controllers/septic_controller.hpp"
#include "controllers/security_controller.hpp"
#include "controllers/socket_controller.hpp"
#include "controllers/tank_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "core/eeprom_storage.hpp"
#include "core/network/gsm_modem.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "hal/gpio/gpio.hpp"
#include "utils/logger.hpp"

class Controllers
{
public:
    Controllers(Gpio &gpio, OneWireManager &ow, EepromStorage &storage, Logger &logs,
                TelegramBot &tgbot, TelegramMenu &tgmenu, GsmModem &gsm)
        : _sockets(gpio, logs),
          _meteo(ow, logs),
          _thermo(gpio, _meteo, logs),
          _tanks(gpio, logs, tgbot, tgmenu),
          _septic(gpio, logs, tgbot, tgmenu),
          _security(gpio, ow, logs, tgbot, tgmenu),
          _ring(gpio, logs),
          _storage(storage),
          _logs(logs)
    {
        _security.setGsmModem(gsm);
    }

    bool begin()
    {
        _logs.info(F("CTRL"), F("Init begin"));
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
        _logs.info(F("CTRL"), F("Tanks init"));
        if (!_tanks.begin())
        {
            _logs.error(F("CTRL"), F("Tanks init failed"));
            return false;
        }
        _logs.info(F("CTRL"), F("Septic init"));
        if (!_septic.begin())
        {
            _logs.error(F("CTRL"), F("Septic init failed"));
            return false;
        }
        _logs.info(F("CTRL"), F("Security init"));
        if (!_security.begin())
        {
            _logs.error(F("CTRL"), F("Security init failed"));
            return false;
        }
        _logs.info(F("CTRL"), F("Ring init"));
        if (!_ring.begin())
        {
            _logs.error(F("CTRL"), F("Ring init failed"));
            return false;
        }
        loadFromStorage_();
        _logs.info(F("CTRL"), F("Init done"));
        return true;
    }

    void task()
    {
        _sockets.task();
        _meteo.task();
        _thermo.task();
        _tanks.task();
        _septic.task();
        _security.task();
        _ring.task();
        saveIfNeeded_();
    }

    void applyConfig(JsonObjectConst cfg)
    {
        if (cfg["sockets_enabled"].is<bool>())
            _sockets.setControllerEnabled(cfg["sockets_enabled"].as<bool>());
        if (cfg["lights_enabled"].is<bool>())
            _sockets.setLightsEnabled(cfg["lights_enabled"].as<bool>());
        if (cfg["meteo_enabled"].is<bool>())
            _meteo.setControllerEnabled(cfg["meteo_enabled"].as<bool>());
        if (cfg["thermo_enabled"].is<bool>())
            _thermo.setControllerEnabled(cfg["thermo_enabled"].as<bool>());
        if (cfg["tanks_enabled"].is<bool>())
            _tanks.setControllerEnabled(cfg["tanks_enabled"].as<bool>());
        if (cfg["septic_enabled"].is<bool>())
            _septic.setControllerEnabled(cfg["septic_enabled"].as<bool>());
        if (cfg["security_enabled"].is<bool>())
            _security.setControllerEnabled(cfg["security_enabled"].as<bool>());
        if (cfg["ring"].is<JsonObjectConst>())
            _ring.applyConfig(cfg["ring"].as<JsonObjectConst>());
        const bool has_lights = cfg["lights"].is<JsonArrayConst>();
        if (cfg["sockets"].is<JsonArrayConst>())
            _sockets.applyConfig(cfg["sockets"].as<JsonArrayConst>(), !has_lights);
        if (has_lights)
            _sockets.applyLightsConfig(cfg["lights"].as<JsonArrayConst>());
        if (cfg["meteo"].is<JsonArrayConst>())
            _meteo.applyConfig(cfg["meteo"].as<JsonArrayConst>());
        if (cfg["thermo"].is<JsonArrayConst>())
            _thermo.applyConfig(cfg["thermo"].as<JsonArrayConst>());
        if (cfg["tanks"].is<JsonArrayConst>())
            _tanks.applyConfig(cfg["tanks"].as<JsonArrayConst>());
        if (cfg["septic"].is<JsonArrayConst>())
            _septic.applyConfig(cfg["septic"].as<JsonArrayConst>());
        if (cfg["security"].is<JsonArrayConst>())
            _security.applyConfig(cfg["security"].as<JsonArrayConst>());
        if (cfg["security_keys"].is<JsonArrayConst>())
            _security.applyKeys(cfg["security_keys"].as<JsonArrayConst>());
        if (cfg["security_rfid_keys"].is<JsonArrayConst>())
            _security.applyRfidKeys(cfg["security_rfid_keys"].as<JsonArrayConst>());
        if (cfg["security_phones"].is<JsonArrayConst>())
            _security.applyPhones(cfg["security_phones"].as<JsonArrayConst>());
        if (cfg["security_siren"].is<unsigned>())
        {
            const unsigned raw = cfg["security_siren"].as<unsigned>();
            if (raw <= 0xFFu)
                _security.setSirenPort((uint8_t)raw);
        }
    }

    void serialize(JsonObject out) const
    {
        out["sockets_enabled"] = _sockets.controllerEnabled();
        JsonArray arr = out["sockets"].to<JsonArray>();
        _sockets.serialize(arr);
        out["lights_enabled"] = _sockets.lightsEnabled();
        JsonArray larr = out["lights"].to<JsonArray>();
        _sockets.serializeLights(larr);
        out["meteo_enabled"] = _meteo.controllerEnabled();
        JsonArray marr = out["meteo"].to<JsonArray>();
        _meteo.serialize(marr);
        out["thermo_enabled"] = _thermo.controllerEnabled();
        JsonArray tarr = out["thermo"].to<JsonArray>();
        _thermo.serialize(tarr);
        out["tanks_enabled"] = _tanks.controllerEnabled();
        JsonArray tks = out["tanks"].to<JsonArray>();
        _tanks.serialize(tks);
        out["septic_enabled"] = _septic.controllerEnabled();
        JsonArray sep = out["septic"].to<JsonArray>();
        _septic.serialize(sep);
        out["security_enabled"] = _security.controllerEnabled();
        JsonArray sec = out["security"].to<JsonArray>();
        _security.serialize(sec);
        JsonObject ring = out["ring"].to<JsonObject>();
        _ring.serialize(ring);
        JsonArray keys = out["security_keys"].to<JsonArray>();
        _security.serializeKeys(keys);
        JsonArray rkeys = out["security_rfid_keys"].to<JsonArray>();
        _security.serializeRfidKeys(rkeys);
        JsonArray phones = out["security_phones"].to<JsonArray>();
        _security.serializePhones(phones);
        if (_security.sirenPort() != SecurityController::kInvalidPort)
            out["security_siren"] = (unsigned)_security.sirenPort();
    }

    SocketController &sockets() { return _sockets; }
    const SocketController &sockets() const { return _sockets; }
    MeteoController &meteo() { return _meteo; }
    const MeteoController &meteo() const { return _meteo; }
    ThermoController &thermo() { return _thermo; }
    const ThermoController &thermo() const { return _thermo; }
    TankController &tanks() { return _tanks; }
    const TankController &tanks() const { return _tanks; }
    SepticController &septic() { return _septic; }
    const SepticController &septic() const { return _septic; }
    SecurityController &security() { return _security; }
    const SecurityController &security() const { return _security; }
    RingController &ring() { return _ring; }
    const RingController &ring() const { return _ring; }
    void setSaveIntervalMs(uint32_t ms) { _save_interval_ms = ms; }

private:
    SocketController _sockets;
    MeteoController _meteo;
    ThermoController _thermo;
    TankController _tanks;
    SepticController _septic;
    SecurityController _security;
    RingController _ring;
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
        EepromStorage::LightSnapshot lsnap;
        if (_storage.loadLights(lsnap))
            _sockets.applyLightsSnapshot(lsnap.enabled_mask, lsnap.state_mask, EepromStorage::kLightMaskBytes);
        EepromStorage::ThermoSnapshot tsnap;
        if (_storage.loadThermo(tsnap))
        {
            _thermo.applySnapshot(tsnap.power_mask, EepromStorage::kThermoMaskBytes);
            _thermo.applyTargetSnapshot(tsnap.target_t10, EepromStorage::kThermoCount);
        }
        EepromStorage::TankSnapshot tanksnap;
        if (_storage.loadTanks(tanksnap))
            _tanks.applySnapshot(tanksnap.power_mask, EepromStorage::kTankMaskBytes);
        EepromStorage::SecuritySnapshot ssnap;
        if (_storage.loadSecurity(ssnap))
            _security.applySnapshot(ssnap.armed);
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
        if (_sockets.takeLightsDirty())
        {
            EepromStorage::LightSnapshot snap;
            _sockets.buildLightsSnapshot(snap.enabled_mask, snap.state_mask, EepromStorage::kLightMaskBytes);
            if (_storage.saveLights(snap))
                saved = true;
        }
        if (_thermo.takeDirty())
        {
            EepromStorage::ThermoSnapshot tsnap;
            _thermo.buildSnapshot(tsnap.power_mask, EepromStorage::kThermoMaskBytes);
            _thermo.buildTargetSnapshot(tsnap.target_t10, EepromStorage::kThermoCount);
            if (_storage.saveThermo(tsnap))
                saved = true;
        }
        if (_tanks.takeDirty())
        {
            EepromStorage::TankSnapshot tanksnap;
            _tanks.buildSnapshot(tanksnap.power_mask, EepromStorage::kTankMaskBytes);
            if (_storage.saveTanks(tanksnap))
                saved = true;
        }
        if (_security.takeDirty())
        {
            EepromStorage::SecuritySnapshot ssnap;
            _security.buildSnapshot(ssnap.armed);
            if (_storage.saveSecurity(ssnap))
                saved = true;
        }
        if (saved)
            _last_save_ms = now;
    }

};
