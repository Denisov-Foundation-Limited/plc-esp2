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

#include "utils/users_registry.hpp"

#include <string.h>
#include "mbedtls/sha256.h"
#if defined(ESP32)
#include <esp_system.h>
#endif

using UsersAclController = UsersRegistry::AclController;
using UsersUser = UsersRegistry::User;
constexpr size_t UsersKaclBytes = UsersRegistry::kAclBytes;

void UsersRegistry::User::clearAcl(){
        for (size_t i = 0; i < acl_bits.size(); ++i)
            acl_bits[i] = 0;
    }

void UsersRegistry::User::grantAllAcl(){
        for (size_t i = 0; i < acl_bits.size(); ++i)
            acl_bits[i] = 0xFF;
        const size_t tail_bits = acl_bits.size() * 8u - kAclTotalBits;
        if (tail_bits > 0)
        {
            const uint8_t keep = (uint8_t)(8u - tail_bits);
            const uint8_t mask = (uint8_t)((1u << keep) - 1u);
            acl_bits[acl_bits.size() - 1] &= mask;
        }
    }

bool UsersRegistry::User::controllerAllowed(uint8_t unit, UsersAclController ctrl) const{
        const size_t bit = UsersRegistry::controllerBitIndex_(unit, ctrl);
        return UsersRegistry::getAclBit_(acl_bits, bit);
    }

bool UsersRegistry::User::canViewItem(uint8_t unit, UsersAclController ctrl, uint16_t item_id) const{
        if (!controllerAllowed(unit, ctrl))
            return false;
        size_t bit = 0;
        if (!UsersRegistry::itemViewBitIndex_(unit, ctrl, item_id, bit))
            return false;
        return UsersRegistry::getAclBit_(acl_bits, bit);
    }

bool UsersRegistry::User::itemViewAllowedRaw(uint8_t unit, UsersAclController ctrl, uint16_t item_id) const{
        size_t bit = 0;
        if (!UsersRegistry::itemViewBitIndex_(unit, ctrl, item_id, bit))
            return false;
        return UsersRegistry::getAclBit_(acl_bits, bit);
    }

bool UsersRegistry::User::canControlItem(uint8_t unit, UsersAclController ctrl, uint16_t item_id) const{
        if (!controllerAllowed(unit, ctrl))
            return false;
        size_t bit = 0;
        if (!UsersRegistry::itemControlBitIndex_(unit, ctrl, item_id, bit))
            return false;
        return UsersRegistry::getAclBit_(acl_bits, bit);
    }

bool UsersRegistry::User::itemControlAllowedRaw(uint8_t unit, UsersAclController ctrl, uint16_t item_id) const{
        size_t bit = 0;
        if (!UsersRegistry::itemControlBitIndex_(unit, ctrl, item_id, bit))
            return false;
        return UsersRegistry::getAclBit_(acl_bits, bit);
    }

bool UsersRegistry::User::setControllerAllowed(uint8_t unit, UsersAclController ctrl, bool allow){
        const size_t bit = UsersRegistry::controllerBitIndex_(unit, ctrl);
        return UsersRegistry::setAclBit_(acl_bits, bit, allow);
    }

bool UsersRegistry::User::setItemView(uint8_t unit, UsersAclController ctrl, uint16_t item_id, bool allow){
        size_t bit = 0;
        if (!UsersRegistry::itemViewBitIndex_(unit, ctrl, item_id, bit))
            return false;
        return UsersRegistry::setAclBit_(acl_bits, bit, allow);
    }

bool UsersRegistry::User::setItemControl(uint8_t unit, UsersAclController ctrl, uint16_t item_id, bool allow){
        size_t bit = 0;
        if (!UsersRegistry::itemControlBitIndex_(unit, ctrl, item_id, bit))
            return false;
        return UsersRegistry::setAclBit_(acl_bits, bit, allow);
    }

bool UsersRegistry::User::hasWebPassword() const{
        return web_password_hash.length() == 64 && web_password_salt.length() >= 16;
    }

bool UsersRegistry::User::setWebPassword(const String &password){
        if (password.length() == 0)
            return false;
        const String salt = UsersRegistry::generateSaltHex_(16);
        if (salt.length() == 0)
            return false;
        const String hash = UsersRegistry::sha256HexSalted_(password, salt);
        if (hash.length() != 64)
            return false;
        web_password_salt = salt;
        web_password_hash = hash;
        return true;
    }

bool UsersRegistry::User::checkWebPassword(const String &password) const{
        if (!hasWebPassword())
            return false;
        const String hash = UsersRegistry::sha256HexSalted_(password, web_password_salt);
        if (hash.length() != 64)
            return false;
        return hash.equalsIgnoreCase(web_password_hash);
    }

void UsersRegistry::User::clearWebPassword(){
        web_password_hash = "";
        web_password_salt = "";
    }

UsersRegistry::UsersRegistry(){
    for (size_t i = 0; i < kMaxUsers; ++i)
    {
        _users[i].id = (uint8_t)(i + 1);
        _users[i].grantAllAcl();
    }
}

size_t UsersRegistry::size() const{ return kMaxUsers; }

const UsersUser &UsersRegistry::user(size_t idx) const{ return _users[idx]; }

UsersUser &UsersRegistry::user(size_t idx){ return _users[idx]; }

void UsersRegistry::clear(){
    for (size_t i = 0; i < kMaxUsers; ++i)
    {
        User &u = _users[i];
        const uint8_t id = u.id;
        u = User{};
        u.id = id;
        u.grantAllAcl();
    }
}

bool UsersRegistry::applyFromJson(JsonArrayConst arr){
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
        if (obj["web_password_hash"].is<const char *>())
            u.web_password_hash = obj["web_password_hash"].as<const char *>();
        if (obj["web_password_salt"].is<const char *>())
            u.web_password_salt = obj["web_password_salt"].as<const char *>();
        u.web_password_hash = normalizeHex(u.web_password_hash, 64);
        u.web_password_salt = normalizeHex(u.web_password_salt, 64);
        if (u.web_password_hash.length() != 64 || u.web_password_salt.length() < 16)
        {
            u.web_password_hash = "";
            u.web_password_salt = "";
            if (obj["web_password"].is<const char *>())
            {
                const String legacy = obj["web_password"].as<const char *>();
                if (legacy.length() > 0)
                    u.setWebPassword(legacy);
            }
        }
        if (obj["tg_username"].is<const char *>())
            u.tg_username = normalizeTgUsername(obj["tg_username"].as<const char *>());
        if (obj["tg_chat_id"].is<int64_t>())
            u.tg_chat_id = obj["tg_chat_id"].as<int64_t>();
        else if (obj["tg_chat_id"].is<long long>())
            u.tg_chat_id = (int64_t)obj["tg_chat_id"].as<long long>();
        if (obj["is_admin"].is<bool>())
            u.tg_admin = obj["is_admin"].as<bool>();
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
        bool acl_loaded = false;
        if (obj["acl_b64"].is<const char *>())
            acl_loaded = decodeAclBase64(obj["acl_b64"].as<const char *>(), u.acl_bits);
        if (!acl_loaded)
            u.grantAllAcl();
        ++seq;
    }
    return true;
}

void UsersRegistry::serializeToJson(JsonArray out) const{
    for (size_t i = 0; i < kMaxUsers; ++i)
    {
        const User &u = _users[i];
        if (!u.enabled)
            continue;
        JsonObject obj = out.add<JsonObject>();
        obj["id"] = (unsigned)u.id;
        obj["enabled"] = u.enabled;
        if (u.username.length())
            obj["username"] = u.username;
        if (u.web_password_hash.length())
            obj["web_password_hash"] = u.web_password_hash;
        if (u.web_password_salt.length())
            obj["web_password_salt"] = u.web_password_salt;
        if (u.tg_username.length())
            obj["tg_username"] = u.tg_username;
        if (u.tg_chat_id != 0)
            obj["tg_chat_id"] = (long long)u.tg_chat_id;
        obj["is_admin"] = u.tg_admin;
        obj["tg_notify"] = u.tg_notify;
        if (u.gsm_phone.length())
            obj["gsm_phone"] = u.gsm_phone;
        obj["gsm_call"] = u.gsm_call;
        obj["gsm_sms"] = u.gsm_sms;
        if (u.ibutton_key.length())
            obj["ibutton_key"] = u.ibutton_key;
        if (u.rfid_key.length())
            obj["rfid_key"] = u.rfid_key;
        obj["acl_b64"] = encodeAclBase64(u.acl_bits);
    }
}

String UsersRegistry::normalizeTgUsername(String user){
    user.trim();
    if (user.startsWith("@"))
        user.remove(0, 1);
    user.toLowerCase();
    return user;
}

String UsersRegistry::normalizeUsername(String user){
    user.trim();
    user.toLowerCase();
    return user;
}

String UsersRegistry::normalizeHex(const String &in, size_t max_len){
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

String UsersRegistry::normalizePhone(const String &in){
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

String UsersRegistry::sha256HexSalted_(const String &password, const String &salt_hex){
    const String payload = salt_hex + ":" + password;
    uint8_t hash[32] = {};
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, (const unsigned char *)payload.c_str(), payload.length());
    mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    return bytesToHex_(hash, sizeof(hash));
}

String UsersRegistry::bytesToHex_(const uint8_t *data, size_t len){
    static const char kHex[] = "0123456789ABCDEF";
    if (!data || len == 0)
        return "";
    String out;
    out.reserve(len * 2u);
    for (size_t i = 0; i < len; ++i)
    {
        const uint8_t b = data[i];
        out += kHex[(b >> 4) & 0x0F];
        out += kHex[b & 0x0F];
    }
    return out;
}

uint32_t UsersRegistry::rand32_(){
#if defined(ESP32)
    return esp_random();
#else
    uint32_t r = (uint32_t)random(0x7FFFFFFF);
    r = (r << 1) ^ (uint32_t)micros();
    return r;
#endif
}

String UsersRegistry::generateSaltHex_(size_t bytes){
    if (bytes == 0)
        return "";
    String out;
    out.reserve(bytes * 2u);
    static const char kHex[] = "0123456789ABCDEF";
    uint32_t pool = 0;
    uint8_t pool_left = 0;
    for (size_t i = 0; i < bytes; ++i)
    {
        if (pool_left == 0)
        {
            pool = rand32_();
            pool_left = 4;
        }
        const uint8_t b = (uint8_t)(pool & 0xFFu);
        pool >>= 8;
        --pool_left;
        out += kHex[(b >> 4) & 0x0F];
        out += kHex[b & 0x0F];
    }
    return out;
}

size_t UsersRegistry::controllerItemsCount_(UsersAclController ctrl){
    const size_t idx = (size_t)ctrl;
    if (idx >= kAclItemsPerController.size())
        return 0;
    return kAclItemsPerController[idx];
}

size_t UsersRegistry::controllerItemsOffset_(UsersAclController ctrl){
    const size_t idx = (size_t)ctrl;
    if (idx >= kAclItemsPerController.size())
        return kAclItemsPerUnit;
    size_t off = 0;
    for (size_t i = 0; i < idx; ++i)
        off += kAclItemsPerController[i];
    return off;
}

size_t UsersRegistry::controllerBitIndex_(uint8_t unit, UsersAclController ctrl){
    return (size_t)unit * kAclControllerCount + (size_t)ctrl;
}

bool UsersRegistry::itemViewBitIndex_(uint8_t unit, UsersAclController ctrl, uint16_t item_id, size_t &out){
    if (unit >= kAclUnitCount)
        return false;
    if (item_id == 0)
        return false;
    const size_t count = controllerItemsCount_(ctrl);
    if (count == 0 || item_id > count)
        return false;
    const size_t unit_off = (size_t)unit * kAclItemsPerUnit;
    const size_t ctrl_off = controllerItemsOffset_(ctrl);
    out = kAclControllerBits + unit_off + ctrl_off + (size_t)(item_id - 1u);
    return out < (kAclControllerBits + kAclItemViewBits);
}

bool UsersRegistry::itemControlBitIndex_(uint8_t unit, UsersAclController ctrl, uint16_t item_id, size_t &out){
    size_t view_bit = 0;
    if (!itemViewBitIndex_(unit, ctrl, item_id, view_bit))
        return false;
    out = kAclControllerBits + kAclItemViewBits + (view_bit - kAclControllerBits);
    return out < kAclTotalBits;
}

bool UsersRegistry::getAclBit_(const std::array<uint8_t, UsersKaclBytes> &bits, size_t bit_index){
    if (bit_index >= kAclTotalBits)
        return false;
    const size_t byte_idx = bit_index >> 3;
    const uint8_t mask = (uint8_t)(1u << (bit_index & 7u));
    return (bits[byte_idx] & mask) != 0;
}

bool UsersRegistry::setAclBit_(std::array<uint8_t, UsersKaclBytes> &bits, size_t bit_index, bool value){
    if (bit_index >= kAclTotalBits)
        return false;
    const size_t byte_idx = bit_index >> 3;
    const uint8_t mask = (uint8_t)(1u << (bit_index & 7u));
    if (value)
        bits[byte_idx] |= mask;
    else
        bits[byte_idx] &= (uint8_t)~mask;
    return true;
}

char UsersRegistry::b64Char_(uint8_t v){
    static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    return kB64[v & 0x3Fu];
}

int8_t UsersRegistry::b64Index_(char c){
    if (c >= 'A' && c <= 'Z')
        return (int8_t)(c - 'A');
    if (c >= 'a' && c <= 'z')
        return (int8_t)(26 + (c - 'a'));
    if (c >= '0' && c <= '9')
        return (int8_t)(52 + (c - '0'));
    if (c == '+' || c == '-')
        return 62;
    if (c == '/' || c == '_')
        return 63;
    return -1;
}

String UsersRegistry::encodeAclBase64(const std::array<uint8_t, UsersKaclBytes> &bits){
    String out;
    out.reserve(((bits.size() + 2u) / 3u) * 4u);
    size_t i = 0;
    while (i < bits.size())
    {
        const uint8_t b0 = bits[i++];
        const bool has_b1 = i < bits.size();
        const uint8_t b1 = has_b1 ? bits[i++] : 0;
        const bool has_b2 = i < bits.size();
        const uint8_t b2 = has_b2 ? bits[i++] : 0;
        const uint32_t x = ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | (uint32_t)b2;
        out += b64Char_((uint8_t)((x >> 18) & 0x3F));
        out += b64Char_((uint8_t)((x >> 12) & 0x3F));
        out += has_b1 ? b64Char_((uint8_t)((x >> 6) & 0x3F)) : '=';
        out += has_b2 ? b64Char_((uint8_t)(x & 0x3F)) : '=';
    }
    return out;
}

bool UsersRegistry::decodeAclBase64(const char *src, std::array<uint8_t, UsersKaclBytes> &out){
    if (!src)
        return false;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = 0;
    size_t out_pos = 0;
    uint8_t block[4] = {};
    uint8_t block_len = 0;
    for (size_t i = 0; src[i] != '\0'; ++i)
    {
        const char c = src[i];
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t')
            continue;
        if (c == '=')
        {
            block[block_len++] = 0xFF;
        }
        else
        {
            const int8_t v = b64Index_(c);
            if (v < 0)
                return false;
            block[block_len++] = (uint8_t)v;
        }
        if (block_len < 4)
            continue;
        if (block[0] == 0xFF || block[1] == 0xFF)
            return false;
        const uint8_t a = block[0];
        const uint8_t b = block[1];
        const bool p2 = (block[2] == 0xFF);
        const bool p3 = (block[3] == 0xFF);
        const uint8_t c2 = p2 ? 0 : block[2];
        const uint8_t d = p3 ? 0 : block[3];
        const uint32_t x = ((uint32_t)a << 18) | ((uint32_t)b << 12) | ((uint32_t)c2 << 6) | (uint32_t)d;
        if (out_pos >= out.size())
            return false;
        out[out_pos++] = (uint8_t)((x >> 16) & 0xFF);
        if (!p2)
        {
            if (out_pos >= out.size())
                return false;
            out[out_pos++] = (uint8_t)((x >> 8) & 0xFF);
        }
        if (!p3)
        {
            if (out_pos >= out.size())
                return false;
            out[out_pos++] = (uint8_t)(x & 0xFF);
        }
        block_len = 0;
        block[0] = block[1] = block[2] = block[3] = 0;
        if (p2 || p3)
            break;
    }
    if (block_len != 0)
        return false;
    if (out_pos != out.size())
        return false;
    const size_t tail_bits = out.size() * 8u - kAclTotalBits;
    if (tail_bits > 0)
    {
        const uint8_t keep = (uint8_t)(8u - tail_bits);
        const uint8_t mask = (uint8_t)((1u << keep) - 1u);
        out[out.size() - 1] &= mask;
    }
    return true;
}
