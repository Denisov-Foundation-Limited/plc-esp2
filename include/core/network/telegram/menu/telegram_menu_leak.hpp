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

#include "controllers/leak_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuLeak
{
public:
    static bool cmdLeak_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdLeakList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdLeakShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdLeakAck_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static void buildLeakLabels_(TelegramMenu &self, int64_t chat_id, std::vector<String> &out)
    ;

    static String leakListTextHtml_(TelegramMenu &self, int64_t chat_id)
    ;

    static String leakZoneTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    ;

    static String leakZoneControlMarkup_()
    ;

    static void sendLeakMenu_(TelegramMenu &self, int64_t chat_id)
    ;

    static void sendLeakZone_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    ;

    static bool handleLeakAction_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool handleLeakSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool parseLeakIdFromText_(const String &text, uint8_t &out)
    ;

    static bool parseLeakId_(const String &text, uint8_t &out)
    ;

    static bool parseLeakLabel_(const String &text, uint8_t &out)
    ;
};
