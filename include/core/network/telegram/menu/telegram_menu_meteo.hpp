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

#include <LittleFS.h>

#include "controllers/meteo_controller.hpp"
#include "utils/meteo_history.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuMeteo
{
public:
    static bool cmdMeteo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdMeteoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdMeteoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static void buildMeteoLabels_(TelegramMenu &self, std::vector<String> &out)
    ;

    static String meteoListTextHtml_(TelegramMenu &self, int64_t chat_id)
    ;

    static String meteoSensorTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    ;

    static String meteoHistoryTextHtml_(TelegramMenu &self, uint8_t id)
    ;

    static uint8_t scaleBars_(float v, float max_v)
    ;

    static String barString_(uint8_t bars)
    ;

    static void sendMeteoMenu_(TelegramMenu &self, int64_t chat_id)
    ;

    static bool handleMeteoSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool parseMeteoIdFromText_(const String &text, uint8_t &out)
    ;

    static bool parseMeteoId_(const String &text, uint8_t &out)
    ;

    static bool parseMeteoLabel_(const String &text, uint8_t &out)
    ;
};
