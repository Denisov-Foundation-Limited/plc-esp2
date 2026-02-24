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

class OneWireManager;
class I2CManager;
class SPIManager;
class UartManager;
class Gpio;
class Logger;

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

    bool begin();
    void loop();
    Error lastError() const;
    static const char *errorName(Error err);

private:
    bool uartOk_() const;

    OneWireManager &_ow;
    I2CManager &_i2c;
    SPIManager &_spi;
    UartManager &_uart;
    Gpio &_gpio;
    Logger &_logs;
    Error _err = Error::Ok;
};
