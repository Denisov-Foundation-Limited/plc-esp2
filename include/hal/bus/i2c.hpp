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
#include <vector>

#include "boards/board_profile.hpp"
#include "hal/gpio/portio.hpp"

class I2CManager
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
            uint8_t sda_gpio = 0;
            uint8_t scl_gpio = 0;
            if (!i2cPinsFromPorts_(c.sda, c.scl, sda_gpio, scl_gpio))
            {
                _err = Error::InvalidPins;
                return false;
            }
            w->begin(sda_gpio, scl_gpio, c.freq);
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

    bool scanDevices(uint8_t bus_num, std::vector<uint8_t> &addrs)
    {
        TwoWire *w = wirePtr_(bus_num);
        if (!w)
        {
            _err = Error::InvalidBus;
            return false;
        }
        addrs.clear();
        for (uint8_t addr = 1; addr < 127; ++addr)
        {
            w->beginTransmission(addr);
            uint8_t res = w->endTransmission();
            if (res == 0)
                addrs.push_back(addr);
        }
        return true;
    }

private:
    Error _err = Error::Ok;

    static bool i2cPinsFromPorts_(uint8_t sda_port, uint8_t scl_port,
                                 uint8_t &out_sda_gpio, uint8_t &out_scl_gpio)
    {
        if (sda_port >= PortIO::PORT_COUNT || scl_port >= PortIO::PORT_COUNT)
            return false;
        const auto &psda = ActiveBoardProfile::PORTS[sda_port];
        const auto &pscl = ActiveBoardProfile::PORTS[scl_port];
        if (psda.backend != PortIO::Backend::Esp32 || pscl.backend != PortIO::Backend::Esp32)
            return false;
        if (psda.u.esp.gpio == 0xFF || pscl.u.esp.gpio == 0xFF)
            return false;
        out_sda_gpio = psda.u.esp.gpio;
        out_scl_gpio = pscl.u.esp.gpio;
        return true;
    }

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
