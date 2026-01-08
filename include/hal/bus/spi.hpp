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

class SPIManager
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        InvalidBus
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
            s->begin(c.sck < 0 ? -1 : c.sck,
                     c.miso < 0 ? -1 : c.miso,
                     c.mosi < 0 ? -1 : c.mosi,
                     c.cs < 0 ? -1 : c.cs);
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
