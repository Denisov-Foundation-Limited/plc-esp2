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

#include "hal/pn532.hpp"

#include <cstring>
#include <Wire.h>

bool PN532::begin(TwoWire &wire, int sda, int scl, uint32_t freq, uint8_t addr7, int irqPin, int rstPin)
{
    _wire = &wire;
    _addr = addr7;
    _irqPin = irqPin;
    _rstPin = rstPin;

    _wire->begin(sda, scl, freq);

    if (_irqPin >= 0)
        pinMode(_irqPin, INPUT_PULLUP);

    if (_rstPin >= 0)
    {
        pinMode(_rstPin, OUTPUT);
        digitalWrite(_rstPin, LOW);
        delay(10);
        digitalWrite(_rstPin, HIGH);
        delay(50);
    }

    uint32_t fw = 0;
    if (getFirmwareVersion(fw) != Status::Ok)
        return false;
    if (SAMConfig() != Status::Ok)
        return false;
    return true;
}

PN532::Status PN532::getFirmwareVersion(uint32_t &out)
{
    const uint8_t cmd[] = {CMD_GETFIRMWAREVERSION};
    Status st = sendCommand(cmd, sizeof(cmd), 100);
    if (st != Status::Ok)
        return st;

    uint8_t resp[16];
    size_t len = 0;
    st = readResponse(resp, sizeof(resp), len, 100);
    if (st != Status::Ok)
        return st;

    if (len < 6 || resp[0] != PN532_TO_HOST || resp[1] != (CMD_GETFIRMWAREVERSION + 1))
        return Status::BadResponse;

    out = (uint32_t(resp[2]) << 24) | (uint32_t(resp[3]) << 16) | (uint32_t(resp[4]) << 8) | uint32_t(resp[5]);
    return Status::Ok;
}

PN532::Status PN532::readPassiveTargetID(UID &out, uint16_t timeoutMs)
{
    const uint8_t cmd[] = {CMD_INLISTPASSIVETARGET, 0x01, 0x00};

    Status st = sendCommand(cmd, sizeof(cmd), 100);
    if (st != Status::Ok)
        return st;

    uint8_t resp[32];
    size_t len = 0;
    st = readResponse(resp, sizeof(resp), len, timeoutMs);
    if (st != Status::Ok)
        return st;

    if (len < 8 || resp[0] != PN532_TO_HOST || resp[1] != (CMD_INLISTPASSIVETARGET + 1))
        return Status::BadResponse;

    uint8_t uidLen = resp[7];
    if (uidLen == 0 || uidLen > sizeof(out.bytes))
        return Status::BadFrame;

    out.len = uidLen;
    memcpy(out.bytes, &resp[8], uidLen);
    return Status::Ok;
}

bool PN532::readUIDString(String &out, uint16_t timeoutMs)
{
    UID uid;
    if (readPassiveTargetID(uid, timeoutMs) != Status::Ok)
        return false;
    out = uidToString(uid);
    return true;
}

String PN532::uidToString(const UID &u)
{
    String s;
    for (uint8_t i = 0; i < u.len; i++)
    {
        if (u.bytes[i] < 0x10)
            s += '0';
        s += String(u.bytes[i], HEX);
        if (i + 1 < u.len)
            s += ':';
    }
    s.toUpperCase();
    return s;
}

PN532::Status PN532::SAMConfig()
{
    const uint8_t cmd[] = {CMD_SAMCONFIGURATION, 0x01, 0x14, 0x01};

    Status st = sendCommand(cmd, sizeof(cmd), 100);
    if (st != Status::Ok)
        return st;

    uint8_t resp[8];
    size_t len = 0;
    st = readResponse(resp, sizeof(resp), len, 100);
    if (st != Status::Ok)
        return st;

    if (len < 2 || resp[0] != PN532_TO_HOST || resp[1] != (CMD_SAMCONFIGURATION + 1))
        return Status::BadResponse;
    return Status::Ok;
}

bool PN532::waitReady(uint32_t timeoutMs)
{
    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs)
    {
        if (_irqPin >= 0)
        {
            if (digitalRead(_irqPin) == LOW)
                return true;
        }
        else
        {
            if (_wire->requestFrom((int)_addr, 1) == 1)
            {
                if (_wire->read() == 0x01)
                    return true;
            }
        }
        delay(2);
    }
    return false;
}

bool PN532::i2cWrite(const uint8_t *data, size_t len)
{
    _wire->beginTransmission(_addr);
    _wire->write((uint8_t)0x00);
    _wire->write(data, len);
    return (_wire->endTransmission() == 0);
}

bool PN532::i2cRead(uint8_t *data, size_t len)
{
    if (_wire->requestFrom((int)_addr, (int)(len + 1)) != (int)(len + 1))
        return false;
    _wire->read();
    for (size_t i = 0; i < len; i++)
        data[i] = _wire->read();
    return true;
}

PN532::Status PN532::sendCommand(const uint8_t *cmd, size_t cmdLen, uint32_t timeoutMs)
{
    uint8_t frame[64];
    if (cmdLen + 8 > sizeof(frame))
        return Status::BadFrame;

    uint8_t len = cmdLen + 1;
    uint8_t sum = HOST_TO_PN;
    size_t i = 0;

    frame[i++] = PREAMBLE;
    frame[i++] = START1;
    frame[i++] = START2;
    frame[i++] = len;
    frame[i++] = uint8_t(~len + 1);
    frame[i++] = HOST_TO_PN;

    for (size_t k = 0; k < cmdLen; k++)
    {
        frame[i++] = cmd[k];
        sum += cmd[k];
    }

    frame[i++] = uint8_t(~sum + 1);
    frame[i++] = POSTAMBLE;

    if (!i2cWrite(frame, i))
        return Status::I2cError;
    if (!waitReady(timeoutMs))
        return Status::Timeout;

    uint8_t ack[6];
    static constexpr uint8_t ACK[6] = {0, 0, 0xFF, 0, 0xFF, 0};
    if (!i2cRead(ack, sizeof(ack)))
        return Status::I2cError;
    if (memcmp(ack, ACK, sizeof(ACK)) != 0)
        return Status::BadAck;

    return Status::Ok;
}

PN532::Status PN532::readResponse(uint8_t *out, size_t cap, size_t &outLen, uint32_t timeoutMs)
{
    outLen = 0;
    if (!waitReady(timeoutMs))
        return Status::Timeout;

    uint8_t hdr[6];
    if (!i2cRead(hdr, sizeof(hdr)))
        return Status::I2cError;
    if (hdr[0] != 0 || hdr[1] != 0 || hdr[2] != 0xFF)
        return Status::BadFrame;

    uint8_t len = hdr[3];
    if (uint8_t(len + hdr[4]) != 0)
        return Status::BadFrame;

    uint8_t buf[64];
    size_t need = len + 2;
    if (need > sizeof(buf))
        return Status::BadFrame;
    if (!i2cRead(buf, need))
        return Status::I2cError;

    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++)
        sum += buf[i];
    if (uint8_t(sum + buf[len]) != 0 || buf[len + 1] != 0)
        return Status::BadFrame;

    if (len > cap)
        return Status::BadFrame;
    memcpy(out, buf, len);
    outLen = len;
    return Status::Ok;
}
