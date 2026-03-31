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

#include "hal/camera_store.hpp"

#include <ArduinoJson.h>

void CameraStore::setDefaults(CameraConfigEntry cfgs[kCameraCount])
{
    if (!cfgs)
        return;
    for (size_t i = 0; i < kCameraCount; ++i)
    {
        cfgs[i] = CameraConfigEntry{};
        cfgs[i].id = (uint8_t)(i + 1u);
        cfgs[i].name = String(F("Камера #")) + String((unsigned)(i + 1u));
    }
}

bool CameraStore::load(fs::FS &fs, CameraConfigEntry cfgs[kCameraCount])
{
    if (!cfgs)
        return false;
    setDefaults(cfgs);
    if (!fs.exists(kConfigPath))
        return true;
    File f = fs.open(kConfigPath, "r");
    if (!f)
        return false;
    DynamicJsonDocument doc(8192);
    const auto err = deserializeJson(doc, f);
    f.close();
    if (err || !doc["cameras"].is<JsonArrayConst>())
        return false;
    JsonArrayConst arr = doc["cameras"].as<JsonArrayConst>();
    size_t idx = 0;
    for (JsonVariantConst v : arr)
    {
        if (idx >= kCameraCount || !v.is<JsonObjectConst>())
            break;
        JsonObjectConst obj = v.as<JsonObjectConst>();
        auto &cfg = cfgs[idx];
        cfg.id = (uint8_t)(obj["id"] | (unsigned)(idx + 1u));
        cfg.enabled = obj["enabled"] | false;
        cfg.name = String(obj["name"] | cfg.name);
        cfg.snapshot_url = String(obj["snapshot_url"] | "");
        cfg.username = String(obj["username"] | "");
        cfg.password = String(obj["password"] | "");
        ++idx;
    }
    return true;
}

bool CameraStore::save(fs::FS &fs, const CameraConfigEntry cfgs[kCameraCount])
{
    if (!cfgs)
        return false;
    DynamicJsonDocument doc(8192);
    JsonArray arr = doc["cameras"].to<JsonArray>();
    for (size_t i = 0; i < kCameraCount; ++i)
    {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = cfgs[i].id;
        obj["enabled"] = cfgs[i].enabled;
        obj["name"] = cfgs[i].name;
        obj["snapshot_url"] = cfgs[i].snapshot_url;
        if (cfgs[i].username.length())
            obj["username"] = cfgs[i].username;
        if (cfgs[i].password.length())
            obj["password"] = cfgs[i].password;
    }
    File f = fs.open(kConfigPath, "w");
    if (!f)
        return false;
    const size_t written = serializeJson(doc, f);
    f.close();
    return written > 0;
}

bool CameraStore::find(CameraConfigEntry cfgs[kCameraCount], uint8_t id, CameraConfigEntry *&out)
{
    if (!cfgs)
    {
        out = nullptr;
        return false;
    }
    for (size_t i = 0; i < kCameraCount; ++i)
    {
        if (cfgs[i].id == id)
        {
            out = &cfgs[i];
            return true;
        }
    }
    out = nullptr;
    return false;
}

bool CameraStore::getById(fs::FS &fs, uint8_t id, CameraConfigEntry &out)
{
    CameraConfigEntry cfgs[kCameraCount];
    if (!load(fs, cfgs))
        return false;
    CameraConfigEntry *cfg = nullptr;
    if (!find(cfgs, id, cfg) || !cfg)
        return false;
    out = *cfg;
    return true;
}

String CameraStore::buildEffectiveUrl(const CameraConfigEntry &cfg)
{
    String url = cfg.snapshot_url;
    url.trim();
    if (!cfg.username.length())
        return url;
    const int scheme = url.indexOf(F("://"));
    if (scheme < 0)
        return url;
    if (url.indexOf('@', scheme + 3) >= 0)
        return url;
    String out;
    out.reserve(url.length() + cfg.username.length() + cfg.password.length() + 4);
    out += url.substring(0, scheme + 3);
    out += cfg.username;
    if (cfg.password.length())
    {
        out += ':';
        out += cfg.password;
    }
    out += '@';
    out += url.substring(scheme + 3);
    return out;
}
