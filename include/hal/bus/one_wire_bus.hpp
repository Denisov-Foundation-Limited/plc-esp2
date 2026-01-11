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

class OneWireBus
{
public:
    OneWireBus() = default;

    void begin(uint8_t pin)
    {
        _pin = pin;
        _inited = true;
        release_();
        reset_search();
    }

    uint8_t reset()
    {
        if (!_inited)
            return 0;

        uint8_t presence = 0;
        noInterrupts();
        driveLow_();
        delayMicroseconds(480);
        release_();
        delayMicroseconds(70);
        presence = (digitalRead(_pin) == LOW) ? 1 : 0;
        interrupts();
        delayMicroseconds(410);
        return presence;
    }

    void write(uint8_t v)
    {
        for (uint8_t i = 0; i < 8; ++i)
        {
            writeBit_(v & 0x01);
            v >>= 1;
        }

        release_();
    }

    uint8_t read()
    {
        uint8_t v = 0;
        for (uint8_t i = 0; i < 8; ++i)
            v |= (readBit_() << i);
        return v;
    }

    void select(const uint8_t rom[8])
    {
        write(0x55);
        for (uint8_t i = 0; i < 8; ++i)
            write(rom[i]);
    }

    void skip()
    {
        write(0xCC);
    }

    void reset_search()
    {
        _last_discrepancy = 0;
        _last_device_flag = false;
        _last_family_discrepancy = 0;
    }

    uint8_t search(uint8_t *new_addr)
    {
        uint8_t id_bit_number = 1;
        uint8_t last_zero = 0;
        uint8_t rom_byte_number = 0;
        uint8_t rom_byte_mask = 1;
        uint8_t search_result = 0;
        uint8_t id_bit = 0;
        uint8_t cmp_id_bit = 0;

        if (_last_device_flag)
            return 0;

        if (!reset())
        {
            reset_search();
            return 0;
        }

        write(0xF0);

        do
        {
            id_bit = readBit_();
            cmp_id_bit = readBit_();

            if ((id_bit == 1) && (cmp_id_bit == 1))
                break;

            uint8_t search_direction = 0;
            if (id_bit != cmp_id_bit)
            {
                search_direction = id_bit;
            }
            else
            {
                if (id_bit_number < _last_discrepancy)
                    search_direction = ((_rom_no[rom_byte_number] & rom_byte_mask) > 0);
                else
                    search_direction = (id_bit_number == _last_discrepancy);

                if (search_direction == 0)
                {
                    last_zero = id_bit_number;
                    if (last_zero < 9)
                        _last_family_discrepancy = last_zero;
                }
            }

            if (search_direction == 1)
                _rom_no[rom_byte_number] |= rom_byte_mask;
            else
                _rom_no[rom_byte_number] &= ~rom_byte_mask;

            writeBit_(search_direction);

            ++id_bit_number;
            rom_byte_mask <<= 1;

            if (rom_byte_mask == 0)
            {
                ++rom_byte_number;
                rom_byte_mask = 1;
            }
        } while (rom_byte_number < 8);

        if (!(id_bit_number < 65))
        {
            _last_discrepancy = last_zero;
            if (_last_discrepancy == 0)
                _last_device_flag = true;
            search_result = 1;
        }

        if (!search_result || (_rom_no[0] == 0))
        {
            reset_search();
            return 0;
        }

        for (uint8_t i = 0; i < 8; ++i)
            new_addr[i] = _rom_no[i];

        return search_result;
    }

    static uint8_t crc8(const uint8_t *addr, uint8_t len)
    {
        uint8_t crc = 0;
        while (len--)
        {
            uint8_t inbyte = *addr++;
            for (uint8_t i = 0; i < 8; ++i)
            {
                uint8_t mix = (crc ^ inbyte) & 0x01;
                crc >>= 1;
                if (mix)
                    crc ^= 0x8C;
                inbyte >>= 1;
            }
        }
        return crc;
    }

private:
    void driveLow_()
    {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
    }

    void release_()
    {
        pinMode(_pin, INPUT);
    }

    void writeBit_(uint8_t v)
    {
        noInterrupts();
        if (v)
        {
            driveLow_();
            delayMicroseconds(6);
            release_();
            delayMicroseconds(64);
        }
        else
        {
            driveLow_();
            delayMicroseconds(60);
            release_();
            delayMicroseconds(10);
        }
        interrupts();
    }

    uint8_t readBit_()
    {
        uint8_t r = 0;
        noInterrupts();
        driveLow_();
        delayMicroseconds(6);
        release_();
        delayMicroseconds(9);
        r = (digitalRead(_pin) == HIGH) ? 1 : 0;
        interrupts();
        delayMicroseconds(55);
        return r;
    }

    uint8_t _pin = 0;
    bool _inited = false;
    uint8_t _rom_no[8] = {};
    uint8_t _last_discrepancy = 0;
    uint8_t _last_family_discrepancy = 0;
    bool _last_device_flag = false;
};
