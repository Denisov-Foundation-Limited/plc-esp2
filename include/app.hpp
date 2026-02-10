#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
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
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_protocol.hpp"
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
#include "utils/build_info.hpp"
#include "utils/users_registry.hpp"

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
          plc(i2c, io, rtc)
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
    UsersRegistry users;
    TelegramMenu telegram_menu;
    Controllers controllers;
    MeteoHistory meteo_history;

    TaskBinder<TASK_MGR_TSK_COUNT> task_binder;
    Ftest ftest;
    PlcScanLoop plc_scan;

    ControlContext(CoreContext &core, HardwareContext &hw, CommsContext &comms)
        : users(),
          telegram_menu(hw.plc, comms.wifi, hw.rtc, comms.telegram_bot, core.configs, core.logs, users),
          controllers(hw.gpio, hw.ow, hw.eeprom_storage, core.logs, comms.telegram_bot, telegram_menu, comms.gsm,
                      hw.rtc),
          meteo_history(hw.rtc, controllers.meteo()),
          task_binder(core.tm, comms.wifi, comms.telegram_bot, hw.ext, controllers, meteo_history,
                      hw.display, hw.plc),
          ftest(core.logs, hw.io, hw.ow, hw.ibutton, hw.ds18b20, hw.i2c, hw.rtc, hw.ext, core.tm, task_binder),
          plc_scan(hw.io)
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
                     comms.telegram_bot, control.telegram_menu, core.logs, hw.ext, hw.i2c, hw.ow,
                     control.controllers),
          network(core.logs, comms.wifi, comms.gsm, comms.telegram, comms.telegram_bot, control.telegram_menu,
                  fw_upgrade, web, comms.telegram_wifi_client, control.controllers, hw.plc, hw.rtc),
          stack_slave(hw.io, hw.ds18b20, hw.ow, hw.i2c, hw.plc, hw.rtc, comms.telegram, core.logs, hw.ext,
                      control.controllers.sockets(), control.controllers.meteo(), control.controllers.thermo(),
                      control.controllers.septic(), control.controllers.security(), control.controllers.tanks(),
                      control.controllers.watering(), control.controllers.ring(),
                      control.controllers.avr(), control.controllers.leak())
    {
    }
};

struct ConfigContext
{
    ConfigsManager configs_manager;

    ConfigContext(CoreContext &core, HardwareContext &hw, CommsContext &comms,
                  ControlContext &control, UiContext &ui, NetworkContext &network)
        : configs_manager(core.configs, comms.wifi, comms.telegram, network.network, ui.console,
                          control.telegram_menu, hw.plc, control.controllers, comms.gsm, control.users)
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
    StackCache stack_cache;
    NetworkContext net;
    ConfigContext cfg;

    App()
        : core(),
          hw(core.logs, core.uart),
          comms(core.logs, core.uart),
          control(core, hw, comms),
          ui(core, hw, comms, control),
          stack_cache(),
          net(core, hw, comms, control, ui),
          cfg(core, hw, comms, control, ui, net)
    {
        core.logs.setRtc(hw.rtc);
        ui.console.setStackMaster(&net.network.stackMaster());
        ui.console.setStackSlave(&net.stack_slave);
        ui.console.setConfigsManager(cfg.configs_manager);

        control.telegram_menu.setConfigsManager(cfg.configs_manager);
        control.telegram_menu.setStackMaster(net.network.stackMaster());
        control.telegram_menu.setSockets(control.controllers.sockets());
        control.telegram_menu.setMeteo(control.controllers.meteo());
        control.telegram_menu.setThermo(control.controllers.thermo());
        control.telegram_menu.setTanks(control.controllers.tanks());
        control.telegram_menu.setSeptic(control.controllers.septic());
        control.telegram_menu.setSecurity(control.controllers.security());

        stack_cache.setLogger(&core.logs);
        net.fw_upgrade.setStackCache(stack_cache);
        net.fw_upgrade.setConfigsManager(cfg.configs_manager);
        net.fw_upgrade.setStackMaster(net.network.stackMaster());
        net.fw_upgrade.setStackSlave(&net.stack_slave);
        net.fw_upgrade.setGsmModem(comms.gsm);
        net.fw_upgrade.setCloudClient(net.network.cloudClient());
        net.fw_upgrade.setUsersRegistry(control.users);
        net.network.setStackConfig(cfg.configs_manager);
        control.controllers.thermo().setRemoteMeteoProvider(&App::onRemoteMeteo_, this);
        control.controllers.meteo().setRemoteMeteoProvider(&App::onRemoteMeteoProxy_, this);
        control.controllers.meteo().setRemoteNodeNameProvider(&App::onRemoteNodeName_, this);
        control.controllers.meteo().setRemoteSensorNameProvider(&App::onRemoteSensorName_, this);
        control.controllers.meteo().setAlarmHandler(&App::onMeteoAlarm_, this);
        control.controllers.security().setArmStateHandler(&App::onSecurityArmState_, this);
        control.controllers.security().setPreArmCheckHandler(&App::onSecurityPreArmCheck_, this);
        control.controllers.security().setAlarmStateHandler(&App::onSecurityAlarmState_, this);
        control.controllers.security().setClearDetectHandler(&App::onSecurityClearDetect_, this);
        control.controllers.security().setDetectHandler(&App::onSecurityDetect_, this);
        control.controllers.security().setRfidUidHandler(&App::onSecurityRfidUid_, this);
        control.controllers.security().setIButtonSerialHandler(&App::onSecurityIButtonSerial_, this);
        control.controllers.security().setRfidI2c(&hw.i2c);
        control.controllers.security().setUsersRegistry(control.users);
        control.controllers.septic().setDetectHandler(&App::onSepticDetect_, this);
        control.controllers.tanks().setDetectHandler(&App::onTankEmpty_, this);
        control.controllers.ring().setHoldHandler(&App::onRingHold_, this);
        control.controllers.watering().setEventHandler(&App::onWateringEvent_, this);
        hw.display.setSlotProvider(&App::onDisplaySlot_, this);
        net.network.stackMaster().setEventHandler(&App::onStackNodeEvent_, this);
        net.network.stackMaster().setFrameHandlerTertiary(&App::onStackFrame_, this);
        net.stack_slave.setConfigsManager(cfg.configs_manager);
#if defined(ESP32)
        net.stack_slave.attach(net.network.stackNode());
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

        net.stack_slave.initAllocations();
        stack_cache.initAllocations();
        stack_cache.logAllocations();

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
        updateSecurityNotifyMode_();
        updateSepticNotifyMode_();
        updateTanksNotifyMode_();
        updateDisplayLayout_();
        net.network.setStackDeviceName(hw.plc.deviceName());
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
        updateDisplayLayout_();
        updateTankAlarms_();
        updateSepticAlarms_();
        updateSecurityAlarms_();
        updateMeteoAlarms_();
        pollSecurityPrearmWarmup_();
        pollStackCaches_();
        pollSecurityStatusFromMaster_();
        control.plc_scan.tick();
        ui.console.loop();
        comms.gsm.loop();
        net.network.loop();
        updateStackMasterMode_();
        net.stack_slave.loop();
        core.tm.loop();
        flushPendingSecurityDetect_();
        flushPendingSepticDetect_();
        flushPendingTankEmpty_();
        flushPendingWateringEvent_();
        flushPendingRfid_();
        flushPendingIButton_();
        flushPendingRingButton_();
    }

private:
    bool stackMasterActive_() const
    {
        return cfg.configs_manager.stackRole() == ConfigsManagerIface::StackRole::Master ||
               net.network.stackFallbackActive();
    }

    bool stackSlaveActive_() const
    {
        return cfg.configs_manager.stackRole() == ConfigsManagerIface::StackRole::Slave &&
               !net.network.stackFallbackActive();
    }

    void updateMasterLed_(bool master_active)
    {
        const uint8_t pin = ActiveBoardProfile::MASTER_LED_PIN;
        if (pin == 0xFF)
            return;
        if (!_master_led_initialized)
        {
            hw.io.pinMode(pin, PortIO::PortMode::Output);
            _master_led_initialized = true;
        }
        if (_master_led_state == master_active)
            return;
        hw.io.write(pin, master_active);
        _master_led_state = master_active;
    }

    void updateStackMasterMode_()
    {
        const bool active = net.network.stackMasterActive();
        if (_stack_master_effective != active)
        {
            _stack_master_effective = active;
            stack_cache.setMasterOverride(active);
            updateSecurityNotifyMode_();
            updateSepticNotifyMode_();
            updateTanksNotifyMode_();
            if (active)
                core.logs.warn(F("STACK"), F("Role switch: master"));
            else
                core.logs.warn(F("STACK"), F("Role switch: slave"));
        }
        updateMasterLed_(active);
    }

    void updateTankAlarms_()
    {
        TankController &tanks = control.controllers.tanks();
        uint32_t detail_mask = 0;
        uint32_t unit_mask = 0;
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = tanks.configByIndex(i);
            const auto *st = tanks.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            if (cfg->id == 0 || cfg->id > 32)
                continue;
            const bool empty = !(st->level_low || st->level_mid || st->level_full);
            if (!st->levels_ok || empty)
                detail_mask |= (1u << (cfg->id - 1));
        }
        if (stackMasterActive_())
        {
            StackMaster &master = net.network.stackMaster();
            const size_t count = master.nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = master.nodeIdAt(i);
                if (node_id == 0)
                    continue;
                const auto *cache = stack_cache.tanksCache(node_id);
                if (!cache || !cache->has_data || !cache->last_ok)
                    continue;
                for (size_t j = 0; j < cache->item_count; ++j)
                {
                    const auto &it = cache->items[j];
                    if (!it.enabled)
                        continue;
                    if (it.id == 0 || it.id > 32)
                        continue;
                    const bool empty = !(it.level_low || it.level_mid || it.level_full);
                    if (!it.levels_ok || empty)
                    {
                        detail_mask |= (1u << (it.id - 1));
                        if (i < 32)
                            unit_mask |= (1u << i);
                    }
                }
            }
        }
        hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Tanks, detail_mask);
        hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Tanks, unit_mask);
    }

    void updateSepticAlarms_()
    {
        SepticController &septic = control.controllers.septic();
        uint32_t detail_mask = 0;
        uint32_t unit_mask = 0;
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = septic.configByIndex(i);
            const auto *st = septic.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            if (cfg->id == 0 || cfg->id > 32)
                continue;
            if (st->alarm)
                detail_mask |= (1u << (cfg->id - 1));
        }
        if (stackMasterActive_())
        {
            StackMaster &master = net.network.stackMaster();
            const size_t count = master.nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = master.nodeIdAt(i);
                if (node_id == 0)
                    continue;
                const auto *cache = stack_cache.septicCache(node_id);
                if (!cache || !cache->has_data || !cache->items || !cache->last_ok)
                    continue;
                for (size_t j = 0; j < cache->item_count; ++j)
                {
                    const auto &it = cache->items[j];
                    if (!it.enabled)
                        continue;
                    if (it.id == 0 || it.id > 32)
                        continue;
                    if (it.alarm)
                    {
                        detail_mask |= (1u << (it.id - 1));
                        if (i < 32)
                            unit_mask |= (1u << i);
                    }
                }
            }
        }
        hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Septic, detail_mask);
        hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Septic, unit_mask);
    }

    void updateSecurityAlarms_()
    {
        SecurityController &sec = control.controllers.security();
        uint32_t detail_mask = 0;
        uint32_t unit_mask = 0;
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = sec.configByIndex(i);
            const auto *st = sec.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled || cfg->silent)
                continue;
            if (cfg->id == 0 || cfg->id > 32)
                continue;
            if (st->is_detect)
                detail_mask |= (1u << (cfg->id - 1));
        }
        if (stackMasterActive_())
        {
            StackMaster &master = net.network.stackMaster();
            const size_t count = master.nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = master.nodeIdAt(i);
                if (node_id == 0)
                    continue;
                const auto *cache = stack_cache.securityCache(node_id);
                if (!cache || !cache->has_data || !cache->items || !cache->last_ok)
                    continue;
                for (size_t j = 0; j < cache->item_count; ++j)
                {
                    const auto &it = cache->items[j];
                    if (!it.enabled || it.silent)
                        continue;
                    if (it.id == 0 || it.id > 32)
                        continue;
                    if (it.detect)
                    {
                        detail_mask |= (1u << (it.id - 1));
                        if (i < 32)
                            unit_mask |= (1u << i);
                    }
                }
            }
        }
        hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Security, detail_mask);
        hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Security, unit_mask);
    }

    void updateMeteoAlarms_()
    {
        MeteoController &meteo = control.controllers.meteo();
        uint32_t detail_mask = 0;
        uint32_t unit_mask = 0;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            const auto *st = meteo.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            if (cfg->id == 0 || cfg->id > 32)
                continue;
            if (!st->ok)
                detail_mask |= (1u << (cfg->id - 1));
        }
        if (stackMasterActive_())
        {
            StackMaster &master = net.network.stackMaster();
            const size_t count = master.nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = master.nodeIdAt(i);
                if (node_id == 0)
                    continue;
                const auto *cache = stack_cache.meteoCache(node_id);
                if (!cache || !cache->has_data || !cache->items || !cache->last_ok)
                    continue;
                for (size_t j = 0; j < cache->item_count; ++j)
                {
                    const auto &it = cache->items[j];
                    if (!it.enabled)
                        continue;
                    if (it.id == 0 || it.id > 32)
                        continue;
                    if (!it.ok)
                    {
                        detail_mask |= (1u << (it.id - 1));
                        if (i < 32)
                            unit_mask |= (1u << i);
                    }
                }
            }
        }
        hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Meteo, detail_mask);
        hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Meteo, unit_mask);
    }

    void pollStackCaches_()
    {
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        if (count == 0)
            return;
        const uint32_t now = millis();
        if ((uint32_t)(now - _last_stack_poll_ms) < kStackPollMs)
            return;
        _last_stack_poll_ms = now;
        if (_stack_poll_index >= count)
            _stack_poll_index = 0;
        const uint32_t node_id = master.nodeIdAt(_stack_poll_index++);
        if (node_id == 0)
            return;
        if (!master.nodeIsOnline(node_id, kStackNodeStaleMs))
            return;
        stack_cache.requestSecurity(node_id);
        stack_cache.requestSecurityPrearm(node_id);
        stack_cache.requestSeptic(node_id);
        stack_cache.requestTanks(node_id);
        stack_cache.requestMeteo(node_id);
        stack_cache.requestAvr(node_id);
        stack_cache.requestLeak(node_id);
    }

    static bool onRemoteMeteo_(void *ctx, uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp)
    {
        if (!ctx || node_id == 0 || sensor_id == 0)
            return false;
        App *self = static_cast<App *>(ctx);
        if (self->stackMasterActive_())
        {
            auto &stack_cache = self->stack_cache;
            const auto *cache = stack_cache.meteoCache(node_id);
            if (!cache || !cache->has_data)
            {
                stack_cache.requestMeteo(node_id);
                return false;
            }
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != sensor_id)
                    continue;
                temp_c = it.temp_c;
                has_temp = it.has_temp;
                return true;
            }
            return false;
        }
        if (self->net.stack_slave.remoteMeteoTemp(node_id, sensor_id, temp_c, has_temp))
            return true;
        self->net.stack_slave.requestRemoteMeteoAll();
        return false;
    }

    static bool onRemoteMeteoProxy_(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                    float &temp_c, bool &has_temp, float &hum, bool &has_hum, bool &ok)
    {
        if (!ctx || node_id == 0 || sensor_id == 0)
            return false;
        App *self = static_cast<App *>(ctx);
        if (self->stackMasterActive_())
        {
            const auto *cache = self->stack_cache.meteoCache(node_id);
            if (!cache || !cache->has_data)
            {
                self->stack_cache.requestMeteo(node_id);
                return false;
            }
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != sensor_id)
                    continue;
                temp_c = it.temp_c;
                hum = it.hum;
                has_temp = it.has_temp;
                has_hum = it.has_hum;
                ok = it.ok;
                return true;
            }
            return false;
        }
        if (self->net.stack_slave.remoteMeteoRead(node_id, sensor_id, temp_c, has_temp, hum, has_hum, ok))
            return true;
        self->net.stack_slave.requestRemoteMeteoAll();
        return false;
    }

    static bool onRemoteNodeName_(void *ctx, uint32_t node_id, String &out)
    {
        if (!ctx || node_id == 0)
            return false;
        App *self = static_cast<App *>(ctx);
        out = self->stackNodeLabel_(node_id);
        return out.length() > 0;
    }

    static bool onRemoteSensorName_(void *ctx, uint32_t node_id, uint8_t sensor_id, String &out)
    {
        if (!ctx || node_id == 0 || sensor_id == 0)
            return false;
        App *self = static_cast<App *>(ctx);
        if (self->stackMasterActive_())
        {
            const auto *cache = self->stack_cache.meteoCache(node_id);
            if (!cache || !cache->has_data)
            {
                self->stack_cache.requestMeteo(node_id);
                return false;
            }
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != sensor_id)
                    continue;
                if (it.name[0])
                {
                    out = it.name;
                    return true;
                }
                return false;
            }
            return false;
        }
        const auto *cache = self->net.stack_slave.remoteMeteoCache(node_id);
        if (!cache || !cache->has_data || !cache->items)
        {
            self->net.stack_slave.requestRemoteMeteoAll();
            return false;
        }
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (it.id != sensor_id)
                continue;
            if (it.name[0])
            {
                out = it.name;
                return true;
            }
            return false;
        }
        return false;
    }

    static void onMeteoAlarm_(void *ctx, uint32_t node_id, uint8_t sensor_id, bool alarm)
    {
        if (!ctx || sensor_id == 0 || sensor_id > 32)
            return;
        App *self = static_cast<App *>(ctx);
        if (node_id != 0 && self->stackSlaveActive_())
            return;
        self->hw.plc.setAlarmDetail(PlcControl::AlarmModule::Meteo, (uint8_t)(sensor_id - 1), alarm);
        if (node_id == 0)
            return;
        const int unit_idx = self->stackNodeIndex_(node_id);
        if (unit_idx >= 0 && unit_idx < 32)
            self->hw.plc.setAlarmUnit(PlcControl::AlarmModule::Meteo, (uint8_t)unit_idx, alarm);
    }

    static void onSecurityArmState_(void *ctx, bool armed)
    {
        if (!ctx)
            return;
        App *self = static_cast<App *>(ctx);
        self->broadcastSecurityState_(armed);
        if (!armed)
            self->broadcastSecurityAlarm_(false);
    }

    static bool onSecurityPreArmCheck_(void *ctx, String &out, String *plain_out)
    {
        if (!ctx)
            return false;
        return static_cast<App *>(ctx)->collectRemoteSecurityDetections_(out, plain_out);
    }

    static void onStackNodeEvent_(void *ctx, uint32_t node_id, bool online)
    {
        if (!ctx || node_id == 0)
            return;
        App *self = static_cast<App *>(ctx);
        String name;
        String ip;
        uint16_t fw_ver = 0;
        self->net.network.stackMaster().nodeInfo(node_id, name, ip, fw_ver);
        const String label = name.length() ? name : self->stackNodeLabel_(node_id);
        const char *ip_c = ip.length() ? ip.c_str() : "n/a";
        if (online)
        {
            self->core.logs.info(F("STACK"), F("node online: %s ip: %s fw: %u"),
                                 label.c_str(), ip_c, (unsigned)fw_ver);
            self->sendSecurityStateToNode_(node_id, self->control.controllers.security().armed(), true);
            self->stack_cache.requestSecurityPrearm(node_id);
        }
        else
        {
            self->core.logs.warn(F("STACK"), F("node offline: %s ip: %s fw: %u"),
                                 label.c_str(), ip_c, (unsigned)fw_ver);
        }
    }

    static void onSecurityAlarmState_(void *ctx, bool alarm_on)
    {
        if (!ctx)
            return;
        static_cast<App *>(ctx)->broadcastSecurityAlarm_(alarm_on);
    }

    static void onSecurityClearDetect_(void *ctx)
    {
        if (!ctx)
            return;
        App *self = static_cast<App *>(ctx);
        self->hw.plc.setAlarmDetailMask(PlcControl::AlarmModule::Security, 0);
        self->hw.plc.setAlarmUnitMask(PlcControl::AlarmModule::Security, 0);
        self->broadcastSecurityClear_();
    }

    static void onSecurityDetect_(void *ctx, uint8_t sensor_id, const String &name, bool silent)
    {
        if (!ctx)
            return;
        App *self = static_cast<App *>(ctx);
        if (!silent && sensor_id > 0 && sensor_id <= 32)
            self->hw.plc.setAlarmDetail(PlcControl::AlarmModule::Security, (uint8_t)(sensor_id - 1), true);
        self->sendSecurityDetectToMaster_(sensor_id, name, silent);
    }

    static void onSepticDetect_(void *ctx, uint8_t septic_id, const String &name, bool is_alarm)
    {
        if (!ctx)
            return;
        App *self = static_cast<App *>(ctx);
        if (is_alarm && septic_id > 0 && septic_id <= 32)
            self->hw.plc.setAlarmDetail(PlcControl::AlarmModule::Septic, (uint8_t)(septic_id - 1), true);
        self->sendSepticDetectToMaster_(septic_id, name, is_alarm);
    }

    static void onTankEmpty_(void *ctx, uint8_t tank_id, const String &name, bool empty)
    {
        if (!ctx)
            return;
        if (!empty)
            return;
        static_cast<App *>(ctx)->sendTankEmptyToMaster_(tank_id, name);
    }

    static void onWateringEvent_(void *ctx, WateringController::Event ev,
                                 const WateringController::RuleConfig &cfg,
                                 const WateringController::RuleState &st)
    {
        if (!ctx)
            return;
        static_cast<App *>(ctx)->sendWateringEventToMaster_(ev, cfg, st);
    }

    static void onRingHold_(void *ctx, bool on)
    {
        if (!ctx)
            return;
        App *self = static_cast<App *>(ctx);
        const RingController::Source src = self->control.controllers.ring().lastSource();
        if (src == RingController::Source::Button && self->stackSlaveActive_())
        {
            if (!self->sendRingButtonToMaster_(on))
            {
                self->_pending_ring_button = true;
                self->_pending_ring_button_pressed = on;
            }
        }
        self->broadcastRingHold_(on);
        if (on)
            self->notifyRingHold_();
    }

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        if (!ctx || node_id == 0)
            return;
        static_cast<App *>(ctx)->handleStackFrame_(node_id, frame);
    }

    static bool onSecurityRfidUid_(void *ctx, const String &uid)
    {
        if (!ctx)
            return false;
        return static_cast<App *>(ctx)->handleSecurityRfidUid_(uid);
    }

    static bool onSecurityIButtonSerial_(void *ctx, const String &serial)
    {
        if (!ctx)
            return false;
        return static_cast<App *>(ctx)->handleSecurityIButtonSerial_(serial);
    }

    static bool onDisplaySlot_(void *ctx, const DisplaySlotConfig &slot, char out[5])
    {
        if (!ctx)
            return false;
        return static_cast<App *>(ctx)->renderDisplaySlot_(slot, out);
    }

    void broadcastSecurityState_(bool armed)
    {
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
            sendSecurityStateToNode_(master.nodeIdAt(i), armed, false);
    }

    void sendSecurityStateToNode_(uint32_t node_id)
    {
        sendSecurityStateToNode_(node_id, control.controllers.security().armed(), false);
    }

    void sendSecurityStateToNode_(uint32_t node_id, bool armed, bool force)
    {
        if (node_id == 0)
            return;
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();

        StaticJsonDocument<128> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["armed"] = armed;
        params["alarm"] = armed ? control.controllers.security().alarmOn() : false;
        if (force && armed)
            params["force"] = true;

        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                      reinterpret_cast<const uint8_t *>(payload), len);
    }

    void sendSecurityDetectToMaster_(uint8_t sensor_id, const String &name, bool silent)
    {
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
        {
            _pending_detect = true;
            _pending_sensor_id = sensor_id;
            _pending_sensor_name = name;
            _pending_sensor_silent = silent;
            return;
        }
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "alarm";
        JsonObject params = doc["params"].to<JsonObject>();
        params["alarm"] = true;
        params["sensor_id"] = sensor_id;
        if (name.length())
            params["name"] = name;
        if (silent)
            params["silent"] = true;

        char payload[160] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        if (!node.send((uint8_t)StackMsgType::CmdSet,
                       reinterpret_cast<const uint8_t *>(payload), len))
        {
            _pending_detect = true;
            _pending_sensor_id = sensor_id;
            _pending_sensor_name = name;
            _pending_sensor_silent = silent;
        }
    }

    void sendSepticDetectToMaster_(uint8_t septic_id, const String &name, bool is_alarm)
    {
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
        {
            _pending_septic_detect = true;
            _pending_septic_id = septic_id;
            _pending_septic_name = name;
            _pending_septic_alarm = is_alarm;
            return;
        }
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Septic;
        doc["action"] = "level";
        JsonObject params = doc["params"].to<JsonObject>();
        params["id"] = septic_id;
        params["alarm"] = is_alarm;
        params["level"] = is_alarm ? "alarm" : "warning";
        if (name.length())
            params["name"] = name;

        char payload[160] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        if (!node.send((uint8_t)StackMsgType::CmdSet,
                       reinterpret_cast<const uint8_t *>(payload), len))
        {
            _pending_septic_detect = true;
            _pending_septic_id = septic_id;
            _pending_septic_name = name;
            _pending_septic_alarm = is_alarm;
        }
    }

    void sendTankEmptyToMaster_(uint8_t tank_id, const String &name)
    {
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
        {
            _pending_tank_empty = true;
            _pending_tank_id = tank_id;
            _pending_tank_name = name;
            return;
        }
        StaticJsonDocument<160> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Tanks;
        doc["action"] = "empty";
        JsonObject params = doc["params"].to<JsonObject>();
        params["id"] = tank_id;
        params["empty"] = true;
        if (name.length())
            params["name"] = name;

        char payload[140] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        if (!node.send((uint8_t)StackMsgType::CmdSet,
                       reinterpret_cast<const uint8_t *>(payload), len))
        {
            _pending_tank_empty = true;
            _pending_tank_id = tank_id;
            _pending_tank_name = name;
        }
    }

    void sendWateringEventToMaster_(WateringController::Event ev,
                                    const WateringController::RuleConfig &cfg,
                                    const WateringController::RuleState &st)
    {
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
        {
            _pending_watering_event = true;
            _pending_watering_event_type = ev;
            _pending_watering_event_cfg = cfg;
            _pending_watering_event_state = st;
            return;
        }
        StaticJsonDocument<256> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Watering;
        doc["action"] = "event";
        JsonObject params = doc["params"].to<JsonObject>();
        params["id"] = cfg.id;
        if (cfg.name.length())
            params["name"] = cfg.name;
        if (cfg.port != WateringController::kInvalidPort)
            params["port"] = cfg.port;
        if (cfg.tank_id)
            params["tank"] = cfg.tank_id;
        if (cfg.resume_after_refill)
            params["resume"] = true;
        params["resume_level"] = cfg.resume_level;
        params["remaining_ms"] = st.remaining_ms;
        const char *event_str = "stop";
        switch (ev)
        {
        case WateringController::Event::Start:
            event_str = "start";
            break;
        case WateringController::Event::PauseEmpty:
            event_str = "pause";
            params["reason"] = "empty";
            break;
        case WateringController::Event::Resume:
            event_str = "resume";
            break;
        case WateringController::Event::StopDone:
            event_str = "stop";
            params["reason"] = "done";
            break;
        case WateringController::Event::StopEmpty:
            event_str = "stop";
            params["reason"] = "empty";
            break;
        case WateringController::Event::Stop:
        default:
            event_str = "stop";
            break;
        }
        params["event"] = event_str;

        char payload[224] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        if (!node.send((uint8_t)StackMsgType::CmdSet,
                       reinterpret_cast<const uint8_t *>(payload), len))
        {
            _pending_watering_event = true;
            _pending_watering_event_type = ev;
            _pending_watering_event_cfg = cfg;
            _pending_watering_event_state = st;
        }
    }

    void broadcastRingHold_(bool on)
    {
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        if (count == 0)
            return;

        StaticJsonDocument<128> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Ring;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["state"] = on;
        const String key = cfg.configs_manager.stackApiKey();
        if (key.length())
            doc["api_key"] = key;

        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        for (size_t i = 0; i < count; ++i)
            master.sendTo(master.nodeIdAt(i), (uint8_t)StackMsgType::CmdSet,
                          reinterpret_cast<const uint8_t *>(payload), len);
    }

    void notifyRingHold_()
    {
        const RingController::Source src = control.controllers.ring().lastSource();
        if (src == RingController::Source::Button)
        {
            const String tg_msg = F("Звонок включен по кнопке");
            core.logs.info(F("RING"), F("Ring enabled by button"));
            sendTelegramNotify_(tg_msg);
            return;
        }
        if (src == RingController::Source::Web)
        {
            const String tg_msg = F("Звонок включен из веб-интерфейса");
            core.logs.info(F("RING"), F("Ring enabled from web"));
            sendTelegramNotify_(tg_msg);
            return;
        }
        if (src == RingController::Source::Cli)
        {
            const String tg_msg = F("Звонок включен из CLI");
            core.logs.info(F("RING"), F("Ring enabled from CLI"));
            sendTelegramNotify_(tg_msg);
            return;
        }
        if (src == RingController::Source::Stack)
        {
            const String tg_msg = F("Звонок включен из стека");
            core.logs.info(F("RING"), F("Ring enabled from stack"));
            sendTelegramNotify_(tg_msg);
            return;
        }
    }

    void sendTelegramNotify_(const String &msg)
    {
        const auto users = control.telegram_menu.allowedUsers();
        if (users.empty())
            return;
        for (size_t i = 0; i < users.size; ++i)
        {
            const auto &user = users[i];
            if (!user.enabled || !user.is_notify || user.chat_id == 0)
                continue;
            comms.telegram_bot.sendText(user.chat_id, msg);
        }
    }

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame)
    {
        if (!stackMasterActive_())
            return;
        if (frame.type != (uint8_t)StackMsgType::CmdSet)
            return;
        DynamicJsonDocument doc(512);
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint8_t feature = (uint8_t)(doc["feature"] | 0);
        String action = doc["action"] | "";
        action.toLowerCase();
        if (feature == (uint8_t)StackFeature::Security)
        {
            if (action == "alarm")
            {
                JsonObjectConst params = doc["params"];
                const bool alarm = params["alarm"].is<bool>() ? params["alarm"].as<bool>()
                                                              : (params["alarm"].as<int>() != 0);
                if (!alarm)
                    return;
                const uint8_t sensor_id = (uint8_t)(params["sensor_id"] | 0);
                const String name = params["name"] | "";
                const bool silent = params["silent"] | false;
                const String source = stackNodeLabel_(node_id);
                core.logs.warn(F("SECURITY"), F("remote detect: node: %s id: %u name: %s silent: %u"),
                               source.c_str(),
                               (unsigned)sensor_id,
                               name.length() ? name.c_str() : "",
                               silent ? 1u : 0u);

                if (!silent && sensor_id > 0 && sensor_id <= 32)
                    hw.plc.setAlarmDetail(PlcControl::AlarmModule::Security, (uint8_t)(sensor_id - 1), true);
                const int unit_idx = stackNodeIndex_(node_id);
                if (unit_idx >= 0 && unit_idx < 32 && !silent)
                    hw.plc.setAlarmUnit(PlcControl::AlarmModule::Security, (uint8_t)unit_idx, true);

                control.controllers.security().setAlarmState(true);
                broadcastSecurityAlarm_(true);
                control.controllers.security().notifyRemoteDetect(source, sensor_id, name, silent);
                return;
            }
            if (action == "rfid")
            {
                JsonObjectConst params = doc["params"];
                const String uid = params["uid"] | "";
                if (!uid.length())
                    return;
                const String source = params["name"] | stackNodeLabel_(node_id);
                core.logs.info(F("SECURITY"), F("remote RFID: node: %s uid: %s"),
                               source.c_str(), uid.c_str());
                SecurityController &sec = control.controllers.security();
                const bool matched = sec.processRfidUidString(uid.c_str(), source.c_str());
                const String result = matched ? "disarm" : "reject";
                sendRfidResultToNode_(node_id, uid, matched, result, sec.armed());
                return;
            }
            if (action == "ibutton")
            {
                JsonObjectConst params = doc["params"];
                const String serial = params["serial"] | "";
                if (!serial.length())
                    return;
                const String source = params["name"] | stackNodeLabel_(node_id);
                core.logs.info(F("SECURITY"), F("remote iButton: node: %s serial: %s"),
                               source.c_str(), serial.c_str());
                SecurityController &sec = control.controllers.security();
                const bool matched = sec.processIButtonSerialString(serial.c_str(), source.c_str());
                const String result = matched ? "disarm" : "reject";
                sendIButtonResultToNode_(node_id, serial, matched, result, sec.armed());
                return;
            }
            if (action == "status_req")
            {
                sendSecurityStateToNode_(node_id);
                return;
            }
            return;
        }
        if (feature == (uint8_t)StackFeature::Ring)
        {
            if (action != "button")
                return;
            JsonObjectConst params = doc["params"];
            const bool pressed = params["pressed"].is<bool>() ? params["pressed"].as<bool>()
                                                              : (params["pressed"].as<int>() != 0);
            control.controllers.ring().setHoldRelayWithSource(pressed, RingController::Source::Stack);
            return;
        }
        if (feature == (uint8_t)StackFeature::Septic)
        {
            handleSepticFrame_(node_id, action, doc["params"]);
            return;
        }
        if (feature == (uint8_t)StackFeature::Tanks)
        {
            handleTankFrame_(node_id, action, doc["params"]);
            return;
        }
        if (feature == (uint8_t)StackFeature::Watering)
        {
            handleWateringFrame_(node_id, action, doc["params"]);
            return;
        }
    }

    void handleSepticFrame_(uint32_t node_id, const String &action, JsonVariantConst params)
    {
        if (action != "level")
            return;
        const String level = params["level"] | "";
        bool is_alarm = false;
        if (level.length())
        {
            String low = level;
            low.toLowerCase();
            is_alarm = (low == "alarm" || low == "overflow");
        }
        else
        {
            is_alarm = params["alarm"].is<bool>() ? params["alarm"].as<bool>()
                                                  : (params["alarm"].as<int>() != 0);
        }
        const uint8_t septic_id = (uint8_t)(params["id"] | 0);
        const String name = params["name"] | "";
        const String source = stackNodeLabel_(node_id);
        core.logs.warn(F("SEPTIC"), F("remote %s: node: %s id: %u name: %s"),
                       is_alarm ? "alarm" : "warning",
                       source.c_str(),
                       (unsigned)septic_id,
                       name.length() ? name.c_str() : "");
        if (septic_id > 0 && septic_id <= 32)
            hw.plc.setAlarmDetail(PlcControl::AlarmModule::Septic, (uint8_t)(septic_id - 1), is_alarm);
        const int unit_idx = stackNodeIndex_(node_id);
        if (unit_idx >= 0 && unit_idx < 32)
            hw.plc.setAlarmUnit(PlcControl::AlarmModule::Septic, (uint8_t)unit_idx, is_alarm);
        control.controllers.septic().notifyRemoteLevel(source, septic_id, name, is_alarm);
    }

    void handleTankFrame_(uint32_t node_id, const String &action, JsonVariantConst params)
    {
        if (action != "empty")
            return;
        const bool empty = params["empty"].is<bool>() ? params["empty"].as<bool>()
                                                      : (params["empty"].as<int>() != 0);
        if (!empty)
            return;
        const uint8_t tank_id = (uint8_t)(params["id"] | 0);
        const String name = params["name"] | "";
        const String source = stackNodeLabel_(node_id);
        core.logs.warn(F("TANK"), F("remote empty: node: %s id: %u name: %s"),
                       source.c_str(),
                       (unsigned)tank_id,
                       name.length() ? name.c_str() : "");
        if (tank_id > 0 && tank_id <= 32)
            hw.plc.setAlarmDetail(PlcControl::AlarmModule::Tanks, (uint8_t)(tank_id - 1), true);
        const int unit_idx = stackNodeIndex_(node_id);
        if (unit_idx >= 0 && unit_idx < 32)
            hw.plc.setAlarmUnit(PlcControl::AlarmModule::Tanks, (uint8_t)unit_idx, true);
        control.controllers.tanks().notifyRemoteEmpty(source, tank_id, name);
    }

    void handleWateringFrame_(uint32_t node_id, const String &action, JsonVariantConst params)
    {
        if (action != "event")
            return;
        const String event = params["event"] | "";
        if (!event.length())
            return;
        const uint8_t rule_id = (uint8_t)(params["id"] | 0);
        const uint8_t port = (uint8_t)(params["port"] | WateringController::kInvalidPort);
        const uint8_t tank_id = (uint8_t)(params["tank"] | 0);
        const String name = params["name"] | "";
        const String reason = params["reason"] | "";
        const uint32_t remaining_ms = (uint32_t)(params["remaining_ms"] | 0u);
        const uint8_t resume_level = (uint8_t)(params["resume_level"] | 0u);
        const String source = stackNodeLabel_(node_id);

        String msg;
        msg.reserve(64);
        msg += "remote ";
        msg += event;
        if (reason.length())
        {
            msg += " (";
            msg += reason;
            msg += ")";
        }
        core.logs.info(F("WATER"), F("%s: node: %s rule: %u name: %s port: %u tank: %u rem_ms: %lu resume_lvl: %u"),
                       msg.c_str(),
                       source.c_str(),
                       (unsigned)rule_id,
                       name.length() ? name.c_str() : "",
                       (unsigned)port,
                       (unsigned)tank_id,
                       (unsigned long)remaining_ms,
                       (unsigned)resume_level);
    }

    void updateSecurityNotifyMode_()
    {
        control.controllers.security().setNotifyEnabled(stackMasterActive_());
    }

    void updateSepticNotifyMode_()
    {
        control.controllers.septic().setNotifyEnabled(stackMasterActive_());
    }

    void updateTanksNotifyMode_()
    {
        control.controllers.tanks().setNotifyEnabled(stackMasterActive_());
    }

    void updateDisplayLayout_()
    {
        const size_t count = cfg.configs_manager.displaySlotCount();
        for (size_t i = 0; i < Display::kSlotCount && i < count; ++i)
        {
            DisplaySlotConfig slot{};
            if (!cfg.configs_manager.displaySlot(i, slot))
                continue;
            if (!displaySlotEqual_(_display_slots[i], slot))
            {
                _display_slots[i] = slot;
                hw.display.setSlot(i, slot);
            }
        }
    }

    void flushPendingSecurityDetect_()
    {
        if (!_pending_detect)
            return;
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_detect = false;
        sendSecurityDetectToMaster_(_pending_sensor_id, _pending_sensor_name, _pending_sensor_silent);
    }

    void flushPendingSepticDetect_()
    {
        if (!_pending_septic_detect)
            return;
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_septic_detect = false;
        sendSepticDetectToMaster_(_pending_septic_id, _pending_septic_name, _pending_septic_alarm);
    }

    void flushPendingTankEmpty_()
    {
        if (!_pending_tank_empty)
            return;
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_tank_empty = false;
        sendTankEmptyToMaster_(_pending_tank_id, _pending_tank_name);
    }

    void flushPendingWateringEvent_()
    {
        if (!_pending_watering_event)
            return;
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_watering_event = false;
        sendWateringEventToMaster_(_pending_watering_event_type, _pending_watering_event_cfg,
                                   _pending_watering_event_state);
    }

    void flushPendingRfid_()
    {
        if (!_pending_rfid)
            return;
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_rfid = false;
        sendRfidToMaster_(_pending_rfid_uid, hw.plc.deviceName());
    }

    void flushPendingIButton_()
    {
        if (!_pending_ibutton)
            return;
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_ibutton = false;
        sendIButtonToMaster_(_pending_ibutton_serial, hw.plc.deviceName());
    }

    void flushPendingRingButton_()
    {
        if (!_pending_ring_button)
            return;
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_ring_button = false;
        sendRingButtonToMaster_(_pending_ring_button_pressed);
    }

    bool handleSecurityRfidUid_(const String &uid_str)
    {
        if (!stackSlaveActive_())
            return false;
        if (uid_str.length() == 0)
            return false;
        const String name = hw.plc.deviceName();
        StackNode &node = net.network.stackNode();
        if (!node.connected())
        {
            _pending_rfid = true;
            _pending_rfid_uid = uid_str;
            return true;
        }
        if (!sendRfidToMaster_(uid_str, name))
        {
            _pending_rfid = true;
            _pending_rfid_uid = uid_str;
            return true;
        }
        return true;
    }

    bool handleSecurityIButtonSerial_(const String &serial)
    {
        if (!stackSlaveActive_())
            return false;
        if (serial.length() == 0)
            return false;
        const String name = hw.plc.deviceName();
        StackNode &node = net.network.stackNode();
        if (!node.connected())
        {
            _pending_ibutton = true;
            _pending_ibutton_serial = serial;
            return true;
        }
        if (!sendIButtonToMaster_(serial, name))
        {
            _pending_ibutton = true;
            _pending_ibutton_serial = serial;
            return true;
        }
        return true;
    }

    bool sendRingButtonToMaster_(bool pressed)
    {
        if (!stackSlaveActive_())
            return false;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return false;
        StaticJsonDocument<128> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Ring;
        doc["action"] = "button";
        JsonObject params = doc["params"].to<JsonObject>();
        params["pressed"] = pressed;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        return node.send((uint8_t)StackMsgType::CmdSet,
                         reinterpret_cast<const uint8_t *>(payload), len);
    }

    bool renderDisplaySlot_(const DisplaySlotConfig &slot, char out[5])
    {
        if (!out)
            return false;
        const uint32_t node_id = slot.node_id;
        const bool local = (node_id == 0);
        const bool is_master = stackMasterActive_();
        const bool is_slave = stackSlaveActive_();
        for (size_t i = 0; i < 4; ++i)
            out[i] = ' ';
        out[4] = '\0';
        switch (slot.kind)
        {
        case DisplaySlotKind::Time:
        {
            Ds3231Mz::DateTime dt{};
            if (!hw.rtc.Time(dt))
                return false;
            if (slot.field == DisplaySlotField::TimeMin)
                snprintf(out, 5, "%02u ", (unsigned)dt.minute);
            else
                snprintf(out, 5, "%02u:", (unsigned)dt.hour);
            return true;
        }
        case DisplaySlotKind::Security:
        {
            if (local)
            {
                const bool armed = control.controllers.security().armed();
                const char *txt = armed ? "ARM " : "DIS ";
                memcpy(out, txt, 4);
                return true;
            }
            if (is_master)
            {
                const auto *cache = stack_cache.securityCache(node_id);
                if (!cache || !cache->has_data)
                {
                    stack_cache.requestSecurity(node_id);
                    return false;
                }
                if (!cache->last_ok && cache->last_error.length())
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                const char *txt = cache->armed ? "ARM " : "DIS ";
                memcpy(out, txt, 4);
                return true;
            }
            if (!is_slave)
                return false;
            const auto *rcache = net.stack_slave.remoteSecurityCache(node_id);
            if (!rcache || !rcache->has_data)
            {
                net.stack_slave.requestRemoteSecurity(node_id);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
            if (age_ms > 3000u)
                net.stack_slave.requestRemoteSecurity(node_id);
            if (age_ms > 8000u)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            if (!rcache->last_ok && rcache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            const char *txt = rcache->armed ? "ARM " : "DIS ";
            memcpy(out, txt, 4);
            return true;
        }
        case DisplaySlotKind::Socket:
        {
            if (slot.index == 0)
                return false;
            if (local)
            {
                const SocketController::SocketState *st = control.controllers.sockets().state(slot.index);
                if (!st)
                    return false;
                const char *txt = st->relay_on ? "ON  " : "OFF ";
                memcpy(out, txt, 4);
                return true;
            }
            if (is_master)
            {
                const auto *cache = stack_cache.socketsCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    stack_cache.requestSockets(node_id);
                    return false;
                }
                if (!cache->last_ok && cache->last_error.length())
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != slot.index || !it.enabled)
                        continue;
                    const char *txt = it.state ? "ON  " : "OFF ";
                    memcpy(out, txt, 4);
                    return true;
                }
                return false;
            }
            if (!is_slave)
                return false;
            const auto *rcache = net.stack_slave.remoteSocketsCache(node_id);
            if (!rcache || !rcache->has_data || !rcache->items)
            {
                net.stack_slave.requestRemoteSockets(node_id);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
            if (age_ms > 3000u)
                net.stack_slave.requestRemoteSockets(node_id);
            if (age_ms > 8000u)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            if (!rcache->last_ok && rcache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < rcache->item_count; ++i)
            {
                const auto &it = rcache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                const char *txt = it.state ? "ON  " : "OFF ";
                memcpy(out, txt, 4);
                return true;
            }
            return false;
        }
        case DisplaySlotKind::Light:
        {
            if (slot.index == 0)
                return false;
            if (local)
            {
                const SocketController::LightState *st = control.controllers.sockets().lightState(slot.index);
                if (!st)
                    return false;
                const char *txt = st->relay_on ? "ON  " : "OFF ";
                memcpy(out, txt, 4);
                return true;
            }
            if (is_master)
            {
                const auto *cache = stack_cache.lightsCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    stack_cache.requestLights(node_id);
                    return false;
                }
                if (!cache->last_ok && cache->last_error.length())
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != slot.index || !it.enabled)
                        continue;
                    const char *txt = it.state ? "ON  " : "OFF ";
                    memcpy(out, txt, 4);
                    return true;
                }
                return false;
            }
            if (!is_slave)
                return false;
            const auto *rcache = net.stack_slave.remoteLightsCache(node_id);
            if (!rcache || !rcache->has_data || !rcache->items)
            {
                net.stack_slave.requestRemoteLights(node_id);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
            if (age_ms > 3000u)
                net.stack_slave.requestRemoteLights(node_id);
            if (age_ms > 8000u)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            if (!rcache->last_ok && rcache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < rcache->item_count; ++i)
            {
                const auto &it = rcache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                const char *txt = it.state ? "ON  " : "OFF ";
                memcpy(out, txt, 4);
                return true;
            }
            return false;
        }
        case DisplaySlotKind::Meteo:
        {
            if (slot.index == 0)
                return false;
            if (local)
            {
                const MeteoController::SensorState *st = control.controllers.meteo().state(slot.index);
                if (!st || !st->ok)
                    return false;
                if (slot.field == DisplaySlotField::MeteoHum)
                {
                    if (!st->has_humidity)
                        return false;
                    const int h = (int)roundf(st->humidity);
                    snprintf(out, 5, "%2d%%", h);
                }
                else
                {
                if (!st->has_temp)
                    return false;
                const int t = (int)roundf(st->temp_c);
                formatTemp3_(out, t);
            }
        }
            else if (is_master)
            {
                const auto *cache = stack_cache.meteoCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    stack_cache.requestMeteo(node_id);
                    return false;
                }
                if (!cache->last_ok && cache->last_error.length())
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                const StackCache::StackMeteoItem *found = nullptr;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    if (cache->items[i].id == slot.index && cache->items[i].enabled)
                    {
                        found = &cache->items[i];
                        break;
                    }
                }
                if (!found || !found->ok)
                    return false;
                if (slot.field == DisplaySlotField::MeteoHum)
                {
                    if (!found->has_hum)
                        return false;
                    const int h = (int)roundf(found->hum);
                    snprintf(out, 5, "%2d%%", h);
                }
                else
                {
                if (!found->has_temp)
                    return false;
                const int t = (int)roundf(found->temp_c);
                formatTemp3_(out, t);
            }
        }
            else
            {
                const auto *cache = net.stack_slave.remoteMeteoCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    net.stack_slave.requestRemoteMeteoAll();
                    return false;
                }
                const uint32_t age_ms = (uint32_t)(millis() - cache->updated_ms);
                if (age_ms > 3000u)
                    net.stack_slave.requestRemoteMeteoAll();
                if (age_ms > kStackNodeStaleMs)
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                if (!cache->last_ok && cache->last_error.length())
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                const StackSlaveHandler::RemoteMeteoItem *found = nullptr;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    if (cache->items[i].id == slot.index)
                    {
                        found = &cache->items[i];
                        break;
                    }
                }
                if (!found || !found->ok)
                    return false;
                if (slot.field == DisplaySlotField::MeteoHum)
                {
                    if (!found->has_hum)
                        return false;
                    const int h = (int)roundf(found->hum);
                    snprintf(out, 5, "%2d%%", h);
                }
                else
                {
                if (!found->has_temp)
                    return false;
                const int t = (int)roundf(found->temp_c);
                formatTemp3_(out, t);
            }
        }
        if (strlen(out) < 4)
        {
            size_t len = strlen(out);
                while (len < 4)
                    out[len++] = ' ';
                out[4] = '\0';
            }
            return true;
        }
        case DisplaySlotKind::Thermo:
        {
            if (slot.index == 0)
                return false;
            if (local)
            {
                const ThermoController::DeviceState *st = control.controllers.thermo().state(slot.index);
                if (!st)
                    return false;
                if (!st->power_on)
                    memcpy(out, "IDL ", 4);
                else if (st->heat_on)
                    memcpy(out, "HET ", 4);
                else if (st->cool_on)
                    memcpy(out, "COL ", 4);
                else
                    memcpy(out, "IDL ", 4);
                return true;
            }
            if (is_master)
            {
                const auto *cache = stack_cache.thermoCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    stack_cache.requestThermo(node_id);
                    return false;
                }
                if (!cache->last_ok && cache->last_error.length())
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != slot.index || !it.enabled)
                        continue;
                    if (!it.power_on)
                        memcpy(out, "IDL ", 4);
                    else if (it.heat_on)
                        memcpy(out, "HET ", 4);
                    else if (it.cool_on)
                        memcpy(out, "COL ", 4);
                    else
                        memcpy(out, "IDL ", 4);
                    return true;
                }
                return false;
            }
            if (!is_slave)
                return false;
            const auto *rcache = net.stack_slave.remoteThermoCache(node_id);
            if (!rcache || !rcache->has_data || !rcache->items)
            {
                net.stack_slave.requestRemoteThermo(node_id);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
            if (age_ms > 3000u)
                net.stack_slave.requestRemoteThermo(node_id);
            if (age_ms > kStackNodeStaleMs)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            if (!rcache->last_ok && rcache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < rcache->item_count; ++i)
            {
                const auto &it = rcache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                if (!it.power_on)
                    memcpy(out, "IDL ", 4);
                else if (it.heat_on)
                    memcpy(out, "HET ", 4);
                else if (it.cool_on)
                    memcpy(out, "COL ", 4);
                else
                    memcpy(out, "IDL ", 4);
                return true;
            }
            return false;
        }
        case DisplaySlotKind::Tank:
        {
            if (slot.index == 0)
                return false;
            if (local)
            {
                const TankController::TankState *st = control.controllers.tanks().state(slot.index);
                if (!st || !st->levels_ok)
                    return false;
                if (st->level_full)
                    memcpy(out, "99% ", 4);
                else if (st->level_mid)
                    memcpy(out, "66% ", 4);
                else if (st->level_low)
                    memcpy(out, "33% ", 4);
                else
                    memcpy(out, "0%  ", 4);
                return true;
            }
            if (is_master)
            {
                const auto *cache = stack_cache.tanksCache(node_id);
                if (!cache || !cache->items)
                {
                    stack_cache.requestTanks(node_id);
                    return false;
                }
                const bool node_online = net.network.stackMaster().nodeIsOnline(node_id, kStackNodeStaleMs);
                const uint32_t now = millis();
                const uint32_t age_ms = (uint32_t)(now - cache->updated_ms);
                if (age_ms > 3000u)
                    stack_cache.requestTanks(node_id);
                if (!cache->has_data)
                {
                    bool no_data_long = false;
                    if (cache->pending_since_ms)
                        no_data_long = (uint32_t)(now - cache->pending_since_ms) > kDisplayNoDataErrMs;
                    else if (cache->updated_ms)
                        no_data_long = (uint32_t)(now - cache->updated_ms) > kDisplayNoDataErrMs;
                    else
                        no_data_long = now > kDisplayNoDataErrMs;
                    if (no_data_long && !node_online)
                    {
                        memcpy(out, "ERR ", 4);
                        return true;
                    }
                    return false;
                }
                if (age_ms > kDisplayNoDataErrMs && !node_online)
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != slot.index || !it.enabled)
                        continue;
                    if (!it.levels_ok)
                        return false;
                    if (it.level_full)
                        memcpy(out, "99% ", 4);
                    else if (it.level_mid)
                        memcpy(out, "66% ", 4);
                    else if (it.level_low)
                        memcpy(out, "33% ", 4);
                    else
                        memcpy(out, "0%  ", 4);
                    return true;
                }
                return false;
            }
            if (!is_slave)
                return false;
            const auto *rcache = net.stack_slave.remoteTanksCache(node_id);
            if (!rcache || !rcache->items)
            {
                net.stack_slave.requestRemoteTanks(node_id);
                return false;
            }
            const bool master_connected = net.network.stackNode().connected();
            const uint32_t now = millis();
            const uint32_t age_ms = (uint32_t)(now - rcache->updated_ms);
            if (age_ms > 3000u)
                net.stack_slave.requestRemoteTanks(node_id);
            if (!rcache->has_data)
            {
                bool no_data_long = false;
                if (rcache->pending_since_ms)
                    no_data_long = (uint32_t)(now - rcache->pending_since_ms) > kDisplayNoDataErrMs;
                else if (rcache->updated_ms)
                    no_data_long = (uint32_t)(now - rcache->updated_ms) > kDisplayNoDataErrMs;
                else
                    no_data_long = now > kDisplayNoDataErrMs;
                if (no_data_long && !master_connected)
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                return false;
            }
            if (age_ms > kDisplayNoDataErrMs && !master_connected)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < rcache->item_count; ++i)
            {
                const auto &it = rcache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                if (!it.levels_ok)
                    return false;
                if (it.level_full)
                    memcpy(out, "99% ", 4);
                else if (it.level_mid)
                    memcpy(out, "66% ", 4);
                else if (it.level_low)
                    memcpy(out, "33% ", 4);
                else
                    memcpy(out, "0%  ", 4);
                return true;
            }
            return false;
        }
        case DisplaySlotKind::Septic:
        {
            if (slot.index == 0)
                return false;
            if (local)
            {
                const size_t idx = (size_t)(slot.index - 1);
                const SepticController::SepticState *st = control.controllers.septic().stateByIndex(idx);
                if (!st)
                    return false;
                if (st->alarm)
                    memcpy(out, "ALM ", 4);
                else if (st->warning)
                    memcpy(out, "WRN ", 4);
                else
                    memcpy(out, "OK  ", 4);
                return true;
            }
            if (is_master)
            {
                const auto *cache = stack_cache.septicCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    stack_cache.requestSeptic(node_id);
                    return false;
                }
                if (!cache->last_ok && cache->last_error.length())
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != slot.index || !it.enabled)
                        continue;
                    if (it.alarm)
                        memcpy(out, "ALM ", 4);
                    else if (it.warning)
                        memcpy(out, "WRN ", 4);
                    else
                        memcpy(out, "OK  ", 4);
                    return true;
                }
                return false;
            }
            if (!is_slave)
                return false;
            const auto *rcache = net.stack_slave.remoteSepticCache(node_id);
            if (!rcache || !rcache->has_data || !rcache->items)
            {
                net.stack_slave.requestRemoteSeptic(node_id);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
            if (age_ms > 3000u)
                net.stack_slave.requestRemoteSeptic(node_id);
            if (age_ms > 8000u)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            if (!rcache->last_ok && rcache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < rcache->item_count; ++i)
            {
                    const auto &it = rcache->items[i];
                    if (it.id != slot.index || !it.enabled)
                        continue;
                    if (it.alarm)
                        memcpy(out, "ALM ", 4);
                    else if (it.warning)
                        memcpy(out, "WRN ", 4);
                    else
                        memcpy(out, "OK  ", 4);
                    return true;
            }
            return false;
        }
        case DisplaySlotKind::Avr:
        {
            if (local)
            {
                const auto &st = control.controllers.avr().state();
                if (slot.field == DisplaySlotField::AvrMainOk)
                    memcpy(out, st.main_ok ? "ON  " : "OFF ", 4);
                else if (slot.field == DisplaySlotField::AvrReserveOk)
                    memcpy(out, st.reserve_ok ? "ON  " : "OFF ", 4);
                else if (st.active_source == AvrController::Source::Main)
                    memcpy(out, "MAN ", 4);
                else if (st.active_source == AvrController::Source::Reserve)
                    memcpy(out, "RES ", 4);
                else
                    memcpy(out, "OFF ", 4);
                return true;
            }
            if (is_master)
            {
                const auto *cache = stack_cache.avrCache(node_id);
                if (!cache || !cache->has_data)
                {
                    stack_cache.requestAvr(node_id);
                    return false;
                }
                const uint32_t age_ms = (uint32_t)(millis() - cache->updated_ms);
                if (age_ms > 3000u)
                    stack_cache.requestAvr(node_id);
                if (age_ms > 8000u || (!cache->last_ok && cache->last_error.length()))
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                if (slot.field == DisplaySlotField::AvrMainOk)
                    memcpy(out, cache->main_ok ? "ON  " : "OFF ", 4);
                else if (slot.field == DisplaySlotField::AvrReserveOk)
                    memcpy(out, cache->reserve_ok ? "ON  " : "OFF ", 4);
                else if (strcmp(cache->active_source, "main") == 0)
                    memcpy(out, "MAN ", 4);
                else if (strcmp(cache->active_source, "reserve") == 0)
                    memcpy(out, "RES ", 4);
                else
                    memcpy(out, "OFF ", 4);
                return true;
            }
            return false;
        }
        case DisplaySlotKind::Leak:
        {
            if (slot.index == 0)
                return false;
            if (local)
            {
                const auto *st = control.controllers.leak().state(slot.index);
                if (!st)
                    return false;
                if (st->wet || st->alarm_latched)
                    memcpy(out, "ALRM", 4);
                else
                    memcpy(out, "DRY ", 4);
                return true;
            }
            if (is_master)
            {
                const auto *cache = stack_cache.leakCache(node_id);
                if (!cache || !cache->has_data || !cache->items)
                {
                    stack_cache.requestLeak(node_id);
                    return false;
                }
                const uint32_t age_ms = (uint32_t)(millis() - cache->updated_ms);
                if (age_ms > 3000u)
                    stack_cache.requestLeak(node_id);
                if (age_ms > 8000u || (!cache->last_ok && cache->last_error.length()))
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != slot.index || !it.enabled)
                        continue;
                    if (it.wet || it.alarm_latched)
                        memcpy(out, "ALRM", 4);
                    else
                        memcpy(out, "DRY ", 4);
                    return true;
                }
                return false;
            }
            return false;
        }
        case DisplaySlotKind::Text:
        {
            if (!slot.text[0])
                return false;
            for (size_t i = 0; i < 4; ++i)
                out[i] = slot.text[i] ? slot.text[i] : ' ';
            out[4] = '\0';
            return true;
        }
        case DisplaySlotKind::None:
        default:
            return false;
        }
    }

    static void formatTemp3_(char out[5], int t)
    {
        if (!out)
            return;
        if (t <= -10)
            snprintf(out, 5, "%3d", t);
        else
            snprintf(out, 5, "%2d%c", t, Display::kDegreeChar);
    }

    static bool displaySlotEqual_(const DisplaySlotConfig &a, const DisplaySlotConfig &b)
    {
        if (a.kind != b.kind || a.node_id != b.node_id || a.index != b.index || a.field != b.field)
            return false;
        return strncmp(a.text, b.text, sizeof(a.text)) == 0;
    }

    bool sendRfidToMaster_(const String &uid, const String &name)
    {
        if (!stackSlaveActive_())
            return false;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return false;
        StaticJsonDocument<128> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "rfid";
        JsonObject params = doc["params"].to<JsonObject>();
        params["uid"] = uid;
        if (name.length())
            params["name"] = name;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        return node.send((uint8_t)StackMsgType::CmdSet,
                         reinterpret_cast<const uint8_t *>(payload), len);
    }

    bool sendIButtonToMaster_(const String &serial, const String &name)
    {
        if (!stackSlaveActive_())
            return false;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return false;
        StaticJsonDocument<128> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "ibutton";
        JsonObject params = doc["params"].to<JsonObject>();
        params["serial"] = serial;
        if (name.length())
            params["name"] = name;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        return node.send((uint8_t)StackMsgType::CmdSet,
                         reinterpret_cast<const uint8_t *>(payload), len);
    }

    void sendRfidResultToNode_(uint32_t node_id, const String &uid, bool matched,
                               const String &result, bool armed)
    {
        if (node_id == 0)
            return;
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();
        StaticJsonDocument<160> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "rfid_result";
        JsonObject params = doc["params"].to<JsonObject>();
        params["uid"] = uid;
        params["match"] = matched;
        if (result.length())
            params["result"] = result;
        params["armed"] = armed;
        char payload[160] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                      reinterpret_cast<const uint8_t *>(payload), len);
    }

    void sendIButtonResultToNode_(uint32_t node_id, const String &serial, bool matched,
                                  const String &result, bool armed)
    {
        if (node_id == 0)
            return;
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();
        StaticJsonDocument<176> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "ibutton_result";
        JsonObject params = doc["params"].to<JsonObject>();
        params["serial"] = serial;
        params["match"] = matched;
        if (result.length())
            params["result"] = result;
        params["armed"] = armed;
        char payload[176] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                      reinterpret_cast<const uint8_t *>(payload), len);
    }

    void pollSecurityStatusFromMaster_()
    {
        if (!stackSlaveActive_())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        const uint32_t now = millis();
        if ((uint32_t)(now - _last_rfid_status_ms) < 5000u)
            return;
        _last_rfid_status_ms = now;
        StaticJsonDocument<128> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "status_req";
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        node.send((uint8_t)StackMsgType::CmdSet,
                  reinterpret_cast<const uint8_t *>(payload), len);
    }

    bool collectRemoteSecurityDetections_(String &out, String *plain_out)
    {
        if (!stackMasterActive_())
            return false;
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        bool any = false;
        bool missing = false;
        uint32_t beep_nodes[StackMaster::MAX_SESSIONS] = {};
        size_t beep_count = 0;
        auto mark_beep = [&beep_nodes, &beep_count](uint32_t node_id) {
            for (size_t i = 0; i < beep_count; ++i)
            {
                if (beep_nodes[i] == node_id)
                    return;
            }
            if (beep_count < StackMaster::MAX_SESSIONS)
                beep_nodes[beep_count++] = node_id;
        };
        uint32_t pending_nodes[StackMaster::MAX_SESSIONS] = {};
        uint32_t pending_req_ms[StackMaster::MAX_SESSIONS] = {};
        size_t pending_count = 0;
        const uint32_t now = millis();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t node_id = master.nodeIdAt(i);
            if (node_id == 0)
                continue;
            if (!master.nodeIsOnline(node_id, kStackNodeStaleMs))
                continue;
            const auto *cache = stack_cache.securityPrearmCache(node_id);
            const bool stale = cache && cache->has_data && (uint32_t)(now - cache->updated_ms) > kPreArmFreshMs;
            if (!cache || cache->pending || !cache->has_data || !cache->items || !cache->last_ok || stale)
            {
                stack_cache.requestSecurityPrearmForce(node_id);
                if (pending_count < StackMaster::MAX_SESSIONS)
                {
                    pending_nodes[pending_count] = node_id;
                    pending_req_ms[pending_count] = millis();
                    ++pending_count;
                }
            }
        }
        if (pending_count)
        {
            const uint32_t wait_until = millis() + kPreArmWaitMs;
            bool any_pending = true;
            while (any_pending && (int32_t)(millis() - wait_until) < 0)
            {
                any_pending = false;
                for (size_t i = 0; i < pending_count; ++i)
                {
                    const uint32_t node_id = pending_nodes[i];
                    if (node_id == 0)
                        continue;
                    const auto *cache = stack_cache.securityPrearmCache(node_id);
                    if (!cache)
                        continue;
                    if (cache->pending || !cache->has_data || !cache->items || !cache->last_ok ||
                        cache->updated_ms < pending_req_ms[i])
                    {
                        any_pending = true;
                    }
                }
                if (any_pending)
                    delay(20);
            }
        }
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t node_id = master.nodeIdAt(i);
            if (node_id == 0)
                continue;
            if (!master.nodeIsOnline(node_id, kStackNodeStaleMs))
                continue;
            const auto *cache = stack_cache.securityPrearmCache(node_id);
            const uint32_t now2 = millis();
            uint32_t req_ms = 0;
            for (size_t j = 0; j < pending_count; ++j)
            {
                if (pending_nodes[j] == node_id)
                {
                    req_ms = pending_req_ms[j];
                    break;
                }
            }
            const bool stale = cache && cache->has_data && (uint32_t)(now2 - cache->updated_ms) > kPreArmFreshMs;
            if (!cache || cache->pending || !cache->has_data || !cache->items || !cache->last_ok || stale ||
                (req_ms != 0 && cache->updated_ms < req_ms))
            {
                const String label = stackNodeLabel_(node_id);
                if (out.length())
                    out += F("\n");
                out += F("ожидание данных: ");
                out += escapeHtml_(label);
                if (plain_out)
                {
                    if (plain_out->length())
                        *plain_out += F(", ");
                    *plain_out += F("ожидание данных: ");
                    *plain_out += label;
                }
                core.logs.warn(F("SECURITY"), F("prearm waiting data from %s"), label.c_str());
                missing = true;
                any = true;
                mark_beep(node_id);
                continue;
            }
            String line;
            String plain_line;
            bool node_any = false;
            for (size_t j = 0; j < cache->item_count; ++j)
            {
                const auto &it = cache->items[j];
                if (!node_any)
                {
                    const String label = stackNodeLabel_(node_id);
                    line += escapeHtml_(label);
                    line += F(": ");
                    plain_line += label;
                    plain_line += F(": ");
                }
                else
                {
                    line += F(", ");
                    plain_line += F(", ");
                }
                line += String((unsigned)it.id);
                plain_line += String((unsigned)it.id);
                if (it.name[0])
                {
                    const String name = String(it.name);
                    line += F(" (");
                    line += F("<b>");
                    line += escapeHtml_(name);
                    line += F("</b>");
                    line += F(")");
                    plain_line += F(" (");
                    plain_line += name;
                    plain_line += F(")");
                }
                core.logs.warn(F("SECURITY"), F("prearm blocked %s sensor %u (%s)"),
                               stackNodeLabel_(node_id).c_str(),
                               (unsigned)it.id,
                               it.name[0] ? it.name : "-");
                node_any = true;
            }
            if (!node_any)
                continue;
            mark_beep(node_id);
            if (out.length())
                out += F("\n");
            out += line;
            if (plain_out)
            {
                if (plain_out->length())
                    *plain_out += F(", ");
                *plain_out += plain_line;
            }
            any = true;
        }
        if (beep_count)
        {
            for (size_t i = 0; i < beep_count; ++i)
                sendSecurityBeepToNode_(beep_nodes[i], "reject");
        }
        if (missing)
            return true;
        return any;
    }

    String stackNodeLabel_(uint32_t node_id) const
    {
        StackMaster &master = const_cast<App *>(this)->net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            if (master.nodeIdAt(i) == node_id)
            {
                String name = master.nodeNameAt(i);
                if (name.length() > 0)
                    return name;
            }
        }
        char buf[12] = {};
        snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)node_id);
        return String(buf);
    }

    static String escapeHtml_(const String &in)
    {
        String out;
        out.reserve(in.length() + 8);
        for (size_t i = 0; i < in.length(); ++i)
        {
            const char c = in.charAt(i);
            switch (c)
            {
            case '&':
                out += F("&amp;");
                break;
            case '<':
                out += F("&lt;");
                break;
            case '>':
                out += F("&gt;");
                break;
            case '"':
                out += F("&quot;");
                break;
            case '\'':
                out += F("&#39;");
                break;
            default:
                out += c;
                break;
            }
        }
        return out;
    }

    int stackNodeIndex_(uint32_t node_id) const
    {
        StackMaster &master = const_cast<App *>(this)->net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            if (master.nodeIdAt(i) == node_id)
                return (int)i;
        }
        return -1;
    }

    static constexpr uint32_t kPreArmFreshMs = 8000;
    static constexpr uint32_t kPreArmWaitMs = 900;
    static constexpr uint32_t kPreArmPollMs = 1000;
    static constexpr uint32_t kStackNodeStaleMs = 15000;
    static constexpr uint32_t kDisplayNoDataErrMs = 30000;

    void broadcastSecurityAlarm_(bool alarm_on)
    {
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
            sendSecurityAlarmToNode_(master.nodeIdAt(i), alarm_on);
    }

    void sendSecurityAlarmToNode_(uint32_t node_id, bool alarm_on)
    {
        if (node_id == 0)
            return;
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();

        StaticJsonDocument<128> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["alarm"] = alarm_on;

        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                      reinterpret_cast<const uint8_t *>(payload), len);
    }

    void sendSecurityBeepToNode_(uint32_t node_id, const char *kind)
    {
        if (node_id == 0)
            return;
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();

        StaticJsonDocument<96> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["beep"] = kind ? kind : "reject";

        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                      reinterpret_cast<const uint8_t *>(payload), len);
    }

    void pollSecurityPrearmWarmup_()
    {
        if (!stackMasterActive_())
            return;
        const uint32_t now = millis();
        if ((uint32_t)(now - _last_prearm_poll_ms) < kPreArmPollMs)
            return;
        _last_prearm_poll_ms = now;
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t node_id = master.nodeIdAt(i);
            if (node_id == 0)
                continue;
            if (!master.nodeIsOnline(node_id, kStackNodeStaleMs))
                continue;
            const auto *cache = stack_cache.securityPrearmCache(node_id);
            const bool stale = cache && cache->has_data && (uint32_t)(now - cache->updated_ms) > kPreArmFreshMs;
            if (!cache || !cache->has_data || !cache->items || !cache->last_ok || stale || cache->pending)
                stack_cache.requestSecurityPrearm(node_id);
        }
    }

    void broadcastSecurityClear_()
    {
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
            sendSecurityClearToNode_(master.nodeIdAt(i));
    }

    void sendSecurityClearToNode_(uint32_t node_id)
    {
        if (node_id == 0)
            return;
        if (!stackMasterActive_())
            return;
        StackMaster &master = net.network.stackMaster();

        StaticJsonDocument<96> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["clear"] = true;

        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                      reinterpret_cast<const uint8_t *>(payload), len);
    }

    static constexpr uint32_t kStackPollMs = 5000;

    bool _pending_detect = false;
    uint8_t _pending_sensor_id = 0;
    String _pending_sensor_name;
    bool _pending_sensor_silent = false;
    bool _pending_septic_detect = false;
    uint8_t _pending_septic_id = 0;
    String _pending_septic_name;
    bool _pending_septic_alarm = false;
    bool _pending_tank_empty = false;
    uint8_t _pending_tank_id = 0;
    String _pending_tank_name;
    bool _pending_watering_event = false;
    WateringController::Event _pending_watering_event_type = WateringController::Event::Stop;
    WateringController::RuleConfig _pending_watering_event_cfg{};
    WateringController::RuleState _pending_watering_event_state{};
    bool _pending_rfid = false;
    String _pending_rfid_uid;
    bool _pending_ibutton = false;
    String _pending_ibutton_serial;
    uint32_t _last_rfid_status_ms = 0;
    uint32_t _last_prearm_poll_ms = 0;
    bool _pending_ring_button = false;
    bool _pending_ring_button_pressed = false;
    bool _stack_master_effective = false;
    bool _master_led_initialized = false;
    bool _master_led_state = false;
    uint32_t _last_stack_poll_ms = 0;
    size_t _stack_poll_index = 0;
    DisplaySlotConfig _display_slots[Display::kSlotCount]{};
};
