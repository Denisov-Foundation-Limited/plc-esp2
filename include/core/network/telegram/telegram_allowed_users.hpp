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
#include <stddef.h>

struct TelegramAllowedUser
{
    String username;
    int64_t chat_id = 0;
    bool is_admin = false;
    bool is_notify = false;
    bool enabled = true;
};

struct TelegramAllowedUsersView
{
    const TelegramAllowedUser *data = nullptr;
    size_t size = 0;

    bool empty() const { return size == 0; }
    const TelegramAllowedUser &operator[](size_t index) const { return data[index]; }
};

class TelegramAllowedUsersProvider
{
public:
    virtual TelegramAllowedUsersView allowedUsers() const = 0;
protected:
    ~TelegramAllowedUsersProvider() = default;
};
