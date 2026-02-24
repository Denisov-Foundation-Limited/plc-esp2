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

#include "controllers/security_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuSecurity
{
public:
    static bool cmdSecurity_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSecurityStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSecurityList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSecurityArm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSecurityDisarm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSecuritySilent_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static String securityStatusText_(TelegramMenu &self, int64_t chat_id)
    ;

    static String securityListTextHtml_(TelegramMenu &self, int64_t chat_id)
    ;

    static String securityControlMarkup_()
    ;

    static void sendSecurityMenu_(TelegramMenu &self, int64_t chat_id)
    ;

    static bool handleSecuritySelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static String userFromChat_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool parseSecurityId_(const String &text, uint8_t &out)
    ;
};


