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

#include "core/network/network.hpp"

#include "boards/board_profile_base.hpp"
#include "core/network/gsm_modem.hpp"
#include "core/network/stack/stack_master_server.hpp"
#include "core/network/stack/stack_rs485_server.hpp"
#include "core/network/stack/stack_route_adapter.hpp"
#include "core/network/stack/stack_slave_client.hpp"
#include <ArduinoJson.h>
#include "core/network/web/web_interface.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/rtc.hpp"
#include "controllers/controllers.hpp"
#include "plc/plc_control.hpp"

namespace
{
uint32_t stackNodeIdFromMac_(uint64_t mac)
{
    // Hash all 6 bytes of eFuse MAC to reduce collisions vs low32-only.
    uint32_t h = 2166136261u; // FNV-1a 32-bit offset basis
    for (uint8_t i = 0; i < 6; ++i)
    {
        const uint8_t b = (uint8_t)((mac >> (8u * i)) & 0xFFu);
        h ^= b;
        h *= 16777619u; // FNV prime
    }
    if (h == 0)
        h = 1;
    return h;
}
} // namespace

Network::Network(Logger &logs, WifiManager &wifi, GsmModem &gsm, WebInterface &fw, AsyncWebServer &web,
        Controllers &controllers, PlcControl &plc, RTC &rtc)
    : _logs(logs), _wifi(wifi), _gsm(gsm), _fw_upgrade(fw), _web(web),
      _stack_server(kStackPort),
      _stack_master(_stack_server, _logs),
      _stack_node(_logs),
      _stack_master_server(_logs),
      _stack_rs485_server(_logs),
      _stack_slave_client(_logs),
      _stack_route(_logs),
      _cloud(_logs, controllers, plc, wifi, rtc)
{
    _cloud.setGsm(&gsm);
    _cloud.setNetwork(this);
    _cloud.setStackMaster(&_stack_master);
    _cloud.setStackNodeNameProvider(&Network::provideCloudStackNodeName_, this);
    _stack_route.bindMaster(_stack_master_server);
    _stack_route.bindRs485Master(_stack_rs485_server);
    _stack_route.bindSlave(_stack_slave_client);
    _stack_route.setNotificationHandler(&Network::onStackNotify_, this);
    _stack_master_server.setNodeEventHandler(&Network::onStackNodeEvent_, this);
    _stack_rs485_server.setNodeEventHandler(&Network::onStackNodeEvent_, this);
}
void Network::setStackConfig(ConfigsManagerIface &cfg)
{
    const auto guard = _stack_lock.guard();
    _stack_cfg = &cfg;
    if (_started)
        requestStackCommand_(StackCommand::Reconfigure);
}
void Network::setStackNodeEventHandler(StackNodeEventHandler cb, void *ctx)
{
    const auto guard = _stack_lock.guard();
    _stack_node_event_cb = cb;
    _stack_node_event_ctx = ctx;
}
void Network::setStackDeviceName(const String &name)
{
    const auto guard = _stack_lock.guard();
    _stack_device_name = name;
    if (_started)
        requestStackCommand_(StackCommand::Reconfigure);
}
bool Network::begin()
{
    _last_error = Error::None;
    if (ActiveBoardProfile::GSM.enabled && _gsm.enabled())
    {
        _logs.info(F("NET"), F("Init GSM modem"));
        if (!_gsm.begin(ActiveBoardProfile::GSM.uart_index))
            _logs.warn(F("GSM"), F("Modem init failed, continue without GSM"));
    }
    else
    {
        _logs.info(F("NET"), F("GSM modem disabled"));
    }
    _logs.info(F("NET"), F("Init Wi-Fi"));
    if (!_wifi.begin())
    {
        _last_error = Error::Wifi;
        _logs.warn(F("NET"), F("Wi-Fi init failed"));
        return false;
    }
    _logs.info(F("NET"), F("Init Web interface FS"));
    if (!_fw_upgrade.begin())
    {
        _last_error = Error::WebInterfaceFs;
        _logs.warn(F("NET"), F("Web interface FS init failed"));
        return false;
    }
    _logs.info(F("NET"), F("Register Web routes"));
    _fw_upgrade.registerRoutes();
    _logs.info(F("NET"), F("Start Web server"));
    _web.begin();
    beginStack_();
    _started = true;
    if (_cloud_cfg_set && _cloud.enabled())
        _cloud.begin(_cloud_cfg);
    _logs.info(F("NET"), F("Init done"));
    return true;
}
Network::Error Network::lastError() const
{ return _last_error; }
void Network::loop()
{
}

void Network::stackLoop()
{
    processStackCommands_();
    maintainStackWsReadiness_();
    _stack_master_server.loop();
    _stack_rs485_server.loop();
    _stack_slave_client.loop();
    _stack_route.loop();
    updateStackFallback_();
    refreshStackRuntimeState_();
}
void Network::setCloudConfig(const CloudClient::Config &cfg)
{
    _cloud_cfg = cfg;
    _cloud_cfg_set = _cloud_cfg.host.length() > 0;
    if (_cloud_cfg.transport == CloudTransportKind::Http)
        _cloud.setTransport(_cloud_http_transport);
    else
        _cloud.useDefaultTransport();
    if (!_cloud_cfg_set)
    {
        _cloud.disconnect();
        return;
    }
    if (_started && _cloud.enabled())
        _cloud.begin(_cloud_cfg);
}
void Network::setCloudEnabled(bool enabled)
{
    _cloud.setEnabled(enabled);
    if (_started && enabled && _cloud_cfg_set)
        _cloud.begin(_cloud_cfg);
}
void Network::setCloudApiKey(const String &key)
{ _cloud.setApiKey(key); }
void Network::setCloudFirmwareVersion(const String &ver)
{ _cloud.setFirmwareVersion(ver); }
void Network::setCloudEventIntervalMs(uint32_t ms)
{ _cloud.setAutoEventIntervalMs(ms); }

void Network::requestStackCommand_(StackCommand cmd)
{
    _stack_cmd_pending.fetch_or((uint8_t)cmd);
}

void Network::processStackCommands_()
{
    const uint8_t pending = _stack_cmd_pending.exchange((uint8_t)StackCommand::None);
    if ((pending & (uint8_t)StackCommand::Reconfigure) != 0)
    {
        beginStack_();
        return;
    }
    if ((pending & (uint8_t)StackCommand::Stop) != 0)
        stopStack_();
    if ((pending & (uint8_t)StackCommand::Restart) != 0)
        beginStack_();
    if ((pending & (uint8_t)StackCommand::SwitchTargetFallback) != 0)
        switchStackTarget_(StackTarget::Fallback);
    else if ((pending & (uint8_t)StackCommand::SwitchTargetPrimary) != 0)
        switchStackTarget_(StackTarget::Primary);
}

bool Network::stackWsNetworkReady_() const
{
    return _wifi.apEnabled() || _wifi.isConnected();
}

void Network::maintainStackWsReadiness_()
{
    ConfigsManagerIface::StackTransportKind transport = ConfigsManagerIface::StackTransportKind::WebSocket;
    ConfigsManagerIface::StackRole role = ConfigsManagerIface::StackRole::Master;
    bool deferred = false;
    bool master_started = false;
    {
        const auto guard = _stack_lock.guard();
        transport = _stack_cfg ? _stack_cfg->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
        role = _stack_role;
        deferred = _stack_ws_start_deferred;
        master_started = _stack_master_started;
    }
    if (transport != ConfigsManagerIface::StackTransportKind::WebSocket)
        return;

    const bool ready = stackWsNetworkReady_();
    if (!ready)
    {
        if (!deferred && (master_started || role == ConfigsManagerIface::StackRole::Slave))
        {
            {
                const auto guard = _stack_lock.guard();
                _stack_ws_start_deferred = true;
            }
            _logs.warn(F("STACK"), F("WS paused: network not ready"));
            requestStackCommand_(StackCommand::Stop);
        }
        return;
    }

    if (deferred)
    {
        {
            const auto guard = _stack_lock.guard();
            _stack_ws_start_deferred = false;
        }
        _logs.info(F("STACK"), F("WS network ready: restart stack"));
        requestStackCommand_(StackCommand::Reconfigure);
    }
}

void Network::beginStack_()
{
    const auto guard = _stack_lock.guard();
    StackRouteAdapter::ExchangePolicy policy = StackRouteAdapter::ExchangePolicy::Auto;
    if (_stack_cfg)
    {
        switch (_stack_cfg->stackExchangePolicy())
        {
        case ConfigsManagerIface::StackExchangePolicy::Direct:
            policy = StackRouteAdapter::ExchangePolicy::Direct;
            break;
        case ConfigsManagerIface::StackExchangePolicy::Poll:
            policy = StackRouteAdapter::ExchangePolicy::Poll;
            break;
        case ConfigsManagerIface::StackExchangePolicy::Auto:
        default:
            policy = StackRouteAdapter::ExchangePolicy::Auto;
            break;
        }
    }
    _stack_route.setExchangePolicy(policy);

    StackRouteAdapter::PayloadMode payload_mode = StackRouteAdapter::PayloadMode::Auto;
    if (_stack_cfg)
    {
        switch (_stack_cfg->stackPayloadMode())
        {
        case ConfigsManagerIface::StackPayloadMode::Json:
            payload_mode = StackRouteAdapter::PayloadMode::Json;
            break;
        case ConfigsManagerIface::StackPayloadMode::Binary:
            payload_mode = StackRouteAdapter::PayloadMode::Binary;
            break;
        case ConfigsManagerIface::StackPayloadMode::Auto:
        default:
            payload_mode = StackRouteAdapter::PayloadMode::Auto;
            break;
        }
    }
    _stack_route.setPayloadMode(payload_mode);

    setStackRuntimeState_(StackRuntimeState::Starting);
    _stack_master_server.stop();
    _stack_rs485_server.stop();
    _stack_slave_client.disconnect();
    _stack_route.setRuntimeBindings(false, false, false);
    _stack_master_started = false;
    _stack_role = _stack_cfg ? _stack_cfg->stackRole() : ConfigsManagerIface::StackRole::Master;
    _stack_fallback_enabled = _stack_cfg ? _stack_cfg->stackFallbackEnabled() : false;
    _stack_fallback_host = _stack_cfg ? _stack_cfg->stackFallbackHost() : String();
    _stack_primary_host = _stack_cfg ? _stack_cfg->stackMasterHost() : String();
    _stack_fallback_active = false;
    _stack_disconnect_ms = 0;
    _stack_last_primary_try_ms = 0;
    _stack_target = StackTarget::Primary;
    const ConfigsManagerIface::StackTransportKind transport =
        _stack_cfg ? _stack_cfg->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
    if (transport == ConfigsManagerIface::StackTransportKind::WebSocket && !stackWsNetworkReady_())
    {
        _stack_ws_start_deferred = true;
        _logs.warn(F("STACK"), F("WS start deferred: network not ready"));
        setStackRuntimeState_(StackRuntimeState::Starting);
        return;
    }
    _stack_ws_start_deferred = false;
    if (_stack_role == ConfigsManagerIface::StackRole::Master)
    {
        _logs.info(F("STACK"), F("Role: master transport: %s"),
                   transport == ConfigsManagerIface::StackTransportKind::Rs485 ? "rs485" : "websocket");
        const uint32_t local_node_id = stackNodeIdFromMac_(ESP.getEfuseMac());
        _stack_route.setLocalNodeId(local_node_id);
        bool started = false;
        if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
        {
            StackRs485Server::Config cfg;
            cfg.port = kStackPort;
            cfg.api_key = _stack_cfg ? _stack_cfg->stackApiKey() : String();
            cfg.local_node_id = local_node_id;
            _stack_rs485_server.setConfig(cfg);
            started = _stack_rs485_server.begin();
            _stack_route.setRuntimeBindings(false, true, false);
        }
        else
        {
            StackMasterServer::Config cfg;
            cfg.port = kStackPort;
            cfg.api_key = _stack_cfg ? _stack_cfg->stackApiKey() : String();
            cfg.local_node_id = local_node_id;
            _stack_master_server.setConfig(cfg);
            started = _stack_master_server.begin();
            _stack_route.setRuntimeBindings(true, false, false);
        }
        if (started)
        {
            _logs.info(F("STACK"), F("Master started: name: %s transport: %s node_id: 0x%08lX port: %u"),
                       _stack_device_name.length() ? _stack_device_name.c_str() : "-",
                       transport == ConfigsManagerIface::StackTransportKind::Rs485 ? "rs485" : "websocket",
                       (unsigned long)local_node_id, (unsigned)kStackPort);
        }
        else
        {
            _logs.error(F("STACK"), F("Master start failed: name: %s transport: %s node_id: 0x%08lX port: %u"),
                        _stack_device_name.length() ? _stack_device_name.c_str() : "-",
                        transport == ConfigsManagerIface::StackTransportKind::Rs485 ? "rs485" : "websocket",
                        (unsigned long)local_node_id, (unsigned)kStackPort);
            setStackRuntimeState_(StackRuntimeState::Degraded);
            return;
        }
        _stack_master_started = true;
        setStackRuntimeState_(StackRuntimeState::Online);
        return;
    }

    if (_stack_primary_host.length() == 0)
    {
        _logs.warn(F("STACK"), F("Role: slave, master host missing"));
        setStackRuntimeState_(StackRuntimeState::Degraded);
        return;
    }
    _logs.info(F("STACK"), F("Role: slave transport: %s master: %s"),
               transport == ConfigsManagerIface::StackTransportKind::Rs485 ? "rs485" : "websocket",
               _stack_primary_host.c_str());
    uint64_t mac = ESP.getEfuseMac();
    _stack_node.setNodeId(stackNodeIdFromMac_(mac));
    uint32_t caps = 0;
    if (_stack_cfg && _stack_cfg->stackSlaveController())
        caps |= StackCapController;
    _stack_node.setCaps(caps);
    _stack_node.setServer(_stack_primary_host, kStackPort);
    if (_stack_device_name.length() > 0)
        _stack_node.setDeviceName(_stack_device_name);
    if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
    {
        ensureStackSlaveStarted_();
        return;
    }
    ensureStackSlaveStarted_();
}

void Network::stopStack_()
{
    const auto guard = _stack_lock.guard();
    _stack_master_server.stop();
    _stack_rs485_server.stop();
    _stack_slave_client.disconnect();
    _stack_route.setRuntimeBindings(false, false, false);
    _stack_master_started = false;
    _stack_fallback_active = false;
    _stack_disconnect_ms = 0;
    setStackRuntimeState_(StackRuntimeState::Stopped);
}
void Network::ensureStackMasterStarted_()
{
    const auto guard = _stack_lock.guard();
    if (_stack_master_started)
        return;
    const ConfigsManagerIface::StackTransportKind transport =
        _stack_cfg ? _stack_cfg->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
    if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
    {
        StackRs485Server::Config cfg;
        cfg.port = kStackPort;
        cfg.api_key = _stack_cfg ? _stack_cfg->stackApiKey() : String();
        cfg.local_node_id = stackNodeIdFromMac_(ESP.getEfuseMac());
        _stack_rs485_server.setConfig(cfg);
        _stack_rs485_server.begin();
        _stack_route.setRuntimeBindings(false, true, false);
    }
    else
    {
        StackMasterServer::Config cfg;
        cfg.port = kStackPort;
        cfg.api_key = _stack_cfg ? _stack_cfg->stackApiKey() : String();
        cfg.local_node_id = stackNodeIdFromMac_(ESP.getEfuseMac());
        _stack_master_server.setConfig(cfg);
        _stack_master_server.begin();
        _stack_route.setRuntimeBindings(true, false, false);
    }
    _stack_master_started = true;
}
void Network::ensureStackSlaveStarted_()
{
    const auto guard = _stack_lock.guard();
    const ConfigsManagerIface::StackTransportKind transport =
        _stack_cfg ? _stack_cfg->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
    StackSlaveClient::Config cfg;
    cfg.host = (_stack_target == StackTarget::Primary) ? _stack_primary_host : _stack_fallback_host;
    cfg.port = kStackPort;
    cfg.api_key = _stack_cfg ? _stack_cfg->stackApiKey() : String();
    cfg.device_name = _stack_device_name;
    cfg.node_id = stackNodeIdFromMac_(ESP.getEfuseMac());
    _stack_route.setLocalNodeId(cfg.node_id);
    cfg.caps = (_stack_cfg && _stack_cfg->stackSlaveController()) ? StackCapController : 0u;
    if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
        cfg.transport = StackSlaveClient::Config::TransportKind::Rs485Stub;
    _stack_slave_client.setConfig(cfg);
    _stack_slave_client.begin();
    _stack_route.setRuntimeBindings(false, false, true);
    setStackRuntimeState_(transport == ConfigsManagerIface::StackTransportKind::Rs485
                              ? StackRuntimeState::Online
                              : StackRuntimeState::AuthPending);
}
void Network::switchStackTarget_(StackTarget target)
{
    const auto guard = _stack_lock.guard();
    if (target == _stack_target)
        return;
    const String host = (target == StackTarget::Primary) ? _stack_primary_host : _stack_fallback_host;
    if (!host.length())
        return;
    _stack_target = target;
    _stack_node.setServer(host, kStackPort);
    _stack_node.disconnect();
    ensureStackSlaveStarted_();
    if (target == StackTarget::Primary)
        _logs.info(F("STACK"), F("Switch stack host to primary: %s"), host.c_str());
    else
        _logs.warn(F("STACK"), F("Switch stack host to fallback: %s"), host.c_str());
}
void Network::updateStackFallback_()
{
    const uint32_t now = millis();
    ConfigsManagerIface::StackRole role = ConfigsManagerIface::StackRole::Master;
    bool fallback_enabled = false;
    bool fallback_active = false;
    String fallback_host;
    StackTarget target = StackTarget::Primary;
    uint32_t disconnect_ms = 0;
    uint32_t last_primary_try_ms = 0;
    {
        const auto guard = _stack_lock.guard();
        role = _stack_role;
        fallback_enabled = _stack_fallback_enabled;
        fallback_active = _stack_fallback_active;
        fallback_host = _stack_fallback_host;
        target = _stack_target;
        disconnect_ms = _stack_disconnect_ms;
        last_primary_try_ms = _stack_last_primary_try_ms;
    }
    if (role != ConfigsManagerIface::StackRole::Slave)
        return;

    const bool connected = _stack_slave_client.isAuthorized();
    bool log_restored = false;
    bool log_local_fallback = false;
    bool start_local_master = false;
    bool switch_to_fallback = false;
    bool switch_to_primary = false;

    if (connected)
    {
        const auto guard = _stack_lock.guard();
        _stack_disconnect_ms = 0;
        if (_stack_target == StackTarget::Primary && _stack_fallback_active)
        {
            _stack_fallback_active = false;
            log_restored = true;
        }
        if (_stack_target == StackTarget::Primary)
        {
            if (log_restored)
                _logs.info(F("STACK"), F("Master connection restored, fallback disabled"));
            return;
        }
    }

    {
        const auto guard = _stack_lock.guard();
        if (!connected && _stack_disconnect_ms == 0)
            _stack_disconnect_ms = now;
        disconnect_ms = _stack_disconnect_ms;
        fallback_active = _stack_fallback_active;
        target = _stack_target;
        last_primary_try_ms = _stack_last_primary_try_ms;
    }

    const bool can_local_fallback = fallback_enabled && fallback_host.length() == 0;
    if (!connected && can_local_fallback && !fallback_active &&
        (now - disconnect_ms) >= kStackFallbackDelayMs)
    {
        {
            const auto guard = _stack_lock.guard();
            if (!_stack_fallback_active)
            {
                _stack_fallback_active = true;
                log_local_fallback = true;
                start_local_master = true;
            }
        }
    }

    if (log_local_fallback)
        _logs.warn(F("STACK"), F("Master connection lost, fallback master enabled"));
    if (start_local_master)
        ensureStackMasterStarted_();
    if (log_restored)
        _logs.info(F("STACK"), F("Master connection restored, fallback disabled"));

    if (fallback_enabled && fallback_host.length())
    {
        if (target == StackTarget::Primary)
        {
            if (!connected && (now - disconnect_ms) >= kStackFallbackDelayMs)
                switch_to_fallback = true;
        }
        else
        {
            if ((now - last_primary_try_ms) >= kStackFallbackRetryPrimaryMs)
            {
                const auto guard = _stack_lock.guard();
                if ((now - _stack_last_primary_try_ms) >= kStackFallbackRetryPrimaryMs)
                {
                    _stack_last_primary_try_ms = now;
                    switch_to_primary = true;
                }
            }
        }
    }

    if (switch_to_fallback)
        requestStackCommand_(StackCommand::SwitchTargetFallback);
    else if (switch_to_primary)
        requestStackCommand_(StackCommand::SwitchTargetPrimary);
}
StackNode &Network::stackNode()
{ return _stack_node; }
StackMaster &Network::stackMaster()
{ return _stack_master; }
StackRs485Server &Network::stackRs485Server()
{ return _stack_rs485_server; }
StackRouteAdapter &Network::stackRoute()
{ return _stack_route; }
CloudClient &Network::cloudClient()
{ return _cloud; }
ConfigsManagerIface::StackRole Network::stackRole() const
{
    const auto guard = _stack_lock.guard();
    return _stack_role;
}
bool Network::stackFallbackActive() const
{
    const auto guard = _stack_lock.guard();
    return _stack_fallback_active;
}
Network::StackRuntimeState Network::stackRuntimeState() const
{
    return (StackRuntimeState)_stack_runtime_state_raw.load();
}

const char *Network::stackRuntimeStateText() const
{
    switch (stackRuntimeState())
    {
    case StackRuntimeState::Stopped:
        return "stopped";
    case StackRuntimeState::Starting:
        return "starting";
    case StackRuntimeState::AuthPending:
        return "auth_pending";
    case StackRuntimeState::Online:
        return "online";
    case StackRuntimeState::Degraded:
        return "degraded";
    case StackRuntimeState::FallbackMaster:
        return "fallback_master";
    }
    return "unknown";
}
uint32_t Network::stackLocalNodeId() const
{
    return stackNodeIdFromMac_(ESP.getEfuseMac());
}
size_t Network::stackOnlineDeviceCount() const
{
    const auto guard = _stack_lock.guard();
    const ConfigsManagerIface::StackTransportKind transport =
        _stack_cfg ? _stack_cfg->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
    if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
        return _stack_rs485_server.router().registry().onlineCount();
    return _stack_master_server.registry().onlineCount();
}

bool Network::stackDeviceSnapshotAt(size_t idx, StackDeviceRegistry::DeviceInfo &out) const
{
    const auto guard = _stack_lock.guard();
    const ConfigsManagerIface::StackTransportKind transport =
        _stack_cfg ? _stack_cfg->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
    if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
        return _stack_rs485_server.router().registry().snapshotAt(idx, out);
    return _stack_master_server.registry().snapshotAt(idx, out);
}

bool Network::stackDeviceSnapshotByNodeId(uint32_t node_id, StackDeviceRegistry::DeviceInfo &out) const
{
    const auto guard = _stack_lock.guard();
    const ConfigsManagerIface::StackTransportKind transport =
        _stack_cfg ? _stack_cfg->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
    if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
        return _stack_rs485_server.router().registry().snapshotByNodeId(node_id, out);
    return _stack_master_server.registry().snapshotByNodeId(node_id, out);
}

bool Network::prepareStackIndexStateRequest(uint32_t node_id, uint32_t now_ms, uint32_t fresh_ms, uint32_t pending_ms)
{
    return _stack_unit_snapshot.prepareRequest(node_id, now_ms, fresh_ms, pending_ms);
}

bool Network::stackIndexStateSnapshot(uint32_t node_id, StackUnitSnapshot::Snapshot &out) const
{
    return _stack_unit_snapshot.snapshot(node_id, out);
}

void Network::clearStackIndexStatePending(uint32_t node_id)
{
    _stack_unit_snapshot.clearPending(node_id);
}

void Network::updateStackIndexState(uint32_t node_id, const StackUnitSnapshot::Snapshot &state)
{
    _stack_unit_snapshot.update(node_id, state);
}

void Network::invalidateStackIndexState(uint32_t node_id)
{
    _stack_unit_snapshot.invalidate(node_id);
}

bool Network::stackSlaveSendResponse(uint32_t target_node, const char *feature, const char *action, uint32_t reply_to,
                                     const JsonDocument *payload)
{
    const auto guard = _stack_lock.guard();
    const StackTransport::RouteMeta meta = StackRouteAdapter::makeResponseMeta(reply_to);
    return _stack_slave_client.sendRoute(target_node, feature, action, payload, &meta);
}

bool Network::stackSlaveAuthorized() const
{
    const auto guard = _stack_lock.guard();
    return _stack_slave_client.isAuthorized();
}

bool Network::stackMasterActive() const
{
    const auto guard = _stack_lock.guard();
    return _stack_role == ConfigsManagerIface::StackRole::Master || _stack_fallback_active;
}

Network::StackDiagnostics Network::stackDiagnostics() const
{
    StackDiagnostics out;
    {
        const auto guard = _stack_lock.guard();
        out.runtime_state = stackRuntimeStateText();
        out.fallback_active = _stack_fallback_active;
        out.master_active = (_stack_role == ConfigsManagerIface::StackRole::Master || _stack_fallback_active);
        out.network_lock_held_ms = _stack_lock.heldMs();
    }
    out.online_devices = (uint16_t)stackOnlineDeviceCount();
    out.exchange = _stack_route.exchangeDiagnostics();
    out.rs485 = _stack_rs485_server.transport().diagnostics();
    return out;
}

void Network::setStackRuntimeState_(StackRuntimeState state)
{
    _stack_runtime_state_raw.store((uint8_t)state);
}

void Network::refreshStackRuntimeState_()
{
    bool fallback_active = false;
    ConfigsManagerIface::StackRole role = ConfigsManagerIface::StackRole::Master;
    ConfigsManagerIface::StackTransportKind transport = ConfigsManagerIface::StackTransportKind::WebSocket;
    {
        const auto guard = _stack_lock.guard();
        fallback_active = _stack_fallback_active;
        role = _stack_role;
        transport = _stack_cfg ? _stack_cfg->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
    }

    if (fallback_active)
    {
        setStackRuntimeState_(StackRuntimeState::FallbackMaster);
        return;
    }
    if (role == ConfigsManagerIface::StackRole::Master)
    {
        setStackRuntimeState_(StackRuntimeState::Online);
        return;
    }
    if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
    {
        setStackRuntimeState_(_stack_slave_client.isConnected() ? StackRuntimeState::Online : StackRuntimeState::Degraded);
        return;
    }
    if (_stack_slave_client.isAuthorized())
        setStackRuntimeState_(StackRuntimeState::Online);
    else if (_stack_slave_client.isConnected())
        setStackRuntimeState_(StackRuntimeState::AuthPending);
    else
        setStackRuntimeState_(StackRuntimeState::Degraded);
}

void Network::onStackNotify_(void *ctx, uint32_t source_node, const StackJsonProtocol::NotifyMessage &notify)
{
    if (!ctx)
        return;
    static_cast<Network *>(ctx)->handleStackNotify_(source_node, notify);
}

void Network::handleStackNotify_(uint32_t source_node, const StackJsonProtocol::NotifyMessage &notify)
{
    const char *code = notify.code[0] ? notify.code : "-";
    const char *msg = notify.message.length() ? notify.message.c_str() : "-";
    if (strcmp(notify.level, "alarm") == 0 || strcmp(notify.level, "critical") == 0 || strcmp(notify.level, "error") == 0)
    {
        _logs.warn(F("STACK"), F("Notify: node 0x%08lX level %s feature %s code %s msg %s"),
                   (unsigned long)source_node,
                   notify.level, notify.feature, code, msg);
    }
    else
    {
        _logs.info(F("STACK"), F("Notify: node 0x%08lX level %s feature %s code %s msg %s"),
                   (unsigned long)source_node,
                   notify.level, notify.feature, code, msg);
    }

    DynamicJsonDocument payload(768);
    payload["level"] = notify.level;
    payload["feature"] = notify.feature;
    if (notify.code[0])
        payload["code"] = notify.code;
    if (notify.message.length())
        payload["message"] = notify.message;
    if (notify.payload.length())
    {
        DynamicJsonDocument extra(384);
        if (!deserializeJson(extra, notify.payload))
            payload["data"].set(extra.as<JsonVariantConst>());
        else
            payload["raw"] = notify.payload;
    }

    String data_json;
    serializeJson(payload, data_json);
    const String kind = String(notify.feature) + ".notify";
    const String reason = notify.code[0] ? String(notify.code) : String(notify.level);
    _cloud.publishScopedEvent("stack", source_node, kind, reason, data_json);
}

void Network::onStackNodeEvent_(void *ctx, uint32_t node_id, bool online)
{
    if (!ctx)
        return;
    static_cast<Network *>(ctx)->handleStackNodeEvent_(node_id, online);
}

void Network::handleStackNodeEvent_(uint32_t node_id, bool online)
{
    if (!online)
        invalidateStackIndexState(node_id);
    StackNodeEventHandler cb = nullptr;
    void *cb_ctx = nullptr;
    {
        const auto guard = _stack_lock.guard();
        cb = _stack_node_event_cb;
        cb_ctx = _stack_node_event_ctx;
    }
    if (cb)
        cb(cb_ctx, node_id, online);
}

bool Network::provideCloudStackNodeName_(void *ctx, uint32_t node_id, String &out)
{
    if (!ctx)
        return false;
    return static_cast<Network *>(ctx)->cloudStackNodeName_(node_id, out);
}

bool Network::cloudStackNodeName_(uint32_t node_id, String &out) const
{
    const auto guard = _stack_lock.guard();
    if (node_id == 0)
        return false;
    StackDeviceRegistry::DeviceInfo device;
    if (_stack_master_server.registry().snapshotByNodeId(node_id, device))
    {
        out = device.name;
        return out.length() != 0;
    }
    if (_stack_rs485_server.router().registry().snapshotByNodeId(node_id, device))
    {
        out = device.name;
        return out.length() != 0;
    }
    return false;
}
