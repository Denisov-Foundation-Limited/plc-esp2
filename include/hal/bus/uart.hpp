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

    HardwareSerial *serialByUartNum(uint8_t uart_num);
    bool beginByIndex(uint8_t idx, HardwareSerial &ser, uint32_t config = SERIAL_8N1);
    HardwareSerial *beginSerialForIndex(uint8_t idx, uint32_t config = SERIAL_8N1);

private:
    static bool uartPinsFromPorts_(uint8_t tx_port, uint8_t rx_port,
                                   uint8_t &out_tx_gpio, uint8_t &out_rx_gpio);
};
