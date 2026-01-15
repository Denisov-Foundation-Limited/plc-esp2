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
#include <LittleFS.h>
#include <stdint.h>
#include <vector>

#include "boards/board_profile.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_node.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "core/network/stack/stack_types.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/ds18b20.hpp"
#include "hal/gpio/portio.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "core/network/telegram/telegram.hpp"
#include "utils/logger.hpp"

class StackSlaveHandler
{
public:
    StackSlaveHandler(PortIO &portio, Ds18b20 &ds18b20, OneWireManager &ow, I2CManager &i2c,
                      PlcControl &plc, RTC &rtc, TelegramClient &telegram, Logger &logs)
        : _portio(portio),
          _ds18b20(ds18b20),
          _ow(ow),
          _i2c(i2c),
          _plc(plc),
          _rtc(rtc),
          _telegram(telegram),
          _logs(logs)
    {
    }

    void attach(StackNode &node)
    {
        _node = &node;
        node.setFrameHandler(&StackSlaveHandler::onFrame_, this);
        node.setStatusProvider(&StackSlaveHandler::onStatus_, this);
    }

private:
    struct I2cEntry
    {
        uint8_t bus = 0;
        uint8_t addr = 0;
    };
    struct OwEntry
    {
        uint8_t bus = 0;
        char addr[17] = {};
    };

    PortIO &_portio;
    Ds18b20 &_ds18b20;
    OneWireManager &_ow;
    I2CManager &_i2c;
    PlcControl &_plc;
    RTC &_rtc;
    TelegramClient &_telegram;
    Logger &_logs;
    StackNode *_node = nullptr;
    std::vector<I2cEntry> _last_i2c;
    std::vector<OwEntry> _last_ow;

    static void onFrame_(void *ctx, const StackFrame &frame)
    {
        if (!ctx)
            return;
        static_cast<StackSlaveHandler *>(ctx)->handleFrame_(frame);
    }

    static bool onStatus_(void *ctx, std::vector<uint8_t> &out)
    {
        if (!ctx)
            return false;
        return static_cast<StackSlaveHandler *>(ctx)->buildStatus_(out);
    }

    void handleFrame_(const StackFrame &frame)
    {
        if (!_node)
            return;
        if (frame.type != (uint8_t)StackMsgType::CmdGet && frame.type != (uint8_t)StackMsgType::CmdSet)
            return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, frame.payload.data(), frame.payload.size());
        if (err)
        {
            sendErr_(0, "json parse");
            return;
        }

        const uint16_t cmd_id = doc["cmd_id"] | 0;
        const uint8_t feature = (uint8_t)(doc["feature"] | 0);
        String action = doc["action"] | "";
        action.toLowerCase();
        JsonVariantConst params = doc["params"];

        switch ((StackFeature)feature)
        {
        case StackFeature::System:
            handleSystem_(cmd_id, action, params);
            break;
        case StackFeature::Ports:
            handlePorts_(cmd_id, action, params);
            break;
        case StackFeature::TempSensors:
            handleTempSensors_(cmd_id, action, params);
            break;
        case StackFeature::I2cScan:
            handleI2cScan_(cmd_id, action);
            break;
        case StackFeature::OwScan:
            handleOwScan_(cmd_id, action);
            break;
        case StackFeature::Fan:
            handleFan_(cmd_id, action, params);
            break;
        case StackFeature::Rtc:
            handleRtc_(cmd_id, action, params);
            break;
        case StackFeature::PlcStatus:
            handlePlcStatus_(cmd_id, action);
            break;
        case StackFeature::Relays:
            handleRelays_(cmd_id, action, params);
            break;
        case StackFeature::DigitalInputs:
            handleDigitalInputs_(cmd_id, action);
            break;
        case StackFeature::Telegram:
            handleTelegram_(cmd_id, action);
            break;
        case StackFeature::Storage:
            handleStorage_(cmd_id, action);
            break;
        default:
            sendErr_(cmd_id, "unknown feature");
            break;
        }
    }

    void handleSystem_(uint16_t cmd_id, const String &action, JsonVariantConst)
    {
        if (action == "get_info")
        {
            JsonDocument doc;
            doc["uptime_ms"] = (uint32_t)millis();
            doc["board"] = ActiveBoardProfile::UI_NAME;
            doc["fw_version"] = "";
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "reboot")
        {
            sendAck_(cmd_id);
            delay(100);
            ESP.restart();
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handlePorts_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get_state")
        {
            JsonDocument doc;
            JsonArray arr = doc["ports"].to<JsonArray>();
            if (params.is<JsonObjectConst>() && params["ids"].is<JsonArrayConst>())
            {
                JsonArrayConst ids = params["ids"].as<JsonArrayConst>();
                for (JsonVariantConst v : ids)
                {
                    if (!v.is<unsigned>())
                        continue;
                    const uint8_t id = (uint8_t)v.as<unsigned>();
                    const bool state = _portio.read(id);
                    JsonObject o = arr.add<JsonObject>();
                    o["id"] = id;
                    o["state"] = state;
                }
            }
            else
            {
                for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
                {
                    const auto &p = ActiveBoardProfile::PORTS[i];
                    if (p.caps == Cap::None)
                        continue;
                    JsonObject o = arr.add<JsonObject>();
                    o["id"] = i;
                    o["state"] = _portio.read(i);
                }
            }
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "set_state")
        {
            if (!params.is<JsonObjectConst>() || !params["items"].is<JsonArrayConst>())
            {
                sendErr_(cmd_id, "missing items");
                return;
            }
            JsonArrayConst items = params["items"].as<JsonArrayConst>();
            for (JsonVariantConst v : items)
            {
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                const bool state = item["state"].is<bool>() ? item["state"].as<bool>() : (item["state"].as<int>() != 0);
                const auto &p = ActiveBoardProfile::PORTS[id];
                if (!p.allow_control)
                    continue;
                _portio.write(id, state);
            }
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleTempSensors_(uint16_t cmd_id, const String &action, JsonVariantConst)
    {
        std::vector<String> serials;
        _ds18b20.listSerials(serials);
        if (action == "list")
        {
            JsonDocument doc;
            JsonArray arr = doc["serials"].to<JsonArray>();
            for (const auto &s : serials)
                arr.add(s);
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "read_all")
        {
            JsonDocument doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (const auto &s : serials)
            {
                float t = 0.0f;
                const bool ok = _ds18b20.readTempC(s, t);
                JsonObject o = arr.add<JsonObject>();
                o["serial"] = s;
                if (ok)
                    o["temp_c"] = t;
                else
                    o["temp_c"] = nullptr;
            }
            sendAck_(cmd_id, doc);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleI2cScan_(uint16_t cmd_id, const String &action)
    {
        if (action == "run")
            scanI2c_();
        if (action == "run" || action == "get_last")
        {
            JsonDocument doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (const auto &e : _last_i2c)
            {
                JsonObject o = arr.add<JsonObject>();
                o["bus"] = e.bus;
                char addr_buf[8] = {};
                snprintf(addr_buf, sizeof(addr_buf), "0x%02X", e.addr);
                o["addr"] = addr_buf;
            }
            sendAck_(cmd_id, doc);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleOwScan_(uint16_t cmd_id, const String &action)
    {
        if (action == "run")
            scanOw_();
        if (action == "run" || action == "get_last")
        {
            JsonDocument doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (const auto &e : _last_ow)
            {
                JsonObject o = arr.add<JsonObject>();
                o["bus"] = e.bus;
                o["addr"] = e.addr;
            }
            sendAck_(cmd_id, doc);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleFan_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get_status")
        {
            JsonDocument doc;
            doc["mode"] = _plc.fanManualMode() ? "manual" : "auto";
            doc["fan_on"] = _plc.fanStatus();
            doc["board_temp"] = _plc.boardTemp();
            doc["on_c"] = _plc.fanOnC();
            doc["hyst_c"] = _plc.fanHysteresisC();
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "set_mode")
        {
            if (!params.is<JsonObjectConst>() || !params["mode"].is<const char *>())
            {
                sendErr_(cmd_id, "missing mode");
                return;
            }
            String mode = params["mode"].as<const char *>();
            mode.toLowerCase();
            if (mode == "auto")
            {
                _plc.setFanAuto();
                sendAck_(cmd_id);
                return;
            }
            if (mode == "manual")
            {
                const bool state = params["state"].is<bool>() ? params["state"].as<bool>() : (params["state"].as<int>() != 0);
                _plc.setFanManual(state);
                sendAck_(cmd_id);
                return;
            }
            sendErr_(cmd_id, "invalid mode");
            return;
        }
        if (action == "set_thresh")
        {
            if (!params.is<JsonObjectConst>())
            {
                sendErr_(cmd_id, "missing params");
                return;
            }
            const float on_c = params["on_c"] | _plc.fanOnC();
            const float hyst = params["hyst_c"] | _plc.fanHysteresisC();
            _plc.setFanThresholds(on_c, hyst);
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleRtc_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get_time")
        {
            Ds3231Mz::DateTime dt{};
            if (!_rtc.Time(dt))
            {
                sendErr_(cmd_id, "rtc error");
                return;
            }
            char date_buf[16] = {};
            char time_buf[16] = {};
            snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u",
                     (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day);
            snprintf(time_buf, sizeof(time_buf), "%02u:%02u:%02u",
                     (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
            float t = 0.0f;
            _rtc.readTemp(t);
            JsonDocument doc;
            doc["date"] = date_buf;
            doc["time"] = time_buf;
            doc["weekday"] = (unsigned)dt.day_of_week;
            doc["temp_c"] = t;
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "set_time")
        {
            if (!params.is<JsonObjectConst>())
            {
                sendErr_(cmd_id, "missing params");
                return;
            }
            String date = params["date"] | "";
            String time = params["time"] | "";
            if (!setRtc_(date, time))
            {
                sendErr_(cmd_id, "invalid datetime");
                return;
            }
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handlePlcStatus_(uint16_t cmd_id, const String &action)
    {
        if (action != "get")
        {
            sendErr_(cmd_id, "unsupported");
            return;
        }
        JsonDocument doc;
        doc["board_temp"] = _plc.boardTemp();
        doc["cpu_temp"] = _plc.cpuTemp();
        doc["fan_on"] = _plc.fanStatus();
        sendAck_(cmd_id, doc);
    }

    void handleRelays_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            JsonDocument doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != PortIO::PinType::Relay)
                    continue;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = i;
                o["state"] = _portio.read(i);
            }
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "set")
        {
            if (!params.is<JsonObjectConst>() || !params["items"].is<JsonArrayConst>())
            {
                sendErr_(cmd_id, "missing items");
                return;
            }
            JsonArrayConst items = params["items"].as<JsonArrayConst>();
            for (JsonVariantConst v : items)
            {
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                const bool state = item["state"].is<bool>() ? item["state"].as<bool>() : (item["state"].as<int>() != 0);
                const auto &p = ActiveBoardProfile::PORTS[id];
                if (p.type != PortIO::PinType::Relay || !p.allow_control)
                    continue;
                _portio.write(id, state);
            }
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleDigitalInputs_(uint16_t cmd_id, const String &action)
    {
        if (action != "get")
        {
            sendErr_(cmd_id, "unsupported");
            return;
        }
        JsonDocument doc;
        JsonArray arr = doc["items"].to<JsonArray>();
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = ActiveBoardProfile::PORTS[i];
            if (p.caps == Cap::None)
                continue;
            if (p.type != PortIO::PinType::DInput && p.type != PortIO::PinType::Button)
                continue;
            JsonObject o = arr.add<JsonObject>();
            o["id"] = i;
            o["state"] = _portio.read(i);
        }
        sendAck_(cmd_id, doc);
    }

    void handleTelegram_(uint16_t cmd_id, const String &action)
    {
        if (action != "get")
        {
            sendErr_(cmd_id, "unsupported");
            return;
        }
        JsonDocument doc;
        doc["token_set"] = _telegram.token().length() > 0;
        doc["chat_id"] = (long long)_telegram.chatId();
        doc["insecure"] = _telegram.insecure();
        doc["use_proxy"] = _telegram.useProxy();
        doc["proxy_host"] = _telegram.proxyHost();
        doc["proxy_port"] = (unsigned)_telegram.proxyPort();
        sendAck_(cmd_id, doc);
    }

    void handleStorage_(uint16_t cmd_id, const String &action)
    {
        if (action != "list")
        {
            sendErr_(cmd_id, "unsupported");
            return;
        }
        JsonDocument doc;
        JsonArray arr = doc["files"].to<JsonArray>();
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file)
        {
            JsonObject o = arr.add<JsonObject>();
            o["name"] = file.name();
            o["size"] = (unsigned)file.size();
            file = root.openNextFile();
        }
        doc["total"] = (unsigned)LittleFS.totalBytes();
        doc["used"] = (unsigned)LittleFS.usedBytes();
        sendAck_(cmd_id, doc);
    }

    bool buildStatus_(std::vector<uint8_t> &out)
    {
        JsonDocument doc;
        doc["uptime_ms"] = (uint32_t)millis();
        doc["board_temp"] = _plc.boardTemp();
        doc["cpu_temp"] = _plc.cpuTemp();
        doc["fan_on"] = _plc.fanStatus();
        return serializeJsonToVec_(doc, out);
    }

    void scanI2c_()
    {
        _last_i2c.clear();
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
            for (const auto &addr : addrs)
                _last_i2c.push_back({bus, addr});
        }
    }

    void scanOw_()
    {
        _last_ow.clear();
        for (uint8_t i = 0; i < ActiveBoardProfile::ONEWIRE_COUNT; ++i)
        {
            OneWireBus *bus = _ow.busPtrByIndex(i);
            if (!bus)
                continue;
            uint8_t addr[8] = {};
            bus->reset_search();
            while (bus->search(addr))
            {
                if (OneWireBus::crc8(addr, 7) != addr[7])
                    continue;
                OwEntry e{};
                e.bus = i;
                addrToHex_(addr, e.addr);
                _last_ow.push_back(e);
            }
        }
    }

    void addrToHex_(const uint8_t in[8], char out[17]) const
    {
        static const char kHex[] = "0123456789ABCDEF";
        for (uint8_t i = 0; i < 8; ++i)
        {
            out[i * 2] = kHex[(in[i] >> 4) & 0x0F];
            out[i * 2 + 1] = kHex[in[i] & 0x0F];
        }
        out[16] = '\0';
    }

    bool setRtc_(const String &date, const String &time)
    {
        const int p1 = date.indexOf('-');
        const int p2 = (p1 >= 0) ? date.indexOf('-', p1 + 1) : -1;
        const int t1 = time.indexOf(':');
        const int t2 = (t1 >= 0) ? time.indexOf(':', t1 + 1) : -1;
        if (p1 <= 0 || p2 <= p1 || t1 <= 0 || t2 <= t1)
            return false;
        const uint16_t year = (uint16_t)date.substring(0, p1).toInt();
        const uint8_t month = (uint8_t)date.substring(p1 + 1, p2).toInt();
        const uint8_t day = (uint8_t)date.substring(p2 + 1).toInt();
        const uint8_t hour = (uint8_t)time.substring(0, t1).toInt();
        const uint8_t min = (uint8_t)time.substring(t1 + 1, t2).toInt();
        const uint8_t sec = (uint8_t)time.substring(t2 + 1).toInt();
        if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31)
            return false;
        if (hour > 23 || min > 59 || sec > 59)
            return false;
        Ds3231Mz::DateTime dt{};
        dt.year = year;
        dt.month = month;
        dt.day = day;
        dt.day_of_week = calcDow_(year, month, day);
        dt.hour = hour;
        dt.minute = min;
        dt.second = sec;
        return _rtc.setTime(dt);
    }

    static uint8_t calcDow_(uint16_t y, uint8_t m, uint8_t d)
    {
        static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (m < 3)
            y -= 1;
        const uint8_t dow = (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
        return (uint8_t)(dow + 1);
    }

    void sendAck_(uint16_t cmd_id)
    {
        JsonDocument empty;
        sendAck_(cmd_id, empty);
    }

    void sendAck_(uint16_t cmd_id, const JsonDocument &data)
    {
        JsonDocument doc;
        doc["cmd_id"] = cmd_id;
        doc["ok"] = true;
        if (!data.isNull())
            doc["data"] = data.as<JsonVariantConst>();
        sendJson_((uint8_t)StackMsgType::Ack, doc);
    }

    void sendErr_(uint16_t cmd_id, const char *msg)
    {
        JsonDocument doc;
        doc["cmd_id"] = cmd_id;
        doc["ok"] = false;
        doc["error"] = msg ? msg : "error";
        sendJson_((uint8_t)StackMsgType::Err, doc);
    }

    void sendJson_(uint8_t type, JsonDocument &doc)
    {
        std::vector<uint8_t> payload;
        if (!serializeJsonToVec_(doc, payload))
            return;
        _node->send(type, payload.data(), payload.size());
    }

    static bool serializeJsonToVec_(JsonDocument &doc, std::vector<uint8_t> &out)
    {
        String s;
        if (serializeJson(doc, s) == 0)
            return false;
        out.assign(s.begin(), s.end());
        return true;
    }
};
