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
#include "core/network/telegram/telegram_bot.hpp"
#include "core/network/telegram/telegram_menu.hpp"
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

Network::Network(Logger &logs, WifiManager &wifi, GsmModem &gsm, TelegramClient &tgbot, TelegramBot &bot,
        TelegramMenu &menu, WebInterface &fw, AsyncWebServer &web, WiFiClientSecure &wifi_client,
        Controllers &controllers, PlcControl &plc, RTC &rtc)
    : _logs(logs), _wifi(wifi), _gsm(gsm), _tgbot(tgbot), _bot(bot), _menu(menu),
      _fw_upgrade(fw), _web(web),
      _wifi_client(wifi_client),
      _stack_server(kStackPort),
      _stack_master(_stack_server, _logs),
      _stack_node(_logs),
      _cloud(_logs, controllers, plc, wifi, rtc)
{
    _cloud.setGsm(&gsm);
    _cloud.setStackMaster(&_stack_master);
}
void Network::setStackConfig(ConfigsManagerIface &cfg)
{
    _stack_cfg = &cfg;
    _stack_master.setConfigsManager(cfg);
    _cloud.setConfigsManager(&cfg);
}
void Network::setStackDeviceName(const String &name)
{ _stack_device_name = name; }
void Network::setTelegramClient(Client &client)
{ _tgbot_ext_client = &client; }
void Network::setTelegramClientKind(TelegramNetCfg::ClientKind kind)
{
    _client_override = kind;
    _client_override_set = true;
}
void Network::setTelegramProxy(const String &host, uint16_t port, const String &path)
{
    _proxy_override = true;
    _proxy_use = true;
    _proxy_host = host;
    _proxy_port = port;
    _proxy_path = path;
}
void Network::disableTelegramProxy()
{
    _proxy_override = true;
    _proxy_use = false;
    _proxy_host = "";
    _proxy_port = 0;
    _proxy_path = "";
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
    _logs.info(F("NET"), F("Configure Telegram network"));
    if (!configureTelegram_(ActiveBoardProfile::TELEGRAM_NET))
        return false;
    _logs.info(F("NET"), F("Telegram polling disabled, notifications only"));
    _logs.info(F("NET"), F("Init Stack"));
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
    _stack_node.loop();
    updateStackFallback_();
}
void Network::setCloudConfig(const CloudClient::Config &cfg)
{
    _cloud_cfg = cfg;
    _cloud_cfg_set = _cloud_cfg.host.length() > 0;
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
bool Network::configureTelegram_(const TelegramNetCfg &cfg)
{
    const bool use_proxy = _proxy_override ? _proxy_use : cfg.use_proxy;
    const char *host = _proxy_override ? _proxy_host.c_str() : cfg.proxy_host;
    const uint16_t port = _proxy_override ? _proxy_port : cfg.proxy_port;
    const char *path = _proxy_override ? _proxy_path.c_str() : cfg.proxy_path;

    if (use_proxy)
    {
        if (!host || host[0] == '\0')
        {
            _last_error = Error::TelegramProxyInvalid;
            return false;
        }
        _tgbot.setProxy(host, port, path);
    }
    else
    {
        _tgbot.clearProxy();
    }

    const TelegramNetCfg::ClientKind kind = _client_override_set ? _client_override : cfg.client;
    switch (kind)
    {
    case TelegramNetCfg::ClientKind::WifiSecure:
#if defined(ESP32)
        // Reduce TLS RAM footprint for Telegram API calls/uploads.
        tuneTlsClientBuffers_(_wifi_client);
#endif
        _tgbot.setClientSecure(_wifi_client);
        return true;
    case TelegramNetCfg::ClientKind::TinyGsm:
        if (_tgbot_ext_client)
        {
            _tgbot.setClient(*_tgbot_ext_client, TelegramClient::ClientKind::TinyGsm, false, nullptr);
            return true;
        }
        _last_error = Error::TelegramClientMissing;
        return false;
    default:
        return true;
    }
}
void Network::beginStack_()
{
    _stack_role = _stack_cfg ? _stack_cfg->stackRole() : ConfigsManagerIface::StackRole::Master;
    _stack_fallback_enabled = _stack_cfg ? _stack_cfg->stackFallbackEnabled() : false;
    _stack_fallback_host = _stack_cfg ? _stack_cfg->stackFallbackHost() : String();
    _stack_primary_host = _stack_cfg ? _stack_cfg->stackMasterHost() : String();
    _stack_fallback_active = false;
    _stack_disconnect_ms = 0;
    _stack_last_primary_try_ms = 0;
    _stack_target = StackTarget::Primary;
    if (_stack_role == ConfigsManagerIface::StackRole::Master)
    {
        _logs.info(F("STACK"), F("Role: master"));
        _stack_master.begin();
        _stack_master_started = true;
        return;
    }

    if (_stack_primary_host.length() == 0)
    {
        _logs.warn(F("STACK"), F("Role: slave, master host missing"));
        return;
    }
    _logs.info(F("STACK"), F("Role: slave, master: %s"), _stack_primary_host.c_str());
    uint64_t mac = ESP.getEfuseMac();
    _stack_node.setNodeId(stackNodeIdFromMac_(mac));
    uint32_t caps = 0;
    if (_stack_cfg && _stack_cfg->stackSlaveController())
        caps |= StackCapController;
    _stack_node.setCaps(caps);
    _stack_node.setServer(_stack_primary_host, kStackPort);
    if (_stack_device_name.length() > 0)
        _stack_node.setDeviceName(_stack_device_name);
    _stack_node.begin();
}
void Network::ensureStackMasterStarted_()
{
    if (_stack_master_started)
        return;
    _stack_master.begin();
    _stack_master_started = true;
}
void Network::switchStackTarget_(StackTarget target)
{
    if (target == _stack_target)
        return;
    const String host = (target == StackTarget::Primary) ? _stack_primary_host : _stack_fallback_host;
    if (!host.length())
        return;
    _stack_target = target;
    _stack_node.setServer(host, kStackPort);
    _stack_node.disconnect();
    if (target == StackTarget::Primary)
        _logs.info(F("STACK"), F("Switch stack host to primary: %s"), host.c_str());
    else
        _logs.warn(F("STACK"), F("Switch stack host to fallback: %s"), host.c_str());
}
void Network::updateStackFallback_()
{
    if (_stack_role != ConfigsManagerIface::StackRole::Slave)
        return;
    const uint32_t now = millis();
    const bool connected = _stack_node.connected();
    if (connected)
    {
        _stack_disconnect_ms = 0;
        if (_stack_target == StackTarget::Primary && _stack_fallback_active)
        {
            _stack_fallback_active = false;
            _logs.info(F("STACK"), F("Master connection restored, fallback disabled"));
        }
        if (_stack_target == StackTarget::Primary)
            return;
    }

    if (!connected && _stack_disconnect_ms == 0)
        _stack_disconnect_ms = now;

    const bool can_local_fallback = _stack_fallback_enabled && _stack_fallback_host.length() == 0;
    if (!connected && can_local_fallback && !_stack_fallback_active &&
        (now - _stack_disconnect_ms) >= kStackFallbackDelayMs)
    {
        _stack_fallback_active = true;
        _logs.warn(F("STACK"), F("Master connection lost, fallback master enabled"));
        ensureStackMasterStarted_();
    }

    if (_stack_fallback_enabled && _stack_fallback_host.length())
    {
        if (_stack_target == StackTarget::Primary)
        {
            if (!connected && (now - _stack_disconnect_ms) >= kStackFallbackDelayMs)
                switchStackTarget_(StackTarget::Fallback);
        }
        else
        {
            if ((now - _stack_last_primary_try_ms) >= kStackFallbackRetryPrimaryMs)
            {
                _stack_last_primary_try_ms = now;
                switchStackTarget_(StackTarget::Primary);
            }
        }
    }
}
StackNode &Network::stackNode()
{ return _stack_node; }
StackMaster &Network::stackMaster()
{ return _stack_master; }
CloudClient &Network::cloudClient()
{ return _cloud; }
ConfigsManagerIface::StackRole Network::stackRole() const
{ return _stack_role; }
bool Network::stackFallbackActive() const
{ return _stack_fallback_active; }
bool Network::stackMasterActive() const
{
    return _stack_role == ConfigsManagerIface::StackRole::Master || _stack_fallback_active;
}
