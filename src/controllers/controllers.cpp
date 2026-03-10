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

Controllers::Controllers(Gpio &gpio, OneWireManager &ow, EepromStorage &storage, Logger &logs,
 TelegramBot &tgbot, TelegramAllowedUsersProvider &tgmenu, GsmModem &gsm, RTC &rtc)
 : _sockets(gpio, logs),
 _meteo(ow, logs),
 _thermo(gpio, _meteo, logs),
 _tanks(gpio, logs, tgbot, tgmenu),
 _septic(gpio, logs, tgbot, tgmenu),
 _security(gpio, ow, logs, tgbot, tgmenu),
 _ring(gpio, logs),
 _watering(gpio, _tanks, rtc, logs),
 _avr(gpio, logs, tgbot, tgmenu),
 _leak(gpio, logs, tgbot, tgmenu),
 _storage(storage),
 _logs(logs){
    _security.setGsmModem(gsm);
}

bool Controllers::begin(){
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

void Controllers::invalidateGpioUsageCache() const{ _gpio_usage_cache.valid = false; }

bool Controllers::gpioPortUsed(uint8_t port) const{
    if (port >= PortIO::PORT_COUNT)
        return false;
    ensureGpioUsageCache_();
    return _gpio_usage_cache.used[port];
}

bool Controllers::gpioPortUsedByType(uint8_t port, PortIO::PinType type) const{
    if (port >= PortIO::PORT_COUNT)
        return false;
    const auto &p = ActiveBoardProfile::PORTS[port];
    if (p.caps == Cap::None || p.type != type)
        return false;
    return gpioPortUsed(port);
}

void Controllers::setSaveIntervalMs(uint32_t ms){ _save_interval_ms = ms; }

bool Controllers::eepromSaveEnabled() const{ return _eeprom_save_enabled; }

bool Controllers::eepromLoadEnabled() const{ return _eeprom_load_enabled; }

void Controllers::setEepromSaveEnabled(bool enabled){ _eeprom_save_enabled = enabled; }

void Controllers::setEepromLoadEnabled(bool enabled){ _eeprom_load_enabled = enabled; }

void Controllers::restoreFromStorage()
{
    if (!_eeprom_load_enabled)
        return;
    loadFromStorage_();
}

void Controllers::markPortUsed_(bool used[], uint8_t port){
    if (port < PortIO::PORT_COUNT)
        used[port] = true;
}

void Controllers::rebuildGpioUsageCache_() const{
    for (size_t i = 0; i < PortIO::PORT_COUNT; ++i)
        _gpio_usage_cache.used[i] = false;

    for (size_t i = 0; i < SocketController::kSocketCount; ++i)
    {
        const auto *cfg = _sockets.configByIndex(i);
        if (!cfg)
            continue;
        if (!cfg->enabled)
            continue;
        markPortUsed_(_gpio_usage_cache.used, cfg->button_port);
        markPortUsed_(_gpio_usage_cache.used, cfg->relay_port);
    }
    for (size_t i = 0; i < SocketController::kLightCount; ++i)
    {
        const auto *cfg = _sockets.lightConfigByIndex(i);
        if (!cfg)
            continue;
        if (!cfg->enabled)
            continue;
        markPortUsed_(_gpio_usage_cache.used, cfg->button_port);
        markPortUsed_(_gpio_usage_cache.used, cfg->relay_port);
    }
    for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
    {
        const auto *cfg = _meteo.configByIndex(i);
        if (!cfg)
            continue;
        if (cfg->type != MeteoController::SensorType::Dht22)
            continue;
        markPortUsed_(_gpio_usage_cache.used, cfg->dht_pin);
    }
    for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
    {
        const auto *cfg = _thermo.configByIndex(i);
        if (!cfg)
            continue;
        markPortUsed_(_gpio_usage_cache.used, cfg->heat_port);
        markPortUsed_(_gpio_usage_cache.used, cfg->cool_port);
        markPortUsed_(_gpio_usage_cache.used, cfg->button_port);
    }
    for (size_t i = 0; i < TankController::kTankCount; ++i)
    {
        const auto *cfg = _tanks.configByIndex(i);
        if (!cfg)
            continue;
        markPortUsed_(_gpio_usage_cache.used, cfg->level_low);
        markPortUsed_(_gpio_usage_cache.used, cfg->level_mid);
        markPortUsed_(_gpio_usage_cache.used, cfg->level_full);
        markPortUsed_(_gpio_usage_cache.used, cfg->relay_valve);
        markPortUsed_(_gpio_usage_cache.used, cfg->relay_pump);
        markPortUsed_(_gpio_usage_cache.used, cfg->relay_alarm);
    }
    for (size_t i = 0; i < SepticController::kSepticCount; ++i)
    {
        const auto *cfg = _septic.configByIndex(i);
        if (!cfg)
            continue;
        markPortUsed_(_gpio_usage_cache.used, cfg->warning_port);
        markPortUsed_(_gpio_usage_cache.used, cfg->alarm_port);
        markPortUsed_(_gpio_usage_cache.used, cfg->relay_warning);
        markPortUsed_(_gpio_usage_cache.used, cfg->relay_alarm);
    }
    if (_security.sirenPort() != SecurityController::kInvalidPort)
        markPortUsed_(_gpio_usage_cache.used, _security.sirenPort());
    for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
    {
        const auto *cfg = _security.configByIndex(i);
        if (!cfg)
            continue;
        markPortUsed_(_gpio_usage_cache.used, cfg->port);
    }
    const auto &rcfg = _ring.config();
    markPortUsed_(_gpio_usage_cache.used, rcfg.button_port);
    markPortUsed_(_gpio_usage_cache.used, rcfg.relay_port);

    const auto &acfg = _avr.config();
    markPortUsed_(_gpio_usage_cache.used, acfg.main_ok_port);
    markPortUsed_(_gpio_usage_cache.used, acfg.reserve_ok_port);
    markPortUsed_(_gpio_usage_cache.used, acfg.feedback_main_port);
    markPortUsed_(_gpio_usage_cache.used, acfg.feedback_reserve_port);
    markPortUsed_(_gpio_usage_cache.used, acfg.relay_main_port);
    markPortUsed_(_gpio_usage_cache.used, acfg.relay_reserve_port);

    for (size_t i = 0; i < LeakController::kZoneCount; ++i)
    {
        const auto *cfg = _leak.configByIndex(i);
        if (!cfg)
            continue;
        markPortUsed_(_gpio_usage_cache.used, cfg->sensor_port);
        markPortUsed_(_gpio_usage_cache.used, cfg->valve_port);
        markPortUsed_(_gpio_usage_cache.used, cfg->alarm_port);
    }

    for (size_t i = 0; i < WateringController::kRuleCount; ++i)
    {
        const auto *cfg = _watering.configByIndex(i);
        if (!cfg)
            continue;
        markPortUsed_(_gpio_usage_cache.used, cfg->port);
    }
}

void Controllers::ensureGpioUsageCache_() const{
    const uint32_t now = millis();
    if (_gpio_usage_cache.valid && _gpio_usage_cache.built_ms == now)
        return;
    rebuildGpioUsageCache_();
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
        _thermo.applyTargetSnapshot(tsnap.target_t10, EepromStorage::kThermoCount);
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
    if (!_eeprom_save_enabled)
        return;
    if (!_storage.isReady())
        return;
    const uint32_t now = millis();
    const bool force_security = _security.takeForceSave();
    if (!force_security && _save_interval_ms && (uint32_t)(now - _last_save_ms) < _save_interval_ms)
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
        _last_save_ms = now;
}
