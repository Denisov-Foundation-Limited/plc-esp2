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
#include <stdint.h>

#include "core/rtc.hpp"
#include "core/network/wifi_manager.hpp"
#include "ftest.hpp"
#include "plc/plc_control.hpp"
#include "core/cli/cli_config.hpp"
#include "core/cli/cli_enable.hpp"
#include "core/cli/modules/cli_stack.hpp"
#include "core/cli/modules/cli_socket.hpp"
#include "core/cli/modules/cli_meteo.hpp"
#include "core/cli/modules/cli_thermo.hpp"
#include "core/cli/modules/cli_tank.hpp"
#include "core/cli/modules/cli_security.hpp"
#include "core/cli/modules/cli_septic.hpp"
#include "core/cli/modules/cli_ring.hpp"
#include "core/cli/modules/cli_avr.hpp"
#include "core/cli/modules/cli_leak.hpp"
#include "core/cli/modules/cli_watering.hpp"
#include "core/cli/modules/cli_cloud.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/gpio/portio.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_slave_handler.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/users_registry.hpp"
#include "controllers/controllers.hpp"
class CliConsole
{
public:
    using CLIEnable = CLIEnableT<CliConsole>;
    using CLIConfig = CLIConfigT<CliConsole>;
    using CLIWifi = CLIWifiT<CliConsole>;
    using CLIStack = CLIStackT<CliConsole>;
    using CLISocket = CLISocketT<CliConsole>;
    using CLIMeteo = CLIMeteoT<CliConsole>;
    using CLIThermo = CLIThermoT<CliConsole>;
    using CLITank = CLITankT<CliConsole>;
    using CLISeptic = CLISepticT<CliConsole>;
    using CLISecurity = CLISecurityT<CliConsole>;
    using CLIRing = CLIRingT<CliConsole>;
    using CLIAvr = CLIAvrT<CliConsole>;
    using CLILeak = CLILeakT<CliConsole>;
    using CLIWatering = CLIWateringT<CliConsole>;
    using CLICloud = CLICloudT<CliConsole>;
    static constexpr const char kAdminUser[] = "admin";

    CliConsole(PlcControl &plc, WifiManager &wifi, RTC &rtc, Ftest &ftest, I2CManager &i2c, OneWireManager &ow,
               Configs &configs, Extender &ext,
               UsersRegistry &users,
               Controllers &controllers, StackMaster *stack_master);

    void begin(Stream &io);

    void setStackMaster(StackMaster *master);
    void setStackSlave(StackSlaveHandler *slave);

    void loop();

    bool setAdminPassword_(const String &pass);

    bool setAdminPasswordHashHex_(const String &hex);

    String adminPasswordHashHex() const;

    bool checkAdminPassword(const String &pass) const;

    bool adminPasswordSet() const;
    const String &currentUser() const;

    void enterUser();
    void enterEnable();
    void enterConfig();
    void enterConfigWifi();
    void enterConfigTime();
    void enterConfigSocket();
    void enterConfigMeteo();
    void enterConfigThermo();
    void enterConfigTank();
    void enterConfigSeptic();
    void enterConfigSecurity();
    void enterConfigRing();
    void enterConfigAvr();
    void enterConfigLeak();
    void enterConfigWatering();
    void enterConfigCloud();
    void logout();

    void cmdShowPlc_();

    void cmdShowBoard_();

    void cmdShowPort_(uint8_t id);

    bool gpioPortUsed_(uint8_t id) const;

    void cmdShowPorts_();

    void cmdShowWifi_();

    void cmdShowTime_();

    void cmdShowCloud_();

    void cmdCopy_(const String &line);

    void cmdShowI2c_();

    void cmdShowStack_();

    void cmdShowOw_();

    void cmdShowConfig_();

    void cmdFtest_();

    void cmdExtScan_();

    void cmdExtList_();

    void cmdWifiRestart_();

    void cmdStack_(const String &line);

    void cmdRestart_();

    void cmdWriteConfig_();

    bool setStackRole_(ConfigsManagerIface::StackRole role);

    bool setStackMasterHost_(const String &host);

    bool setStackApiKey_(const String &key);

    bool setStackFallbackEnabled_(bool enabled);

    bool setStackFallbackHost_(const String &host);

    bool setStackSlaveController_(bool controller);

    void cmdEraseConfig_();

private:
    enum class Mode : uint8_t
    {
        User,
        Enable,
        Config,
        ConfigWifi,
        ConfigTime,
        ConfigSocket,
        ConfigMeteo,
        ConfigThermo,
        ConfigTank,
        ConfigSeptic,
        ConfigSecurity,
        ConfigRing,
        ConfigAvr,
        ConfigLeak,
        ConfigWatering,
        ConfigCloud
    };

    enum class State : uint8_t
    {
        NeedUser,
        NeedPass,
        LoggedIn
    };

    static constexpr size_t kMaxLine = 96;
    void showHelpTopic_(const String &topic);

    void handleTab_();

    void handleShow_(String what);

    static bool eq_(const String &a, const char *b);

    static bool startsWith_(const String &a, const char *b);

    static bool parseUint_(const String &s, uint16_t &out);

    const UsersRegistry::User *sessionUser_() const;

    bool cliSessionIsAdmin_() const;

    bool cliAclControllerAllowed_(UsersRegistry::AclController ctrl, uint8_t unit = 0) const;

    bool cliAclCanViewItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint8_t unit = 0) const;

    bool cliAclCanControlItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint8_t unit = 0) const;

    bool cliAclAnyView_(UsersRegistry::AclController ctrl, uint16_t max_item_id, uint8_t unit = 0) const;

    bool parseStackAclUnit_(const String &raw_unit, uint8_t &out_unit) const;

    bool denyAcl_();

    bool enforceAclShow_(String what);

    bool enforceAclEnable_(const String &line);

    bool enforceAcl_(const String &line);

    void handleLine_(String line);

    void handleLogin_(const String &line);

    void printPrompt_();

    bool handleEscape_(char c);

    void redrawLine_(const String &new_line, size_t old_len);

    void addHistory_(const String &line);

    void historyUp_();

    void historyDown_();

    void printLine_(const __FlashStringHelper *s);

    void printKeyValue_(const __FlashStringHelper *key, const __FlashStringHelper *value, size_t key_w);

    void printKeyValue_(const __FlashStringHelper *key, const String &value, size_t key_w);

    void printKeyValueTab_(const __FlashStringHelper *key, const __FlashStringHelper *value, size_t key_w);

    void printKeyValueTab_(const __FlashStringHelper *key, const String &value, size_t key_w);

    void beginCmdOutput_();

    static void sha256_(const char *input, uint8_t out[32]);

    bool isAdminUser_(const String &user) const;

    bool checkAdmin_(const char *pass) const;

    static int hexNibble_(char c);

    static bool hexToBytes_(const String &hex, uint8_t out[32]);

    static void bytesToHex_(const uint8_t in[32], char out[65]);

    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    Ftest &_ftest;
    I2CManager &_i2c;
    OneWireManager &_ow;
    Configs &_configs;
    Extender &_ext;
    UsersRegistry &_users;
    Controllers &_controllers;
    ConfigsManagerIface *_configs_manager = nullptr;

    Stream *_io = nullptr;
    String _line;
    String _user_input;
    int16_t _session_user_idx = -1;
    State _state = State::NeedUser;
    Mode _mode = Mode::Enable;
    uint8_t _admin_hash[32] = {};
    bool _admin_set = false;
    bool _saw_cr = false;
    uint8_t _esc_state = 0;
    bool _cmd_blank_after = false;

    static constexpr size_t kHistoryMax = 12;
    String _history[kHistoryMax];
    size_t _history_len = 0;
    int _history_pos = -1;
    String _history_saved;

    CLIWifi _wifi_cli;
    CLIStack _stack_cli;
    CLISocket _socket_cli;
    CLIMeteo _meteo_cli;
    CLIThermo _thermo_cli;
    CLITank _tank_cli;
    CLISeptic _septic_cli;
    CLISecurity _security_cli;
    CLIRing _ring_cli;
    CLIAvr _avr_cli;
    CLILeak _leak_cli;
    CLIWatering _watering_cli;
    CLICloud _cloud_cli;
    CLIEnable _enable;
    CLIConfig _config;

    void printExtList_();

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame);

    static String payloadToString_(const uint8_t *data, size_t len);

    void printExtHeader_();

    void printExtRow_(const String &unit, uint8_t id, uint8_t bus, const char *addr,
                      const __FlashStringHelper *type, const char *type_str);

    void printI2cHeader_();

    void printI2cRow_(const String &unit, uint8_t bus, const char *addr);

    void printOwHeader_();

    void printOwRow_(const String &unit, uint8_t bus,
                     const __FlashStringHelper *type, const char *addr,
                     const char *type_str = nullptr);

    void printPlcHeader_();

    void printPlcRow_(const String &unit, const String &name, bool fan,
                      float board_c, float on_c, float hyst_c,
                      const float *rtc_c);

    void printRtcHeader_();

    void printRtcRow_(const String &unit, const char *date, const char *time,
                      unsigned weekday);

    void refreshPrompt_();

    static const __FlashStringHelper *extTypeName_(Extender::Type t);

    const __FlashStringHelper *extDevTypeName_(uint8_t dev) const;

    static const __FlashStringHelper *portTypeName_(PortIO::PinType t);

    static const __FlashStringHelper *locationName_(PortIO::Location loc);

    static const __FlashStringHelper *owBusName_(OneWireCfg::OwType t);

    static void owAddrToHex_(const uint8_t in[8], char out[17]);

    void printPortsHeader_();

    void printSocketsHeader_();

    void printSocketIdRangeInline_();

    void printSocketRow_(const char *unit, uint8_t id, bool enabled,
                         const char *name, int button, int relay, bool state);

    void printPortRow_(const String &unit, uint8_t id, const PortIO::PortDesc &p);

    void printPortStateRow_(const String &unit, uint8_t id,
                            const char *backend, const char *loc, const char *type, bool ctrl,
                            int dev, int pin, const char *hw);

    void printPadIntOrDash_(int v, uint8_t width);

    void printPad_(uint8_t value, uint8_t width);

    void printPadStr_(const __FlashStringHelper *s, uint8_t width);

    void printPadStr_(const char *s, uint8_t width);

    static size_t utf8CharCount_(const char *s);

    template <typename>
    friend class CLIEnableT;
    template <typename>
    friend class CLIConfigT;
    template <typename>
    friend class CLIWifiT;
    template <typename>
    friend class CLIStackT;
    template <typename>
    friend class CLISocketT;
    template <typename>
    friend class CLIMeteoT;
    template <typename>
    friend class CLIThermoT;
    template <typename>
    friend class CLITankT;
    template <typename>
    friend class CLISepticT;
    template <typename>
    friend class CLISecurityT;
    template <typename>
    friend class CLIRingT;
    template <typename>
    friend class CLIAvrT;
    template <typename>
    friend class CLILeakT;
    template <typename>
    friend class CLIWateringT;
    template <typename>
    friend class CLICloudT;

public:
    void setConfigsManager(ConfigsManagerIface &mgr);
};
