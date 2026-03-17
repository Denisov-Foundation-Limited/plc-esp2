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
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <type_traits>
#include <utility>

#include "utils/logger.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_node.hpp"
#include "core/network/cloud/cloud_client.hpp"
#include "utils/configs_manager_iface.hpp"

class TelegramBot;
class TelegramMenu;
class WifiManager;
class WebInterface;
class GsmModem;
class RTC;
class Controllers;
class PlcControl;

class Network
{
public:
    enum class Error : uint8_t
    {
        None = 0,
        Wifi,
        TelegramClientMissing,
        TelegramProxyInvalid,
        WebInterfaceFs
    };

    Network(Logger &logs, WifiManager &wifi, GsmModem &gsm, TelegramClient &tgbot, TelegramBot &bot,
            TelegramMenu &menu, WebInterface &fw, AsyncWebServer &web, WiFiClientSecure &wifi_client,
            Controllers &controllers, PlcControl &plc, RTC &rtc);

    void setStackConfig(ConfigsManagerIface &cfg);
    void setStackDeviceName(const String &name);

    void setTelegramClient(Client &client);
    void setTelegramClientKind(TelegramNetCfg::ClientKind kind);

    void setTelegramProxy(const String &host, uint16_t port, const String &path);

    void disableTelegramProxy();

    bool begin();

    Error lastError() const;

    void loop();

    void setCloudConfig(const CloudClient::Config &cfg);
    void setCloudEnabled(bool enabled);
    void setCloudApiKey(const String &key);
    void setCloudFirmwareVersion(const String &ver);
    void setCloudEventIntervalMs(uint32_t ms);

private:
    template <typename T, typename = void>
    struct HasSetBufferSizes_ : std::false_type
    {
    };

    template <typename T>
    struct HasSetBufferSizes_<T, std::void_t<decltype(std::declval<T &>().setBufferSizes(0, 0))>> : std::true_type
    {
    };

    template <typename T>
    static void tuneTlsClientBuffers_(T &client)
    {
#if defined(ESP32)
        if constexpr (HasSetBufferSizes_<T>::value)
            client.setBufferSizes(2048, 512);
#else
        (void)client;
#endif
    }

    Logger &_logs;
    WifiManager &_wifi;
    GsmModem &_gsm;
    TelegramClient &_tgbot;
    TelegramBot &_bot;
    TelegramMenu &_menu;
    WebInterface &_fw_upgrade;
    AsyncWebServer &_web;
    WiFiClientSecure &_wifi_client;
    Client *_tgbot_ext_client = nullptr;
    ConfigsManagerIface *_stack_cfg = nullptr;
    Error _last_error = Error::None;
    bool _client_override_set = false;
    TelegramNetCfg::ClientKind _client_override = TelegramNetCfg::ClientKind::WifiSecure;
    bool _proxy_override = false;
    bool _proxy_use = false;
    String _proxy_host;
    uint16_t _proxy_port = 0;
    String _proxy_path;
    bool _started = false;
    static constexpr uint32_t kTelegramPollIntervalMs = 500;
    static constexpr uint16_t kTelegramPollTimeoutSec = 2;

    static constexpr uint16_t kStackPort = 9010;
    static constexpr uint32_t kStackFallbackDelayMs = 10000;
    static constexpr uint32_t kStackFallbackRetryPrimaryMs = 30000;
    AsyncServer _stack_server;
    StackMaster _stack_master;
    StackNode _stack_node;
    CloudClient _cloud;
    CloudClient::Config _cloud_cfg;
    bool _cloud_cfg_set = false;
    ConfigsManagerIface::StackRole _stack_role = ConfigsManagerIface::StackRole::Master;
    String _stack_device_name;
    bool _stack_master_started = false;
    bool _stack_fallback_enabled = false;
    bool _stack_fallback_active = false;
    String _stack_primary_host;
    String _stack_fallback_host;
    uint32_t _stack_disconnect_ms = 0;
    uint32_t _stack_last_primary_try_ms = 0;
    enum class StackTarget : uint8_t
    {
        Primary = 0,
        Fallback
    };
    StackTarget _stack_target = StackTarget::Primary;

    bool configureTelegram_(const TelegramNetCfg &cfg);

    void beginStack_();

    void ensureStackMasterStarted_();

    void switchStackTarget_(StackTarget target);

    void updateStackFallback_();

public:
    StackNode &stackNode();
    StackMaster &stackMaster();
    CloudClient &cloudClient();
    ConfigsManagerIface::StackRole stackRole() const;
    bool stackFallbackActive() const;
    bool stackMasterActive() const;
};
