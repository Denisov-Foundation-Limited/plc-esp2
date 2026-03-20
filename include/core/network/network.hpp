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
#include <ESPAsyncWebServer.h>
#include <type_traits>
#include <utility>

#include "utils/logger.hpp"
#include "core/network/stack/stack_async_tcp_transport.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_node.hpp"
#include "core/network/cloud/cloud_client.hpp"
#include "core/network/cloud/cloud_http_transport.hpp"
#include "utils/configs_manager_iface.hpp"

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
        WebInterfaceFs
    };

    Network(Logger &logs, WifiManager &wifi, GsmModem &gsm, WebInterface &fw, AsyncWebServer &web,
            Controllers &controllers, PlcControl &plc, RTC &rtc);

    void setStackConfig(ConfigsManagerIface &cfg);
    void setStackDeviceName(const String &name);

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
    WebInterface &_fw_upgrade;
    AsyncWebServer &_web;
    ConfigsManagerIface *_stack_cfg = nullptr;
    Error _last_error = Error::None;
    bool _started = false;

    static constexpr uint16_t kStackPort = 9010;
    static constexpr uint32_t kStackFallbackDelayMs = 10000;
    static constexpr uint32_t kStackFallbackRetryPrimaryMs = 30000;
    AsyncTcpStackServerTransport _stack_server;
    AsyncTcpStackClientTransport _stack_client;
    StackMaster _stack_master;
    StackNode _stack_node;
    CloudClient _cloud;
    CloudHttpTransport _cloud_http_transport;
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
    uint32_t _last_network_stall_recovery_ms = 0;
    uint32_t _last_seen_stack_reset_ms = 0;
    uint32_t _last_seen_stack_reset_count = 0;
    enum class StackTarget : uint8_t
    {
        Primary = 0,
        Fallback
    };
    StackTarget _stack_target = StackTarget::Primary;

    void beginStack_();

    void ensureStackMasterStarted_();

    void switchStackTarget_(StackTarget target);

    void updateStackFallback_();
    void detectNetworkStall_();

public:
    StackNode &stackNode();
    StackMaster &stackMaster();
    CloudClient &cloudClient();
    ConfigsManagerIface::StackRole stackRole() const;
    bool stackFallbackActive() const;
    bool stackMasterActive() const;
};
