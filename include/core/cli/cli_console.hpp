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
#include <ArduinoJson.h>
#include <array>
#include <stdint.h>
#include <string.h>

#if defined(ESP32)
#include "mbedtls/sha256.h"
#include <HTTPClient.h>
#endif

#include "core/rtc.hpp"
#include "core/network/wifi_manager.hpp"
#include "core/network/telegram/telegram.hpp"
#include "core/network/telegram/telegram_menu.hpp"
#include "core/network/tftp_client.hpp"
#include "ftest.hpp"
#include "plc/plc_control.hpp"
#include "core/cli/cli_config.hpp"
#include "core/cli/cli_enable.hpp"
#include "core/cli/modules/cli_stack.hpp"
#include "core/cli/modules/cli_tgbot.hpp"
#include "core/cli/modules/cli_socket.hpp"
#include "core/cli/modules/cli_meteo.hpp"
#include "core/cli/modules/cli_thermo.hpp"
#include "core/cli/modules/cli_tank.hpp"
#include "core/cli/modules/cli_security.hpp"
#include "core/cli/modules/cli_septic.hpp"
#include "boards/board_profile.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/gpio/portio.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"
#include "controllers/controllers.hpp"

#if defined(ESP32)
#include <Update.h>
#endif

class CliConsole
{
public:
    using CLIEnable = CLIEnableT<CliConsole>;
    using CLIConfig = CLIConfigT<CliConsole>;
    using CLIWifi = CLIWifiT<CliConsole>;
    using CLITgbot = CLITgbotT<CliConsole>;
    using CLIStack = CLIStackT<CliConsole>;
    using CLISocket = CLISocketT<CliConsole>;
    using CLIMeteo = CLIMeteoT<CliConsole>;
    using CLIThermo = CLIThermoT<CliConsole>;
    using CLITank = CLITankT<CliConsole>;
    using CLISeptic = CLISepticT<CliConsole>;
    using CLISecurity = CLISecurityT<CliConsole>;
    static constexpr const char kAdminUser[] = "admin";

    CliConsole(PlcControl &plc, WifiManager &wifi, RTC &rtc, Ftest &ftest, I2CManager &i2c, OneWireManager &ow,
               TelegramClient &tgbot, TelegramMenu &tgbot_menu, Configs &configs, Extender &ext,
               Controllers &controllers, StackMaster *stack_master)
        : _plc(plc),
          _wifi(wifi),
          _rtc(rtc),
          _ftest(ftest),
          _i2c(i2c),
          _ow(ow),
          _tgbot(tgbot),
          _tgbot_menu(tgbot_menu),
          _configs(configs),
          _ext(ext),
          _controllers(controllers),
          _wifi_cli(*this),
          _tgbot_cli(*this),
          _stack_cli(*this),
          _socket_cli(*this, controllers.sockets()),
          _meteo_cli(*this, controllers.meteo()),
          _thermo_cli(*this, controllers.thermo(), controllers.meteo()),
          _tank_cli(*this, controllers.tanks()),
          _septic_cli(*this, controllers.septic()),
          _security_cli(*this, controllers.security()),
          _enable(*this, _wifi_cli),
          _config(*this, _wifi_cli, _tgbot_cli, _socket_cli, _meteo_cli, _thermo_cli, _tank_cli, _septic_cli,
                  _security_cli)
    {
        _stack_cli.bind(stack_master);
    }

    void begin(Stream &io)
    {
        _io = &io;
        _state = State::NeedUser;
        _mode = Mode::Enable;
        _line = "";
        _user_input = "";
        printPrompt_();
    }

    void setStackMaster(StackMaster *master) { _stack_cli.bind(master); }

    void loop()
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

    bool setAdminPassword_(const String &pass)
    {
        if (pass.length() == 0)
            return false;
        uint8_t hash[32] = {};
        sha256_(pass.c_str(), hash);
        memcpy(_admin_hash, hash, sizeof(_admin_hash));
        _admin_set = true;
        return true;
    }

    bool setAdminPasswordHashHex_(const String &hex)
    {
        uint8_t hash[32] = {};
        if (!hexToBytes_(hex, hash))
            return false;
        memcpy(_admin_hash, hash, sizeof(_admin_hash));
        _admin_set = true;
        return true;
    }

    String adminPasswordHashHex() const
    {
        if (!_admin_set)
            return String();
        char out[65] = {};
        bytesToHex_(_admin_hash, out);
        return String(out);
    }

    bool checkAdminPassword(const String &pass) const
    {
        return checkAdmin_(pass.c_str());
    }

    bool adminPasswordSet() const { return _admin_set; }
    const String &currentUser() const { return _user_input; }

    void enterUser() { _mode = Mode::Enable; printPrompt_(); }
    void enterEnable() { _mode = Mode::Enable; printPrompt_(); }
    void enterConfig() { _mode = Mode::Config; printPrompt_(); }
    void enterConfigWifi() { _mode = Mode::ConfigWifi; printPrompt_(); }
    void enterConfigTgbot() { _mode = Mode::ConfigTgbot; printPrompt_(); }
    void enterConfigTime() { _mode = Mode::ConfigTime; printPrompt_(); }
    void enterConfigSocket() { _mode = Mode::ConfigSocket; printPrompt_(); }
    void enterConfigMeteo() { _mode = Mode::ConfigMeteo; printPrompt_(); }
    void enterConfigThermo() { _mode = Mode::ConfigThermo; printPrompt_(); }
    void enterConfigTank() { _mode = Mode::ConfigTank; printPrompt_(); }
    void enterConfigSeptic() { _mode = Mode::ConfigSeptic; printPrompt_(); }
    void enterConfigSecurity() { _mode = Mode::ConfigSecurity; printPrompt_(); }
    void logout()
    {
        _state = State::NeedUser;
        _mode = Mode::Enable;
        _user_input = "";
        printPrompt_();
    }

    void cmdShowPlc_()
    {
        printPlcHeader_();
        const float board_t = _plc.boardTemp();
        const float cpu_t = _plc.cpuTemp();
        const bool fan = _plc.fanStatus();
        const float on_c = _plc.fanOnC();
        const float hyst_c = _plc.fanHysteresisC();
        float rtc_t = 0.0f;
        const bool rtc_ok = _rtc.readTemp(rtc_t);
        printPlcRow_("CPU", String(ActiveBoardProfile::UI_NAME), fan, board_t, cpu_t,
                     on_c, hyst_c, rtc_ok ? &rtc_t : nullptr);
        _stack_cli.requestStackPlc_();
    }

    void cmdShowBoard_()
    {
        _io->println(F("Board:"));
        const size_t key_w = 4; // name
        printKeyValue_(F("name"), String(ActiveBoardProfile::UI_NAME), key_w);
    }

    void cmdShowPort_(uint8_t id)
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

    void cmdShowPorts_()
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
        _stack_cli.requestStackPorts_();
    }

    void cmdShowWifi_()
    {
        _io->println(F("Wi-Fi configurations:"));
        const size_t key_w = 11; // ap_password
        printKeyValue_(F("mode"), _wifi.ap() ? F("AP") : F("STA"), key_w);
        printKeyValue_(F("ssid"), _wifi.ssid(), key_w);
        printKeyValue_(F("password"), _wifi.password(), key_w);
        printKeyValue_(F("ap_ssid"), _wifi.apSsid(), key_w);
        printKeyValue_(F("ap_password"), _wifi.apPassword(), key_w);
    }

    void cmdShowTime_()
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

        _stack_cli.requestStackRtc_();
    }

    void cmdShowTelegram_()
    {
        _io->println(F("Telegram:"));
        const size_t key_w = 10; // proxy_host
        printKeyValue_(F("token"), _tgbot.token(), key_w);
        printKeyValue_(F("chat_id"), String((long long)_tgbot.chatId()), key_w);
        printKeyValue_(F("insecure"), _tgbot.insecure() ? F("true") : F("false"), key_w);
        printKeyValue_(F("client"), _tgbot.clientKindName(), key_w);
        printKeyValue_(F("proxy"), _tgbot.useProxy() ? F("true") : F("false"), key_w);
        printKeyValue_(F("proxy_host"), _tgbot.proxyHost(), key_w);
        printKeyValue_(F("proxy_port"), String((unsigned)_tgbot.proxyPort()), key_w);
        printKeyValue_(F("proxy_path"), _tgbot.proxyPath(), key_w);
    }

    void cmdCopy_(const String &line)
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

    void cmdShowI2c_()
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
        _stack_cli.requestStackI2cScan_();
    }

    void cmdShowStack_()
    {
        _stack_cli.cmdShowStack_();
    }

    void cmdShowOw_()
    {
        printOwHeader_();
        for (uint8_t i = 0; i < ActiveBoardProfile::ONEWIRE_COUNT; ++i)
        {
            OneWireBus *bus = _ow.busPtrByIndex(i);
            if (!bus)
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
        _stack_cli.requestStackOwScan_();
    }

    void cmdShowConfig_()
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

    void cmdFtest_()
    {
        _ftest.start();
        _io->println(F("ftest started"));
    }

    void cmdExtScan_()
    {
        _ext.rescan();
        printExtList_();
    }

    void cmdExtList_()
    {
        printExtList_();
    }

    void cmdWifiRestart_()
    {
        if (_wifi.restart())
            _io->println(F("Wi-Fi restarted"));
        else
            _io->println(F("Wi-Fi restart failed"));
    }

    void cmdStack_(const String &line)
    {
        _stack_cli.cmdStack_(line);
    }

    void cmdRestart_()
    {
#if defined(ESP32)
        _io->println(F("Restarting..."));
        _io->flush();
        ESP.restart();
#else
        _io->println(F("Restart not supported"));
#endif
    }

    void cmdWriteConfig_()
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

    bool setStackRole_(ConfigsManagerIface::StackRole role)
    {
        if (!_configs_manager)
            return false;
        _configs_manager->setStackRole(role);
        return true;
    }

    bool setStackMasterHost_(const String &host)
    {
        if (!_configs_manager)
            return false;
        _configs_manager->setStackMasterHost(host);
        return true;
    }

    bool setStackApiKey_(const String &key)
    {
        if (!_configs_manager)
            return false;
        _configs_manager->setStackApiKey(key);
        return true;
    }

    void cmdEraseConfig_()
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

private:
    enum class Mode : uint8_t
    {
        User,
        Enable,
        Config,
        ConfigWifi,
        ConfigTgbot,
        ConfigTime,
        ConfigSocket,
        ConfigMeteo,
        ConfigThermo,
        ConfigTank,
        ConfigSeptic,
        ConfigSecurity
    };

    enum class State : uint8_t
    {
        NeedUser,
        NeedPass,
        LoggedIn
    };

    static constexpr size_t kMaxLine = 96;
    void showHelpTopic_(const String &topic)
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
            _io->println(F("  show stack      - stack role settings"));
            _io->println(F("  show telegram   - Telegram settings"));
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
            _io->println(F("  show septic     - list septic"));
            _io->print(F("  show septic <id>"));
            _septic_cli.printIdRangeInline();
            _io->println(F(" - septic details"));
            _io->println(F("  show security   - list security sensors"));
            _io->print(F("  show security <id>"));
            _security_cli.printIdRangeInline();
            _io->println(F(" - sensor details"));
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
            return;
        }
        if (t == "tgbot")
        {
            _tgbot_cli.printHelpTopic();
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
        if (t == "system")
        {
            _io->println(F("System commands:"));
            _io->println(F("  ftest   - start functional test task"));
            _io->println(F("  reload  - restart controller"));
            _io->println(F("  reset   - restart controller"));
            _io->println(F("  write   - save configuration"));
            _io->println(F("  erase   - delete configuration"));
            return;
        }
        _io->println(F("Unknown topic"));
    }

    void handleTab_()
    {
        if (!_io || _state != State::LoggedIn)
            return;
        static const std::array<const char *, 62> kEnableCmds = {{
            "show plc",
            "show board",
            "show wifi",
            "show time",
            "show i2c",
            "show ow",
            "show stack",
            "show telegram",
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
            "show septic",
            "show septic <id>",
            "show security",
            "show security <id>",
            "socket toggle <id>",
            "socket on <id>",
            "socket off <id>",
            "security status",
            "security arm",
            "security disarm",
            "ftest",
            "copy tftp://<ip>/firmware.bin firmware",
            "copy http://<ip>/firmware.bin firmware",
            "stack nodes",
            "stack send <id> <get|set> <json>",
            "stack socket <unit> <on|off|toggle> <id>",
            "stack thermo <unit> <on|off|toggle> <id>",
            "stack septic <unit> <status|get>",
            "stack security <unit> <arm|disarm|status|clear>",
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
            "help tgbot",
            "help socket",
            "help meteo",
            "help thermo",
            "help tank",
            "help septic",
            "help security"}};

        static const std::array<const char *, 30> kConfigCmds = {{
            "password <pass>",
            "admin password <pass>",
            "stack role <master|slave>",
            "stack master <host>",
            "stack api_key <value>",
            "stack api_key clear",
            "stack api_key gen",
            "wifi",
            "tgbot",
            "time",
            "socket",
            "meteo",
            "thermo",
            "tank",
            "septic",
            "security",
            "exit",
            "end",
            "help",
            "help show",
            "help wifi",
            "help user",
            "help system",
            "help tgbot",
            "help socket",
            "help meteo",
            "help thermo",
            "help tank",
            "help septic",
            "help security"}};

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

        static const std::array<const char *, 20> kConfigTgbotCmds = {{
            "token <value>",
            "chat <id>",
            "insecure on",
            "insecure off",
            "allow list",
            "allow add <username>",
            "allow del <username>",
            "allow clear",
            "send <text>",
            "poll",
            "show",
            "exit",
            "end",
            "help",
            "help show",
            "help wifi",
            "help user",
            "help system",
            "help tgbot",
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
        case Mode::ConfigTgbot:
            cmds = kConfigTgbotCmds.data();
            count = kConfigTgbotCmds.size();
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


    void handleShow_(String what)
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
        else if (eq_(what, "telegram"))
            cmdShowTelegram_();
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
        else
            _io->println(F("Unknown show"));
        printPrompt_();
    }

    static bool eq_(const String &a, const char *b)
    {
        String t = a;
        t.toLowerCase();
        return t == b;
    }

    static bool startsWith_(const String &a, const char *b)
    {
        String t = a;
        t.toLowerCase();
        return t.startsWith(b);
    }

    static bool parseUint_(const String &s, uint16_t &out)
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

    void handleLine_(String line)
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
        case Mode::ConfigTgbot:
            _config.handleTgbotContext(line);
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
        case Mode::User:
            _enable.handle(line);
            break;
        }
    }

    void handleLogin_(const String &line)
    {
        if (_state == State::NeedUser)
        {
            if (line.length() == 0)
            {
                printPrompt_();
                return;
            }
            _user_input = line;
            _state = State::NeedPass;
            printPrompt_();
            return;
        }

        if (_state == State::NeedPass)
        {
            if (!isAdminUser_(_user_input))
            {
                printLine_(F("Login invalid"));
                _state = State::NeedUser;
                _user_input = "";
                printPrompt_();
                return;
            }

            if (!_admin_set)
            {
                _state = State::LoggedIn;
                _mode = Mode::Enable;
                printPrompt_();
                return;
            }

            if (checkAdmin_(line.c_str()))
            {
                _state = State::LoggedIn;
                _mode = Mode::Enable;
                printPrompt_();
            }
            else
            {
                printLine_(F("Login invalid"));
                _state = State::NeedUser;
                _user_input = "";
                printPrompt_();
            }
        }
    }

    void printPrompt_()
    {
        if (!_io)
            return;
        if (_cmd_blank_after)
        {
            _io->println();
            _cmd_blank_after = false;
        }
        if (_state == State::NeedUser)
        {
            _io->print(F("login: "));
            return;
        }
        if (_state == State::NeedPass)
        {
            _io->print(F("password: "));
            return;
        }

        switch (_mode)
        {
        case Mode::User:
            _io->print(F("plc> "));
            break;
        case Mode::Enable:
            _io->print(F("plc# "));
            break;
        case Mode::Config:
            _io->print(F("plc(config)# "));
            break;
        case Mode::ConfigWifi:
            _io->print(F("plc(config-wifi)# "));
            break;
        case Mode::ConfigTgbot:
            _io->print(F("plc(config-tgbot)# "));
            break;
        case Mode::ConfigTime:
            _io->print(F("plc(config-time)# "));
            break;
        case Mode::ConfigSocket:
            _io->print(F("plc(config-socket)# "));
            break;
        case Mode::ConfigMeteo:
            _io->print(F("plc(config-meteo)# "));
            break;
        case Mode::ConfigThermo:
            _io->print(F("plc(config-thermo)# "));
            break;
        case Mode::ConfigTank:
            _io->print(F("plc(config-tank)# "));
            break;
        case Mode::ConfigSeptic:
            _io->print(F("plc(config-septic)# "));
            break;
        case Mode::ConfigSecurity:
            _io->print(F("plc(config-security)# "));
            break;
        }
    }

    bool handleEscape_(char c)
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

    void redrawLine_(const String &new_line, size_t old_len)
    {
        if (!_io)
            return;
        _io->print('\r');
        printPrompt_();
        _io->print(new_line);
        if (old_len > new_line.length())
        {
            const size_t extra = old_len - new_line.length();
            for (size_t i = 0; i < extra; ++i)
                _io->print(' ');
            _io->print('\r');
            printPrompt_();
            _io->print(new_line);
        }
    }

    void addHistory_(const String &line)
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

    void historyUp_()
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

    void historyDown_()
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

    void printLine_(const __FlashStringHelper *s)
    {
        if (_io)
            _io->println(s);
    }

    void printKeyValue_(const __FlashStringHelper *key, const __FlashStringHelper *value, size_t key_w)
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

    void printKeyValue_(const __FlashStringHelper *key, const String &value, size_t key_w)
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

    void printKeyValueTab_(const __FlashStringHelper *key, const __FlashStringHelper *value, size_t key_w)
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

    void printKeyValueTab_(const __FlashStringHelper *key, const String &value, size_t key_w)
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

    void beginCmdOutput_()
    {
        if (!_io)
            return;
        _io->println();
        _cmd_blank_after = true;
    }

    static void sha256_(const char *input, uint8_t out[32])
    {
        if (!input)
            return;
#if defined(ESP32)
        mbedtls_sha256_context ctx;
        mbedtls_sha256_init(&ctx);
        mbedtls_sha256_starts_ret(&ctx, 0);
        mbedtls_sha256_update_ret(&ctx, (const unsigned char *)input, strlen(input));
        mbedtls_sha256_finish_ret(&ctx, out);
        mbedtls_sha256_free(&ctx);
#else
        (void)input;
        for (uint8_t i = 0; i < 32; ++i)
            out[i] = 0;
#endif
    }

    bool isAdminUser_(const String &user) const
    {
        String u = user;
        u.toLowerCase();
        return u == kAdminUser;
    }

    bool checkAdmin_(const char *pass) const
    {
        if (!pass || !_admin_set)
            return false;
        uint8_t hash[32] = {};
        sha256_(pass, hash);
        return memcmp(_admin_hash, hash, sizeof(hash)) == 0;
    }

    static int hexNibble_(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        return -1;
    }

    static bool hexToBytes_(const String &hex, uint8_t out[32])
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

    static void bytesToHex_(const uint8_t in[32], char out[65])
    {
        static const char kHex[] = "0123456789abcdef";
        for (uint8_t i = 0; i < 32; ++i)
        {
            out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
            out[i * 2 + 1] = kHex[in[i] & 0x0F];
        }
        out[64] = '\0';
    }

    PlcControl &_plc;
    WifiManager &_wifi;
    RTC &_rtc;
    Ftest &_ftest;
    I2CManager &_i2c;
    OneWireManager &_ow;
    TelegramClient &_tgbot;
    TelegramMenu &_tgbot_menu;
    Configs &_configs;
    Extender &_ext;
    Controllers &_controllers;
    ConfigsManagerIface *_configs_manager = nullptr;

    Stream *_io = nullptr;
    String _line;
    String _user_input;
    State _state = State::NeedUser;
    Mode _mode = Mode::Enable;
    uint8_t _admin_hash[32] = {};
    bool _admin_set = false;
    bool _saw_cr = false;
    uint8_t _esc_state = 0;
    bool _cmd_blank_after = false;

    static constexpr size_t kHistoryMax = 12;
    String _history[kHistoryMax];
    size_t _history_len = 0;
    int _history_pos = -1;
    String _history_saved;

    CLIWifi _wifi_cli;
    CLITgbot _tgbot_cli;
    CLIStack _stack_cli;
    CLISocket _socket_cli;
    CLIMeteo _meteo_cli;
    CLIThermo _thermo_cli;
    CLITank _tank_cli;
    CLISeptic _septic_cli;
    CLISecurity _security_cli;
    CLIEnable _enable;
    CLIConfig _config;
    uint32_t _tgbot_last_update_id = 0;

    void printExtList_()
    {
        const auto *devs = _ext.devs();
        const bool has_stack = _stack_cli.canRequestStackExt_();
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
        if (has_stack && !any)
            printExtHeader_();
        if (!any && !has_stack)
        {
            _io->println(F("Extenders: none"));
            return;
        }
        if (has_stack)
            _stack_cli.requestStackExtList_();
    }

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        if (!ctx)
            return;
        static_cast<CliConsole *>(ctx)->_stack_cli.handleStackFrame_(node_id, frame);
    }

    static String payloadToString_(const uint8_t *data, size_t len)
    {
        String out;
        if (!data || len == 0)
            return out;
        out.reserve(len + 1);
        for (size_t i = 0; i < len; ++i)
            out += (char)data[i];
        return out;
    }

    void printExtHeader_()
    {
        _io->println(F("Extenders:"));
        _io->println(F("  Unit        ID  Bus  Addr  Type"));
        _io->println(F("  ----------  --  ---  ----  --------"));
    }

    void printExtRow_(const String &unit, uint8_t id, uint8_t bus, const char *addr,
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

    void printI2cHeader_()
    {
        _io->println(F("I2C devices:"));
        _io->println(F("    Unit        Bus  Addr"));
        _io->println(F("    ----------  ---  -----"));
    }

    void printI2cRow_(const String &unit, uint8_t bus, const char *addr)
    {
        if (!addr)
            addr = "-";
        char line[48] = {};
        snprintf(line, sizeof(line), "    %-10.10s  %3u  %s", unit.c_str(), (unsigned)bus, addr);
        _io->println(line);
    }

    void printOwHeader_()
    {
        _io->println(F("OneWire devices:"));
        _io->println(F("    Unit        Bus  Type     Addr"));
        _io->println(F("    ----------  ---  -------  ----------------"));
    }

    void printOwRow_(const String &unit, uint8_t bus,
                     const __FlashStringHelper *type, const char *addr,
                     const char *type_str = nullptr)
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

    void printPlcHeader_()
    {
        _io->println(F("PLC status:"));
        _io->println(F("  Unit        DeviceName        Fan  BoardC  CpuC    RtcC    Thresh  Hyst"));
        _io->println(F("  ----------  ----------------  ---  ------  ------  ------  ------  ------"));
    }

    void printPlcRow_(const String &unit, const String &name, bool fan,
                      float board_c, float cpu_c, float on_c, float hyst_c,
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
        dtostrf(cpu_c, 0, 2, buf);
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

    void printRtcHeader_()
    {
        _io->println(F("RTC time:"));
        _io->println(F("  Unit        Date        Time      Weekday"));
        _io->println(F("  ----------  ----------  --------  -------"));
    }

    void printRtcRow_(const String &unit, const char *date, const char *time,
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

    void refreshPrompt_()
    {
        _cmd_blank_after = true;
        printPrompt_();
        _io->print(_line);
    }

    static const __FlashStringHelper *extTypeName_(Extender::Type t)
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

    const __FlashStringHelper *extDevTypeName_(uint8_t dev) const
    {
        const auto *devs = _ext.devs();
        if (!devs || dev >= _ext.devCount())
            return F("None");
        return extTypeName_(devs[dev].type);
    }

    static const __FlashStringHelper *portTypeName_(PortIO::PinType t)
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

    static const __FlashStringHelper *locationName_(PortIO::Location loc)
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

    static const __FlashStringHelper *owBusName_(OneWireCfg::OwType t)
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

    static void owAddrToHex_(const uint8_t in[8], char out[17])
    {
        static const char kHex[] = "0123456789ABCDEF";
        for (uint8_t i = 0; i < 8; ++i)
        {
            out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
            out[i * 2 + 1] = kHex[in[i] & 0x0F];
        }
        out[16] = '\0';
    }

    void printPortsHeader_()
    {
        _io->println(F("  Unit        ID  Backend   Loc      Type     Ctrl Dev Pin  HW"));
        _io->println(F("  ----------  --  --------  -------  -------  ---- --- ---  --------"));
    }

    void printSocketsHeader_()
    {
        _io->println(F("Sockets:"));
        _io->println(F("  Unit      ID  En  Name             Btn  Relay  State"));
        _io->println(F("  --------  --  --  ---------------- ---  -----  -----"));
    }

    void printSocketIdRangeInline_()
    {
        _io->print(F(" (1.."));
        _io->print(SocketController::kSocketCount);
        _io->print(F(")"));
    }

    void printSocketRow_(const char *unit, uint8_t id, bool enabled,
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

    void printPortRow_(const String &unit, uint8_t id, const PortIO::PortDesc &p)
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

    void printPortStateRow_(const String &unit, uint8_t id,
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

    void printPadIntOrDash_(int v, uint8_t width)
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

    void printPad_(uint8_t value, uint8_t width)
    {
        char buf[6] = {};
        snprintf(buf, sizeof(buf), "%u", (unsigned)value);
        printPadStr_(buf, width);
    }

    void printPadStr_(const __FlashStringHelper *s, uint8_t width)
    {
        if (!_io)
            return;
        char buf[16] = {};
        strncpy_P(buf, reinterpret_cast<const char *>(s), sizeof(buf) - 1);
        printPadStr_(buf, width);
    }

    void printPadStr_(const char *s, uint8_t width)
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

    static size_t utf8CharCount_(const char *s)
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

    template <typename>
    friend class CLIEnableT;
    template <typename>
    friend class CLIConfigT;
    template <typename>
    friend class CLIWifiT;
    template <typename>
    friend class CLITgbotT;
    template <typename>
    friend class CLIStackT;
    template <typename>
    friend class CLISocketT;
    template <typename>
    friend class CLIMeteoT;
    template <typename>
    friend class CLIThermoT;
    template <typename>
    friend class CLITankT;
    template <typename>
    friend class CLISepticT;
    template <typename>
    friend class CLISecurityT;

public:
    void setConfigsManager(ConfigsManagerIface &mgr) { _configs_manager = &mgr; }
};


