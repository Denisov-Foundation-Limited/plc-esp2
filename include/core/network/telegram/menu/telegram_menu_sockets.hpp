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

#include "controllers/socket_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuSockets
{
public:
    static bool cmdSockets_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdLights_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSocketList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSocketOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSocketOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdSocketToggle_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool startSocketAction_(TelegramMenu &self, TelegramBot &bot, const TelegramClient::Update &u, String &reply, uint8_t action)
    ;

    static void buildSocketLabels_(TelegramMenu &self, std::vector<String> &out, bool lights_only = false)
    ;

    static String socketListTextHtml_(TelegramMenu &self, int64_t chat_id, bool lights_only = false)
    ;

    static void sendSocketMenu_(TelegramMenu &self, int64_t chat_id, bool lights_only)
    ;

    static bool handleSocketToggleSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool parseSocketIdFromText_(const String &text, uint8_t &out, uint8_t max_id)
    ;

    static bool parseSocketId_(const String &text, uint8_t &out, uint8_t max_id)
    ;

    static bool parseSocketLabel_(const String &text, uint8_t &out, uint8_t max_id)
    ;
};
