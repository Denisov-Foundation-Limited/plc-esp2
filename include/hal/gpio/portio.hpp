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
#include <array>
#include <stdint.h>

#include "hal/gpio/extender.hpp"
#include "hal/gpio/gpio_caps.hpp"
#include "utils/rtos_lock.hpp"

class PortIO
{
public:
    static constexpr uint8_t PORT_COUNT = 172;
    using PortId = uint8_t;
    using LockGuard = RtosRecursiveLock::Guard;

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
        uint8_t ui_id = 0;
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

    LockGuard lockGuard() const { return _lock.guard(); }

    bool begin();
    void loop();
    void setOutputsEnabled(bool enabled);
    Error lastError() const;
    bool lastState(PortId id, bool &outLogical) const;
    const PortDesc &desc(PortId id) const;
    PinType type(PortId id) const;
    void pinMode(PortId id, PortMode mode);
    void write(PortId id, bool logicalLevel);
    bool read(PortId id, bool &outLogical) const;
    bool read(PortId id) const;
    int adcRead(PortId id);
    void pwmWrite(PortId id, uint32_t duty);
    bool writeFast(PortId id, bool logicalLevel);
    bool readFast(PortId id, bool &outLogicalLevel) const;

private:
    const std::array<PortDesc, PORT_COUNT> &_ports;
    Extender *_ext;
    Error _err = Error::Ok;
    bool _outputs_enabled = false;

    static constexpr uint8_t kMaxPin =
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
        48;
#else
        39;
#endif

    bool _last[PORT_COUNT] = {};
    bool _hasLast[PORT_COUNT] = {};
    bool _ext_present[Extender::MAX_DEVS] = {};
    mutable RtosRecursiveLock _lock;

    bool usesExtender_() const;
    static uint8_t toArduinoMode_(PortMode m);
    bool validate_(const PortDesc &p);
    void applyDeferredOutputs_();
    void restoreExtenderOutputs_(uint8_t dev);
};
