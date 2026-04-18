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
      _stack_master_server(_logs),
      _stack_rs485_server(_logs),
      _stack_slave_client(_logs),
      _stack_route(_logs),
      _cloud(_logs, controllers, plc, wifi, rtc)
{
    _cloud.setGsm(&gsm);
    _cloud.setNetwork(this);
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
    return _wifi.apActive() || _wifi.isConnected();
}

void Network::maintainStackWsReadiness_()
{
    ConfigsManagerIface::StackTransportKind transport = ConfigsManagerIface::StackTransportKind::WebSocket;
    ConfigsManagerIface::StackRole role = ConfigsManagerIface::StackRole::Master;
    bool deferred = false;
    bool master_started = false;
    uint32_t ready_since_ms = 0;
    bool ready_wait_logged = false;
    {
        const auto guard = _stack_lock.guard();
        transport = _stack_cfg ? _stack_cfg->stackTransport() : ConfigsManagerIface::StackTransportKind::WebSocket;
        role = _stack_role;
        deferred = _stack_ws_start_deferred;
        master_started = _stack_master_started;
        ready_since_ms = _stack_ws_ready_since_ms;
        ready_wait_logged = _stack_ws_ready_wait_logged;
    }
    if (transport != ConfigsManagerIface::StackTransportKind::WebSocket)
        return;

    const bool ready = stackWsNetworkReady_();
    if (!ready)
    {
        // Do not tear down an already running WebSocket master on transient
        // network readiness flaps during startup. This caused one-time slave
        // disconnects right after the first successful connect/auth cycle.
        if (role == ConfigsManagerIface::StackRole::Master && master_started)
            return;

        if (!deferred && (master_started || role == ConfigsManagerIface::StackRole::Slave))
        {
            {
                const auto guard = _stack_lock.guard();
                _stack_ws_start_deferred = true;
                _stack_ws_ready_since_ms = 0;
                _stack_ws_ready_wait_logged = false;
            }
            _logs.warn(F("STACK"), F("WS paused: network not ready"));
            requestStackCommand_(StackCommand::Stop);
        }
        else if (deferred && (ready_since_ms != 0 || ready_wait_logged))
        {
            const auto guard = _stack_lock.guard();
            _stack_ws_ready_since_ms = 0;
            _stack_ws_ready_wait_logged = false;
        }
        return;
    }

    if (deferred)
    {
        if (role == ConfigsManagerIface::StackRole::Slave)
        {
            const uint32_t now = millis();
            if (ready_since_ms == 0)
            {
                const auto guard = _stack_lock.guard();
                _stack_ws_ready_since_ms = now;
                _stack_ws_ready_wait_logged = false;
                return;
            }
            if ((uint32_t)(now - ready_since_ms) < kStackWsSlaveReadyDebounceMs)
            {
                if (!ready_wait_logged)
                {
                    {
                        const auto guard = _stack_lock.guard();
                        _stack_ws_ready_wait_logged = true;
                    }
                    _logs.info(F("STACK"), F("WS ready debounce: wait %lu ms before slave restart"),
                               (unsigned long)kStackWsSlaveReadyDebounceMs);
                }
                return;
            }
        }
        {
            const auto guard = _stack_lock.guard();
            _stack_ws_start_deferred = false;
            _stack_ws_ready_since_ms = 0;
            _stack_ws_ready_wait_logged = false;
        }
        _logs.info(F("STACK"), F("WS network ready: restart stack"));
        requestStackCommand_(StackCommand::Reconfigure);
    }
}

void Network::beginStack_()
{
    StackRouteAdapter::ExchangePolicy policy = StackRouteAdapter::ExchangePolicy::Direct;
    StackRouteAdapter::PayloadMode payload_mode = StackRouteAdapter::PayloadMode::Json;
    ConfigsManagerIface::StackRole role = ConfigsManagerIface::StackRole::Master;
    ConfigsManagerIface::StackTransportKind transport = ConfigsManagerIface::StackTransportKind::WebSocket;
    bool ws_deferred = false;
    bool fallback_enabled = false;
    String fallback_host;
    String primary_host;
    String api_key;
    String device_name;
    const uint32_t local_node_id = stackNodeIdFromMac_(ESP.getEfuseMac());

    {
        const auto guard = _stack_lock.guard();
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
            default:
                policy = StackRouteAdapter::ExchangePolicy::Direct;
                break;
            }

            switch (_stack_cfg->stackPayloadMode())
            {
            case ConfigsManagerIface::StackPayloadMode::Json:
                payload_mode = StackRouteAdapter::PayloadMode::Json;
                break;
            case ConfigsManagerIface::StackPayloadMode::Binary:
                payload_mode = StackRouteAdapter::PayloadMode::Binary;
                break;
            default:
                payload_mode = StackRouteAdapter::PayloadMode::Json;
                break;
            }

            role = _stack_cfg->stackRole();
            transport = _stack_cfg->stackTransport();
            fallback_enabled = _stack_cfg->stackFallbackEnabled();
            fallback_host = _stack_cfg->stackFallbackHost();
            primary_host = _stack_cfg->stackMasterHost();
            api_key = _stack_cfg->stackApiKey();
        }

        _stack_master_started = false;
        _stack_role = role;
        _stack_fallback_enabled = fallback_enabled;
        _stack_fallback_host = fallback_host;
        _stack_primary_host = primary_host;
        _stack_fallback_active = false;
        _stack_disconnect_ms = 0;
        _stack_last_primary_try_ms = 0;
        _stack_target = StackTarget::Primary;
        _stack_ws_start_deferred = false;
        _stack_ws_ready_since_ms = 0;
        _stack_ws_ready_wait_logged = false;
        device_name = _stack_device_name;
        if (transport == ConfigsManagerIface::StackTransportKind::WebSocket && !stackWsNetworkReady_())
        {
            _stack_ws_start_deferred = true;
            ws_deferred = true;
        }
    }

    _stack_route.setExchangePolicy(policy);
    _stack_route.setPayloadMode(payload_mode);
    setStackRuntimeState_(StackRuntimeState::Starting);
    _stack_master_server.stop();
    _stack_rs485_server.stop();
    _stack_slave_client.disconnect();
    _stack_route.setRuntimeBindings(false, false, false);
    if (ws_deferred)
    {
        _logs.warn(F("STACK"), F("WS start deferred: network not ready"));
        setStackRuntimeState_(StackRuntimeState::Starting);
        return;
    }
    if (role == ConfigsManagerIface::StackRole::Master)
    {
        _logs.info(F("STACK"), F("Role: master transport: %s"),
                   transport == ConfigsManagerIface::StackTransportKind::Rs485 ? "rs485" : "websocket");
        _stack_route.setLocalNodeId(local_node_id);
        bool started = false;
        if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
        {
            StackRs485Server::Config cfg;
            cfg.port = kStackPort;
            cfg.api_key = api_key;
            cfg.local_node_id = local_node_id;
            _stack_rs485_server.setConfig(cfg);
            started = _stack_rs485_server.begin();
        }
        else
        {
            StackMasterServer::Config cfg;
            cfg.port = kStackPort;
            cfg.api_key = api_key;
            cfg.local_node_id = local_node_id;
            _stack_master_server.setConfig(cfg);
            started = _stack_master_server.begin();
        }
        if (started)
        {
            _stack_route.setRuntimeBindings(transport != ConfigsManagerIface::StackTransportKind::Rs485,
                                            transport == ConfigsManagerIface::StackTransportKind::Rs485,
                                            false);
            {
                const auto guard = _stack_lock.guard();
                _stack_master_started = true;
            }
            _logs.info(F("STACK"), F("Master started: name: %s transport: %s node_id: 0x%08lX port: %u"),
                       device_name.length() ? device_name.c_str() : "-",
                       transport == ConfigsManagerIface::StackTransportKind::Rs485 ? "rs485" : "websocket",
                       (unsigned long)local_node_id, (unsigned)kStackPort);
        }
        else
        {
            _logs.error(F("STACK"), F("Master start failed: name: %s transport: %s node_id: 0x%08lX port: %u"),
                        device_name.length() ? device_name.c_str() : "-",
                        transport == ConfigsManagerIface::StackTransportKind::Rs485 ? "rs485" : "websocket",
                        (unsigned long)local_node_id, (unsigned)kStackPort);
            setStackRuntimeState_(StackRuntimeState::Degraded);
            return;
        }
        setStackRuntimeState_(StackRuntimeState::Online);
        return;
    }

    if (primary_host.length() == 0)
    {
        _logs.warn(F("STACK"), F("Role: slave, master host missing"));
        setStackRuntimeState_(StackRuntimeState::Degraded);
        return;
    }
    _logs.info(F("STACK"), F("Role: slave transport: %s master: %s"),
               transport == ConfigsManagerIface::StackTransportKind::Rs485 ? "rs485" : "websocket",
               primary_host.c_str());
    ensureStackSlaveStarted_();
}

void Network::stopStack_()
{
    _stack_master_server.stop();
    _stack_rs485_server.stop();
    _stack_slave_client.disconnect();
    _stack_route.setRuntimeBindings(false, false, false);
    {
        const auto guard = _stack_lock.guard();
        _stack_master_started = false;
        _stack_fallback_active = false;
        _stack_disconnect_ms = 0;
        _stack_ws_ready_since_ms = 0;
        _stack_ws_ready_wait_logged = false;
    }
    setStackRuntimeState_(StackRuntimeState::Stopped);
}
void Network::ensureStackMasterStarted_()
{
    ConfigsManagerIface::StackTransportKind transport = ConfigsManagerIface::StackTransportKind::WebSocket;
    String api_key;
    const uint32_t local_node_id = stackNodeIdFromMac_(ESP.getEfuseMac());
    {
        const auto guard = _stack_lock.guard();
        if (_stack_master_started)
            return;
        if (_stack_cfg)
        {
            transport = _stack_cfg->stackTransport();
            api_key = _stack_cfg->stackApiKey();
        }
    }
    if (transport == ConfigsManagerIface::StackTransportKind::Rs485)
    {
        StackRs485Server::Config cfg;
        cfg.port = kStackPort;
        cfg.api_key = api_key;
        cfg.local_node_id = local_node_id;
        _stack_rs485_server.setConfig(cfg);
        if (_stack_rs485_server.begin())
        {
            _stack_route.setRuntimeBindings(false, true, false);
            const auto guard = _stack_lock.guard();
            _stack_master_started = true;
        }
    }
    else
    {
        StackMasterServer::Config cfg;
        cfg.port = kStackPort;
        cfg.api_key = api_key;
        cfg.local_node_id = local_node_id;
        _stack_master_server.setConfig(cfg);
        if (_stack_master_server.begin())
        {
            _stack_route.setRuntimeBindings(true, false, false);
            const auto guard = _stack_lock.guard();
            _stack_master_started = true;
        }
    }
}
void Network::ensureStackSlaveStarted_()
{
    ConfigsManagerIface::StackTransportKind transport = ConfigsManagerIface::StackTransportKind::WebSocket;
    StackSlaveClient::Config cfg;
    {
        const auto guard = _stack_lock.guard();
        if (_stack_cfg)
        {
            transport = _stack_cfg->stackTransport();
            cfg.api_key = _stack_cfg->stackApiKey();
            cfg.caps = _stack_cfg->stackSlaveController() ? kStackCapController : 0u;
            cfg.payload_mode = _stack_cfg->stackPayloadMode();
        }
        cfg.host = (_stack_target == StackTarget::Primary) ? _stack_primary_host : _stack_fallback_host;
        cfg.port = kStackPort;
        cfg.device_name = _stack_device_name;
    }
    cfg.node_id = stackNodeIdFromMac_(ESP.getEfuseMac());
    _stack_route.setLocalNodeId(cfg.node_id);
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
    String host;
    {
        const auto guard = _stack_lock.guard();
        if (target == _stack_target)
            return;
        host = (target == StackTarget::Primary) ? _stack_primary_host : _stack_fallback_host;
        if (!host.length())
            return;
        _stack_target = target;
    }
    _stack_slave_client.disconnect();
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

bool Network::stackIndexState(uint32_t node_id, StackUnitSnapshot::State &out) const
{
    return _stack_unit_snapshot.state(node_id, out);
}

bool Network::stackIndexCacheState(uint32_t node_id, StackUnitSnapshot::CacheState &out) const
{
    return _stack_unit_snapshot.cacheState(node_id, out);
}

bool Network::stackIndexRequestState(uint32_t node_id, StackUnitSnapshot::RequestState &out) const
{
    return _stack_unit_snapshot.requestState(node_id, out);
}

bool Network::stackIndexSocketById(uint32_t node_id, uint8_t id, StackUnitSnapshot::SocketItem &out) const
{
    return _stack_unit_snapshot.socketById(node_id, id, out);
}

bool Network::stackIndexSocketAt(uint32_t node_id, uint8_t index, StackUnitSnapshot::SocketItem &out) const
{
    return _stack_unit_snapshot.socketAt(node_id, index, out);
}

bool Network::stackIndexSocketsPage(uint32_t node_id, uint8_t offset, StackUnitSnapshot::SocketItem *out, uint8_t capacity,
                                    uint8_t &out_count) const
{
    return _stack_unit_snapshot.socketsPage(node_id, offset, out, capacity, out_count);
}

bool Network::stackIndexLightById(uint32_t node_id, uint8_t id, StackUnitSnapshot::SocketItem &out) const
{
    return _stack_unit_snapshot.lightById(node_id, id, out);
}

bool Network::stackIndexLightAt(uint32_t node_id, uint8_t index, StackUnitSnapshot::SocketItem &out) const
{
    return _stack_unit_snapshot.lightAt(node_id, index, out);
}

bool Network::stackIndexLightsPage(uint32_t node_id, uint8_t offset, StackUnitSnapshot::SocketItem *out, uint8_t capacity,
                                   uint8_t &out_count) const
{
    return _stack_unit_snapshot.lightsPage(node_id, offset, out, capacity, out_count);
}

bool Network::stackIndexMeteoById(uint32_t node_id, uint8_t id, StackUnitSnapshot::MeteoItem &out) const
{
    return _stack_unit_snapshot.meteoById(node_id, id, out);
}

bool Network::stackIndexMeteoAt(uint32_t node_id, uint8_t index, StackUnitSnapshot::MeteoItem &out) const
{
    return _stack_unit_snapshot.meteoAt(node_id, index, out);
}

bool Network::stackIndexMeteoPage(uint32_t node_id, uint8_t offset, StackUnitSnapshot::MeteoItem *out, uint8_t capacity,
                                  uint8_t &out_count) const
{
    return _stack_unit_snapshot.meteoPage(node_id, offset, out, capacity, out_count);
}

bool Network::stackIndexThermoById(uint32_t node_id, uint8_t id, StackUnitSnapshot::ThermoItem &out) const
{
    return _stack_unit_snapshot.thermoById(node_id, id, out);
}

bool Network::stackIndexThermoAt(uint32_t node_id, uint8_t index, StackUnitSnapshot::ThermoItem &out) const
{
    return _stack_unit_snapshot.thermoAt(node_id, index, out);
}

bool Network::stackIndexThermoPage(uint32_t node_id, uint8_t offset, StackUnitSnapshot::ThermoItem *out, uint8_t capacity,
                                   uint8_t &out_count) const
{
    return _stack_unit_snapshot.thermoPage(node_id, offset, out, capacity, out_count);
}

bool Network::stackIndexTankById(uint32_t node_id, uint8_t id, StackUnitSnapshot::TankItem &out) const
{
    return _stack_unit_snapshot.tankById(node_id, id, out);
}

bool Network::stackIndexTankAt(uint32_t node_id, uint8_t index, StackUnitSnapshot::TankItem &out) const
{
    return _stack_unit_snapshot.tankAt(node_id, index, out);
}

bool Network::stackIndexTanksPage(uint32_t node_id, uint8_t offset, StackUnitSnapshot::TankItem *out, uint8_t capacity,
                                  uint8_t &out_count) const
{
    return _stack_unit_snapshot.tanksPage(node_id, offset, out, capacity, out_count);
}

bool Network::stackIndexLeakById(uint32_t node_id, uint8_t id, StackUnitSnapshot::LeakItem &out) const
{
    return _stack_unit_snapshot.leakById(node_id, id, out);
}

bool Network::stackIndexLeakAt(uint32_t node_id, uint8_t index, StackUnitSnapshot::LeakItem &out) const
{
    return _stack_unit_snapshot.leakAt(node_id, index, out);
}

bool Network::stackIndexLeaksPage(uint32_t node_id, uint8_t offset, StackUnitSnapshot::LeakItem *out, uint8_t capacity,
                                  uint8_t &out_count) const
{
    return _stack_unit_snapshot.leaksPage(node_id, offset, out, capacity, out_count);
}

bool Network::prepareStackPageRequest(StackUnitSnapshot::PageKind kind, uint32_t node_id, uint32_t now_ms, uint16_t offset,
                                      uint32_t pending_ms)
{
    switch (kind)
    {
        case StackUnitSnapshot::PageKind::Sockets:
            return _stack_unit_snapshot.prepareSocketsPageRequest(node_id, now_ms, offset, pending_ms);
        case StackUnitSnapshot::PageKind::Lights:
            return _stack_unit_snapshot.prepareLightsPageRequest(node_id, now_ms, offset, pending_ms);
        case StackUnitSnapshot::PageKind::Meteo:
            return _stack_unit_snapshot.prepareMeteoPageRequest(node_id, now_ms, offset, pending_ms);
        case StackUnitSnapshot::PageKind::Thermo:
            return _stack_unit_snapshot.prepareThermoPageRequest(node_id, now_ms, offset, pending_ms);
        case StackUnitSnapshot::PageKind::Tanks:
            return _stack_unit_snapshot.prepareTanksPageRequest(node_id, now_ms, offset, pending_ms);
        case StackUnitSnapshot::PageKind::Leak:
        default:
            return _stack_unit_snapshot.prepareLeakPageRequest(node_id, now_ms, offset, pending_ms);
    }
}

void Network::completeStackPageRequest(StackUnitSnapshot::PageKind kind, uint32_t node_id, uint16_t offset)
{
    switch (kind)
    {
        case StackUnitSnapshot::PageKind::Sockets:
            _stack_unit_snapshot.completeSocketsPageRequest(node_id, offset);
            return;
        case StackUnitSnapshot::PageKind::Lights:
            _stack_unit_snapshot.completeLightsPageRequest(node_id, offset);
            return;
        case StackUnitSnapshot::PageKind::Meteo:
            _stack_unit_snapshot.completeMeteoPageRequest(node_id, offset);
            return;
        case StackUnitSnapshot::PageKind::Thermo:
            _stack_unit_snapshot.completeThermoPageRequest(node_id, offset);
            return;
        case StackUnitSnapshot::PageKind::Tanks:
            _stack_unit_snapshot.completeTanksPageRequest(node_id, offset);
            return;
        case StackUnitSnapshot::PageKind::Leak:
        default:
            _stack_unit_snapshot.completeLeakPageRequest(node_id, offset);
            return;
    }
}

void Network::clearStackPageRequest(StackUnitSnapshot::PageKind kind, uint32_t node_id)
{
    switch (kind)
    {
        case StackUnitSnapshot::PageKind::Sockets:
            _stack_unit_snapshot.clearSocketsPageRequest(node_id);
            return;
        case StackUnitSnapshot::PageKind::Lights:
            _stack_unit_snapshot.clearLightsPageRequest(node_id);
            return;
        case StackUnitSnapshot::PageKind::Meteo:
            _stack_unit_snapshot.clearMeteoPageRequest(node_id);
            return;
        case StackUnitSnapshot::PageKind::Thermo:
            _stack_unit_snapshot.clearThermoPageRequest(node_id);
            return;
        case StackUnitSnapshot::PageKind::Tanks:
            _stack_unit_snapshot.clearTanksPageRequest(node_id);
            return;
        case StackUnitSnapshot::PageKind::Leak:
        default:
            _stack_unit_snapshot.clearLeakPageRequest(node_id);
            return;
    }
}

void Network::clearStackIndexStatePending(uint32_t node_id)
{
    _stack_unit_snapshot.clearPending(node_id);
}

void Network::applyStackIndexSystemState(uint32_t node_id, const StackUnitSnapshot::State &state)
{
    _stack_unit_snapshot.applySystemState(node_id, state);
}

void Network::applyStackIndexControllerSummary(uint32_t node_id, const StackUnitSnapshot::State &state)
{
    _stack_unit_snapshot.applyControllerSummary(node_id, state);
}

void Network::updateStackIndexSocketsPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t on_total,
                                          const StackUnitSnapshot::SocketItem *items, uint8_t item_count,
                                          uint32_t updated_ms)
{
    _stack_unit_snapshot.applySocketsPage(node_id, offset, enabled_total, on_total, items, item_count, updated_ms);
}

void Network::updateStackIndexLightsPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t on_total,
                                         const StackUnitSnapshot::SocketItem *items, uint8_t item_count,
                                         uint32_t updated_ms)
{
    _stack_unit_snapshot.applyLightsPage(node_id, offset, enabled_total, on_total, items, item_count, updated_ms);
}

void Network::updateStackIndexMeteoPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t ok_total,
                                        const StackUnitSnapshot::MeteoItem *items, uint8_t item_count,
                                        uint32_t updated_ms)
{
    _stack_unit_snapshot.applyMeteoPage(node_id, offset, enabled_total, ok_total, items, item_count, updated_ms);
}

void Network::updateStackIndexThermoPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t active_total,
                                         const StackUnitSnapshot::ThermoItem *items, uint8_t item_count,
                                         uint32_t updated_ms)
{
    _stack_unit_snapshot.applyThermoPage(node_id, offset, enabled_total, active_total, items, item_count, updated_ms);
}

void Network::updateStackIndexTanksPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t alert_total,
                                        const StackUnitSnapshot::TankItem *items, uint8_t item_count,
                                        uint32_t updated_ms)
{
    _stack_unit_snapshot.applyTanksPage(node_id, offset, enabled_total, alert_total, items, item_count, updated_ms);
}

void Network::updateStackIndexLeaksPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t alert_total,
                                        const StackUnitSnapshot::LeakItem *items, uint8_t item_count,
                                        uint32_t updated_ms)
{
    _stack_unit_snapshot.applyLeaksPage(node_id, offset, enabled_total, alert_total, items, item_count, updated_ms);
}

void Network::invalidateStackIndexState(uint32_t node_id)
{
    _stack_unit_snapshot.invalidate(node_id);
}

bool Network::stackSlaveSendResponse(uint32_t target_node, const char *feature, const char *action, uint32_t reply_to,
                                     const JsonDocument *payload)
{
    StackRouteAdapter::Mode mode = StackRouteAdapter::Mode::Json;
    {
        const auto guard = _stack_lock.guard();
        if (_stack_cfg)
        {
            switch (_stack_cfg->stackPayloadMode())
            {
            case ConfigsManagerIface::StackPayloadMode::Json:
                mode = StackRouteAdapter::Mode::Json;
                break;
            case ConfigsManagerIface::StackPayloadMode::Binary:
                mode = StackRouteAdapter::Mode::Binary;
                break;
            default:
                mode = StackRouteAdapter::Mode::Json;
                break;
            }
        }
    }
    bool sent = false;
    if (mode == StackRouteAdapter::Mode::Json)
    {
        const StackTransport::RouteMeta meta = StackRouteAdapter::makeResponseMeta(reply_to);
        sent = _stack_slave_client.sendRoute(target_node, feature, action, payload, &meta);
    }
    else
        sent = _stack_route.sendResponse(target_node, feature, action, reply_to, payload, mode);
    if (!sent)
    {
        _logs.warn(F("STACK"), F("Slave send response failed: dst 0x%08lX feature: %s action: %s reply_to: %lu mode: %s"),
                   (unsigned long)target_node, feature ? feature : "-", action ? action : "-",
                   (unsigned long)reply_to, mode == StackRouteAdapter::Mode::Binary ? "binary" : "json");
    }
    return sent;
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
    payload["binary"] = notify.is_binary;
    payload["level"] = notify.level;
    payload["feature"] = notify.feature;
    if (notify.code[0])
        payload["code"] = notify.code;
    if (notify.message.length())
        payload["message"] = notify.message;
    if (!notify.is_binary && notify.payload.length())
    {
        DynamicJsonDocument extra(384);
        if (!deserializeJson(extra, notify.payload.c_str(), notify.payload.length()))
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
