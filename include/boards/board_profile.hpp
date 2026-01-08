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

// Выбор профиля через флаг компиляции
#include "boards/profile_validator.hpp"
#if defined(BOARD_FCPLC_3V1)
#include "boards/profiles/board_profile_fcplc_3v1.hpp"
using ActiveBoardProfile = BoardProfileFCPLC_3v1;

// Compile-time safety net (pinpoint failing section)
static_assert(ProfileValidator<ActiveBoardProfile>::arrays_sane, "Profile arrays size mismatch");
static_assert(ProfileValidator<ActiveBoardProfile>::i2c_sane, "Invalid I2C config (pins/freq)");
static_assert(ProfileValidator<ActiveBoardProfile>::i2c_unique, "Duplicate I2C bus_num in I2CS[]");
static_assert(ProfileValidator<ActiveBoardProfile>::uart_sane, "Invalid UART config (pins/baud)");
static_assert(ProfileValidator<ActiveBoardProfile>::uart_unique, "Duplicate UART uart_num in UARTS[]");
static_assert(ProfileValidator<ActiveBoardProfile>::spi_sane, "Invalid SPI config (pins/freq)");
static_assert(ProfileValidator<ActiveBoardProfile>::spi_unique, "Duplicate SPI bus_num in SPIS[]");
static_assert(ProfileValidator<ActiveBoardProfile>::onewire_sane, "Invalid OneWire config (pin)");
static_assert(ProfileValidator<ActiveBoardProfile>::onewire_unique, "Duplicate OneWire bus_id in ONEWIRES[]");
static_assert(ProfileValidator<ActiveBoardProfile>::extenders_ok, "Extender config invalid or missing I2C/devices");
static_assert(ProfileValidator<ActiveBoardProfile>::ports_sane, "Invalid PORTS[] config");
static_assert(ProfileValidator<ActiveBoardProfile>::pins_ok, "Pin conflict between peripherals/ports");

#elif defined(BOARD_FCPLC_XX)
#elif defined(BOARD_FCPLC_XX)
#include "boards/progiles/fcplc_xx.hpp"
using ActiveBoardProfile = BoardProfileFCPLC_XX;

#else
#error "No Board profile defined."
#endif
