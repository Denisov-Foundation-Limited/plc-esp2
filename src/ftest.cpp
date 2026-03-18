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

#include "ftest.hpp"

#include "boards/board_profile.hpp"
#include "hal/gpio/gpio_caps.hpp"

Ftest::Ftest(Logger &logs, IoStack &io, OneWireManager &ow, IButton &ibutton,
               Ds18b20 &ds18b20, I2CManager &i2c, RTC &rtc, Extender &ext,
               TaskBinder &tb)
    : _logs(logs), _io(io), _ow(ow), _ibutton(ibutton),
      _ds18b20(ds18b20), _i2c(i2c), _rtc(rtc), _ext(ext), _tb(tb){
}

void Ftest::start(){
    initOneWire_();
    initBoardTemp_();
    initEeprom_();

    _tb.enableFtest(true);
}

void Ftest::task(){
    _state = !_state;

    _logs.info(F("FTEST"), F("===================================================="));
    _logs.info(F("FTEST"), F(""));
    _logs.info(F("FTEST"), F("[OUTPUTS]"));
    logGpioOuts_();

    _logs.info(F("FTEST"), F(""));
    _logs.info(F("FTEST"), F("[INPUTS]"));
    logGpioIns_();

    _logs.info(F("FTEST"), F(""));
    _logs.info(F("FTEST"), F("[BUTTONS]"));
    logButtons_();

    _logs.info(F("FTEST"), F(""));
    _logs.info(F("FTEST"), F("[DS18B20]"));
    logDs18b20_();

    _logs.info(F("FTEST"), F(""));
    _logs.info(F("FTEST"), F("[IBUTTON]"));
    logIbutton_();

    _logs.info(F("FTEST"), F(""));
    _logs.info(F("FTEST"), F("[BOARD_TEMP]"));
    logBoardTemp_();

    _logs.info(F("FTEST"), F(""));
    _logs.info(F("FTEST"), F("[RTC]"));
    logRtc_();

    _logs.info(F("FTEST"), F(""));
    _logs.info(F("FTEST"), F("[EEPROM]"));
    logEeprom_();
}

void Ftest::initEeprom_(){
    const auto cfg = ActiveBoardProfile::EEPROM;
    _eeprom_lock_ctx.i2c = &_i2c;
    _eeprom_lock_ctx.bus = cfg.bus_num;
    _eeprom.setBusLockCallbacks(&Ftest::i2cLockCb_, &Ftest::i2cUnlockCb_, &_eeprom_lock_ctx);
    TwoWire *wire = _i2c.wirePtr(cfg.bus_num);
    if (!wire || !_eeprom.begin(*wire, cfg.addr))
    {
        _eeprom_ok = false;
        _logs.error(F("FTEST"), F("EEPROM init failed"));
        return;
    }
    _eeprom_ok = true;
}

void Ftest::logEeprom_(){
    if (!_eeprom_ok)
    {
        _logs.info(F("FTEST"), F("EEPROM: err"));
        return;
    }
    const uint32_t used = _eeprom.usedBytes();
    const uint32_t total = At24lc512::capacityBytes();
    const uint32_t free = At24lc512::remainingBytes(used);
    _logs.info(F("FTEST"), F("EEPROM used: %lu free: %lu total: %lu"),
               (unsigned long)used, (unsigned long)free, (unsigned long)total);
}

void Ftest::initBoardTemp_(){
    const auto cfg = ActiveBoardProfile::BOARD_TEMP;
    _lm75_bus = cfg.bus_num;
    TwoWire *wire = _i2c.wirePtr(cfg.bus_num);
    I2CManager::ScopedBusLock lk(_i2c, _lm75_bus);
    if (!wire || !lk.locked() || !_lm75.begin(*wire, cfg.addr))
    {
        _lm75_ok = false;
        _logs.error(F("FTEST"), F("LM75 init failed"));
        return;
    }
    _lm75_ok = true;
}

void Ftest::logBoardTemp_(){
    if (!_lm75_ok)
    {
        _logs.info(F("FTEST"), F("LM75: err"));
        return;
    }
    float t = 0.0f;
    I2CManager::ScopedBusLock lk(_i2c, _lm75_bus);
    if (!lk.locked() || !_lm75.readTempC(t))
    {
        _logs.info(F("FTEST"), F("BOARD: err"));
        return;
    }
    _logs.info(F("FTEST"), F("BOARD: %.2fC"), t);
}

bool Ftest::i2cLockCb_(void *ctx, uint32_t timeout_ms)
{
    I2cLockCtx *c = static_cast<I2cLockCtx *>(ctx);
    return c && c->i2c ? c->i2c->lockBus(c->bus, timeout_ms) : false;
}

void Ftest::i2cUnlockCb_(void *ctx)
{
    I2cLockCtx *c = static_cast<I2cLockCtx *>(ctx);
    if (c && c->i2c)
        c->i2c->unlockBus(c->bus);
}

void Ftest::logRtc_(){
    Ds3231Mz::DateTime dt{};
    if (!_rtc.Time(dt))
    {
        _logs.info(F("FTEST"), F("RTC: err"));
        return;
    }
    float rtc_t = 0.0f;
    const bool rtc_ok = _rtc.readTemp(rtc_t);
    char date_buf[16] = {};
    char time_buf[16] = {};
    snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
             (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
    snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
             (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
    if (rtc_ok)
        _logs.info(F("FTEST"), F("RTC: %s %s %.2fC"), date_buf, time_buf, rtc_t);
    else
        _logs.info(F("FTEST"), F("RTC: %s %s temp: err"), date_buf, time_buf);
}

void Ftest::logGpioOuts_(){
    for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
    {
        const auto &p = _io.desc(i);
        if (p.caps == Cap::None)
            continue;
        if (!isPortActive_(p))
            continue;

        if (p.type == PortIO::PinType::Led ||
            p.type == PortIO::PinType::Relay ||
            p.type == PortIO::PinType::Buzzer ||
            p.type == PortIO::PinType::Fan)
        {
            if (has(p.caps, Cap::Output))
            {
                _io.write(i, _state);
                _logs.info(F("FTEST"), F("GPIO[%u]: State: %s type: %s"), i, _state ? "High" : "Low", pinTypeName_(p.type));
            }
            continue;
        }
    }
}

void Ftest::logGpioIns_(){
    for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
    {
        const auto &p = _io.desc(i);
        if (p.caps == Cap::None)
            continue;
        if (!isPortActive_(p))
            continue;

        if (p.type == PortIO::PinType::DInput)
        {
            if (has(p.caps, Cap::Input))
            {
                const bool v = _io.read(i);
                _logs.info(F("FTEST"), F("GPIO[%u]: State: %s type: %s"), i, v ? "High" : "Low", pinTypeName_(p.type));
            }
            continue;
        }
    }
}

void Ftest::logButtons_(){
    for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
    {
        const auto &p = _io.desc(i);
        if (p.caps == Cap::None)
            continue;
        if (!isPortActive_(p))
            continue;

        if (p.type == PortIO::PinType::Button)
        {
            bool v = _io.read(i);
            _logs.info(F("FTEST"), F("BUTTON[%u]: State: %s"), i, v ? "High" : "Low");
        }
    }
}

void Ftest::logDs18b20_(){
    if (!_ds18b20_ok)
    {
        OneWireBus *temp_bus = _ow.busPtrById(OneWireManager::OwBusType::Temp);
        if (!temp_bus)
        {
            _logs.info(F("FTEST"), F("DS18B20: bus missing"));
            return;
        }
        _ds18b20.begin(*temp_bus);
        _ds18b20_ok = true;
    }
    static constexpr size_t kMaxSerials = 50;
    char serials[kMaxSerials][17] = {};
    size_t count = 0;
    _ds18b20.listSerials(serials, kMaxSerials, count);
    if (count == 0)
    {
        _logs.info(F("FTEST"), F("DS18B20: none"));
        return;
    }
    for (size_t i = 0; i < count; ++i)
        _logs.info(F("FTEST"), F("DS18B20[%u]: %s"), (unsigned)i, serials[i]);
}

void Ftest::logIbutton_(){
    uint8_t addr[8] = {};
    if (_ibutton.readSerial(addr))
    {
        char hex[17] = {};
        IButton::toHex(addr, hex);
        _logs.info(F("FTEST"), F("IBUTTON: %s"), hex);
    }
}

void Ftest::initOneWire_(){
    _ibutton.setBusLockCallbacks(&Ftest::owIButtonLockCb_, &Ftest::owIButtonUnlockCb_, this);
    _ds18b20.setBusLockCallbacks(&Ftest::owTempLockCb_, &Ftest::owTempUnlockCb_, this);
    OneWireBus *ib_bus = _ow.busPtrById(OneWireManager::OwBusType::iButton);
    if (!ib_bus || !_ibutton.begin(*ib_bus))
    {
        _logs.error(F("FTEST"), F("OW iButton bus missing"));
    }
    OneWireBus *temp_bus = _ow.busPtrById(OneWireManager::OwBusType::Temp);
    if (!temp_bus)
    {
        _ds18b20_ok = false;
        _logs.error(F("FTEST"), F("OW DS18B20 bus missing"));
        return;
    }
    _ds18b20.begin(*temp_bus);
    _ds18b20_ok = true;
}

bool Ftest::owIButtonLockCb_(void *ctx, uint32_t timeout_ms)
{
    auto *self = static_cast<Ftest *>(ctx);
    return self ? self->_ow.lockBusById(OneWireManager::OwBusType::iButton, timeout_ms) : false;
}

void Ftest::owIButtonUnlockCb_(void *ctx)
{
    auto *self = static_cast<Ftest *>(ctx);
    if (self)
        self->_ow.unlockBusById(OneWireManager::OwBusType::iButton);
}

bool Ftest::owTempLockCb_(void *ctx, uint32_t timeout_ms)
{
    auto *self = static_cast<Ftest *>(ctx);
    return self ? self->_ow.lockBusById(OneWireManager::OwBusType::Temp, timeout_ms) : false;
}

void Ftest::owTempUnlockCb_(void *ctx)
{
    auto *self = static_cast<Ftest *>(ctx);
    if (self)
        self->_ow.unlockBusById(OneWireManager::OwBusType::Temp);
}

const char *Ftest::pinTypeName_(PortIO::PinType t){
    switch (t)
    {
    case PortIO::PinType::System:
        return kTypeSystem;
    case PortIO::PinType::Relay:
        return kTypeRelay;
    case PortIO::PinType::Led:
        return kTypeLed;
    case PortIO::PinType::Sensor:
        return kTypeSensor;
    case PortIO::PinType::Button:
        return kTypeButton;
    case PortIO::PinType::DInput:
        return kTypeDInput;
    case PortIO::PinType::Buzzer:
        return kTypeBuzzer;
    case PortIO::PinType::Fan:
        return kTypeFan;
    default:
        return kTypeUnknown;
    }
}

bool Ftest::isPortActive_(const PortIO::PortDesc &p) const{
    if (p.backend != PortIO::Backend::Extender)
        return true;
    return _ext.isPresent(p.u.ext.dev);
}
