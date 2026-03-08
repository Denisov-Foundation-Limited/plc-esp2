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

#include "controllers/meteo_controller.hpp"

#include <string.h>

#include "boards/board_profile.hpp"

MeteoController::MeteoController(OneWireManager &ow, Logger &logs) : _ow(ow), _logs(logs){ reset_(); }

void MeteoController::setRemoteMeteoProvider(MeteoController::RemoteMeteoProvider cb, void *ctx){
    _remote_cb = cb;
    _remote_ctx = ctx;
}

void MeteoController::setRemoteNodeNameProvider(MeteoController::RemoteNodeNameProvider cb, void *ctx){
    _remote_name_cb = cb;
    _remote_name_ctx = ctx;
}

void MeteoController::setRemoteSensorNameProvider(MeteoController::RemoteSensorNameProvider cb, void *ctx){
    _remote_sensor_name_cb = cb;
    _remote_sensor_name_ctx = ctx;
}

void MeteoController::setRemoteSensorTypeProvider(MeteoController::RemoteSensorTypeProvider cb, void *ctx){
    _remote_sensor_type_cb = cb;
    _remote_sensor_type_ctx = ctx;
}

void MeteoController::setAlarmHandler(MeteoController::AlarmHandler cb, void *ctx){
    _alarm_cb = cb;
    _alarm_ctx = ctx;
}

bool MeteoController::begin(){
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

void MeteoController::task(){
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

void MeteoController::applyConfig(JsonArrayConst sensors){
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
        if (obj["group_id"].is<unsigned>())
        {
            const unsigned raw = obj["group_id"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.group_id = (uint8_t)raw;
        }
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

void MeteoController::serialize(JsonArray out) const{
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
        if (cfg.group_id != 0)
            obj["group_id"] = cfg.group_id;
    }
}

bool MeteoController::controllerEnabled() const{ return _controller_enabled; }

void MeteoController::setControllerEnabled(bool enabled){
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

bool MeteoController::setEnabled(size_t id, bool enable){
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

bool MeteoController::setType(size_t id, MeteoController::SensorType type){
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

bool MeteoController::setDht22Pin(size_t id, uint8_t pin){
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

bool MeteoController::setDs18b20Addr(size_t id, const uint8_t addr[MeteoController::kAddrLen], bool set){
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

bool MeteoController::setName(size_t id, const String &name){
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].name = name;
    return true;
}

bool MeteoController::setGroupId(size_t id, uint8_t group_id){
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].group_id = group_id;
    return true;
}

bool MeteoController::setRemoteSource(size_t id, uint32_t node_id, uint8_t sensor_id){
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

const MeteoController::SensorConfig *MeteoController::config(size_t id) const{
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_cfg[idx];
}

bool MeteoController::displayName(uint8_t id, String &out) const{
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

const MeteoController::SensorState *MeteoController::state(size_t id) const{
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_state[idx];
}

const MeteoController::SensorConfig *MeteoController::configByIndex(size_t idx) const{
    if (idx >= kSensorCount)
        return nullptr;
    return &_cfg[idx];
}

const MeteoController::SensorState *MeteoController::stateByIndex(size_t idx) const{
    if (idx >= kSensorCount)
        return nullptr;
    return &_state[idx];
}

void MeteoController::listDs18b20Serials(char out[][17], size_t max, size_t &count){
    count = 0;
    if (!_ds_bus || !out || max == 0)
        return;
    _ds18b20.begin(*_ds_bus);
    _ds18b20.listSerials(out, max, count);
}

bool MeteoController::parseHexAddr(const char *hex, uint8_t out[MeteoController::kAddrLen]){
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

void MeteoController::formatHexAddr(const uint8_t addr[MeteoController::kAddrLen], char out[17]){
    static const char kHex[] = "0123456789ABCDEF";
    for (uint8_t i = 0; i < kAddrLen; ++i)
    {
        out[i * 2] = kHex[(addr[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[addr[i] & 0x0F];
    }
    out[16] = '\0';
}

const char *MeteoController::typeName(MeteoController::SensorType type){
    return typeName_(type);
}

void MeteoController::reset_(){
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

bool MeteoController::indexById_(uint8_t id, size_t &out) const{
    if (id == 0 || id > kSensorCount)
        return false;
    out = (size_t)(id - 1);
    return true;
}

bool MeteoController::readIfDue_(size_t idx, uint32_t now){
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

bool MeteoController::readRemoteIfDue_(const MeteoController::SensorConfig &cfg, MeteoController::SensorState &st, uint32_t now){
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

bool MeteoController::readDs18b20IfDue_(const MeteoController::SensorConfig &cfg, MeteoController::SensorState &st, uint32_t now){
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

bool MeteoController::readDht22_(const MeteoController::SensorConfig &cfg, float &out_temp, bool &out_has_temp, float &out_hum, bool &out_has_hum){
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

bool MeteoController::mapDhtPinToGpio_(uint8_t port, uint8_t &gpio){
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

MeteoController::SensorType MeteoController::parseType_(JsonVariantConst v){
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

bool MeteoController::parsePin_(JsonVariantConst v, uint8_t &out){
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

const char *MeteoController::typeName_(MeteoController::SensorType type){
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

const char *MeteoController::typeNameLog_(MeteoController::SensorType type){
    switch (type)
    {
    case SensorType::Ds18b20:
        return "DS18B20";
    case SensorType::Dht22:
        return "DHT22";
    case SensorType::None:
    default:
        return "None";
    }
}

int MeteoController::hexNibble_(char c){
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    return -1;
}

void MeteoController::updateDs18Conversion_(uint32_t now){
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

void MeteoController::logMeteoStateChange_(const MeteoController::SensorConfig &cfg, const MeteoController::SensorState &st, bool prev_ok){
    if (prev_ok == st.ok)
        return;
    String name = cfg.name.length() ? cfg.name : String((unsigned)cfg.id);
    String source;
    if (cfg.source_node_id)
    {
        if (!(_remote_name_cb && _remote_name_cb(_remote_name_ctx, cfg.source_node_id, source) &&
              source.length()))
        {
            char buf[12] = {};
            snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)cfg.source_node_id);
            source = buf;
        }
    }
    SensorType log_type = cfg.type;
    if (log_type == SensorType::None && cfg.source_node_id && cfg.source_sensor_id && _remote_sensor_type_cb)
    {
        SensorType remote_type = SensorType::None;
        if (_remote_sensor_type_cb(_remote_sensor_type_ctx, cfg.source_node_id, cfg.source_sensor_id,
                                   remote_type) &&
            remote_type != SensorType::None)
            log_type = remote_type;
    }
    const char *type = typeNameLog_(log_type);
    if (st.ok)
    {
        if (source.length())
            _logs.info(F("METEO"), F("Sensor ok: id: %u name: %s source: %s type: %s"),
                       (unsigned)cfg.id, name.c_str(), source.c_str(), type);
        else
            _logs.info(F("METEO"), F("Sensor ok: id: %u name: %s type: %s"),
                       (unsigned)cfg.id, name.c_str(), type);
    }
    else
    {
        if (source.length())
            _logs.warn(F("METEO"), F("Sensor error: id: %u name: %s source: %s type: %s"),
                       (unsigned)cfg.id, name.c_str(), source.c_str(), type);
        else
            _logs.warn(F("METEO"), F("Sensor error: id: %u name: %s type: %s"),
                       (unsigned)cfg.id, name.c_str(), type);
    }
    if (_alarm_cb)
        _alarm_cb(_alarm_ctx, cfg.source_node_id, cfg.id, !st.ok);
}

void MeteoController::applyReadResult_(const MeteoController::SensorConfig &cfg, MeteoController::SensorState &st, bool ok, bool has_temp, float temp_c, bool has_hum,
 float hum){
    const bool was_error = (st.fail_count >= kFailThreshold);
    if (ok)
    {
        const bool first_remote_success =
            !st.had_success && (cfg.type == SensorType::None) &&
            (cfg.source_node_id != 0) && (cfg.source_sensor_id != 0);
        st.temp_c = temp_c;
        st.humidity = hum;
        st.has_temp = has_temp;
        st.has_humidity = has_hum;
        st.fail_count = 0;
        st.ok = true;
        st.had_success = true;
        if (was_error || first_remote_success)
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
        const bool suppress_initial_remote_error =
            (cfg.type == SensorType::None) && (cfg.source_node_id != 0) && (cfg.source_sensor_id != 0) &&
            !st.had_success;
        if (!was_error && !suppress_initial_remote_error)
            logMeteoStateChange_(cfg, st, true);
        return;
    }

    st.ok = true;
}
