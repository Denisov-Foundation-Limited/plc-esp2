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
#include <WiFi.h>

#include "utils/logger.hpp"

class WifiManager
{
public:
    explicit WifiManager(Logger &log)
        : _log(log)
    {
    }

    bool begin()
    {
        _last_status = (wl_status_t)0xFF;
        _last_ap_clients = 0xFF;

        if (_ap)
        {
            if (_log.ready())
                _log.info(F("WIFI"), F("Mode: AP (SSID: %s)"), _ap_ssid.c_str());
            WiFi.mode(WIFI_AP);
            const bool ok = WiFi.softAP(_ap_ssid.c_str(), _ap_password.c_str());
            if (ok && _log.ready())
            {
                const String ip = WiFi.softAPIP().toString();
                _log.info(F("WIFI"), F("AP IP %s"), ip.c_str());
            }
            return ok;
        }

        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);
        WiFi.mode(WIFI_STA);
        if (_log.ready())
        {
            if (_ssid.length() == 0)
                _log.warn(F("WIFI"), F("STA begin: SSID is empty"));
            else
                _log.info(F("WIFI"), F("STA begin: SSID: %s"), _ssid.c_str());
            if (_password.length() == 0)
                _log.warn(F("WIFI"), F("STA begin: password is empty"));
        }
        WiFi.begin(_ssid.c_str(), _password.c_str());
        return true;
    }

    bool restart()
    {
        if (_log.ready())
            _log.info(F("WIFI"), F("Restart"));
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        delay(200);
        return begin();
    }

    void task()
    {
        if (_ap)
        {
            const uint8_t clients = WiFi.softAPgetStationNum();
            if (clients != _last_ap_clients)
            {
                _last_ap_clients = clients;
                if (_log.ready())
                    _log.info(F("WIFI"), F("AP clients: %u"), clients);
            }
            return;
        }

        const wl_status_t st = WiFi.status();
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

    void setSsid(const String &ssid) { _ssid = ssid; }
    void setPassword(const String &password) { _password = password; }
    void setAp(bool ap) { _ap = ap; }
    void setApSsid(const String &ssid) { _ap_ssid = ssid; }
    void setApPassword(const String &password) { _ap_password = password; }

    const String &ssid() const { return _ssid; }
    const String &password() const { return _password; }
    bool ap() const { return _ap; }
    const String &apSsid() const { return _ap_ssid; }
    const String &apPassword() const { return _ap_password; }
    bool isConnected() const { return !_ap && WiFi.status() == WL_CONNECTED; }

private:
    static const char *statusToString_(wl_status_t st)
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

    String _ssid;
    String _password;
    bool _ap = false;
    String _ap_ssid;
    String _ap_password;

    wl_status_t _last_status = (wl_status_t)0xFF;
    uint8_t _last_ap_clients = 0xFF;
    Logger &_log;
};
