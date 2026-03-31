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
#include <FS.h>
#include <stdint.h>

struct CameraConfigEntry
{
    uint8_t id = 1;
    bool enabled = false;
    String name;
    String snapshot_url;
    String username;
    String password;
};

class CameraStore
{
public:
    static constexpr size_t kCameraCount = 4;
    static constexpr const char *kConfigPath = "/cameras.json";

    static void setDefaults(CameraConfigEntry cfgs[kCameraCount]);
    static bool load(fs::FS &fs, CameraConfigEntry cfgs[kCameraCount]);
    static bool save(fs::FS &fs, const CameraConfigEntry cfgs[kCameraCount]);
    static bool find(CameraConfigEntry cfgs[kCameraCount], uint8_t id, CameraConfigEntry *&out);
    static bool getById(fs::FS &fs, uint8_t id, CameraConfigEntry &out);
    static String buildEffectiveUrl(const CameraConfigEntry &cfg);
};
