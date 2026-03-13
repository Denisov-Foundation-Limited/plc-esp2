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

#include "core/network/wifi_manager.hpp"

#include "boards/board_profile.hpp"
#include "hal/io_stack.hpp"
#include "utils/logger.hpp"

WifiManager::WifiManager(Logger &log)
    : _log(log)
{
}
bool WifiManager::begin()
{
    _last_status = (wl_status_t)0xFF;
    _last_ap_clients = 0xFF;
    initNetLed_();

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
#if defined(ESP32)
    WiFi.setSleep(false);
    if (_log.ready())
        _log.info(F("WIFI"), F("Power save: off"));
#endif

    const bool sta_enabled = staEnabled();
    const bool ap_enabled = apEnabled();
    wifi_mode_t wifi_mode = WIFI_OFF;
    if (sta_enabled && ap_enabled)
        wifi_mode = WIFI_AP_STA;
    else if (ap_enabled)
        wifi_mode = WIFI_AP;
    else
        wifi_mode = WIFI_STA;

    WiFi.mode(wifi_mode);

    bool ap_ok = true;
    if (ap_enabled)
    {
        if (_log.ready())
        {
            if (sta_enabled)
                _log.info(F("WIFI"), F("Mode: STA+AP (STA SSID: %s AP SSID: %s)"),
                          _ssid.c_str(), _ap_ssid.c_str());
            else
                _log.info(F("WIFI"), F("Mode: AP (SSID: %s)"), _ap_ssid.c_str());
        }
        ap_ok = WiFi.softAP(_ap_ssid.c_str(), _ap_password.c_str());
        if (ap_ok && _log.ready())
        {
            const String ip = WiFi.softAPIP().toString();
            _log.info(F("WIFI"), F("AP IP %s"), ip.c_str());
        }
    }
    if (sta_enabled)
    {
        if (_log.ready())
        {
            if (!ap_enabled)
                _log.info(F("WIFI"), F("Mode: STA (SSID: %s)"), _ssid.c_str());
            if (_ssid.length() == 0)
                _log.warn(F("WIFI"), F("STA begin: SSID is empty"));
            else
                _log.info(F("WIFI"), F("STA begin: SSID: %s"), _ssid.c_str());
            if (_password.length() == 0)
                _log.warn(F("WIFI"), F("STA begin: password is empty"));
        }
        WiFi.begin(_ssid.c_str(), _password.c_str());
    }
    setNetLed_(ap_enabled && ap_ok);
    return ap_ok;
}
bool WifiManager::restart()
{
    if (_log.ready())
        _log.info(F("WIFI"), F("Restart"));
    setNetLed_(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    return begin();
}
void WifiManager::task()
{
    const bool sta_enabled = staEnabled();
    const bool ap_enabled = apEnabled();
    if (ap_enabled)
    {
        const uint8_t clients = WiFi.softAPgetStationNum();
        if (clients != _last_ap_clients)
        {
            _last_ap_clients = clients;
            if (_log.ready())
                _log.info(F("WIFI"), F("AP clients: %u"), clients);
        }
    }
    else
    {
        _last_ap_clients = 0xFF;
    }

    wl_status_t st = WL_DISCONNECTED;
    if (sta_enabled)
    {
        st = WiFi.status();
        if (st != _last_status)
        {
            _last_status = st;
            if (_log.ready())
                _log.info(F("WIFI"), F("STA %s"), statusToString_(st));
            if (st == WL_CONNECTED && _log.ready())
            {
                const String ip = WiFi.localIP().toString();
                _log.info(F("WIFI"), F("STA IP %s"), ip.c_str());
            }
        }
    }
    else
    {
        _last_status = (wl_status_t)0xFF;
    }
    setNetLed_(ap_enabled || (sta_enabled && st == WL_CONNECTED));
}
void WifiManager::setSsid(const String &ssid)
{ _ssid = ssid; }
void WifiManager::setPassword(const String &password)
{ _password = password; }
void WifiManager::setMode(Mode mode)
{ _mode = mode; }
bool WifiManager::setModeByName(const String &mode)
{
    Mode parsed = Mode::Ap;
    if (!parseMode(mode, parsed))
        return false;
    _mode = parsed;
    return true;
}
void WifiManager::setAp(bool ap)
{ _mode = ap ? Mode::Ap : Mode::Sta; }
void WifiManager::setApSsid(const String &ssid)
{ _ap_ssid = ssid; }
void WifiManager::setApPassword(const String &password)
{ _ap_password = password; }
void WifiManager::setIo(IoStack &io)
{
    _io = &io;
    _net_led_initialized = false;
}
const String &WifiManager::ssid() const
{ return _ssid; }
const String &WifiManager::password() const
{ return _password; }
WifiManager::Mode WifiManager::mode() const
{ return _mode; }
bool WifiManager::staEnabled() const
{ return _mode == Mode::Sta || _mode == Mode::StaAp; }
bool WifiManager::ap() const
{ return apEnabled(); }
bool WifiManager::apEnabled() const
{ return _mode == Mode::Ap || _mode == Mode::StaAp; }
const String &WifiManager::apSsid() const
{ return _ap_ssid; }
const String &WifiManager::apPassword() const
{ return _ap_password; }
bool WifiManager::isConnected() const
{ return staEnabled() && WiFi.status() == WL_CONNECTED; }
const char *WifiManager::modeName(Mode mode)
{
    switch (mode)
    {
    case Mode::Sta:
        return "sta";
    case Mode::StaAp:
        return "sta_ap";
    case Mode::Ap:
    default:
        return "ap";
    }
}
const char *WifiManager::modeLabel(Mode mode)
{
    switch (mode)
    {
    case Mode::Sta:
        return "STA";
    case Mode::StaAp:
        return "STA+AP";
    case Mode::Ap:
    default:
        return "AP";
    }
}
const char *WifiManager::modeName() const
{ return modeName(_mode); }
const char *WifiManager::modeLabel() const
{ return modeLabel(_mode); }
bool WifiManager::parseMode(const String &input, WifiManager::Mode &out)
{
    String mode = input;
    mode.trim();
    mode.toLowerCase();
    if (mode == "sta")
    {
        out = Mode::Sta;
        return true;
    }
    if (mode == "ap")
    {
        out = Mode::Ap;
        return true;
    }
    if (mode == "sta_ap" || mode == "sta+ap" || mode == "ap_sta")
    {
        out = Mode::StaAp;
        return true;
    }
    return false;
}
const char *WifiManager::statusToString_(wl_status_t st)
{
    static const char kIdle[] PROGMEM = "Idle";
    static const char kNoSsid[] PROGMEM = "No SSID";
    static const char kScanDone[] PROGMEM = "Scan done";
    static const char kConnected[] PROGMEM = "Connected";
    static const char kConnectFailed[] PROGMEM = "Connect failed";
    static const char kConnectionLost[] PROGMEM = "Connection lost";
    static const char kDisconnected[] PROGMEM = "Disconnected";
    static const char kUnknown[] PROGMEM = "Unknown";

    switch (st)
    {
    case WL_IDLE_STATUS:
        return kIdle;
    case WL_NO_SSID_AVAIL:
        return kNoSsid;
    case WL_SCAN_COMPLETED:
        return kScanDone;
    case WL_CONNECTED:
        return kConnected;
    case WL_CONNECT_FAILED:
        return kConnectFailed;
    case WL_CONNECTION_LOST:
        return kConnectionLost;
    case WL_DISCONNECTED:
        return kDisconnected;
    default:
        return kUnknown;
    }
}

void WifiManager::initNetLed_()
{
    if (_net_led_initialized || !_io)
        return;
    const uint8_t pin = ActiveBoardProfile::NET_LED_PIN;
    if (pin == 0xFF)
        return;
    _io->pinMode(pin, PortIO::PortMode::Output);
    _io->write(pin, false);
    _net_led_state = false;
    _net_led_initialized = true;
}

void WifiManager::setNetLed_(bool on)
{
    if (!_io)
        return;
    initNetLed_();
    if (!_net_led_initialized || _net_led_state == on)
        return;
    _io->write(ActiveBoardProfile::NET_LED_PIN, on);
    _net_led_state = on;
}
