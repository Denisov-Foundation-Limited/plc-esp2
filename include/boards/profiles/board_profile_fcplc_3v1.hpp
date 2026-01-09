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
        /*  0 */ { 1, 26, 27, 115200 }
    }};

    // ---- RTC ----
    static inline constexpr RtcCfg RTC = { 0, 0x68 };
    static inline constexpr LcdCfg LCD = { 0, 0x3F };

    // ---- Control pins ----
    static inline constexpr uint8_t BUZZER_PIN = 34;
    static inline constexpr uint8_t STATUS_PIN = 35;
    static inline constexpr uint8_t NET_LED_PIN = 36;
    static inline constexpr uint8_t FAN_PIN = 37;
    static inline constexpr uint8_t ALARM_LED_PIN = 38;
    static inline constexpr uint8_t BTN_UP_PIN = 39;
    static inline constexpr uint8_t BTN_OK_PIN = 40;
    static inline constexpr uint8_t BTN_DOWN_PIN = 41;
    static inline constexpr uint8_t LCD_BACKLIGHT_PIN = 42;

    // ---- I2C ----
    static inline constexpr uint8_t I2C_COUNT = 2;
    static inline constexpr std::array<I2cCfg, I2C_COUNT> I2CS{{
        /*  0 */ { 0, 22, 23, 400000 },
        /*  1 */ { 1, 24, 25, 400000 }
    }};

    // ---- SPI ----
    static inline constexpr uint8_t SPI_COUNT = 1;
    static inline constexpr std::array<SpiCfg, SPI_COUNT> SPIS{{
        /*  0 */ { 0, 28, 29, 30, 31, 10000000 }
    }};

    // ---- OneWire ----
    static inline constexpr uint8_t ONEWIRE_COUNT = 2;
    static inline constexpr std::array<OneWireCfg, ONEWIRE_COUNT> ONEWIRES{{
        /*  0 */ { OneWireCfg::OwType::iButton, 32, false },
        /*  1 */ { OneWireCfg::OwType::Temp,    33, false },
    }};

    // ---- Extenders ----
    static inline constexpr uint8_t EXT_DEVS_COUNT = 11;
    static inline constexpr std::array<Extender::DevCfg, EXT_DEVS_COUNT> EXT_DEVS{{
        // Onboard extender
        /*  0 */ { 0, 0x20, Extender::Type::MCP23017 },
        // Rear pannel extender
        /*  1 */ { 0, 0x21, Extender::Type::MCP23017 },
        // Rear pannel LCD
        /*  2 */ { 0, 0x22, Extender::Type::PCF8574 },
        // Extended Units
        /*  3 */ { 1, 0x20, Extender::Type::MCP23017 },
        /*  4 */ { 1, 0x21, Extender::Type::MCP23017 },
        /*  5 */ { 1, 0x22, Extender::Type::MCP23017 },
        /*  6 */ { 1, 0x23, Extender::Type::MCP23017 },
        /*  7 */ { 1, 0x24, Extender::Type::MCP23017 },
        /*  8 */ { 1, 0x25, Extender::Type::MCP23017 },
        /*  9 */ { 1, 0x26, Extender::Type::MCP23017 },
        /* 10 */ { 1, 0x27, Extender::Type::MCP23017 }
    }};

    // ---- Ports ----
    static inline constexpr std::array<PortIO::PortDesc, PortIO::PORT_COUNT> PORTS{{
        // Onboard sensors pins for DHT22 or other sensors [1..6]
        /*  0 */ { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, PortIO::PinType::Sensor, false, false, { 0, 0, 0, 0 }, { .esp = { 15, false } } },
        /*  1 */ { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, PortIO::PinType::Sensor, false, false, { 0, 0, 0, 0 }, { .esp = { 16, false } } },
        /*  2 */ { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, PortIO::PinType::Sensor, false, false, { 0, 0, 0, 0 }, { .esp = { 17, false } } },
        /*  3 */ { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, PortIO::PinType::Sensor, false, false, { 0, 0, 0, 0 }, { .esp = { 18, false } } },
        /*  4 */ { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, PortIO::PinType::Sensor, false, false, { 0, 0, 0, 0 }, { .esp = {  8, false } } },
        /*  5 */ { PortIO::Backend::Esp32, ESP_GPIO_IN, PortIO::PortMode::Input, PortIO::PinType::Sensor, false, false, { 0, 0, 0, 0 }, { .esp = {  9, false } } },

        // Onboard Relays pins [1:8]
        /*  6 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Relay, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  7, false }}},
        /*  7 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Relay, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  6, false }}},
        /*  8 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Relay, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  5, false }}},
        /*  9 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Relay, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  4, false }}},
        /* 10 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Relay, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  3, false }}},
        /* 11 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Relay, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  2, false }}},
        /* 12 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Relay, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  1, false }}},
        /* 13 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Relay, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 15, false }}},

        // Onboard Digital Inputs [1..8]
        /* 14 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::DInput, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  8, false }}},
        /* 15 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::DInput, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  9, false }}},
        /* 16 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::DInput, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 10, false }}},
        /* 17 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::DInput, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 11, false }}},
        /* 18 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::DInput, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 12, false }}},
        /* 19 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::DInput, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 13, false }}},
        /* 20 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::DInput, true, false, { 0, 0, 0, 0 }, { .ext = { 0, 14, false }}},
        /* 21 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::DInput, true, false, { 0, 0, 0, 0 }, { .ext = { 0,  0, false }}},

        // I2C reserved pins (not exposed as ports)
        /* 22 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = {  2, false } } },
        /* 23 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = {  1, false } } },
        /* 24 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = { 47, false } } },
        /* 25 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = { 48, false } } },

        // UART reserved pins (not exposed as ports)
        /* 26 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = {  5, false } } },
        /* 27 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = {  4, false } } },

        // SPI reserved pins (not exposed as ports)
        /* 28 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = { 13, false } } },
        /* 29 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = { 12, false } } },
        /* 30 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = { 11, false } } },
        /* 31 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = { 14, false } } },

        // OneWire reserved pins (not exposed as ports)
        /* 32 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = {  6, false } } },
        /* 33 */ { PortIO::Backend::Esp32, Cap::None, PortIO::PortMode::Input, PortIO::PinType::System, false, false, { 0, 0, 0, 0 }, { .esp = {  7, false } } },

        // Buzzer
        /* 34 */ { PortIO::Backend::Esp32, ESP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Buzzer, false, false, { 0, 0, 0, 0 }, { .esp = { 38, false } } },
        
        // Status LED
        /* 35 */ { PortIO::Backend::Esp32, ESP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Led, false, false, { 0, 0, 0, 0 }, { .esp = { 39, false } } },
        
        // Net LED
        /* 36 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Led, false, false, { 0, 0, 0, 0 }, { .ext = { 1, 7, false } } },
        
        // Fan
        /* 37 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Fan, false, false, { 0, 0, 0, 0 }, { .ext = { 1, 1, false } } },

        // Alarm LED
        /* 38 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Led, false, false, { 0, 0, 0, 0 }, { .ext = { 1, 5, false } } },

        // Up Button
        /* 39 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::Button, false, false, { 0, 0, 0, 0 }, { .ext = { 1, 4, false } } },

        // Ok Button
        /* 40 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::Button, false, false, { 0, 0, 0, 0 }, { .ext = { 1, 3, false } } },

        // Down Button
        /* 41 */ { PortIO::Backend::Extender, MCP_GPIO_PU, PortIO::PortMode::InputPullUp, PortIO::PinType::Button, false, false, { 0, 0, 0, 0 }, { .ext = { 1, 2, false } } },

        // LCD Backlight
        /* 42 */ { PortIO::Backend::Extender, MCP_GPIO_OUT, PortIO::PortMode::Output, PortIO::PinType::Led, false, false, { 0, 0, 0, 0 }, { .ext = { 1, 0, false } } },
    }};

    template <uint8_t P>
    static constexpr Cap capsOf()
    {
        static_assert(P < PortIO::PORT_COUNT, "Port out of range");
        return PORTS[P].caps;
    }
};
