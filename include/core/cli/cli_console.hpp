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
#include "core/cli/modules/cli_tgbot.hpp"
#include "boards/board_profile.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/gpio/portio.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "utils/configs.hpp"
#include "utils/configs_manager_iface.hpp"

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
    static constexpr const char kAdminUser[] = "admin";

    CliConsole(PlcControl &plc, WifiManager &wifi, RTC &rtc, Ftest &ftest, I2CManager &i2c, OneWireManager &ow,
               TelegramClient &tgbot, TelegramMenu &tgbot_menu, Configs &configs, Extender &ext,
               StackMaster *stack_master)
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
          _stack_master(stack_master),
          _wifi_cli(*this),
          _tgbot_cli(*this),
          _enable(*this, _wifi_cli),
          _config(*this, _wifi_cli, _tgbot_cli)
    {
        if (_stack_master)
            _stack_master->setFrameHandler(&CliConsole::onStackFrame_, this);
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
        _admin_password = pass;
        _admin_set = true;
        return true;
    }

    const String &adminPassword() const { return _admin_password; }
    bool adminPasswordSet() const { return _admin_set; }

    void enterUser() { _mode = Mode::Enable; printPrompt_(); }
    void enterEnable() { _mode = Mode::Enable; printPrompt_(); }
    void enterConfig() { _mode = Mode::Config; printPrompt_(); }
    void enterConfigWifi() { _mode = Mode::ConfigWifi; printPrompt_(); }
    void enterConfigTgbot() { _mode = Mode::ConfigTgbot; printPrompt_(); }
    void enterConfigTime() { _mode = Mode::ConfigTime; printPrompt_(); }
    void logout()
    {
        _state = State::NeedUser;
        _mode = Mode::Enable;
        _user_input = "";
        printPrompt_();
    }

    void cmdShowPlc_()
    {
        const float t = _plc.boardTemp();
        const bool fan = _plc.fanStatus();
        _io->println(F("PLC status:"));
        const size_t key_w = 6; // temp_c
        printKeyValue_(F("fan"), fan ? F("on") : F("off"), key_w);
        char buf[16] = {};
        dtostrf(t, 0, 2, buf);
        printKeyValue_(F("temp_c"), buf, key_w);
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
        printPortRow_(id, p);
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
            printPortRow_(i, p);
        }
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
        _io->println(F("RTC time:"));
        const size_t key_w = 8; // weekday
        char date_buf[16] = {};
        char time_buf[16] = {};
        snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
                 (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
                 (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
        printKeyValue_(F("date"), date_buf, key_w);
        printKeyValue_(F("time"), time_buf, key_w);
        printKeyValue_(F("weekday"), String((unsigned)dt.day_of_week), key_w);
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
        _io->println(F("I2C devices:"));
        _io->println(F("    Bus  Addr"));
        _io->println(F("    --- -----"));
        bool scanned[3] = {false, false, false};
        for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
        {
            const uint8_t bus = ActiveBoardProfile::I2CS[i].bus_num;
            if (bus < 3 && scanned[bus])
                continue;
            if (bus < 3)
                scanned[bus] = true;
            std::vector<uint8_t> addrs;
            if (!_i2c.scanDevices(bus, addrs))
                continue;
            for (size_t a = 0; a < addrs.size(); ++a)
            {
                char addr_buf[8] = {};
                snprintf(addr_buf, sizeof(addr_buf), "0x%02X", addrs[a]);
                char line[20] = {};
                snprintf(line, sizeof(line), "    %3u  %s", (unsigned)bus, addr_buf);
                _io->println(line);
            }
        }
    }

    void cmdShowStack_()
    {
        if (!_configs_manager)
        {
            _io->println(F("Config manager missing"));
            return;
        }
        _io->println(F("Stack:"));
        const size_t key_w = 11; // master_host
        const auto role = _configs_manager->stackRole();
        printKeyValue_(F("role"), stackRoleName_(role), key_w);
        printKeyValue_(F("master_host"), _configs_manager->stackMasterHost(), key_w);
    }

    void cmdShowOw_()
    {
        _io->println(F("OneWire devices:"));
        _io->println(F("    Bus  Type     Addr"));
        _io->println(F("    ---  -------  ----------------"));
        bool any = false;
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
                _io->print(F("    "));
                printPad_(i, 3);
                _io->print(F("  "));
                printPadStr_(owBusName_(cfg.bus_id), 7);
                _io->print(F("  "));
                _io->println(hex);
                any = true;
            }
        }
        if (!any)
            _io->println(F("    none"));
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
        String cmd = line;
        cmd.trim();
        if (cmd == "stack nodes")
        {
            listStackNodes_();
            return;
        }
        if (!cmd.startsWith("stack send "))
        {
            _io->println(F("Usage: stack nodes"));
            _io->println(F("       stack send <id> <get|set> <json>"));
            return;
        }
        if (!_stack_master)
        {
            _io->println(F("Stack master unavailable"));
            return;
        }
        if (_configs_manager &&
            _configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _io->println(F("Stack role is slave"));
            return;
        }
        String rest = cmd.substring(strlen("stack send "));
        rest.trim();
        const int sp1 = rest.indexOf(' ');
        if (sp1 <= 0)
        {
            _io->println(F("Invalid node id"));
            return;
        }
        String id_str = rest.substring(0, sp1);
        rest = rest.substring(sp1 + 1);
        rest.trim();
        const int sp2 = rest.indexOf(' ');
        if (sp2 <= 0)
        {
            _io->println(F("Missing get/set"));
            return;
        }
        String kind = rest.substring(0, sp2);
        kind.toLowerCase();
        String json = rest.substring(sp2 + 1);
        json.trim();
        if (json.length() == 0)
        {
            _io->println(F("Missing JSON payload"));
            return;
        }
        uint32_t node_id = (uint32_t)strtoul(id_str.c_str(), nullptr, 0);
        uint8_t type = 0;
        if (kind == "get")
            type = (uint8_t)StackMsgType::CmdGet;
        else if (kind == "set")
            type = (uint8_t)StackMsgType::CmdSet;
        else
        {
            _io->println(F("Invalid command type"));
            return;
        }
        const bool ok = _stack_master->sendTo(node_id, type,
                                              (const uint8_t *)json.c_str(), json.length());
        _io->println(ok ? F("OK") : F("Send failed"));
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
        ConfigTime
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
        static const char *const kEnableCmds[] = {
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
            "ftest",
            "copy tftp://<ip>/firmware.bin firmware",
            "copy http://<ip>/firmware.bin firmware",
            "stack nodes",
            "stack send <id> <get|set> <json>",
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
            "help tgbot"};
        static const size_t kEnableCmdsCount = sizeof(kEnableCmds) / sizeof(kEnableCmds[0]);

        static const char *const kConfigCmds[] = {
            "password <pass>",
            "admin password <pass>",
            "stack role <master|slave>",
            "stack master <host>",
            "wifi",
            "tgbot",
            "time",
            "exit",
            "end",
            "help",
            "help show",
            "help wifi",
            "help user",
            "help system",
            "help tgbot"};
        static const size_t kConfigCmdsCount = sizeof(kConfigCmds) / sizeof(kConfigCmds[0]);

        static const char *const kConfigWifiCmds[] = {
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
            "help system"};
        static const size_t kConfigWifiCmdsCount = sizeof(kConfigWifiCmds) / sizeof(kConfigWifiCmds[0]);

        static const char *const kConfigTgbotCmds[] = {
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
            "help tgbot"};
        static const size_t kConfigTgbotCmdsCount = sizeof(kConfigTgbotCmds) / sizeof(kConfigTgbotCmds[0]);

        static const char *const kConfigTimeCmds[] = {
            "date <YYYY-MM-DD>",
            "time <HH:MM:SS>",
            "set <YYYY-MM-DD> <HH:MM:SS>",
            "show",
            "exit",
            "end",
            "help"};
        static const size_t kConfigTimeCmdsCount = sizeof(kConfigTimeCmds) / sizeof(kConfigTimeCmds[0]);

        const char *const *cmds = nullptr;
        size_t count = 0;
        switch (_mode)
        {
        case Mode::Enable:
            cmds = kEnableCmds;
            count = kEnableCmdsCount;
            break;
        case Mode::Config:
            cmds = kConfigCmds;
            count = kConfigCmdsCount;
            break;
        case Mode::ConfigWifi:
            cmds = kConfigWifiCmds;
            count = kConfigWifiCmdsCount;
            break;
        case Mode::ConfigTgbot:
            cmds = kConfigTgbotCmds;
            count = kConfigTgbotCmdsCount;
            break;
        case Mode::ConfigTime:
            cmds = kConfigTimeCmds;
            count = kConfigTimeCmdsCount;
            break;
        case Mode::User:
            cmds = kEnableCmds;
            count = kEnableCmdsCount;
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
    StackMaster *_stack_master = nullptr;
    ConfigsManagerIface *_configs_manager = nullptr;

    Stream *_io = nullptr;
    String _line;
    String _user_input;
    State _state = State::NeedUser;
    Mode _mode = Mode::Enable;
    uint8_t _admin_hash[32] = {};
    bool _admin_set = false;
    String _admin_password;
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
    CLIEnable _enable;
    CLIConfig _config;
    uint32_t _tgbot_last_update_id = 0;

    void printExtList_()
    {
        const auto *devs = _ext.devs();
        if (!devs)
        {
            _io->println(F("Extenders: none"));
            return;
        }
        _io->println(F("Extenders:"));
        _io->println(F("  ID  Bus  Addr  Type      Present"));
        _io->println(F("  --  ---  ----  --------  -------"));
        for (uint8_t i = 0; i < _ext.devCount(); ++i)
        {
            const auto &d = devs[i];
            if (d.i2c_addr == 0 || d.type == Extender::Type::None)
                continue;
            _io->print(F("  "));
            printPad_(i, 2);
            _io->print(F("  "));
            printPad_(d.bus_num, 3);
            _io->print(F("  "));
            char addr_buf[8] = {};
            snprintf(addr_buf, sizeof(addr_buf), "0x%02X", d.i2c_addr);
            printPadStr_(addr_buf, 4);
            _io->print(F("  "));
            printPadStr_(extTypeName_(d.type), 8);
            _io->print(F("  "));
            _io->println(_ext.isPresent(i) ? F("yes") : F("no"));
        }
    }

    void listStackNodes_()
    {
        if (!_stack_master)
        {
            _io->println(F("Stack master unavailable"));
            return;
        }
        if (_configs_manager &&
            _configs_manager->stackRole() != ConfigsManagerIface::StackRole::Master)
        {
            _io->println(F("Stack role is slave"));
            return;
        }
        const size_t count = _stack_master->nodeCount();
        if (count == 0)
        {
            _io->println(F("Stack nodes: none"));
            return;
        }
        _io->println(F("Stack nodes:"));
        _io->println(F("  ID       Name"));
        _io->println(F("  -------- ----------------"));
        for (size_t i = 0; i < count; ++i)
        {
            uint32_t id = _stack_master->nodeIdAt(i);
            String name = _stack_master->nodeNameAt(i);
            char buf[12] = {};
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)id);
            _io->print(F("  "));
            _io->print(buf);
            _io->print(F("  "));
            _io->println(name.length() ? name : String("-"));
        }
    }

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        if (!ctx)
            return;
        static_cast<CliConsole *>(ctx)->handleStackFrame_(node_id, frame);
    }

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame)
    {
        if (!_io)
            return;
        String payload = payloadToString_(frame.payload);
        _io->println();
        _io->print(F("[STACK] node="));
        _io->print(node_id);
        _io->print(F(" type="));
        _io->print(stackMsgName_(frame.type));
        _io->print(F(" payload="));
        _io->println(payload.length() ? payload : String(F("<empty>")));
        _cmd_blank_after = true;
        printPrompt_();
        _io->print(_line);
    }

    static String payloadToString_(const std::vector<uint8_t> &data)
    {
        String out;
        if (data.empty())
            return out;
        out.reserve(data.size() + 1);
        for (uint8_t b : data)
            out += (char)b;
        return out;
    }

    static const __FlashStringHelper *stackMsgName_(uint8_t type)
    {
        switch (type)
        {
        case (uint8_t)StackMsgType::Hello:
            return F("hello");
        case (uint8_t)StackMsgType::Features:
            return F("features");
        case (uint8_t)StackMsgType::Status:
            return F("status");
        case (uint8_t)StackMsgType::CmdSet:
            return F("cmd_set");
        case (uint8_t)StackMsgType::CmdGet:
            return F("cmd_get");
        case (uint8_t)StackMsgType::Ack:
            return F("ack");
        case (uint8_t)StackMsgType::Err:
            return F("err");
        default:
            return F("unknown");
        }
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

    static const __FlashStringHelper *stackRoleName_(ConfigsManagerIface::StackRole role)
    {
        return (role == ConfigsManagerIface::StackRole::Master) ? F("master") : F("slave");
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
        case PortIO::Location::Unit1:
            return F("UNIT_1");
        case PortIO::Location::Unit2:
            return F("UNIT_2");
        case PortIO::Location::Unit3:
            return F("UNIT_3");
        case PortIO::Location::Unit4:
            return F("UNIT_4");
        case PortIO::Location::Unit5:
            return F("UNIT_5");
        case PortIO::Location::Unit6:
            return F("UNIT_6");
        case PortIO::Location::Unit7:
            return F("UNIT_7");
        case PortIO::Location::Unit8:
            return F("UNIT_8");
        case PortIO::Location::Unit9:
            return F("UNIT_9");
        case PortIO::Location::Unit10:
            return F("UNIT_10");
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
        _io->println(F("  ID  Backend   Loc      Type     Ctrl Dev Pin  HW"));
        _io->println(F("  --  --------  -------  -------  ---- --- ---  --------"));
    }

    void printPortRow_(uint8_t id, const PortIO::PortDesc &p)
    {
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
        size_t len = strlen(s);
        if (len >= width)
        {
            _io->print(s);
            return;
        }
        for (size_t i = 0; i < width - len; ++i)
            _io->print(' ');
        _io->print(s);
    }

    template <typename>
    friend class CLIEnableT;
    template <typename>
    friend class CLIConfigT;
    template <typename>
    friend class CLIWifiT;
    template <typename>
    friend class CLITgbotT;

public:
    void setConfigsManager(ConfigsManagerIface &mgr) { _configs_manager = &mgr; }
};
