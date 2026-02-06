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
#include <string.h>

#include "hal/bus/onewire.hpp"
#include "hal/dht22.hpp"
#include "hal/ds18b20.hpp"
#include "boards/board_profile.hpp"
#include "utils/logger.hpp"

class MeteoController
{
public:
    static constexpr size_t kSensorCount = 50;
    static constexpr uint8_t kInvalidPin = 0xFF;
    static constexpr uint8_t kAddrLen = 8;

    enum class SensorType : uint8_t
    {
        None = 0,
        Ds18b20,
        Dht22
    };

    struct SensorConfig
    {
        uint8_t id = 1;
        bool enabled = false;
        SensorType type = SensorType::None;
        uint8_t dht_pin = kInvalidPin;
        uint8_t ds18_addr[kAddrLen] = {};
        bool ds18_addr_set = false;
        uint32_t source_node_id = 0;
        uint8_t source_sensor_id = 0;
        String name;
    };

    struct SensorState
    {
        float temp_c = 0.0f;
        float humidity = 0.0f;
        bool has_temp = false;
        bool has_humidity = false;
        bool ok = false;
        uint8_t fail_count = 0;
        uint32_t last_read_ms = 0;
    };

    MeteoController(OneWireManager &ow, Logger &logs) : _ow(ow), _logs(logs) { reset_(); }
    using RemoteMeteoProvider = bool (*)(void *ctx, uint32_t node_id, uint8_t sensor_id,
                                         float &temp_c, bool &has_temp, float &hum, bool &has_hum, bool &ok);
    using RemoteNodeNameProvider = bool (*)(void *ctx, uint32_t node_id, String &out);
    using RemoteSensorNameProvider = bool (*)(void *ctx, uint32_t node_id, uint8_t sensor_id, String &out);
    using AlarmHandler = void (*)(void *ctx, uint32_t node_id, uint8_t sensor_id, bool alarm);
    void setRemoteMeteoProvider(RemoteMeteoProvider cb, void *ctx)
    {
        _remote_cb = cb;
        _remote_ctx = ctx;
    }
    void setRemoteNodeNameProvider(RemoteNodeNameProvider cb, void *ctx)
    {
        _remote_name_cb = cb;
        _remote_name_ctx = ctx;
    }
    void setRemoteSensorNameProvider(RemoteSensorNameProvider cb, void *ctx)
    {
        _remote_sensor_name_cb = cb;
        _remote_sensor_name_ctx = ctx;
    }
    void setAlarmHandler(AlarmHandler cb, void *ctx)
    {
        _alarm_cb = cb;
        _alarm_ctx = ctx;
    }

    bool begin()
    {
        _ds_bus = _ow.busPtrById(OneWireManager::OwBusType::Temp);
        if (_ds_bus)
        {
            _ds18b20.begin(*_ds_bus);
            _logs.info(F("METEO"), F("DS18B20 bus ready"));
        }
        else
        {
            _logs.warn(F("METEO"), F("DS18B20 bus missing"));
        }
        return true;
    }

    void task()
    {
        if (!_controller_enabled)
            return;
        const uint32_t now = millis();
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            const size_t idx = (_scan_index + i) % kSensorCount;
            if (readIfDue_(idx, now))
            {
                _scan_index = (idx + 1) % kSensorCount;
                return;
            }
        }
    }

    void applyConfig(JsonArrayConst sensors)
    {
        reset_();
        size_t idx = 0;
        for (JsonVariantConst v : sensors)
        {
            if (idx >= kSensorCount)
                break;
            if (!v.is<JsonObjectConst>())
            {
                ++idx;
                continue;
            }
            JsonObjectConst obj = v.as<JsonObjectConst>();
            uint8_t id = (uint8_t)(idx + 1);
            if (obj["id"].is<unsigned>())
            {
                const unsigned raw = obj["id"].as<unsigned>();
                if (raw <= 0xFFu)
                    id = (uint8_t)raw;
            }
            size_t dst = 0;
            if (!indexById_(id, dst))
            {
                ++idx;
                continue;
            }
            SensorConfig &cfg = _cfg[dst];
            cfg.id = id;
            bool enabled_set = false;
            if (obj["enabled"].is<bool>())
            {
                cfg.enabled = obj["enabled"].as<bool>();
                enabled_set = true;
            }
            if (obj["type"].is<const char *>() || obj["type"].is<unsigned>())
                cfg.type = parseType_(obj["type"]);
            if (cfg.type == SensorType::None)
            {
                if (obj["pin"].is<unsigned>())
                    cfg.type = SensorType::Dht22;
                else if (obj["addr"].is<const char *>())
                    cfg.type = SensorType::Ds18b20;
            }

            if (cfg.type == SensorType::Dht22)
                parsePin_(obj["pin"], cfg.dht_pin);
            if (cfg.type == SensorType::Ds18b20 && obj["addr"].is<const char *>())
                cfg.ds18_addr_set = parseHexAddr(obj["addr"].as<const char *>(), cfg.ds18_addr);
            if (obj["name"].is<const char *>())
                cfg.name = obj["name"].as<const char *>();
            if (obj["src_node"].is<unsigned>())
                cfg.source_node_id = (uint32_t)obj["src_node"].as<unsigned>();
            if (obj["src_sensor"].is<unsigned>())
            {
                const unsigned src = obj["src_sensor"].as<unsigned>();
                if (src > 0 && src <= kSensorCount)
                    cfg.source_sensor_id = (uint8_t)src;
            }
            if (cfg.source_node_id == 0 || cfg.source_sensor_id == 0)
            {
                cfg.source_node_id = 0;
                cfg.source_sensor_id = 0;
            }

            if (!enabled_set)
                cfg.enabled = true;
            ++idx;
        }
    }

    void serialize(JsonArray out) const
    {
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            const SensorConfig &cfg = _cfg[i];
            if (!cfg.enabled)
                continue;
            JsonObject obj = out.add<JsonObject>();
            obj["id"] = cfg.id;
            obj["enabled"] = cfg.enabled;
            if (cfg.name.length())
                obj["name"] = cfg.name;
            obj["type"] = typeName_(cfg.type);
            if (cfg.type == SensorType::Dht22 && cfg.dht_pin != kInvalidPin)
                obj["pin"] = cfg.dht_pin;
            if (cfg.type == SensorType::Ds18b20 && cfg.ds18_addr_set)
            {
                char hex[17] = {};
                formatHexAddr(cfg.ds18_addr, hex);
                obj["addr"] = hex;
            }
            if (cfg.source_node_id && cfg.source_sensor_id)
            {
                obj["src_node"] = (unsigned long)cfg.source_node_id;
                obj["src_sensor"] = (unsigned)cfg.source_sensor_id;
            }
        }
    }

    bool controllerEnabled() const { return _controller_enabled; }
    void setControllerEnabled(bool enabled)
    {
        if (_controller_enabled == enabled)
            return;
        _controller_enabled = enabled;
        _ds_conv_pending = false;
        _ds_conv_ready = false;
        _ds_last_conv_ms = 0;
        if (!_controller_enabled)
        {
            reset_();
            return;
        }
        _dht22_pin = kInvalidPin;
        for (size_t i = 0; i < kSensorCount; ++i)
            _state[i] = SensorState{};
    }

    bool setEnabled(size_t id, bool enable)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        if (!enable)
        {
            SensorConfig &cfg = _cfg[idx];
            const uint8_t saved_id = cfg.id;
            cfg = SensorConfig{};
            cfg.id = saved_id;
            cfg.enabled = false;
            _state[idx] = SensorState{};
            _logs.info(F("METEO"), F("id: %u enabled: false"), (unsigned)cfg.id);
            return true;
        }
        _cfg[idx].enabled = true;
        _logs.info(F("METEO"), F("id: %u enabled: true"), (unsigned)_cfg[idx].id);
        return true;
    }

    bool setType(size_t id, SensorType type)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        SensorConfig &cfg = _cfg[idx];
        cfg.type = type;
        cfg.source_node_id = 0;
        cfg.source_sensor_id = 0;
        if (type == SensorType::Dht22)
        {
            cfg.ds18_addr_set = false;
            memset(cfg.ds18_addr, 0, sizeof(cfg.ds18_addr));
        }
        else if (type == SensorType::Ds18b20)
        {
            cfg.dht_pin = kInvalidPin;
        }
        else
        {
            cfg.dht_pin = kInvalidPin;
            cfg.ds18_addr_set = false;
            memset(cfg.ds18_addr, 0, sizeof(cfg.ds18_addr));
        }
        _state[idx] = SensorState{};
        return true;
    }

    bool setDht22Pin(size_t id, uint8_t pin)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        SensorConfig &cfg = _cfg[idx];
        cfg.type = SensorType::Dht22;
        cfg.source_node_id = 0;
        cfg.source_sensor_id = 0;
        cfg.dht_pin = pin;
        cfg.ds18_addr_set = false;
        memset(cfg.ds18_addr, 0, sizeof(cfg.ds18_addr));
        _state[idx] = SensorState{};
        return true;
    }

    bool setDs18b20Addr(size_t id, const uint8_t addr[kAddrLen], bool set)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        SensorConfig &cfg = _cfg[idx];
        cfg.type = SensorType::Ds18b20;
        cfg.source_node_id = 0;
        cfg.source_sensor_id = 0;
        cfg.dht_pin = kInvalidPin;
        if (set)
        {
            for (uint8_t i = 0; i < kAddrLen; ++i)
                cfg.ds18_addr[i] = addr[i];
            cfg.ds18_addr_set = true;
        }
        else
        {
            memset(cfg.ds18_addr, 0, sizeof(cfg.ds18_addr));
            cfg.ds18_addr_set = false;
        }
        _state[idx] = SensorState{};
        return true;
    }

    bool setName(size_t id, const String &name)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        _cfg[idx].name = name;
        return true;
    }

    bool setRemoteSource(size_t id, uint32_t node_id, uint8_t sensor_id)
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return false;
        SensorConfig &cfg = _cfg[idx];
        if (node_id == 0 || sensor_id == 0)
        {
            cfg.source_node_id = 0;
            cfg.source_sensor_id = 0;
        }
        else
        {
            cfg.source_node_id = node_id;
            cfg.source_sensor_id = sensor_id;
        }
        _state[idx] = SensorState{};
        return true;
    }

    const SensorConfig *config(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_cfg[idx];
    }

    bool displayName(uint8_t id, String &out) const
    {
        const SensorConfig *cfg = config(id);
        if (!cfg)
            return false;
        if (cfg->name.length())
        {
            out = cfg->name;
            return true;
        }
        if (cfg->source_node_id && cfg->source_sensor_id)
        {
            String sensor;
            if (_remote_sensor_name_cb &&
                _remote_sensor_name_cb(_remote_sensor_name_ctx, cfg->source_node_id, cfg->source_sensor_id, sensor) &&
                sensor.length())
            {
                out = sensor;
            }
            else
            {
                out = String("Sensor ") + String((unsigned)cfg->source_sensor_id);
            }
            return true;
        }
        out = String("Sensor ") + String((unsigned)cfg->id);
        return true;
    }

    const SensorState *state(size_t id) const
    {
        size_t idx = 0;
        if (!indexById_(id, idx))
            return nullptr;
        return &_state[idx];
    }

    const SensorConfig *configByIndex(size_t idx) const
    {
        if (idx >= kSensorCount)
            return nullptr;
        return &_cfg[idx];
    }

    const SensorState *stateByIndex(size_t idx) const
    {
        if (idx >= kSensorCount)
            return nullptr;
        return &_state[idx];
    }

    void listDs18b20Serials(char out[][17], size_t max, size_t &count)
    {
        count = 0;
        if (!_ds_bus || !out || max == 0)
            return;
        _ds18b20.begin(*_ds_bus);
        _ds18b20.listSerials(out, max, count);
    }

    static bool parseHexAddr(const char *hex, uint8_t out[kAddrLen])
    {
        if (!hex)
            return false;
        const size_t len = strlen(hex);
        if (len != 16)
            return false;
        for (uint8_t i = 0; i < kAddrLen; ++i)
        {
            const int hi = hexNibble_(hex[i * 2]);
            const int lo = hexNibble_(hex[i * 2 + 1]);
            if (hi < 0 || lo < 0)
                return false;
            out[i] = (uint8_t)((hi << 4) | lo);
        }
        return true;
    }

    static void formatHexAddr(const uint8_t addr[kAddrLen], char out[17])
    {
        static const char kHex[] = "0123456789ABCDEF";
        for (uint8_t i = 0; i < kAddrLen; ++i)
        {
            out[i * 2] = kHex[(addr[i] >> 4) & 0x0F];
            out[i * 2 + 1] = kHex[addr[i] & 0x0F];
        }
        out[16] = '\0';
    }

    static const char *typeName(SensorType type)
    {
        return typeName_(type);
    }

private:
    static constexpr uint32_t kDht22IntervalMs = 2000;
    static constexpr uint32_t kDs18b20IntervalMs = 1000;
    static constexpr uint32_t kRemoteIntervalMs = 2000;
    static constexpr uint8_t kFailThreshold = 10;

    OneWireManager &_ow;
    Logger &_logs;
    OneWireBus *_ds_bus = nullptr;
    Ds18b20 _ds18b20;
    DHT22 _dht22;
    uint8_t _dht22_pin = kInvalidPin;
    bool _ds_conv_pending = false;
    bool _ds_conv_ready = false;
    uint32_t _ds_last_conv_ms = 0;
    SensorConfig _cfg[kSensorCount];
    SensorState _state[kSensorCount];
    bool _controller_enabled = false;
    size_t _scan_index = 0;
    RemoteMeteoProvider _remote_cb = nullptr;
    void *_remote_ctx = nullptr;
    RemoteNodeNameProvider _remote_name_cb = nullptr;
    void *_remote_name_ctx = nullptr;
    RemoteSensorNameProvider _remote_sensor_name_cb = nullptr;
    void *_remote_sensor_name_ctx = nullptr;
    AlarmHandler _alarm_cb = nullptr;
    void *_alarm_ctx = nullptr;

    void reset_()
    {
        _dht22_pin = kInvalidPin;
        _ds_conv_pending = false;
        _ds_conv_ready = false;
        _ds_last_conv_ms = 0;
        for (size_t i = 0; i < kSensorCount; ++i)
        {
            _cfg[i] = SensorConfig{};
            _state[i] = SensorState{};
            _cfg[i].id = (uint8_t)(i + 1);
        }
    }

    bool indexById_(uint8_t id, size_t &out) const
    {
        if (id == 0 || id > kSensorCount)
            return false;
        out = (size_t)(id - 1);
        return true;
    }

    bool readIfDue_(size_t idx, uint32_t now)
    {
        SensorConfig &cfg = _cfg[idx];
        SensorState &st = _state[idx];
        if (!cfg.enabled)
            return false;
        if (cfg.source_node_id && cfg.source_sensor_id)
            return readRemoteIfDue_(cfg, st, now);
        if (cfg.type == SensorType::None)
            return false;
        const uint32_t interval = (cfg.type == SensorType::Dht22) ? kDht22IntervalMs : kDs18b20IntervalMs;
        if (st.last_read_ms && (uint32_t)(now - st.last_read_ms) < interval)
            return false;
        if (cfg.type == SensorType::Ds18b20)
            return readDs18b20IfDue_(cfg, st, now);
        float t = 0.0f;
        float h = 0.0f;
        bool has_temp = false;
        bool has_hum = false;
        const bool ok = readDht22_(cfg, t, has_temp, h, has_hum);
        applyReadResult_(cfg, st, ok, has_temp, t, has_hum, h);
        st.last_read_ms = now;
        return true;
    }

    bool readRemoteIfDue_(const SensorConfig &cfg, SensorState &st, uint32_t now)
    {
        if (st.last_read_ms && (uint32_t)(now - st.last_read_ms) < kRemoteIntervalMs)
            return false;
        bool has_temp = false;
        bool has_hum = false;
        bool ok = false;
        float temp_c = 0.0f;
        float hum = 0.0f;
        if (_remote_cb)
            _remote_cb(_remote_ctx, cfg.source_node_id, cfg.source_sensor_id, temp_c, has_temp, hum, has_hum, ok);
        applyReadResult_(cfg, st, ok, has_temp, temp_c, has_hum, hum);
        st.last_read_ms = now;
        return true;
    }

    bool readDs18b20IfDue_(const SensorConfig &cfg, SensorState &st, uint32_t now)
    {
        if (!_ds_bus || !cfg.ds18_addr_set)
        {
            applyReadResult_(cfg, st, false, false, 0.0f, false, 0.0f);
            st.last_read_ms = now;
            return true;
        }
        updateDs18Conversion_(now);
        if (_ds_conv_pending || !_ds_conv_ready)
            return false;
        float t = 0.0f;
        uint8_t addr[kAddrLen] = {};
        for (uint8_t i = 0; i < kAddrLen; ++i)
            addr[i] = cfg.ds18_addr[i];
        const bool ok = _ds18b20.readTempCNoWait(addr, t);
        applyReadResult_(cfg, st, ok, ok, t, false, 0.0f);
        st.last_read_ms = now;
        return true;
    }

    bool readDht22_(const SensorConfig &cfg, float &out_temp, bool &out_has_temp, float &out_hum, bool &out_has_hum)
    {
        if (cfg.dht_pin == kInvalidPin)
        {
            return false;
        }
        uint8_t gpio = 0xFF;
        if (!mapDhtPinToGpio_(cfg.dht_pin, gpio))
        {
            return false;
        }
        float t = 0.0f;
        float h = 0.0f;
        if (_dht22_pin != gpio)
        {
            _dht22.begin(gpio);
            _dht22_pin = gpio;
        }
        if (!_dht22.read(t, h))
        {
            return false;
        }
        out_temp = t;
        out_hum = h;
        out_has_temp = true;
        out_has_hum = true;
        return true;
    }

    static bool mapDhtPinToGpio_(uint8_t port, uint8_t &gpio)
    {
        if (port >= PortIO::PORT_COUNT)
            return false;
        const auto &p = ActiveBoardProfile::PORTS[port];
        if (p.caps == Cap::None)
            return false;
        if (p.backend != PortIO::Backend::Esp32)
            return false;
        if (p.u.esp.gpio == 0xFF)
            return false;
        gpio = p.u.esp.gpio;
        return true;
    }

    static SensorType parseType_(JsonVariantConst v)
    {
        if (v.is<unsigned>())
        {
            const unsigned raw = v.as<unsigned>();
            if (raw <= (unsigned)SensorType::Dht22)
                return (SensorType)raw;
            return SensorType::None;
        }
        if (v.is<const char *>())
        {
            String t = v.as<const char *>();
            t.toLowerCase();
            if (t == "ds18b20")
                return SensorType::Ds18b20;
            if (t == "dht22")
                return SensorType::Dht22;
        }
        return SensorType::None;
    }

    static bool parsePin_(JsonVariantConst v, uint8_t &out)
    {
        if (v.is<unsigned>())
        {
            const unsigned val = v.as<unsigned>();
            if (val <= 0xFFu)
            {
                out = (uint8_t)val;
                return true;
            }
        }
        return false;
    }

    static const char *typeName_(SensorType type)
    {
        switch (type)
        {
        case SensorType::Ds18b20:
            return "ds18b20";
        case SensorType::Dht22:
            return "dht22";
        case SensorType::None:
        default:
            return "none";
        }
    }

    static int hexNibble_(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return 10 + (c - 'A');
        if (c >= 'a' && c <= 'f')
            return 10 + (c - 'a');
        return -1;
    }

    void updateDs18Conversion_(uint32_t now)
    {
        if (!_ds_bus)
            return;
        if (_ds_conv_pending)
        {
            if (_ds18b20.ready())
            {
                _ds_conv_pending = false;
                _ds_conv_ready = true;
                _ds_last_conv_ms = now;
            }
            return;
        }
        if (!_ds_conv_ready || (uint32_t)(now - _ds_last_conv_ms) >= kDs18b20IntervalMs)
        {
            if (_ds18b20.startConversion())
                _ds_conv_pending = true;
            else
                _ds_conv_ready = false;
        }
    }

    void logMeteoStateChange_(const SensorConfig &cfg, const SensorState &st, bool prev_ok)
    {
        if (prev_ok == st.ok)
            return;
        String name = cfg.name.length() ? cfg.name : String((unsigned)cfg.id);
        if (cfg.source_node_id)
        {
            String remote;
            if (_remote_name_cb && _remote_name_cb(_remote_name_ctx, cfg.source_node_id, remote) &&
                remote.length())
            {
                name += " @";
                name += remote;
            }
            else
            {
                char buf[12] = {};
                snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)cfg.source_node_id);
                name += " @";
                name += buf;
            }
        }
        const char *type = typeName_(cfg.type);
        if (st.ok)
            _logs.info(F("METEO"), F("sensor ok: id: %u name: %s type: %s"),
                       (unsigned)cfg.id, name.c_str(), type);
        else
            _logs.warn(F("METEO"), F("sensor error: id: %u name: %s type: %s"),
                       (unsigned)cfg.id, name.c_str(), type);
        if (_alarm_cb)
            _alarm_cb(_alarm_ctx, cfg.source_node_id, cfg.id, !st.ok);
    }

    void applyReadResult_(const SensorConfig &cfg, SensorState &st, bool ok, bool has_temp, float temp_c, bool has_hum,
                          float hum)
    {
        const bool was_error = (st.fail_count >= kFailThreshold);
        if (ok)
        {
            st.temp_c = temp_c;
            st.humidity = hum;
            st.has_temp = has_temp;
            st.has_humidity = has_hum;
            st.fail_count = 0;
            st.ok = true;
            if (was_error)
                logMeteoStateChange_(cfg, st, false);
            return;
        }

        if (st.fail_count < 0xFF)
            ++st.fail_count;
        if (st.fail_count >= kFailThreshold)
        {
            st.ok = false;
            st.has_temp = false;
            st.has_humidity = false;
            if (!was_error)
                logMeteoStateChange_(cfg, st, true);
            return;
        }

        st.ok = true;
    }
};
