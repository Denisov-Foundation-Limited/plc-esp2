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

#include "controllers/controllers.hpp"

#include <LittleFS.h>

Controllers::Controllers(Gpio &gpio, OneWireManager &ow, EepromStorage &storage, Logger &logs,
 GsmModem &gsm, RTC &rtc)
 : _sockets(gpio, logs),
 _meteo(ow, logs),
 _thermo(gpio, _meteo, logs),
 _tanks(gpio, logs),
 _septic(gpio, logs),
 _security(gpio, ow, logs),
 _ring(gpio, logs),
 _watering(gpio, _tanks, rtc, logs),
 _avr(gpio, logs),
 _leak(gpio, logs),
 _storage(storage),
 _logs(logs){
    _security.setGsmModem(gsm);
}

bool Controllers::begin(){
    auto guard = _lock.guard();
    _logs.info(F("CTRL"), F("Init begin"));
    const bool sockets_enabled = _sockets.controllerEnabled() || _sockets.lightsEnabled();
    if (sockets_enabled)
        _logs.info(F("CTRL"), F("Sockets init"));
    if (!_sockets.begin())
    {
        _logs.error(F("CTRL"), F("Sockets init failed"));
        return false;
    }
    if (_meteo.controllerEnabled())
        _logs.info(F("CTRL"), F("Meteo init"));
    if (!_meteo.begin())
    {
        _logs.error(F("CTRL"), F("Meteo init failed"));
        return false;
    }
    if (_thermo.controllerEnabled())
        _logs.info(F("CTRL"), F("Thermo init"));
    if (!_thermo.begin())
    {
        _logs.error(F("CTRL"), F("Thermo init failed"));
        return false;
    }
    if (_tanks.controllerEnabled())
        _logs.info(F("CTRL"), F("Tanks init"));
    if (!_tanks.begin())
    {
        _logs.error(F("CTRL"), F("Tanks init failed"));
        return false;
    }
    if (_septic.controllerEnabled())
        _logs.info(F("CTRL"), F("Septic init"));
    if (!_septic.begin())
    {
        _logs.error(F("CTRL"), F("Septic init failed"));
        return false;
    }
    if (_security.controllerEnabled())
        _logs.info(F("CTRL"), F("Security init"));
    if (!_security.begin())
    {
        _logs.error(F("CTRL"), F("Security init failed"));
        return false;
    }
    if (_ring.controllerEnabled())
        _logs.info(F("CTRL"), F("Ring init"));
    if (!_ring.begin())
    {
        _logs.error(F("CTRL"), F("Ring init failed"));
        return false;
    }
    if (_watering.controllerEnabled())
        _logs.info(F("CTRL"), F("Watering init"));
    if (!_watering.begin())
    {
        _logs.error(F("CTRL"), F("Watering init failed"));
        return false;
    }
    if (_avr.controllerEnabled())
        _logs.info(F("CTRL"), F("AVR init"));
    if (!_avr.begin())
    {
        _logs.error(F("CTRL"), F("AVR init failed"));
        return false;
    }
    if (_leak.controllerEnabled())
        _logs.info(F("CTRL"), F("Leak init"));
    if (!_leak.begin())
    {
        _logs.error(F("CTRL"), F("Leak init failed"));
        return false;
    }
    _logs.info(F("CTRL"), F("Init done"));
    return true;
}

void Controllers::task(){
    saveIfNeeded_();
}

void Controllers::applyConfig(JsonObjectConst cfg){
    auto guard = _lock.guard();
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
    if (cfg["watering_enabled"].is<bool>())
        _watering.setControllerEnabled(cfg["watering_enabled"].as<bool>());
    if (cfg["avr_enabled"].is<bool>())
        _avr.setControllerEnabled(cfg["avr_enabled"].as<bool>());
    if (cfg["leak_enabled"].is<bool>())
        _leak.setControllerEnabled(cfg["leak_enabled"].as<bool>());
    if (cfg["ring"].is<JsonObjectConst>())
        _ring.applyConfig(cfg["ring"].as<JsonObjectConst>());
    if (cfg["avr"].is<JsonObjectConst>())
        _avr.applyConfig(cfg["avr"].as<JsonObjectConst>());
    if (cfg["leak"].is<JsonArrayConst>())
        _leak.applyConfig(cfg["leak"].as<JsonArrayConst>());
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
    if (cfg["watering"].is<JsonArrayConst>())
        _watering.applyConfig(cfg["watering"].as<JsonArrayConst>());
    if (cfg["security_siren"].is<unsigned>())
    {
        const unsigned raw = cfg["security_siren"].as<unsigned>();
        if (raw <= 0xFFu)
            _security.setSirenPort((uint8_t)raw);
    }
}

void Controllers::serialize(JsonObject out) const{
    auto guard = _lock.guard();
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
    out["watering_enabled"] = _watering.controllerEnabled();
    JsonArray watering = out["watering"].to<JsonArray>();
    _watering.serialize(watering);
    out["avr_enabled"] = _avr.controllerEnabled();
    JsonObject avr = out["avr"].to<JsonObject>();
    _avr.serialize(avr);
    out["leak_enabled"] = _leak.controllerEnabled();
    JsonArray leak = out["leak"].to<JsonArray>();
    _leak.serialize(leak);
    JsonObject ring = out["ring"].to<JsonObject>();
    _ring.serialize(ring);
    if (_security.sirenPort() != SecurityController::kInvalidPort)
        out["security_siren"] = (unsigned)_security.sirenPort();
}

SocketController &Controllers::sockets(){ return _sockets; }

const SocketController &Controllers::sockets() const{ return _sockets; }

MeteoController &Controllers::meteo(){ return _meteo; }

const MeteoController &Controllers::meteo() const{ return _meteo; }

ThermoController &Controllers::thermo(){ return _thermo; }

const ThermoController &Controllers::thermo() const{ return _thermo; }

TankController &Controllers::tanks(){ return _tanks; }

const TankController &Controllers::tanks() const{ return _tanks; }

SepticController &Controllers::septic(){ return _septic; }

const SepticController &Controllers::septic() const{ return _septic; }

SecurityController &Controllers::security(){ return _security; }

const SecurityController &Controllers::security() const{ return _security; }

RingController &Controllers::ring(){ return _ring; }

const RingController &Controllers::ring() const{ return _ring; }

WateringController &Controllers::watering(){ return _watering; }

const WateringController &Controllers::watering() const{ return _watering; }

AvrController &Controllers::avr(){ return _avr; }

const AvrController &Controllers::avr() const{ return _avr; }

LeakController &Controllers::leak(){ return _leak; }

const LeakController &Controllers::leak() const{ return _leak; }

void Controllers::invalidateGpioUsageCache() const{
    auto guard = _lock.guard();
    _gpio_usage_cache.valid = false;
}

bool Controllers::gpioPortUsed(uint8_t port) const{
    if (port >= PortIO::PORT_COUNT)
        return false;
    ensureGpioUsageCache_();
    auto guard = _lock.guard();
    return _gpio_usage_cache.used[port];
}

bool Controllers::gpioPortUsedByType(uint8_t port, PortIO::PinType type) const{
    auto guard = _lock.guard();
    if (port >= PortIO::PORT_COUNT)
        return false;
    const auto &p = ActiveBoardProfile::PORTS[port];
    if (p.caps == Cap::None || p.type != type)
        return false;
    return gpioPortUsed(port);
}

void Controllers::setSaveIntervalMs(uint32_t ms){
    auto guard = _lock.guard();
    _save_interval_ms = ms;
}

bool Controllers::eepromSaveEnabled() const{
    auto guard = _lock.guard();
    return _eeprom_save_enabled;
}

bool Controllers::eepromLoadEnabled() const{
    auto guard = _lock.guard();
    return _eeprom_load_enabled;
}

void Controllers::setEepromSaveEnabled(bool enabled){
    auto guard = _lock.guard();
    _eeprom_save_enabled = enabled;
}

void Controllers::setEepromLoadEnabled(bool enabled){
    auto guard = _lock.guard();
    _eeprom_load_enabled = enabled;
}

void Controllers::restoreFromStorage()
{
    auto guard = _lock.guard();
    if (!_eeprom_load_enabled)
        return;
    loadFromStorage_();
}

bool Controllers::ensureSocketConfigsLoaded()
{
    bool need_sockets = false;
    bool need_lights = false;
    {
        auto guard = _lock.guard();
        if (_sockets.controllerEnabled())
        {
            need_sockets = true;
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = _sockets.configByIndex(i);
                if (cfg && cfg->enabled)
                {
                    need_sockets = false;
                    break;
                }
            }
        }
        if (_sockets.lightsEnabled())
        {
            need_lights = true;
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = _sockets.lightConfigByIndex(i);
                if (cfg && cfg->enabled)
                {
                    need_lights = false;
                    break;
                }
            }
        }
    }
    if (!need_sockets && !need_lights)
        return false;

    if (!LittleFS.exists(Configs::kPath))
        return false;
    File f = LittleFS.open(Configs::kPath, "r");
    if (!f)
        return false;
    DynamicJsonDocument doc(32768);
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err || !doc["controllers"].is<JsonObjectConst>())
        return false;

    const JsonObjectConst cfg = doc["controllers"].as<JsonObjectConst>();
    const bool has_lights = cfg["lights"].is<JsonArrayConst>();
    const bool use_legacy_lights = !has_lights && (need_sockets || need_lights);
    bool recovered = false;

    if ((need_sockets || use_legacy_lights) && cfg["sockets"].is<JsonArrayConst>())
    {
        _sockets.applyConfig(cfg["sockets"].as<JsonArrayConst>(), use_legacy_lights);
        recovered = true;
    }
    if (has_lights && need_lights)
    {
        _sockets.applyLightsConfig(cfg["lights"].as<JsonArrayConst>());
        recovered = true;
    }
    if (!recovered)
        return false;

    if (need_sockets || use_legacy_lights)
        _sockets.reinitializeConfiguredSockets();
    if (need_lights || use_legacy_lights)
        _sockets.reinitializeConfiguredLights();
    invalidateGpioUsageCache();
    return true;
}

void Controllers::markPortUsed_(bool used[], uint8_t port){
    if (port < PortIO::PORT_COUNT)
        used[port] = true;
}

void Controllers::rebuildGpioUsageCache_(bool used[]) const{
    for (size_t i = 0; i < PortIO::PORT_COUNT; ++i)
        used[i] = false;

    {
        auto sockets_guard = _sockets.lockGuard();
        if (_sockets.controllerEnabled())
        {
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = _sockets.configByIndex(i);
                if (!cfg)
                    continue;
                if (!cfg->enabled)
                    continue;
                markPortUsed_(used, cfg->button_port);
                markPortUsed_(used, cfg->relay_port);
            }
        }
        if (_sockets.lightsEnabled())
        {
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = _sockets.lightConfigByIndex(i);
                if (!cfg)
                    continue;
                if (!cfg->enabled)
                    continue;
                markPortUsed_(used, cfg->button_port);
                markPortUsed_(used, cfg->relay_port);
            }
        }
    }
    {
        auto meteo_guard = _meteo.lockGuard();
        if (_meteo.controllerEnabled())
        {
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = _meteo.configByIndex(i);
                if (!cfg)
                    continue;
                if (!cfg->enabled)
                    continue;
                if (cfg->type != MeteoController::SensorType::Dht22)
                    continue;
                markPortUsed_(used, cfg->dht_pin);
            }
        }
    }
    {
        auto thermo_guard = _thermo.lockGuard();
        if (_thermo.controllerEnabled())
        {
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = _thermo.configByIndex(i);
                if (!cfg)
                    continue;
                if (!cfg->enabled)
                    continue;
                markPortUsed_(used, cfg->heat_port);
                markPortUsed_(used, cfg->cool_port);
                markPortUsed_(used, cfg->button_port);
            }
        }
    }
    {
        auto tanks_guard = _tanks.lockGuard();
        if (_tanks.controllerEnabled())
        {
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = _tanks.configByIndex(i);
                if (!cfg)
                    continue;
                if (!cfg->enabled)
                    continue;
                markPortUsed_(used, cfg->level_low);
                markPortUsed_(used, cfg->level_mid);
                markPortUsed_(used, cfg->level_full);
                markPortUsed_(used, cfg->relay_valve);
                markPortUsed_(used, cfg->relay_pump);
                markPortUsed_(used, cfg->relay_alarm);
            }
        }
    }
    {
        auto septic_guard = _septic.lockGuard();
        if (_septic.controllerEnabled())
        {
            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = _septic.configByIndex(i);
                if (!cfg)
                    continue;
                if (!cfg->enabled)
                    continue;
                markPortUsed_(used, cfg->warning_port);
                markPortUsed_(used, cfg->alarm_port);
                markPortUsed_(used, cfg->relay_warning);
                markPortUsed_(used, cfg->relay_alarm);
            }
        }
    }
    {
        auto security_guard = _security.lockGuard();
        if (_security.controllerEnabled())
        {
            if (_security.sirenPort() != SecurityController::kInvalidPort)
                markPortUsed_(used, _security.sirenPort());
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                const auto *cfg = _security.configByIndex(i);
                if (!cfg)
                    continue;
                if (!cfg->enabled)
                    continue;
                markPortUsed_(used, cfg->port);
            }
        }
    }
    {
        auto ring_guard = _ring.lockGuard();
        if (_ring.controllerEnabled())
        {
            const auto &rcfg = _ring.config();
            markPortUsed_(used, rcfg.button_port);
            markPortUsed_(used, rcfg.relay_port);
        }
    }
    {
        auto avr_guard = _avr.lockGuard();
        if (_avr.controllerEnabled())
        {
            const auto &acfg = _avr.config();
            markPortUsed_(used, acfg.main_ok_port);
            markPortUsed_(used, acfg.reserve_ok_port);
            markPortUsed_(used, acfg.feedback_main_port);
            markPortUsed_(used, acfg.feedback_reserve_port);
            markPortUsed_(used, acfg.relay_main_port);
            markPortUsed_(used, acfg.relay_reserve_port);
        }
    }

    auto leak_guard = _leak.lockGuard();
    if (_leak.controllerEnabled())
    {
        for (size_t i = 0; i < LeakController::kZoneCount; ++i)
        {
            const auto *cfg = _leak.configByIndex(i);
            if (!cfg)
                continue;
            if (!cfg->enabled)
                continue;
            markPortUsed_(used, cfg->sensor_port);
            markPortUsed_(used, cfg->valve_port);
            markPortUsed_(used, cfg->alarm_port);
        }
    }

    auto watering_guard = _watering.lockGuard();
    if (_watering.controllerEnabled())
    {
        for (size_t i = 0; i < WateringController::kRuleCount; ++i)
        {
            const auto *cfg = _watering.configByIndex(i);
            if (!cfg)
                continue;
            if (!cfg->enabled)
                continue;
            markPortUsed_(used, cfg->port);
        }
    }
}

void Controllers::ensureGpioUsageCache_() const{
    const uint32_t now = millis();
    {
        auto guard = _lock.guard();
        if (_gpio_usage_cache.valid && (uint32_t)(now - _gpio_usage_cache.built_ms) < 1000u)
            return;
    }
    bool used[PortIO::PORT_COUNT] = {};
    rebuildGpioUsageCache_(used);
    auto guard = _lock.guard();
    memcpy(_gpio_usage_cache.used, used, sizeof(used));
    _gpio_usage_cache.valid = true;
    _gpio_usage_cache.built_ms = now;
}

void Controllers::loadFromStorage_(){
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
        _thermo.applyTargetSnapshot(tsnap.target_c, EepromStorage::kThermoCount);
    }
    EepromStorage::TankSnapshot tanksnap;
    if (_storage.loadTanks(tanksnap))
        _tanks.applySnapshot(tanksnap.power_mask, EepromStorage::kTankMaskBytes);
    EepromStorage::SecuritySnapshot ssnap;
    if (_storage.loadSecurity(ssnap))
        _security.applySnapshot(ssnap.flags);
    EepromStorage::WateringSnapshot wsnap;
    if (_storage.loadWatering(wsnap))
        _watering.applySnapshot(wsnap.status_mask, EepromStorage::kWateringMaskBytes);
    EepromStorage::WateringRuntimeSnapshot wtsnap;
    if (_storage.loadWateringRuntime(wtsnap))
        _watering.applyRuntimeSnapshot(wtsnap.active_mask, wtsnap.paused_mask, wtsnap.remaining_ms,
                                       wtsnap.last_start_key, EepromStorage::kWateringMaskBytes);
}

void Controllers::saveIfNeeded_(){
    bool eeprom_save_enabled = true;
    uint32_t save_interval_ms = 0;
    uint32_t last_save_ms = 0;
    {
        auto guard = _lock.guard();
        eeprom_save_enabled = _eeprom_save_enabled;
        save_interval_ms = _save_interval_ms;
        last_save_ms = _last_save_ms;
    }
    if (!eeprom_save_enabled)
        return;
    if (!_storage.isReady())
        return;
    const uint32_t now = millis();
    const bool force_security = _security.takeForceSave();
    if (!force_security && save_interval_ms && (uint32_t)(now - last_save_ms) < save_interval_ms)
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
        _thermo.buildTargetSnapshot(tsnap.target_c, EepromStorage::kThermoCount);
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
        _security.buildSnapshot(ssnap.flags);
        if (_storage.saveSecurity(ssnap))
            saved = true;
    }
    else if (force_security)
    {
        EepromStorage::SecuritySnapshot ssnap;
        _security.buildSnapshot(ssnap.flags);
        if (_storage.saveSecurity(ssnap))
            saved = true;
    }
    if (_watering.takeDirty())
    {
        EepromStorage::WateringSnapshot wsnap;
        _watering.buildSnapshot(wsnap.status_mask, EepromStorage::kWateringMaskBytes);
        if (_storage.saveWatering(wsnap))
            saved = true;
    }
    if (_watering.takeRuntimeDirty())
    {
        EepromStorage::WateringRuntimeSnapshot wtsnap;
        _watering.buildRuntimeSnapshot(wtsnap.active_mask, wtsnap.paused_mask, wtsnap.remaining_ms,
                                       wtsnap.last_start_key, EepromStorage::kWateringMaskBytes);
        if (_storage.saveWateringRuntime(wtsnap))
            saved = true;
    }
    if (saved)
    {
        auto guard = _lock.guard();
        _last_save_ms = now;
    }
}
