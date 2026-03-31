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

#include "utils/logger.hpp"

Gpio::Gpio(IoStack& io, Logger *log) : _io(io), _log(log) {}

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
  if (port >= PortIO::PORT_COUNT) return false;
  const Cap caps = ActiveBoardProfile::PORTS[port].caps;
  if (!has(caps, Cap::Input)) return false;
  return readFiltered_(port, out, timeout_ms);
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
  _debounce_inited[port] = false;
  return true;
}

bool Gpio::lastStateDyn(uint8_t port, bool& out) const {
  return _io.lastState(port, out);
}

bool Gpio::setInputDebounceMsDyn(uint8_t port, uint32_t debounce_ms) {
  if (port >= PortIO::PORT_COUNT) return false;
  _debounce_override_ms[port] = debounce_ms;
  _debounce_inited[port] = false;
  return true;
}

bool Gpio::shouldDebounce_(uint8_t port) const {
  if (port >= PortIO::PORT_COUNT) return false;
  const auto &desc = ActiveBoardProfile::PORTS[port];
  switch (desc.type) {
    case PortIO::PinType::Sensor:
    case PortIO::PinType::Button:
    case PortIO::PinType::DInput:
      return true;
    default:
      return false;
  }
}

uint32_t Gpio::debounceMsForPort_(uint8_t port) const {
  if (port >= PortIO::PORT_COUNT) return kInputDebounceMs;
  const uint32_t override_ms = _debounce_override_ms[port];
  return override_ms ? override_ms : kInputDebounceMs;
}

bool Gpio::readFiltered_(uint8_t port, bool& out, uint32_t timeout_ms) const {
  const bool raw = _io.read(port, timeout_ms);
  if (!shouldDebounce_(port)) {
    out = raw;
    return true;
  }

  const uint32_t now = millis();
  if (!_debounce_inited[port]) {
    _debounce_inited[port] = true;
    _debounce_raw[port] = raw;
    _debounce_stable[port] = raw;
    _debounce_changed_ms[port] = now;
    out = raw;
    return true;
  }

  if (_debounce_raw[port] != raw) {
    _debounce_raw[port] = raw;
    _debounce_changed_ms[port] = now;
  }

  const uint32_t debounce_ms = debounceMsForPort_(port);
  if (_debounce_stable[port] != _debounce_raw[port] &&
      (uint32_t)(now - _debounce_changed_ms[port]) >= debounce_ms) {
    _debounce_stable[port] = _debounce_raw[port];
  }

  out = _debounce_stable[port];
  return true;
}
