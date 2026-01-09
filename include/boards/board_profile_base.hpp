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
#include <array>
#include <stdint.h>
// ---------- Serial logging sink ----------
struct LogCfg
{
    enum class Sink : uint8_t
    {
        UsbSerial, // Serial
        UartIndex  // UARTS[index] -> Serial1/Serial2
    };

    Sink sink = Sink::UsbSerial;
    uint8_t uart_index = 0;     // only if Sink::UartIndex
    uint32_t usb_baud = 115200; // only if Sink::UsbSerial
};

// ---------- Peripheral configs ----------
struct UartCfg
{
    uint8_t uart_num; // ESP32 Arduino: 1->Serial1, 2->Serial2
    int8_t tx;        // PortIO::PortId for TX
    int8_t rx;        // PortIO::PortId for RX
    uint32_t baud;
};

struct I2cCfg
{
    uint8_t bus_num; // 0->Wire, 1->Wire1, 2->Wire2 (if available)
    int8_t sda;      // PortIO::PortId for SDA
    int8_t scl;      // PortIO::PortId for SCL
    uint32_t freq;
};

struct RtcCfg
{
    uint8_t bus_num; // I2C bus id
    uint8_t addr;    // RTC chip I2C address
};

struct LcdCfg
{
    uint8_t bus_num; // I2C bus id
    uint8_t addr;    // LCD I2C address
};

struct SpiCfg
{
    uint8_t bus_num; // 0->SPI, 1->HSPI, 2->VSPI (ESP32 Arduino policy)
    int8_t sck;  // PortIO::PortId for SCK
    int8_t miso; // PortIO::PortId for MISO
    int8_t mosi; // PortIO::PortId for MOSI
    int8_t cs;   // PortIO::PortId for CS, -1 if unused
    uint32_t freq;
};

struct OneWireCfg
{
    enum class OwType : uint8_t
    {
        iButton = 0,
        Temp
    };
    OwType bus_id = OwType::iButton; // logical id
    int8_t pin;     // PortIO::PortId
    bool parasite_power;
};
