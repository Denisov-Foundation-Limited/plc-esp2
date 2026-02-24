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

#include "controllers/watering_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuWatering
{
public:
    static bool cmdWatering_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdWateringList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdWateringShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static void buildWateringLabels_(TelegramMenu &self, int64_t chat_id, std::vector<String> &out)
    ;

    static String wateringControlMarkup_(TelegramMenu &self, int64_t chat_id)
    ;

    static String wateringListTextHtml_(TelegramMenu &self, int64_t chat_id)
    ;

    static String wateringRuleTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    ;

    static String wateringRuleControlMarkup_()
    ;

    static void sendWateringMenu_(TelegramMenu &self, int64_t chat_id)
    ;

    static void sendWateringRule_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    ;

    static bool handleWateringAction_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool handleWateringSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool parseWateringIdFromText_(const String &text, uint8_t &out)
    ;

    static bool parseWateringId_(const String &text, uint8_t &out)
    ;

    static bool parseWateringLabel_(const String &text, uint8_t &out)
    ;
};
