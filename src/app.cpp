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

#include "app.hpp"

#include <LittleFS.h>

#include "boards/profile_validator.hpp"
#include "utils/build_info.hpp"

CoreContext::CoreContext()
        : logs(uart),
          tm(),
          configs()
{
}

HardwareContext::HardwareContext(Logger &logs, UartManager &uart)
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
          plc(i2c, io, rtc)
{
}

CommsContext::CommsContext(Logger &logs, UartManager &uart)
        : wifi(logs),
          sim800l(),
          gsm(uart, sim800l, logs),
          telegram_wifi_client(),
          telegram(logs),
          telegram_bot(telegram)
{
}

ControlContext::ControlContext(CoreContext &core, HardwareContext &hw, CommsContext &comms)
        : users(),
          telegram_menu(hw.plc, comms.wifi, hw.rtc, comms.telegram_bot, core.configs, core.logs, users),
          controllers(hw.gpio, hw.ow, hw.eeprom_storage, core.logs, comms.telegram_bot, telegram_menu, comms.gsm,
                      hw.rtc),
          rules(),
          meteo_history(hw.rtc, controllers.meteo()),
          task_binder(core.tm, comms.wifi, comms.telegram_bot, hw.ext, controllers, meteo_history,
                      hw.display, hw.plc, core.logs),
          ftest(core.logs, hw.io, hw.ow, hw.ibutton, hw.ds18b20, hw.i2c, hw.rtc, hw.ext, core.tm, task_binder),
          plc_scan(hw.io)
{
}

UiContext::UiContext(CoreContext &core, HardwareContext &hw, CommsContext &comms, ControlContext &control)
        : console(hw.plc, comms.wifi, hw.rtc, control.ftest, hw.i2c, hw.ow, comms.telegram,
                  control.telegram_menu, core.configs, hw.ext, control.users, control.controllers, nullptr)
{
}

NetworkContext::NetworkContext(CoreContext &core, HardwareContext &hw, CommsContext &comms, ControlContext &control, UiContext &ui)
        : web(ActiveBoardProfile::WEB_PORT),
          fw_upgrade(web, ui.console, comms.wifi, core.configs, hw.plc, hw.rtc, comms.telegram,
                     comms.telegram_bot, control.telegram_menu, core.logs, hw.ext, hw.i2c, hw.ow,
                     control.controllers, control.rules),
          network(core.logs, comms.wifi, comms.gsm, comms.telegram, comms.telegram_bot, control.telegram_menu,
                  fw_upgrade, web, comms.telegram_wifi_client, control.controllers, hw.plc, hw.rtc),
          stack_slave(hw.io, hw.ds18b20, hw.ow, hw.i2c, hw.plc, hw.rtc, comms.telegram, core.logs, hw.ext,
                      control.controllers.sockets(), control.controllers.meteo(), control.controllers.thermo(),
                      control.controllers.septic(), control.controllers.security(), control.controllers.tanks(),
                      control.controllers.watering(), control.controllers.ring(),
                      control.controllers.avr(), control.controllers.leak(), control.controllers)
{
}

ConfigContext::ConfigContext(CoreContext &core, HardwareContext &hw, CommsContext &comms,
                  ControlContext &control, UiContext &ui, NetworkContext &network)
        : configs_manager(core.configs, comms.wifi, comms.telegram, network.network, ui.console,
                          control.telegram_menu, hw.plc, control.controllers, control.rules, comms.gsm, control.users)
{
}

App::App()
        : core(),
          hw(core.logs, core.uart),
          comms(core.logs, core.uart),
          control(core, hw, comms),
          ui(core, hw, comms, control),
          net(core, hw, comms, control, ui),
          cfg(core, hw, comms, control, ui, net),
          stack(core, hw, comms, control, ui, net, cfg)
{
    core.logs.setRtc(hw.rtc);
    ui.console.setStackMaster(&net.network.stackMaster());
    ui.console.setStackSlave(&net.stack_slave);
    ui.console.setConfigsManager(cfg.configs_manager);

    control.telegram_menu.setConfigsManager(cfg.configs_manager);
    control.telegram_menu.setStackMaster(net.network.stackMaster());
    control.telegram_menu.setStackCache(stack.stackCache());
    control.telegram_menu.setSockets(control.controllers.sockets());
    control.telegram_menu.setMeteo(control.controllers.meteo());
    control.telegram_menu.setThermo(control.controllers.thermo());
    control.telegram_menu.setTanks(control.controllers.tanks());
    control.telegram_menu.setSeptic(control.controllers.septic());
    control.telegram_menu.setSecurity(control.controllers.security());
    control.telegram_menu.setAvr(control.controllers.avr());
    control.telegram_menu.setLeak(control.controllers.leak());
    control.telegram_menu.setRing(control.controllers.ring());
    control.telegram_menu.setWatering(control.controllers.watering());
    control.telegram_menu.setRules(control.rules);

    net.fw_upgrade.setStackCache(stack.stackCache());
    net.fw_upgrade.setConfigsManager(cfg.configs_manager);
    net.fw_upgrade.setStackMaster(net.network.stackMaster());
    net.fw_upgrade.setStackSlave(&net.stack_slave);
    net.fw_upgrade.setGsmModem(comms.gsm);
    net.fw_upgrade.setCloudClient(net.network.cloudClient());
    net.fw_upgrade.setUsersRegistry(control.users);
    net.fw_upgrade.setRules(control.rules);

    net.network.setStackConfig(cfg.configs_manager);

    control.controllers.security().setRfidI2c(&hw.i2c);
    control.controllers.security().setUsersRegistry(control.users);
    control.controllers.security().setPlcControl(hw.plc);

    stack.bindCallbacks();
}

bool App::begin()
{
    if (!core.logs.beginAuto())
    {
        Serial.begin(UartManager::DEFAULT_SPEED);
        core.logs.begin(Serial);
        core.logs.error(F("APP"), F("LOG Auto bind failed, fallback to USB"));
    }

    stack.init();

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

    stack.applyLoadedConfig();

    cfg.configs_manager.setCloudFirmwareVersion(BuildInfo::kFwVersion);
    net.network.setCloudFirmwareVersion(BuildInfo::kFwVersion);
    if (comms.wifi.ap())
        core.logs.info(F("WIFI"), F("Mode: AP (SSID: %s)"), comms.wifi.apSsid().c_str());
    else
        core.logs.info(F("WIFI"), F("Mode: STA (SSID: %s)"), comms.wifi.ssid().c_str());

    bool ok = true;

    core.logs.info(F("APP"), F("Initializing HAL"));
    if (!hw.hal.begin())
    {
        core.logs.error(F("APP"), F("HAL init failed: %s"), Hal::errorName(hw.hal.lastError()));
        ok = false;
    }

    core.logs.info(F("APP"), F("Initializing EEPROM"));
    {
        const auto cfg_eeprom = ActiveBoardProfile::EEPROM;
        TwoWire *wire = hw.i2c.wirePtr(cfg_eeprom.bus_num);
        const bool eeprom_present = wire && hw.i2c.probeAddress(cfg_eeprom.bus_num, cfg_eeprom.addr);
        const bool eeprom_ok = eeprom_present && hw.eeprom.begin(*wire, cfg_eeprom.addr);
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
    const uint8_t bl_pin = ActiveBoardProfile::LCD_BACKLIGHT_PIN;
    if (bl_pin != 0xFF)
    {
        hw.portio.pinMode(bl_pin, PortIO::PortMode::Output);
        hw.portio.write(bl_pin, true);
    }
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

    if (!control.controllers.begin())
    {
        core.logs.error(F("APP"), F("Controllers init failed"));
        ok = false;
    }

    if (ok)
        core.logs.info(F("APP"), F("Application init [OK]"));
    else
        core.logs.error(F("APP"), F("Application init [FAIL]"));

    control.task_binder.bindFtest(control.ftest);
    control.task_binder.bindAll();
    control.task_binder.bindStack(stack);
    control.plc_scan.begin();

    return ok;
}

void App::loop()
{
    stack.setTaskPhase(StackRuntime::TaskPhase::PreNetwork);
    stack.taskPre();
    control.plc_scan.tick();
    ui.console.loop();
    comms.gsm.loop();
    net.network.loop();
    stack.setTaskPhase(StackRuntime::TaskPhase::PostNetwork);
    core.tm.loop();
    stack.setTaskPhase(StackRuntime::TaskPhase::Idle);
}
