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

#include "hal/io_stack.hpp"
#include <Arduino.h>

IoStack::IoStack(PortIO &portio)
    : _portio(portio)
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

bool IoStack::begin()
{
    auto guard = _lock.guard();
    return _portio.begin();
}

void IoStack::loop()
{
    auto guard = _lock.guard();
    _portio.loop();
}

void IoStack::pinMode(uint8_t id, PortIO::PortMode mode)
{
    auto guard = _lock.guard();
    _portio.pinMode(id, mode);
}

void IoStack::initImages()
{
    auto guard = _lock.guard();
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
            if (!_dirty[i])
            {
                bool v = false;
                if (_portio.lastState(i, v))
                    _outputs[i] = v;
                _applied[i] = _outputs[i];
            }
        }
    }
}

void IoStack::scanInputs()
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
            auto guard = _lock.guard();
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

void IoStack::applyOutputs()
{
    for (uint8_t i = 0; i < PORT_COUNT; ++i)
    {
        const auto &p = _portio.desc(i);
        if (p.caps == Cap::None)
            continue;
        if (!has(p.caps, Cap::Output) || has(p.caps, Cap::InputOnly))
            continue;
        bool should_apply = false;
        bool v = false;
        {
            auto guard = _lock.guard();
            v = _outputs[i];
            should_apply = _dirty[i] || _applied[i] != v;
        }
        if (should_apply)
        {
            _portio.write(i, v);
            auto guard = _lock.guard();
            if (_outputs[i] == v)
            {
                _applied[i] = v;
                _dirty[i] = false;
            }
            else
            {
                _dirty[i] = true;
            }
        }
    }
    _portio.loop();
}

bool IoStack::write(uint8_t id, bool logicalLevel, uint32_t timeout_ms)
{
    auto guard = _lock.guard(timeout_ms);
    if (!guard.locked())
        return false;
    if (id >= PORT_COUNT)
        return false;
    const auto &p = _portio.desc(id);
    if (!has(p.caps, Cap::Output) || has(p.caps, Cap::InputOnly))
        return false;
    _outputs[id] = logicalLevel;
    _dirty[id] = true;
    return true;
}

bool IoStack::read(uint8_t id, uint32_t timeout_ms) const
{
    auto guard = _lock.guard(timeout_ms);
    if (!guard.locked())
        return false;
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

const PortIO::PortDesc &IoStack::desc(uint8_t id) const
{
    return _portio.desc(id);
}

bool IoStack::lastState(uint8_t id, bool &outLogical) const
{
    auto guard = _lock.guard();
    if (id >= PORT_COUNT)
        return false;
    const auto &p = _portio.desc(id);
    if (!has(p.caps, Cap::Output) || has(p.caps, Cap::InputOnly))
        return false;
    outLogical = _outputs[id];
    return true;
}

uint32_t IoStack::millis_()
{
#if defined(ARDUINO)
    return (uint32_t)::millis();
#else
    return 0;
#endif
}
