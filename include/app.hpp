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
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>

#include "boards/board_profile.hpp"

#include "core/task_binder.hpp"
#include "core/task_manager.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/gsm_modem.hpp"
#include "core/rtc.hpp"
#include "core/display.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "core/eeprom_storage.hpp"
#include "core/network/network.hpp"
#include "core/network/stack/stack_slave_handler.hpp"
#include "core/cli/cli_console.hpp"
#include "core/network/web/web_interface.hpp"
#include "core/plc_scan.hpp"
#include "core/rules_controller.hpp"

#include "hal/at24lc512.hpp"
#include "hal/dht22.hpp"
#include "hal/ds18b20.hpp"
#include "hal/ds3231mz.hpp"
#include "hal/lcd1602_i2c.hpp"
#include "hal/lm75ad.hpp"
#include "hal/sim800l.hpp"
#include "hal/ibutton.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/gpio/portio.hpp"
#include "hal/gpio/gpio.hpp"
#include "hal/io_stack.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/bus/spi.hpp"
#include "hal/bus/uart.hpp"
#include "hal/hal.hpp"

#include "plc/plc_control.hpp"
#include "controllers/controllers.hpp"
#include "ftest.hpp"

#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager.hpp"
#include "utils/meteo_history.hpp"
#include "utils/users_registry.hpp"

struct CoreContext
{
    UartManager uart;
    Logger logs;
    TaskManager<TASK_MGR_TSK_COUNT> tm;
    Configs configs;

    CoreContext();
};

struct HardwareContext
{
    I2CManager i2c;
    SPIManager spi;
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

    Extender ext;
    PortIO portio;
    IoStack io;
    Gpio gpio;

    Hal hal;
    PlcControl plc;

    HardwareContext(Logger &logs, UartManager &uart);
};

struct CommsContext
{
    WifiManager wifi;
    Sim800l sim800l;
    GsmModem gsm;

    WiFiClientSecure telegram_wifi_client;
    TelegramClient telegram;
    TelegramBot telegram_bot;

    CommsContext(Logger &logs, UartManager &uart);
};

struct ControlContext
{
    UsersRegistry users;
    TelegramMenu telegram_menu;
    Controllers controllers;
    RulesController rules;
    MeteoHistory meteo_history;
    PlcScanLoop plc_scan;

    TaskBinder<TASK_MGR_TSK_COUNT> task_binder;
    Ftest ftest;

    ControlContext(CoreContext &core, HardwareContext &hw, CommsContext &comms);
};

struct UiContext
{
    CliConsole console;

    UiContext(CoreContext &core, HardwareContext &hw, CommsContext &comms, ControlContext &control);
};

struct NetworkContext
{
    AsyncWebServer web;
    WebInterface fw_upgrade;
    Network network;
    StackSlaveHandler stack_slave;

    NetworkContext(CoreContext &core, HardwareContext &hw, CommsContext &comms, ControlContext &control, UiContext &ui);
};

struct ConfigContext
{
    ConfigsManager configs_manager;

    ConfigContext(CoreContext &core, HardwareContext &hw, CommsContext &comms,
                  ControlContext &control, UiContext &ui, NetworkContext &network);
};

#include "core/stack/stack_runtime.hpp"

struct App
{
    CoreContext core;
    HardwareContext hw;
    CommsContext comms;
    ControlContext control;
    UiContext ui;
    NetworkContext net;
    ConfigContext cfg;
    StackRuntime stack;

    App();

    bool begin();

    void loop();
};
