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

namespace
{
void appConsoleLoopCb_(void *ctx)
{
    if (!ctx)
        return;
    static_cast<CliConsole *>(ctx)->loop();
}

struct AppI2cLockCtx
{
    I2CManager *i2c = nullptr;
    uint8_t bus = 0;
};
AppI2cLockCtx g_app_eeprom_lock_ctx{};

struct AppI2cProbeEntry
{
    uint8_t addr;
    const __FlashStringHelper *name;
};

bool appI2cLockCb_(void *ctx, uint32_t timeout_ms)
{
    AppI2cLockCtx *c = static_cast<AppI2cLockCtx *>(ctx);
    return c && c->i2c ? c->i2c->lockBus(c->bus, timeout_ms) : false;
}

void appI2cUnlockCb_(void *ctx)
{
    AppI2cLockCtx *c = static_cast<AppI2cLockCtx *>(ctx);
    if (c && c->i2c)
        c->i2c->unlockBus(c->bus);
}

void appLogI2cProbeMap_(Logger &logs, I2CManager &i2c, uint8_t bus,
                        const AppI2cProbeEntry *items, size_t count)
{
    if (!items || count == 0)
        return;
    for (size_t i = 0; i < count; ++i)
    {
        const bool ok = i2c.probeAddress(bus, items[i].addr);
        logs.info(F("I2C"), F("Probe: bus: %u addr: 0x%02X dev: %S ok: %s"),
                  (unsigned)bus, (unsigned)items[i].addr, items[i].name, ok ? "true" : "false");
    }
}
} // namespace

#ifndef APP_GPIO_SCAN_METRICS
#define APP_GPIO_SCAN_METRICS 0
#endif

#ifndef APP_GPIO_SCAN_PERIOD_MS
#define APP_GPIO_SCAN_PERIOD_MS 1000u
#endif

#ifndef APP_GPIO_SCAN_WARN_MS
#define APP_GPIO_SCAN_WARN_MS 100u
#endif

#ifndef APP_GPIO_SCAN_WARN_CONSECUTIVE
#define APP_GPIO_SCAN_WARN_CONSECUTIVE 3u
#endif

#ifndef APP_GPIO_SCAN_REPORT_MS
#define APP_GPIO_SCAN_REPORT_MS 60000u
#endif

CoreContext::CoreContext()
        : logs(uart),
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
          plc_scan(hw.io),
          task_binder(comms.wifi, comms.telegram_bot, hw.ext, controllers, meteo_history,
                      hw.display, hw.plc, plc_scan, core.logs),
          ftest(core.logs, hw.io, hw.ow, hw.ibutton, hw.ds18b20, hw.i2c, hw.rtc, hw.ext, task_binder)
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
        comms.wifi.setIo(hw.io);
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
    net.network.cloudClient().setStackCache(&stack.stackCache());
    net.fw_upgrade.setUsersRegistry(control.users);
    net.fw_upgrade.setRules(control.rules);

    net.network.setStackConfig(cfg.configs_manager);
    control.task_binder.setGsmModem(comms.gsm);
    control.task_binder.setCloudClient(net.network.cloudClient());
    control.task_binder.setNetwork(net.network);
    control.task_binder.setConsoleLoop(&appConsoleLoopCb_, &ui.console);

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
    switch (comms.wifi.mode())
    {
    case WifiManager::Mode::StaAp:
        core.logs.info(F("WIFI"), F("Mode: STA+AP (STA SSID: %s AP SSID: %s)"),
                       comms.wifi.ssid().c_str(), comms.wifi.apSsid().c_str());
        break;
    case WifiManager::Mode::Ap:
        core.logs.info(F("WIFI"), F("Mode: AP (SSID: %s)"), comms.wifi.apSsid().c_str());
        break;
    case WifiManager::Mode::Sta:
    default:
        core.logs.info(F("WIFI"), F("Mode: STA (SSID: %s)"), comms.wifi.ssid().c_str());
        break;
    }

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
        g_app_eeprom_lock_ctx.i2c = &hw.i2c;
        g_app_eeprom_lock_ctx.bus = cfg_eeprom.bus_num;
        hw.eeprom.setBusLockCallbacks(&appI2cLockCb_, &appI2cUnlockCb_, &g_app_eeprom_lock_ctx);
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
    {
        static constexpr AppI2cProbeEntry kBus0BootProbe[] = {
            { 0x20, F("mcp0") },
            { 0x21, F("mcp1") },
            { 0x22, F("lcd") },
            { 0x50, F("eeprom") },
            { 0x68, F("rtc") },
        };
        appLogI2cProbeMap_(core.logs, hw.i2c, 0, kBus0BootProbe,
                           sizeof(kBus0BootProbe) / sizeof(kBus0BootProbe[0]));
    }
    const bool rtc_ok = hw.rtc.begin();
    if (!rtc_ok)
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
    else if (bl_pin != 0xFF)
    {
        hw.portio.pinMode(bl_pin, PortIO::PortMode::Output);
        hw.portio.write(bl_pin, true);
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

    control.controllers.restoreFromStorage();
    hw.io.applyOutputs();
    hw.portio.setOutputsEnabled(true);
    control.plc_scan.begin();

    if (ok)
        core.logs.info(F("APP"), F("Application init [OK]"));
    else
        core.logs.error(F("APP"), F("Application init [FAIL]"));

    control.task_binder.bindFtest(control.ftest);
    control.task_binder.bindAll();
    control.task_binder.bindStack(stack);
    if (rtc_ok)
        core.logs.setRtc(hw.rtc);

    return ok;
}

void App::loop()
{
    control.task_binder.runStackPre(stack);

#if APP_GPIO_SCAN_METRICS
    {
        static uint32_t last_scan_ms = 0;
        static uint32_t last_report_ms = 0;
        static uint32_t max_scan_us = 0;
        static uint32_t max_gap_us = 0;
        static uint32_t max_gap_overrun_us = 0;
        static uint32_t prev_scan_started_us = 0;
        static uint16_t slow_scan_streak = 0;
        static bool slow_warn_active = false;

        const uint32_t now_ms = millis();
        if ((uint32_t)(now_ms - last_scan_ms) >= APP_GPIO_SCAN_PERIOD_MS)
        {
            last_scan_ms = now_ms;

            uint16_t input_count = 0;
            uint16_t ext_input_count = 0;
            const uint32_t started_us = micros();
            uint32_t gap_us = 0;
            uint32_t gap_overrun_us = 0;
            if (prev_scan_started_us != 0)
            {
                gap_us = started_us - prev_scan_started_us;
                const uint32_t expected_gap_us = APP_GPIO_SCAN_PERIOD_MS * 1000u;
                if (gap_us > expected_gap_us)
                    gap_overrun_us = gap_us - expected_gap_us;
            }
            prev_scan_started_us = started_us;

            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                const auto &p = hw.portio.desc(i);
                if (p.caps == Cap::None || !has(p.caps, Cap::Input))
                    continue;
                ++input_count;
                if (p.backend == PortIO::Backend::Extender)
                    ++ext_input_count;
                (void)hw.portio.read(i);
            }
            const uint32_t scan_us = micros() - started_us;
            if (scan_us > max_scan_us)
                max_scan_us = scan_us;
            if (gap_us > max_gap_us)
                max_gap_us = gap_us;
            if (gap_overrun_us > max_gap_overrun_us)
                max_gap_overrun_us = gap_overrun_us;

            const bool slow_scan = (scan_us >= APP_GPIO_SCAN_WARN_MS * 1000u);
            const bool delayed_scan = (gap_overrun_us >= APP_GPIO_SCAN_WARN_MS * 1000u);
            const bool slow_condition = (slow_scan || delayed_scan);
            if (slow_condition)
                ++slow_scan_streak;
            else
                slow_scan_streak = 0;

            if (slow_scan_streak >= APP_GPIO_SCAN_WARN_CONSECUTIVE && !slow_warn_active)
            {
                core.logs.warn(F("GPIO"), F("Input scan slow streak: gap_us: %lu max_gap_us: %lu gap_overrun_us: %lu max_gap_overrun_us: %lu scan_us: %lu max_us: %lu streak: %u limit_ms: %u inputs: %u ext_inputs: %u"),
                               (unsigned long)gap_us,
                               (unsigned long)max_gap_us,
                               (unsigned long)gap_overrun_us,
                               (unsigned long)max_gap_overrun_us,
                               (unsigned long)scan_us,
                               (unsigned long)max_scan_us,
                               (unsigned)slow_scan_streak,
                               (unsigned)APP_GPIO_SCAN_WARN_MS,
                               (unsigned)input_count,
                               (unsigned)ext_input_count);
                slow_warn_active = true;
            }
            if (!slow_condition)
                slow_warn_active = false;

            if ((uint32_t)(now_ms - last_report_ms) >= APP_GPIO_SCAN_REPORT_MS)
            {
                last_report_ms = now_ms;
                core.logs.info(F("GPIO"), F("Input scan stats: gap_us: %lu max_gap_us: %lu gap_overrun_us: %lu max_gap_overrun_us: %lu scan_us: %lu max_us: %lu inputs: %u ext_inputs: %u"),
                               (unsigned long)gap_us,
                               (unsigned long)max_gap_us,
                               (unsigned long)gap_overrun_us,
                               (unsigned long)max_gap_overrun_us,
                               (unsigned long)scan_us,
                               (unsigned long)max_scan_us,
                               (unsigned)input_count,
                               (unsigned)ext_input_count);
            }
        }
    }
#endif
}
