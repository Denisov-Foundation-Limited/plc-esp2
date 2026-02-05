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
#include "clients/rfid_reader.hpp"
#include "clients/ring_client.hpp"
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
    TelegramMenu telegram_menu;
    Controllers controllers;
    MeteoHistory meteo_history;
    RfidReader rfid_reader;
    RingClient ring_client;

    TaskBinder<TASK_MGR_TSK_COUNT> task_binder;
    Ftest ftest;
    PlcScanLoop plc_scan;

    ControlContext(CoreContext &core, HardwareContext &hw, CommsContext &comms)
        : telegram_menu(hw.plc, comms.wifi, hw.rtc, comms.telegram_bot, core.configs, core.logs),
          controllers(hw.gpio, hw.ow, hw.eeprom_storage, core.logs, comms.telegram_bot, telegram_menu, comms.gsm),
          meteo_history(hw.rtc, controllers.meteo()),
          rfid_reader(),
          ring_client(hw.gpio, core.logs),
          task_binder(core.tm, comms.wifi, comms.telegram_bot, hw.ext, controllers, meteo_history, rfid_reader,
                      ring_client, hw.display),
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
                     comms.telegram_bot, control.telegram_menu, core.logs, hw.ext, hw.i2c, hw.ow,
                     control.controllers, control.rfid_reader),
          network(core.logs, comms.wifi, comms.gsm, comms.telegram, comms.telegram_bot, control.telegram_menu,
                  fw_upgrade, web, comms.telegram_wifi_client, control.controllers, hw.plc, hw.rtc),
          stack_slave(hw.io, hw.ds18b20, hw.ow, hw.i2c, hw.plc, hw.rtc, comms.telegram, core.logs, hw.ext,
                      control.controllers.sockets(), control.controllers.meteo(), control.controllers.thermo(),
                      control.controllers.septic(), control.controllers.security(), control.controllers.tanks(),
                      control.controllers.ring())
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
        ui.console.setStackMaster(&net.network.stackMaster());
        ui.console.setConfigsManager(cfg.configs_manager);

        control.telegram_menu.setConfigsManager(cfg.configs_manager);
        control.telegram_menu.setStackMaster(net.network.stackMaster());
        control.telegram_menu.setSockets(control.controllers.sockets());
        control.telegram_menu.setMeteo(control.controllers.meteo());
        control.telegram_menu.setThermo(control.controllers.thermo());
        control.telegram_menu.setTanks(control.controllers.tanks());
        control.telegram_menu.setSeptic(control.controllers.septic());
        control.telegram_menu.setSecurity(control.controllers.security());

        net.fw_upgrade.setConfigsManager(cfg.configs_manager);
        net.fw_upgrade.setStackMaster(net.network.stackMaster());
        net.fw_upgrade.setStackSlave(&net.stack_slave);
        net.fw_upgrade.setGsmModem(comms.gsm);
        net.fw_upgrade.setCloudClient(net.network.cloudClient());
        net.network.setStackConfig(cfg.configs_manager);
        control.controllers.thermo().setRemoteMeteoProvider(&App::onRemoteMeteo_, this);
        control.controllers.meteo().setRemoteMeteoProvider(&App::onRemoteMeteoProxy_, this);
        control.controllers.security().setArmStateHandler(&App::onSecurityArmState_, this);
        control.controllers.security().setAlarmStateHandler(&App::onSecurityAlarmState_, this);
        control.controllers.security().setClearDetectHandler(&App::onSecurityClearDetect_, this);
        control.controllers.security().setDetectHandler(&App::onSecurityDetect_, this);
        control.controllers.septic().setDetectHandler(&App::onSepticDetect_, this);
        control.controllers.tanks().setDetectHandler(&App::onTankEmpty_, this);
        control.controllers.ring().setHoldHandler(&App::onRingHold_, this);
        control.ring_client.setButtonHandler(&App::onRingClientButton_, this);
        control.rfid_reader.setUidHandler(&App::onRfidUid_, this);
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
        net.fw_upgrade.initStackCacheAllocations();
        net.fw_upgrade.logStackCacheAllocations();

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
        updateRfidMode_();
        updateRingClientMode_();
        updateRingClientConfig_();
        updateDisplayLayout_();
        net.network.setStackDeviceName(hw.plc.deviceName());
        cfg.configs_manager.setCloudFirmwareVersion(BuildInfo::kFwVersion);
        net.network.setCloudFirmwareVersion(BuildInfo::kFwVersion);
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

        core.logs.info(F("APP"), F("Initializing RFID reader"));
        if (!control.rfid_reader.begin(hw.i2c))
            core.logs.warn(F("APP"), F("RFID reader init failed"));

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
        updateRfidMode_();
        updateRingClientMode_();
        updateRingClientConfig_();
        updateDisplayLayout_();
        pollSecurityStatusFromMaster_();
        control.plc_scan.tick();
        ui.console.loop();
        comms.gsm.loop();
        net.network.loop();
        net.stack_slave.loop();
        core.tm.loop();
        flushPendingSecurityDetect_();
        flushPendingSepticDetect_();
        flushPendingTankEmpty_();
        flushPendingRfid_();
        flushPendingRingClient_();
    }

private:
    static bool onRemoteMeteo_(void *ctx, uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp)
    {
        if (!ctx || node_id == 0 || sensor_id == 0)
            return false;
        App *self = static_cast<App *>(ctx);
        if (self->cfg.configs_manager.stackRole() == ConfigsManagerIface::StackRole::Master)
        {
            auto &stack_cache = self->net.fw_upgrade.stackCache();
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
        return self->net.stack_slave.remoteMeteoTemp(node_id, sensor_id, temp_c, has_temp);
    }

    static bool onRemoteMeteoProxy_(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                    float &temp_c, bool &has_temp, float &hum, bool &has_hum, bool &ok)
    {
        if (!ctx || node_id == 0 || sensor_id == 0)
            return false;
        App *self = static_cast<App *>(ctx);
        if (self->cfg.configs_manager.stackRole() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = self->net.fw_upgrade.stackCache().meteoCache(node_id);
            if (!cache || !cache->has_data)
                return false;
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

    static void onSecurityArmState_(void *ctx, bool armed)
    {
        if (!ctx)
            return;
        App *self = static_cast<App *>(ctx);
        self->broadcastSecurityState_(armed);
        if (!armed)
            self->broadcastSecurityAlarm_(false);
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
            self->sendSecurityStateToNode_(node_id);
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
        static_cast<App *>(ctx)->broadcastSecurityClear_();
    }

    static void onSecurityDetect_(void *ctx, uint8_t sensor_id, const String &name, bool silent)
    {
        if (!ctx)
            return;
        static_cast<App *>(ctx)->sendSecurityDetectToMaster_(sensor_id, name, silent);
    }

    static void onSepticDetect_(void *ctx, uint8_t septic_id, const String &name, bool is_alarm)
    {
        if (!ctx)
            return;
        static_cast<App *>(ctx)->sendSepticDetectToMaster_(septic_id, name, is_alarm);
    }

    static void onTankEmpty_(void *ctx, uint8_t tank_id, const String &name, bool empty)
    {
        if (!ctx)
            return;
        if (!empty)
            return;
        static_cast<App *>(ctx)->sendTankEmptyToMaster_(tank_id, name);
    }

    static void onRingHold_(void *ctx, bool on)
    {
        if (!ctx)
            return;
        App *self = static_cast<App *>(ctx);
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

    static void onRingClientButton_(void *ctx, bool pressed)
    {
        if (!ctx)
            return;
        static_cast<App *>(ctx)->handleRingClientButton_(pressed);
    }

    static void onRfidUid_(void *ctx, const RfidReader::Uid &uid)
    {
        if (!ctx)
            return;
        static_cast<App *>(ctx)->handleRfidUid_(uid);
    }

    static bool onDisplaySlot_(void *ctx, const DisplaySlotConfig &slot, char out[5])
    {
        if (!ctx)
            return false;
        return static_cast<App *>(ctx)->renderDisplaySlot_(slot, out);
    }

    void broadcastSecurityState_(bool armed)
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Master)
            return;
        StackMaster &master = net.network.stackMaster();
        const size_t count = master.nodeCount();
        for (size_t i = 0; i < count; ++i)
            sendSecurityStateToNode_(master.nodeIdAt(i), armed);
    }

    void sendSecurityStateToNode_(uint32_t node_id)
    {
        sendSecurityStateToNode_(node_id, control.controllers.security().armed());
    }

    void sendSecurityStateToNode_(uint32_t node_id, bool armed)
    {
        if (node_id == 0)
            return;
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Master)
            return;
        StackMaster &master = net.network.stackMaster();

        StaticJsonDocument<128> doc;
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        params["armed"] = armed;
        params["alarm"] = armed ? control.controllers.security().alarmOn() : false;

        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return;
        master.sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                      reinterpret_cast<const uint8_t *>(payload), len);
    }

    void sendSecurityDetectToMaster_(uint8_t sensor_id, const String &name, bool silent)
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
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
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
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
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
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

    void broadcastRingHold_(bool on)
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Master)
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
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Master)
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
                const bool was_armed = sec.armed();
                const bool matched = sec.processRfidUidString(uid.c_str(), source.c_str());
                const String result = matched ? (was_armed ? "disarm" : "arm") : "reject";
                sendRfidResultToNode_(node_id, uid, matched, result, sec.armed());
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
        control.controllers.tanks().notifyRemoteEmpty(source, tank_id, name);
    }

    void updateSecurityNotifyMode_()
    {
        const auto role = cfg.configs_manager.stackRole();
        control.controllers.security().setNotifyEnabled(role == ConfigsManagerIface::StackRole::Master);
    }

    void updateSepticNotifyMode_()
    {
        const auto role = cfg.configs_manager.stackRole();
        control.controllers.septic().setNotifyEnabled(role == ConfigsManagerIface::StackRole::Master);
    }

    void updateTanksNotifyMode_()
    {
        const auto role = cfg.configs_manager.stackRole();
        control.controllers.tanks().setNotifyEnabled(role == ConfigsManagerIface::StackRole::Master);
    }

    void updateRfidMode_()
    {
        const auto role = cfg.configs_manager.stackRole();
        const bool enable = (role == ConfigsManagerIface::StackRole::Slave) &&
                            cfg.configs_manager.rfidEnabled();
        if (_rfid_enabled != enable)
        {
            _rfid_enabled = enable;
            control.rfid_reader.setEnabled(enable);
        }
    }

    void updateRingClientMode_()
    {
        const auto role = cfg.configs_manager.stackRole();
        const bool enable = (role == ConfigsManagerIface::StackRole::Slave) &&
                            cfg.configs_manager.ringClientEnabled();
        if (_ring_client_enabled != enable)
        {
            _ring_client_enabled = enable;
            control.ring_client.setEnabled(enable);
        }
    }

    void updateRingClientConfig_()
    {
        const uint8_t port = cfg.configs_manager.ringClientButtonPort();
        if (_ring_client_button_port != port)
        {
            _ring_client_button_port = port;
            control.ring_client.setButtonPort(port);
        }
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
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
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
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
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
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_tank_empty = false;
        sendTankEmptyToMaster_(_pending_tank_id, _pending_tank_name);
    }

    void flushPendingRfid_()
    {
        if (!_pending_rfid)
            return;
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_rfid = false;
        sendRfidToMaster_(_pending_rfid_uid, _pending_rfid_name);
    }

    void flushPendingRingClient_()
    {
        if (!_pending_ring_client)
            return;
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
            return;
        _pending_ring_client = false;
        sendRingClientToMaster_(_pending_ring_client_pressed);
    }

    void handleRfidUid_(const RfidReader::Uid &uid)
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
            return;
        const String uid_str = RfidReader::uidToString(uid);
        if (uid_str.length() == 0)
            return;
        const String name = hw.plc.deviceName();
        StackNode &node = net.network.stackNode();
        if (!node.connected())
        {
            _pending_rfid = true;
            _pending_rfid_uid = uid_str;
            _pending_rfid_name = name;
            return;
        }
        if (!sendRfidToMaster_(uid_str, name))
        {
            _pending_rfid = true;
            _pending_rfid_uid = uid_str;
            _pending_rfid_name = name;
        }
    }

    void handleRingClientButton_(bool pressed)
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
            return;
        if (!cfg.configs_manager.ringClientEnabled())
            return;
        StackNode &node = net.network.stackNode();
        if (!node.connected())
        {
            _pending_ring_client = true;
            _pending_ring_client_pressed = pressed;
            return;
        }
        if (!sendRingClientToMaster_(pressed))
        {
            _pending_ring_client = true;
            _pending_ring_client_pressed = pressed;
        }
    }

    bool sendRingClientToMaster_(bool pressed)
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
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
            snprintf(out, 5, "%02u%02u", (unsigned)dt.hour, (unsigned)dt.minute);
            return true;
        }
        case DisplaySlotKind::Security:
        {
            const bool armed = control.controllers.security().armed();
            const char *txt = armed ? "ARM " : "DIS ";
            memcpy(out, txt, 4);
            return true;
        }
        case DisplaySlotKind::Socket:
        {
            if (slot.index == 0)
                return false;
            const SocketController::SocketState *st = control.controllers.sockets().state(slot.index);
            if (!st)
                return false;
            const char *txt = st->relay_on ? "ON  " : "OFF ";
            memcpy(out, txt, 4);
            return true;
        }
        case DisplaySlotKind::Light:
        {
            if (slot.index == 0)
                return false;
            const SocketController::LightState *st = control.controllers.sockets().lightState(slot.index);
            if (!st)
                return false;
            const char *txt = st->relay_on ? "ON  " : "OFF ";
            memcpy(out, txt, 4);
            return true;
        }
        case DisplaySlotKind::Meteo:
        {
            if (slot.index == 0)
                return false;
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
                snprintf(out, 5, "%2d%c", t, Display::kDegreeChar);
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
        case DisplaySlotKind::Tank:
        {
            if (slot.index == 0)
                return false;
            const TankController::TankState *st = control.controllers.tanks().state(slot.index);
            if (!st || !st->levels_ok)
                return false;
            if (st->level_full)
                memcpy(out, "FULL", 4);
            else if (st->level_mid)
                memcpy(out, "MID ", 4);
            else if (st->level_low)
                memcpy(out, "LOW ", 4);
            else
                memcpy(out, "EMP ", 4);
            return true;
        }
        case DisplaySlotKind::Septic:
        {
            if (slot.index == 0)
                return false;
            const size_t idx = (size_t)(slot.index - 1);
            const SepticController::SepticState *st = control.controllers.septic().stateByIndex(idx);
            if (!st)
                return false;
            if (st->alarm)
                memcpy(out, "ALRM", 4);
            else if (st->warning)
                memcpy(out, "WARN", 4);
            else
                memcpy(out, "OK  ", 4);
            return true;
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

    static bool displaySlotEqual_(const DisplaySlotConfig &a, const DisplaySlotConfig &b)
    {
        if (a.kind != b.kind || a.index != b.index || a.field != b.field)
            return false;
        return strncmp(a.text, b.text, sizeof(a.text)) == 0;
    }

    bool sendRfidToMaster_(const String &uid, const String &name)
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
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

    void sendRfidResultToNode_(uint32_t node_id, const String &uid, bool matched,
                               const String &result, bool armed)
    {
        if (node_id == 0)
            return;
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Master)
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

    void pollSecurityStatusFromMaster_()
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Slave)
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

    void broadcastSecurityAlarm_(bool alarm_on)
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Master)
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
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Master)
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

    void broadcastSecurityClear_()
    {
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Master)
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
        if (cfg.configs_manager.stackRole() != ConfigsManagerIface::StackRole::Master)
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
    bool _pending_rfid = false;
    String _pending_rfid_uid;
    String _pending_rfid_name;
    bool _rfid_enabled = false;
    uint32_t _last_rfid_status_ms = 0;
    bool _ring_client_enabled = false;
    uint8_t _ring_client_button_port = 0xFF;
    bool _pending_ring_client = false;
    bool _pending_ring_client_pressed = false;
    DisplaySlotConfig _display_slots[Display::kSlotCount]{};
};
