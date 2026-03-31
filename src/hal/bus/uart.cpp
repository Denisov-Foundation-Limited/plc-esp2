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

#include "hal/bus/uart.hpp"
#include "hal/gpio/portio.hpp"

HardwareSerial *UartManager::serialByUartNum(uint8_t uart_num)
{
    switch (uart_num)
    {
    case 1:
        return &Serial1;
    case 2:
        return &Serial2;
    default:
        return nullptr;
    }
}

bool UartManager::beginByIndex(uint8_t idx, HardwareSerial &ser, uint32_t config)
{
    if (idx >= ActiveBoardProfile::UART_COUNT)
        return false;
    const auto u = ActiveBoardProfile::UARTS[idx];
    uint8_t tx_gpio = 0;
    uint8_t rx_gpio = 0;
    if (!uartPinsFromPorts_(u.tx, u.rx, tx_gpio, rx_gpio))
        return false;
    ser.begin(u.baud, config, rx_gpio, tx_gpio);
    return true;
}

HardwareSerial *UartManager::beginSerialForIndex(uint8_t idx, uint32_t config)
{
    if (idx >= ActiveBoardProfile::UART_COUNT)
        return nullptr;
    const auto u = ActiveBoardProfile::UARTS[idx];
    HardwareSerial *ser = serialByUartNum(u.uart_num);
    if (!ser)
        return nullptr;
    uint8_t tx_gpio = 0;
    uint8_t rx_gpio = 0;
    if (!uartPinsFromPorts_(u.tx, u.rx, tx_gpio, rx_gpio))
        return nullptr;
    ser->begin(u.baud, config, rx_gpio, tx_gpio);
    return ser;
}

bool UartManager::uartPinsFromPorts_(uint8_t tx_port, uint8_t rx_port,
                                     uint8_t &out_tx_gpio, uint8_t &out_rx_gpio)
{
    if (tx_port >= PortIO::PORT_COUNT || rx_port >= PortIO::PORT_COUNT)
        return false;
    const auto &ptx = ActiveBoardProfile::PORTS[tx_port];
    const auto &prx = ActiveBoardProfile::PORTS[rx_port];
    if (ptx.backend != PortIO::Backend::Esp32 || prx.backend != PortIO::Backend::Esp32)
        return false;
    if (ptx.u.esp.gpio == 0xFF || prx.u.esp.gpio == 0xFF)
        return false;
    out_tx_gpio = ptx.u.esp.gpio;
    out_rx_gpio = prx.u.esp.gpio;
    return true;
}
