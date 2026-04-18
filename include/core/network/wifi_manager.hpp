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
class IoStack;

class WifiManager
{
public:
    enum class Mode : uint8_t
    {
        Sta = 0,
        Ap,
        StaAp
    };

    explicit WifiManager(Logger &log);

    bool begin();

    bool restart();

    void task();

    void setSsid(const String &ssid);
    void setPassword(const String &password);
    void setMode(Mode mode);
    bool setModeByName(const String &mode);
    void setAp(bool ap);
    void setApSsid(const String &ssid);
    void setApPassword(const String &password);
    void setIo(IoStack &io);

    const String &ssid() const;
    const String &password() const;
    Mode mode() const;
    bool staEnabled() const;
    bool ap() const;
    bool apEnabled() const;
    bool staActive() const;
    bool apActive() const;
    const String &apSsid() const;
    const String &apPassword() const;
    bool isConnected() const;

    static const char *modeName(Mode mode);
    static const char *modeLabel(Mode mode);
    static bool parseMode(const String &input, Mode &out);
    const char *modeName() const;
    const char *modeLabel() const;

private:
    static constexpr uint32_t kStaReconnectPollMs = 5000u;
    static constexpr uint32_t kStaRebeginPollMs = 15000u;

    static const char *statusToString_(wl_status_t st);
    void pollStaReconnect_(wl_status_t st);

    String _ssid;
    String _password;
    Mode _mode = Mode::Ap;
    String _ap_ssid = "FCPLC";
    String _ap_password;

    wl_status_t _last_status = (wl_status_t)0xFF;
    uint8_t _last_ap_clients = 0xFF;
    Logger &_log;
    IoStack *_io = nullptr;
    bool _net_led_initialized = false;
    bool _net_led_state = false;
    uint32_t _last_sta_poll_ms = 0;
    uint32_t _last_sta_begin_ms = 0;

    void initNetLed_();
    void setNetLed_(bool on);
};
