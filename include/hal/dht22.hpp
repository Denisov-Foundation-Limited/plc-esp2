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

#include <stdint.h>

class DHT22
{
public:
    enum class Error : uint8_t
    {
        Ok = 0,
        Timeout,
        Checksum
    };

    DHT22() = default;
    explicit DHT22(uint8_t pin);

    void begin();
    void begin(uint8_t pin);
    bool read(float &out_temp_c, float &out_humidity);
    bool readTempC(float &out_temp_c);
    bool readHumidity(float &out_humidity);
    Error lastError() const;

private:
    // 100us is too tight under RTOS jitter; keep some margin for stable reads.
    static constexpr uint32_t kTimeoutUs = 180;

    uint32_t expectPulse_(bool level);
    bool readRaw_(uint8_t data[5]);
    bool fail_(Error e);

    uint8_t _pin;
    Error _err = Error::Ok;
};
