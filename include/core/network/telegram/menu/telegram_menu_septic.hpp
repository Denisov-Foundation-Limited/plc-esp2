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

#include "controllers/septic_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuSeptic
{
public:
    static bool cmdSeptic_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSepticStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSepticList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSepticMonitor_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static String septicStatusText_(TelegramMenu &self, int64_t chat_id)
    ;

    static String septicListTextHtml_(TelegramMenu &self, int64_t chat_id)
    ;

    static String septicControlMarkup_(TelegramMenu &self, int64_t chat_id)
    ;

    static void sendSepticMenu_(TelegramMenu &self, int64_t chat_id)
    ;

    static bool handleSepticSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool parseSepticId_(const String &text, uint8_t &out)
    ;
};
