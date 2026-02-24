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

class TelegramMenuAvr
{
public:
    static bool cmdAvr_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdAvrStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static const char *sourceRuByName_(const String &src)
    ;

    static const char *sourceRu_(AvrController::Source src)
    ;

    static const char *faultRuByName_(const String &fault)
    ;

    static const char *faultRu_(AvrController::Fault fault)
    ;

    static String avrStatusTextHtml_(TelegramMenu &self, int64_t chat_id)
    ;

    static String avrControlMarkup_(TelegramMenu &self, int64_t chat_id)
    ;

    static void sendAvrMenu_(TelegramMenu &self, int64_t chat_id)
    ;

    static bool handleAvrSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;
};
