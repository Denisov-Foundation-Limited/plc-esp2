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

#include <Arduino.h>
#include <vector>

struct TelegramAllowedUser
{
    String username;
    int64_t chat_id = 0;
    bool is_admin = false;
    bool is_notify = false;
    bool enabled = true;
};

class TelegramAllowedUsersProvider
{
public:
    virtual const std::vector<TelegramAllowedUser> &allowedUsers() const = 0;
protected:
    ~TelegramAllowedUsersProvider() = default;
};
