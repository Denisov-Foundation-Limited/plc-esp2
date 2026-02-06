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

enum class StackUnit : uint8_t
{
    Cpu = 0,
    Unit1,
    Unit2,
    Unit3,
    Unit4,
    Unit5,
    Unit6,
    Unit7,
    Unit8,
    Unit9,
    Unit10,
    Unknown
};

enum StackCaps : uint32_t
{
    StackCapController = 1u << 0
};

static inline bool stackCapsHas(uint32_t caps, uint32_t flag)
{
    return (caps & flag) != 0;
}

struct StackHello
{
    uint32_t node_id = 0;
    uint16_t proto_ver = 1;
    uint16_t fw_ver = 0;
    uint32_t caps = 0;
    String name;

    static size_t encode(const StackHello &v, uint8_t *out, size_t cap)
    {
        if (!out || cap < 13)
            return 0;
        writeU32_(out, 0, v.node_id);
        writeU16_(out, 4, v.proto_ver);
        writeU16_(out, 6, v.fw_ver);
        writeU32_(out, 8, v.caps);
        const uint8_t name_len = (uint8_t)min<size_t>(v.name.length(), 32);
        if ((size_t)(13 + name_len) > cap)
            return 0;
        out[12] = name_len;
        for (uint8_t i = 0; i < name_len; ++i)
            out[13 + i] = (uint8_t)v.name[i];
        return (size_t)(13 + name_len);
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

    static void writeU16_(uint8_t *out, size_t offset, uint16_t v)
    {
        out[offset] = (uint8_t)(v & 0xFF);
        out[offset + 1] = (uint8_t)((v >> 8) & 0xFF);
    }

    static void writeU32_(uint8_t *out, size_t offset, uint32_t v)
    {
        out[offset] = (uint8_t)(v & 0xFF);
        out[offset + 1] = (uint8_t)((v >> 8) & 0xFF);
        out[offset + 2] = (uint8_t)((v >> 16) & 0xFF);
        out[offset + 3] = (uint8_t)((v >> 24) & 0xFF);
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

    static size_t encode(const StackStatus &v, uint8_t *out, size_t cap)
    {
        if (!out || cap < 4)
            return 0;
        writeU32_(out, 0, v.uptime_ms);
        return 4;
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

    static void writeU32_(uint8_t *out, size_t offset, uint32_t v)
    {
        out[offset] = (uint8_t)(v & 0xFF);
        out[offset + 1] = (uint8_t)((v >> 8) & 0xFF);
        out[offset + 2] = (uint8_t)((v >> 16) & 0xFF);
        out[offset + 3] = (uint8_t)((v >> 24) & 0xFF);
    }
};
