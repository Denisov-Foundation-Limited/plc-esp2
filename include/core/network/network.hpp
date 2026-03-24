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
#include <atomic>
#include <type_traits>
#include <utility>

#include "utils/logger.hpp"
#include "core/network/cloud/cloud_client.hpp"
#include "core/network/cloud/cloud_http_transport.hpp"
#include "core/network/stack/stack_route_adapter.hpp"
#include "core/network/stack/stack_device_registry.hpp"
#include "core/network/stack/stack_master_server.hpp"
#include "core/network/stack/stack_rs485_server.hpp"
#include "core/network/stack/stack_slave_client.hpp"
#include "core/network/stack/stack_unit_snapshot.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/rtos_lock.hpp"

class WifiManager;
class WebInterface;
class GsmModem;
class RTC;
class Controllers;
class PlcControl;

class Network
{
public:
    using StackNodeEventHandler = void (*)(void *ctx, uint32_t node_id, bool online);

    enum class Error : uint8_t
    {
        None = 0,
        Wifi,
        WebInterfaceFs
    };

    enum class StackRuntimeState : uint8_t
    {
        Stopped = 0,
        Starting,
        AuthPending,
        Online,
        Degraded,
        FallbackMaster
    };

    enum class StackCommand : uint8_t
    {
        None = 0,
        Reconfigure = 1 << 0,
        Restart = 1 << 1,
        Stop = 1 << 2,
        SwitchTargetPrimary = 1 << 3,
        SwitchTargetFallback = 1 << 4
    };

    struct StackDiagnostics
    {
        const char *runtime_state = "unknown";
        bool fallback_active = false;
        bool master_active = false;
        uint16_t online_devices = 0;
        uint32_t network_lock_held_ms = 0;
        StackRouteAdapter::ExchangeDiagnostics exchange{};
        StackRs485Transport::Diagnostics rs485{};
    };

    Network(Logger &logs, WifiManager &wifi, GsmModem &gsm, WebInterface &fw, AsyncWebServer &web,
            Controllers &controllers, PlcControl &plc, RTC &rtc);

    void setStackConfig(ConfigsManagerIface &cfg);
    void setStackDeviceName(const String &name);
    void setStackNodeEventHandler(StackNodeEventHandler cb, void *ctx);

    bool begin();

    Error lastError() const;

    void loop();
    void stackLoop();

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
    StackMasterServer _stack_master_server;
    StackRs485Server _stack_rs485_server;
    StackSlaveClient _stack_slave_client;
    StackRouteAdapter _stack_route;
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
    enum class StackTarget : uint8_t
    {
        Primary = 0,
        Fallback
    };
    StackTarget _stack_target = StackTarget::Primary;

    void beginStack_();
    void stopStack_();
    void processStackCommands_();
    void requestStackCommand_(StackCommand cmd);
    bool stackWsNetworkReady_() const;
    void maintainStackWsReadiness_();

    void ensureStackMasterStarted_();
    void ensureStackSlaveStarted_();

    void switchStackTarget_(StackTarget target);

    void updateStackFallback_();
    static void onStackNotify_(void *ctx, uint32_t source_node, const StackJsonProtocol::NotifyMessage &notify);
    void handleStackNotify_(uint32_t source_node, const StackJsonProtocol::NotifyMessage &notify);
    static void onStackNodeEvent_(void *ctx, uint32_t node_id, bool online);
    void handleStackNodeEvent_(uint32_t node_id, bool online);
    static bool provideCloudStackNodeName_(void *ctx, uint32_t node_id, String &out);
    bool cloudStackNodeName_(uint32_t node_id, String &out) const;

public:
    StackRs485Server &stackRs485Server();
    StackRouteAdapter &stackRoute();
    CloudClient &cloudClient();
    ConfigsManagerIface::StackRole stackRole() const;
    StackRuntimeState stackRuntimeState() const;
    const char *stackRuntimeStateText() const;
    uint32_t stackLocalNodeId() const;
    size_t stackOnlineDeviceCount() const;
    bool stackDeviceSnapshotAt(size_t idx, StackDeviceRegistry::DeviceInfo &out) const;
    bool stackDeviceSnapshotByNodeId(uint32_t node_id, StackDeviceRegistry::DeviceInfo &out) const;
    bool prepareStackIndexStateRequest(uint32_t node_id, uint32_t now_ms, uint32_t fresh_ms, uint32_t pending_ms);
    bool stackIndexState(uint32_t node_id, StackUnitSnapshot::State &out) const;
    bool stackIndexStateSnapshot(uint32_t node_id, StackUnitSnapshot::Snapshot &out) const;
    bool stackIndexSocketById(uint32_t node_id, uint8_t id, StackUnitSnapshot::SocketItem &out) const;
    bool stackIndexSocketAt(uint32_t node_id, uint8_t index, StackUnitSnapshot::SocketItem &out) const;
    bool stackIndexLightById(uint32_t node_id, uint8_t id, StackUnitSnapshot::SocketItem &out) const;
    bool stackIndexLightAt(uint32_t node_id, uint8_t index, StackUnitSnapshot::SocketItem &out) const;
    bool prepareStackSocketsPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    bool prepareStackLightsPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    void completeStackSocketsPageRequest(uint32_t node_id, uint16_t offset);
    void completeStackLightsPageRequest(uint32_t node_id, uint16_t offset);
    void clearStackSocketsPageRequest(uint32_t node_id);
    void clearStackLightsPageRequest(uint32_t node_id);
    void clearStackIndexStatePending(uint32_t node_id);
    void updateStackIndexState(uint32_t node_id, const StackUnitSnapshot::Snapshot &state);
    void invalidateStackIndexState(uint32_t node_id);
    bool stackSlaveSendResponse(uint32_t target_node, const char *feature, const char *action, uint32_t reply_to,
                                const JsonDocument *payload = nullptr);
    bool stackSlaveAuthorized() const;
    bool stackFallbackActive() const;
    bool stackMasterActive() const;
    StackDiagnostics stackDiagnostics() const;

private:
    void setStackRuntimeState_(StackRuntimeState state);
    void refreshStackRuntimeState_();

private:
    mutable RtosRecursiveLock _stack_lock;
    StackUnitSnapshot _stack_unit_snapshot;
    std::atomic<uint8_t> _stack_cmd_pending{(uint8_t)StackCommand::None};
    std::atomic<uint8_t> _stack_runtime_state_raw{(uint8_t)StackRuntimeState::Stopped};
    bool _stack_ws_start_deferred = false;
    StackNodeEventHandler _stack_node_event_cb = nullptr;
    void *_stack_node_event_ctx = nullptr;
};
