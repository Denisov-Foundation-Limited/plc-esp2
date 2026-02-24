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

#include "controllers/thermo_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuThermo
{
public:
    static bool cmdThermo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdThermoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdThermoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static void buildThermoLabels_(TelegramMenu &self, std::vector<String> &out)
    ;

    static String thermoListTextHtml_(TelegramMenu &self, int64_t chat_id)
    ;

    static String thermoDeviceTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    ;

    static const char *thermoModeLabel_(ThermoController::Mode mode)
    ;

    static void sendThermoMenu_(TelegramMenu &self, int64_t chat_id)
    ;

    static void sendThermoDevice_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    ;

    static String thermoControlMarkup_()
    ;

    static bool handleThermoAction_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool handleThermoSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool parseThermoIdFromText_(const String &text, uint8_t &out)
    ;

    static bool parseThermoId_(const String &text, uint8_t &out)
    ;

    static bool parseThermoLabel_(const String &text, uint8_t &out)
    ;
};
