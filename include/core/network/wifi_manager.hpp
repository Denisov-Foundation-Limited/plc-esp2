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

class Logger;

class WifiManager
{
public:
    explicit WifiManager(Logger &log);

    bool begin();

    bool restart();

    void task();

    void setSsid(const String &ssid);
    void setPassword(const String &password);
    void setAp(bool ap);
    void setApSsid(const String &ssid);
    void setApPassword(const String &password);

    const String &ssid() const;
    const String &password() const;
    bool ap() const;
    const String &apSsid() const;
    const String &apPassword() const;
    bool isConnected() const;

private:
    static const char *statusToString_(wl_status_t st);

    String _ssid;
    String _password;
    bool _ap = true;
    String _ap_ssid = "FCPLC";
    String _ap_password;

    wl_status_t _last_status = (wl_status_t)0xFF;
    uint8_t _last_ap_clients = 0xFF;
    Logger &_log;
};
