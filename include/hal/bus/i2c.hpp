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
#include <Wire.h>

#include "boards/board_profile.hpp"
class I2CManager
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
        for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
        {
            const auto c = ActiveBoardProfile::I2CS[i];
            TwoWire *w = wirePtr_(c.bus_num);
            if (!w)
            {
                _err = Error::InvalidBus;
                return false;
            }

#if defined(ESP32)
            w->begin(c.sda, c.scl, c.freq);
#else
            w->begin();
            w->setClock(c.freq);
#endif
        }
        return true;
    }

    TwoWire *wirePtr(uint8_t bus_num) { return wirePtr_(bus_num); }

    TwoWire &wire(uint8_t bus_num)
    {
        TwoWire *w = wirePtr_(bus_num);
        if (!w)
        {
            _err = Error::InvalidBus;
            return Wire;
        }
        return *w;
    }

    Error lastError() const { return _err; }

private:
    Error _err = Error::Ok;

    static TwoWire *wirePtr_(uint8_t bus_num)
    {
        switch (bus_num)
        {
        case 0:
            return &Wire;
#if defined(ESP32)
        case 1:
            return &Wire1;
#if defined(Wire2)
        case 2:
            return &Wire2;
#endif
#endif
        default:
            return nullptr;
        }
    }
};
