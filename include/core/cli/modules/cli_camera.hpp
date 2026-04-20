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
#include <LittleFS.h>

#include "hal/camera_store.hpp"

template <typename ConsoleT>
class CLICameraT
{
public:
    explicit CLICameraT(ConsoleT &console) : _c(console) {}

    void printHelpEnable() const
    {
        _c._io->println(F("    show cameras   - list camera configs"));
        _c._io->println(F("    show camera <id> - camera details"));
    }

    void printHelpConfigLines() const
    {
        _c._io->println(F("  Cameras:"));
        _c._io->println(F("    camera                 - enter Camera context"));
    }

    void printHelpContextLines() const
    {
        _c._io->println(F("  Cameras:"));
        _c._io->println(F("    show                    - list cameras"));
        _c._io->println(F("    show <id>               - camera details"));
        _c._io->println(F("    enable <id>             - enable camera"));
        _c._io->println(F("    disable <id>            - disable camera"));
        _c._io->println(F("    name <id> <text>        - set camera name"));
        _c._io->println(F("    url <id> <value>        - set snapshot URL"));
        _c._io->println(F("    user <id> <value|clear> - set username"));
        _c._io->println(F("    password <id> <value|clear> - set password"));
    }

    void printHelpTopic() const
    {
        _c._io->println(F("Camera commands:"));
        _c._io->println(F("  camera                 - enter Camera context"));
        printHelpContextLines();
    }

    void showCameras() const
    {
        CameraConfigEntry cfgs[CameraStore::kCameraCount];
        loadOrDefaults_(cfgs);
        _c._io->println(F("Cameras:"));
        _c._io->println(F("  id  en  name"));
        for (size_t i = 0; i < CameraStore::kCameraCount; ++i)
        {
            const auto &cfg = cfgs[i];
            _c._io->print(F("  "));
            _c._io->print(String((unsigned)cfg.id));
            _c._io->print(F("   "));
            _c._io->print(cfg.enabled ? F("on  ") : F("off "));
            _c._io->println(cfg.name.length() ? cfg.name : String(F("-")));
        }
    }

    void showCamera(uint8_t id) const
    {
        CameraConfigEntry cfgs[CameraStore::kCameraCount];
        loadOrDefaults_(cfgs);
        CameraConfigEntry *cfg = nullptr;
        if (!CameraStore::find(cfgs, id, cfg) || !cfg)
        {
            _c._io->println(F("Invalid camera id"));
            return;
        }
        _c._io->println(F("Camera:"));
        _c._io->println(String(F("  id: ")) + String((unsigned)cfg->id));
        _c._io->println(String(F("  enabled: ")) + (cfg->enabled ? F("true") : F("false")));
        _c._io->println(String(F("  name: ")) + cfg->name);
        _c._io->println(String(F("  url: ")) + cfg->snapshot_url);
        _c._io->println(String(F("  user: ")) + cfg->username);
        _c._io->println(String(F("  password: ")) + (cfg->password.length() ? F("***") : F("")));
    }

    bool handleContext(const String &line)
    {
        String cmd = line;
        cmd.trim();
        String lower = cmd;
        lower.toLowerCase();

        if (lower == "show")
        {
            showCameras();
            _c.printPrompt_();
            return true;
        }
        if (lower.startsWith("show "))
        {
            String tail = cmd.substring(5);
            tail.trim();
            uint16_t id = 0;
            if (!ConsoleT::parseUint_(tail, id) || id == 0 || id > CameraStore::kCameraCount)
                _c._io->println(F("Usage: show <id>"));
            else
                showCamera((uint8_t)id);
            _c.printPrompt_();
            return true;
        }

        CameraConfigEntry cfgs[CameraStore::kCameraCount];
        loadOrDefaults_(cfgs);

        if (lower.startsWith("enable ") || lower.startsWith("disable "))
        {
            const bool enabled = lower.startsWith("enable ");
            String tail = cmd.substring(enabled ? 7 : 8);
            tail.trim();
            uint16_t id = 0;
            CameraConfigEntry *cfg = nullptr;
            if (!ConsoleT::parseUint_(tail, id) || id == 0 || id > CameraStore::kCameraCount || !CameraStore::find(cfgs, (uint8_t)id, cfg) || !cfg)
                _c._io->println(F("Invalid camera id"));
            else
            {
                cfg->enabled = enabled;
                _c._io->println(save_(cfgs) ? F("OK") : F("Failed"));
            }
            _c.printPrompt_();
            return true;
        }

        if (handleTextCmd_(cmd, lower, "name", cfgs, [](CameraConfigEntry &cfg, const String &v) { cfg.name = v; }) ||
            handleTextCmd_(cmd, lower, "url", cfgs, [](CameraConfigEntry &cfg, const String &v) { cfg.snapshot_url = v; }) ||
            handleTextCmd_(cmd, lower, "user", cfgs, [](CameraConfigEntry &cfg, const String &v) { cfg.username = (v == "clear" ? String() : v); }) ||
            handleTextCmd_(cmd, lower, "password", cfgs, [](CameraConfigEntry &cfg, const String &v) { cfg.password = (v == "clear" ? String() : v); }))
        {
            _c.printPrompt_();
            return true;
        }
        return false;
    }

private:
    template <typename FnT>
    bool handleTextCmd_(const String &cmd, const String &lower, const char *name, CameraConfigEntry cfgs[CameraStore::kCameraCount], FnT fn)
    {
        String prefix = String(name) + " ";
        if (!lower.startsWith(prefix))
            return false;
        String rest = cmd.substring(prefix.length());
        rest.trim();
        const int sp = rest.indexOf(' ');
        if (sp <= 0)
        {
            _c._io->print(F("Usage: "));
            _c._io->print(name);
            _c._io->println(F(" <id> <value>"));
            return true;
        }
        String id_str = rest.substring(0, sp);
        String value = rest.substring(sp + 1);
        id_str.trim();
        value.trim();
        uint16_t id = 0;
        CameraConfigEntry *cfg = nullptr;
        if (!ConsoleT::parseUint_(id_str, id) || id == 0 || id > CameraStore::kCameraCount || !CameraStore::find(cfgs, (uint8_t)id, cfg) || !cfg)
        {
            _c._io->println(F("Invalid camera id"));
            return true;
        }
        fn(*cfg, value);
        _c._io->println(save_(cfgs) ? F("OK") : F("Failed"));
        return true;
    }

    static void loadOrDefaults_(CameraConfigEntry cfgs[CameraStore::kCameraCount])
    {
        if (!CameraStore::load(LittleFS, cfgs))
            CameraStore::setDefaults(cfgs);
    }

    static bool save_(CameraConfigEntry cfgs[CameraStore::kCameraCount])
    {
        for (size_t i = 0; i < CameraStore::kCameraCount; ++i)
        {
            if (!cfgs[i].name.length())
                cfgs[i].name = String(F("Камера #")) + String((unsigned)cfgs[i].id);
        }
        return CameraStore::save(LittleFS, cfgs);
    }

    ConsoleT &_c;
};
