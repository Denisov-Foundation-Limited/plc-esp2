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
          task_binder(core.tm, comms.wifi, comms.telegram_bot, hw.ext, controllers, meteo_history),
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
                     control.controllers),
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
        net.fw_upgrade.setGsmModem(comms.gsm);
        net.fw_upgrade.setCloudClient(net.network.cloudClient());
        net.network.setStackConfig(cfg.configs_manager);
        control.controllers.security().setArmStateHandler(&App::onSecurityArmState_, this);
        control.controllers.security().setAlarmStateHandler(&App::onSecurityAlarmState_, this);
        control.controllers.security().setClearDetectHandler(&App::onSecurityClearDetect_, this);
        control.controllers.security().setDetectHandler(&App::onSecurityDetect_, this);
        control.controllers.septic().setDetectHandler(&App::onSepticDetect_, this);
        control.controllers.tanks().setDetectHandler(&App::onTankEmpty_, this);
        control.controllers.ring().setHoldHandler(&App::onRingHold_, this);
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
        net.network.loop();
        flushPendingSecurityDetect_();
        flushPendingSepticDetect_();
        flushPendingTankEmpty_();
    }

private:
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
            if (action != "alarm")
                return;
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
};

