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

#include "controllers/tank_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/stack/stack_master.hpp"

class TelegramMenuTanks
{
public:
    static bool cmdTanks_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdTanksList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static bool cmdTanksShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply)
    ;

    static void buildTankLabels_(TelegramMenu &self, std::vector<String> &out)
    ;

    static const char *tankLevelLabel_(const TankController::TankState &st)
    ;

    static const char *tankLevelLabelRemote_(const StackCache::StackTankItem &st)
    ;

    static uint8_t tankFillRows_(const TankController::TankState &st)
    ;

    static uint8_t tankFillRowsRemote_(const StackCache::StackTankItem &st)
    ;

    static String tankLevelArt_(uint8_t fill_rows)
    ;

    static String tankListTextHtml_(TelegramMenu &self, int64_t chat_id)
    ;

    static String tankDeviceTextHtml_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    ;

    static void sendTanksMenu_(TelegramMenu &self, int64_t chat_id)
    ;

    static void sendTankDevice_(TelegramMenu &self, int64_t chat_id, uint8_t id)
    ;

    static String tankControlMarkup_()
    ;

    static bool handleTankAction_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool handleTankSelection_(TelegramMenu &self, const TelegramClient::Update &u)
    ;

    static bool parseTankIdFromText_(const String &text, uint8_t &out)
    ;

    static bool parseTankId_(const String &text, uint8_t &out)
    ;

    static bool parseTankLabel_(const String &text, uint8_t &out)
    ;
};
