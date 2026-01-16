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
#include <array>
#include <stdint.h>

#include "hal/gpio/extender.hpp"
#include "hal/gpio/gpio_caps.hpp"
#if defined(ESP32)
#include "driver/gpio.h"
#endif

class PortIO
{
public:
    static constexpr uint8_t PORT_COUNT = 171;
    using PortId = uint8_t;

    enum class PinType : uint8_t
    {
        Unknown = 0,
        System,
        Relay,
        Led,
        Sensor,
        Button,
        DInput,
        Buzzer,
        Fan
    };

    enum class PortMode : uint8_t
    {
        Input,
        InputPullUp,
        InputPullDown,
        Output,
        OutputOpenDrain
    };
    enum class Backend : uint8_t
    {
        Esp32,
        Extender
    };
    enum class Location : uint8_t
    {
        Cpu = 0,
        Ext1,
        Ext2,
        Ext3,
        Ext4,
        Ext5,
        Ext6,
        Ext7,
        Ext8,
        Ext9,
        Ext10,
        Unknown
    };

    struct PwmCfg
    {
        uint8_t channel;
        uint32_t freq;
        uint8_t resolution;
        uint32_t duty_init;
    };
    struct Esp32Pin
    {
        uint8_t gpio;
        bool inverted;
    };
    struct ExtPin
    {
        uint8_t dev;
        uint8_t pin;
        bool inverted;
    };

    struct PortDesc
    {
        Backend backend;
        Cap caps;
        PortMode mode;
        PinType type;
        Location location = Location::Cpu;
        bool allow_control;
        bool initial_level;

        bool pwm_enable;
        PwmCfg pwm;

        union
        {
            Esp32Pin esp;
            ExtPin ext;
        } u;
    };

    enum class Error : uint8_t
    {
        Ok = 0,
        ModeNotSupported,
        PwmNotSupported,
        OutputOnInputOnly,
        ExtenderMissing,
        InvalidPin,
        AdcNotSupported
    };

    constexpr PortIO(const std::array<PortDesc, PORT_COUNT> &ports, Extender *ext)
        : _ports(ports), _ext(ext) {}

    bool begin()
    {
        _err = Error::Ok;
        for (uint8_t i = 0; i < PORT_COUNT; ++i)
        {
            _hasLast[i] = false;
            _last[i] = false;
        }

        if (usesExtender_() && _ext == nullptr)
        {
            _err = Error::ExtenderMissing;
            return false;
        }

        if (_ext)
            _ext->begin();

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
                // PCF-style "release" default
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

    void loop()
    {
        if (_ext)
            _ext->flushAll();
    }
    Error lastError() const { return _err; }

    bool lastState(PortId id, bool &outLogical) const
    {
        if (id >= PORT_COUNT)
            return false;
        if (!_hasLast[id])
            return false;
        outLogical = _last[id];
        return true;
    }

    const PortDesc &desc(PortId id) const { return _ports[id]; }
    PinType type(PortId id) const
    {
        if (id >= PORT_COUNT)
            return PinType::Unknown;
        return _ports[id].type;
    }

    void pinMode(PortId id, PortMode mode)
    {
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

    void write(PortId id, bool logicalLevel)
    {
        if (id >= PORT_COUNT)
            return;
        const auto &p = _ports[id];
        if (p.caps == Cap::None)
            return;

        _last[id] = logicalLevel;
        _hasLast[id] = true;

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

    bool read(PortId id) const
    {
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
            return v;
        }
        if (_ext)
        {
            v = _ext->read(p.u.ext.dev, p.u.ext.pin);
            if (p.u.ext.inverted)
                v = !v;
            return v;
        }
        return false;
    }

    int adcRead(PortId id)
    {
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

    void pwmWrite(PortId id, uint32_t duty)
    {
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

    bool writeFast(PortId id, bool logicalLevel)
    {
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

    bool readFast(PortId id, bool &outLogicalLevel) const
    {
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

private:
    const std::array<PortDesc, PORT_COUNT> &_ports;
    Extender *_ext;
    Error _err = Error::Ok;

    static constexpr uint8_t kMaxPin =
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
        48;
#else
        39;
#endif

    bool _last[PORT_COUNT] = {};
    bool _hasLast[PORT_COUNT] = {};

    bool usesExtender_() const
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

    static uint8_t toArduinoMode_(PortMode m)
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

    bool validate_(const PortDesc &p)
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
};
