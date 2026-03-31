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

#include "hal/dht22.hpp"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
static portMUX_TYPE s_dht22_timing_mux = portMUX_INITIALIZER_UNLOCKED;

DHT22::DHT22(uint8_t pin)
    : _pin(pin)
{
}

void DHT22::begin()
{
    if (_pin == 0xFF)
        return;
    pinMode(_pin, INPUT_PULLUP);
}

void DHT22::begin(uint8_t pin)
{
    _pin = pin;
    begin();
}

bool DHT22::read(float &out_temp_c, float &out_humidity)
{
    uint8_t data[5] = {};
    if (!readRaw_(data))
        return false;

    const uint16_t raw_h = (uint16_t)((data[0] << 8) | data[1]);
    const uint16_t raw_t = (uint16_t)(((data[2] & 0x7F) << 8) | data[3]);

    out_humidity = raw_h * 0.1f;
    out_temp_c = raw_t * 0.1f;
    if (data[2] & 0x80)
        out_temp_c = -out_temp_c;

    _err = Error::Ok;
    return true;
}

bool DHT22::readTempC(float &out_temp_c)
{
    float h = 0.0f;
    return read(out_temp_c, h);
}

bool DHT22::readHumidity(float &out_humidity)
{
    float t = 0.0f;
    return read(t, out_humidity);
}

DHT22::Error DHT22::lastError() const
{
    return _err;
}

uint32_t DHT22::expectPulse_(bool level)
{
    uint32_t start = micros();
    while (digitalRead(_pin) == (level ? HIGH : LOW))
    {
        if ((micros() - start) > kTimeoutUs)
            return 0;
    }
    return micros() - start;
}

bool DHT22::readRaw_(uint8_t data[5])
{
    _err = Error::Ok;

    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    delay(2);
    digitalWrite(_pin, HIGH);
    delayMicroseconds(40);
    pinMode(_pin, INPUT_PULLUP);

    bool timing_ok = true;
    // DHT22 bit timing is microsecond-sensitive; avoid preemption during capture.
    portENTER_CRITICAL(&s_dht22_timing_mux);
    if (!expectPulse_(LOW) || !expectPulse_(HIGH))
        timing_ok = false;
    for (uint8_t i = 0; timing_ok && i < 40; ++i)
    {
        const uint32_t low = expectPulse_(LOW);
        const uint32_t high = expectPulse_(HIGH);
        if (!low || !high)
        {
            timing_ok = false;
            break;
        }

        data[i / 8] <<= 1;
        if (high > low)
            data[i / 8] |= 1;
    }
    portEXIT_CRITICAL(&s_dht22_timing_mux);
    if (!timing_ok)
        return fail_(Error::Timeout);

    const uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (sum != data[4])
        return fail_(Error::Checksum);

    return true;
}

bool DHT22::fail_(Error e)
{
    _err = e;
    return false;
}
