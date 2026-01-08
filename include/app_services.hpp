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

#include "boards/board_profile.hpp"
#include "boards/profile_validator.hpp"

#include "core/task_binder.hpp"
#include "core/task_manager.hpp"
#include "core/wifi_manager.hpp"

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

#include "utils/logger.hpp"

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
    Ds3231Mz rtc;
    At24lc512 eeprom;
    Lcd1602I2c lcd;
    Lm75ad lm75ad;
    Sim800l sim800l;

    Extender ext;
    PortIO portio;
    Gpio gpio;

    TaskManager<TASK_MGR_TSK_COUNT> tm;
    TaskBinder<TASK_MGR_TSK_COUNT> task_binder;

    AppServices()
        : logs(uart),
          wifi(logs),
          ext(ActiveBoardProfile::EXT_DEVS),
          portio(ActiveBoardProfile::PORTS, &ext),
          gpio(portio),
          ibutton(ow),
          ds18b20(ow),
          dht22(),
          rtc(i2c),
          eeprom(i2c),
          lcd(i2c),
          lm75ad(i2c),
          task_binder(tm, wifi)
    {
    }

    bool begin()
    {
        if (!logs.beginAuto())
        {
            Serial.begin(UartManager::DEFAULT_SPEED);
            logs.begin(Serial);
            logs.error(F("APP"), F("LOG Auto bind failed, fallback to USB"));
        }

        logs.info(F("APP"), F("Starting application..."));
        ProfileValidator<ActiveBoardProfile>::printDiagnostics(Serial);

        bool ok = true;

        if (!i2c.beginAll())
        {
            logs.error(F("APP"), F("I2C Init failed"));
            ok = false;
        }
        else if (!rtc.begin())
        {
            logs.error(F("APP"), F("RTC Init failed"));
            ok = false;
        }
        else if (!eeprom.begin())
        {
            logs.error(F("APP"), F("EEPROM Init failed"));
            ok = false;
        }
        else if (!lcd.begin())
        {
            logs.error(F("APP"), F("LCD Init failed"));
            ok = false;
        }
        if (!spi.beginAll())
        {
            logs.error(F("APP"), F("SPI Init failed"));
            ok = false;
        }
        if (!ow.beginAll())
        {
            logs.error(F("APP"), F("OW Init failed"));
            ok = false;
        }
        else if (!ibutton.begin(OneWireManager::OwBusType::iButton))
        {
            logs.error(F("APP"), F("OW iButton bus missing"));
            ok = false;
        }
        else if (!ds18b20.begin(OneWireManager::OwBusType::Temp))
        {
            logs.error(F("APP"), F("OW DS18B20 bus missing"));
            ok = false;
        }

        HardwareSerial *sim_ser = uart.beginSerialForIndex(0);
        if (!sim_ser)
        {
            logs.error(F("APP"), F("SIM UART index 0 not available"));
            ok = false;
        }
        else if (!sim800l.begin(*sim_ser))
        {
            logs.error(F("APP"), F("SIM Init failed"));
            ok = false;
        }
        else
        {
            sim800l.setEcho(false);
            sim800l.setSmsTextMode();
            sim800l.setCallerId(true);
        }

        ext.begin(i2c);

        if (!gpio.begin())
        {
            logs.error(F("APP"), F("GPIO Init failed"));
            ok = false;
        }
        else
        {
            logs.info(F("APP"), F("GPIO Init OK"));
        }

        if (!wifi.begin())
        {
            logs.error(F("APP"), F("WIFI Init failed"));
            ok = false;
        }

        task_binder.bindAll();

        return ok;
    }

    void tick()
    {
        gpio.tick();
        tm.tick();
    }
};
