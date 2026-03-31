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

#include "hal/gpio/portio.hpp"

#include <Arduino.h>

#if defined(ESP32)
#include "driver/gpio.h"
#endif

bool PortIO::begin()
{
    auto guard = _lock.guard();
    _err = Error::Ok;
    _outputs_enabled = false;
    for (uint8_t i = 0; i < PORT_COUNT; ++i)
    {
        _hasLast[i] = false;
        _last[i] = false;
    }
    for (uint8_t i = 0; i < Extender::MAX_DEVS; ++i)
        _ext_present[i] = false;

    if (usesExtender_() && _ext == nullptr)
    {
        _err = Error::ExtenderMissing;
        return false;
    }

    if (_ext)
    {
        _ext->begin();
        const uint8_t dev_count = _ext->devCount();
        for (uint8_t i = 0; i < dev_count && i < Extender::MAX_DEVS; ++i)
            _ext_present[i] = _ext->isPresent(i);
    }

    for (uint8_t i = 0; i < PORT_COUNT; ++i)
    {
        const auto &p = _ports[i];
        if (p.caps == Cap::None)
            continue;
        if (!validate_(p))
            return false;

        pinMode(i, p.mode);

        if (has(p.caps, Cap::Output) &&
            (p.mode == PortMode::Output || p.mode == PortMode::OutputOpenDrain))
        {
            write(i, p.initial_level);
        }
        else if (p.backend == Backend::Extender)
        {
            write(i, true);
        }

        if (p.pwm_enable)
        {
            if (p.backend != Backend::Esp32)
            {
                _err = Error::PwmNotSupported;
                return false;
            }
            if (!::ledcSetup(p.pwm.channel, p.pwm.freq, p.pwm.resolution))
            {
                _err = Error::PwmNotSupported;
                return false;
            }
            ::ledcAttachPin(p.u.esp.gpio, p.pwm.channel);
            pwmWrite(i, p.pwm.duty_init);
        }
    }

    if (_ext)
        _ext->flushAll();
    return true;
}

void PortIO::loop()
{
    auto guard = _lock.guard();
    if (_ext)
    {
        const uint8_t dev_count = _ext->devCount();
        for (uint8_t i = 0; i < dev_count && i < Extender::MAX_DEVS; ++i)
        {
            const bool present = _ext->isPresent(i);
            if (present && !_ext_present[i])
                restoreExtenderOutputs_(i);
            _ext_present[i] = present;
        }
        _ext->flushAll();
    }
}

void PortIO::setOutputsEnabled(bool enabled)
{
    auto guard = _lock.guard();
    if (_outputs_enabled == enabled)
        return;
    _outputs_enabled = enabled;
    if (_outputs_enabled)
        applyDeferredOutputs_();
}

PortIO::Error PortIO::lastError() const { return _err; }

bool PortIO::lastState(PortId id, bool &outLogical) const
{
    auto guard = _lock.guard();
    if (id >= PORT_COUNT)
        return false;
    if (!_hasLast[id])
        return false;
    outLogical = _last[id];
    return true;
}

const PortIO::PortDesc &PortIO::desc(PortId id) const { return _ports[id]; }

PortIO::PinType PortIO::type(PortId id) const
{
    auto guard = _lock.guard();
    if (id >= PORT_COUNT)
        return PinType::Unknown;
    return _ports[id].type;
}

void PortIO::pinMode(PortId id, PortMode mode)
{
    auto guard = _lock.guard();
    if (id >= PORT_COUNT)
        return;
    const auto &p = _ports[id];
    if (p.caps == Cap::None)
        return;

    if (p.backend == Backend::Esp32)
    {
        if (p.u.esp.gpio == 0xFF)
            return;
        ::pinMode(p.u.esp.gpio, toArduinoMode_(mode));
    }
    else if (_ext)
    {
        _ext->pinMode(p.u.ext.dev, p.u.ext.pin, toArduinoMode_(mode));
    }
}

void PortIO::write(PortId id, bool logicalLevel)
{
    auto guard = _lock.guard();
    if (id >= PORT_COUNT)
        return;
    const auto &p = _ports[id];
    if (p.caps == Cap::None)
        return;

    _last[id] = logicalLevel;
    _hasLast[id] = true;

    if (!_outputs_enabled)
        return;

    bool v = logicalLevel;

    if (p.backend == Backend::Esp32)
    {
        if (p.u.esp.gpio == 0xFF)
            return;
        if (p.u.esp.inverted)
            v = !v;
        ::digitalWrite(p.u.esp.gpio, v ? HIGH : LOW);
    }
    else if (_ext)
    {
        if (p.u.ext.inverted)
            v = !v;
        _ext->write(p.u.ext.dev, p.u.ext.pin, v);
    }
}

bool PortIO::read(PortId id) const
{
    bool out = false;
    (void)read(id, out);
    return out;
}

bool PortIO::read(PortId id, bool &out) const
{
    auto guard = _lock.guard();
    out = false;
    if (id >= PORT_COUNT)
        return false;
    const auto &p = _ports[id];
    if (p.caps == Cap::None)
        return false;

    bool v = false;
    if (p.backend == Backend::Esp32)
    {
        if (p.u.esp.gpio == 0xFF)
            return false;
        v = (::digitalRead(p.u.esp.gpio) != 0);
        if (p.u.esp.inverted)
            v = !v;
        out = v;
        return true;
    }
    if (_ext)
    {
        if (!_ext->read(p.u.ext.dev, p.u.ext.pin, v))
            return false;
        if (p.u.ext.inverted)
            v = !v;
        out = v;
        return true;
    }
    return false;
}

int PortIO::adcRead(PortId id)
{
    auto guard = _lock.guard();
    if (id >= PORT_COUNT)
        return 0;
    const auto &p = _ports[id];
    if (!has(p.caps, Cap::ADC) || p.backend != Backend::Esp32)
    {
        _err = Error::AdcNotSupported;
        return 0;
    }
    if (p.u.esp.gpio == 0xFF)
        return 0;
    return ::analogRead(p.u.esp.gpio);
}

void PortIO::pwmWrite(PortId id, uint32_t duty)
{
    auto guard = _lock.guard();
    if (id >= PORT_COUNT)
        return;
    const auto &p = _ports[id];
    if (!p.pwm_enable)
        return;

    uint32_t d = duty;
    if (p.u.esp.inverted)
    {
        const uint32_t maxDuty = (1u << p.pwm.resolution) - 1u;
        d = maxDuty - d;
    }
    ::ledcWrite(p.pwm.channel, d);
}

bool PortIO::writeFast(PortId id, bool logicalLevel)
{
    auto guard = _lock.guard();
    if (id >= PORT_COUNT)
        return false;
    const auto &p = _ports[id];
    if (p.backend != Backend::Esp32)
        return false;
    if (!has(p.caps, Cap::ISR_FAST))
        return false;
    if (p.u.esp.gpio == 0xFF)
        return false;

#if defined(ESP32)
    bool v = logicalLevel;
    if (p.u.esp.inverted)
        v = !v;
    gpio_set_level((gpio_num_t)p.u.esp.gpio, v ? 1 : 0);
    return true;
#else
    (void)logicalLevel;
    return false;
#endif
}

bool PortIO::readFast(PortId id, bool &outLogicalLevel) const
{
    auto guard = _lock.guard();
    if (id >= PORT_COUNT)
        return false;
    const auto &p = _ports[id];
    if (p.backend != Backend::Esp32)
        return false;
    if (!has(p.caps, Cap::ISR_FAST))
        return false;
    if (p.u.esp.gpio == 0xFF)
        return false;

#if defined(ESP32)
    int v = gpio_get_level((gpio_num_t)p.u.esp.gpio);
    bool b = (v != 0);
    if (p.u.esp.inverted)
        b = !b;
    outLogicalLevel = b;
    return true;
#else
    (void)outLogicalLevel;
    return false;
#endif
}

bool PortIO::usesExtender_() const
{
    for (uint8_t i = 0; i < PORT_COUNT; ++i)
    {
        if (_ports[i].caps == Cap::None)
            continue;
        if (_ports[i].backend == Backend::Extender)
            return true;
    }
    return false;
}

uint8_t PortIO::toArduinoMode_(PortMode m)
{
    switch (m)
    {
    case PortMode::Input:
        return INPUT;
    case PortMode::InputPullUp:
        return INPUT_PULLUP;
    case PortMode::InputPullDown:
        return INPUT_PULLDOWN;
    case PortMode::Output:
        return OUTPUT;
    case PortMode::OutputOpenDrain:
        return OUTPUT_OPEN_DRAIN;
    }
    return INPUT;
}

bool PortIO::validate_(const PortDesc &p)
{
    if (!has(p.caps, Cap::GPIO))
    {
        _err = Error::ModeNotSupported;
        return false;
    }

    if (p.backend == Backend::Esp32)
    {
        if (p.u.esp.gpio != 0xFF && p.u.esp.gpio > kMaxPin)
        {
            _err = Error::InvalidPin;
            return false;
        }
    }
    else
    {
        if (_ext == nullptr)
        {
            _err = Error::ExtenderMissing;
            return false;
        }
        if (!_ext->isConfigured(p.u.ext.dev))
        {
            _err = Error::ExtenderMissing;
            return false;
        }
    }

    if (has(p.caps, Cap::InputOnly) &&
        (p.mode == PortMode::Output || p.mode == PortMode::OutputOpenDrain))
    {
        _err = Error::OutputOnInputOnly;
        return false;
    }

    switch (p.mode)
    {
    case PortMode::Input:
        if (!has(p.caps, Cap::Input))
        {
            _err = Error::ModeNotSupported;
            return false;
        }
        break;
    case PortMode::InputPullUp:
        if (!has(p.caps, Cap::Input) || !has(p.caps, Cap::PullUp))
        {
            _err = Error::ModeNotSupported;
            return false;
        }
        break;
    case PortMode::InputPullDown:
        if (!has(p.caps, Cap::Input) || !has(p.caps, Cap::PullDown))
        {
            _err = Error::ModeNotSupported;
            return false;
        }
        break;
    case PortMode::Output:
    case PortMode::OutputOpenDrain:
        if (!has(p.caps, Cap::Output))
        {
            _err = Error::ModeNotSupported;
            return false;
        }
        break;
    }

    if (p.pwm_enable)
    {
        if (!has(p.caps, Cap::PWM) || !has(p.caps, Cap::Output))
        {
            _err = Error::PwmNotSupported;
            return false;
        }
    }

    if (has(p.caps, Cap::ADC))
    {
        if (p.backend != Backend::Esp32)
        {
            _err = Error::AdcNotSupported;
            return false;
        }
        if (!has(p.caps, Cap::Input))
        {
            _err = Error::AdcNotSupported;
            return false;
        }
    }

    return true;
}

void PortIO::applyDeferredOutputs_()
{
    for (uint8_t i = 0; i < PORT_COUNT; ++i)
    {
        const auto &p = _ports[i];
        if (p.caps == Cap::None)
            continue;
        if (!has(p.caps, Cap::Output) || has(p.caps, Cap::InputOnly))
            continue;
        if (!_hasLast[i])
            continue;

        bool v = _last[i];
        if (p.backend == Backend::Esp32)
        {
            if (p.u.esp.gpio == 0xFF)
                continue;
            if (p.u.esp.inverted)
                v = !v;
            ::digitalWrite(p.u.esp.gpio, v ? HIGH : LOW);
        }
        else if (_ext)
        {
            if (p.u.ext.inverted)
                v = !v;
            _ext->write(p.u.ext.dev, p.u.ext.pin, v);
        }
    }
    if (_ext)
        _ext->flushAll();
}

void PortIO::restoreExtenderOutputs_(uint8_t dev)
{
    for (uint8_t i = 0; i < PORT_COUNT; ++i)
    {
        const auto &p = _ports[i];
        if (p.caps == Cap::None)
            continue;
        if (p.backend != Backend::Extender || p.u.ext.dev != dev)
            continue;

        pinMode(i, p.mode);

        if (has(p.caps, Cap::Output) &&
            (p.mode == PortMode::Output || p.mode == PortMode::OutputOpenDrain) &&
            !has(p.caps, Cap::InputOnly))
        {
            bool v = p.initial_level;
            if (_hasLast[i])
                v = _last[i];
            write(i, v);
        }
        else
        {
            write(i, true);
        }
    }
}
