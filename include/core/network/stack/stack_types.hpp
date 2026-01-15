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
#include <stdint.h>
#include <vector>

struct StackHello
{
    uint32_t node_id = 0;
    uint16_t proto_ver = 1;
    uint16_t fw_ver = 0;
    uint32_t caps = 0;
    String name;

    static void encode(const StackHello &v, std::vector<uint8_t> &out)
    {
        out.clear();
        out.reserve(13 + v.name.length());
        writeU32_(out, v.node_id);
        writeU16_(out, v.proto_ver);
        writeU16_(out, v.fw_ver);
        writeU32_(out, v.caps);
        const uint8_t name_len = (uint8_t)min<size_t>(v.name.length(), 32);
        out.push_back(name_len);
        for (uint8_t i = 0; i < name_len; ++i)
            out.push_back((uint8_t)v.name[i]);
    }

    static bool decode(const uint8_t *data, size_t len, StackHello &out)
    {
        if (!data || len < 12)
            return false;
        out.node_id = readU32_(data + 0);
        out.proto_ver = readU16_(data + 4);
        out.fw_ver = readU16_(data + 6);
        out.caps = readU32_(data + 8);
        out.name = "";
        if (len >= 13)
        {
            const uint8_t name_len = data[12];
            if (name_len > 0 && (13 + name_len) <= len)
                out.name = String((const char *)(data + 13), name_len);
        }
        return true;
    }

private:
    static uint16_t readU16_(const uint8_t *p)
    {
        return (uint16_t)p[0] | (uint16_t)p[1] << 8;
    }

    static uint32_t readU32_(const uint8_t *p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    static void writeU16_(std::vector<uint8_t> &out, uint16_t v)
    {
        out.push_back((uint8_t)(v & 0xFF));
        out.push_back((uint8_t)((v >> 8) & 0xFF));
    }

    static void writeU32_(std::vector<uint8_t> &out, uint32_t v)
    {
        out.push_back((uint8_t)(v & 0xFF));
        out.push_back((uint8_t)((v >> 8) & 0xFF));
        out.push_back((uint8_t)((v >> 16) & 0xFF));
        out.push_back((uint8_t)((v >> 24) & 0xFF));
    }
};

struct StackAck
{
    uint16_t cmd_id = 0;
    uint8_t code = 0;
};

struct StackErr
{
    uint16_t cmd_id = 0;
    uint8_t code = 1;
};

struct StackStatus
{
    uint32_t uptime_ms = 0;

    static void encode(const StackStatus &v, std::vector<uint8_t> &out)
    {
        out.clear();
        out.reserve(4);
        writeU32_(out, v.uptime_ms);
    }

    static bool decode(const uint8_t *data, size_t len, StackStatus &out)
    {
        if (!data || len < 4)
            return false;
        out.uptime_ms = readU32_(data);
        return true;
    }

private:
    static uint32_t readU32_(const uint8_t *p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    static void writeU32_(std::vector<uint8_t> &out, uint32_t v)
    {
        out.push_back((uint8_t)(v & 0xFF));
        out.push_back((uint8_t)((v >> 8) & 0xFF));
        out.push_back((uint8_t)((v >> 16) & 0xFF));
        out.push_back((uint8_t)((v >> 24) & 0xFF));
    }
};
