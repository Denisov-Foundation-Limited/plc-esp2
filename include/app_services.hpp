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
#include "core/network/network.hpp"
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
#include "hal/bus/i2c.hpp"
#include "hal/ibutton.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/bus/spi.hpp"
#include "hal/bus/uart.hpp"
#include "hal/hal.hpp"
#include "ftest.hpp"
#include "plc/plc_control.hpp"

#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager.hpp"
#include "core/network/web/web_interface.hpp"

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
    Lcd1602I2c lcd_hal;
    Display display;
    Lm75ad lm75ad;
    Sim800l sim800l;
    WiFiClientSecure telegram_wifi_client;
    TelegramClient telegram;

    Extender ext;
    PortIO portio;
    Gpio gpio;
    Hal hal;
    PlcControl plc;
    TelegramBot telegram_bot;
    TelegramMenu telegram_menu;
    Network network;
    AsyncWebServer web;
    WebInterface fw_upgrade;

    TaskManager<TASK_MGR_TSK_COUNT> tm;
    TaskBinder<TASK_MGR_TSK_COUNT> task_binder;
    Ftest ftest;
    CliConsole console;
    Configs configs;
    ConfigsManager configs_manager;

    AppServices()
        : logs(uart),
          wifi(logs),
          ibutton(),
          ds18b20(),
          dht22(),
          ds3231(),
          rtc(i2c, ds3231),
          eeprom(),
          lcd_hal(),
          display(i2c, lcd_hal),
          lm75ad(),
          sim800l(),
          telegram_wifi_client(),
          ext(i2c, ActiveBoardProfile::EXT_DEVS, &logs),
          portio(ActiveBoardProfile::PORTS, &ext),
          gpio(portio),
          hal(ow, i2c, spi, uart, gpio),
          plc(i2c, portio),
          telegram(logs),
          telegram_bot(telegram),
          telegram_menu(plc, wifi, rtc, telegram_bot, configs),
          web(ActiveBoardProfile::WEB_PORT),
          fw_upgrade(web, console, wifi, configs, plc, rtc, telegram, telegram_menu, logs, ext, i2c, ow),
          network(logs, wifi, telegram, telegram_bot, telegram_menu, fw_upgrade, web, telegram_wifi_client),
          tm(),
          task_binder(tm, wifi, plc, telegram, ext),
          ftest(logs, portio, ow, ibutton, ds18b20, i2c, tm, task_binder),
          console(plc, wifi, rtc, ftest, i2c, ow, telegram, telegram_menu, configs, ext),
          configs(),
          configs_manager(configs, wifi, telegram, network, console, telegram_menu)
    {
        console.setConfigsManager(configs_manager);
        telegram_menu.setConfigsManager(configs_manager);
        fw_upgrade.setConfigsManager(configs_manager);
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
        else if (!configs_manager.loadConfigs())
        {
            const char *err = "Unknown error";
            switch (configs.lastError())
            {
            case Configs::Error::FsMount:
                err = "FS mount failed";
                break;
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
            logs.error(F("APP"), F("Configs load failed: %s"), err);
        }

        bool ok = true;

        logs.info(F("APP"), F("Initializing HAL")); 
        if (!hal.begin())
        {
            switch (hal.lastError())
            {
            case Hal::Error::I2c:
                logs.error(F("APP"), F("HAL I2C Init failed"));
                break;
            case Hal::Error::Gpio:
                logs.error(F("APP"), F("HAL GPIO Init failed"));
                break;
            case Hal::Error::Spi:
                logs.error(F("APP"), F("HAL SPI Init failed"));
                break;
            case Hal::Error::OneWire:
                logs.error(F("APP"), F("HAL OW Init failed"));
                break;
            case Hal::Error::Uart:
                logs.error(F("APP"), F("HAL UART Init failed"));
                break;
            default:
                logs.error(F("APP"), F("HAL Init failed"));
                break;
            }
            ok = false;
        }

        logs.info(F("APP"), F("Initializing RTC"));
        if (!rtc.begin())
        {
            switch (rtc.lastError())
            {
            case RTC::Error::NoBus:
                logs.error(F("APP"), F("RTC I2C bus missing"));
                break;
            case RTC::Error::InvalidConfig:
                logs.error(F("APP"), F("RTC config invalid"));
                break;
            case RTC::Error::I2c:
                logs.error(F("APP"), F("RTC I2C error"));
                break;
            default:
                logs.error(F("APP"), F("RTC Init failed"));
                break;
            }
        }

        logs.info(F("APP"), F("Initializing Display"));
        if (!display.begin())
        {
            switch (display.lastError())
            {
            case Display::Error::NoBus:
                logs.error(F("APP"), F("LCD I2C bus missing"));
                break;
            case Display::Error::InvalidConfig:
                logs.error(F("APP"), F("LCD config invalid"));
                break;
            case Display::Error::I2c:
                logs.error(F("APP"), F("LCD I2C error"));
                break;
            default:
                logs.error(F("APP"), F("LCD Init failed"));
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

        if (ok)
            logs.info(F("APP"), F("Application init [OK]"));
        else
            logs.error(F("APP"), F("Application init [FAIL]"));

        task_binder.bindFtest(ftest);
        task_binder.bindAll();

        return ok;
    }

    void loop()
    {
        hal.loop();
        console.loop();
        tm.loop();
        network.loop();
    }

private:
};
