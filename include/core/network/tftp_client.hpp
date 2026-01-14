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
#include <WiFiUdp.h>

class TftpClient
{
public:
    using WriteHandler = bool (*)(void *ctx, const uint8_t *data, size_t len);

    bool download(const IPAddress &server, const String &path, WriteHandler writer, void *ctx,
                  uint16_t port = 69, uint16_t timeout_ms = 1500, uint8_t max_retries = 5)
    {
        _last_error = "";
        String file = path;
        if (file.startsWith("/"))
            file.remove(0, 1);
        if (file.length() == 0)
        {
            _last_error = F("empty filename");
            return false;
        }

        WiFiUDP udp;
        if (!udp.begin(0))
        {
            _last_error = F("udp begin failed");
            return false;
        }

        if (!sendRrq_(udp, server, port, file))
            return false;

        uint16_t expected = 1;
        uint16_t server_port = port;
        bool server_port_set = false;
        uint8_t retries = 0;
        uint8_t buf[516];

        while (true)
        {
            int packet_len = readPacket_(udp, buf, sizeof(buf), timeout_ms);
            if (packet_len <= 0)
            {
                if (retries++ >= max_retries)
                {
                    _last_error = F("tftp timeout");
                    udp.stop();
                    return false;
                }
                if (expected == 1)
                {
                    if (!sendRrq_(udp, server, port, file))
                        return false;
                }
                else
                {
                    sendAck_(udp, server, server_port, (uint16_t)(expected - 1));
                }
                continue;
            }

            if (udp.remoteIP() != server)
                continue;

            const uint16_t opcode = (uint16_t)((buf[0] << 8) | buf[1]);
            if (opcode == 3)
            {
                const uint16_t block = (uint16_t)((buf[2] << 8) | buf[3]);
                if (!server_port_set)
                {
                    server_port = udp.remotePort();
                    server_port_set = true;
                }
                if (block == expected)
                {
                    const int data_len = packet_len - 4;
                    if (writer && data_len > 0 && !writer(ctx, buf + 4, (size_t)data_len))
                    {
                        _last_error = F("write failed");
                        udp.stop();
                        return false;
                    }
                    sendAck_(udp, server, server_port, block);
                    expected++;
                    retries = 0;
                    if (data_len < 512)
                    {
                        udp.stop();
                        return true;
                    }
                }
                else if (block < expected)
                {
                    sendAck_(udp, server, server_port, block);
                }
            }
            else if (opcode == 5)
            {
                String msg;
                for (int i = 4; i < packet_len; ++i)
                {
                    if (buf[i] == 0)
                        break;
                    msg += (char)buf[i];
                }
                _last_error = msg.length() ? msg : F("tftp error");
                udp.stop();
                return false;
            }
            else if (opcode == 6)
            {
                sendAck_(udp, server, server_port, 0);
                retries = 0;
            }
        }
    }

    const String &lastError() const { return _last_error; }

private:
    String _last_error;

    bool sendRrq_(WiFiUDP &udp, const IPAddress &server, uint16_t port, const String &file)
    {
        const char *mode = "octet";
        const size_t file_len = (size_t)file.length();
        const size_t mode_len = strlen(mode);
        const size_t size = 2 + file_len + 1 + mode_len + 1;
        uint8_t pkt[2 + 255 + 1 + 6 + 1] = {};
        if (size > sizeof(pkt))
        {
            _last_error = F("filename too long");
            return false;
        }
        pkt[0] = 0;
        pkt[1] = 1;
        memcpy(pkt + 2, file.c_str(), file_len);
        pkt[2 + file_len] = 0;
        memcpy(pkt + 2 + file_len + 1, mode, mode_len);
        pkt[2 + file_len + 1 + mode_len] = 0;

        if (!udp.beginPacket(server, port))
        {
            _last_error = F("udp beginPacket failed");
            return false;
        }
        udp.write(pkt, size);
        if (!udp.endPacket())
        {
            _last_error = F("udp endPacket failed");
            return false;
        }
        return true;
    }

    void sendAck_(WiFiUDP &udp, const IPAddress &server, uint16_t port, uint16_t block)
    {
        uint8_t pkt[4];
        pkt[0] = 0;
        pkt[1] = 4;
        pkt[2] = (uint8_t)(block >> 8);
        pkt[3] = (uint8_t)(block & 0xFF);
        udp.beginPacket(server, port);
        udp.write(pkt, sizeof(pkt));
        udp.endPacket();
    }

    int readPacket_(WiFiUDP &udp, uint8_t *buf, size_t buf_len, uint16_t timeout_ms)
    {
        const uint32_t start = millis();
        while ((uint32_t)(millis() - start) < timeout_ms)
        {
            const int size = udp.parsePacket();
            if (size > 0)
            {
                const int to_read = size > (int)buf_len ? (int)buf_len : size;
                return udp.read(buf, to_read);
            }
            delay(1);
        }
        return 0;
    }
};
