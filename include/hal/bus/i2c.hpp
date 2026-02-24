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

class TwoWire;

class I2CManager
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        InvalidBus,
        InvalidPins
    };

    bool beginAll();
    TwoWire *wirePtr(uint8_t bus_num);
    TwoWire &wire(uint8_t bus_num);
    Error lastError() const;
    bool scanDevices(uint8_t bus_num, bool present[127]);
    bool probeAddress(uint8_t bus_num, uint8_t addr);

private:
    Error _err = Error::Ok;

    static bool i2cPinsFromPorts_(uint8_t sda_port, uint8_t scl_port,
                                  uint8_t &out_sda_gpio, uint8_t &out_scl_gpio);
    static TwoWire *wirePtr_(uint8_t bus_num);
};
