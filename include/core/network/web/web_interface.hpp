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
#include <LittleFS.h>
#include <WiFi.h>
#include <string.h>

#if defined(ESP32)
#include <ESPAsyncWebServer.h>
#include <Update.h>
#endif

#include "boards/board_profile.hpp"
#include "core/cli/cli_console.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/stack/stack_cache.hpp"
#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/stack/stack_slave_handler.hpp"
#include "core/network/web/interfaces/web_interface_pages.hpp"
#include "core/network/web/interfaces/web_interface_handler_fwd.hpp"
#include "core/network/web/interfaces/web_interface_texts_ru.hpp"
#include "core/network/web/web_interface_controllers_ops.hpp"
#include "core/network/web/web_interface_stack_ops.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "utils/logger.hpp"
#include "utils/configs.hpp"
#include "utils/users_registry.hpp"
#include "utils/fs_config.hpp"
#include "utils/configs_manager_iface.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "core/network/gsm_modem.hpp"
#include "core/network/cloud/cloud_client.hpp"
#include "core/display_slots.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "controllers/controllers.hpp"
#include "core/rules_controller.hpp"
#include "core/network/web/interfaces/web_interface_assets.hpp"

class WebInterfaceControllersRingHelper;

class WebInterface
{
public:
    ~WebInterface() = default;

    WebInterface(AsyncWebServer &server, CliConsole &cli, WifiManager &wifi, Configs &configs, PlcControl &plc,
                 RTC &rtc, TelegramClient &tgbot, TelegramBot &tgbot_bot, TelegramMenu &tgbot_menu, Logger &logs,
                 Extender &ext,
                 I2CManager &i2c, OneWireManager &ow, Controllers &controllers, RulesController &rules);


    bool begin(bool format_on_fail = false);


    void setAuth(const String &user, const String &pass);


    void setMaxUploadBytes(size_t bytes);


    void setAllowedExtensions(const String &exts_csv);


    void setGsmModem(GsmModem &modem);

    void setStackCache(StackCache &cache);

    void setConfigsManager(ConfigsManagerIface &mgr);

    void setUsersRegistry(UsersRegistry &users);

    void setStackMaster(StackMaster &master);

    void setStackSlave(StackSlaveHandler *slave);

    void setCloudClient(CloudClient &client);

    void setRules(RulesController &rules);


    StackCache &stackCache();

    const StackCache &stackCache() const;

    void logStackCacheAllocations();


    void registerRoutes();

private:
    friend class WebInterfaceControllersOps;
    friend class WebInterfaceStackOps;
    friend class WebInterfaceSystemRoutes;
    friend class WebInterfaceStackRoutes;
    friend class WebInterfaceDevicesRoutes;
    friend class WebInterfaceApiRoutes;
    friend class WebInterfaceControllersSocketsHelper;
    friend class WebInterfaceControllersLightsHelper;
    friend class WebInterfaceControllersSecurityHelper;
    friend class WebInterfaceControllersMeteoHelper;
    friend class WebInterfaceControllersThermoHelper;
    friend class WebInterfaceControllersSepticHelper;
    friend class WebInterfaceControllersTanksHelper;
    friend class WebInterfaceControllersWateringHelper;
    friend class WebInterfaceControllersRingHelper;
    friend class WebInterfaceControllersDisplayHelper;
    friend class ControllersHandler;
    friend class SocketsHandler;
    friend class LightsHandler;
    friend class ThermoHandler;
    friend class IndexHandler;
    friend class WifiHandler;
    friend class ManageHandler;
    friend class PortsHandler;
    friend class BusesHandler;
    friend class StackHandler;
    friend class UsersHandler;
    friend class GroupsHandler;
    friend class DisplayHandler;
    friend class AdminHandler;
    friend class LogsHandler;
    friend class StatusHandler;
    friend class SepticHandler;
    friend class RingHandler;
    friend class SecurityHandler;
    friend class TelegramHandler;
    friend class CloudHandler;
    friend class MeteoHandler;
    friend class TankHandler;
    friend class WateringHandler;
    friend class AvrHandler;
    friend class LeakHandler;
    friend class RulesHandler;
    void handleAdminSave_(AsyncWebServerRequest *request);


    String listFilesHtml_();


    static const char *extTypeName_(Extender::Type t);


    static const char *extDevTypeName_(uint8_t dev);


    static const char *portTypeName_(PortIO::PinType t);


    static const char *locationName_(PortIO::Location loc);


    static uint8_t locationIndex_(PortIO::Location loc);


    static void appendPortLabel_(String &out, PortIO::PinType type, const PortIO::PortDesc &p, uint8_t ui_id);


    static const char *owBusName_(OneWireCfg::OwType t);


    static void owAddrToHex_(const uint8_t in[8], char out[17]);


    String listPortsHtml_();


    String listExtendersHtml_();


    String listStackPortsHtml_(uint32_t node_id) const;


    String listStackExtendersHtml_(uint32_t node_id) const;

    String indexDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const;
    String busesDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const;
    String portsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const;
    String stackBusesStatusText_(uint32_t node_id) const;
    String stackPortsStatusText_(uint32_t node_id) const;
    bool isStackBusesView_(uint32_t node_id) const;
    bool isStackPortsView_(uint32_t node_id) const;
    uint32_t parseStackNodeIdParam_(AsyncWebServerRequest *request) const;
    void handleStackFrame_(uint32_t node_id, const StackFrame &frame);
    bool requestStackPorts_(uint32_t node_id);
    bool refreshStackPorts_(uint32_t node_id);
    bool requestStackExtenders_(uint32_t node_id);
    bool requestStackI2c_(uint32_t node_id, bool run);
    bool requestStackOw_(uint32_t node_id, bool run);
    bool requestStackTempSensors_(uint32_t node_id);
    bool refreshStackTempSensors_(uint32_t node_id);
    bool requestStackPlcStatus_(uint32_t node_id);
    bool requestStackRtcStatus_(uint32_t node_id);
    uint16_t nextStackCmdId_();
    String listI2cHtml_();
    String listStackI2cHtml_(uint32_t node_id) const;
    String stackNodesBlockHtml_() const;
    String listStackNodesStatusHtml_() const;
    String listStackNodesHtml_() const;
    String globalUsedPortsJson_(PortIO::PinType type) const;
    bool stackPortTypeMatch_(const StackCache::StackPortItem &it, PortIO::PinType type) const;
    String stackPortOptionsJson_(uint32_t node_id, PortIO::PinType type) const;
    String stackUsedPortsJson_(uint32_t node_id, PortIO::PinType type) const;
    String stackMeteoDs18OptionsJson_(uint32_t node_id) const;
    String stackMeteoDs18UsedJson_(uint32_t node_id) const;
    String listOwHtml_();
    String listStackOwHtml_(uint32_t node_id) const;
    void handleUpload_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                       size_t len, bool final);
    void handleOta_(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
                    bool final);
    void handleUploadDone_(AsyncWebServerRequest *request);
    void handleOtaDone_(AsyncWebServerRequest *request);
    void handleWifiSave_(AsyncWebServerRequest *request);
    void handleStackSave_(AsyncWebServerRequest *request);
    void handleStackGenKey_(AsyncWebServerRequest *request);
    void handleDeviceSave_(AsyncWebServerRequest *request);
    void handleReboot_(AsyncWebServerRequest *request);
    void handleFileDownload_(AsyncWebServerRequest *request);
    void handleDelete_(AsyncWebServerRequest *request);
    void handleUiHash_(AsyncWebServerRequest *request);
    bool hasUsersRegistryWebAuth_() const;
    bool checkLegacyAdminAuth_(const String &user, const String &pass) const;
    bool findUsersRegistryAuth_(const String &user, const String &pass, size_t &user_idx) const;
    bool checkUsersRegistryAuth_(const String &user, const String &pass) const;
    bool checkAuth_(AsyncWebServerRequest *request, bool *set_cookie, bool require_session = false);
    bool checkAuthApi_(AsyncWebServerRequest *request, bool *set_cookie);
    const UsersRegistry::User *sessionUser_() const;
    uint8_t aclUnitByNodeId_(uint32_t node_id) const;
    bool webAclControllerAllowed_(UsersRegistry::AclController ctrl, uint32_t node_id = 0) const;
    bool webAclCanViewItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id = 0) const;
    bool webAclCanControlItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint32_t node_id = 0) const;
    bool webSessionIsAdmin_() const;
    bool requireWebAdmin_(AsyncWebServerRequest *request, bool *set_cookie);
    bool requireWebAclController_(AsyncWebServerRequest *request, bool *set_cookie,
                                  UsersRegistry::AclController ctrl, uint32_t node_id = 0);
    String requestIp_(AsyncWebServerRequest *request) const;
    String wifiIp_() const;
    String wifiStaSegment_() const;
    String wifiStaStatus_() const;
    String navHtml_() const;
    String deviceName_() const;
    ConfigsManagerIface::StackRole stackRole_() const;
    String stackMasterHost_() const;
    bool stackFallbackEnabled_() const;
    String stackFallbackHost_() const;
    bool stackSlaveController_() const;
    String stackApiKey_() const;
    bool cloudEnabled_() const;
    String cloudHost_() const;
    uint16_t cloudPort_() const;
    String cloudPath_() const;
    bool cloudUseSsl_() const;
    uint32_t cloudReconnectMs_() const;
    uint32_t cloudEventMs_() const;
    String cloudApiKey_() const;
    String cloudFwVersion_() const;
    uint32_t cloudDeviceId_() const;
    bool cloudConnected_() const;
    const char *stackRoleName_(ConfigsManagerIface::StackRole role);
    bool saveWifiConfig_();
    bool isAllowedExt_(const String &path) const;
    bool parseSocketPort_(const String &input, uint8_t &out);
    const char *displaySlotKindName_(DisplaySlotKind kind) const;
    const char *displaySlotFieldName_(DisplaySlotField field) const;
    bool parseDisplaySlotKind_(const String &input, DisplaySlotKind &out);
    bool parseDisplaySlotField_(const String &input, DisplaySlotField &out);
    bool parseUint_(const String &input, uint16_t &out);
    bool parseUint_(const String &input, uint32_t &out);
    bool parseMeteoType_(const String &input, MeteoController::SensorType &out);
    MeteoController::SensorType parseMeteoTypeName_(const char *input) const;
    bool parseSecurityType_(const String &input, SecurityController::SensorType &out);
    bool parseMeteoPin_(const String &input, uint8_t &out);
    bool parseMeteoAddr_(const String &input, uint8_t out[MeteoController::kAddrLen], bool &set);
    bool parseSecurityKeyHex_(const String &s, uint8_t out[8]);
    bool parseThermoSensor_(const String &input, uint8_t &out, uint32_t &out_node);
    bool parseThermoMode_(const String &input, ThermoController::Mode &out);
    bool parseThermoFloat_(const String &input, float &out);
    bool hasGroups_(uint32_t node_id = 0) const;
    uint8_t firstGroupId_(uint32_t node_id = 0) const;
    String groupVisibilityStyleAttr_(uint8_t group_id, uint32_t node_id = 0) const;
    String groupOptionsHtml_(uint8_t selected_group_id, bool include_none, bool disabled_if_empty, uint32_t node_id = 0) const;
    String groupFilterHtml_(const char *select_id, uint32_t node_id = 0) const;
    String topFiltersBackHtml_() const;
    String composeTopFiltersHtml_(const String &device_html, const String &group_html) const;
    uint8_t parseGroupIdParam_(AsyncWebServerRequest *request, const String &name) const;

#define WEB_INTERFACE_CLASS_CONTEXT 1
#include "core/network/web/interfaces/web_interface_controllers_sockets.hpp"
#include "core/network/web/interfaces/web_interface_controllers_lights.hpp"
#include "core/network/web/interfaces/web_interface_controllers_security.hpp"
#include "core/network/web/interfaces/web_interface_controllers_meteo.hpp"
#include "core/network/web/interfaces/web_interface_controllers_thermo.hpp"
#include "core/network/web/interfaces/web_interface_controllers_septic.hpp"
#include "core/network/web/interfaces/web_interface_controllers_tanks.hpp"
#include "core/network/web/interfaces/web_interface_controllers_watering.hpp"
#include "core/network/web/interfaces/web_interface_controllers_display.hpp"
#undef WEB_INTERFACE_CLASS_CONTEXT

    String ringDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const;
    bool isStackRingView_(uint32_t node_id) const;
    bool sendStackRingCmd_(uint32_t node_id, bool set_state, bool state);
    bool sendStackRingCmdAll_(bool set_state, bool state);
    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame);

    static String paramValue_(AsyncWebServerRequest *request, const String &name);


    static String paramValueAny_(AsyncWebServerRequest *request, const String &name);


    static void appendHtmlEscaped_(String &out, const char *in);


    static void appendHtmlEscaped_(String &out, const String &in);


    static void copyStr_(char *dst, size_t size, const char *src);


    static String maskSecretValue_(const String &value);


    static bool isMaskedSecret_(const String &input, const String &actual);


    static String safeHtmlValue_(const String &value, const char *fallback);


    static String sanitizeUtf8_(const String &in);


    static bool isValidUtf8_(const String &in);


    static void appendUtf8_(String &out, uint16_t code);


    static String cp1251ToUtf8_(const String &in);


    static bool parseBasicAuth_(AsyncWebServerRequest *request, String &user, String &pass);


    static int8_t b64Index_(char c);


    static bool decodeBase64_(const String &in, String &out);


    static void requestBasicAuth_(AsyncWebServerRequest *request);


    String sanitizeUploadName_(const String &filename) const;


    String sanitizePath_(const String &path) const;


    float boardTemp_() const;


    float cpuTemp_() const;


    String formatTemp_(float temp_c) const;


    String rtcTimeStr_() const;


    String rtcDateStr_() const;


    String rtcTimeOnlyStr_() const;


    bool setRtc_(const String &date, const String &time);


    static uint8_t calcDow_(uint16_t y, uint8_t m, uint8_t d);


    void notifyRingPress_(bool stack_view, uint32_t node_id);


    void sendTelegramNotify_(const String &msg);


    static void appendJsonEscaped_(String &out, const String &value);


    float rtcTemp_() const;


    String fanStatusIcon_() const;


    String fanStatusIcon_(bool on) const;


    void sendHtml_(AsyncWebServerRequest *request, const String &page, bool set_cookie);


    void sendText_(AsyncWebServerRequest *request, int code, const char *type, const String &text, bool set_cookie);


    void sendHtmlRaw_(AsyncWebServerRequest *request, const String &page, bool set_cookie);


    void sendRedirect_(AsyncWebServerRequest *request, const char *path, bool set_cookie);


    void sendRedirect_(AsyncWebServerRequest *request, const String &path, bool set_cookie);


    String sessionCookie_() const;


    void injectAutoRefresh_(String &page) const;


    static uint32_t fnv1a_(uint32_t hash, const uint8_t *data, size_t len);


    static void hashAdd_(uint32_t &hash, const String &value);


    static void hashAdd_(uint32_t &hash, const char *value);


    static void hashAdd_(uint32_t &hash, uint32_t value);


    static void hashAdd_(uint32_t &hash, int32_t value);


    static void hashAdd_(uint32_t &hash, const uint8_t *data, size_t len);


    static int32_t scaled10_(float value);


    uint32_t uiPageHash_(const String &path);

    static void appendHex_(String &out, uint32_t value);


    static String stackNodeIdHex_(uint32_t value);


    static String genApiKey_();


    static uint32_t rand32_();


    String makeSessionToken_() const;


    void issueSession_(int16_t user_idx = -1);


    void clearSession_();


    void refreshSession_();


    bool sessionValid_(const String &token) const;


    bool extractSessionToken_(AsyncWebServerRequest *request, String &out) const;


    bool sessionPrincipalValid_() const;


    String gsmStatusLabel_() const;


    AsyncWebServer &_server;
    WifiManager &_wifi;
    Configs &_configs;
    ConfigsManagerIface *_configs_manager = nullptr;
    PlcControl *_plc = nullptr;
    RTC *_rtc = nullptr;
    TelegramClient *_tgbot = nullptr;
    TelegramBot *_tgbot_bot = nullptr;
    TelegramMenu *_tgbot_menu = nullptr;
    Controllers *_controllers = nullptr;
    RulesController *_rules = nullptr;
    GsmModem *_gsm = nullptr;
    I2CManager *_i2c = nullptr;
    OneWireManager *_ow = nullptr;

    uint16_t _stack_cmd_id = 0;
    WebInterfaceControllersOps _controllers_ops;
    WebInterfaceStackOps _stack_ops;
    File _upload;
    bool _upload_ok = true;
    bool _upload_in_progress = false;
    size_t _upload_size = 0;
    size_t _max_upload = 0;
    bool _ota_ok = false;
    size_t _ota_size = 0;
    String _ota_error;
    String _allowed_exts;
    String _last_status;
    String _wifi_status;
    String _gsm_status;
    String _upload_error;
    String _upload_name;
    String _ota_name;
    String _tgbot_status;
    String _cloud_status;
    String _stack_status;
    String _device_status;
    String _sockets_status;
    String _lights_status;
    String _controllers_status;
    String _meteo_status;
    String _thermo_status;
    String _tanks_status;
    String _watering_status;
    String _septic_status;
    String _ring_status;
    String _avr_status;
    String _camera_status;
    uint8_t *_camera_preview_buf = nullptr;
    size_t _camera_preview_size = 0;
    uint8_t _camera_preview_id = 0;
    uint32_t _camera_preview_ver = 0;
    String _leak_status;
    String _security_status;
    String _groups_status;
    String _rules_status;
    String _users_status;
    String _display_status;
    bool _auth_enabled = false;
    String _auth_user;
    String _auth_pass;
    CliConsole *_cli_auth = nullptr;
    Extender *_ext = nullptr;
    Logger *_log = nullptr;
    StackMaster *_stack_master = nullptr;
    StackSlaveHandler *_stack_slave = nullptr;
    StackCache *_stack_cache = nullptr;
    CloudClient *_cloud = nullptr;
    UsersRegistry *_users = nullptr;
    String _session_token;
    uint32_t _session_expire_ms = 0;
    uint32_t _session_ttl_ms = 10u * 60u * 1000u;
    int16_t _session_user_idx = -1;
    bool _upload_set_cookie = false;
    bool _ota_set_cookie = false;
    bool _ota_in_progress = false;
};









