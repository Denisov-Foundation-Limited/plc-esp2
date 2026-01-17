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

#include <stdint.h>

#include "boards/board_profile.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/bus/spi.hpp"
#include "hal/bus/uart.hpp"
#include "hal/gpio/gpio.hpp"
#include "utils/logger.hpp"

class Hal
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        I2c,
        Gpio,
        Spi,
        OneWire,
        Uart
    };

    Hal(OneWireManager &ow, I2CManager &i2c, SPIManager &spi, UartManager &uart, Gpio &gpio, Logger &logs)
        : _ow(ow), _i2c(i2c), _spi(spi), _uart(uart), _gpio(gpio), _logs(logs)
    {
    }

    bool begin()
    {
        _err = Error::Ok;
        _logs.info(F("HAL"), F("I2C init"));
        if (!_i2c.beginAll())
        {
            _err = Error::I2c;
            return false;
        }
        _logs.info(F("HAL"), F("GPIO init"));
        if (!_gpio.begin())
        {
            _err = Error::Gpio;
            return false;
        }
        _logs.info(F("HAL"), F("SPI init"));
        if (!_spi.beginAll())
        {
            _err = Error::Spi;
            return false;
        }
        _logs.info(F("HAL"), F("OneWire init"));
        if (!_ow.beginAll())
        {
            _err = Error::OneWire;
            return false;
        }
        _logs.info(F("HAL"), F("UART init"));
        if (!uartOk_())
        {
            _err = Error::Uart;
            return false;
        }

        return true;
    }

    void loop()
    {
        _gpio.loop();
    }

    Error lastError() const { return _err; }
    static const char *errorName(Error err)
    {
        switch (err)
        {
        case Error::I2c:
            return "I2C init failed";
        case Error::Gpio:
            return "GPIO init failed";
        case Error::Spi:
            return "SPI init failed";
        case Error::OneWire:
            return "OneWire init failed";
        case Error::Uart:
            return "UART init failed";
        case Error::Ok:
            return "OK";
        default:
            return "Unknown";
        }
    }

private:
    bool uartOk_() const
    {
        for (uint8_t i = 0; i < UartManager::count(); ++i)
        {
            const auto u = ActiveBoardProfile::UARTS[i];
            if (_uart.serialByUartNum(u.uart_num) == nullptr)
                return false;
        }
        return true;
    }

    OneWireManager &_ow;
    I2CManager &_i2c;
    SPIManager &_spi;
    UartManager &_uart;
    Gpio &_gpio;
    Logger &_logs;
    Error _err = Error::Ok;
};
