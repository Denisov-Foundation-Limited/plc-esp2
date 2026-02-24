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

class SPIClass;

class SPIManager
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        InvalidBus,
        InvalidPins
    };

    bool beginAll();
    SPIClass *spiPtr(uint8_t bus_num);
    SPIClass &spi(uint8_t bus_num);
    Error lastError() const;

private:
    Error _err = Error::Ok;

    static bool spiPinFromPort_(int8_t port, int &out_gpio);
    static SPIClass *spiPtr_(uint8_t bus_num);
};
