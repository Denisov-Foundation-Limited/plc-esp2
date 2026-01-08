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

#include "boards/profiles/board_profile_common.hpp"

#include "hal/gpio/extender.hpp"
#include "hal/gpio/gpio_caps.hpp"
#include "hal/gpio/portio.hpp"
struct BoardProfileFCPLC_3v1 : BoardProfileCommon
{
    // ---- logging sink ----
    static inline constexpr LogCfg LOG = {
        LogCfg::Sink::UsbSerial, 0, 115200
    };

    // ---- UART ----
    static inline constexpr uint8_t UART_COUNT = 1;
    static inline constexpr std::array<UartCfg, UART_COUNT> UARTS{{
        { 1, 5, 4, 115200 }
    }};

    // ---- Buzzer ----
    static inline constexpr uint8_t BUZZER_PIN = 38;

    // ---- Status ----
    static inline constexpr uint8_t STATUS_PIN = 39;

    // ---- I2C ----
    static inline constexpr uint8_t I2C_COUNT = 2;
    static inline constexpr std::array<I2cCfg, I2C_COUNT> I2CS{{
        { 0, 2, 1, 400000 },
        { 1, 47, 48, 400000 }
    }};

    // ---- SPI ----
    static inline constexpr uint8_t SPI_COUNT = 1;
    static inline constexpr std::array<SpiCfg, SPI_COUNT> SPIS{{
        { 0, 13, 12, 11, 14, 10000000 }
    }};

    // ---- OneWire ----
    static inline constexpr uint8_t ONEWIRE_COUNT = 2;
    static inline constexpr std::array<OneWireCfg, ONEWIRE_COUNT> ONEWIRES{{
        { OneWireCfg::OwType::iButton, 6, false },
        { OneWireCfg::OwType::Temp,    7, false },
    }};

    // ---- Extenders ----
    static inline constexpr uint8_t EXT_DEVS_COUNT = 10;
    static inline constexpr std::array<Extender::DevCfg, EXT_DEVS_COUNT> EXT_DEVS{{
        // Onboard extender
        { 0, 0x20, Extender::Type::MCP23017 },
        // Rear pannel extender
        { 0, 0x21, Extender::Type::MCP23017 },
        // Extended Units
        { 1, 0x20, Extender::Type::MCP23017 },
        { 1, 0x21, Extender::Type::MCP23017 },
        { 1, 0x22, Extender::Type::MCP23017 },
        { 1, 0x23, Extender::Type::MCP23017 },
        { 1, 0x24, Extender::Type::MCP23017 },
        { 1, 0x25, Extender::Type::MCP23017 },
        { 1, 0x26, Extender::Type::MCP23017 },
        { 1, 0x27, Extender::Type::MCP23017 }
    }};

    // ---- Ports ----
    static inline constexpr std::array<PortIO::PortDesc, PortIO::PORT_COUNT> PORTS{{
        // Onboard sensors pins for DHT22 or other sensors [1..6]
        { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, false, false, { 0, 0, 0, 0 }, { .esp = { 15, false } } },
        { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, false, false, { 0, 0, 0, 0 }, { .esp = { 16, false } } },
        { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, false, false, { 0, 0, 0, 0 }, { .esp = { 17, false } } },
        { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, false, false, { 0, 0, 0, 0 }, { .esp = { 18, false } } },
        { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, false, false, { 0, 0, 0, 0 }, { .esp = {  8, false } } },
        { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, false, false, { 0, 0, 0, 0 }, { .esp = {  9, false } } },

        // Onboard Relays pins [1:8]
        { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  7, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  6, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  5, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  4, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  3, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  2, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  1, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 15, false }}},

        // Onboard Digital Inputs [1..8]
        { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  8, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  9, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 10, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 11, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 12, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 13, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 14, false }}},
        { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  0, false }}},

        // Unused
        { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, false, false, { 0, 0, 0, 0 }, { .esp = { 0xFF, false } } },
    }};

    template <uint8_t P>
    static constexpr Cap capsOf()
    {
        static_assert(P < PortIO::PORT_COUNT, "Port out of range");
        return PORTS[P].caps;
    }
};
