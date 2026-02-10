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
#include <ArduinoJson.h>
#include <array>

class UsersRegistry
{
public:
    static constexpr size_t kMaxUsers = 20;

    struct User
    {
        uint8_t id = 1;
        bool enabled = false;
        String username;
        String tg_username;
        int64_t tg_chat_id = 0;
        bool tg_admin = false;
        bool tg_notify = false;
        String gsm_phone;
        bool gsm_call = false;
        bool gsm_sms = false;
        String ibutton_key;
        String rfid_key;
    };

    UsersRegistry()
    {
        for (size_t i = 0; i < kMaxUsers; ++i)
            _users[i].id = (uint8_t)(i + 1);
    }

    size_t size() const { return kMaxUsers; }
    const User &user(size_t idx) const { return _users[idx]; }
    User &user(size_t idx) { return _users[idx]; }

    void clear()
    {
        for (size_t i = 0; i < kMaxUsers; ++i)
        {
            User &u = _users[i];
            const uint8_t id = u.id;
            u = User{};
            u.id = id;
        }
    }

    bool applyFromJson(JsonArrayConst arr)
    {
        clear();
        size_t seq = 0;
        for (JsonVariantConst v : arr)
        {
            if (!v.is<JsonObjectConst>())
            {
                ++seq;
                continue;
            }
            JsonObjectConst obj = v.as<JsonObjectConst>();
            size_t idx = seq;
            if (obj["id"].is<unsigned>())
            {
                const unsigned raw = obj["id"].as<unsigned>();
                if (raw >= 1 && raw <= kMaxUsers)
                    idx = (size_t)(raw - 1);
            }
            if (idx >= kMaxUsers)
            {
                ++seq;
                continue;
            }
            User &u = _users[idx];
            if (obj["enabled"].is<bool>())
                u.enabled = obj["enabled"].as<bool>();
            if (obj["username"].is<const char *>())
                u.username = obj["username"].as<const char *>();
            if (obj["tg_username"].is<const char *>())
                u.tg_username = normalizeTgUsername(obj["tg_username"].as<const char *>());
            if (obj["tg_chat_id"].is<int64_t>())
                u.tg_chat_id = obj["tg_chat_id"].as<int64_t>();
            else if (obj["tg_chat_id"].is<long long>())
                u.tg_chat_id = (int64_t)obj["tg_chat_id"].as<long long>();
            if (obj["tg_admin"].is<bool>())
                u.tg_admin = obj["tg_admin"].as<bool>();
            if (obj["tg_notify"].is<bool>())
                u.tg_notify = obj["tg_notify"].as<bool>();
            if (obj["gsm_phone"].is<const char *>())
                u.gsm_phone = normalizePhone(obj["gsm_phone"].as<const char *>());
            if (obj["gsm_call"].is<bool>())
                u.gsm_call = obj["gsm_call"].as<bool>();
            if (obj["gsm_sms"].is<bool>())
                u.gsm_sms = obj["gsm_sms"].as<bool>();
            if (obj["ibutton_key"].is<const char *>())
                u.ibutton_key = normalizeHex(obj["ibutton_key"].as<const char *>(), 16);
            if (obj["rfid_key"].is<const char *>())
                u.rfid_key = normalizeHex(obj["rfid_key"].as<const char *>(), 20);
            ++seq;
        }
        return true;
    }

    void serializeToJson(JsonArray out) const
    {
        for (size_t i = 0; i < kMaxUsers; ++i)
        {
            const User &u = _users[i];
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = (unsigned)u.id;
            obj["enabled"] = u.enabled;
            if (u.username.length())
                obj["username"] = u.username;
            if (u.tg_username.length())
                obj["tg_username"] = u.tg_username;
            if (u.tg_chat_id != 0)
                obj["tg_chat_id"] = (long long)u.tg_chat_id;
            obj["tg_admin"] = u.tg_admin;
            obj["tg_notify"] = u.tg_notify;
            if (u.gsm_phone.length())
                obj["gsm_phone"] = u.gsm_phone;
            obj["gsm_call"] = u.gsm_call;
            obj["gsm_sms"] = u.gsm_sms;
            if (u.ibutton_key.length())
                obj["ibutton_key"] = u.ibutton_key;
            if (u.rfid_key.length())
                obj["rfid_key"] = u.rfid_key;
        }
    }

    static String normalizeTgUsername(String user)
    {
        user.trim();
        if (user.startsWith("@"))
            user.remove(0, 1);
        user.toLowerCase();
        return user;
    }

    static String normalizeHex(const String &in, size_t max_len)
    {
        String out;
        out.reserve(in.length());
        for (size_t i = 0; i < in.length(); ++i)
        {
            const char c = in.charAt(i);
            const bool digit = (c >= '0' && c <= '9');
            const bool hex_l = (c >= 'a' && c <= 'f');
            const bool hex_u = (c >= 'A' && c <= 'F');
            if (!digit && !hex_l && !hex_u)
                continue;
            out += (char)toupper((unsigned char)c);
            if (max_len > 0 && out.length() >= max_len)
                break;
        }
        return out;
    }

    static String normalizePhone(const String &in)
    {
        String out;
        out.reserve(in.length());
        for (size_t i = 0; i < in.length(); ++i)
        {
            const char c = in.charAt(i);
            if (c >= '0' && c <= '9')
                out += c;
        }
        return out;
    }

private:
    std::array<User, kMaxUsers> _users{};
};
