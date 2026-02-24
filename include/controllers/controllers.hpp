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

#include "controllers/avr_controller.hpp"
#include "controllers/leak_controller.hpp"
#include "controllers/meteo_controller.hpp"
#include "controllers/ring_controller.hpp"
#include "controllers/septic_controller.hpp"
#include "controllers/security_controller.hpp"
#include "controllers/socket_controller.hpp"
#include "controllers/tank_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "controllers/watering_controller.hpp"
#include "core/eeprom_storage.hpp"
#include "core/network/gsm_modem.hpp"
#include "core/network/telegram/telegram_allowed_users.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/rtc.hpp"
#include "hal/gpio/gpio.hpp"
#include "utils/logger.hpp"

class Controllers
{
public:
    Controllers(Gpio &gpio, OneWireManager &ow, EepromStorage &storage, Logger &logs,
                TelegramBot &tgbot, TelegramAllowedUsersProvider &tgmenu, GsmModem &gsm, RTC &rtc)
        ;bool begin();void task();void applyConfig(JsonObjectConst cfg);void serialize(JsonObject out) const;SocketController &sockets();const SocketController &sockets() const;MeteoController &meteo();const MeteoController &meteo() const;ThermoController &thermo();const ThermoController &thermo() const;TankController &tanks();const TankController &tanks() const;SepticController &septic();const SepticController &septic() const;SecurityController &security();const SecurityController &security() const;RingController &ring();const RingController &ring() const;WateringController &watering();const WateringController &watering() const;AvrController &avr();const AvrController &avr() const;LeakController &leak();const LeakController &leak() const;void invalidateGpioUsageCache() const;bool gpioPortUsed(uint8_t port) const;bool gpioPortUsedByType(uint8_t port, PortIO::PinType type) const;void setSaveIntervalMs(uint32_t ms);private:
    struct GpioUsageCache
    {
        bool valid = false;
        uint32_t built_ms = 0;
        bool used[PortIO::PORT_COUNT] = {};
    };

    SocketController _sockets;
    MeteoController _meteo;
    ThermoController _thermo;
    TankController _tanks;
    SepticController _septic;
    SecurityController _security;
    RingController _ring;
    WateringController _watering;
    AvrController _avr;
    LeakController _leak;
    EepromStorage &_storage;
    Logger &_logs;
    uint32_t _last_save_ms = 0;
    uint32_t _save_interval_ms = 10000;
    mutable GpioUsageCache _gpio_usage_cache;

    static void markPortUsed_(bool used[], uint8_t port);void rebuildGpioUsageCache_() const;void ensureGpioUsageCache_() const;void loadFromStorage_();void saveIfNeeded_();};
