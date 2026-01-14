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
    static inline constexpr const char *UI_NAME = "FCPLC-3V1";

    // ---- logging sink ----
    static inline constexpr LogCfg LOG = {
        .sink = LogCfg::Sink::UsbSerial,
        .uart_index = 0,
        .usb_baud = 115200
    };

    // ---- UART ----
    static inline constexpr uint8_t UART_COUNT = 1;
    static inline constexpr std::array<UartCfg, UART_COUNT> UARTS{{
        /*  0 */ { .uart_num = 1, .tx = 26, .rx = 27, .baud = 115200 }
    }};

    // ---- RTC ----
    static inline constexpr RtcCfg RTC = { .bus_num = 0, .addr = 0x68 };
    static inline constexpr LcdCfg LCD = { .bus_num = 0, .addr = 0x3F };
    static inline constexpr BoardTempCfg BOARD_TEMP = { .bus_num = 0, .addr = 0x48, .hysteresis_c = 2.0f, .fan_on_c = 45.0f };
    static inline constexpr EepromCfg EEPROM = { .bus_num = 0, .addr = 0x50 };
    static inline constexpr TelegramNetCfg TELEGRAM_NET = {
        .client = TelegramNetCfg::ClientKind::WifiSecure,
        .use_proxy = false,
        .proxy_host = "",
        .proxy_port = 0,
        .proxy_path = ""
    };

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
        /*  0 */ { .bus_num = 0, .sda = 22, .scl = 23, .freq = 400000 },
        /*  1 */ { .bus_num = 1, .sda = 24, .scl = 25, .freq = 400000 }
    }};

    // ---- SPI ----
    static inline constexpr uint8_t SPI_COUNT = 1;
    static inline constexpr std::array<SpiCfg, SPI_COUNT> SPIS{{
        /*  0 */ { .bus_num = 0, .sck = 28, .miso = 29, .mosi = 30, .cs = 31, .freq = 10000000 }
    }};

    // ---- OneWire ----
    static inline constexpr uint8_t ONEWIRE_COUNT = 2;
    static inline constexpr std::array<OneWireCfg, ONEWIRE_COUNT> ONEWIRES{{
        /*  0 */ { .bus_id = OneWireCfg::OwType::iButton, .pin = 32, .parasite_power = false },
        /*  1 */ { .bus_id = OneWireCfg::OwType::Temp,    .pin = 33, .parasite_power = false },
    }};

    // ---- Extenders ----
    static inline constexpr uint8_t EXT_DEVS_COUNT = 10;
    static inline constexpr std::array<Extender::DevCfg, EXT_DEVS_COUNT> EXT_DEVS{{
        // Onboard extender
        /*  0 */ { .bus_num = 0, .i2c_addr = 0x20, .type = Extender::Type::MCP23017 },
        // Rear pannel extender
        /*  1 */ { .bus_num = 0, .i2c_addr = 0x21, .type = Extender::Type::MCP23017 },
        // Extended Units
        /*  2 */ { .bus_num = 1, .i2c_addr = 0x20, .type = Extender::Type::MCP23017 },
        /*  3 */ { .bus_num = 1, .i2c_addr = 0x21, .type = Extender::Type::MCP23017 },
        /*  4 */ { .bus_num = 1, .i2c_addr = 0x22, .type = Extender::Type::MCP23017 },
        /*  5 */ { .bus_num = 1, .i2c_addr = 0x23, .type = Extender::Type::MCP23017 },
        /*  6 */ { .bus_num = 1, .i2c_addr = 0x24, .type = Extender::Type::MCP23017 },
        /*  7 */ { .bus_num = 1, .i2c_addr = 0x25, .type = Extender::Type::MCP23017 },
        /*  8 */ { .bus_num = 1, .i2c_addr = 0x26, .type = Extender::Type::MCP23017 },
        /*  9 */ { .bus_num = 1, .i2c_addr = 0x27, .type = Extender::Type::MCP23017 }
    }};

    // ---- Ports ----
    static inline constexpr std::array<PortIO::PortDesc, PortIO::PORT_COUNT> PORTS{{
        // Onboard sensors pins for DHT22 or other sensors [1..6]
        /*  0 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 15, .inverted = false } } },
        /*  1 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 16, .inverted = false } } },
        /*  2 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 17, .inverted = false } } },
        /*  3 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 18, .inverted = false } } },
        /*  4 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 8, .inverted = false } } },
        /*  5 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 9, .inverted = false } } },

        // Onboard Relays pins [1:8]
        /*  6 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 7, .inverted = false } } },
        /*  7 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 6, .inverted = false } } },
        /*  8 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 5, .inverted = false } } },
        /*  9 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 4, .inverted = false } } },
        /* 10 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 3, .inverted = false } } },
        /* 11 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 2, .inverted = false } } },
        /* 12 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 1, .inverted = false } } },
        /* 13 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 15, .inverted = false } } },

        // Onboard Digital Inputs [1..8]
        /* 14 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 8, .inverted = false } } },
        /* 15 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 9, .inverted = false } } },
        /* 16 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 10, .inverted = false } } },
        /* 17 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 11, .inverted = false } } },
        /* 18 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 12, .inverted = false } } },
        /* 19 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 13, .inverted = false } } },
        /* 20 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 14, .inverted = false } } },
        /* 21 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 0, .inverted = false } } },

        // I2C reserved pins (not exposed as ports)
        /* 22 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 2, .inverted = false } } },
        /* 23 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 1, .inverted = false } } },
        /* 24 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 47, .inverted = false } } },
        /* 25 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 48, .inverted = false } } },

        // UART reserved pins (not exposed as ports)
        /* 26 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 5, .inverted = false } } },
        /* 27 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 4, .inverted = false } } },

        // SPI reserved pins (not exposed as ports)
        /* 28 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 13, .inverted = false } } },
        /* 29 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 12, .inverted = false } } },
        /* 30 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 11, .inverted = false } } },
        /* 31 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 14, .inverted = false } } },

        // OneWire reserved pins (not exposed as ports)
        /* 32 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 6, .inverted = false } } },
        /* 33 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 7, .inverted = false } } },

        // Buzzer
        /* 34 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Buzzer, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 38, .inverted = false } } },

        // Status LED
        /* 35 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Led, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 39, .inverted = false } } },

        // Net LED
        /* 36 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Led, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 7, .inverted = false } } },

        // Fan
        /* 37 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Fan, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 1, .inverted = false } } },

        // Alarm LED
        /* 38 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Led, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 5, .inverted = false } } },

        // Up Button
        /* 39 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::Button, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 4, .inverted = false } } },

        // Ok Button
        /* 40 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::Button, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 3, .inverted = false } } },

        // Down Button
        /* 41 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::Button, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 2, .inverted = false } } },

        // LCD Backlight
        /* 42 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Led, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 0, .inverted = false } } },
    }};

    template <uint8_t P>
    static constexpr Cap capsOf()
    {
        static_assert(P < PortIO::PORT_COUNT, "Port out of range");
        return PORTS[P].caps;
    }
};

