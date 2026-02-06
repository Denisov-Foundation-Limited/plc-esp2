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

#include "boards/board_profile.hpp"
#include "hal/gpio/portio.hpp"

class IoStack
{
public:
    static constexpr uint8_t PORT_COUNT = PortIO::PORT_COUNT;

    explicit IoStack(PortIO &portio) : _portio(portio)
    {
        for (uint8_t i = 0; i < PORT_COUNT; ++i)
        {
            _inputs[i] = false;
            _outputs[i] = false;
            _applied[i] = false;
            _dirty[i] = false;
            _raw_inputs[i] = false;
            _last_change_ms[i] = 0;
        }
    }

    bool begin() { return _portio.begin(); }

    void loop() { _portio.loop(); }

    void pinMode(uint8_t id, PortIO::PortMode mode)
    {
        _portio.pinMode(id, mode);
    }

    void initImages()
    {
        for (uint8_t i = 0; i < PORT_COUNT; ++i)
        {
            const auto &p = _portio.desc(i);
            if (p.caps == Cap::None)
                continue;
            if (has(p.caps, Cap::Input))
            {
                const bool v = _portio.read(i);
                _inputs[i] = v;
                _raw_inputs[i] = v;
                _last_change_ms[i] = 0;
            }
            if (has(p.caps, Cap::Output) && !has(p.caps, Cap::InputOnly))
            {
                bool v = false;
                if (_portio.lastState(i, v))
                    _outputs[i] = v;
                _applied[i] = _outputs[i];
                _dirty[i] = false;
            }
        }
    }

    void scanInputs()
    {
        const uint32_t now = millis_();
        for (uint8_t i = 0; i < PORT_COUNT; ++i)
        {
            const auto &p = _portio.desc(i);
            if (p.caps == Cap::None)
                continue;
            if (has(p.caps, Cap::Input))
            {
                const bool raw = _portio.read(i);
                if (raw != _raw_inputs[i])
                {
                    _raw_inputs[i] = raw;
                    _last_change_ms[i] = now;
                }
                if (_last_change_ms[i] == 0)
                    _last_change_ms[i] = now;
                if ((uint32_t)(now - _last_change_ms[i]) >= kDebounceMs)
                    _inputs[i] = _raw_inputs[i];
            }
        }
    }

    void applyOutputs()
    {
        for (uint8_t i = 0; i < PORT_COUNT; ++i)
        {
            const auto &p = _portio.desc(i);
            if (p.caps == Cap::None)
                continue;
            if (!has(p.caps, Cap::Output) || has(p.caps, Cap::InputOnly))
                continue;
            const bool v = _outputs[i];
            if (_dirty[i] || _applied[i] != v)
            {
                _portio.write(i, v);
                _applied[i] = v;
                _dirty[i] = false;
            }
        }
        _portio.loop();
    }

    bool write(uint8_t id, bool logicalLevel)
    {
        if (id >= PORT_COUNT)
            return false;
        const auto &p = _portio.desc(id);
        if (!has(p.caps, Cap::Output) || has(p.caps, Cap::InputOnly))
            return false;
        _outputs[id] = logicalLevel;
        _dirty[id] = true;
        return true;
    }

    bool read(uint8_t id) const
    {
        if (id >= PORT_COUNT)
            return false;
        const auto &p = _portio.desc(id);
        if (p.caps == Cap::None)
            return false;
        if (has(p.caps, Cap::Output))
            return _outputs[id];
        if (has(p.caps, Cap::Input))
            return _inputs[id];
        return false;
    }

    const PortIO::PortDesc &desc(uint8_t id) const { return _portio.desc(id); }

    bool lastState(uint8_t id, bool &outLogical) const
    {
        if (id >= PORT_COUNT)
            return false;
        const auto &p = _portio.desc(id);
        if (!has(p.caps, Cap::Output) || has(p.caps, Cap::InputOnly))
            return false;
        outLogical = _outputs[id];
        return true;
    }

private:
    static constexpr uint32_t kDebounceMs = 100;

    static uint32_t millis_()
    {
#if defined(ARDUINO)
        return (uint32_t)::millis();
#else
        return 0;
#endif
    }

    PortIO &_portio;
    bool _inputs[PORT_COUNT] = {};
    bool _outputs[PORT_COUNT] = {};
    bool _applied[PORT_COUNT] = {};
    bool _dirty[PORT_COUNT] = {};
    bool _raw_inputs[PORT_COUNT] = {};
    uint32_t _last_change_ms[PORT_COUNT] = {};
};
