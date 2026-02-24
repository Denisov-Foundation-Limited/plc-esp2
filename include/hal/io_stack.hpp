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

#include "hal/gpio/portio.hpp"

class IoStack
{
public:
    static constexpr uint8_t PORT_COUNT = PortIO::PORT_COUNT;

    explicit IoStack(PortIO &portio);

    bool begin();
    void loop();
    void pinMode(uint8_t id, PortIO::PortMode mode);
    void initImages();
    void scanInputs();
    void applyOutputs();
    bool write(uint8_t id, bool logicalLevel);
    bool read(uint8_t id) const;
    const PortIO::PortDesc &desc(uint8_t id) const;
    bool lastState(uint8_t id, bool &outLogical) const;

private:
    static constexpr uint32_t kDebounceMs = 100;

    static uint32_t millis_();

    PortIO &_portio;
    bool _inputs[PORT_COUNT] = {};
    bool _outputs[PORT_COUNT] = {};
    bool _applied[PORT_COUNT] = {};
    bool _dirty[PORT_COUNT] = {};
    bool _raw_inputs[PORT_COUNT] = {};
    uint32_t _last_change_ms[PORT_COUNT] = {};
};
