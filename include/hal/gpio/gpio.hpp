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

#include "hal/gpio/gpio_caps.hpp"
#include "hal/io_stack.hpp"

class Gpio {
public:
  enum Port : uint8_t { LED = 0, BTN = 1, POT = 2, RELAY = 3 };

  explicit Gpio(IoStack& io);

  bool begin();
  void loop();

  template<uint8_t P>
  inline void write(bool v) {
    static_assert(P < PortIO::PORT_COUNT, "Port out of range");
    static_assert(has(ActiveBoardProfile::template capsOf<P>(), Cap::Output), "write() requires Cap::Output");
    static_assert(!has(ActiveBoardProfile::template capsOf<P>(), Cap::InputOnly), "write() forbidden for InputOnly");
    _io.write(P, v);
  }

  template<uint8_t P>
  inline bool read() const {
    static_assert(P < PortIO::PORT_COUNT, "Port out of range");
    static_assert(has(ActiveBoardProfile::template capsOf<P>(), Cap::Input), "read() requires Cap::Input");
    return _io.read(P);
  }

  template<uint8_t P>
  inline void pinMode(PortIO::PortMode mode) {
    static_assert(P < PortIO::PORT_COUNT, "Port out of range");
    constexpr Cap caps = ActiveBoardProfile::template capsOf<P>();

    switch (mode) {
      case PortIO::PortMode::Input:
        static_assert(has(caps, Cap::Input), "Input requires Cap::Input");
        break;
      case PortIO::PortMode::InputPullUp:
        static_assert(has(caps, Cap::Input) && has(caps, Cap::PullUp), "InputPullUp requires Cap::Input|Cap::PullUp");
        break;
      case PortIO::PortMode::InputPullDown:
        static_assert(has(caps, Cap::Input) && has(caps, Cap::PullDown), "InputPullDown requires Cap::Input|Cap::PullDown");
        break;
      case PortIO::PortMode::Output:
      case PortIO::PortMode::OutputOpenDrain:
        static_assert(has(caps, Cap::Output), "Output requires Cap::Output");
        static_assert(!has(caps, Cap::InputOnly), "Output forbidden for InputOnly");
        break;
    }

    _io.pinMode(P, mode);
  }

  template<uint8_t P>
  inline bool lastState(bool& out) const {
    static_assert(P < PortIO::PORT_COUNT, "Port out of range");
    static_assert(has(ActiveBoardProfile::template capsOf<P>(), Cap::Output), "lastState() meaningful for outputs");
    return _io.lastState(P, out);
  }

  // ---- runtime loop layer ----
  Cap capsDyn(uint8_t port) const;
  bool writeDyn(uint8_t port, bool v, uint32_t timeout_ms = 0xFFFFFFFFu);
  bool readDyn(uint8_t port, bool& out, uint32_t timeout_ms = 0xFFFFFFFFu) const;
  bool tryReadDyn(uint8_t port, bool& out, uint32_t timeout_ms = 0xFFFFFFFFu) const;
  bool pinModeDyn(uint8_t port, PortIO::PortMode mode);
  bool lastStateDyn(uint8_t port, bool& out) const;

private:
  IoStack& _io;
};
