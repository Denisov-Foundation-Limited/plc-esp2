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

#if defined(ARDUINO_ARCH_ESP32)
#include <driver/rtc_io.h>
#include <soc/gpio_struct.h>
#include <esp_idf_version.h>
#endif

class OneWireBus
{
public:
    OneWireBus() = default;

    void begin(uint8_t pin)
    {
        _pin = pin;
        _inited = true;
#if defined(ARDUINO_ARCH_ESP32)
        pinMode(_pin, INPUT);
#endif
        release_();
        reset_search();
    }

    uint8_t reset()
    {
        if (!_inited)
            return 0;

        uint8_t presence = 0;
        release_();
        for (uint8_t retries = 125; retries > 0; --retries)
        {
            if (readPin_() == HIGH)
                break;
            delayMicroseconds(2);
            if (retries == 1)
                return 0;
        }
        critEnter_();
        driveLow_();
        delayMicroseconds(kResetLowUs);
        release_();
        delayMicroseconds(kResetReleaseUs);
        presence = (readPin_() == LOW) ? 1 : 0;
        critExit_();
        delayMicroseconds(kResetRecoverUs);
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
    static constexpr uint16_t kResetLowUs = 480;
    static constexpr uint16_t kResetReleaseUs = 70;
    static constexpr uint16_t kResetRecoverUs = 410;
    static constexpr uint8_t kWrite1LowUs = 10;
    static constexpr uint8_t kWrite1SlotUs = 55;
    static constexpr uint8_t kWrite0LowUs = 65;
    static constexpr uint8_t kWrite0SlotUs = 5;
    static constexpr uint8_t kReadLowUs = 3;
    static constexpr uint8_t kReadSampleUs = 10;
    static constexpr uint8_t kReadSlotUs = 53;

    void driveLow_()
    {
#if defined(ARDUINO_ARCH_ESP32)
        directWriteLow_(_pin);
        directModeOutput_(_pin);
#else
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
#endif
    }

    void release_()
    {
#if defined(ARDUINO_ARCH_ESP32)
        directModeInput_(_pin);
#else
        pinMode(_pin, INPUT);
#endif
    }

    void driveHigh_()
    {
#if defined(ARDUINO_ARCH_ESP32)
        directWriteHigh_(_pin);
        directModeOutput_(_pin);
#else
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, HIGH);
#endif
    }

    uint8_t readPin_() const
    {
#if defined(ARDUINO_ARCH_ESP32)
        return directRead_(_pin);
#else
        return digitalRead(_pin);
#endif
    }

#if defined(ARDUINO_ARCH_ESP32)
    static inline uint8_t directRead_(uint8_t pin)
    {
#if CONFIG_IDF_TARGET_ESP32C3
        return (GPIO.in.val >> pin) & 0x1;
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
        if (pin < 32)
            return (GPIO.in >> pin) & 0x1;
        if (pin < 54)
            return (GPIO.in1.val >> (pin - 32)) & 0x1;
#else
        if (pin < 32)
            return (GPIO.in >> pin) & 0x1;
        if (pin < 46)
            return (GPIO.in1.val >> (pin - 32)) & 0x1;
#endif
        return 0;
    }

    static inline void directWriteLow_(uint8_t pin)
    {
#if CONFIG_IDF_TARGET_ESP32C3
        GPIO.out_w1tc.val = (1U << pin);
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
        if (pin < 32)
            GPIO.out_w1tc = (1U << pin);
        else if (pin < 54)
            GPIO.out1_w1tc.val = (1U << (pin - 32));
#else
        if (pin < 32)
            GPIO.out_w1tc = (1U << pin);
        else if (pin < 46)
            GPIO.out1_w1tc.val = (1U << (pin - 32));
#endif
    }

    static inline void directWriteHigh_(uint8_t pin)
    {
#if CONFIG_IDF_TARGET_ESP32C3
        GPIO.out_w1ts.val = (1U << pin);
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
        if (pin < 32)
            GPIO.out_w1ts = (1U << pin);
        else if (pin < 54)
            GPIO.out1_w1ts.val = (1U << (pin - 32));
#else
        if (pin < 32)
            GPIO.out_w1ts = (1U << pin);
        else if (pin < 46)
            GPIO.out1_w1ts.val = (1U << (pin - 32));
#endif
    }

    static inline void directModeInput_(uint8_t pin)
    {
#if CONFIG_IDF_TARGET_ESP32C3
        GPIO.enable_w1tc.val = (1U << pin);
#else
        if (digitalPinIsValid(pin))
        {
#if ESP_IDF_VERSION_MAJOR < 4
            uint32_t rtc_reg(rtc_gpio_desc[pin].reg);
            if (rtc_reg)
            {
                ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].mux);
                ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].pullup | rtc_gpio_desc[pin].pulldown);
            }
#endif
            if (pin < 32)
                GPIO.enable_w1tc = (1U << pin);
            else
                GPIO.enable1_w1tc.val = (1U << (pin - 32));
        }
#endif
    }

    static inline void directModeOutput_(uint8_t pin)
    {
#if CONFIG_IDF_TARGET_ESP32C3
        GPIO.enable_w1ts.val = (1U << pin);
#else
        if (digitalPinIsValid(pin))
        {
#if ESP_IDF_VERSION_MAJOR < 4
            uint32_t rtc_reg(rtc_gpio_desc[pin].reg);
            if (rtc_reg)
            {
                ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].mux);
                ESP_REG(rtc_reg) = ESP_REG(rtc_reg) & ~(rtc_gpio_desc[pin].pullup | rtc_gpio_desc[pin].pulldown);
            }
#endif
            if (pin < 32)
                GPIO.enable_w1ts = (1U << pin);
            else
                GPIO.enable1_w1ts.val = (1U << (pin - 32));
        }
#endif
    }
#endif

    static inline void critEnter_()
    {
#if defined(ARDUINO_ARCH_ESP32)
        portENTER_CRITICAL(&mux_);
#else
        noInterrupts();
#endif
    }

    static inline void critExit_()
    {
#if defined(ARDUINO_ARCH_ESP32)
        portEXIT_CRITICAL(&mux_);
#else
        interrupts();
#endif
    }

    void writeBit_(uint8_t v)
    {
        critEnter_();
        if (v)
        {
            driveLow_();
            delayMicroseconds(kWrite1LowUs);
            driveHigh_();
            delayMicroseconds(kWrite1SlotUs);
        }
        else
        {
            driveLow_();
            delayMicroseconds(kWrite0LowUs);
            driveHigh_();
            delayMicroseconds(kWrite0SlotUs);
        }
        critExit_();
    }

    uint8_t readBit_()
    {
        uint8_t r = 0;
        critEnter_();
        driveLow_();
        delayMicroseconds(kReadLowUs);
        release_();
        delayMicroseconds(kReadSampleUs);
        r = (readPin_() == HIGH) ? 1 : 0;
        critExit_();
        delayMicroseconds(kReadSlotUs);
        return r;
    }

    uint8_t _pin = 0;
    bool _inited = false;
    uint8_t _rom_no[8] = {};
    uint8_t _last_discrepancy = 0;
    uint8_t _last_family_discrepancy = 0;
    bool _last_device_flag = false;

#if defined(ARDUINO_ARCH_ESP32)
    static inline portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
#endif
};
