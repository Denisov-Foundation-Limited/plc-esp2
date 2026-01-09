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
#include <SPI.h>
#include <stdint.h>

#include "boards/board_profile.hpp"
#include "boards/board_profile_base.hpp"
#include "hal/gpio/portio.hpp"

class SPIManager
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        InvalidBus,
        InvalidPins
    };

    bool beginAll()
    {
        _err = Error::Ok;
        if (ActiveBoardProfile::SPI_COUNT == 0)
            return true;

        for (uint8_t i = 0; i < ActiveBoardProfile::SPI_COUNT; ++i)
        {
            const SpiCfg &c = ActiveBoardProfile::SPIS[i];
            SPIClass *s = spiPtr_(c.bus_num);
            if (!s)
            {
                _err = Error::InvalidBus;
                return false;
            }

#if defined(ESP32)
            int sck_gpio = -1;
            int miso_gpio = -1;
            int mosi_gpio = -1;
            int cs_gpio = -1;
            if (!spiPinFromPort_(c.sck, sck_gpio) ||
                !spiPinFromPort_(c.miso, miso_gpio) ||
                !spiPinFromPort_(c.mosi, mosi_gpio) ||
                !spiPinFromPort_(c.cs, cs_gpio))
            {
                _err = Error::InvalidPins;
                return false;
            }
            s->begin(sck_gpio, miso_gpio, mosi_gpio, cs_gpio);
#else
            (void)c;
            s->begin();
#endif
        }
        return true;
    }

    SPIClass *spiPtr(uint8_t bus_num) { return spiPtr_(bus_num); }

    SPIClass &spi(uint8_t bus_num)
    {
        SPIClass *s = spiPtr_(bus_num);
        if (!s)
        {
            _err = Error::InvalidBus;
            return SPI;
        }
        return *s;
    }

    Error lastError() const { return _err; }

private:
    Error _err = Error::Ok;

    static bool spiPinFromPort_(int8_t port, int &out_gpio)
    {
        if (port < 0)
        {
            out_gpio = -1;
            return true;
        }
        if (port >= PortIO::PORT_COUNT)
            return false;
        const auto &p = ActiveBoardProfile::PORTS[(uint8_t)port];
        if (p.backend != PortIO::Backend::Esp32)
            return false;
        if (p.u.esp.gpio == 0xFF)
            return false;
        out_gpio = p.u.esp.gpio;
        return true;
    }

    static SPIClass *spiPtr_(uint8_t bus_num)
    {
        switch (bus_num)
        {
        case 0:
            return &SPI;
#if defined(ESP32)
        case 1:
        {
            static SPIClass hspi(HSPI);
            return &hspi;
        }
#endif
        default:
            return nullptr;
        }
    }
};
