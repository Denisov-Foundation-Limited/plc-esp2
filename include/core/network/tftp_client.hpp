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

class IPAddress;
class WiFiUDP;

class TftpClient
{
public:
    using WriteHandler = bool (*)(void *ctx, const uint8_t *data, size_t len);

    bool download(const IPAddress &server, const String &path, WriteHandler writer, void *ctx,
                  uint16_t port = 69, uint16_t timeout_ms = 1500, uint8_t max_retries = 5);

    const String &lastError() const;

private:
    String _last_error;

    bool sendRrq_(WiFiUDP &udp, const IPAddress &server, uint16_t port, const String &file);

    void sendAck_(WiFiUDP &udp, const IPAddress &server, uint16_t port, uint16_t block);

    int readPacket_(WiFiUDP &udp, uint8_t *buf, size_t buf_len, uint16_t timeout_ms);
};
