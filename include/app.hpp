#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "boards/board_profile.hpp"
#include "boards/profile_validator.hpp"

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

#include "hal/at24lc512.hpp"
#include "hal/dht22.hpp"
#include "hal/ds18b20.hpp"
#include "hal/ds3231mz.hpp"
#include "hal/lcd1602_i2c.hpp"
#include "hal/lm75ad.hpp"
#include "hal/sim800l.hpp"
#include "hal/ibutton.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/gpio/extender_impl.hpp"
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

struct CoreContext
{
    UartManager uart;
    Logger logs;
    TaskManager<TASK_MGR_TSK_COUNT> tm;
    Configs configs;

    CoreContext()
        : logs(uart),
          tm(),
          configs()
    {
    }
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

    HardwareContext(Logger &logs, UartManager &uart)
        : ds18b20(),
          dht22(),
          ds3231(),
          rtc(i2c, ds3231),
          eeprom(),
          eeprom_storage(eeprom),
          lcd_hal(),
          display(i2c, lcd_hal),
          lm75ad(),
          ext(i2c, ActiveBoardProfile::EXT_DEVS, &logs),
          portio(ActiveBoardProfile::PORTS, &ext),
          io(portio),
          gpio(io),
          hal(ow, i2c, spi, uart, gpio, logs),
          plc(i2c, io)
    {
    }
};

struct CommsContext
{
    WifiManager wifi;
    Sim800l sim800l;
    GsmModem gsm;

    WiFiClientSecure telegram_wifi_client;
    TelegramClient telegram;
    TelegramBot telegram_bot;

    CommsContext(Logger &logs, UartManager &uart)
        : wifi(logs),
          sim800l(),
          gsm(uart, sim800l, logs),
          telegram_wifi_client(),
          telegram(logs),
          telegram_bot(telegram)
    {
    }
};

struct ControlContext
{
    TelegramMenu telegram_menu;
    Controllers controllers;
    MeteoHistory meteo_history;

    TaskBinder<TASK_MGR_TSK_COUNT> task_binder;
    Ftest ftest;
    PlcScanLoop plc_scan;

    ControlContext(CoreContext &core, HardwareContext &hw, CommsContext &comms)
        : telegram_menu(hw.plc, comms.wifi, hw.rtc, comms.telegram_bot, core.configs, core.logs),
          controllers(hw.gpio, hw.ow, hw.eeprom_storage, core.logs, comms.telegram_bot, telegram_menu, comms.gsm),
          meteo_history(hw.rtc, controllers.meteo()),
          task_binder(core.tm, comms.wifi, comms.telegram, hw.ext, controllers, meteo_history),
          ftest(core.logs, hw.io, hw.ow, hw.ibutton, hw.ds18b20, hw.i2c, hw.rtc, hw.ext, core.tm, task_binder),
          plc_scan(hw.io, hw.plc)
    {
    }
};

struct UiContext
{
    CliConsole console;

    UiContext(CoreContext &core, HardwareContext &hw, CommsContext &comms, ControlContext &control)
        : console(hw.plc, comms.wifi, hw.rtc, control.ftest, hw.i2c, hw.ow, comms.telegram,
                  control.telegram_menu, core.configs, hw.ext, control.controllers, nullptr)
    {
    }
};

struct NetworkContext
{
    AsyncWebServer web;
    WebInterface fw_upgrade;
    Network network;
    StackSlaveHandler stack_slave;

    NetworkContext(CoreContext &core, HardwareContext &hw, CommsContext &comms, ControlContext &control, UiContext &ui)
        : web(ActiveBoardProfile::WEB_PORT),
          fw_upgrade(web, ui.console, comms.wifi, core.configs, hw.plc, hw.rtc, comms.telegram,
                     control.telegram_menu, core.logs, hw.ext, hw.i2c, hw.ow, control.controllers),
          network(core.logs, comms.wifi, comms.gsm, comms.telegram, comms.telegram_bot, control.telegram_menu,
                  fw_upgrade, web, comms.telegram_wifi_client),
          stack_slave(hw.io, hw.ds18b20, hw.ow, hw.i2c, hw.plc, hw.rtc, comms.telegram, core.logs, hw.ext,
                      control.controllers.sockets(), control.controllers.meteo(), control.controllers.thermo(),
                      control.controllers.septic(), control.controllers.security())
    {
    }
};

struct ConfigContext
{
    ConfigsManager configs_manager;

    ConfigContext(CoreContext &core, HardwareContext &hw, CommsContext &comms,
                  ControlContext &control, UiContext &ui, NetworkContext &network)
        : configs_manager(core.configs, comms.wifi, comms.telegram, network.network, ui.console,
                          control.telegram_menu, hw.plc, control.controllers, comms.gsm)
    {
    }
};

struct App
{
    CoreContext core;
    HardwareContext hw;
    CommsContext comms;
    ControlContext control;
    UiContext ui;
    NetworkContext net;
    ConfigContext cfg;

    App()
        : core(),
          hw(core.logs, core.uart),
          comms(core.logs, core.uart),
          control(core, hw, comms),
          ui(core, hw, comms, control),
          net(core, hw, comms, control, ui),
          cfg(core, hw, comms, control, ui, net)
    {
        core.logs.setRtc(hw.rtc);
        ui.console.setStackMaster(net.network.stackMaster());
        ui.console.setConfigsManager(cfg.configs_manager);

        control.telegram_menu.setConfigsManager(cfg.configs_manager);
        control.telegram_menu.setStackMaster(*net.network.stackMaster());
        control.telegram_menu.setSockets(control.controllers.sockets());
        control.telegram_menu.setMeteo(control.controllers.meteo());
        control.telegram_menu.setThermo(control.controllers.thermo());
        control.telegram_menu.setTanks(control.controllers.tanks());
        control.telegram_menu.setSeptic(control.controllers.septic());
        control.telegram_menu.setSecurity(control.controllers.security());

        net.fw_upgrade.setConfigsManager(cfg.configs_manager);
        net.fw_upgrade.setStackMaster(*net.network.stackMaster());
        net.fw_upgrade.setGsmModem(comms.gsm);
        net.network.setStackConfig(cfg.configs_manager);
        net.stack_slave.setConfigsManager(cfg.configs_manager);
#if defined(ESP32)
        if (auto *node = net.network.stackNode())
            net.stack_slave.attach(*node);
#endif
    }

    bool begin()
    {
        if (!core.logs.beginAuto())
        {
            Serial.begin(UartManager::DEFAULT_SPEED);
            core.logs.begin(Serial);
            core.logs.error(F("APP"), F("LOG Auto bind failed, fallback to USB"));
        }

        delay(1000);
        ui.console.begin(Serial);

        core.logs.info(F("APP"), F("Starting application..."));
        ProfileValidator<ActiveBoardProfile>::printDiagnostics(Serial);

        if (!core.configs.begin())
        {
            core.logs.error(F("APP"), F("Configs mount failed"));
        }
        else
        {
            const bool loaded = cfg.configs_manager.loadConfigs();
            const Configs::Error cfg_err = core.configs.lastError();
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
                core.logs.info(F("APP"), F("Configs loaded: %s (%u bytes)"), Configs::kPath, (unsigned)cfg_size);
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
                    core.logs.warn(F("APP"), F("Configs missing: %s (saving defaults)"), Configs::kPath);
                    if (!cfg.configs_manager.save())
                    {
                        const char *save_err = "Unknown error";
                        switch (core.configs.lastError())
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
                        core.logs.error(F("APP"), F("Configs save failed: %s"), save_err);
                    }
                }
                else
                {
                    core.logs.error(F("APP"), F("Configs load failed: %s (%s, %u bytes)"),
                                   err, Configs::kPath, (unsigned)cfg_size);
                }
            }
        }
        net.network.setStackDeviceName(hw.plc.deviceName());
        if (comms.wifi.ap())
            core.logs.info(F("WIFI"), F("Mode: AP (SSID=%s)"), comms.wifi.apSsid().c_str());
        else
            core.logs.info(F("WIFI"), F("Mode: STA (SSID=%s)"), comms.wifi.ssid().c_str());

        bool ok = true;

        core.logs.info(F("APP"), F("Initializing HAL"));
        if (!hw.hal.begin())
        {
            core.logs.error(F("APP"), F("HAL init failed: %s"), Hal::errorName(hw.hal.lastError()));
            ok = false;
        }

        core.logs.info(F("APP"), F("Initializing EEPROM"));
        {
            const auto cfg = ActiveBoardProfile::EEPROM;
            TwoWire *wire = hw.i2c.wirePtr(cfg.bus_num);
            const bool eeprom_ok = wire && hw.eeprom.begin(*wire, cfg.addr);
            hw.eeprom_storage.setReady(eeprom_ok);
            if (!eeprom_ok)
                core.logs.warn(F("APP"), F("EEPROM init failed"));
            else
            {
                const uint32_t used = hw.eeprom.usedBytes();
                const uint32_t total = At24lc512::capacityBytes();
                const uint32_t free = At24lc512::remainingBytes(used);
                core.logs.info(F("APP"), F("EEPROM used: %lu free: %lu total: %lu"),
                              (unsigned long)used, (unsigned long)free, (unsigned long)total);
            }
        }

        core.logs.info(F("APP"), F("Initializing RTC"));
        if (!hw.rtc.begin())
        {
            switch (hw.rtc.lastError())
            {
            case RTC::Error::NoBus:
                core.logs.warn(F("APP"), F("RTC I2C bus missing"));
                break;
            case RTC::Error::InvalidConfig:
                core.logs.warn(F("APP"), F("RTC config invalid"));
                break;
            case RTC::Error::I2c:
                core.logs.warn(F("APP"), F("RTC I2C error"));
                break;
            default:
                core.logs.warn(F("APP"), F("RTC Init failed"));
                break;
            }
        }

        core.logs.info(F("APP"), F("Initializing Display"));
        if (!hw.display.begin())
        {
            switch (hw.display.lastError())
            {
            case Display::Error::NoBus:
                core.logs.warn(F("APP"), F("LCD I2C bus missing"));
                break;
            case Display::Error::InvalidConfig:
                core.logs.warn(F("APP"), F("LCD config invalid"));
                break;
            case Display::Error::I2c:
                core.logs.warn(F("APP"), F("LCD I2C error"));
                break;
            default:
                core.logs.warn(F("APP"), F("LCD Init failed"));
                break;
            }
        }

        core.logs.info(F("APP"), F("Initializing PLC Control"));
        if (!hw.plc.begin())
        {
            switch (hw.plc.lastError())
            {
            case PlcControl::Error::NoBus:
                core.logs.error(F("APP"), F("PLC I2C bus missing"));
                break;
            case PlcControl::Error::InvalidConfig:
                core.logs.error(F("APP"), F("PLC temp config invalid"));
                break;
            case PlcControl::Error::I2c:
                core.logs.error(F("APP"), F("PLC temp sensor error"));
                break;
            default:
                core.logs.error(F("APP"), F("PLC Init failed"));
                break;
            }
            ok = false;
        }

        core.logs.info(F("APP"), F("Initializing Network"));
        if (!net.network.begin())
        {
            switch (net.network.lastError())
            {
            case Network::Error::Wifi:
                core.logs.error(F("APP"), F("WIFI Init failed"));
                break;
            case Network::Error::TelegramClientMissing:
                core.logs.error(F("APP"), F("Telegram client missing"));
                break;
            case Network::Error::TelegramProxyInvalid:
                core.logs.error(F("APP"), F("Telegram proxy invalid"));
                break;
            case Network::Error::WebInterfaceFs:
                core.logs.error(F("APP"), F("WebInterface FS mount failed"));
                break;
            default:
                core.logs.error(F("APP"), F("Network init failed"));
                break;
            }
            ok = false;
        }

        control.controllers.begin();

        if (ok)
            core.logs.info(F("APP"), F("Application init [OK]"));
        else
            core.logs.error(F("APP"), F("Application init [FAIL]"));

        control.task_binder.bindFtest(control.ftest);
        control.task_binder.bindAll();
        control.plc_scan.begin();

        return ok;
    }

    void loop()
    {
        control.plc_scan.tick();
        ui.console.loop();
        comms.gsm.loop();
        const uint32_t budget_us = control.plc_scan.timeToNextUs();
        core.tm.loop(budget_us);
        if (budget_us > 200)
            net.network.loop();
    }
};
