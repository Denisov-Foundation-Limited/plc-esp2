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
        /*  0 */ { .uart_num = 1, .tx = 28, .rx = 29, .baud = 115200 }
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
    static inline constexpr GsmCfg GSM = {
        .enabled = true,
        .uart_index = 0
    };

    // ---- Control pins ----
    static inline constexpr uint8_t BUZZER_PIN = 6;
    static inline constexpr uint8_t STATUS_PIN = 7;
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
        /*  0 */ { .bus_num = 0, .sda = 24, .scl = 25, .freq = 400000 },
        /*  1 */ { .bus_num = 1, .sda = 26, .scl = 27, .freq = 400000 }
    }};
    static inline constexpr uint8_t RFID_I2C_INDEX = 1;

    // ---- SPI ----
    static inline constexpr uint8_t SPI_COUNT = 1;
    static inline constexpr std::array<SpiCfg, SPI_COUNT> SPIS{{
        /*  0 */ { .bus_num = 0, .sck = 30, .miso = 31, .mosi = 32, .cs = 33, .freq = 10000000 }
    }};

    // ---- OneWire ----
    static inline constexpr uint8_t ONEWIRE_COUNT = 2;
    static inline constexpr std::array<OneWireCfg, ONEWIRE_COUNT> ONEWIRES{{
        /*  0 */ { .bus_id = OneWireCfg::OwType::iButton, .pin = 34, .parasite_power = false },
        /*  1 */ { .bus_id = OneWireCfg::OwType::Temp,    .pin = 35, .parasite_power = false },
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
        /*   0 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 15, .inverted = false } } },
        /*   1 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 16, .inverted = false } } },
        /*   2 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 17, .inverted = false } } },
        /*   3 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 18, .inverted = false } } },
        /*   4 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 8, .inverted = false } } },
        /*   5 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_IN, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::Sensor, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 9, .inverted = false } } },
        
        // Buzzer
        /*   6 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Buzzer, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 38, .inverted = false } } },

        // Status LED
        /*   7 */ { .backend = PortIO::Backend::Esp32, .caps = ESP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Led, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 39, .inverted = false } } },

        // Onboard Relays pins [1:8]
        /*   8 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 7, .inverted = false } } },
        /*   9 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 6, .inverted = false } } },
        /*  10 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 5, .inverted = false } } },
        /*  11 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 4, .inverted = false } } },
        /*  12 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 3, .inverted = false } } },
        /*  13 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 2, .inverted = false } } },
        /*  14 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 1, .inverted = false } } },
        /*  15 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 15, .inverted = false } } },

        // Onboard Digital Inputs [1..8]
        /*  16 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 8, .inverted = false } } },
        /*  17 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 9, .inverted = false } } },
        /*  18 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 10, .inverted = false } } },
        /*  19 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 11, .inverted = false } } },
        /*  20 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 12, .inverted = false } } },
        /*  21 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 13, .inverted = false } } },
        /*  22 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 14, .inverted = false } } },
        /*  23 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Cpu, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 0, .pin = 0, .inverted = false } } },

        // I2C reserved pins (not exposed as ports)
        /*  24 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 2, .inverted = false } } },
        /*  25 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 1, .inverted = false } } },
        /*  26 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 47, .inverted = false } } },
        /*  27 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 48, .inverted = false } } },

        // UART reserved pins (not exposed as ports)
        /*  28 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 5, .inverted = false } } },
        /*  29 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 4, .inverted = false } } },

        // SPI reserved pins (not exposed as ports)
        /*  30 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 13, .inverted = false } } },
        /*  31 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 12, .inverted = false } } },
        /*  32 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 11, .inverted = false } } },
        /*  33 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 14, .inverted = false } } },

        // OneWire reserved pins (not exposed as ports)
        /*  34 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 6, .inverted = false } } },
        /*  35 */ { .backend = PortIO::Backend::Esp32, .caps = Cap::None, .mode = PortIO::PortMode::Input, .type = PortIO::PinType::System, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .esp = { .gpio = 7, .inverted = false } } },

        // Net LED
        /*  36 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Led, .location = PortIO::Location::Cpu, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 7, .inverted = false } } },

        // Fan
        /*  37 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Fan, .location = PortIO::Location::Cpu, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 1, .inverted = false } } },

        // Alarm LED
        /*  38 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Led, .location = PortIO::Location::Cpu, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 5, .inverted = false } } },

        // Up Button
        /*  39 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::Button, .location = PortIO::Location::Cpu, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 4, .inverted = false } } },

        // Ok Button
        /*  40 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::Button, .location = PortIO::Location::Cpu, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 3, .inverted = false } } },

        // Down Button
        /*  41 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::Button, .location = PortIO::Location::Cpu, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 2, .inverted = false } } },

        // LCD Backlight
        /*  42 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Led, .location = PortIO::Location::Cpu, .allow_control = false, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 1, .pin = 0, .inverted = false } } },
    
        // Ext1 pins
        /*  43 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 7, .inverted = false } } },
        /*  44 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 6, .inverted = false } } },
        /*  45 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 5, .inverted = false } } },
        /*  46 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 4, .inverted = false } } },
        /*  47 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 3, .inverted = false } } },
        /*  48 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 2, .inverted = false } } },
        /*  49 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 1, .inverted = false } } },
        /*  50 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 15, .inverted = false } } },
        /*  51 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 8, .inverted = false } } },
        /*  52 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 9, .inverted = false } } },
        /*  53 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 10, .inverted = false } } },
        /*  54 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 11, .inverted = false } } },
        /*  55 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 12, .inverted = false } } },
        /*  56 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 13, .inverted = false } } },
        /*  57 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 14, .inverted = false } } },
        /*  58 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext1, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 2, .pin = 0, .inverted = false } } },

        // Ext2 pins
        /*  59 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 7, .inverted = false } } },
        /*  60 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 6, .inverted = false } } },
        /*  61 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 5, .inverted = false } } },
        /*  62 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 4, .inverted = false } } },
        /*  63 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 3, .inverted = false } } },
        /*  64 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 2, .inverted = false } } },
        /*  65 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 1, .inverted = false } } },
        /*  66 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 15, .inverted = false } } },
        /*  67 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 8, .inverted = false } } },
        /*  68 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 9, .inverted = false } } },
        /*  69 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 10, .inverted = false } } },
        /*  70 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 11, .inverted = false } } },
        /*  71 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 12, .inverted = false } } },
        /*  72 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 13, .inverted = false } } },
        /*  73 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 14, .inverted = false } } },
        /*  74 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext2, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 3, .pin = 0, .inverted = false } } },

        // Ext3 pins
        /*  75 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 7, .inverted = false } } },
        /*  76 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 6, .inverted = false } } },
        /*  77 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 5, .inverted = false } } },
        /*  78 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 4, .inverted = false } } },
        /*  79 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 3, .inverted = false } } },
        /*  80 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 2, .inverted = false } } },
        /*  81 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 1, .inverted = false } } },
        /*  82 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 15, .inverted = false } } },
        /*  83 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 8, .inverted = false } } },
        /*  84 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 9, .inverted = false } } },
        /*  85 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 10, .inverted = false } } },
        /*  86 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 11, .inverted = false } } },
        /*  87 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 12, .inverted = false } } },
        /*  88 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 13, .inverted = false } } },
        /*  89 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 14, .inverted = false } } },
        /*  90 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext3, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 4, .pin = 0, .inverted = false } } },

        // Ext4 pins
        /*  91 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 7, .inverted = false } } },
        /*  92 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 6, .inverted = false } } },
        /*  93 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 5, .inverted = false } } },
        /*  94 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 4, .inverted = false } } },
        /*  95 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 3, .inverted = false } } },
        /*  96 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 2, .inverted = false } } },
        /*  97 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 1, .inverted = false } } },
        /*  98 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 15, .inverted = false } } },
        /*  99 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 8, .inverted = false } } },
        /* 100 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 9, .inverted = false } } },
        /* 101 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 10, .inverted = false } } },
        /* 102 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 11, .inverted = false } } },
        /* 103 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 12, .inverted = false } } },
        /* 104 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 13, .inverted = false } } },
        /* 105 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 14, .inverted = false } } },
        /* 106 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext4, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 5, .pin = 0, .inverted = false } } },

        // Ext5 pins
        /* 107 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 7, .inverted = false } } },
        /* 108 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 6, .inverted = false } } },
        /* 109 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 5, .inverted = false } } },
        /* 110 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 4, .inverted = false } } },
        /* 111 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 3, .inverted = false } } },
        /* 112 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 2, .inverted = false } } },
        /* 113 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 1, .inverted = false } } },
        /* 114 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 15, .inverted = false } } },
        /* 115 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 8, .inverted = false } } },
        /* 116 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 9, .inverted = false } } },
        /* 117 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 10, .inverted = false } } },
        /* 118 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 11, .inverted = false } } },
        /* 119 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 12, .inverted = false } } },
        /* 120 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 13, .inverted = false } } },
        /* 121 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 14, .inverted = false } } },
        /* 122 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext5, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 6, .pin = 0, .inverted = false } } },

        // Ext6 pins
        /* 123 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 7, .inverted = false } } },
        /* 124 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 6, .inverted = false } } },
        /* 125 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 5, .inverted = false } } },
        /* 126 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 4, .inverted = false } } },
        /* 127 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 3, .inverted = false } } },
        /* 128 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 2, .inverted = false } } },
        /* 129 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 1, .inverted = false } } },
        /* 130 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 15, .inverted = false } } },
        /* 131 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 8, .inverted = false } } },
        /* 132 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 9, .inverted = false } } },
        /* 133 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 10, .inverted = false } } },
        /* 134 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 11, .inverted = false } } },
        /* 135 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 12, .inverted = false } } },
        /* 136 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 13, .inverted = false } } },
        /* 137 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 14, .inverted = false } } },
        /* 138 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext6, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 7, .pin = 0, .inverted = false } } },

        // Ext7 pins
        /* 139 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 7, .inverted = false } } },
        /* 140 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 6, .inverted = false } } },
        /* 141 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 5, .inverted = false } } },
        /* 142 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 4, .inverted = false } } },
        /* 143 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 3, .inverted = false } } },
        /* 144 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 2, .inverted = false } } },
        /* 145 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 1, .inverted = false } } },
        /* 146 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 15, .inverted = false } } },
        /* 147 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 8, .inverted = false } } },
        /* 148 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 9, .inverted = false } } },
        /* 149 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 10, .inverted = false } } },
        /* 150 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 11, .inverted = false } } },
        /* 151 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 12, .inverted = false } } },
        /* 152 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 13, .inverted = false } } },
        /* 153 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 14, .inverted = false } } },
        /* 154 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext7, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 8, .pin = 0, .inverted = false } } },

        // Ext8 pins
        /* 155 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 7, .inverted = false } } },
        /* 156 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 6, .inverted = false } } },
        /* 157 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 5, .inverted = false } } },
        /* 158 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 4, .inverted = false } } },
        /* 159 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 3, .inverted = false } } },
        /* 160 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 2, .inverted = false } } },
        /* 161 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 1, .inverted = false } } },
        /* 162 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_OUT, .mode = PortIO::PortMode::Output, .type = PortIO::PinType::Relay, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = false, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 15, .inverted = false } } },
        /* 163 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 8, .inverted = false } } },
        /* 164 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 9, .inverted = false } } },
        /* 165 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 10, .inverted = false } } },
        /* 166 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 11, .inverted = false } } },
        /* 167 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 12, .inverted = false } } },
        /* 168 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 13, .inverted = false } } },
        /* 169 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 14, .inverted = false } } },
        /* 170 */ { .backend = PortIO::Backend::Extender, .caps = MCP_GPIO_PU, .mode = PortIO::PortMode::InputPullUp, .type = PortIO::PinType::DInput, .location = PortIO::Location::Ext8, .allow_control = true, .initial_level = true, .pwm_enable = false, .pwm = {}, .u = { .ext = { .dev = 9, .pin = 0, .inverted = false } } },
    }};

    template <uint8_t P>
    static constexpr Cap capsOf()
    {
        static_assert(P < PortIO::PORT_COUNT, "Port out of range");
        return PORTS[P].caps;
    }
};

