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
#include <vector>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "boards/board_profile.hpp"
#include "boards/profile_validator.hpp"

#include "core/task_binder.hpp"
#include "core/task_manager.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "core/display.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "core/eeprom_storage.hpp"
#include "core/network/network.hpp"
#include "core/network/stack/stack_slave_handler.hpp"
#include "core/cli/cli_console.hpp"

#include "hal/dht22.hpp"
#include "hal/at24lc512.hpp"
#include "hal/ds18b20.hpp"
#include "hal/ds3231mz.hpp"
#include "hal/lcd1602_i2c.hpp"
#include "hal/lm75ad.hpp"
#include "hal/sim800l.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/gpio/gpio.hpp"
#include "hal/gpio/portio.hpp"
#include "hal/io_stack.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/ibutton.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/bus/spi.hpp"
#include "hal/bus/uart.hpp"
#include "hal/hal.hpp"
#include "ftest.hpp"
#include "plc/plc_control.hpp"
#include "controllers/controllers.hpp"

#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager.hpp"
#include "utils/meteo_history.hpp"
#include "core/network/web/web_interface.hpp"
#include "core/plc_scan.hpp"
#include "hal/gpio/extender_impl.hpp"

struct AppServices
{
    UartManager uart;
    Logger logs;

    I2CManager i2c;
    SPIManager spi;
    WifiManager wifi;
    OneWireManager ow;
    IButton ibutton;
    Ds18b20 ds18b20;
    DHT22 dht22;
    Ds3231Mz ds3231;
    RTC rtc;
    At24lc512 eeprom;
    EepromStorage eeprom_storage;
    Lcd1602I2c lcd_hal;
    Display display;
    Lm75ad lm75ad;
    Sim800l sim800l;
    WiFiClientSecure telegram_wifi_client;
    TelegramClient telegram;

    Extender ext;
    PortIO portio;
    IoStack io;
    Gpio gpio;
    Hal hal;
    PlcControl plc;
    Configs configs;
    TelegramBot telegram_bot;
    TelegramMenu telegram_menu;
    Controllers controllers;
    MeteoHistory meteo_history;

    TaskManager<TASK_MGR_TSK_COUNT> tm;
    TaskBinder<TASK_MGR_TSK_COUNT> task_binder;
    Ftest ftest;
    CliConsole console;

    AsyncWebServer web;
    WebInterface fw_upgrade;
    Network network;
    StackSlaveHandler stack_slave;

    ConfigsManager configs_manager;
    PlcScanLoop plc_scan;

    AppServices()
        : logs(uart),
          wifi(logs),
          ibutton(),
          ds18b20(),
          dht22(),
          ds3231(),
          rtc(i2c, ds3231),
          eeprom(),
          eeprom_storage(eeprom),
          lcd_hal(),
          display(i2c, lcd_hal),
          lm75ad(),
          sim800l(),
          telegram_wifi_client(),
          telegram(logs),
          ext(i2c, ActiveBoardProfile::EXT_DEVS, &logs),
          portio(ActiveBoardProfile::PORTS, &ext),
          io(portio),
          gpio(io),
          hal(ow, i2c, spi, uart, gpio, logs),
          plc(i2c, io),
          configs(),
          telegram_bot(telegram),
          telegram_menu(plc, wifi, rtc, telegram_bot, configs, logs),
          controllers(gpio, ow, eeprom_storage, logs, telegram_bot, telegram_menu),
          meteo_history(rtc, controllers.meteo()),
          tm(),
          task_binder(tm, wifi, telegram, ext, controllers, meteo_history),
          ftest(logs, io, ow, ibutton, ds18b20, i2c, rtc, ext, tm, task_binder),
          console(plc, wifi, rtc, ftest, i2c, ow, telegram, telegram_menu, configs, ext, controllers, nullptr),
          web(ActiveBoardProfile::WEB_PORT),
          fw_upgrade(web, console, wifi, configs, plc, rtc, telegram, telegram_menu, logs, ext, i2c, ow, controllers),
          network(logs, wifi, telegram, telegram_bot, telegram_menu, fw_upgrade, web, telegram_wifi_client),
          stack_slave(io, ds18b20, ow, i2c, plc, rtc, telegram, logs, ext,
                      controllers.sockets(), controllers.meteo(), controllers.thermo(), controllers.septic(),
                      controllers.security()),
          configs_manager(configs, wifi, telegram, network, console, telegram_menu, plc, controllers),
          plc_scan(io, plc)
    {
        logs.setRtc(rtc);
        console.setStackMaster(network.stackMaster());
        console.setConfigsManager(configs_manager);
        telegram_menu.setConfigsManager(configs_manager);
        telegram_menu.setStackMaster(*network.stackMaster());
        telegram_menu.setSockets(controllers.sockets());
        telegram_menu.setMeteo(controllers.meteo());
        telegram_menu.setThermo(controllers.thermo());
        telegram_menu.setTanks(controllers.tanks());
        telegram_menu.setSeptic(controllers.septic());
        telegram_menu.setSecurity(controllers.security());
        fw_upgrade.setConfigsManager(configs_manager);
        fw_upgrade.setStackMaster(*network.stackMaster());
        network.setStackConfig(configs_manager);
#if defined(ESP32)
        if (auto *node = network.stackNode())
            stack_slave.attach(*node);
#endif
    }

    bool begin()
    {
        if (!logs.beginAuto())
        {
            Serial.begin(UartManager::DEFAULT_SPEED);
            logs.begin(Serial);
            logs.error(F("APP"), F("LOG Auto bind failed, fallback to USB"));
        }

        delay(1000);
        console.begin(Serial);

        logs.info(F("APP"), F("Starting application..."));
        ProfileValidator<ActiveBoardProfile>::printDiagnostics(Serial);

        if (!configs.begin())
        {
            logs.error(F("APP"), F("Configs mount failed"));
        }
        else
        {
            const bool loaded = configs_manager.loadConfigs();
            const Configs::Error cfg_err = configs.lastError();
            uint32_t cfg_size = 0;
            if (LittleFS.exists(Configs::kPath))
            {
                File f = LittleFS.open(Configs::kPath, "r");
                if (f)
                {
                    cfg_size = (uint32_t)f.size();
                    f.close();
                }
            }

            if (loaded && cfg_err == Configs::Error::Ok)
            {
                logs.info(F("APP"), F("Configs loaded: %s (%u bytes)"), Configs::kPath, (unsigned)cfg_size);
            }
            else
            {
                const char *err = "Unknown error";
                switch (cfg_err)
                {
                case Configs::Error::OpenRead:
                    err = "Open read failed";
                    break;
                case Configs::Error::OpenWrite:
                    err = "Open write failed";
                    break;
                case Configs::Error::JsonParse:
                    err = "JSON parse failed";
                    break;
                case Configs::Error::JsonSerialize:
                    err = "JSON serialize failed";
                    break;
                default:
                    break;
                }

                if (cfg_err == Configs::Error::OpenRead)
                {
                    logs.warn(F("APP"), F("Configs missing: %s (saving defaults)"), Configs::kPath);
                    if (!configs_manager.save())
                    {
                        const char *save_err = "Unknown error";
                        switch (configs.lastError())
                        {
                        case Configs::Error::OpenWrite:
                            save_err = "Open write failed";
                            break;
                        case Configs::Error::JsonSerialize:
                            save_err = "JSON serialize failed";
                            break;
                        default:
                            break;
                        }
                        logs.error(F("APP"), F("Configs save failed: %s"), save_err);
                    }
                }
                else
                {
                    logs.error(F("APP"), F("Configs load failed: %s (%s, %u bytes)"),
                               err, Configs::kPath, (unsigned)cfg_size);
                }
            }
        }
        network.setStackDeviceName(plc.deviceName());
        if (wifi.ap())
            logs.info(F("WIFI"), F("Mode: AP (SSID=%s)"), wifi.apSsid().c_str());
        else
            logs.info(F("WIFI"), F("Mode: STA (SSID=%s)"), wifi.ssid().c_str());

        bool ok = true;

        logs.info(F("APP"), F("Initializing HAL")); 
        if (!hal.begin())
        {
            logs.error(F("APP"), F("HAL init failed: %s"), Hal::errorName(hal.lastError()));
            ok = false;
        }

        logs.info(F("APP"), F("Initializing EEPROM"));
        {
            const auto cfg = ActiveBoardProfile::EEPROM;
            TwoWire *wire = i2c.wirePtr(cfg.bus_num);
            const bool eeprom_ok = wire && eeprom.begin(*wire, cfg.addr);
            eeprom_storage.setReady(eeprom_ok);
            if (!eeprom_ok)
                logs.warn(F("APP"), F("EEPROM init failed"));
            else
            {
                const uint32_t used = eeprom.usedBytes();
                const uint32_t total = At24lc512::capacityBytes();
                const uint32_t free = At24lc512::remainingBytes(used);
                logs.info(F("APP"), F("EEPROM used: %lu free: %lu total: %lu"),
                          (unsigned long)used, (unsigned long)free, (unsigned long)total);
            }
        }

        logs.info(F("APP"), F("Initializing RTC"));
        if (!rtc.begin())
        {
            switch (rtc.lastError())
            {
            case RTC::Error::NoBus:
                logs.warn(F("APP"), F("RTC I2C bus missing"));
                break;
            case RTC::Error::InvalidConfig:
                logs.warn(F("APP"), F("RTC config invalid"));
                break;
            case RTC::Error::I2c:
                logs.warn(F("APP"), F("RTC I2C error"));
                break;
            default:
                logs.warn(F("APP"), F("RTC Init failed"));
                break;
            }
        }

        logs.info(F("APP"), F("Initializing Display"));
        if (!display.begin())
        {
            switch (display.lastError())
            {
            case Display::Error::NoBus:
                logs.warn(F("APP"), F("LCD I2C bus missing"));
                break;
            case Display::Error::InvalidConfig:
                logs.warn(F("APP"), F("LCD config invalid"));
                break;
            case Display::Error::I2c:
                logs.warn(F("APP"), F("LCD I2C error"));
                break;
            default:
                logs.warn(F("APP"), F("LCD Init failed"));
                break;
            }
        }

        logs.info(F("APP"), F("Initializing PLC Control"));
        if (!plc.begin())
        {
            switch (plc.lastError())
            {
            case PlcControl::Error::NoBus:
                logs.error(F("APP"), F("PLC I2C bus missing"));
                break;
            case PlcControl::Error::InvalidConfig:
                logs.error(F("APP"), F("PLC temp config invalid"));
                break;
            case PlcControl::Error::I2c:
                logs.error(F("APP"), F("PLC temp sensor error"));
                break;
            default:
                logs.error(F("APP"), F("PLC Init failed"));
                break;
            }
            ok = false;
        }

        logs.info(F("APP"), F("Initializing Network"));
        if (!network.begin())
        {
            switch (network.lastError())
            {
            case Network::Error::Wifi:
                logs.error(F("APP"), F("WIFI Init failed"));
                break;
            case Network::Error::TelegramClientMissing:
                logs.error(F("APP"), F("Telegram client missing"));
                break;
            case Network::Error::TelegramProxyInvalid:
                logs.error(F("APP"), F("Telegram proxy invalid"));
                break;
            case Network::Error::WebInterfaceFs:
                logs.error(F("APP"), F("WebInterface FS mount failed"));
                break;
            default:
                logs.error(F("APP"), F("Network init failed"));
                break;
            }
            ok = false;
        }

        controllers.begin();

        if (ok)
            logs.info(F("APP"), F("Application init [OK]"));
        else
            logs.error(F("APP"), F("Application init [FAIL]"));

        task_binder.bindFtest(ftest);
        task_binder.bindAll();
        plc_scan.begin();

        return ok;
    }

    void loop()
    {
        plc_scan.tick();
        console.loop();
        const uint32_t budget_us = plc_scan.timeToNextUs();
        tm.loop(budget_us);
        if (budget_us > 200)
            network.loop();
    }

private:
};
