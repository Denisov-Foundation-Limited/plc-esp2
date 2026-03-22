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

class StackRs485Protocol
{
public:
    static constexpr uint8_t kPreamble0 = 0x53; // 'S'
    static constexpr uint8_t kPreamble1 = 0x52; // 'R'
    static constexpr uint8_t kVersion = 1;
    static constexpr size_t kHeaderSize = 8;

    struct FrameView
    {
        uint8_t flags = 0;
        uint8_t msg_type = 0;
        const uint8_t *payload = nullptr;
        size_t payload_size = 0;
    };

    static size_t encodedSize(size_t payload_size);
    static bool encode(uint8_t flags, uint8_t msg_type, const uint8_t *payload, size_t payload_size,
                       uint8_t *out, size_t cap, size_t &used);
    static bool decode(const uint8_t *data, size_t size, FrameView &out);

private:
    static uint16_t crc16_(const uint8_t *data, size_t size);
};
