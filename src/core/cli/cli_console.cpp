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

#include "core/cli/cli_console.hpp"
#include "core/network/network.hpp"

#include <ArduinoJson.h>
#include <string.h>

#include "boards/board_profile.hpp"
#include "core/network/tftp_client.hpp"
#include "mbedtls/sha256.h"
#include <Update.h>

CliConsole::CliConsole(PlcControl &plc, WifiManager &wifi, RTC &rtc, Ftest &ftest, I2CManager &i2c, OneWireManager &ow,
           Configs &configs, Extender &ext, Camera &camera,
           UsersRegistry &users,
           Controllers &controllers)
    : _plc(plc),
      _wifi(wifi),
      _rtc(rtc),
      _ftest(ftest),
      _i2c(i2c),
      _ow(ow),
      _configs(configs),
      _ext(ext),
      _camera(camera),
      _users(users),
      _controllers(controllers),
      _wifi_cli(*this),
      _socket_cli(*this, controllers.sockets()),
      _meteo_cli(*this, controllers.meteo()),
      _thermo_cli(*this, controllers.thermo(), controllers.meteo()),
      _tank_cli(*this, controllers.tanks()),
      _septic_cli(*this, controllers.septic()),
      _security_cli(*this, controllers.security()),
      _ring_cli(*this, controllers.ring()),
      _avr_cli(*this, controllers.avr()),
      _leak_cli(*this, controllers.leak()),
      _watering_cli(*this, controllers.watering()),
      _cloud_cli(*this),
      _camera_cli(*this),
      _groups_cli(*this),
      _display_cli(*this),
      _user_cli(*this),
      _enable(*this, _wifi_cli),
      _config(*this, _wifi_cli, _socket_cli, _meteo_cli, _thermo_cli, _tank_cli, _septic_cli,
              _security_cli, _ring_cli, _avr_cli, _leak_cli, _watering_cli, _cloud_cli,
              _camera_cli, _groups_cli, _display_cli, _user_cli)
{
}
void CliConsole::begin(Stream &io)
{
    _raw_io = &io;
    _locked_io.bind(_raw_io);
    _io = &_locked_io;
    _state = State::NeedUser;
    _mode = Mode::Enable;
    _line = "";
    _user_input = "";
    printPrompt_();
}
void CliConsole::loop()
{
    if (!_io)
        return;
    while (_io->available())
    {
        char c = (char)_io->read();
        if (c == '\r')
        {
            _io->println();
            handleLine_(_line);
            _line = "";
            _saw_cr = true;
            continue;
        }
        if (c == '\n')
        {
            if (_saw_cr)
            {
                _saw_cr = false;
                continue;
            }
            _io->println();
            handleLine_(_line);
            _line = "";
            continue;
        }
        if (handleEscape_(c))
            continue;
        _saw_cr = false;
        if (c == 0x7F || c == 0x08)
        {
            if (_line.length() > 0)
            {
                _line.remove(_line.length() - 1);
                if (_state != State::NeedPass)
                    _io->print(F("\b \b"));
            }
            continue;
        }
        if (c == '\t')
        {
            handleTab_();
            continue;
        }
        if (_state != State::NeedPass)
            _io->print(c);
        if (_line.length() < kMaxLine)
            _line += c;
    }
}
void CliConsole::onLoggerOutput_()
{
    if (!_io || _raw_io == nullptr)
        return;
    if ((_state == State::NeedUser || _state == State::NeedPass) && _line.length() == 0)
        return;
    Logger::OutputGuard guard;
    clearPromptLineUnlocked_(*_raw_io);
    printPromptUnlocked_(*_raw_io, false);
    if (_line.length())
        _raw_io->print(_line);
}
bool CliConsole::setAdminPassword_(const String &pass)
{
    if (pass.length() == 0)
        return false;
    uint8_t hash[32] = {};
    sha256_(pass.c_str(), hash);
    memcpy(_admin_hash, hash, sizeof(_admin_hash));
    _admin_set = true;
    return true;
}
bool CliConsole::setAdminPasswordHashHex_(const String &hex)
{
    uint8_t hash[32] = {};
    if (!hexToBytes_(hex, hash))
        return false;
    memcpy(_admin_hash, hash, sizeof(_admin_hash));
    _admin_set = true;
    return true;
}
String CliConsole::adminPasswordHashHex() const
{
    if (!_admin_set)
        return String();
    char out[65] = {};
    bytesToHex_(_admin_hash, out);
    return String(out);
}
bool CliConsole::checkAdminPassword(const String &pass) const
{
    return checkAdmin_(pass.c_str());
}
bool CliConsole::adminPasswordSet() const
{ return _admin_set; }
const String &CliConsole::currentUser() const
{ return _user_input; }
void CliConsole::enterUser()
{ _mode = Mode::Enable; printPrompt_(); }
void CliConsole::enterEnable()
{ _mode = Mode::Enable; printPrompt_(); }
void CliConsole::enterConfig()
{ _mode = Mode::Config; printPrompt_(); }
void CliConsole::enterConfigWifi()
{ _mode = Mode::ConfigWifi; printPrompt_(); }
void CliConsole::enterConfigTime()
{ _mode = Mode::ConfigTime; printPrompt_(); }
void CliConsole::enterConfigSocket()
{ _mode = Mode::ConfigSocket; printPrompt_(); }
void CliConsole::enterConfigMeteo()
{ _mode = Mode::ConfigMeteo; printPrompt_(); }
void CliConsole::enterConfigThermo()
{ _mode = Mode::ConfigThermo; printPrompt_(); }
void CliConsole::enterConfigTank()
{ _mode = Mode::ConfigTank; printPrompt_(); }
void CliConsole::enterConfigSeptic()
{ _mode = Mode::ConfigSeptic; printPrompt_(); }
void CliConsole::enterConfigSecurity()
{ _mode = Mode::ConfigSecurity; printPrompt_(); }
void CliConsole::enterConfigRing()
{ _mode = Mode::ConfigRing; printPrompt_(); }
void CliConsole::enterConfigAvr()
{ _mode = Mode::ConfigAvr; printPrompt_(); }
void CliConsole::enterConfigLeak()
{ _mode = Mode::ConfigLeak; printPrompt_(); }
void CliConsole::enterConfigWatering()
{ _mode = Mode::ConfigWatering; printPrompt_(); }
void CliConsole::enterConfigCloud()
{ _mode = Mode::ConfigCloud; printPrompt_(); }
void CliConsole::enterConfigCamera()
{ _mode = Mode::ConfigCamera; printPrompt_(); }
void CliConsole::enterConfigGroups()
{ _mode = Mode::ConfigGroups; printPrompt_(); }
void CliConsole::enterConfigDisplay()
{ _mode = Mode::ConfigDisplay; printPrompt_(); }
void CliConsole::enterConfigUser()
{ _mode = Mode::ConfigUser; printPrompt_(); }
void CliConsole::logout()
{
    _state = State::NeedUser;
    _mode = Mode::Enable;
    _user_input = "";
    _session_user_idx = -1;
    printPrompt_();
}
void CliConsole::cmdShowPlc_()
{
    printPlcHeader_();
    const float board_t = _plc.boardTemp();
    const bool fan = _plc.fanStatus();
    const float on_c = _plc.fanOnC();
    const float hyst_c = _plc.fanHysteresisC();
    float rtc_t = 0.0f;
    const bool rtc_ok = _rtc.readTemp(rtc_t);
    printPlcRow_("CPU", String(ActiveBoardProfile::UI_NAME), fan, board_t,
                 on_c, hyst_c, rtc_ok ? &rtc_t : nullptr);
}
void CliConsole::cmdShowBoard_()
{
    _io->println(F("Board:"));
    const size_t key_w = 4; // name
    printKeyValue_(F("name"), String(ActiveBoardProfile::UI_NAME), key_w);
}
void CliConsole::cmdShowPort_(uint8_t id)
{
    if (id >= PortIO::PORT_COUNT)
    {
        _io->println(F("Invalid port id"));
        return;
    }
    const auto &p = ActiveBoardProfile::PORTS[id];
    if (p.caps == Cap::None)
    {
        _io->println(F("Port not used"));
        return;
    }
    printPortsHeader_();
    printPortRow_("CPU", id, p);
}
bool CliConsole::gpioPortUsed_(uint8_t id) const
{
    return _controllers.gpioPortUsed(id);
}
void CliConsole::cmdShowPorts_()
{
    _io->println(F("Ports:"));
    printPortsHeader_();
    const auto *devs = _ext.devs();
    for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
    {
        const auto &p = ActiveBoardProfile::PORTS[i];
        if (p.caps == Cap::None)
            continue;
        if (p.backend == PortIO::Backend::Extender)
        {
            const uint8_t dev = p.u.ext.dev;
            if (!devs || dev >= _ext.devCount())
                continue;
            if (devs[dev].type != Extender::Type::MCP23017)
                continue;
            if (!_ext.isPresent(dev))
                continue;
        }
        printPortRow_("CPU", i, p);
    }
}
void CliConsole::cmdShowWifi_()
{
    _io->println(F("Wi-Fi configurations:"));
    const size_t key_w = 11; // ap_password
    printKeyValue_(F("mode"), _wifi.modeLabel(), key_w);
    printKeyValue_(F("ssid"), _wifi.ssid(), key_w);
    printKeyValue_(F("password"), _wifi.password(), key_w);
    printKeyValue_(F("ap_ssid"), _wifi.apSsid(), key_w);
    printKeyValue_(F("ap_password"), _wifi.apPassword(), key_w);
}
void CliConsole::cmdShowTime_()
{
    Ds3231Mz::DateTime dt{};
    if (!_rtc.Time(dt))
    {
        _io->println(F("RTC error"));
        return;
    }
    printRtcHeader_();
    char date_buf[16] = {};
    char time_buf[16] = {};
    snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
             (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
    snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
             (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
    printRtcRow_("CPU", date_buf, time_buf, (unsigned)dt.day_of_week);
}
void CliConsole::cmdShowCloud_()
{
    if (!_configs_manager)
    {
        _io->println(F("Config manager missing"));
        return;
    }
    _io->println(F("Cloud:"));
    const size_t key_w = 12; // reconnect_ms
    printKeyValue_(F("enabled"), _configs_manager->cloudEnabled() ? F("true") : F("false"), key_w);
    printKeyValue_(F("transport"), _configs_manager->cloudTransport() == CloudTransportKind::Http ? F("http") : F("ws"), key_w);
    printKeyValue_(F("host"), _configs_manager->cloudHost(), key_w);
    printKeyValue_(F("port"), String((unsigned)_configs_manager->cloudPort()), key_w);
    printKeyValue_(F("path"), _configs_manager->cloudPath(), key_w);
    printKeyValue_(F("ssl"), _configs_manager->cloudUseSsl() ? F("true") : F("false"), key_w);
    printKeyValue_(F("reconnect_ms"), String((unsigned)_configs_manager->cloudReconnectMs()), key_w);
    printKeyValue_(F("event_ms"), String((unsigned)_configs_manager->cloudEventIntervalMs()), key_w);
    printKeyValue_(F("api_key"), _configs_manager->cloudApiKey(), key_w);
    printKeyValue_(F("fw_version"), _configs_manager->cloudFirmwareVersion(), key_w);
    printKeyValue_(F("device_id"), String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFu)), key_w);
}
void CliConsole::cmdCopy_(const String &line)
{
    String args = line;
    if (args.startsWith("copy"))
        args = args.substring(4);
    args.trim();
    const int space = args.indexOf(' ');
    if (space < 0)
    {
        _io->println(F("Usage: copy tftp://<ip>/firmware.bin firmware"));
        _io->println(F("       copy http://<ip>/firmware.bin firmware"));
        return;
    }
    String url = args.substring(0, space);
    String dest = args.substring(space + 1);
    dest.trim();
    if (dest != "firmware")
    {
        _io->println(F("Only firmware destination supported"));
        return;
    }
    if (url.startsWith("http://"))
    {
        const int slash = url.lastIndexOf('/');
        if (slash < 0 || url.substring(slash + 1) != "firmware.bin")
        {
            _io->println(F("Only firmware.bin supported"));
            return;
        }
#if !defined(ESP32)
        _io->println(F("OTA not supported"));
        return;
#else
        _io->println(F("HTTP download started"));
        HTTPClient http;
        if (!http.begin(url))
        {
            _io->println(F("HTTP begin failed"));
            return;
        }
        const int code = http.GET();
        if (code != HTTP_CODE_OK)
        {
            _io->print(F("HTTP failed: "));
            _io->println(code);
            http.end();
            return;
        }
        const int len = http.getSize();
        if (!Update.begin(len > 0 ? (size_t)len : UPDATE_SIZE_UNKNOWN))
        {
            _io->println(Update.errorString());
            http.end();
            return;
        }
        WiFiClient *stream = http.getStreamPtr();
        const size_t written = Update.writeStream(*stream);
        if (len > 0 && written != (size_t)len)
        {
            Update.abort();
            http.end();
            _io->println(F("HTTP read incomplete"));
            return;
        }
        http.end();
        if (!Update.end(true))
        {
            _io->print(F("Update failed: "));
            _io->println(Update.errorString());
            return;
        }
        _io->println(F("Update OK, rebooting"));
        _io->flush();
        delay(500);
        ESP.restart();
#endif
        return;
    }
    if (!url.startsWith("tftp://"))
    {
        _io->println(F("Only tftp:// or http:// URLs supported"));
        return;
    }
    String target = url.substring(strlen("tftp://"));
    const int slash = target.indexOf('/');
    if (slash <= 0)
    {
        _io->println(F("Invalid TFTP URL"));
        return;
    }
    String host = target.substring(0, slash);
    String file = target.substring(slash + 1);
    if (file != "firmware.bin")
    {
        _io->println(F("Only firmware.bin supported"));
        return;
    }
    IPAddress ip;
    if (!ip.fromString(host))
    {
        _io->println(F("Invalid TFTP host"));
        return;
    }
#if !defined(ESP32)
    _io->println(F("OTA not supported"));
    return;
#else
    _io->println(F("TFTP download started"));
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
        _io->println(Update.errorString());
        return;
    }
    struct OtaCtx
    {
        size_t bytes = 0;
    } ctx;
    auto writer = [](void *c, const uint8_t *data, size_t len) -> bool {
        if (Update.write(const_cast<uint8_t *>(data), len) != len)
            return false;
        OtaCtx *st = static_cast<OtaCtx *>(c);
        st->bytes += len;
        return true;
    };
    TftpClient client;
    if (!client.download(ip, file, writer, &ctx))
    {
        Update.abort();
        _io->print(F("TFTP failed: "));
        _io->println(client.lastError());
        return;
    }
    if (!Update.end(true))
    {
        _io->print(F("Update failed: "));
        _io->println(Update.errorString());
        return;
    }
    _io->println(F("Update OK, rebooting"));
    _io->flush();
    delay(500);
    ESP.restart();
#endif
}
void CliConsole::cmdPhoto_(const String &line)
{
    auto buildCloudPhotoUrl = [this](String &out_url, String &out_err) -> bool
    {
        if (!_configs_manager)
        {
            out_err = F("Config manager missing");
            return false;
        }
        if (!_configs_manager->cloudEnabled())
        {
            out_err = F("Cloud disabled");
            return false;
        }
        const String host = _configs_manager->cloudHost();
        const uint16_t port = _configs_manager->cloudPort();
        if (host.length() == 0 || port == 0)
        {
            out_err = F("Cloud host/port not configured");
            return false;
        }

        String base_path = _configs_manager->cloudPath();
        if (!base_path.startsWith("/"))
            base_path = "/" + base_path;
        const int ws_idx = base_path.indexOf("/ws/");
        if (ws_idx >= 0)
            base_path = base_path.substring(0, ws_idx);
        else if (base_path.endsWith("/ws/device"))
            base_path = base_path.substring(0, base_path.length() - String("/ws/device").length());
        if (!base_path.startsWith("/"))
            base_path = "/" + base_path;
        if (base_path.length() == 0)
            base_path = "/";
        if (!base_path.endsWith("/"))
            base_path += "/";

        out_url = String(_configs_manager->cloudUseSsl() ? "https://" : "http://") +
                  host + ":" + String(port) + base_path + "api/device/photo";
        return true;
    };

    String args = line;
    if (args.startsWith("photo"))
        args = args.substring(5);
    args.trim();
    if (args.length() == 0)
    {
        _io->println(F("Usage: photo get <http://...jpg>"));
        _io->println(F("       photo upload <http://...>"));
        _io->println(F("       photo cloud"));
        _io->println(F("       photo status"));
        _io->println(F("       photo clear"));
        return;
    }
    if (eq_(args, "status"))
    {
        Camera::Snapshot snap{};
        if (!_camera.snapshot(snap))
        {
            _io->println(F("Photo status unavailable"));
            return;
        }
        _io->print(F("Photo: busy: "));
        _io->print(snap.busy ? F("yes") : F("no"));
        _io->print(F(" ok: "));
        _io->print(snap.ok ? F("yes") : F("no"));
        _io->print(F(" op: "));
        _io->print(Camera::opName(snap.op));
        _io->print(F(" size: "));
        _io->print((unsigned)snap.size);
        _io->print(F(" capacity: "));
        _io->print((unsigned)snap.capacity);
        _io->print(F(" http: "));
        _io->print(snap.http_code);
        _io->print(F(" err: "));
        _io->print(Camera::errorName(snap.error));
        if (snap.error_text.length())
        {
            _io->print(F(" text: "));
            _io->print(snap.error_text);
        }
        if (snap.url.length())
        {
            _io->print(F(" url: "));
            _io->print(snap.url);
        }
        _io->println();
        return;
    }
    if (eq_(args, "clear"))
    {
        if (_camera.clear())
            _io->println(F("Photo buffer cleared"));
        else
            _io->println(F("Camera busy"));
        return;
    }
    if (startsWith_(args, "get "))
    {
        String url = args.substring(4);
        url.trim();
        if (_camera.startDownload(url))
            _io->println(F("Photo download scheduled"));
        else
            _io->println(_camera.lastErrorText().length() ? _camera.lastErrorText() : String(F("Photo download start failed")));
        return;
    }
    if (startsWith_(args, "upload "))
    {
        String url = args.substring(7);
        url.trim();
        if (_camera.startUpload(url))
            _io->println(F("Photo upload scheduled"));
        else
            _io->println(_camera.lastErrorText().length() ? _camera.lastErrorText() : String(F("Photo upload start failed")));
        return;
    }
    if (eq_(args, "cloud"))
    {
        const String api_key = _configs_manager ? _configs_manager->cloudApiKey() : String();
        if (api_key.length() == 0)
        {
            _io->println(F("Cloud API key not configured"));
            return;
        }
        String url;
        String err;
        if (!buildCloudPhotoUrl(url, err))
        {
            _io->println(err);
            return;
        }
        if (_camera.startUpload(url, String(F("image/jpeg")), api_key))
            _io->println(F("Photo cloud upload scheduled"));
        else
            _io->println(_camera.lastErrorText().length() ? _camera.lastErrorText() : String(F("Photo cloud upload start failed")));
        return;
    }
    _io->println(F("Usage: photo get <http://...jpg>"));
    _io->println(F("       photo upload <http://...>"));
    _io->println(F("       photo cloud"));
    _io->println(F("       photo status"));
    _io->println(F("       photo clear"));
}
void CliConsole::cmdShowI2c_()
{
    printI2cHeader_();
    bool scanned[3] = {false, false, false};
    for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
    {
        const uint8_t bus = ActiveBoardProfile::I2CS[i].bus_num;
        if (bus < 3 && scanned[bus])
            continue;
        if (bus < 3)
            scanned[bus] = true;
        bool present[127] = {};
        if (!_i2c.scanDevices(bus, present))
            continue;
        for (uint8_t addr = 1; addr < 127; ++addr)
            if (present[addr])
            {
                char addr_buf[8] = {};
                snprintf(addr_buf, sizeof(addr_buf), "0x%02X", addr);
                printI2cRow_("CPU", bus, addr_buf);
            }
    }
}
void CliConsole::cmdShowStack_()
{
    if (!_configs_manager)
    {
        _io->println(F("Config manager missing"));
        return;
    }
    _io->println(F("Stack:"));
    printKeyValue_(F("role"), _configs_manager->stackRole() == ConfigsManagerIface::StackRole::Slave ? F("slave") : F("master"), 13);
    printKeyValue_(F("master_host"), _configs_manager->stackMasterHost(), 13);
    const __FlashStringHelper *policy = F("direct");
    if (_configs_manager->stackExchangePolicy() == ConfigsManagerIface::StackExchangePolicy::Poll)
        policy = F("poll");
    printKeyValue_(F("policy"), policy, 13);
    printKeyValue_(F("transport"),
                   _configs_manager->stackTransport() == ConfigsManagerIface::StackTransportKind::Rs485 ? F("rs485")
                                                                                                         : F("websocket"),
                   13);
    const __FlashStringHelper *payload = F("auto");
    if (_configs_manager->stackPayloadMode() == ConfigsManagerIface::StackPayloadMode::Json)
        payload = F("json");
    else if (_configs_manager->stackPayloadMode() == ConfigsManagerIface::StackPayloadMode::Binary)
        payload = F("binary");
    printKeyValue_(F("payload"), payload, 13);
    printKeyValue_(F("fallback"), _configs_manager->stackFallbackEnabled() ? F("true") : F("false"), 13);
    printKeyValue_(F("fallback_host"), _configs_manager->stackFallbackHost(), 13);
    printKeyValue_(F("controller"), _configs_manager->stackSlaveController() ? F("true") : F("false"), 13);
    printKeyValue_(F("api_key"), _configs_manager->stackApiKey().length() ? F("***") : F(""), 13);
    if (_network)
    {
        const Network::StackDiagnostics diag = _network->stackDiagnostics();
        _io->println(F("Stack runtime:"));
        printKeyValue_(F("state"), String(diag.runtime_state), 13);
        printKeyValue_(F("master_active"), diag.master_active ? F("true") : F("false"), 13);
        printKeyValue_(F("fallback_active"), diag.fallback_active ? F("true") : F("false"), 13);
        printKeyValue_(F("online"), String((unsigned)diag.online_devices), 13);
        printKeyValue_(F("net_lock_ms"), String((unsigned long)diag.network_lock_held_ms), 13);
        printKeyValue_(F("rt_lock_ms"), String((unsigned long)diag.exchange.lock_held_ms), 13);
        printKeyValue_(F("xchg_slave_q"), String((unsigned)diag.exchange.slave_outbox_used), 13);
        printKeyValue_(F("xchg_master_q"), String((unsigned)diag.exchange.master_inbox_used), 13);
        printKeyValue_(F("notify_q"), String((unsigned)diag.exchange.notify_outbox_used), 13);
        printKeyValue_(F("retried"), String((unsigned long)diag.exchange.retried), 13);
        printKeyValue_(F("expired"), String((unsigned long)diag.exchange.expired), 13);
        printKeyValue_(F("dropped"), String((unsigned long)diag.exchange.dropped), 13);
        printKeyValue_(F("rs485_state"), String((unsigned)diag.rs485.bus_state), 13);
        printKeyValue_(F("rs485_tx_q"), String((unsigned)diag.rs485.tx_queue_used), 13);
        printKeyValue_(F("rs485_pending"), String((unsigned)diag.rs485.pending_used), 13);
        printKeyValue_(F("rs485_timeouts"), String((unsigned long)diag.rs485.request_timeouts), 13);
        printKeyValue_(F("rs485_tx_drop"), String((unsigned long)diag.rs485.tx_queue_drops), 13);
        printKeyValue_(F("rs485_pend_drop"), String((unsigned long)diag.rs485.pending_full_drops), 13);
        printKeyValue_(F("rs485_lock_ms"), String((unsigned long)diag.rs485.lock_held_ms), 13);
    }
}
void CliConsole::cmdShowOw_()
{
    printOwHeader_();
    for (uint8_t i = 0; i < ActiveBoardProfile::ONEWIRE_COUNT; ++i)
    {
        OneWireBus *bus = _ow.busPtrByIndex(i);
        if (!bus)
            continue;
        OneWireManager::ScopedBusLock lk(_ow, i, 200);
        if (!lk.locked())
            continue;
        const auto &cfg = ActiveBoardProfile::ONEWIRES[i];
        uint8_t addr[8] = {};
        bus->reset_search();
        while (bus->search(addr))
        {
            if (OneWireBus::crc8(addr, 7) != addr[7])
                continue;
            char hex[17] = {};
            owAddrToHex_(addr, hex);
            printOwRow_("CPU", i, owBusName_(cfg.bus_id), hex);
        }
    }
}
void CliConsole::cmdShowConfig_()
{
    JsonDocument doc;
    if (!_configs.load(doc))
    {
        const char *err = "Unknown error";
        switch (_configs.lastError())
        {
        case Configs::Error::FsMount:
            err = "FS mount failed";
            break;
        case Configs::Error::OpenRead:
            err = "Open read failed";
            break;
        case Configs::Error::OpenWrite:
            err = "Open write failed";
            break;
        case Configs::Error::JsonParse:
            err = "JSON parse failed";
            break;
        case Configs::Error::JsonSerialize:
            err = "JSON serialize failed";
            break;
        default:
            break;
        }
        _io->print(F("Show config failed: "));
        _io->println(err);
        return;
    }
    _io->println(F("Config file:"));
    String pretty;
    if (serializeJsonPretty(doc, pretty) == 0)
        _io->println(F("{}"));
    else
        _io->println(pretty);
}
void CliConsole::cmdFtest_()
{
    _ftest.start();
    _io->println(F("ftest started"));
}
void CliConsole::cmdExtScan_()
{
    _ext.rescan();
    printExtList_();
}
void CliConsole::cmdExtList_()
{
    printExtList_();
}
void CliConsole::cmdWifiRestart_()
{
    if (_wifi.restart())
        _io->println(F("Wi-Fi restarted"));
    else
        _io->println(F("Wi-Fi restart failed"));
}
void CliConsole::cmdRestart_()
{
    _io->println(F("Restarting..."));
    _io->flush();
    ESP.restart();
}
void CliConsole::cmdWriteConfig_()
{
    const bool saved = _configs_manager && _configs_manager->save();
    if (saved)
    {
        _io->println(F("OK"));
        return;
    }

    const char *err = _configs_manager ? "Unknown error" : "Config manager missing";
    switch (_configs.lastError())
    {
    case Configs::Error::FsMount:
        err = "FS mount failed";
        break;
    case Configs::Error::OpenRead:
        err = "Open read failed";
        break;
    case Configs::Error::OpenWrite:
        err = "Open write failed";
        break;
    case Configs::Error::JsonParse:
        err = "JSON parse failed";
        break;
    case Configs::Error::JsonSerialize:
        err = "JSON serialize failed";
        break;
    default:
        break;
    }
    _io->print(F("Write failed: "));
    _io->println(err);
}
bool CliConsole::setStackRole_(ConfigsManagerIface::StackRole role)
{
    if (!_configs_manager)
        return false;
    _configs_manager->setStackRole(role);
    return true;
}

void CliConsole::setNetwork(Network &network)
{
    _network = &network;
}
bool CliConsole::setStackMasterHost_(const String &host)
{
    if (!_configs_manager)
        return false;
    _configs_manager->setStackMasterHost(host);
    return true;
}
bool CliConsole::setStackApiKey_(const String &key)
{
    if (!_configs_manager)
        return false;
    _configs_manager->setStackApiKey(key);
    return true;
}
bool CliConsole::setStackExchangePolicy_(ConfigsManagerIface::StackExchangePolicy policy)
{
    if (!_configs_manager)
        return false;
    _configs_manager->setStackExchangePolicy(policy);
    return true;
}
bool CliConsole::setStackTransport_(ConfigsManagerIface::StackTransportKind kind)
{
    if (!_configs_manager)
        return false;
    _configs_manager->setStackTransport(kind);
    return true;
}
bool CliConsole::setStackPayloadMode_(ConfigsManagerIface::StackPayloadMode mode)
{
    if (!_configs_manager)
        return false;
    _configs_manager->setStackPayloadMode(mode);
    return true;
}
bool CliConsole::setStackFallbackEnabled_(bool enabled)
{
    if (!_configs_manager)
        return false;
    _configs_manager->setStackFallbackEnabled(enabled);
    return true;
}
bool CliConsole::setStackFallbackHost_(const String &host)
{
    if (!_configs_manager)
        return false;
    _configs_manager->setStackFallbackHost(host);
    return true;
}
bool CliConsole::setStackSlaveController_(bool controller)
{
    if (!_configs_manager)
        return false;
    _configs_manager->setStackSlaveController(controller);
    return true;
}
void CliConsole::cmdEraseConfig_()
{
    if (_configs.erase())
    {
        _io->println(F("OK"));
        return;
    }
    const char *err = "Unknown error";
    switch (_configs.lastError())
    {
    case Configs::Error::FsMount:
        err = "FS mount failed";
        break;
    case Configs::Error::OpenRead:
        err = "Open read failed";
        break;
    case Configs::Error::OpenWrite:
        err = "Open write failed";
        break;
    case Configs::Error::JsonParse:
        err = "JSON parse failed";
        break;
    case Configs::Error::JsonSerialize:
        err = "JSON serialize failed";
        break;
    default:
        break;
    }
    _io->print(F("Erase failed: "));
    _io->println(err);
}
void CliConsole::showHelpTopic_(const String &topic)
{
    String t = topic;
    t.toLowerCase();
    if (t == "show")
    {
        _io->println(F("Show commands:"));
        _io->println(F("  show plc        - fan state and board temperature"));
        _io->println(F("  show board      - board profile name"));
        _io->println(F("  show wifi       - Wi-Fi configuration"));
        _io->println(F("  show time       - RTC date/time"));
        _io->println(F("  show i2c        - I2C device list"));
        _io->println(F("  show ow         - OneWire device list"));
        _io->println(F("  show cloud      - Cloud settings"));
        _io->println(F("  show config     - configuration file contents"));
        _io->println(F("  show port <id>  - port details"));
        _io->println(F("  show ports      - list ports"));
        _io->println(F("  show sockets    - list sockets"));
        _io->print(F("  show socket <id>"));
        printSocketIdRangeInline_();
        _io->println(F(" - socket details"));
        _io->println(F("  show meteo      - list meteo sensors"));
        _io->print(F("  show meteo <id>"));
        _meteo_cli.printIdRangeInline();
        _io->println(F(" - sensor details"));
        _io->println(F("  show thermo     - list thermo devices"));
        _io->print(F("  show thermo <id>"));
        _thermo_cli.printIdRangeInline();
        _io->println(F(" - device details"));
        _io->println(F("  show tanks      - list tanks"));
        _io->print(F("  show tank <id>"));
        _tank_cli.printIdRangeInline();
        _io->println(F(" - tank details"));
        _io->println(F("  show watering   - list watering rules"));
        _io->print(F("  show watering <id>"));
        _watering_cli.printIdRangeInline();
        _io->println(F(" - rule details"));
        _io->println(F("  show septic     - list septic"));
        _io->print(F("  show septic <id>"));
        _septic_cli.printIdRangeInline();
        _io->println(F(" - septic details"));
        _io->println(F("  show security   - list security sensors"));
        _io->print(F("  show security <id>"));
        _security_cli.printIdRangeInline();
        _io->println(F(" - sensor details"));
        _io->println(F("  show ring       - ring status"));
        _io->println(F("  show avr        - AVR config/state"));
        _io->println(F("  show leak       - leak zones/state"));
        _io->println(F("  show cameras    - list camera configs"));
        _io->println(F("  show camera <id> - camera details"));
        _io->println(F("  show groups     - list groups"));
        _io->println(F("  show group <id> - group details"));
        _io->println(F("  show display    - list display slots"));
        _io->println(F("  show users      - list users"));
        _io->println(F("  show user <id>  - user details"));
        return;
    }
    if (t == "wifi")
    {
        _wifi_cli.printHelpTopic();
        return;
    }
    if (t == "user")
    {
        _io->println(F("Admin commands:"));
        _io->println(F("  password <pass>         - set admin password"));
        _io->println(F("  admin password <pass>   - set admin password"));
        _io->println(F("  stack role <master|slave>"));
        _io->println(F("  stack master <host>"));
        _io->println(F("  stack policy <direct|poll>"));
        _io->println(F("  stack transport <websocket|rs485>"));
        _io->println(F("  stack payload <auto|json|binary>"));
        _io->println(F("  stack fallback <on|off>"));
        _io->println(F("  stack fallback_host <host>"));
        _io->println(F("  stack slave_controller <on|off>"));
        _io->println(F("  stack api_key <value|clear|gen>"));
        return;
    }
    if (t == "eeprom")
    {
        _io->println(F("EEPROM commands:"));
        _io->println(F("  eeprom show             - show EEPROM save/load flags"));
        _io->println(F("  eeprom save <on|off>    - enable/disable EEPROM periodic save"));
        _io->println(F("  eeprom load <on|off>    - enable/disable EEPROM load on boot"));
        return;
    }
    if (t == "cloud")
    {
        _cloud_cli.printHelpTopic();
        return;
    }
    if (t == "camera")
    {
        _camera_cli.printHelpTopic();
        return;
    }
    if (t == "groups")
    {
        _groups_cli.printHelpTopic();
        return;
    }
    if (t == "display")
    {
        _display_cli.printHelpTopic();
        return;
    }
    if (t == "user")
    {
        _user_cli.printHelpTopic();
        return;
    }
    if (t == "socket")
    {
        _socket_cli.printHelpContextLines();
        return;
    }
    if (t == "meteo")
    {
        _meteo_cli.printHelpContextLines();
        return;
    }
    if (t == "thermo")
    {
        _thermo_cli.printHelpContextLines();
        return;
    }
    if (t == "tank")
    {
        _tank_cli.printHelpContextLines();
        return;
    }
    if (t == "watering")
    {
        _watering_cli.printHelpContextLines();
        return;
    }
    if (t == "septic")
    {
        _septic_cli.printHelpContextLines();
        return;
    }
    if (t == "security")
    {
        _security_cli.printHelpContextLines();
        return;
    }
    if (t == "ring")
    {
        _ring_cli.printHelpContextLines();
        return;
    }
    if (t == "avr")
    {
        _avr_cli.printHelpContextLines();
        return;
    }
    if (t == "leak")
    {
        _leak_cli.printHelpContextLines();
        return;
    }
    if (t == "system")
    {
        _io->println(F("System commands:"));
        _io->println(F("  ftest   - start functional test task"));
        _io->println(F("  photo get <url>"));
        _io->println(F("  photo upload <url>"));
        _io->println(F("  photo cloud"));
        _io->println(F("  photo status"));
        _io->println(F("  photo clear"));
        _io->println(F("  reload  - restart controller"));
        _io->println(F("  reset   - restart controller"));
        _io->println(F("  write   - save configuration"));
        _io->println(F("  erase   - delete configuration"));
        return;
    }
    _io->println(F("Unknown topic"));
}
void CliConsole::handleTab_()
{
    if (!_io || _state != State::LoggedIn)
        return;
    static const std::array<const char *, 87> kEnableCmds = {{
        "show plc",
        "show board",
        "show wifi",
        "show time",
        "show i2c",
        "show ow",
        "show cloud",
        "show config",
        "show ext",
        "show port <id>",
        "show ports",
        "show sockets",
        "show socket <id>",
        "show meteo",
        "show meteo <id>",
        "show thermo",
        "show thermo <id>",
        "show tanks",
        "show tank <id>",
        "show watering",
        "show watering <id>",
        "show septic",
        "show septic <id>",
        "show security",
        "show security <id>",
        "show ring",
        "show avr",
        "show leak",
        "show cameras",
        "show camera <id>",
        "show groups",
        "show group <id>",
        "show display",
        "show users",
        "show user <id>",
        "ring on",
        "ring off",
        "avr on",
        "avr off",
        "avr source <off|main|reserve>",
        "avr clear_fault",
        "socket toggle <id>",
        "socket on <id>",
        "socket off <id>",
        "security status",
        "security arm",
        "security disarm",
        "ftest",
        "copy tftp://<ip>/firmware.bin firmware",
        "copy http://<ip>/firmware.bin firmware",
        "photo get <http://...jpg>",
        "photo upload <http://...>",
        "photo cloud",
        "photo status",
        "photo clear",
        "wifi restart",
        "reload",
        "reset",
        "write",
        "erase",
        "ext scan",
        "configure terminal",
        "conf t",
        "disable",
        "logout",
        "exit",
        "help",
        "help show",
        "help wifi",
        "help user",
        "help system",
        "help cloud",
        "help socket",
        "help meteo",
        "help thermo",
        "help tank",
        "help watering",
        "help septic",
        "help security",
        "help ring",
        "help avr",
        "help leak",
        "help camera",
        "help groups",
        "help display",
        "help user"}};

    static const std::array<const char *, 54> kConfigCmds = {{
        "password <pass>",
        "admin password <pass>",
        "stack role <master|slave>",
        "stack master <host>",
        "stack policy <direct|poll>",
        "stack transport <websocket|rs485>",
        "stack payload <auto|json|binary>",
        "stack fallback <on|off>",
        "stack fallback_host <host>",
        "stack slave_controller <on|off>",
        "stack api_key <value>",
        "eeprom show",
        "eeprom save <on|off>",
        "eeprom load <on|off>",
        "wifi",
        "cloud",
        "time",
        "socket",
        "meteo",
        "thermo",
        "tank",
        "watering",
        "septic",
        "security",
        "ring",
        "avr",
        "leak",
        "camera",
        "groups",
        "display",
        "user",
        "exit",
        "end",
        "help",
        "help show",
        "help wifi",
        "help user",
        "help eeprom",
        "help system",
        "help cloud",
        "help socket",
        "help meteo",
        "help thermo",
        "help tank",
        "help watering",
        "help septic",
        "help security",
        "help ring",
        "help avr",
        "help leak",
        "help camera",
        "help groups",
        "help display",
        "help user"}};

    static const std::array<const char *, 16> kConfigWifiCmds = {{
        "ssid <value>",
        "password <value>",
        "ap on",
        "ap off",
        "ap_ssid <value>",
        "ap_password <value>",
        "restart",
        "show",
        "exit",
        "end",
        "help",
        "help show",
        "help wifi",
        "help user",
        "help system",
        "help security"}};

    static const std::array<const char *, 8> kConfigTimeCmds = {{
        "date <YYYY-MM-DD>",
        "time <HH:MM:SS>",
        "set <YYYY-MM-DD> <HH:MM:SS>",
        "show",
        "exit",
        "end",
        "help",
        "help security"}};

    static const std::array<const char *, 11> kConfigSocketCmds = {{
        "show",
        "show <id>",
        "enable <id>",
        "disable <id>",
        "name <id> <value>",
        "button <id> <port|none>",
        "relay <id> <port|none>",
        "exit",
        "end",
        "help",
        "help security"}};

    static const std::array<const char *, 11> kConfigMeteoCmds = {{
        "show",
        "show <id>",
        "enable <id>",
        "disable <id>",
        "type <id> <none|ds18b20|dht22>",
        "addr <id> <hex|none>",
        "pin <id> <pin|none>",
        "exit",
        "end",
        "help",
        "help security"}};

    static const std::array<const char *, 15> kConfigThermoCmds = {{
        "show",
        "show <id>",
        "enable <id>",
        "disable <id>",
        "mode <id> <off|heat|cool|auto>",
        "sensor <id> <sensor|none>",
        "target <id> <temp>",
        "hyst <id> <temp>",
        "heat <id> <port|none>",
        "cool <id> <port|none>",
        "button <id> <port|none>",
        "exit",
        "end",
        "help",
        "help security"}};

    static const std::array<const char *, 16> kConfigTankCmds = {{
        "show",
        "show <id>",
        "enable <id>",
        "disable <id>",
        "power <id> <0|1>",
        "name <id> <value>",
        "low <id> <port|none>",
        "mid <id> <port|none>",
        "full <id> <port|none>",
        "valve <id> <port|none>",
        "pump <id> <port|none>",
        "alarm <id> <port|none>",
        "exit",
        "end",
        "help",
        "help security"}};

    static const std::array<const char *, 12> kConfigSepticCmds = {{
        "show",
        "show <id>",
        "enable <id>",
        "disable <id>",
        "name <id> <value>",
        "warning <id> <port|none>",
        "alarm <id> <port|none>",
        "relay_warn <id> <port|none>",
        "relay_alarm <id> <port|none>",
        "exit",
        "end",
        "help"}};

    static const std::array<const char *, 23> kConfigSecurityCmds = {{
        "show",
        "show <id>",
        "enable <id>",
        "disable <id>",
        "type <id> <pir|reed>",
        "port <id> <port|none>",
        "name <id> <text>",
        "siren <port|none>",
        "keys list",
        "key add <hex16> [name]",
        "key name <hex16> <text>",
        "key del <hex16>",
        "key clear",
        "phones list",
        "phone set <id> <num|none> [name]",
        "phone name <id> <text>",
        "phone notify <id> <on|off>",
        "phone call <id> <on|off>",
        "phone enable <id> <on|off>",
        "phone clear",
        "exit",
        "end",
        "help"}};

    static const std::array<const char *, 10> kConfigRingCmds = {{
        "show",
        "on",
        "off",
        "enable",
        "disable",
        "button <port|none>",
        "relay <port|none>",
        "exit",
        "end",
        "help"}};

    static const std::array<const char *, 29> kConfigAvrCmds = {{
        "show",
        "enable",
        "disable",
        "mode <auto|manual>",
        "source <off|main|reserve>",
        "prefer_main <on|off>",
        "auto_return <on|off>",
        "main_ok <port|none>",
        "reserve_ok <port|none>",
        "relay_main <port|none>",
        "relay_reserve <port|none>",
        "fb_main <port|none>",
        "fb_reserve <port|none>",
        "main_ok_al <on|off>",
        "reserve_ok_al <on|off>",
        "fb_main_al <on|off>",
        "fb_reserve_al <on|off>",
        "relay_main_inv <on|off>",
        "relay_reserve_inv <on|off>",
        "debounce <ms>",
        "loss_delay <ms>",
        "return_delay <ms>",
        "break <ms>",
        "warmup <ms>",
        "timeout <ms>",
        "clear_fault",
        "exit",
        "end",
        "help"}};

    static const std::array<const char *, 15> kConfigLeakCmds = {{
        "show",
        "show <id>",
        "enable",
        "disable",
        "zone enable <id>",
        "zone disable <id>",
        "power <id> <on|off>",
        "sensor <id> <port|none>",
        "valve <id> <port|none>",
        "alarm <id> <port|none>",
        "active_low <id> <on|off>",
        "name <id> <text>",
        "ack <id|all>",
        "exit",
        "help"}};

    static const std::array<const char *, 23> kConfigWateringCmds = {{
        "show",
        "show <id>",
        "name <id> <text>",
        "enable <id>",
        "disable <id>",
        "status <id> <on|off>",
        "port <id> <port|none>",
        "tank <id> <tank_id|none>",
        "days <id> <mon,tue,...|all|none>",
        "time <id> <HH:MM>",
        "time2 <id> <HH:MM>",
        "time3 <id> <HH:MM>",
        "slot1 <id> <on|off>",
        "slot2 <id> <on|off>",
        "slot3 <id> <on|off>",
        "duration <id> <min>",
        "duration2 <id> <min>",
        "duration3 <id> <min>",
        "resume <id> <on|off>",
        "resume_level <id> <low|mid|full>",
        "force <id> <on|off>",
        "exit",
        "help"}};

    static const std::array<const char *, 17> kConfigCloudCmds = {{
        "enable on",
        "enable off",
        "transport ws",
        "transport http",
        "host <value>",
        "port <num>",
        "path <value>",
        "ssl on",
        "ssl off",
        "reconnect <ms>",
        "event <ms>",
        "api_key <value>",
        "api_key clear",
        "show",
        "exit",
        "end",
        "help"}};

    static const std::array<const char *, 11> kConfigCameraCmds = {{
        "show",
        "show <id>",
        "enable <id>",
        "disable <id>",
        "name <id> <text>",
        "url <id> <value>",
        "user <id> <value|clear>",
        "password <id> <value|clear>",
        "exit",
        "end",
        "help"}};

    static const std::array<const char *, 10> kConfigGroupsCmds = {{
        "show",
        "show <id>",
        "add <name>",
        "name <id> <text>",
        "sort <id> <num>",
        "delete <id>",
        "exit",
        "end",
        "help",
        "help groups"}};

    static const std::array<const char *, 10> kConfigDisplayCmds = {{
        "show",
        "show <slot>",
        "clear <slot>",
        "text <slot> <text>",
        "set <slot> <kind> <field> [index] [node]",
        "exit",
        "end",
        "help",
        "help display",
        "help show"}};

    static const std::array<const char *, 20> kConfigUserCmds = {{
        "show",
        "show <id>",
        "enable <id>",
        "disable <id>",
        "username <id> <text>",
        "password <id> <text|clear>",
        "phone <id> <num|none>",
        "sms <id> <on|off>",
        "call <id> <on|off>",
        "ibutton <id> <hex|clear>",
        "rfid <id> <hex|clear>",
        "acl show <id> [unit]",
        "acl controller <id> <unit> <ctrl> <on|off>",
        "acl view <id> <unit> <ctrl> <item> <on|off>",
        "acl control <id> <unit> <ctrl> <item> <on|off>",
        "acl clear <id> <unit>",
        "acl grant <id> <unit>",
        "exit",
        "end",
        "help"}};

    const char *const *cmds = nullptr;
    size_t count = 0;
    switch (_mode)
    {
    case Mode::Enable:
        cmds = kEnableCmds.data();
        count = kEnableCmds.size();
        break;
    case Mode::Config:
        cmds = kConfigCmds.data();
        count = kConfigCmds.size();
        break;
    case Mode::ConfigWifi:
        cmds = kConfigWifiCmds.data();
        count = kConfigWifiCmds.size();
        break;
    case Mode::ConfigTime:
        cmds = kConfigTimeCmds.data();
        count = kConfigTimeCmds.size();
        break;
    case Mode::ConfigSocket:
        cmds = kConfigSocketCmds.data();
        count = kConfigSocketCmds.size();
        break;
    case Mode::ConfigMeteo:
        cmds = kConfigMeteoCmds.data();
        count = kConfigMeteoCmds.size();
        break;
    case Mode::ConfigThermo:
        cmds = kConfigThermoCmds.data();
        count = kConfigThermoCmds.size();
        break;
    case Mode::ConfigTank:
        cmds = kConfigTankCmds.data();
        count = kConfigTankCmds.size();
        break;
    case Mode::ConfigSeptic:
        cmds = kConfigSepticCmds.data();
        count = kConfigSepticCmds.size();
        break;
    case Mode::ConfigSecurity:
        cmds = kConfigSecurityCmds.data();
        count = kConfigSecurityCmds.size();
        break;
    case Mode::ConfigRing:
        cmds = kConfigRingCmds.data();
        count = kConfigRingCmds.size();
        break;
    case Mode::ConfigAvr:
        cmds = kConfigAvrCmds.data();
        count = kConfigAvrCmds.size();
        break;
    case Mode::ConfigLeak:
        cmds = kConfigLeakCmds.data();
        count = kConfigLeakCmds.size();
        break;
    case Mode::ConfigWatering:
        cmds = kConfigWateringCmds.data();
        count = kConfigWateringCmds.size();
        break;
    case Mode::ConfigCloud:
        cmds = kConfigCloudCmds.data();
        count = kConfigCloudCmds.size();
        break;
    case Mode::ConfigCamera:
        cmds = kConfigCameraCmds.data();
        count = kConfigCameraCmds.size();
        break;
    case Mode::ConfigGroups:
        cmds = kConfigGroupsCmds.data();
        count = kConfigGroupsCmds.size();
        break;
    case Mode::ConfigDisplay:
        cmds = kConfigDisplayCmds.data();
        count = kConfigDisplayCmds.size();
        break;
    case Mode::ConfigUser:
        cmds = kConfigUserCmds.data();
        count = kConfigUserCmds.size();
        break;
    case Mode::User:
        cmds = kEnableCmds.data();
        count = kEnableCmds.size();
        break;
    }

    auto isPlaceholder = [](const String &tok) -> bool
    {
        return tok.length() >= 3 && tok[0] == '<' && tok[tok.length() - 1] == '>';
    };
    auto tokenize = [](const String &s, String *out, size_t max, bool &ends_space) -> size_t
    {
        ends_space = (s.length() > 0 && s[s.length() - 1] == ' ');
        size_t count_out = 0;
        String cur;
        for (size_t i = 0; i < s.length(); ++i)
        {
            char ch = s[i];
            if (ch == ' ')
            {
                if (cur.length() > 0)
                {
                    if (count_out < max)
                        out[count_out++] = cur;
                    cur = "";
                }
            }
            else
            {
                cur += ch;
            }
        }
        if (cur.length() > 0 && count_out < max)
            out[count_out++] = cur;
        if (ends_space && count_out < max)
            out[count_out++] = "";
        return count_out;
    };
    auto eqTok = [](const String &a, const String &b) -> bool
    {
        String la = a;
        String lb = b;
        la.toLowerCase();
        lb.toLowerCase();
        return la == lb;
    };
    auto startsWithTok = [](const String &a, const String &b) -> bool
    {
        String la = a;
        String lb = b;
        la.toLowerCase();
        lb.toLowerCase();
        return la.startsWith(lb);
    };

    String in_tokens[6];
    bool ends_space = false;
    size_t in_count = tokenize(_line, in_tokens, 6, ends_space);

    int match_idx = -1;
    size_t matches = 0;
    bool single_placeholder = false;

    for (size_t i = 0; i < count; ++i)
    {
        String cmd = cmds[i];
        String cmd_tokens[6];
        bool cmd_space = false;
        size_t cmd_count = tokenize(cmd, cmd_tokens, 6, cmd_space);
        if (in_count > cmd_count)
            continue;

        bool ok = true;
        for (size_t t = 0; t < in_count; ++t)
        {
            const String &in_tok = in_tokens[t];
            const String &cmd_tok = cmd_tokens[t];
            const bool is_last = (t == in_count - 1);
            if (is_last)
            {
                if (isPlaceholder(cmd_tok))
                {
                    ok = false;
                    break;
                }
                if (in_tok.length() > 0 && !startsWithTok(cmd_tok, in_tok))
                {
                    ok = false;
                    break;
                }
            }
            else
            {
                if (!eqTok(cmd_tok, in_tok))
                {
                    ok = false;
                    break;
                }
            }
        }
        if (!ok)
            continue;
        match_idx = (int)i;
        ++matches;
        if (in_count > 0 && cmd_count >= in_count)
        {
            String tok = cmd_tokens[in_count - 1];
            if (isPlaceholder(tok))
                single_placeholder = true;
        }
    }

    if (matches == 0)
        return;
    if (matches == 1 && match_idx >= 0 && !single_placeholder)
    {
        String cmd = cmds[match_idx];
        String cmd_tokens[6];
        bool cmd_space = false;
        size_t cmd_count = tokenize(cmd, cmd_tokens, 6, cmd_space);

        if (in_count == 0)
            return;
        const String &cmd_tok = cmd_tokens[in_count - 1];
        const String &in_tok = in_tokens[in_count - 1];
        if (cmd_tok.length() > in_tok.length())
        {
            String suffix = cmd_tok.substring(in_tok.length());
            _io->print(suffix);
            _line += suffix;
        }
        if (cmd_count > in_count)
        {
            _io->print(' ');
            _line += ' ';
        }
        return;
    }

    _io->println();
    for (size_t i = 0; i < count; ++i)
    {
        String cmd = cmds[i];
        String cmd_tokens[6];
        bool cmd_space = false;
        size_t cmd_count = tokenize(cmd, cmd_tokens, 6, cmd_space);
        if (in_count > cmd_count)
            continue;
        bool ok = true;
        for (size_t t = 0; t < in_count; ++t)
        {
            const String &in_tok = in_tokens[t];
            const String &cmd_tok = cmd_tokens[t];
            const bool is_last = (t == in_count - 1);
            if (is_last)
            {
                if (isPlaceholder(cmd_tok))
                {
                    ok = false;
                    break;
                }
                if (in_tok.length() > 0 && !startsWithTok(cmd_tok, in_tok))
                {
                    ok = false;
                    break;
                }
            }
            else if (!eqTok(cmd_tok, in_tok))
            {
                ok = false;
                break;
            }
        }
        if (!ok)
            continue;
        _io->print(F("  "));
        _io->println(cmd);
    }
    printPrompt_();
    _io->print(_line);
}
void CliConsole::handleShow_(String what)
{
    what.trim();
    if (startsWith_(what, "port "))
    {
        String tail = what.substring(5);
        tail.trim();
        if (tail.length() == 0)
        {
            _io->println(F("Usage: show port <id>"));
        }
        else
        {
            const int id = tail.toInt();
            if (id < 0 || id >= PortIO::PORT_COUNT)
                _io->println(F("Invalid port id"));
            else
                cmdShowPort_((uint8_t)id);
        }
    }
    else if (eq_(what, "ports"))
        cmdShowPorts_();
    else if (eq_(what, "ext"))
        cmdExtList_();
    else if (eq_(what, "plc"))
        cmdShowPlc_();
    else if (eq_(what, "board"))
        cmdShowBoard_();
    else if (eq_(what, "wifi"))
        cmdShowWifi_();
    else if (eq_(what, "time"))
        cmdShowTime_();
    else if (eq_(what, "i2c"))
        cmdShowI2c_();
    else if (eq_(what, "ow"))
        cmdShowOw_();
    else if (eq_(what, "stack"))
        cmdShowStack_();
    else if (eq_(what, "cloud"))
        cmdShowCloud_();
    else if (eq_(what, "config"))
        cmdShowConfig_();
    else if (eq_(what, "sockets"))
        _socket_cli.showSockets();
    else if (startsWith_(what, "socket "))
    {
        String tail = what.substring(7);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
        {
            _io->print(F("Usage: show socket <id>"));
            printSocketIdRangeInline_();
            _io->println();
        }
        else
            _socket_cli.showSocket(id);
    }
    else if (eq_(what, "meteo"))
        _meteo_cli.showSensors();
    else if (startsWith_(what, "meteo "))
    {
        String tail = what.substring(6);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
        {
            _io->print(F("Usage: show meteo <id>"));
            _meteo_cli.printIdRangeInline();
            _io->println();
        }
        else
            _meteo_cli.showSensor(id);
    }
    else if (eq_(what, "thermo"))
        _thermo_cli.showDevices();
    else if (startsWith_(what, "thermo "))
    {
        String tail = what.substring(7);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
        {
            _io->print(F("Usage: show thermo <id>"));
            _thermo_cli.printIdRangeInline();
            _io->println();
        }
        else
            _thermo_cli.showDevice(id);
    }
    else if (eq_(what, "tanks"))
        _tank_cli.showTanks();
    else if (startsWith_(what, "tank "))
    {
        String tail = what.substring(5);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
        {
            _io->print(F("Usage: show tank <id>"));
            _tank_cli.printIdRangeInline();
            _io->println();
        }
        else
            _tank_cli.showTank(id);
    }
    else if (eq_(what, "watering"))
        _watering_cli.showRules();
    else if (startsWith_(what, "watering "))
    {
        String tail = what.substring(9);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
        {
            _io->print(F("Usage: show watering <id>"));
            _watering_cli.printIdRangeInline();
            _io->println();
        }
        else
            _watering_cli.showRule(id);
    }
    else if (eq_(what, "septic"))
        _septic_cli.showSeptic();
    else if (startsWith_(what, "septic "))
    {
        String tail = what.substring(7);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
        {
            _io->print(F("Usage: show septic <id>"));
            _septic_cli.printIdRangeInline();
            _io->println();
        }
        else
            _septic_cli.showSeptic(id);
    }
    else if (eq_(what, "security"))
        _security_cli.showSensors();
    else if (startsWith_(what, "security "))
    {
        String tail = what.substring(9);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
        {
            _io->print(F("Usage: show security <id>"));
            _security_cli.printIdRangeInline();
            _io->println();
        }
        else
            _security_cli.showSensor(id);
    }
    else if (eq_(what, "ring"))
        _ring_cli.showRing();
    else if (eq_(what, "avr"))
        _avr_cli.showAvr();
    else if (eq_(what, "leak"))
        _leak_cli.showAll();
    else if (eq_(what, "cameras"))
        _camera_cli.showCameras();
    else if (startsWith_(what, "camera "))
    {
        String tail = what.substring(7);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
            _io->println(F("Usage: show camera <id>"));
        else
            _camera_cli.showCamera((uint8_t)id);
    }
    else if (eq_(what, "groups"))
        _groups_cli.showGroups();
    else if (startsWith_(what, "group "))
    {
        String tail = what.substring(6);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
            _io->println(F("Usage: show group <id>"));
        else
            _groups_cli.showGroup((uint8_t)id);
    }
    else if (eq_(what, "display"))
        _display_cli.showSlots();
    else if (eq_(what, "users"))
        _user_cli.showUsers();
    else if (startsWith_(what, "user "))
    {
        String tail = what.substring(5);
        tail.trim();
        uint16_t id = 0;
        if (!parseUint_(tail, id))
            _io->println(F("Usage: show user <id>"));
        else
            _user_cli.showUser((uint8_t)id);
    }
    else
        _io->println(F("Unknown show"));
    printPrompt_();
}
bool CliConsole::eq_(const String &a, const char *b)
{
    String t = a;
    t.toLowerCase();
    return t == b;
}
bool CliConsole::startsWith_(const String &a, const char *b)
{
    String t = a;
    t.toLowerCase();
    return t.startsWith(b);
}
bool CliConsole::parseUint_(const String &s, uint16_t &out)
{
    if (s.length() == 0)
        return false;
    for (size_t i = 0; i < s.length(); ++i)
    {
        char c = s[i];
        if (c < '0' || c > '9')
            return false;
    }
    out = (uint16_t)s.toInt();
    return true;
}
const UsersRegistry::User *CliConsole::sessionUser_() const
{
    if (_session_user_idx < 0)
        return nullptr;
    const auto users_guard = _users.guard();
    if ((size_t)_session_user_idx >= _users.size())
        return nullptr;
    const auto &u = _users.user((size_t)_session_user_idx);
    if (!u.enabled)
        return nullptr;
    _session_user_cache = u;
    return &_session_user_cache;
}
bool CliConsole::cliSessionIsAdmin_() const
{
    const auto *u = sessionUser_();
    return u && u->tg_admin;
}
bool CliConsole::cliAclControllerAllowed_(UsersRegistry::AclController ctrl, uint8_t unit) const
{
    const auto *u = sessionUser_();
    if (!u)
        return false;
    if (unit >= UsersRegistry::kAclUnitCount)
        return false;
    return u->controllerAllowed(unit, ctrl);
}
bool CliConsole::cliAclCanViewItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint8_t unit) const
{
    const auto *u = sessionUser_();
    if (!u)
        return false;
    if (unit >= UsersRegistry::kAclUnitCount)
        return false;
    return u->canViewItem(unit, ctrl, item_id);
}
bool CliConsole::cliAclCanControlItem_(UsersRegistry::AclController ctrl, uint16_t item_id, uint8_t unit) const
{
    const auto *u = sessionUser_();
    if (!u)
        return false;
    if (unit >= UsersRegistry::kAclUnitCount)
        return false;
    return u->canControlItem(unit, ctrl, item_id);
}
bool CliConsole::cliAclAnyView_(UsersRegistry::AclController ctrl, uint16_t max_item_id, uint8_t unit) const
{
    for (uint16_t id = 1; id <= max_item_id; ++id)
        if (cliAclCanViewItem_(ctrl, id, unit))
            return true;
    return false;
}
bool CliConsole::denyAcl_()
{
    _io->println(F("ACL deny"));
    printPrompt_();
    return false;
}
bool CliConsole::enforceAclShow_(String what)
{
    what.trim();
    if (eq_(what, "sockets"))
        return cliAclAnyView_(UsersRegistry::AclController::Sockets, 72);
    if (startsWith_(what, "socket "))
    {
        String tail = what.substring(7);
        tail.trim();
        uint16_t id = 0;
        return !parseUint_(tail, id) || cliAclCanViewItem_(UsersRegistry::AclController::Sockets, id);
    }
    if (eq_(what, "meteo"))
        return cliAclAnyView_(UsersRegistry::AclController::Meteo, 50);
    if (startsWith_(what, "meteo "))
    {
        String tail = what.substring(6);
        tail.trim();
        uint16_t id = 0;
        return !parseUint_(tail, id) || cliAclCanViewItem_(UsersRegistry::AclController::Meteo, id);
    }
    if (eq_(what, "thermo"))
        return cliAclAnyView_(UsersRegistry::AclController::Thermo, 20);
    if (startsWith_(what, "thermo "))
    {
        String tail = what.substring(7);
        tail.trim();
        uint16_t id = 0;
        return !parseUint_(tail, id) || cliAclCanViewItem_(UsersRegistry::AclController::Thermo, id);
    }
    if (eq_(what, "tanks"))
        return cliAclAnyView_(UsersRegistry::AclController::Tanks, 20);
    if (startsWith_(what, "tank "))
    {
        String tail = what.substring(5);
        tail.trim();
        uint16_t id = 0;
        return !parseUint_(tail, id) || cliAclCanViewItem_(UsersRegistry::AclController::Tanks, id);
    }
    if (eq_(what, "watering"))
        return cliAclAnyView_(UsersRegistry::AclController::Watering, 30);
    if (startsWith_(what, "watering "))
    {
        String tail = what.substring(9);
        tail.trim();
        uint16_t id = 0;
        return !parseUint_(tail, id) || cliAclCanViewItem_(UsersRegistry::AclController::Watering, id);
    }
    if (eq_(what, "septic"))
        return cliAclCanViewItem_(UsersRegistry::AclController::Septic, 1);
    if (startsWith_(what, "septic "))
        return cliAclCanViewItem_(UsersRegistry::AclController::Septic, 1);
    if (eq_(what, "security"))
        return cliAclAnyView_(UsersRegistry::AclController::Security, 72);
    if (startsWith_(what, "security "))
    {
        String tail = what.substring(9);
        tail.trim();
        uint16_t id = 0;
        return !parseUint_(tail, id) || cliAclCanViewItem_(UsersRegistry::AclController::Security, id);
    }
    if (eq_(what, "ring"))
        return cliAclCanViewItem_(UsersRegistry::AclController::Ring, 1);
    if (eq_(what, "avr"))
        return cliAclCanViewItem_(UsersRegistry::AclController::Avr, 1);
    if (eq_(what, "leak"))
        return cliAclAnyView_(UsersRegistry::AclController::Leak, 16);
    return true;
}
bool CliConsole::enforceAclEnable_(const String &line)
{
    String cmd = line;
    cmd.trim();
    String low = cmd;
    low.toLowerCase();
    if (startsWith_(low, "show "))
        return enforceAclShow_(cmd.substring(5));
    if (startsWith_(low, "socket toggle ") || startsWith_(low, "socket on ") || startsWith_(low, "socket off "))
    {
        String tail = startsWith_(low, "socket toggle ") ? cmd.substring(14) : cmd.substring(startsWith_(low, "socket on ") ? 10 : 11);
        tail.trim();
        uint16_t id = 0;
        return !parseUint_(tail, id) || cliAclCanControlItem_(UsersRegistry::AclController::Sockets, id);
    }
    if (low == "ring on" || low == "ring off")
        return cliAclCanControlItem_(UsersRegistry::AclController::Ring, 1);
    if (low == "avr on" || low == "avr off" || low == "avr clear_fault" || startsWith_(low, "avr source "))
        return cliAclCanControlItem_(UsersRegistry::AclController::Avr, 1);
    if (low == "security status")
        return cliAclAnyView_(UsersRegistry::AclController::Security, 72);
    if (low == "security arm" || low == "security disarm")
        return cliAclCanControlItem_(UsersRegistry::AclController::Security, 1);
    if (startsWith_(low, "configure terminal") || startsWith_(low, "conf t"))
        return false;
    if (low == "write" || low == "erase" || low == "reload" || low == "reset" || startsWith_(low, "copy ") ||
        startsWith_(low, "photo "))
        return false;
    return true;
}
bool CliConsole::enforceAcl_(const String &line)
{
    if (cliSessionIsAdmin_())
        return true;
    switch (_mode)
    {
    case Mode::Enable:
        return enforceAclEnable_(line);
    case Mode::Config:
    case Mode::ConfigWifi:
    case Mode::ConfigTime:
    case Mode::ConfigSocket:
    case Mode::ConfigMeteo:
    case Mode::ConfigThermo:
    case Mode::ConfigTank:
    case Mode::ConfigSeptic:
    case Mode::ConfigSecurity:
    case Mode::ConfigRing:
    case Mode::ConfigAvr:
    case Mode::ConfigLeak:
    case Mode::ConfigWatering:
    case Mode::ConfigCloud:
        return false;
    case Mode::User:
        return true;
    }
    return false;
}
void CliConsole::handleLine_(String line)
{
    line.trim();
    if (_state != State::LoggedIn)
    {
        handleLogin_(line);
        return;
    }
    if (line.length() == 0)
    {
        printPrompt_();
        return;
    }
    beginCmdOutput_();
    if (_state == State::LoggedIn)
        addHistory_(line);
    if (!enforceAcl_(line))
    {
        denyAcl_();
        return;
    }

    switch (_mode)
    {
    case Mode::Enable:
        _enable.handle(line);
        break;
    case Mode::Config:
        _config.handle(line);
        break;
    case Mode::ConfigWifi:
        _config.handleWifiContext(line);
        break;
    case Mode::ConfigTime:
        _config.handleTimeContext(line);
        break;
    case Mode::ConfigSocket:
        _config.handleSocketContext(line);
        break;
    case Mode::ConfigMeteo:
        _config.handleMeteoContext(line);
        break;
    case Mode::ConfigThermo:
        _config.handleThermoContext(line);
        break;
    case Mode::ConfigTank:
        _config.handleTankContext(line);
        break;
    case Mode::ConfigSeptic:
        _config.handleSepticContext(line);
        break;
    case Mode::ConfigSecurity:
        _config.handleSecurityContext(line);
        break;
    case Mode::ConfigRing:
        _config.handleRingContext(line);
        break;
    case Mode::ConfigAvr:
        _config.handleAvrContext(line);
        break;
    case Mode::ConfigLeak:
        _config.handleLeakContext(line);
        break;
    case Mode::ConfigWatering:
        _config.handleWateringContext(line);
        break;
    case Mode::ConfigCloud:
        _config.handleCloudContext(line);
        break;
    case Mode::ConfigCamera:
        _config.handleCameraContext(line);
        break;
    case Mode::ConfigGroups:
        _config.handleGroupsContext(line);
        break;
    case Mode::ConfigDisplay:
        _config.handleDisplayContext(line);
        break;
    case Mode::ConfigUser:
        _config.handleUserContext(line);
        break;
    case Mode::User:
        _enable.handle(line);
        break;
    }
}
void CliConsole::handleLogin_(const String &line)
{
    if (_state == State::NeedUser)
    {
        if (line.length() == 0)
        {
            printPrompt_();
            return;
        }
        _user_input = line;
        _session_user_idx = -1;
        _state = State::NeedPass;
        printPrompt_();
        return;
    }

    if (_state == State::NeedPass)
    {
        int matched_idx = -1;
        const String normalized = UsersRegistry::normalizeUsername(_user_input);
        const auto users_guard = _users.guard();
        for (size_t i = 0; i < _users.size(); ++i)
        {
            const auto &u = _users.user(i);
            if (!u.enabled || u.username.length() == 0)
                continue;
            if (UsersRegistry::normalizeUsername(u.username) == normalized)
            {
                matched_idx = (int)i;
                break;
            }
        }

        if (matched_idx >= 0)
        {
            const auto &u = _users.user((size_t)matched_idx);
            if (u.checkWebPassword(line))
            {
                _state = State::LoggedIn;
                _mode = Mode::Enable;
                _session_user_idx = matched_idx;
                printPrompt_();
                return;
            }
        }
        printLine_(F("Login invalid"));
        _state = State::NeedUser;
        _user_input = "";
        _session_user_idx = -1;
        printPrompt_();
    }
}
void CliConsole::printPrompt_()
{
    if (!_io)
        return;
    Stream *out = _raw_io ? _raw_io : _io;
    if (!out)
        return;
    Logger::OutputGuard guard;
    if (_cmd_blank_after)
    {
        out->println();
        _cmd_blank_after = false;
    }
    printPromptUnlocked_(*out, true);
}
bool CliConsole::handleEscape_(char c)
{
    if (_esc_state == 0)
    {
        if ((uint8_t)c == 0x1B)
        {
            _esc_state = 1;
            return true;
        }
        return false;
    }
    if (_esc_state == 1)
    {
        if (c == '[')
        {
            _esc_state = 2;
            return true;
        }
        _esc_state = 0;
        return false;
    }
    if (_esc_state == 2)
    {
        _esc_state = 0;
        if (_state != State::LoggedIn)
            return true;
        if (c == 'A')
        {
            historyUp_();
            return true;
        }
        if (c == 'B')
        {
            historyDown_();
            return true;
        }
        return true;
    }
    _esc_state = 0;
    return false;
}
void CliConsole::redrawLine_(const String &new_line, size_t old_len)
{
    Stream *out = _raw_io ? _raw_io : _io;
    if (!out)
        return;
    Logger::OutputGuard guard;
    const size_t old_visible = old_len > new_line.length() ? old_len : new_line.length();
    clearPromptLineUnlocked_(*out, old_visible);
    printPromptUnlocked_(*out, false);
    out->print(new_line);
}

size_t CliConsole::promptWidth_() const
{
    if (_state == State::NeedUser)
        return 7;
    if (_state == State::NeedPass)
        return 10;
    switch (_mode)
    {
    case Mode::User:
        return 5;
    case Mode::Enable:
        return 5;
    case Mode::Config:
        return 13;
    case Mode::ConfigWifi:
        return 18;
    case Mode::ConfigTime:
        return 18;
    case Mode::ConfigSocket:
        return 20;
    case Mode::ConfigMeteo:
        return 19;
    case Mode::ConfigThermo:
        return 20;
    case Mode::ConfigTank:
        return 18;
    case Mode::ConfigSeptic:
        return 20;
    case Mode::ConfigSecurity:
        return 22;
    case Mode::ConfigRing:
        return 18;
    case Mode::ConfigAvr:
        return 17;
    case Mode::ConfigLeak:
        return 18;
    case Mode::ConfigWatering:
        return 22;
    case Mode::ConfigCloud:
        return 19;
    case Mode::ConfigCamera:
        return 20;
    case Mode::ConfigGroups:
        return 20;
    case Mode::ConfigDisplay:
        return 21;
    case Mode::ConfigUser:
        return 18;
    }
    return 5;
}

void CliConsole::clearPromptLineUnlocked_(Stream &io, size_t min_extra) const
{
    const size_t clear_len = promptWidth_() + kMaxLine + (min_extra > kMaxLine ? min_extra : 0) + 4;
    io.print('\r');
    for (size_t i = 0; i < clear_len; ++i)
        io.print(' ');
    io.print('\r');
}

void CliConsole::printPromptUnlocked_(Stream &io, bool set_interactive) const
{
    if (_state == State::NeedUser)
        io.print(F("login: "));
    else if (_state == State::NeedPass)
        io.print(F("password: "));
    else
    {
        switch (_mode)
        {
        case Mode::User:
            io.print(F("plc> "));
            break;
        case Mode::Enable:
            io.print(F("plc# "));
            break;
        case Mode::Config:
            io.print(F("plc(config)# "));
            break;
        case Mode::ConfigWifi:
            io.print(F("plc(config-wifi)# "));
            break;
        case Mode::ConfigTime:
            io.print(F("plc(config-time)# "));
            break;
        case Mode::ConfigSocket:
            io.print(F("plc(config-socket)# "));
            break;
        case Mode::ConfigMeteo:
            io.print(F("plc(config-meteo)# "));
            break;
        case Mode::ConfigThermo:
            io.print(F("plc(config-thermo)# "));
            break;
        case Mode::ConfigTank:
            io.print(F("plc(config-tank)# "));
            break;
        case Mode::ConfigSeptic:
            io.print(F("plc(config-septic)# "));
            break;
        case Mode::ConfigSecurity:
            io.print(F("plc(config-security)# "));
            break;
        case Mode::ConfigRing:
            io.print(F("plc(config-ring)# "));
            break;
        case Mode::ConfigAvr:
            io.print(F("plc(config-avr)# "));
            break;
        case Mode::ConfigLeak:
            io.print(F("plc(config-leak)# "));
            break;
        case Mode::ConfigWatering:
            io.print(F("plc(config-watering)# "));
            break;
        case Mode::ConfigCloud:
            io.print(F("plc(config-cloud)# "));
            break;
        case Mode::ConfigCamera:
            io.print(F("plc(config-camera)# "));
            break;
        case Mode::ConfigGroups:
            io.print(F("plc(config-groups)# "));
            break;
        case Mode::ConfigDisplay:
            io.print(F("plc(config-display)# "));
            break;
        case Mode::ConfigUser:
            io.print(F("plc(config-user)# "));
            break;
        }
    }
    (void)set_interactive;
}
void CliConsole::addHistory_(const String &line)
{
    if (line.length() == 0)
        return;
    if (_history_len > 0 && _history[_history_len - 1] == line)
        return;
    if (_history_len < kHistoryMax)
    {
        _history[_history_len++] = line;
    }
    else
    {
        for (size_t i = 1; i < kHistoryMax; ++i)
            _history[i - 1] = _history[i];
        _history[kHistoryMax - 1] = line;
    }
    _history_pos = -1;
    _history_saved = "";
}
void CliConsole::historyUp_()
{
    if (_history_len == 0)
        return;
    if (_history_pos < 0)
    {
        _history_saved = _line;
        _history_pos = (int)_history_len - 1;
    }
    else if (_history_pos > 0)
    {
        _history_pos--;
    }
    const size_t old_len = _line.length();
    _line = _history[_history_pos];
    redrawLine_(_line, old_len);
}
void CliConsole::historyDown_()
{
    if (_history_len == 0 || _history_pos < 0)
        return;
    if (_history_pos < (int)_history_len - 1)
    {
        _history_pos++;
        const size_t old_len = _line.length();
        _line = _history[_history_pos];
        redrawLine_(_line, old_len);
        return;
    }
    _history_pos = -1;
    const size_t old_len = _line.length();
    _line = _history_saved;
    redrawLine_(_line, old_len);
}
void CliConsole::printLine_(const __FlashStringHelper *s)
{
    if (_io)
        _io->println(s);
}
void CliConsole::printKeyValue_(const __FlashStringHelper *key, const __FlashStringHelper *value, size_t key_w)
{
    if (!_io)
        return;
    _io->print(F("    "));
    _io->print(key);
    size_t len = strlen_P(reinterpret_cast<const char *>(key));
    if (len < key_w)
    {
        for (size_t i = 0; i < (key_w - len); ++i)
            _io->print(F(" "));
    }
    _io->print(F(" : "));
    _io->println(value);
}
void CliConsole::printKeyValue_(const __FlashStringHelper *key, const String &value, size_t key_w)
{
    if (!_io)
        return;
    _io->print(F("    "));
    _io->print(key);
    size_t len = strlen_P(reinterpret_cast<const char *>(key));
    if (len < key_w)
    {
        for (size_t i = 0; i < (key_w - len); ++i)
            _io->print(F(" "));
    }
    _io->print(F(" : "));
    _io->println(value);
}
void CliConsole::printKeyValueTab_(const __FlashStringHelper *key, const __FlashStringHelper *value, size_t key_w)
{
    if (!_io)
        return;
    _io->print(F("\t"));
    _io->print(key);
    size_t len = strlen_P(reinterpret_cast<const char *>(key));
    if (len < key_w)
    {
        for (size_t i = 0; i < (key_w - len); ++i)
            _io->print(F(" "));
    }
    _io->print(F(" : "));
    _io->println(value);
}
void CliConsole::printKeyValueTab_(const __FlashStringHelper *key, const String &value, size_t key_w)
{
    if (!_io)
        return;
    _io->print(F("\t"));
    _io->print(key);
    size_t len = strlen_P(reinterpret_cast<const char *>(key));
    if (len < key_w)
    {
        for (size_t i = 0; i < (key_w - len); ++i)
            _io->print(F(" "));
    }
    _io->print(F(" : "));
    _io->println(value);
}
void CliConsole::beginCmdOutput_()
{
    if (!_io)
        return;
    _io->println();
    _cmd_blank_after = true;
}
void CliConsole::sha256_(const char *input, uint8_t out[32])
{
    if (!input)
        return;
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, (const unsigned char *)input, strlen(input));
    mbedtls_sha256_finish_ret(&ctx, out);
    mbedtls_sha256_free(&ctx);
}
bool CliConsole::isAdminUser_(const String &user) const
{
    String u = user;
    u.toLowerCase();
    return u == kAdminUser;
}
bool CliConsole::checkAdmin_(const char *pass) const
{
    if (!pass || !_admin_set)
        return false;
    uint8_t hash[32] = {};
    sha256_(pass, hash);
    return memcmp(_admin_hash, hash, sizeof(hash)) == 0;
}
int CliConsole::hexNibble_(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    return -1;
}
bool CliConsole::hexToBytes_(const String &hex, uint8_t out[32])
{
    if (hex.length() != 64)
        return false;
    for (uint8_t i = 0; i < 32; ++i)
    {
        const int hi = hexNibble_(hex.charAt(i * 2));
        const int lo = hexNibble_(hex.charAt(i * 2 + 1));
        if (hi < 0 || lo < 0)
            return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}
void CliConsole::bytesToHex_(const uint8_t in[32], char out[65])
{
    static const char kHex[] = "0123456789abcdef";
    for (uint8_t i = 0; i < 32; ++i)
    {
        out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[in[i] & 0x0F];
    }
    out[64] = '\0';
}
void CliConsole::printExtList_()
{
    const auto *devs = _ext.devs();
    bool any = false;
    if (devs)
    {
        for (uint8_t i = 0; i < _ext.devCount(); ++i)
        {
            const auto &d = devs[i];
            if (d.i2c_addr == 0 || d.type == Extender::Type::None)
                continue;
            if (!_ext.isPresent(i))
                continue;
            if (!any)
                printExtHeader_();
            any = true;
            char addr_buf[8] = {};
            snprintf(addr_buf, sizeof(addr_buf), "0x%02X", d.i2c_addr);
                printExtRow_("CPU", i, d.bus_num, addr_buf, extTypeName_(d.type), nullptr);
        }
    }
    if (!any)
    {
        _io->println(F("Extenders: none"));
        return;
    }
}
void CliConsole::printExtHeader_()
{
    _io->println(F("Extenders:"));
    _io->println(F("  Unit        ID  Bus  Addr  Type"));
    _io->println(F("  ----------  --  ---  ----  --------"));
}
void CliConsole::printExtRow_(const String &unit, uint8_t id, uint8_t bus, const char *addr,
                  const __FlashStringHelper *type, const char *type_str)
{
    _io->print(F("  "));
    printPadStr_(unit.c_str(), 10);
    _io->print(F("  "));
    printPad_(id, 2);
    _io->print(F("  "));
    printPad_(bus, 3);
    _io->print(F("  "));
    printPadStr_(addr ? addr : "--", 4);
    _io->print(F("  "));
    if (type)
        printPadStr_(type, 8);
    else if (type_str)
        printPadStr_(type_str, 8);
    else
        printPadStr_(F("--"), 8);
    _io->println();
}
void CliConsole::printI2cHeader_()
{
    _io->println(F("I2C devices:"));
    _io->println(F("    Unit        Bus  Addr"));
    _io->println(F("    ----------  ---  -----"));
}
void CliConsole::printI2cRow_(const String &unit, uint8_t bus, const char *addr)
{
    if (!addr)
        addr = "-";
    char line[48] = {};
    snprintf(line, sizeof(line), "    %-10.10s  %3u  %s", unit.c_str(), (unsigned)bus, addr);
    _io->println(line);
}
void CliConsole::printOwHeader_()
{
    _io->println(F("OneWire devices:"));
    _io->println(F("    Unit        Bus  Type     Addr"));
    _io->println(F("    ----------  ---  -------  ----------------"));
}
void CliConsole::printOwRow_(const String &unit, uint8_t bus,
                 const __FlashStringHelper *type, const char *addr,
                 const char *type_str)
{
    if (!addr)
        addr = "-";
    _io->print(F("    "));
    printPadStr_(unit.c_str(), 10);
    _io->print(F("  "));
    printPad_(bus, 3);
    _io->print(F("  "));
    if (type)
        printPadStr_(type, 7);
    else if (type_str)
        printPadStr_(type_str, 7);
    else
        printPadStr_("-", 7);
    _io->print(F("  "));
    _io->println(addr);
}
void CliConsole::printPlcHeader_()
{
    _io->println(F("PLC status:"));
    _io->println(F("  Unit        DeviceName        Fan  BoardC  RtcC    Thresh  Hyst"));
    _io->println(F("  ----------  ----------------  ---  ------  ------  ------  ------"));
}
void CliConsole::printPlcRow_(const String &unit, const String &name, bool fan,
                  float board_c, float on_c, float hyst_c,
                  const float *rtc_c)
{
    _io->print(F("  "));
    printPadStr_(unit.c_str(), 10);
    _io->print(F("  "));
    printPadStr_(name.length() ? name.c_str() : "-", 16);
    _io->print(F("  "));
    printPadStr_(fan ? F("on") : F("off"), 3);
    _io->print(F("  "));
    char buf[16] = {};
    dtostrf(board_c, 0, 2, buf);
    printPadStr_(buf, 6);
    _io->print(F("  "));
    if (rtc_c)
    {
        dtostrf(*rtc_c, 0, 2, buf);
        printPadStr_(buf, 6);
    }
    else
    {
        printPadStr_(F("--"), 6);
    }
    _io->print(F("  "));
    dtostrf(on_c, 0, 2, buf);
    printPadStr_(buf, 6);
    _io->print(F("  "));
    dtostrf(hyst_c, 0, 2, buf);
    printPadStr_(buf, 6);
    _io->println();
}
void CliConsole::printRtcHeader_()
{
    _io->println(F("RTC time:"));
    _io->println(F("  Unit        Date        Time      Weekday"));
    _io->println(F("  ----------  ----------  --------  -------"));
}
void CliConsole::printRtcRow_(const String &unit, const char *date, const char *time,
                  unsigned weekday)
{
    _io->print(F("  "));
    printPadStr_(unit.c_str(), 10);
    _io->print(F("  "));
    printPadStr_(date ? date : "--", 10);
    _io->print(F("  "));
    printPadStr_(time ? time : "--", 8);
    _io->print(F("  "));
    char wd[6] = {};
    snprintf(wd, sizeof(wd), "%u", weekday);
    printPadStr_(weekday > 0 ? wd : "--", 7);
    _io->println();
}
void CliConsole::refreshPrompt_()
{
    _cmd_blank_after = true;
    printPrompt_();
    _io->print(_line);
}
const __FlashStringHelper *CliConsole::extTypeName_(Extender::Type t)
{
    switch (t)
    {
    case Extender::Type::PCF8574:
        return F("PCF8574");
    case Extender::Type::MCP23017:
        return F("MCP23017");
    default:
        return F("None");
    }
}
const __FlashStringHelper *CliConsole::extDevTypeName_(uint8_t dev) const
{
    const auto *devs = _ext.devs();
    if (!devs || dev >= _ext.devCount())
        return F("None");
    return extTypeName_(devs[dev].type);
}
const __FlashStringHelper *CliConsole::portTypeName_(PortIO::PinType t)
{
    switch (t)
    {
    case PortIO::PinType::System:
        return F("System");
    case PortIO::PinType::Relay:
        return F("Relay");
    case PortIO::PinType::Led:
        return F("Led");
    case PortIO::PinType::Sensor:
        return F("Sensor");
    case PortIO::PinType::Button:
        return F("Button");
    case PortIO::PinType::DInput:
        return F("DInput");
    case PortIO::PinType::Buzzer:
        return F("Buzzer");
    case PortIO::PinType::Fan:
        return F("Fan");
    default:
        return F("Unknown");
    }
}
const __FlashStringHelper *CliConsole::locationName_(PortIO::Location loc)
{
    switch (loc)
    {
    case PortIO::Location::Cpu:
        return F("CPU");
    case PortIO::Location::Ext1:
        return F("EXT_1");
    case PortIO::Location::Ext2:
        return F("EXT_2");
    case PortIO::Location::Ext3:
        return F("EXT_3");
    case PortIO::Location::Ext4:
        return F("EXT_4");
    case PortIO::Location::Ext5:
        return F("EXT_5");
    case PortIO::Location::Ext6:
        return F("EXT_6");
    case PortIO::Location::Ext7:
        return F("EXT_7");
    case PortIO::Location::Ext8:
        return F("EXT_8");
    case PortIO::Location::Ext9:
        return F("EXT_9");
    case PortIO::Location::Ext10:
        return F("EXT_10");
    default:
        return F("UNKNOWN");
    }
}
const __FlashStringHelper *CliConsole::owBusName_(OneWireCfg::OwType t)
{
    switch (t)
    {
    case OneWireCfg::OwType::iButton:
        return F("iButton");
    case OneWireCfg::OwType::Temp:
        return F("Temp");
    default:
        return F("Unknown");
    }
}
void CliConsole::owAddrToHex_(const uint8_t in[8], char out[17])
{
    static const char kHex[] = "0123456789ABCDEF";
    for (uint8_t i = 0; i < 8; ++i)
    {
        out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[in[i] & 0x0F];
    }
    out[16] = '\0';
}
void CliConsole::printPortsHeader_()
{
    _io->println(F("  Unit        ID  Backend   Loc      Type     Ctrl Dev Pin  HW"));
    _io->println(F("  ----------  --  --------  -------  -------  ---- --- ---  --------"));
}
void CliConsole::printSocketsHeader_()
{
    _io->println(F("Sockets:"));
    _io->println(F("  Unit      ID  En  Name             Btn  Relay  State"));
    _io->println(F("  --------  --  --  ---------------- ---  -----  -----"));
}
void CliConsole::printSocketIdRangeInline_()
{
    _io->print(F(" (1.."));
    _io->print(SocketController::kSocketCount);
    _io->print(F(")"));
}
void CliConsole::printSocketRow_(const char *unit, uint8_t id, bool enabled,
                     const char *name, int button, int relay, bool state)
{
    if (!_io)
        return;
    char btn_buf[6] = {};
    char rel_buf[6] = {};
    const char *btn = "--";
    const char *rel = "--";
    if (button >= 0)
    {
        snprintf(btn_buf, sizeof(btn_buf), "%d", button);
        btn = btn_buf;
    }
    if (relay >= 0)
    {
        snprintf(rel_buf, sizeof(rel_buf), "%d", relay);
        rel = rel_buf;
    }
    _io->print(F("  "));
    printPadStr_(unit && unit[0] ? unit : "-", 8);
    _io->print(F("  "));
    printPad_(id, 2);
    _io->print(F("  "));
    printPadStr_(enabled ? F("on") : F("off"), 2);
    _io->print(F("  "));
    const char *name_ptr = (name && name[0]) ? name : "-";
    printPadStr_(name_ptr, 16);
    _io->print(F("  "));
    printPadStr_(btn, 3);
    _io->print(F("  "));
    printPadStr_(rel, 5);
    _io->print(F("  "));
    printPadStr_(state ? F("on") : F("off"), 5);
    _io->println();
}
void CliConsole::printPortRow_(const String &unit, uint8_t id, const PortIO::PortDesc &p)
{
    _io->print(F("  "));
    printPadStr_(unit.c_str(), 10);
    _io->print(F("  "));
    printPad_(id, 2);
    _io->print(F("  "));
    printPadStr_(p.backend == PortIO::Backend::Extender ? F("Extender") : F("Esp32"), 8);
    _io->print(F("  "));
    printPadStr_(locationName_(p.location), 7);
    _io->print(F("  "));
    printPadStr_(portTypeName_(p.type), 7);
    _io->print(F("  "));
    printPadStr_(p.allow_control ? F("yes") : F("no"), 4);
    _io->print(F(" "));
    if (p.backend == PortIO::Backend::Extender)
    {
        printPad_(p.u.ext.dev, 3);
        _io->print(F(" "));
        printPad_(p.u.ext.pin, 3);
        _io->print(F("  "));
        printPadStr_(extDevTypeName_(p.u.ext.dev), 8);
        _io->println();
    }
    else
    {
        printPadStr_(F("--"), 3);
        _io->print(F(" "));
        printPad_(p.u.esp.gpio, 3);
        _io->print(F("  "));
        printPadStr_(F("CPU"), 8);
        _io->println();
    }
}
void CliConsole::printPortStateRow_(const String &unit, uint8_t id,
                        const char *backend, const char *loc, const char *type, bool ctrl,
                        int dev, int pin, const char *hw)
{
    _io->print(F("  "));
    printPadStr_(unit.c_str(), 10);
    _io->print(F("  "));
    printPad_(id, 2);
    _io->print(F("  "));
    printPadStr_(backend ? backend : "--", 8);
    _io->print(F("  "));
    printPadStr_(loc ? loc : "--", 7);
    _io->print(F("  "));
    printPadStr_(type ? type : "--", 7);
    _io->print(F("  "));
    printPadStr_(ctrl ? F("yes") : F("no"), 4);
    _io->print(F(" "));
    printPadIntOrDash_(dev, 3);
    _io->print(F(" "));
    printPadIntOrDash_(pin, 3);
    _io->print(F("  "));
    printPadStr_(hw ? hw : "--", 8);
    _io->println();
}
void CliConsole::printPadIntOrDash_(int v, uint8_t width)
{
    if (v < 0)
    {
        printPadStr_(F("--"), width);
        return;
    }
    char buf[12] = {};
    snprintf(buf, sizeof(buf), "%d", v);
    printPadStr_(buf, width);
}
void CliConsole::printPad_(uint8_t value, uint8_t width)
{
    char buf[6] = {};
    snprintf(buf, sizeof(buf), "%u", (unsigned)value);
    printPadStr_(buf, width);
}
void CliConsole::printPadStr_(const __FlashStringHelper *s, uint8_t width)
{
    if (!_io)
        return;
    char buf[16] = {};
    strncpy_P(buf, reinterpret_cast<const char *>(s), sizeof(buf) - 1);
    printPadStr_(buf, width);
}
void CliConsole::printPadStr_(const char *s, uint8_t width)
{
    if (!_io)
        return;
    if (!s)
        s = "";
    size_t len = utf8CharCount_(s);
    if (len >= width)
    {
        _io->print(s);
        return;
    }
    _io->print(s);
    for (size_t i = 0; i < width - len; ++i)
        _io->print(' ');
}
size_t CliConsole::utf8CharCount_(const char *s)
{
    if (!s)
        return 0;
    size_t count = 0;
    for (size_t i = 0; s[i]; ++i)
    {
        const uint8_t c = static_cast<uint8_t>(s[i]);
        if ((c & 0xC0) != 0x80)
            ++count;
    }
    return count;
}
void CliConsole::setConfigsManager(ConfigsManagerIface &mgr)
{ _configs_manager = &mgr; }
