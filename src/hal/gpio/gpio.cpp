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

#include "hal/gpio/gpio.hpp"

Gpio::Gpio(IoStack& io) : _io(io) {}

bool Gpio::begin() { return _io.begin(); }
void Gpio::loop() { _io.loop(); }

Cap Gpio::capsDyn(uint8_t port) const {
  if (port >= PortIO::PORT_COUNT) return Cap::None;
  return ActiveBoardProfile::PORTS[port].caps;
}

bool Gpio::writeDyn(uint8_t port, bool v, uint32_t timeout_ms) {
  if (port >= PortIO::PORT_COUNT) return false;
  const Cap caps = ActiveBoardProfile::PORTS[port].caps;
  if (!has(caps, Cap::Output) || has(caps, Cap::InputOnly)) return false;
  return _io.write(port, v, timeout_ms);
}

bool Gpio::readDyn(uint8_t port, bool& out, uint32_t timeout_ms) const {
  return tryReadDyn(port, out, timeout_ms);
}

bool Gpio::tryReadDyn(uint8_t port, bool& out, uint32_t timeout_ms) const {
  if (port >= PortIO::PORT_COUNT) return false;
  const Cap caps = ActiveBoardProfile::PORTS[port].caps;
  if (!has(caps, Cap::Input)) return false;
  return _io.tryRead(port, out, timeout_ms);
}

bool Gpio::pinModeDyn(uint8_t port, PortIO::PortMode mode) {
  if (port >= PortIO::PORT_COUNT) return false;
  const Cap caps = ActiveBoardProfile::PORTS[port].caps;
  if (caps == Cap::None) return false;

  switch (mode) {
    case PortIO::PortMode::Input:
      if (!has(caps, Cap::Input)) return false;
      break;
    case PortIO::PortMode::InputPullUp:
      if (!has(caps, Cap::Input) || !has(caps, Cap::PullUp)) return false;
      break;
    case PortIO::PortMode::InputPullDown:
      if (!has(caps, Cap::Input) || !has(caps, Cap::PullDown)) return false;
      break;
    case PortIO::PortMode::Output:
    case PortIO::PortMode::OutputOpenDrain:
      if (!has(caps, Cap::Output) || has(caps, Cap::InputOnly)) return false;
      break;
  }
  _io.pinMode(port, mode);
  return true;
}

bool Gpio::lastStateDyn(uint8_t port, bool& out) const {
  return _io.lastState(port, out);
}
