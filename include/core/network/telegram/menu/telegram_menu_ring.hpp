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

#include "controllers/ring_controller.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuRing
{
public:
    static bool cmdRing_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdRingOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdRingOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static String ringStatusText_(TelegramMenu &self, int64_t chat_id)
    ;

    static String ringControlMarkup_(TelegramMenu &self, int64_t chat_id)
    ;

    static void sendRingMenu_(TelegramMenu &self, int64_t chat_id)
    ;

    static bool handleRingSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

private:
    static bool setRingState_(TelegramMenu &self, int64_t chat_id, bool on)
    ;
};
