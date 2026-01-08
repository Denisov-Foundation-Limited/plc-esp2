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

class UartManager
{
public:
    static constexpr uint8_t count() { return ActiveBoardProfile::UART_COUNT; }
    static constexpr unsigned DEFAULT_SPEED = 115200;

    HardwareSerial *serialByUartNum(uint8_t uart_num)
    {
#if defined(ESP32)
        switch (uart_num)
        {
        case 1:
            return &Serial1;
        case 2:
            return &Serial2;
        default:
            return nullptr;
        }
#else
        (void)uart_num;
        return nullptr;
#endif
    }

    bool beginByIndex(uint8_t idx, HardwareSerial &ser, uint32_t config = SERIAL_8N1)
    {
        if (idx >= ActiveBoardProfile::UART_COUNT)
            return false;
        const auto u = ActiveBoardProfile::UARTS[idx];
#if defined(ESP32)
        ser.begin(u.baud, config, u.rx, u.tx);
        return true;
#else
        (void)ser;
        (void)config;
        return false;
#endif
    }

    HardwareSerial *beginSerialForIndex(uint8_t idx, uint32_t config = SERIAL_8N1)
    {
        if (idx >= ActiveBoardProfile::UART_COUNT)
            return nullptr;
        const auto u = ActiveBoardProfile::UARTS[idx];
        HardwareSerial *ser = serialByUartNum(u.uart_num);
        if (!ser)
            return nullptr;
#if defined(ESP32)
        ser->begin(u.baud, config, u.rx, u.tx);
        return ser;
#else
        return nullptr;
#endif
    }
};
