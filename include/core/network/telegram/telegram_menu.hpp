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
#include <vector>

#include "core/network/telegram/telegram_allowed_users.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "controllers/thermo_controller.hpp"
#include "controllers/tank_controller.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/users_registry.hpp"
#include "core/rules_controller.hpp"

class TelegramMenuThermo;
class TelegramMenuSockets;
class TelegramMenuMeteo;
class TelegramMenuTanks;
class TelegramMenuSeptic;
class TelegramMenuSecurity;
class TelegramMenuAvr;
class TelegramMenuLeak;
class TelegramMenuRing;
class TelegramMenuWatering;
class Configs;
class Logger;
class PlcControl;
class WifiManager;
class RTC;
class StackMaster;
class StackCache;
class SocketController;
class MeteoController;
class SepticController;
class SecurityController;
class AvrController;
class LeakController;
class RingController;
class WateringController;

class TelegramMenu : public TelegramAllowedUsersProvider
{
public:
    TelegramMenu(PlcControl &plc, WifiManager &wifi, RTC &rtc, TelegramBot &bot,
                 Configs &configs, Logger &logs, UsersRegistry &users);

    void begin();

    void setAdminPassword(const String &password);
    const String &adminPassword() const;
    void setConfigsManager(ConfigsManagerIface &mgr);
    void setStackMaster(StackMaster &master);
    void setStackCache(StackCache &cache);
    void setSockets(SocketController &sockets);
    void setMeteo(MeteoController &meteo);
    void setThermo(ThermoController &thermo);
    void setTanks(TankController &tanks);
    void setSeptic(SepticController &septic);
    void setSecurity(SecurityController &security);
    void setAvr(AvrController &avr);
    void setLeak(LeakController &leak);
    void setRing(RingController &ring);
    void setWatering(WateringController &watering);
    void setRules(RulesController &rules);

    using AllowedUser = TelegramAllowedUser;

    enum class AllowResult : uint8_t
    {
        Ok = 0,
        Invalid,
        Exists,
        Full
    };

    void setAllowedUsers(const std::vector<AllowedUser> &users);

    AllowResult addAllowedUser(const String &user);

    AllowResult addAllowedUser(const AllowedUser &user);

    bool removeAllowedUser(const String &user);

    void clearAllowedUsers();

    TelegramAllowedUsersView allowedUsers() const override;

    static constexpr size_t kMaxAllowedUsers = UsersRegistry::kMaxUsers;

private:
    friend class TelegramMenuThermo;
    friend class TelegramMenuSockets;
    friend class TelegramMenuMeteo;
    friend class TelegramMenuTanks;
    friend class TelegramMenuSeptic;
    friend class TelegramMenuSecurity;
    friend class TelegramMenuAvr;
    friend class TelegramMenuLeak;
    friend class TelegramMenuRing;
    friend class TelegramMenuWatering;
    static inline TelegramMenu *_self = nullptr;

    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    String _admin_password;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;
    UsersRegistry *_users = nullptr;
    mutable std::array<AllowedUser, kMaxAllowedUsers> _allowed_users_cache{};
    mutable size_t _allowed_users_cache_count = 0;

    struct ChatAuth
    {
        int64_t chat_id = 0;
        String user_id;
        bool authorized = false;
        bool awaiting = false;
        bool awaiting_config = false;
        bool awaiting_device = false;
        bool awaiting_socket = false;
        bool awaiting_thermo = false;
        bool awaiting_tank = false;
        bool awaiting_leak = false;
        bool awaiting_watering = false;
        uint8_t fail_count = 0;
        uint32_t lock_until_ms = 0;
        bool selected_local = true;
        uint32_t selected_node_id = 0;
        bool group_filter_active = false;
        uint8_t group_filter_id = 0;
        String group_filter_menu;
        uint8_t socket_action = 0;
        uint8_t selected_thermo_id = 0;
        uint8_t selected_tank_id = 0;
        uint8_t selected_leak_id = 0;
        uint8_t selected_watering_id = 0;
    };

    static constexpr size_t kMaxAuth = 16;
    std::array<ChatAuth, kMaxAuth> _auth{};
    size_t _auth_count = 0;
    static constexpr size_t kMaxConfigBytes = 8192;
    static constexpr size_t kConfigDocCapacity = 12288;
    DynamicJsonDocument _cfg_doc{kConfigDocCapacity};
    static constexpr uint32_t kHistoryMagic = 0x4D544831u; // "MTH1"
    static constexpr float kThermoTargetStep = 1.0f;
    static constexpr float kThermoHystStep = 0.5f;
    static bool requireAdmin_(TelegramMenu &self, TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdAdmin_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdDevice_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWifi_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdStackList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSockets_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLights_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdMeteo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdThermo_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdTanks_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSeptic_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurity_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdAvr_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdAvrStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLeak_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLeakList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLeakShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLeakAck_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdRing_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdRingOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdRingOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWatering_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWateringList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWateringShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSepticStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSepticList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSepticMonitor_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurityStatus_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurityList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurityArm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecurityDisarm_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSecuritySilent_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSocketList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdMeteoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdMeteoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdThermoList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdTanksList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdThermoShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdTanksShow_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSocketOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSocketOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdSocketToggle_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWifiRestart_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);
    static bool cmdPlcRestart_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWifiApOn_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWifiApOff_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdWifiStaAp_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdLogs_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdConfigSet_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdAllowList_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdAllowAdd_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdAllowDel_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdAllowClear_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool cmdTime_(TelegramBot &bot, const TelegramClient::Update &u, String &reply);

    static bool handleDocument_(TelegramMenu &self, const TelegramClient::Update &u);

    static bool onText_(void *ctx, const TelegramClient::Update &u);

    const ChatAuth *findAuth_(int64_t chat_id) const;

    ChatAuth *findAuth_(int64_t chat_id);

    ChatAuth *ensureAuth_(int64_t chat_id);

    void resetAuth_(int64_t chat_id);

    void resetAwaiting_(int64_t chat_id);

    const StackCache::StackGroupsCache *selectedGroupsCache_(int64_t chat_id) const;

    bool hasLocalGroups_() const;

    bool groupById_(uint8_t group_id, ConfigsManagerIface::GroupConfig &out) const;

    bool hasGroups_(int64_t chat_id) const;

    bool groupById_(int64_t chat_id, uint8_t group_id, ConfigsManagerIface::GroupConfig &out) const;

    void clearGroupFilter_(int64_t chat_id, const char *menu_id = nullptr);

    void setGroupFilter_(int64_t chat_id, const char *menu_id, uint8_t group_id);

    bool groupFilterActive_(int64_t chat_id, const char *menu_id) const;

    uint8_t groupFilterId_(int64_t chat_id, const char *menu_id) const;

    bool parseGroupLabel_(const String &label, uint8_t &group_id) const;

    bool parseGroupLabel_(int64_t chat_id, const String &label, uint8_t &group_id) const;

    String groupLabelById_(uint8_t group_id) const;

    String groupLabelById_(int64_t chat_id, uint8_t group_id) const;

    TelegramBot *_bot = nullptr;
    Logger *_logs = nullptr;
    StackMaster *_stack_master = nullptr;
    StackCache *_stack_cache = nullptr;
    SocketController *_sockets = nullptr;
    MeteoController *_meteo = nullptr;
    ThermoController *_thermo = nullptr;
    TankController *_tanks = nullptr;
    SepticController *_septic = nullptr;
    SecurityController *_security = nullptr;
    AvrController *_avr = nullptr;
    LeakController *_leak = nullptr;
    RingController *_ring = nullptr;
    WateringController *_watering = nullptr;
    RulesController *_rules = nullptr;

    static inline const std::array<TelegramBot::MenuItem, 0> kRootItems = {};

    static inline const std::array<TelegramBot::MenuItem, 13> kDeviceItems = {{
        { "Админка", "Админка", nullptr, nullptr },
        { "Розетки", "/sockets", nullptr, nullptr },
        { "Свет", "/lights", nullptr, nullptr },
        { "Метео", "/meteo", nullptr, nullptr },
        { "Термо", "/thermo", nullptr, nullptr },
        { "Баки", "/tanks", nullptr, nullptr },
        { "Септик", "/septic", nullptr, nullptr },
        { "Охрана", "/security", nullptr, nullptr },
        { "АВР", "/avr", nullptr, nullptr },
        { "Leak", "/leak", nullptr, nullptr },
        { "Звонок", "/ring", nullptr, nullptr },
        { "Полив", "/watering", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 0> kSocketsItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kMeteoItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kThermoItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kTanksItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kSepticItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kSecurityItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kAvrItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kLeakItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kRingItems = {};
    static inline const std::array<TelegramBot::MenuItem, 0> kWateringItems = {};

    static inline const std::array<TelegramBot::MenuItem, 6> kAdminItems = {{
        { "ПЛК", nullptr, "plc", nullptr },
        { "Часы", nullptr, "rtc", nullptr },
        { "Wi-Fi", nullptr, "wifi", nullptr },
        { "Настройки", nullptr, "settings", nullptr },
        { "Логи", "/logs", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 2> kPlcItems = {{
        { "Статус", "/status", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 2> kRtcItems = {{
        { "Время", "/time", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 2> kWifiItems = {{
        { "Состояние", "/wifi", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::MenuItem, 7> kSettingsItems = {{
        { "Перезапуск Wi-Fi", "/wifi_restart", nullptr, nullptr },
        { "Перезапуск ПЛК", "/plc_restart", nullptr, nullptr },
        { "Wi-Fi AP Вкл", "/wifi_ap_on", nullptr, nullptr },
        { "Wi-Fi AP Выкл", "/wifi_ap_off", nullptr, nullptr },
        { "Wi-Fi STA+AP", "/wifi_sta_ap", nullptr, nullptr },
        { "Startup-config", "/config_set", nullptr, nullptr },
        { "Назад", "/back", nullptr, nullptr },
    }};

    static inline const std::array<TelegramBot::Menu, 18> kMenus = {{
        { "root", "Выбор устройства", kRootItems.data(), kRootItems.size(), nullptr },
        { "device", "Меню устройства", kDeviceItems.data(), kDeviceItems.size(), "root" },
        { "sockets", "Розетки", kSocketsItems.data(), kSocketsItems.size(), "device" },
        { "lights", "Свет", kSocketsItems.data(), kSocketsItems.size(), "device" },
        { "meteo", "Метео", kMeteoItems.data(), kMeteoItems.size(), "device" },
        { "thermo", "Термо", kThermoItems.data(), kThermoItems.size(), "device" },
        { "tanks", "Баки", kTanksItems.data(), kTanksItems.size(), "device" },
        { "septic", "Септик", kSepticItems.data(), kSepticItems.size(), "device" },
        { "security", "Охрана", kSecurityItems.data(), kSecurityItems.size(), "device" },
        { "avr", "АВР", kAvrItems.data(), kAvrItems.size(), "device" },
        { "leak", "Leak", kLeakItems.data(), kLeakItems.size(), "device" },
        { "ring", "Звонок", kRingItems.data(), kRingItems.size(), "device" },
        { "watering", "Полив", kWateringItems.data(), kWateringItems.size(), "device" },
        { "admin", "Админка", kAdminItems.data(), kAdminItems.size(), "device" },
        { "plc", "ПЛК", kPlcItems.data(), kPlcItems.size(), "admin" },
        { "rtc", "Часы", kRtcItems.data(), kRtcItems.size(), "admin" },
        { "wifi", "Wi-Fi", kWifiItems.data(), kWifiItems.size(), "admin" },
        { "settings", "Настройки", kSettingsItems.data(), kSettingsItems.size(), "admin" },
    }};

    static inline const std::array<TelegramBot::Command, 54> kCommands = {{
        { "Админка", &TelegramMenu::cmdAdmin_ },
        { "/status", &TelegramMenu::cmdStatus_ },
        { "/wifi", &TelegramMenu::cmdWifi_ },
        { "/time", &TelegramMenu::cmdTime_ },
        { "/wifi_restart", &TelegramMenu::cmdWifiRestart_ },
        { "/plc_restart", &TelegramMenu::cmdPlcRestart_ },
        { "/wifi_ap_on", &TelegramMenu::cmdWifiApOn_ },
        { "/wifi_ap_off", &TelegramMenu::cmdWifiApOff_ },
        { "/wifi_sta_ap", &TelegramMenu::cmdWifiStaAp_ },
        { "/config_set", &TelegramMenu::cmdConfigSet_ },
        { "/allow_list", &TelegramMenu::cmdAllowList_ },
        { "/allow_add", &TelegramMenu::cmdAllowAdd_ },
        { "/allow_del", &TelegramMenu::cmdAllowDel_ },
        { "/allow_clear", &TelegramMenu::cmdAllowClear_ },
        { "/logs", &TelegramMenu::cmdLogs_ },
        { "/device", &TelegramMenu::cmdDevice_ },
        { "/stack_list", &TelegramMenu::cmdStackList_ },
        { "/sockets", &TelegramMenu::cmdSockets_ },
        { "/lights", &TelegramMenu::cmdLights_ },
        { "/socket_list", &TelegramMenu::cmdSocketList_ },
        { "/socket_on", &TelegramMenu::cmdSocketOn_ },
        { "/socket_off", &TelegramMenu::cmdSocketOff_ },
        { "/socket_toggle", &TelegramMenu::cmdSocketToggle_ },
        { "/meteo", &TelegramMenu::cmdMeteo_ },
        { "/meteo_list", &TelegramMenu::cmdMeteoList_ },
        { "/meteo_show", &TelegramMenu::cmdMeteoShow_ },
        { "/thermo", &TelegramMenu::cmdThermo_ },
        { "/thermo_list", &TelegramMenu::cmdThermoList_ },
        { "/thermo_show", &TelegramMenu::cmdThermoShow_ },
        { "/tanks", &TelegramMenu::cmdTanks_ },
        { "/tanks_list", &TelegramMenu::cmdTanksList_ },
        { "/tanks_show", &TelegramMenu::cmdTanksShow_ },
        { "/septic", &TelegramMenu::cmdSeptic_ },
        { "/septic_status", &TelegramMenu::cmdSepticStatus_ },
        { "/septic_list", &TelegramMenu::cmdSepticList_ },
        { "/septic_monitor", &TelegramMenu::cmdSepticMonitor_ },
        { "/security", &TelegramMenu::cmdSecurity_ },
        { "/security_status", &TelegramMenu::cmdSecurityStatus_ },
        { "/security_list", &TelegramMenu::cmdSecurityList_ },
        { "/security_arm", &TelegramMenu::cmdSecurityArm_ },
        { "/security_disarm", &TelegramMenu::cmdSecurityDisarm_ },
        { "/security_silent", &TelegramMenu::cmdSecuritySilent_ },
        { "/avr", &TelegramMenu::cmdAvr_ },
        { "/avr_status", &TelegramMenu::cmdAvrStatus_ },
        { "/leak", &TelegramMenu::cmdLeak_ },
        { "/leak_list", &TelegramMenu::cmdLeakList_ },
        { "/leak_show", &TelegramMenu::cmdLeakShow_ },
        { "/leak_ack", &TelegramMenu::cmdLeakAck_ },
        { "/ring", &TelegramMenu::cmdRing_ },
        { "/ring_on", &TelegramMenu::cmdRingOn_ },
        { "/ring_off", &TelegramMenu::cmdRingOff_ },
        { "/watering", &TelegramMenu::cmdWatering_ },
        { "/watering_list", &TelegramMenu::cmdWateringList_ },
        { "/watering_show", &TelegramMenu::cmdWateringShow_ },
    }};

    static constexpr uint8_t kMaxFails = 3;
    static constexpr uint32_t kLockMs = 30000;

    bool isLocked_(const ChatAuth &st) const;

    static uint32_t msLeft_(uint32_t deadline_ms);

    void onFail_(ChatAuth &st);
    static String normalizeUsername_(String user);

    void rebuildAllowedUsersCache_() const;

    bool hasAllowedUsername_(const String &user) const;

    bool hasAllowedUserChatId_(int64_t chat_id) const;

    bool isAllowedUser_(const TelegramClient::Update &u) const;

    bool isAdminChat_(int64_t chat_id) const;

    bool findAllowedUserByName_(const String &name, size_t &out) const;

    bool findAllowedUserByChatId_(int64_t chat_id, size_t &out) const;

    static void parseAllowUserSpec_(const String &spec, AllowedUser &out);

    static bool parseSocketIdFromText_(const String &text, uint8_t &out);

    static bool parseSocketId_(const String &text, uint8_t &out);

    static bool parseSocketLabel_(const String &text, uint8_t &out);

    static bool parseMeteoIdFromText_(const String &text, uint8_t &out);

    static bool parseMeteoId_(const String &text, uint8_t &out);

    static bool parseMeteoLabel_(const String &text, uint8_t &out);

    static bool parseThermoIdFromText_(const String &text, uint8_t &out);

    static bool parseThermoId_(const String &text, uint8_t &out);

    static bool parseThermoLabel_(const String &text, uint8_t &out);

    static bool parseSecurityId_(const String &text, uint8_t &out);

    static bool parseSepticId_(const String &text, uint8_t &out);

    static bool parseTankIdFromText_(const String &text, uint8_t &out);

    static bool parseTankId_(const String &text, uint8_t &out);

    static bool parseTankLabel_(const String &text, uint8_t &out);

    static bool parseOnOff_(const String &text, bool &out);

    static bool startSocketAction_(TelegramBot &bot, const TelegramClient::Update &u, String &reply, uint8_t action);

    bool isLocalSelected_(int64_t chat_id) const;

    uint32_t selectedNodeId_(int64_t chat_id) const;

    const UsersRegistry::User *aclUserForChat_(int64_t chat_id) const;

    uint8_t aclUnitForChat_(int64_t chat_id) const;

    bool aclControllerAllowedForChat_(int64_t chat_id, UsersRegistry::AclController ctrl) const;
    bool quickActionsAllowedForChat_(int64_t chat_id) const;

    void buildSocketLabels_(std::vector<String> &out) const;
    void buildSocketLabels_(std::vector<String> &out, bool lights_only) const;

    void buildMeteoLabels_(std::vector<String> &out) const;

    void buildThermoLabels_(std::vector<String> &out) const;

    void buildTankLabels_(std::vector<String> &out) const;

    String socketListTextHtml_() const;

    String securityStatusText_() const;

    String septicStatusText_() const;

    String securityListTextHtml_() const;

    String septicListTextHtml_() const;

    String meteoListTextHtml_() const;

    String meteoSensorTextHtml_(uint8_t id) const;

    String meteoHistoryTextHtml_(uint8_t id) const;

    static uint8_t scaleBars_(float v, float max_v);

    static String barString_(uint8_t bars);

    String thermoListTextHtml_() const;

    String thermoDeviceTextHtml_(uint8_t id) const;

    static const char *tankLevelLabel_(const TankController::TankState &st);

    String tankListTextHtml_() const;

    String tankDeviceTextHtml_(uint8_t id) const;

    static const char *thermoModeLabel_(ThermoController::Mode mode);

    struct DeviceEntry
    {
        String label;
        bool local = true;
        uint32_t node_id = 0;
    };

    String adminPrefix_(int64_t chat_id) const;

    String selectedDeviceLabel_(int64_t chat_id) const;

    String safePlcName_() const;

    static String fallbackNodeName_(uint32_t node_id);

    String nodeNameById_(uint32_t node_id) const;

    size_t buildDeviceList_(std::vector<DeviceEntry> &out) const;

    static String escapeJson_(const String &in);

    static String escapeHtml_(const String &in);

    static String buildKeyboardMarkup_(const std::vector<String> &labels);

    static String buildRootKeyboardMarkup_(const std::vector<DeviceEntry> &devices, bool show_quick_actions);

    static String menuPrefix_(void *ctx, int64_t chat_id, const TelegramBot::Menu &menu);

    static String menuMarkup_(void *ctx, int64_t chat_id, const TelegramBot::Menu &menu);

    void sendDeviceMenu_(int64_t chat_id, const String &prefix);

    void sendSocketMenu_(int64_t chat_id);

    void sendMeteoMenu_(int64_t chat_id);

    void sendThermoMenu_(int64_t chat_id);

    void sendTanksMenu_(int64_t chat_id);

    void sendSepticMenu_(int64_t chat_id);

    void sendSecurityMenu_(int64_t chat_id);

    void sendAvrMenu_(int64_t chat_id);

    void sendLeakMenu_(int64_t chat_id);

    void sendRingMenu_(int64_t chat_id);

    void sendWateringMenu_(int64_t chat_id);

    void sendThermoDevice_(int64_t chat_id, uint8_t id);

    void sendTankDevice_(int64_t chat_id, uint8_t id);

    static String thermoControlMarkup_();

    static String tankControlMarkup_();

    String septicControlMarkup_() const;
    String septicControlMarkup_(int64_t chat_id) const;
    String avrControlMarkup_(int64_t chat_id) const;
    String leakControlMarkup_(int64_t chat_id) const;
    String ringControlMarkup_(int64_t chat_id) const;
    String wateringControlMarkup_(int64_t chat_id) const;

    static String securityControlMarkup_();

    String ruleActor_(int64_t chat_id) const;

    static bool parseRuleId_(const String &value, uint8_t &out);

    static bool parseRuleOnOff_(const String &value, bool &on);

    static String normalizeCondToken_(const String &value);

    static bool parseCondNumber_(const String &value, double &out);

    static bool evalCondOp_(const String &actual, const String &op_raw, const String &expected);

    bool tryReadSepticConditionValue_(uint8_t item_id, const String &param, String &out) const;

    bool tryReadRuleConditionActual_(const RulesController::Rule &rule, String &out) const;

    bool checkRuleCondition_(const RulesController::Rule &rule) const;

    bool runControllerRuleAction_(int64_t chat_id, const RulesController::RuleAction &a);

    bool runQuickRule_(int64_t chat_id, uint8_t rule_id);

    bool handleRootDeviceSelection_(const TelegramClient::Update &u);

    bool handleSocketToggleSelection_(const TelegramClient::Update &u);

    bool handleMeteoSelection_(const TelegramClient::Update &u);

    bool handleThermoAction_(const TelegramClient::Update &u);

    bool handleTankAction_(const TelegramClient::Update &u);

    bool handleThermoSelection_(const TelegramClient::Update &u);

    bool handleTankSelection_(const TelegramClient::Update &u);

    bool handleSepticSelection_(const TelegramClient::Update &u);

    bool handleSecuritySelection_(const TelegramClient::Update &u);

    bool handleAvrSelection_(const TelegramClient::Update &u);

    bool handleLeakSelection_(const TelegramClient::Update &u);

    bool handleLeakAction_(const TelegramClient::Update &u);

    bool handleRingSelection_(const TelegramClient::Update &u);

    bool handleWateringSelection_(const TelegramClient::Update &u);

    bool handleWateringAction_(const TelegramClient::Update &u);

    bool selectDevice_(int64_t chat_id, const String &label);
};
