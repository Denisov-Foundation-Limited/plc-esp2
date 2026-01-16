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

#include "hal/gpio/extender.hpp"
#include "hal/gpio/gpio_caps.hpp"
#include "hal/gpio/portio.hpp"

#include "board_profile_base.hpp"
template <typename Profile>
struct ProfileValidator
{
    enum class Error : uint8_t
    {
        Ok = 0,
        ArraySizeMismatch,

        InvalidI2CConfig,
        DuplicateI2CBusNum,

        InvalidUARTConfig,
        DuplicateUARTNum,

        InvalidSPIConfig,
        DuplicateSPIBusNum,

        InvalidOneWireConfig,
        DuplicateOneWireId,

        ExtenderWithoutI2C,
        ExtenderWithoutDevices,

        InvalidPortConfig,
        InvalidExtenderConfig,

        PinConflict
    };

    enum class OwnerKind : uint8_t
    {
        None = 0,
        PortEsp32Gpio,
        UartTx,
        UartRx,
        I2cSda,
        I2cScl,
        SpiSck,
        SpiMiso,
        SpiMosi,
        SpiCs,
        OneWire
    };

    struct PinOwner
    {
        OwnerKind kind = OwnerKind::None;
        uint8_t index = 0; // port index, uart index, i2c index, spi index, onewire index
    };

    struct PinConflictInfo
    {
        bool has = false;
        uint8_t pin = 255;
        PinOwner first{};
        PinOwner second{};
    };

private:
    static constexpr uint8_t kMaxPin =
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
        48;
#else
        39;
#endif

    static constexpr bool pin_ok_(int8_t p)
    {
        return (p >= 0 && p <= kMaxPin);
    }

    static constexpr bool port_to_gpio_(int8_t port, uint8_t &out_gpio)
    {
        if (port < 0 || port >= PortIO::PORT_COUNT)
            return false;
        const auto &p = Profile::PORTS[(uint8_t)port];
        if (p.backend != PortIO::Backend::Esp32)
            return false;
        if (p.u.esp.gpio == 0xFF)
            return false;
        if (!pin_ok_(p.u.esp.gpio))
            return false;
        out_gpio = p.u.esp.gpio;
        return true;
    }

    static constexpr bool arrays_sane_()
    {
        return (Profile::PORTS.size() == PortIO::PORT_COUNT) &&
               (Profile::EXT_DEVS.size() == Profile::EXT_DEVS_COUNT) &&
               (Profile::UARTS.size() == Profile::UART_COUNT) &&
               (Profile::I2CS.size() == Profile::I2C_COUNT) &&
               (Profile::SPIS.size() == Profile::SPI_COUNT) &&
               (Profile::ONEWIRES.size() == Profile::ONEWIRE_COUNT);
    }

    static constexpr bool i2c_sane_()
    {
        for (uint8_t i = 0; i < Profile::I2C_COUNT; ++i)
        {
            const I2cCfg c = Profile::I2CS[i];
            if (c.freq == 0)
                return false;
            uint8_t sda_gpio = 0;
            uint8_t scl_gpio = 0;
            if (!port_to_gpio_(c.sda, sda_gpio) || !port_to_gpio_(c.scl, scl_gpio))
                return false;
            if (c.sda == c.scl || sda_gpio == scl_gpio)
                return false;
        }
        return true;
    }
    static constexpr bool i2c_unique_bus_()
    {
        for (uint8_t i = 0; i < Profile::I2C_COUNT; ++i)
            for (uint8_t j = i + 1; j < Profile::I2C_COUNT; ++j)
                if (Profile::I2CS[i].bus_num == Profile::I2CS[j].bus_num)
                    return false;
        return true;
    }

    static constexpr bool uart_sane_()
    {
        for (uint8_t i = 0; i < Profile::UART_COUNT; ++i)
        {
            const UartCfg u = Profile::UARTS[i];
            if (u.baud == 0)
                return false;
            uint8_t tx_gpio = 0;
            uint8_t rx_gpio = 0;
            if (!port_to_gpio_(u.tx, tx_gpio) || !port_to_gpio_(u.rx, rx_gpio))
                return false;
            if (u.tx == u.rx || tx_gpio == rx_gpio)
                return false;
        }
        return true;
    }
    static constexpr bool uart_unique_()
    {
        for (uint8_t i = 0; i < Profile::UART_COUNT; ++i)
            for (uint8_t j = i + 1; j < Profile::UART_COUNT; ++j)
                if (Profile::UARTS[i].uart_num == Profile::UARTS[j].uart_num)
                    return false;
        return true;
    }

    static constexpr bool spi_sane_()
    {
        for (uint8_t i = 0; i < Profile::SPI_COUNT; ++i)
        {
            const SpiCfg s = Profile::SPIS[i];
            if (s.freq == 0)
                return false;
            uint8_t gpio = 0;
            if (s.sck >= 0 && !port_to_gpio_(s.sck, gpio))
                return false;
            if (s.miso >= 0 && !port_to_gpio_(s.miso, gpio))
                return false;
            if (s.mosi >= 0 && !port_to_gpio_(s.mosi, gpio))
                return false;
            if (s.cs >= 0 && !port_to_gpio_(s.cs, gpio))
                return false;
        }
        return true;
    }
    static constexpr bool spi_unique_bus_()
    {
        for (uint8_t i = 0; i < Profile::SPI_COUNT; ++i)
            for (uint8_t j = i + 1; j < Profile::SPI_COUNT; ++j)
                if (Profile::SPIS[i].bus_num == Profile::SPIS[j].bus_num)
                    return false;
        return true;
    }

    static constexpr bool onewire_sane_()
    {
        for (uint8_t i = 0; i < Profile::ONEWIRE_COUNT; ++i)
        {
            const OneWireCfg w = Profile::ONEWIRES[i];
            uint8_t gpio = 0;
            if (!port_to_gpio_(w.pin, gpio))
                return false;
        }
        return true;
    }
    static constexpr bool onewire_unique_id_()
    {
        for (uint8_t i = 0; i < Profile::ONEWIRE_COUNT; ++i)
            for (uint8_t j = i + 1; j < Profile::ONEWIRE_COUNT; ++j)
                if (Profile::ONEWIRES[i].bus_id == Profile::ONEWIRES[j].bus_id)
                    return false;
        return true;
    }

    static constexpr bool uses_extender_()
    {
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = Profile::PORTS[i];
            if (p.caps == Cap::None)
                continue;
            if (p.backend == PortIO::Backend::Extender)
                return true;
        }
        return false;
    }
    static constexpr bool has_i2c_() { return Profile::I2C_COUNT > 0; }
    static constexpr bool has_ext_devices_()
    {
        for (uint8_t i = 0; i < Profile::EXT_DEVS_COUNT; ++i)
            if (Profile::EXT_DEVS[i].i2c_addr != 0)
                return true;
        return false;
    }

    static constexpr bool extenders_sane_()
    {
        for (uint8_t d = 0; d < Profile::EXT_DEVS_COUNT; ++d)
        {
            const auto cfg = Profile::EXT_DEVS[d];
            if (cfg.i2c_addr == 0)
                continue;
            if (cfg.i2c_addr > 0x7F)
                return false;

            bool bus_ok = false;
            for (uint8_t i = 0; i < Profile::I2C_COUNT; ++i)
            {
                if (Profile::I2CS[i].bus_num == cfg.bus_num)
                {
                    bus_ok = true;
                    break;
                }
            }
            if (!bus_ok)
                return false;
        }
        return true;
    }

    static constexpr bool ports_sane_()
    {
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = Profile::PORTS[i];
            if (p.caps == Cap::None)
                continue;
            if (p.type == PortIO::PinType::Unknown)
                return false;

            if (p.backend == PortIO::Backend::Esp32)
            {
                if (p.u.esp.gpio == 0xFF)
                    return false;
                if (p.u.esp.gpio > kMaxPin)
                    return false;
            }
            else
            {
                if (p.u.ext.dev >= Profile::EXT_DEVS_COUNT)
                    return false;
                if (Profile::EXT_DEVS[p.u.ext.dev].i2c_addr == 0)
                    return false;
            }

            if (has(p.caps, Cap::ADC) && p.backend != PortIO::Backend::Esp32)
                return false;
            if (has(p.caps, Cap::ISR_FAST) && p.backend != PortIO::Backend::Esp32)
                return false;
            if (has(p.caps, Cap::InputOnly) && has(p.caps, Cap::Output))
                return false;
        }
        return true;
    }

    static constexpr PinConflictInfo findFirstPinConflict_()
    {
        PinOwner owners[kMaxPin + 1] = {};
        for (uint8_t i = 0; i <= kMaxPin; ++i)
            owners[i] = {};

        auto tryClaim = [&](uint8_t pin, PinOwner who) -> PinConflictInfo
        {
            if (pin > kMaxPin)
                return {true, pin, owners[0], who};
            if (owners[pin].kind != OwnerKind::None)
            {
                return {true, pin, owners[pin], who};
            }
            owners[pin] = who;
            return {false, 255, {}, {}};
        };

        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = Profile::PORTS[i];
            if (p.caps == Cap::None)
                continue;
            if (p.backend != PortIO::Backend::Esp32)
                continue;
            if (p.u.esp.gpio == 0xFF)
                continue;
            auto c = tryClaim(p.u.esp.gpio, {OwnerKind::PortEsp32Gpio, i});
            if (c.has)
                return c;
        }

        for (uint8_t i = 0; i < Profile::UART_COUNT; ++i)
        {
            const auto u = Profile::UARTS[i];
            uint8_t tx_gpio = 0;
            uint8_t rx_gpio = 0;
            if (!port_to_gpio_(u.tx, tx_gpio) || !port_to_gpio_(u.rx, rx_gpio))
                continue;
            auto c1 = tryClaim(tx_gpio, {OwnerKind::UartTx, i});
            if (c1.has)
                return c1;
            auto c2 = tryClaim(rx_gpio, {OwnerKind::UartRx, i});
            if (c2.has)
                return c2;
        }

        for (uint8_t i = 0; i < Profile::I2C_COUNT; ++i)
        {
            const auto c = Profile::I2CS[i];
            uint8_t sda_gpio = 0;
            uint8_t scl_gpio = 0;
            if (!port_to_gpio_(c.sda, sda_gpio) || !port_to_gpio_(c.scl, scl_gpio))
                continue;
            auto c1 = tryClaim(sda_gpio, {OwnerKind::I2cSda, i});
            if (c1.has)
                return c1;
            auto c2 = tryClaim(scl_gpio, {OwnerKind::I2cScl, i});
            if (c2.has)
                return c2;
        }

        for (uint8_t i = 0; i < Profile::SPI_COUNT; ++i)
        {
            const auto s = Profile::SPIS[i];
            if (s.sck >= 0)
            {
                uint8_t gpio = 0;
                if (port_to_gpio_(s.sck, gpio))
                {
                    auto c = tryClaim(gpio, {OwnerKind::SpiSck, i});
                    if (c.has)
                        return c;
                }
            }
            if (s.miso >= 0)
            {
                uint8_t gpio = 0;
                if (port_to_gpio_(s.miso, gpio))
                {
                    auto c = tryClaim(gpio, {OwnerKind::SpiMiso, i});
                    if (c.has)
                        return c;
                }
            }
            if (s.mosi >= 0)
            {
                uint8_t gpio = 0;
                if (port_to_gpio_(s.mosi, gpio))
                {
                    auto c = tryClaim(gpio, {OwnerKind::SpiMosi, i});
                    if (c.has)
                        return c;
                }
            }
            if (s.cs >= 0)
            {
                uint8_t gpio = 0;
                if (port_to_gpio_(s.cs, gpio))
                {
                    auto c = tryClaim(gpio, {OwnerKind::SpiCs, i});
                    if (c.has)
                        return c;
                }
            }
        }

        for (uint8_t i = 0; i < Profile::ONEWIRE_COUNT; ++i)
        {
            const auto w = Profile::ONEWIRES[i];
            uint8_t gpio = 0;
            if (port_to_gpio_(w.pin, gpio))
            {
                auto c = tryClaim(gpio, {OwnerKind::OneWire, i});
                if (c.has)
                    return c;
            }
        }

        return {false, 255, {}, {}};
    }

    static constexpr bool pins_no_conflicts_() { return !findFirstPinConflict_().has; }

    static constexpr Error compute_error_()
    {
        if (!arrays_sane_())
            return Error::ArraySizeMismatch;

        if (!i2c_sane_())
            return Error::InvalidI2CConfig;
        if (!i2c_unique_bus_())
            return Error::DuplicateI2CBusNum;

        if (!uart_sane_())
            return Error::InvalidUARTConfig;
        if (!uart_unique_())
            return Error::DuplicateUARTNum;

        if (!spi_sane_())
            return Error::InvalidSPIConfig;
        if (!spi_unique_bus_())
            return Error::DuplicateSPIBusNum;

        if (!onewire_sane_())
            return Error::InvalidOneWireConfig;
        if (!onewire_unique_id_())
            return Error::DuplicateOneWireId;

        if (uses_extender_() && !has_i2c_())
            return Error::ExtenderWithoutI2C;
        if (uses_extender_() && !has_ext_devices_())
            return Error::ExtenderWithoutDevices;

        if (!ports_sane_())
            return Error::InvalidPortConfig;
        if (!extenders_sane_())
            return Error::InvalidExtenderConfig;

        if (!pins_no_conflicts_())
            return Error::PinConflict;

        return Error::Ok;
    }

    static String ownerToString_(PinOwner o)
    {
        switch (o.kind)
        {
        case OwnerKind::PortEsp32Gpio:
            return String(F("PORTS[")) + o.index + F("].gpio");
        case OwnerKind::UartTx:
            return String(F("UARTS[")) + o.index + F("].tx");
        case OwnerKind::UartRx:
            return String(F("UARTS[")) + o.index + F("].rx");
        case OwnerKind::I2cSda:
            return String(F("I2CS[")) + o.index + F("].sda");
        case OwnerKind::I2cScl:
            return String(F("I2CS[")) + o.index + F("].scl");
        case OwnerKind::SpiSck:
            return String(F("SPIS[")) + o.index + F("].sck");
        case OwnerKind::SpiMiso:
            return String(F("SPIS[")) + o.index + F("].miso");
        case OwnerKind::SpiMosi:
            return String(F("SPIS[")) + o.index + F("].mosi");
        case OwnerKind::SpiCs:
            return String(F("SPIS[")) + o.index + F("].cs");
        case OwnerKind::OneWire:
            return String(F("ONEWIRES[")) + o.index + F("].pin");
        default:
            return String(F("None"));
        }
    }

public:
    static constexpr bool arrays_sane = arrays_sane_();
    static constexpr bool i2c_sane = i2c_sane_();
    static constexpr bool i2c_unique = i2c_unique_bus_();
    static constexpr bool uart_sane = uart_sane_();
    static constexpr bool uart_unique = uart_unique_();
    static constexpr bool spi_sane = spi_sane_();
    static constexpr bool spi_unique = spi_unique_bus_();
    static constexpr bool onewire_sane = onewire_sane_();
    static constexpr bool onewire_unique = onewire_unique_id_();
    static constexpr bool extenders_ok = (!uses_extender_() || (has_i2c_() && has_ext_devices_())) && extenders_sane_();
    static constexpr bool ports_sane = ports_sane_();
    static constexpr bool pins_ok = pins_no_conflicts_();

    static constexpr Error error = compute_error_();
    static constexpr bool ok = (error == Error::Ok);
    static constexpr PinConflictInfo pin_conflict = findFirstPinConflict_();

    static String errorString()
    {
        switch (error)
        {
        case Error::Ok:
            return F("Profile OK");
        case Error::ArraySizeMismatch:
            return F("Profile array size mismatch (std::array sizes must match COUNTs)");
        case Error::InvalidI2CConfig:
            return F("Invalid I2C config (pins/freq)");
        case Error::DuplicateI2CBusNum:
            return F("Duplicate I2C bus_num in I2CS[]");
        case Error::InvalidUARTConfig:
            return F("Invalid UART config (pins/baud)");
        case Error::DuplicateUARTNum:
            return F("Duplicate UART uart_num in UARTS[]");
        case Error::InvalidSPIConfig:
            return F("Invalid SPI config (pins/freq)");
        case Error::DuplicateSPIBusNum:
            return F("Duplicate SPI bus_num in SPIS[]");
        case Error::InvalidOneWireConfig:
            return F("Invalid OneWire config (pin)");
        case Error::DuplicateOneWireId:
            return F("Duplicate OneWire bus_id in ONEWIRES[]");
        case Error::ExtenderWithoutI2C:
            return F("Extender ports used but no I2C buses defined");
        case Error::ExtenderWithoutDevices:
            return F("Extender ports used but EXT_DEVS[] has no configured devices");
        case Error::InvalidPortConfig:
            return F("Invalid PORTS[] configuration (backend/caps/dev/gpio)");
        case Error::InvalidExtenderConfig:
            return F("Invalid EXT_DEVS[] configuration (addr or bus_num)");
        case Error::PinConflict:
            return F("Pin conflict detected");
        default:
            return F("Unknown profile error");
        }
    }

    static String errorStringDetailed()
    {
        if (error != Error::PinConflict)
            return errorString();
        if (!pin_conflict.has)
            return errorString();

        String s = F("Pin conflict on GPIO ");
        s += pin_conflict.pin;
        s += F(": ");
        s += ownerToString_(pin_conflict.first);
        s += F(" conflicts with ");
        s += ownerToString_(pin_conflict.second);
        return s;
    }

    static void printDiagnostics(Stream &out)
    {
        out.println(F("=== Profile Diagnostics ==="));
        out.print(F("Result: "));
        out.println(errorStringDetailed());
        if (error == Error::PinConflict && pin_conflict.has)
        {
            out.print(F("Conflict pin: GPIO "));
            out.println(pin_conflict.pin);
            out.print(F("First owner: "));
            out.println(ownerToString_(pin_conflict.first));
            out.print(F("Second owner: "));
            out.println(ownerToString_(pin_conflict.second));
        }
    }
};
