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
#include <stdint.h>
enum class Cap : uint32_t
{
    None = 0,
    GPIO = 1u << 0,
    Input = 1u << 1,
    Output = 1u << 2,
    PullUp = 1u << 3,
    PullDown = 1u << 4,
    ADC = 1u << 5,
    PWM = 1u << 6,
    InputOnly = 1u << 7,
    ISR_FAST = 1u << 8
};

constexpr inline Cap operator|(Cap a, Cap b)
{
    return (Cap)((uint32_t)a | (uint32_t)b);
}
constexpr inline Cap operator&(Cap a, Cap b)
{
    return (Cap)((uint32_t)a & (uint32_t)b);
}
constexpr inline bool has(Cap v, Cap f)
{
    return (((uint32_t)v & (uint32_t)f) != 0);
}

// Convenience groups (examples)
constexpr Cap ESP_OUT_FAST_PWM = Cap::GPIO | Cap::Output | Cap::PWM | Cap::ISR_FAST;
constexpr Cap ESP_ADC1_INONLY = Cap::GPIO | Cap::Input | Cap::ADC | Cap::InputOnly;
constexpr Cap ESP_GPIO_IN = Cap::GPIO | Cap::Input;
constexpr Cap ESP_GPIO_IN_PU = Cap::GPIO | Cap::Input | Cap::PullUp;
constexpr Cap ESP_GPIO_OUT = Cap::GPIO | Cap::Output;

// Expanders (generic)
constexpr Cap PCF_GPIO = Cap::GPIO | Cap::Input | Cap::Output;
constexpr Cap MCP_GPIO_PU = Cap::GPIO | Cap::Input | Cap::PullUp;
constexpr Cap MCP_GPIO_OUT = Cap::GPIO | Cap::Output;
