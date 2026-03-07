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

#include "core/cli/cli_console.hpp"
#include "core/network/network.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/gsm_modem.hpp"
#include "controllers/controllers.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/users_registry.hpp"
#include "core/display_slots.hpp"
#include "plc/plc_control.hpp"
#include "core/rules_controller.hpp"

class ConfigsManager : public ConfigsManagerIface
{
public:
    static constexpr size_t kConfigDocCapacity = 32768;
    static constexpr size_t kDisplaySlotCount = 8;
    static constexpr size_t kGroupCount = 16;

    ConfigsManager(Configs &configs, WifiManager &wifi, TelegramClient &telegram,
                   Network &network, CliConsole &console, TelegramMenu &telegram_menu, PlcControl &plc,
                   Controllers &controllers, RulesController &rules, GsmModem &gsm, UsersRegistry &users);StackRole stackRole() const override;String stackMasterHost() const override;String stackApiKey() const override;bool stackFallbackEnabled() const override;String stackFallbackHost() const override;bool stackSlaveController() const override;bool cloudEnabled() const override;String cloudHost() const override;uint16_t cloudPort() const override;String cloudPath() const override;bool cloudUseSsl() const override;uint32_t cloudReconnectMs() const override;uint32_t cloudEventIntervalMs() const override;String cloudApiKey() const override;String cloudFirmwareVersion() const override;size_t groupCount() const override;bool groupByIndex(size_t idx, GroupConfig &out) const override;bool setGroup(uint8_t id, const String &name, uint16_t sort) override;bool removeGroup(uint8_t id) override;uint8_t allocateGroupId() const override;size_t displaySlotCount() const override;bool displaySlot(size_t idx, DisplaySlotConfig &out) const override;void setStackRole(StackRole role) override;void setStackMasterHost(const String &host) override;void setStackApiKey(const String &key) override;void setStackFallbackEnabled(bool enabled) override;void setStackFallbackHost(const String &host) override;void setStackSlaveController(bool controller) override;void setCloudEnabled(bool enabled) override;void setCloudHost(const String &host) override;void setCloudPort(uint16_t port) override;void setCloudPath(const String &path) override;void setCloudUseSsl(bool use_ssl) override;void setCloudReconnectMs(uint32_t ms) override;void setCloudEventIntervalMs(uint32_t ms) override;void setCloudApiKey(const String &key) override;void setCloudFirmwareVersion(const String &ver) override;void setDisplaySlot(size_t idx, const DisplaySlotConfig &slot) override;bool loadConfigs();bool save() override;bool save(const JsonDocument &doc) override;private:
    static constexpr const char *kUsersPath = "/users.json";
    static constexpr const char *kRulesPath = "/rules.json";

    bool hasAnyWebLoginUser_() const;void ensureDefaultUsers_();void ensureDefaultRules_();bool loadUsersFromFs_();bool saveUsersToFs_();bool loadRulesFromFs_();bool saveRulesToFs_();void applyConfig_(const JsonDocument &doc);void clearGroupRefs_(uint8_t group_id);static String sanitizeUtf8_(const String &in);static bool isValidUtf8_(const String &in);static void appendUtf8_(String &out, uint16_t code);static String cp1251ToUtf8_(const String &in);Configs &_configs;
    WifiManager &_wifi;
    TelegramClient &_telegram;
    Network &_network;
    CliConsole &_console;
    TelegramMenu &_telegram_menu;
    PlcControl &_plc;
    Controllers &_controllers;
    RulesController &_rules;
    GsmModem &_gsm;
    UsersRegistry &_users;
    StackRole _stack_role = StackRole::Master;
    String _stack_master_host;
    String _stack_api_key;
    bool _stack_fallback_enabled = false;
    String _stack_fallback_host;
    bool _stack_slave_controller = true;
    String _cloud_host;
    uint16_t _cloud_port = 0;
    String _cloud_path = "/";
    bool _cloud_use_ssl = false;
    uint32_t _cloud_reconnect_ms = 5000;
    String _cloud_api_key;
    String _cloud_fw_version;
    uint32_t _cloud_event_ms = 0;
    bool _cloud_enabled = false;
    GroupConfig _groups[kGroupCount]{};
    DisplaySlotConfig _display_slots[kDisplaySlotCount]{};
    DynamicJsonDocument _doc{kConfigDocCapacity};

    CloudClient::Config buildCloudConfig_() const;void applyCloudConfig_();static const char *displayKindName_(DisplaySlotKind kind);static const char *displayFieldName_(DisplaySlotField field);static DisplaySlotKind parseDisplayKind_(JsonVariantConst v);static DisplaySlotField parseDisplayField_(JsonVariantConst v, DisplaySlotKind kind);};
