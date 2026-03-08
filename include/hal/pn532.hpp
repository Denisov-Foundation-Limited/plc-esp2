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

class TwoWire;

class PN532
{
public:
    struct UID
    {
        uint8_t len = 0;
        uint8_t bytes[10]{};
    };

    enum class Status : uint8_t
    {
        Ok,
        I2cError,
        Timeout,
        BadAck,
        BadFrame,
        BadResponse
    };

    PN532() = default;

    bool begin(TwoWire &wire, int sda, int scl, uint32_t freq = 400000, uint8_t addr7 = 0x24, int irqPin = -1, int rstPin = -1);
    Status getFirmwareVersion(uint32_t &out);
    Status readPassiveTargetID(UID &out, uint16_t timeoutMs = 1000);
    bool readUIDString(String &out, uint16_t timeoutMs = 1000);
    static String uidToString(const UID &u);

private:
    static constexpr uint8_t PREAMBLE = 0x00;
    static constexpr uint8_t START1 = 0x00;
    static constexpr uint8_t START2 = 0xFF;
    static constexpr uint8_t POSTAMBLE = 0x00;
    static constexpr uint8_t HOST_TO_PN = 0xD4;
    static constexpr uint8_t PN532_TO_HOST = 0xD5;
    static constexpr uint8_t CMD_GETFIRMWAREVERSION = 0x02;
    static constexpr uint8_t CMD_SAMCONFIGURATION = 0x14;
    static constexpr uint8_t CMD_INLISTPASSIVETARGET = 0x4A;

    TwoWire *_wire = nullptr;
    uint8_t _addr = 0x24;
    int _irqPin = -1;
    int _rstPin = -1;

    Status SAMConfig();
    bool waitReady(uint32_t timeoutMs);
    bool i2cWrite(const uint8_t *data, size_t len);
    bool i2cRead(uint8_t *data, size_t len);
    Status sendCommand(const uint8_t *cmd, size_t cmdLen, uint32_t timeoutMs);
    Status readResponse(uint8_t *out, size_t cap, size_t &outLen, uint32_t timeoutMs);
};
