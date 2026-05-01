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

#include "utils/rtos_lock.hpp"

class UsersRegistry
{
public:
    static constexpr size_t kMaxUsers = 20;
    static constexpr uint8_t kAclUnitCount = 8;

    enum class AclController : uint8_t
    {
        Sockets = 0,
        Lights,
        Meteo,
        Thermo,
        Tanks,
        Septic,
        Security,
        Watering,
        Leak,
        Avr,
        Ring
    };

    static constexpr size_t kAclControllerCount = 11;
    static constexpr std::array<uint16_t, kAclControllerCount> kAclItemsPerController = {
        72, // sockets
        72, // lights
        50, // meteo
        20, // thermo
        20, // tanks
        1,  // septic
        72, // security
        30, // watering
        16, // leak
        1,  // avr
        1   // ring
    };
    static constexpr size_t kAclItemsPerUnit = 72u + 72u + 50u + 20u + 20u + 1u + 72u + 30u + 16u + 1u + 1u;
    static constexpr size_t kAclControllerBits = (size_t)kAclUnitCount * kAclControllerCount;
    static constexpr size_t kAclItemViewBits = (size_t)kAclUnitCount * kAclItemsPerUnit;
    static constexpr size_t kAclItemControlBits = (size_t)kAclUnitCount * kAclItemsPerUnit;
    static constexpr size_t kAclTotalBits = kAclControllerBits + kAclItemViewBits + kAclItemControlBits;
    static constexpr size_t kAclBytes = (kAclTotalBits + 7u) / 8u;

    struct User
    {
        uint8_t id = 1;
        bool enabled = false;
        String username;
        String web_password_hash;
        String web_password_salt;
        String tg_username;
        int64_t tg_chat_id = 0;
        bool tg_admin = false;
        bool tg_quick_actions = true;
        String gsm_phone;
        bool gsm_call = false;
        bool gsm_sms = false;
        String ibutton_key;
        String rfid_key;
        std::array<uint8_t, kAclBytes> acl_bits{};

        void clearAcl();
        void grantAllAcl();
        bool controllerAllowed(uint8_t unit, AclController ctrl) const;
        bool canViewItem(uint8_t unit, AclController ctrl, uint16_t item_id) const;
        bool itemViewAllowedRaw(uint8_t unit, AclController ctrl, uint16_t item_id) const;
        bool canControlItem(uint8_t unit, AclController ctrl, uint16_t item_id) const;
        bool itemControlAllowedRaw(uint8_t unit, AclController ctrl, uint16_t item_id) const;
        bool setControllerAllowed(uint8_t unit, AclController ctrl, bool allow);
        bool setItemView(uint8_t unit, AclController ctrl, uint16_t item_id, bool allow);
        bool setItemControl(uint8_t unit, AclController ctrl, uint16_t item_id, bool allow);
        bool hasWebPassword() const;
        bool setWebPassword(const String &password);
        bool checkWebPassword(const String &password) const;
        void clearWebPassword();
    };

    using Guard = RtosRecursiveLock::Guard;

    UsersRegistry();
    size_t size() const;
    // Raw accessors require an external guard() held by the caller.
    const User &user(size_t idx) const;
    User &user(size_t idx);
    bool copyUser(size_t idx, User &out) const;
    template <typename FnT>
    bool updateUser(size_t idx, FnT fn)
    {
        if (idx >= kMaxUsers)
            return false;
        const auto g = guard();
        fn(_users[idx]);
        return true;
    }
    Guard guard() const;
    void clear();
    bool applyFromJson(JsonArrayConst arr);
    void serializeToJson(JsonArray out) const;
    static String normalizeTgUsername(String user);
    static String normalizeUsername(String user);
    static String normalizeHex(const String &in, size_t max_len);
    static String normalizePhone(const String &in);

private:
    static String sha256HexSalted_(const String &password, const String &salt_hex);
    static String bytesToHex_(const uint8_t *data, size_t len);
    static uint32_t rand32_();
    static String generateSaltHex_(size_t bytes);
    friend struct User;

    static size_t controllerItemsCount_(AclController ctrl);
    static size_t controllerItemsOffset_(AclController ctrl);
    static size_t controllerBitIndex_(uint8_t unit, AclController ctrl);
    static bool itemViewBitIndex_(uint8_t unit, AclController ctrl, uint16_t item_id, size_t &out);
    static bool itemControlBitIndex_(uint8_t unit, AclController ctrl, uint16_t item_id, size_t &out);
    static bool getAclBit_(const std::array<uint8_t, kAclBytes> &bits, size_t bit_index);
    static bool setAclBit_(std::array<uint8_t, kAclBytes> &bits, size_t bit_index, bool value);
    static char b64Char_(uint8_t v);
    static int8_t b64Index_(char c);
    static String encodeAclBase64(const std::array<uint8_t, kAclBytes> &bits);
    static bool decodeAclBase64(const char *src, std::array<uint8_t, kAclBytes> &out);

    std::array<User, kMaxUsers> _users{};
    mutable RtosRecursiveLock _lock;
};
