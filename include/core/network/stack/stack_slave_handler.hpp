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
#include <new>
#include <stdint.h>
#include <vector>

#if defined(ARDUINO_ARCH_ESP32)
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"
#include "soc/soc_memory_types.h"
#endif

#include "boards/board_profile.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_node.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "core/network/stack/stack_types.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/ds18b20.hpp"
#include "hal/io_stack.hpp"
#include "hal/gpio/extender.hpp"
#include "core/rtc.hpp"
#include "plc/plc_control.hpp"
#include "core/network/telegram/telegram.hpp"
#include "controllers/meteo_controller.hpp"
#include "controllers/septic_controller.hpp"
#include "controllers/security_controller.hpp"
#include "controllers/socket_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "controllers/tank_controller.hpp"
#include "controllers/ring_controller.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"

class StackSlaveHandler
{
public:
    struct RemoteMeteoItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool ok = false;
        bool has_temp = false;
        bool has_hum = false;
        float temp_c = 0.0f;
        float hum = 0.0f;
        static constexpr size_t kNameLen = 64;
        static constexpr size_t kTypeLen = 24;
        static constexpr size_t kAddrLen = 24;
        char name[kNameLen] = {};
        char type[kTypeLen] = {};
        char addr[kAddrLen] = {};
        int pin = -1;
    };
    struct RemoteMeteoCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        String node_name;
        RemoteMeteoItem *items = nullptr;
        size_t capacity = MeteoController::kSensorCount;
        size_t item_count = 0;
        void reset()
        {
            node_id = 0;
            updated_ms = 0;
            pending_cmd_id = 0;
            pending = false;
            has_data = false;
            last_ok = false;
            last_error = String();
            node_name = String();
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = RemoteMeteoItem{};
        }
    };
    struct RemoteSocketItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool state = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct RemoteSocketsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        RemoteSocketItem *items = nullptr;
        size_t capacity = SocketController::kSocketCount;
        size_t item_count = 0;
        void reset()
        {
            node_id = 0;
            updated_ms = 0;
            pending_cmd_id = 0;
            pending = false;
            has_data = false;
            last_ok = false;
            last_error = String();
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = RemoteSocketItem{};
        }
    };
    struct RemoteLightItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool state = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct RemoteLightsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        RemoteLightItem *items = nullptr;
        size_t capacity = SocketController::kLightCount;
        size_t item_count = 0;
        void reset()
        {
            node_id = 0;
            updated_ms = 0;
            pending_cmd_id = 0;
            pending = false;
            has_data = false;
            last_ok = false;
            last_error = String();
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = RemoteLightItem{};
        }
    };
    struct RemoteSepticItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool warning = false;
        bool alarm = false;
    };
    struct RemoteSepticCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        RemoteSepticItem *items = nullptr;
        size_t capacity = SepticController::kSepticCount;
        size_t item_count = 0;
        void reset()
        {
            node_id = 0;
            updated_ms = 0;
            pending_cmd_id = 0;
            pending = false;
            has_data = false;
            last_ok = false;
            last_error = String();
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = RemoteSepticItem{};
        }
    };
    struct RemoteThermoItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool power_on = false;
        bool heat_on = false;
        bool cool_on = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct RemoteThermoCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        RemoteThermoItem *items = nullptr;
        size_t capacity = ThermoController::kDeviceCount;
        size_t item_count = 0;
        void reset()
        {
            node_id = 0;
            updated_ms = 0;
            pending_cmd_id = 0;
            pending = false;
            has_data = false;
            last_ok = false;
            last_error = String();
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = RemoteThermoItem{};
        }
    };
    struct RemoteTankItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool levels_ok = false;
        bool level_low = false;
        bool level_mid = false;
        bool level_full = false;
    };
    struct RemoteTanksCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        RemoteTankItem *items = nullptr;
        size_t capacity = TankController::kTankCount;
        size_t item_count = 0;
        void reset()
        {
            node_id = 0;
            updated_ms = 0;
            pending_cmd_id = 0;
            pending = false;
            has_data = false;
            last_ok = false;
            last_error = String();
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = RemoteTankItem{};
        }
    };
    struct RemoteSecurityItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool detect = false;
        bool silent = false;
        uint8_t port = SecurityController::kInvalidPort;
        static constexpr size_t kTypeLen = 24;
        static constexpr size_t kNameLen = 48;
        char type[kTypeLen] = {};
        char name[kNameLen] = {};
    };
    struct RemoteSecurityCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        bool enabled = false;
        bool armed = false;
        bool alarm = false;
        RemoteSecurityItem *items = nullptr;
        size_t capacity = SecurityController::kSensorCount;
        size_t item_count = 0;
        void reset()
        {
            node_id = 0;
            updated_ms = 0;
            pending_cmd_id = 0;
            pending = false;
            has_data = false;
            last_ok = false;
            last_error = String();
            enabled = false;
            armed = false;
            alarm = false;
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = RemoteSecurityItem{};
        }
    };

    StackSlaveHandler(IoStack &io, Ds18b20 &ds18b20, OneWireManager &ow, I2CManager &i2c,
                      PlcControl &plc, RTC &rtc, TelegramClient &telegram, Logger &logs,
                      Extender &ext, SocketController &sockets, MeteoController &meteo,
                      ThermoController &thermo, SepticController &septic, SecurityController &security,
                      TankController &tanks, RingController &ring)
        : _io(io),
          _ds18b20(ds18b20),
          _ow(ow),
          _i2c(i2c),
          _plc(plc),
          _rtc(rtc),
          _telegram(telegram),
          _logs(logs),
          _ext(ext),
          _sockets(sockets),
          _meteo(meteo),
          _thermo(thermo),
          _septic(septic),
          _security(security),
          _tanks(tanks),
          _ring(ring)
    {
    }
    ~StackSlaveHandler()
    {
        releaseRemoteMeteo_();
        releaseRemoteDevices_();
    }

    void attach(StackNode &node)
    {
        _node = &node;
        node.setFrameHandler(&StackSlaveHandler::onFrame_, this);
        node.setStatusProvider(&StackSlaveHandler::onStatus_, this);
    }
    void setConfigsManager(ConfigsManagerIface &cfg) { _configs = &cfg; }
    void initAllocations()
    {
        if (_alloc_ready)
            return;
        initRemoteMeteo_();
        initRemoteDevices_();
        _alloc_ready = true;
    }
    void loop()
    {
        updateRemoteMeteo_();
        updateBuzzer_();
    }
    const RemoteMeteoCache *remoteMeteoCache(uint32_t node_id) const { return findRemoteMeteoCache_(node_id, false); }
    size_t remoteMeteoCacheSlots() const { return StackMaster::MAX_SESSIONS; }
    const RemoteMeteoCache &remoteMeteoCacheAt(size_t idx) const { return _remote_meteo_cache[idx]; }
    bool requestRemoteMeteoAll() { return requestRemoteMeteoAll_(); }
    const RemoteSocketsCache *remoteSocketsCache(uint32_t node_id) const { return findRemoteSocketsCache_(node_id, false); }
    const RemoteLightsCache *remoteLightsCache(uint32_t node_id) const { return findRemoteLightsCache_(node_id, false); }
    const RemoteSepticCache *remoteSepticCache(uint32_t node_id) const { return findRemoteSepticCache_(node_id, false); }
    const RemoteThermoCache *remoteThermoCache(uint32_t node_id) const { return findRemoteThermoCache_(node_id, false); }
    const RemoteTanksCache *remoteTanksCache(uint32_t node_id) const { return findRemoteTanksCache_(node_id, false); }
    const RemoteSecurityCache *remoteSecurityCache(uint32_t node_id) const { return findRemoteSecurityCache_(node_id, false); }
    bool requestRemoteSockets(uint32_t node_id) { return requestRemoteSockets_(node_id); }
    bool requestRemoteLights(uint32_t node_id) { return requestRemoteLights_(node_id); }
    bool requestRemoteSeptic(uint32_t node_id) { return requestRemoteSeptic_(node_id); }
    bool requestRemoteThermo(uint32_t node_id) { return requestRemoteThermo_(node_id); }
    bool requestRemoteTanks(uint32_t node_id) { return requestRemoteTanks_(node_id); }
    bool requestRemoteSecurity(uint32_t node_id) { return requestRemoteSecurity_(node_id); }
    bool remoteMeteoTemp(uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp) const
    {
        const RemoteMeteoCache *cache = findRemoteMeteoCache_(node_id, false);
        if (!cache || !cache->has_data || !cache->items)
            return false;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const RemoteMeteoItem &it = cache->items[i];
            if (it.id != sensor_id)
                continue;
            temp_c = it.temp_c;
            has_temp = it.has_temp;
            return true;
        }
        return false;
    }

    bool remoteMeteoRead(uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp,
                         float &hum, bool &has_hum, bool &ok) const
    {
        const RemoteMeteoCache *cache = findRemoteMeteoCache_(node_id, false);
        if (!cache || !cache->has_data || !cache->items)
            return false;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const RemoteMeteoItem &it = cache->items[i];
            if (it.id != sensor_id)
                continue;
            temp_c = it.temp_c;
            hum = it.hum;
            has_temp = it.has_temp;
            has_hum = it.has_hum;
            ok = it.ok;
            return true;
        }
        return false;
    }

private:
    static void copyStr_(char *dst, size_t size, const char *src)
    {
        if (!dst || size == 0)
            return;
        if (!src)
        {
            dst[0] = '\0';
            return;
        }
        size_t i = 0;
        for (; i + 1 < size && src[i]; ++i)
            dst[i] = src[i];
        dst[i] = '\0';
    }

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

    IoStack &_io;
    Ds18b20 &_ds18b20;
    OneWireManager &_ow;
    I2CManager &_i2c;
    PlcControl &_plc;
    RTC &_rtc;
    TelegramClient &_telegram;
    Logger &_logs;
    Extender &_ext;
    SocketController &_sockets;
    MeteoController &_meteo;
    ThermoController &_thermo;
    SepticController &_septic;
    SecurityController &_security;
    TankController &_tanks;
    RingController &_ring;
    StackNode *_node = nullptr;
    ConfigsManagerIface *_configs = nullptr;
    bool _alloc_ready = false;
    static constexpr uint8_t MAX_I2C_ADDRS = 127;
    static constexpr uint8_t MAX_OW_ADDRS = 64;
    static constexpr size_t kDocCapacity = 4096;
    I2cEntry _last_i2c[MAX_I2C_ADDRS] = {};
    uint8_t _last_i2c_count = 0;
    OwEntry _last_ow[MAX_OW_ADDRS] = {};
    uint8_t _last_ow_count = 0;
    DynamicJsonDocument _rx_doc{kDocCapacity};
    DynamicJsonDocument _tx_doc{kDocCapacity};
    DynamicJsonDocument _msg_doc{kDocCapacity};
    uint8_t _tx_payload_buf[StackCodec::kMaxPayload] = {};
    bool _rfid_io_ready = false;
    bool _beep_active = false;
    uint8_t _beep_remaining = 0;
    uint16_t _beep_on_ms = 0;
    uint16_t _beep_off_ms = 0;
    bool _beep_state_on = false;
    uint32_t _beep_next_ms = 0;
    RemoteMeteoCache _remote_meteo_cache[StackMaster::MAX_SESSIONS] = {};
    RemoteSocketsCache _remote_sockets_cache[StackMaster::MAX_SESSIONS] = {};
    RemoteLightsCache _remote_lights_cache[StackMaster::MAX_SESSIONS] = {};
    RemoteSepticCache _remote_septic_cache[StackMaster::MAX_SESSIONS] = {};
    RemoteThermoCache _remote_thermo_cache[StackMaster::MAX_SESSIONS] = {};
    RemoteTanksCache _remote_tanks_cache[StackMaster::MAX_SESSIONS] = {};
    RemoteSecurityCache _remote_security_cache[StackMaster::MAX_SESSIONS] = {};
    uint16_t _remote_cmd_id = 0;
    uint16_t _remote_all_cmd_id = 0;
    uint32_t _remote_all_updated_ms = 0;

    static void *allocMem_(size_t bytes)
    {
#if defined(ARDUINO_ARCH_ESP32)
        if (psramFound())
        {
            void *ptr = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (ptr)
                return ptr;
        }
#endif
        return nullptr;
    }

    template <typename T>
    static T *allocItems_(size_t count)
    {
        if (count == 0)
            return nullptr;
        void *mem = allocMem_(sizeof(T) * count);
        if (!mem)
            return nullptr;
        T *items = static_cast<T *>(mem);
        for (size_t i = 0; i < count; ++i)
            new (&items[i]) T();
        return items;
    }

    template <typename T>
    static void releaseItems_(T *items, size_t count)
    {
        if (!items)
            return;
        for (size_t i = 0; i < count; ++i)
            items[i].~T();
        free(items);
    }

    void initRemoteMeteo_()
    {
        for (auto &cache : _remote_meteo_cache)
        {
            cache.items = allocItems_<RemoteMeteoItem>(cache.capacity);
            if (!cache.items)
            {
                cache.capacity = 0;
                _logs.error(F("STACK"), F("Remote meteo cache alloc failed"));
            }
            cache.reset();
        }
    }

    void initRemoteDevices_()
    {
        for (auto &cache : _remote_sockets_cache)
        {
            cache.items = allocItems_<RemoteSocketItem>(cache.capacity);
            if (!cache.items)
            {
                cache.capacity = 0;
                _logs.error(F("STACK"), F("Remote sockets cache alloc failed"));
            }
            cache.reset();
        }
        for (auto &cache : _remote_lights_cache)
        {
            cache.items = allocItems_<RemoteLightItem>(cache.capacity);
            if (!cache.items)
            {
                cache.capacity = 0;
                _logs.error(F("STACK"), F("Remote lights cache alloc failed"));
            }
            cache.reset();
        }
        for (auto &cache : _remote_septic_cache)
        {
            cache.items = allocItems_<RemoteSepticItem>(cache.capacity);
            if (!cache.items)
            {
                cache.capacity = 0;
                _logs.error(F("STACK"), F("Remote septic cache alloc failed"));
            }
            cache.reset();
        }
        for (auto &cache : _remote_thermo_cache)
        {
            cache.items = allocItems_<RemoteThermoItem>(cache.capacity);
            if (!cache.items)
            {
                cache.capacity = 0;
                _logs.error(F("STACK"), F("Remote thermo cache alloc failed"));
            }
            cache.reset();
        }
        for (auto &cache : _remote_tanks_cache)
        {
            cache.items = allocItems_<RemoteTankItem>(cache.capacity);
            if (!cache.items)
            {
                cache.capacity = 0;
                _logs.error(F("STACK"), F("Remote tanks cache alloc failed"));
            }
            cache.reset();
        }
        for (auto &cache : _remote_security_cache)
        {
            cache.items = allocItems_<RemoteSecurityItem>(cache.capacity);
            if (!cache.items)
            {
                cache.capacity = 0;
                _logs.error(F("STACK"), F("Remote security cache alloc failed"));
            }
            cache.reset();
        }
    }

    void releaseRemoteMeteo_()
    {
        for (auto &cache : _remote_meteo_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        _alloc_ready = false;
    }

    void releaseRemoteDevices_()
    {
        for (auto &cache : _remote_sockets_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _remote_lights_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _remote_septic_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _remote_thermo_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _remote_tanks_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _remote_security_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        _alloc_ready = false;
    }

    static void onFrame_(void *ctx, const StackFrame &frame)
    {
        if (!ctx)
            return;
        static_cast<StackSlaveHandler *>(ctx)->handleFrame_(frame);
    }

    static size_t onStatus_(void *ctx, uint8_t *out, size_t cap)
    {
        if (!ctx)
            return 0;
        return static_cast<StackSlaveHandler *>(ctx)->buildStatus_(out, cap);
    }

    void handleFrame_(const StackFrame &frame)
    {
        if (!_node)
            return;
        if (frame.type == (uint8_t)StackMsgType::Ack || frame.type == (uint8_t)StackMsgType::Err)
        {
            handleRemoteMeteoReply_(frame);
            handleRemoteSocketsReply_(frame);
            handleRemoteLightsReply_(frame);
            handleRemoteSepticReply_(frame);
            handleRemoteThermoReply_(frame);
            handleRemoteTanksReply_(frame);
            handleRemoteSecurityReply_(frame);
            return;
        }
        if (frame.type != (uint8_t)StackMsgType::CmdGet && frame.type != (uint8_t)StackMsgType::CmdSet)
            return;

        _rx_doc.clear();
        DeserializationError err = deserializeJson(_rx_doc, frame.payload, frame.payload_len);
        if (err)
        {
            sendErr_(0, "json parse");
            return;
        }

        const uint16_t cmd_id = _rx_doc["cmd_id"] | 0;
        if (!authOk_())
        {
            sendErr_(cmd_id, "auth");
            return;
        }
        const uint8_t feature = (uint8_t)(_rx_doc["feature"] | 0);
        String action = _rx_doc["action"] | "";
        action.toLowerCase();
        JsonVariantConst params = _rx_doc["params"];

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
        case StackFeature::Extenders:
            handleExtenders_(cmd_id, action);
            break;
        case StackFeature::Sockets:
            handleSockets_(cmd_id, action, params);
            break;
        case StackFeature::Meteo:
            handleMeteo_(cmd_id, action);
            break;
        case StackFeature::Thermo:
            handleThermo_(cmd_id, action, params);
            break;
        case StackFeature::Septic:
            handleSeptic_(cmd_id, action, params);
            break;
        case StackFeature::Tanks:
            handleTanks_(cmd_id, action);
            break;
        case StackFeature::Ring:
            handleRing_(cmd_id, action, params);
            break;
        case StackFeature::Security:
            handleSecurity_(cmd_id, action, params);
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
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
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
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["ports"].to<JsonArray>();
            if (params.is<JsonObjectConst>() && params["ids"].is<JsonArrayConst>())
            {
                JsonArrayConst ids = params["ids"].as<JsonArrayConst>();
                for (JsonVariantConst v : ids)
                {
                    if (!v.is<unsigned>())
                        continue;
                    const uint8_t id = (uint8_t)v.as<unsigned>();
                    const bool state = _io.read(id);
                    JsonObject o = arr.add<JsonObject>();
                    fillPortItem_(o, id, state);
                }
            }
            else
            {
                for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
                {
                    const auto &p = ActiveBoardProfile::PORTS[i];
                    if (p.caps == Cap::None)
                        continue;
                    JsonObject o = arr.add<JsonObject>();
                    fillPortItem_(o, i, _io.read(i));
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
                _io.write(id, state);
            }
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleTempSensors_(uint16_t cmd_id, const String &action, JsonVariantConst)
    {
        static constexpr size_t kMaxSerials = 50;
        char serials[kMaxSerials][17] = {};
        size_t serial_count = 0;
        _ds18b20.listSerials(serials, kMaxSerials, serial_count);
        if (action == "list")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["serials"].to<JsonArray>();
            for (size_t i = 0; i < serial_count; ++i)
                arr.add(serials[i]);
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "read_all")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (size_t i = 0; i < serial_count; ++i)
            {
                const char *s = serials[i];
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
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (uint8_t i = 0; i < _last_i2c_count; ++i)
            {
                const auto &e = _last_i2c[i];
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
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (uint8_t i = 0; i < _last_ow_count; ++i)
            {
                const auto &e = _last_ow[i];
                JsonObject o = arr.add<JsonObject>();
                o["bus"] = e.bus;
                o["addr"] = e.addr;
                if (e.bus < ActiveBoardProfile::ONEWIRE_COUNT)
                    o["type"] = owBusName_(ActiveBoardProfile::ONEWIRES[e.bus].bus_id);
            }
            sendAck_(cmd_id, doc);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleExtenders_(uint16_t cmd_id, const String &action)
    {
        if (action != "get_list")
        {
            sendErr_(cmd_id, "unsupported");
            return;
        }
        _tx_doc.clear();
        JsonDocument &doc = _tx_doc;
        JsonArray arr = doc["items"].to<JsonArray>();
        const auto *devs = _ext.devs();
        if (devs)
        {
            for (uint8_t i = 0; i < _ext.devCount(); ++i)
            {
                const auto &d = devs[i];
                if (d.i2c_addr == 0 || d.type == Extender::Type::None)
                    continue;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = i;
                o["bus"] = d.bus_num;
                char addr_buf[8] = {};
                snprintf(addr_buf, sizeof(addr_buf), "0x%02X", d.i2c_addr);
                o["addr"] = addr_buf;
                o["type"] = extTypeName_(d.type);
                o["present"] = _ext.isPresent(i);
            }
        }
        sendAck_(cmd_id, doc);
    }

    void handleFan_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get_status")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
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
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
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
        _tx_doc.clear();
        JsonDocument &doc = _tx_doc;
        doc["board_temp"] = _plc.boardTemp();
        doc["cpu_temp"] = _plc.cpuTemp();
        doc["fan_on"] = _plc.fanStatus();
        doc["on_c"] = _plc.fanOnC();
        doc["hyst_c"] = _plc.fanHysteresisC();
        sendAck_(cmd_id, doc);
    }

    void handleRelays_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
            {
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != PortIO::PinType::Relay)
                    continue;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = i;
                o["state"] = _io.read(i);
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
                _io.write(id, state);
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
        _tx_doc.clear();
        JsonDocument &doc = _tx_doc;
        JsonArray arr = doc["items"].to<JsonArray>();
        for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
        {
            const auto &p = ActiveBoardProfile::PORTS[i];
            if (p.caps == Cap::None)
                continue;
            if (p.type != PortIO::PinType::DInput && p.type != PortIO::PinType::Button)
                continue;
            JsonObject o = arr.add<JsonObject>();
            o["id"] = i;
            o["state"] = _io.read(i);
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
        _tx_doc.clear();
        JsonDocument &doc = _tx_doc;
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
        _tx_doc.clear();
        JsonDocument &doc = _tx_doc;
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

    void handleSockets_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = _sockets.configByIndex(i);
                const auto *st = _sockets.stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = (unsigned)cfg->id;
                o["enabled"] = cfg->enabled;
                if (cfg->name.length())
                    o["name"] = cfg->name;
                if (cfg->button_port != SocketController::kInvalidPort)
                    o["button"] = cfg->button_port;
                if (cfg->relay_port != SocketController::kInvalidPort)
                    o["relay"] = cfg->relay_port;
                o["state"] = st->relay_on;
            }
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "get_lights")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = _sockets.lightConfigByIndex(i);
                const auto *st = _sockets.lightStateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = (unsigned)cfg->id;
                o["enabled"] = cfg->enabled;
                if (cfg->name.length())
                    o["name"] = cfg->name;
                if (cfg->button_port != SocketController::kInvalidPort)
                    o["button"] = cfg->button_port;
                if (cfg->relay_port != SocketController::kInvalidPort)
                    o["relay"] = cfg->relay_port;
                o["state"] = st->relay_on;
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
                if (!v.is<JsonObjectConst>())
                    continue;
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                if (item["toggle"].is<bool>() && item["toggle"].as<bool>())
                {
                    _sockets.toggleRelayById(id);
                    continue;
                }
                if (item["state"].is<bool>())
                {
                    const bool on = item["state"].as<bool>();
                    _sockets.setRelayById(id, on);
                }
                else if (item["state"].is<int>())
                {
                    const bool on = item["state"].as<int>() != 0;
                    _sockets.setRelayById(id, on);
                }
            }
            sendAck_(cmd_id);
            return;
        }
        if (action == "set_lights")
        {
            if (!params.is<JsonObjectConst>() || !params["items"].is<JsonArrayConst>())
            {
                sendErr_(cmd_id, "missing items");
                return;
            }
            JsonArrayConst items = params["items"].as<JsonArrayConst>();
            for (JsonVariantConst v : items)
            {
                if (!v.is<JsonObjectConst>())
                    continue;
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                if (item["toggle"].is<bool>() && item["toggle"].as<bool>())
                {
                    _sockets.toggleLightRelayById(id);
                    continue;
                }
                if (item["state"].is<bool>())
                {
                    const bool on = item["state"].as<bool>();
                    _sockets.setLightRelayById(id, on);
                }
                else if (item["state"].is<int>())
                {
                    const bool on = item["state"].as<int>() != 0;
                    _sockets.setLightRelayById(id, on);
                }
            }
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleMeteo_(uint16_t cmd_id, const String &action)
    {
        if (action != "get")
        {
            sendErr_(cmd_id, "unsupported");
            return;
        }
        _tx_doc.clear();
        JsonDocument &doc = _tx_doc;
        JsonArray arr = doc["items"].to<JsonArray>();
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = _meteo.configByIndex(i);
            const auto *st = _meteo.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = arr.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            if (cfg->name.length())
                o["name"] = cfg->name;
            o["type"] = MeteoController::typeName(cfg->type);
            if (cfg->type == MeteoController::SensorType::Dht22 &&
                cfg->dht_pin != MeteoController::kInvalidPin)
                o["pin"] = cfg->dht_pin;
            if (cfg->type == MeteoController::SensorType::Ds18b20 && cfg->ds18_addr_set)
            {
                char hex[17] = {};
                MeteoController::formatHexAddr(cfg->ds18_addr, hex);
                o["addr"] = hex;
            }
            if (st->has_temp)
                o["temp_c"] = st->temp_c;
            if (st->has_humidity)
                o["hum"] = st->humidity;
            o["has_temp"] = st->has_temp;
            o["has_hum"] = st->has_humidity;
            o["ok"] = st->ok;
        }
        sendAck_(cmd_id, doc);
    }

    void handleThermo_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = _thermo.configByIndex(i);
                const auto *st = _thermo.stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = (unsigned)cfg->id;
                o["enabled"] = cfg->enabled;
                if (cfg->name.length())
                    o["name"] = cfg->name;
                o["sensor"] = (unsigned)cfg->sensor_id;
                if (cfg->sensor_node_id != 0)
                    o["sensor_node"] = (unsigned long)cfg->sensor_node_id;
                o["mode"] = ThermoController::modeName(cfg->mode);
                o["target"] = cfg->target_c;
                o["hyst"] = cfg->hysteresis;
                if (cfg->heat_port != ThermoController::kInvalidPort)
                    o["heat"] = cfg->heat_port;
                if (cfg->cool_port != ThermoController::kInvalidPort)
                    o["cool"] = cfg->cool_port;
                if (cfg->button_port != ThermoController::kInvalidPort)
                    o["button"] = cfg->button_port;
                o["power_on"] = st->power_on;
                o["heat_on"] = st->heat_on;
                o["cool_on"] = st->cool_on;
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
                if (!v.is<JsonObjectConst>())
                    continue;
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                if (item["toggle"].is<bool>() && item["toggle"].as<bool>())
                {
                    _thermo.togglePower(id, "stack");
                    continue;
                }
                if (item["power"].is<bool>() || item["power"].is<int>())
                {
                    const bool on = item["power"].is<bool>() ? item["power"].as<bool>()
                                                             : (item["power"].as<int>() != 0);
                    _thermo.setPower(id, on, "stack");
                    continue;
                }
                if (item["state"].is<bool>() || item["state"].is<int>())
                {
                    const bool on = item["state"].is<bool>() ? item["state"].as<bool>()
                                                             : (item["state"].as<int>() != 0);
                    _thermo.setPower(id, on, "stack");
                }
            }
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleSecurity_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "status")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            doc["enabled"] = _security.controllerEnabled();
            doc["armed"] = _security.armed();
            doc["alarm"] = _security.alarmOn();
            if (_security.sirenPort() != SecurityController::kInvalidPort)
                doc["siren"] = (unsigned)_security.sirenPort();
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "get")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            doc["enabled"] = _security.controllerEnabled();
            doc["armed"] = _security.armed();
            doc["alarm"] = _security.alarmOn();
            JsonArray arr = doc["items"].to<JsonArray>();
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                const auto *cfg = _security.configByIndex(i);
                const auto *st = _security.stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = (unsigned)cfg->id;
                o["enabled"] = cfg->enabled;
                o["type"] = (cfg->type == SecurityController::SensorType::Reed) ? "reed" : "pir";
                if (cfg->port != SecurityController::kInvalidPort)
                    o["port"] = cfg->port;
                o["silent"] = cfg->silent;
                if (cfg->name.length())
                    o["name"] = cfg->name;
                o["detect"] = st->is_detect;
            }
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "prearm")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            _security.fillPrearmItems(arr);
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "set")
        {
            if (!params.is<JsonObjectConst>())
            {
                sendErr_(cmd_id, "missing params");
                return;
            }
            JsonObjectConst obj = params.as<JsonObjectConst>();
            String user = obj["user"] | "";
            const bool force = obj["force"] | false;
            const String beep = obj["beep"] | "";
            if (obj["enabled"].is<bool>())
                _security.setControllerEnabled(obj["enabled"].as<bool>());
            if (obj["armed"].is<bool>())
            {
                const bool on = obj["armed"].as<bool>();
                if (on && !_security.controllerEnabled())
                    _security.setControllerEnabled(true);
                if (on)
                {
                    if (force)
                        _security.armForcedFrom("stack", user);
                    else
                        _security.armFrom("stack", user);
                }
                else
                    _security.disarmFrom("stack", user);
            }
            if (obj["alarm"].is<bool>() || obj["alarm"].is<int>())
            {
                const bool on = obj["alarm"].is<bool>() ? obj["alarm"].as<bool>()
                                                        : (obj["alarm"].as<int>() != 0);
                _security.setAlarmState(on);
            }
            if (obj["clear"].is<bool>() && obj["clear"].as<bool>())
                _security.clearDetect();
            if (isSlave_() && beep.length())
            {
                if (beep == "arm")
                    beepArm_();
                else if (beep == "disarm")
                    beepDisarm_();
                else
                    beepReject_();
            }
            if (isSlave_())
                updateRfidLeds_(_security.armed());
            sendAck_(cmd_id);
            return;
        }
        if (action == "rfid_result")
        {
            if (!params.is<JsonObjectConst>())
            {
                sendErr_(cmd_id, "missing params");
                return;
            }
            JsonObjectConst obj = params.as<JsonObjectConst>();
            const bool match = obj["match"].is<bool>() ? obj["match"].as<bool>()
                                                       : (obj["match"].as<int>() != 0);
            const String uid = obj["uid"] | "";
            const String result = obj["result"] | "";
            const bool armed = obj["armed"].is<bool>() ? obj["armed"].as<bool>()
                                                       : (obj["armed"].as<int>() != 0);
            if (match)
                _logs.info(F("SECURITY"), F("RFID match: %s"), uid.length() ? uid.c_str() : "-");
            else
                _logs.warn(F("SECURITY"), F("RFID not match: %s"), uid.length() ? uid.c_str() : "-");
            if (isSlave_())
            {
                if (result == "arm")
                    beepArm_();
                else if (result == "disarm")
                    beepDisarm_();
                else
                    beepReject_();
                updateRfidLeds_(armed);
                requestSecurityStatus_();
            }
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleSeptic_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "status")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            doc["enabled"] = _septic.controllerEnabled();
            const auto *cfg = _septic.configByIndex(0);
            const auto *st = _septic.stateByIndex(0);
            if (cfg && st)
            {
                doc["monitor"] = cfg->monitoring_on;
                doc["warning"] = st->warning;
                doc["alarm"] = st->alarm;
                if (cfg->warning_port != SepticController::kInvalidPort)
                    doc["warning_port"] = cfg->warning_port;
                if (cfg->alarm_port != SepticController::kInvalidPort)
                    doc["alarm_port"] = cfg->alarm_port;
                if (cfg->relay_warning != SepticController::kInvalidPort)
                    doc["relay_warning"] = cfg->relay_warning;
                if (cfg->relay_alarm != SepticController::kInvalidPort)
                    doc["relay_alarm"] = cfg->relay_alarm;
            }
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "get")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = _septic.configByIndex(i);
                const auto *st = _septic.stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = (unsigned)cfg->id;
                o["enabled"] = cfg->enabled;
                o["monitor"] = cfg->monitoring_on;
                if (cfg->warning_port != SepticController::kInvalidPort)
                    o["warning_port"] = cfg->warning_port;
                if (cfg->alarm_port != SepticController::kInvalidPort)
                    o["alarm_port"] = cfg->alarm_port;
                if (cfg->relay_warning != SepticController::kInvalidPort)
                    o["relay_warning"] = cfg->relay_warning;
                if (cfg->relay_alarm != SepticController::kInvalidPort)
                    o["relay_alarm"] = cfg->relay_alarm;
                o["warning"] = st->warning;
                o["alarm"] = st->alarm;
            }
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "set")
        {
            if (!params.is<JsonObjectConst>())
            {
                sendErr_(cmd_id, "missing params");
                return;
            }
            JsonObjectConst obj = params.as<JsonObjectConst>();
            uint8_t id = 1;
            if (obj["id"].is<unsigned>())
            {
                const unsigned raw = obj["id"].as<unsigned>();
                if (raw > 0 && raw <= 0xFFu)
                    id = (uint8_t)raw;
            }
            if (obj["monitor"].is<bool>() || obj["monitor"].is<int>())
            {
                const bool on = obj["monitor"].is<bool>() ? obj["monitor"].as<bool>()
                                                          : (obj["monitor"].as<int>() != 0);
                if (!_septic.setMonitoring(id, on))
                {
                    sendErr_(cmd_id, "invalid id");
                    return;
                }
                sendAck_(cmd_id);
                return;
            }
            sendErr_(cmd_id, "missing monitor");
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleTanks_(uint16_t cmd_id, const String &action)
    {
        if (action != "get")
        {
            sendErr_(cmd_id, "unsupported");
            return;
        }
        _tx_doc.clear();
        JsonDocument &doc = _tx_doc;
        JsonArray arr = doc["items"].to<JsonArray>();
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = _tanks.configByIndex(i);
            const auto *st = _tanks.stateByIndex(i);
            if (!cfg || !st || !cfg->enabled)
                continue;
            JsonObject o = arr.add<JsonObject>();
            o["id"] = (unsigned)cfg->id;
            o["enabled"] = cfg->enabled;
            o["power_on"] = cfg->power_on;
            if (cfg->name.length())
                o["name"] = cfg->name;
            if (cfg->level_low != TankController::kInvalidPort)
                o["low"] = cfg->level_low;
            if (cfg->level_mid != TankController::kInvalidPort)
                o["mid"] = cfg->level_mid;
            if (cfg->level_full != TankController::kInvalidPort)
                o["full"] = cfg->level_full;
            if (cfg->relay_valve != TankController::kInvalidPort)
                o["valve"] = cfg->relay_valve;
            if (cfg->relay_pump != TankController::kInvalidPort)
                o["pump"] = cfg->relay_pump;
            if (cfg->relay_alarm != TankController::kInvalidPort)
                o["alarm"] = cfg->relay_alarm;

            o["level_low"] = st->level_low;
            o["level_mid"] = st->level_mid;
            o["level_full"] = st->level_full;
            o["levels_ok"] = st->levels_ok;
            o["valve_on"] = st->valve_on;
            o["pump_on"] = st->pump_on;
            o["alarm_on"] = st->alarm_on;
        }
        sendAck_(cmd_id, doc);
    }

    void handleRing_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            const auto &cfg = _ring.config();
            const auto &st = _ring.state();
            doc["enabled"] = cfg.enabled;
            if (cfg.button_port != RingController::kInvalidPort)
                doc["button"] = cfg.button_port;
            if (cfg.relay_port != RingController::kInvalidPort)
                doc["relay"] = cfg.relay_port;
            doc["relay_on"] = st.relay_on;
            sendAck_(cmd_id, doc);
            return;
        }
        if (action == "set")
        {
            if (!params.is<JsonObjectConst>())
            {
                sendErr_(cmd_id, "missing params");
                return;
            }
            JsonObjectConst obj = params.as<JsonObjectConst>();
            if (obj["state"].is<bool>() || obj["state"].is<int>())
            {
                const bool on = obj["state"].is<bool>() ? obj["state"].as<bool>()
                                                        : (obj["state"].as<int>() != 0);
                if (!_ring.setHoldRelayWithSource(on, RingController::Source::Stack))
                {
                    sendErr_(cmd_id, "failed");
                    return;
                }
                sendAck_(cmd_id);
                return;
            }
            sendErr_(cmd_id, "missing state");
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    size_t buildStatus_(uint8_t *out, size_t cap)
    {
        StaticJsonDocument<256> doc;
        doc["uptime_ms"] = (uint32_t)millis();
        doc["board_temp"] = _plc.boardTemp();
        doc["cpu_temp"] = _plc.cpuTemp();
        doc["fan_on"] = _plc.fanStatus();
        return serializeJson(doc, reinterpret_cast<char *>(out), cap);
    }

    void scanI2c_()
    {
        _last_i2c_count = 0;
        bool scanned[3] = {false, false, false};
        bool present[127] = {};
        for (uint8_t i = 0; i < ActiveBoardProfile::I2C_COUNT; ++i)
        {
            const uint8_t bus = ActiveBoardProfile::I2CS[i].bus_num;
            if (bus < 3 && scanned[bus])
                continue;
            if (bus < 3)
                scanned[bus] = true;
            if (!_i2c.scanDevices(bus, present))
                continue;
            for (uint8_t addr = 1; addr < 127; ++addr)
            {
                if (!present[addr])
                    continue;
                if (_last_i2c_count >= MAX_I2C_ADDRS)
                    return;
                _last_i2c[_last_i2c_count++] = {bus, addr};
            }
        }
    }

    void scanOw_()
    {
        _last_ow_count = 0;
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
                if (_last_ow_count >= MAX_OW_ADDRS)
                    return;
                OwEntry e{};
                e.bus = i;
                addrToHex_(addr, e.addr);
                _last_ow[_last_ow_count++] = e;
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

    void fillPortItem_(JsonObject o, uint8_t id, bool state) const
    {
        o["id"] = id;
        o["state"] = state;
        if (id >= IoStack::PORT_COUNT)
            return;
        const auto &p = ActiveBoardProfile::PORTS[id];
        if (p.caps == Cap::None)
            return;

        o["backend"] = (p.backend == PortIO::Backend::Extender) ? "Extender" : "Esp32";
        o["loc"] = stackUnitName_(toStackUnit_(p.location));
        o["type"] = portTypeName_(p.type);
        o["ctrl"] = p.allow_control;
        if (p.backend == PortIO::Backend::Extender)
        {
            o["dev"] = p.u.ext.dev;
            o["pin"] = p.u.ext.pin;
            o["hw"] = extDevTypeName_(p.u.ext.dev);
        }
        else
        {
            o["dev"] = -1;
            o["pin"] = p.u.esp.gpio;
            o["hw"] = "CPU";
        }
    }

    static const char *portTypeName_(PortIO::PinType t)
    {
        switch (t)
        {
        case PortIO::PinType::System:
            return "System";
        case PortIO::PinType::Relay:
            return "Relay";
        case PortIO::PinType::Led:
            return "Led";
        case PortIO::PinType::Sensor:
            return "Sensor";
        case PortIO::PinType::Button:
            return "Button";
        case PortIO::PinType::DInput:
            return "DInput";
        case PortIO::PinType::Buzzer:
            return "Buzzer";
        case PortIO::PinType::Fan:
            return "Fan";
        default:
            return "Unknown";
        }
    }

    static StackUnit toStackUnit_(PortIO::Location loc)
    {
        switch (loc)
        {
        case PortIO::Location::Cpu:
            return StackUnit::Cpu;
        case PortIO::Location::Ext1:
            return StackUnit::Unit1;
        case PortIO::Location::Ext2:
            return StackUnit::Unit2;
        case PortIO::Location::Ext3:
            return StackUnit::Unit3;
        case PortIO::Location::Ext4:
            return StackUnit::Unit4;
        case PortIO::Location::Ext5:
            return StackUnit::Unit5;
        case PortIO::Location::Ext6:
            return StackUnit::Unit6;
        case PortIO::Location::Ext7:
            return StackUnit::Unit7;
        case PortIO::Location::Ext8:
            return StackUnit::Unit8;
        case PortIO::Location::Ext9:
            return StackUnit::Unit9;
        case PortIO::Location::Ext10:
            return StackUnit::Unit10;
        default:
            return StackUnit::Unknown;
        }
    }

    static const char *stackUnitName_(StackUnit unit)
    {
        switch (unit)
        {
        case StackUnit::Cpu:
            return "CPU";
        case StackUnit::Unit1:
            return "UNIT_1";
        case StackUnit::Unit2:
            return "UNIT_2";
        case StackUnit::Unit3:
            return "UNIT_3";
        case StackUnit::Unit4:
            return "UNIT_4";
        case StackUnit::Unit5:
            return "UNIT_5";
        case StackUnit::Unit6:
            return "UNIT_6";
        case StackUnit::Unit7:
            return "UNIT_7";
        case StackUnit::Unit8:
            return "UNIT_8";
        case StackUnit::Unit9:
            return "UNIT_9";
        case StackUnit::Unit10:
            return "UNIT_10";
        default:
            return "UNKNOWN";
        }
    }

    static const char *extDevTypeName_(uint8_t dev)
    {
        if (dev >= ActiveBoardProfile::EXT_DEVS_COUNT)
            return "Unknown";
        switch (ActiveBoardProfile::EXT_DEVS[dev].type)
        {
        case Extender::Type::MCP23017:
            return "MCP23017";
        case Extender::Type::PCF8574:
            return "PCF8574";
        default:
            return "None";
        }
    }

    static const char *extTypeName_(Extender::Type t)
    {
        switch (t)
        {
        case Extender::Type::MCP23017:
            return "MCP23017";
        case Extender::Type::PCF8574:
            return "PCF8574";
        default:
            return "None";
        }
    }

    static const char *owBusName_(OneWireCfg::OwType t)
    {
        switch (t)
        {
        case OneWireCfg::OwType::iButton:
            return "iButton";
        case OneWireCfg::OwType::Temp:
            return "Temp";
        default:
            return "Unknown";
        }
    }

    bool authOk_() const
    {
        if (!_configs)
            return true;
        const String key = _configs->stackApiKey();
        if (key.length() == 0)
            return true;
        const String provided = _rx_doc["api_key"] | "";
        return provided == key;
    }

    ConfigsManagerIface::StackRole stackRole_() const
    {
        if (!_configs)
            return ConfigsManagerIface::StackRole::Master;
        return _configs->stackRole();
    }

    void updateRemoteMeteo_()
    {
        if (!_node || !_configs)
            return;
        if (stackRole_() != ConfigsManagerIface::StackRole::Slave)
            return;
        uint32_t nodes[ThermoController::kDeviceCount] = {};
        size_t node_count = 0;
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = _thermo.configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            if (cfg->sensor_id == ThermoController::kInvalidSensor || cfg->sensor_node_id == 0)
                continue;
            bool known = false;
            for (size_t j = 0; j < node_count; ++j)
            {
                if (nodes[j] == cfg->sensor_node_id)
                {
                    known = true;
                    break;
                }
            }
            if (!known && node_count < ThermoController::kDeviceCount)
                nodes[node_count++] = cfg->sensor_node_id;
        }
        for (size_t i = 0; i < node_count; ++i)
            requestRemoteMeteo_(nodes[i]);
    }

    bool requestRemoteMeteo_(uint32_t node_id)
    {
        if (!_node)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Slave)
            return false;
        RemoteMeteoCache *cache = findRemoteMeteoCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if ((uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextRemoteCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Meteo;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["node"] = node_id;
        if (_configs)
        {
            const String key = _configs->stackApiKey();
            if (key.length())
                doc["api_key"] = key;
        }
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0)
            return false;
        if (!_node->send((uint8_t)StackMsgType::CmdGet, _tx_payload_buf, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestRemoteMeteoAll_()
    {
        if (!_node)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Slave)
            return false;
        const uint32_t now = millis();
        if (_remote_all_cmd_id != 0)
            return false;
        if (_remote_all_updated_ms && (uint32_t)(now - _remote_all_updated_ms) < 2000u)
            return false;
        const uint16_t cmd_id = nextRemoteCmdId_();
        _remote_all_cmd_id = cmd_id;
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Meteo;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["all"] = true;
        if (_configs)
        {
            const String key = _configs->stackApiKey();
            if (key.length())
                doc["api_key"] = key;
        }
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0)
        {
            _remote_all_cmd_id = 0;
            return false;
        }
        if (!_node->send((uint8_t)StackMsgType::CmdGet, _tx_payload_buf, len))
        {
            _remote_all_cmd_id = 0;
            return false;
        }
        return true;
    }

    bool requestRemoteSockets_(uint32_t node_id)
    {
        if (!_node)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Slave)
            return false;
        RemoteSocketsCache *cache = findRemoteSocketsCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if ((uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextRemoteCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["node"] = node_id;
        if (_configs)
        {
            const String key = _configs->stackApiKey();
            if (key.length())
                doc["api_key"] = key;
        }
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0)
            return false;
        if (!_node->send((uint8_t)StackMsgType::CmdGet, _tx_payload_buf, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestRemoteLights_(uint32_t node_id)
    {
        if (!_node)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Slave)
            return false;
        RemoteLightsCache *cache = findRemoteLightsCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if ((uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextRemoteCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "get_lights";
        JsonObject params = doc["params"].to<JsonObject>();
        params["node"] = node_id;
        if (_configs)
        {
            const String key = _configs->stackApiKey();
            if (key.length())
                doc["api_key"] = key;
        }
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0)
            return false;
        if (!_node->send((uint8_t)StackMsgType::CmdGet, _tx_payload_buf, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestRemoteSeptic_(uint32_t node_id)
    {
        if (!_node)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Slave)
            return false;
        RemoteSepticCache *cache = findRemoteSepticCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if ((uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextRemoteCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Septic;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["node"] = node_id;
        if (_configs)
        {
            const String key = _configs->stackApiKey();
            if (key.length())
                doc["api_key"] = key;
        }
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0)
            return false;
        if (!_node->send((uint8_t)StackMsgType::CmdGet, _tx_payload_buf, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestRemoteThermo_(uint32_t node_id)
    {
        if (!_node)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Slave)
            return false;
        RemoteThermoCache *cache = findRemoteThermoCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if ((uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextRemoteCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Thermo;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["node"] = node_id;
        if (_configs)
        {
            const String key = _configs->stackApiKey();
            if (key.length())
                doc["api_key"] = key;
        }
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0)
            return false;
        if (!_node->send((uint8_t)StackMsgType::CmdGet, _tx_payload_buf, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestRemoteTanks_(uint32_t node_id)
    {
        if (!_node)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Slave)
            return false;
        RemoteTanksCache *cache = findRemoteTanksCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if ((uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextRemoteCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Tanks;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["node"] = node_id;
        if (_configs)
        {
            const String key = _configs->stackApiKey();
            if (key.length())
                doc["api_key"] = key;
        }
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0)
            return false;
        if (!_node->send((uint8_t)StackMsgType::CmdGet, _tx_payload_buf, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestRemoteSecurity_(uint32_t node_id)
    {
        if (!_node)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Slave)
            return false;
        RemoteSecurityCache *cache = findRemoteSecurityCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if ((uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextRemoteCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["node"] = node_id;
        if (_configs)
        {
            const String key = _configs->stackApiKey();
            if (key.length())
                doc["api_key"] = key;
        }
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0)
            return false;
        if (!_node->send((uint8_t)StackMsgType::CmdGet, _tx_payload_buf, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    RemoteMeteoCache *findRemoteMeteoCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        for (auto &c : _remote_meteo_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _remote_meteo_cache)
        {
            if (c.node_id == 0)
            {
                c.reset();
                c.node_id = node_id;
                return &c;
            }
        }
        return nullptr;
    }

    const RemoteMeteoCache *findRemoteMeteoCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackSlaveHandler *>(this)->findRemoteMeteoCache_(node_id, create);
    }

    RemoteMeteoCache *findRemoteMeteoCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _remote_meteo_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    RemoteSocketsCache *findRemoteSocketsCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        for (auto &c : _remote_sockets_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _remote_sockets_cache)
        {
            if (c.node_id == 0)
            {
                c.reset();
                c.node_id = node_id;
                return &c;
            }
        }
        return nullptr;
    }

    const RemoteSocketsCache *findRemoteSocketsCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackSlaveHandler *>(this)->findRemoteSocketsCache_(node_id, create);
    }

    RemoteLightsCache *findRemoteLightsCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        for (auto &c : _remote_lights_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _remote_lights_cache)
        {
            if (c.node_id == 0)
            {
                c.reset();
                c.node_id = node_id;
                return &c;
            }
        }
        return nullptr;
    }

    const RemoteLightsCache *findRemoteLightsCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackSlaveHandler *>(this)->findRemoteLightsCache_(node_id, create);
    }

    RemoteSepticCache *findRemoteSepticCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        for (auto &c : _remote_septic_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _remote_septic_cache)
        {
            if (c.node_id == 0)
            {
                c.reset();
                c.node_id = node_id;
                return &c;
            }
        }
        return nullptr;
    }

    const RemoteSepticCache *findRemoteSepticCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackSlaveHandler *>(this)->findRemoteSepticCache_(node_id, create);
    }

    RemoteTanksCache *findRemoteTanksCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        for (auto &c : _remote_tanks_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _remote_tanks_cache)
        {
            if (c.node_id == 0)
            {
                c.reset();
                c.node_id = node_id;
                return &c;
            }
        }
        return nullptr;
    }

    RemoteThermoCache *findRemoteThermoCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        for (auto &c : _remote_thermo_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _remote_thermo_cache)
        {
            if (c.node_id != 0)
                continue;
            c.reset();
            c.node_id = node_id;
            return &c;
        }
        return nullptr;
    }

    const RemoteThermoCache *findRemoteThermoCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackSlaveHandler *>(this)->findRemoteThermoCache_(node_id, create);
    }

    const RemoteTanksCache *findRemoteTanksCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackSlaveHandler *>(this)->findRemoteTanksCache_(node_id, create);
    }

    RemoteSecurityCache *findRemoteSecurityCache_(uint32_t node_id, bool create)
    {
        if (node_id == 0)
            return nullptr;
        for (auto &c : _remote_security_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _remote_security_cache)
        {
            if (c.node_id != 0)
                continue;
            c.reset();
            c.node_id = node_id;
            return &c;
        }
        return nullptr;
    }

    const RemoteSecurityCache *findRemoteSecurityCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackSlaveHandler *>(this)->findRemoteSecurityCache_(node_id, create);
    }

    RemoteSocketsCache *findRemoteSocketsCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _remote_sockets_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    RemoteLightsCache *findRemoteLightsCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _remote_lights_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    RemoteSepticCache *findRemoteSepticCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _remote_septic_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    RemoteTanksCache *findRemoteTanksCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _remote_tanks_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    RemoteThermoCache *findRemoteThermoCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _remote_thermo_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    RemoteSecurityCache *findRemoteSecurityCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _remote_security_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    void handleRemoteMeteoReply_(const StackFrame &frame)
    {
        _rx_doc.clear();
        DeserializationError err = deserializeJson(_rx_doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint16_t cmd_id = _rx_doc["cmd_id"] | 0;
        if (_remote_all_cmd_id != 0 && cmd_id == _remote_all_cmd_id)
        {
            _remote_all_cmd_id = 0;
            _remote_all_updated_ms = millis();
            const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (_rx_doc["ok"] | false);
            if (!ok)
                return;
            JsonArrayConst nodes = _rx_doc["data"]["nodes"].as<JsonArrayConst>();
            if (nodes.isNull())
                return;
            for (JsonObjectConst node : nodes)
            {
                if (!node["node_id"].is<unsigned>())
                    continue;
                const uint32_t node_id = node["node_id"].as<unsigned>();
                RemoteMeteoCache *cache = findRemoteMeteoCache_(node_id, true);
                if (!cache || !cache->items)
                    continue;
                cache->pending = false;
                cache->updated_ms = millis();
                cache->last_ok = true;
                cache->last_error = "";
                cache->node_name = node["node_name"] | "";
                JsonArrayConst items = node["items"].as<JsonArrayConst>();
                if (items.isNull())
                    continue;
                cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (cache->item_count >= MeteoController::kSensorCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    RemoteMeteoItem &dst = cache->items[cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.ok = item["ok"] | false;
                    dst.has_temp = item["has_temp"] | false;
                    dst.has_hum = item["has_hum"] | false;
                    dst.temp_c = item["temp_c"] | 0.0f;
                    dst.hum = item["hum"] | 0.0f;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                    copyStr_(dst.addr, sizeof(dst.addr), item["addr"].as<const char *>());
                    dst.pin = item["pin"] | -1;
                }
                cache->has_data = true;
            }
            return;
        }
        RemoteMeteoCache *cache = findRemoteMeteoCacheByCmd_(cmd_id);
        if (!cache)
            return;
        if (!cache->items)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (_rx_doc["ok"] | false);
        JsonArrayConst items = _rx_doc["data"]["items"].as<JsonArrayConst>();
        cache->pending = false;
        cache->updated_ms = millis();
        cache->last_ok = false;
        cache->last_error = "";
        if (!ok)
        {
            cache->last_error = _rx_doc["error"] | "error";
            return;
        }
        cache->node_name = _rx_doc["data"]["node_name"] | "";
        if (items.isNull())
            return;
        cache->item_count = 0;
        for (JsonObjectConst item : items)
        {
            if (cache->item_count >= MeteoController::kSensorCount)
                break;
            if (!item["id"].is<unsigned>())
                continue;
            RemoteMeteoItem &dst = cache->items[cache->item_count++];
            dst.id = (uint8_t)item["id"].as<unsigned>();
            dst.enabled = item["enabled"] | false;
            dst.ok = item["ok"] | false;
            dst.has_temp = item["has_temp"] | false;
            dst.has_hum = item["has_hum"] | false;
            dst.temp_c = item["temp_c"] | 0.0f;
            dst.hum = item["hum"] | 0.0f;
            copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
            copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
            copyStr_(dst.addr, sizeof(dst.addr), item["addr"].as<const char *>());
            dst.pin = item["pin"] | -1;
        }
        cache->has_data = true;
        cache->last_ok = true;
    }

    void handleRemoteSocketsReply_(const StackFrame &frame)
    {
        _rx_doc.clear();
        DeserializationError err = deserializeJson(_rx_doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint16_t cmd_id = _rx_doc["cmd_id"] | 0;
        RemoteSocketsCache *cache = findRemoteSocketsCacheByCmd_(cmd_id);
        if (!cache || !cache->items)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (_rx_doc["ok"] | false);
        JsonArrayConst items = _rx_doc["data"]["items"].as<JsonArrayConst>();
        cache->pending = false;
        cache->updated_ms = millis();
        cache->last_ok = false;
        cache->last_error = "";
        if (!ok)
        {
            cache->last_error = _rx_doc["error"] | "error";
            return;
        }
        if (items.isNull())
            return;
        cache->item_count = 0;
        for (JsonObjectConst item : items)
        {
            if (cache->item_count >= SocketController::kSocketCount)
                break;
            if (!item["id"].is<unsigned>())
                continue;
            RemoteSocketItem &dst = cache->items[cache->item_count++];
            dst.id = (uint8_t)item["id"].as<unsigned>();
            dst.enabled = item["enabled"] | false;
            dst.state = item["state"] | false;
            copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
        }
        cache->has_data = true;
        cache->last_ok = true;
    }

    void handleRemoteLightsReply_(const StackFrame &frame)
    {
        _rx_doc.clear();
        DeserializationError err = deserializeJson(_rx_doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint16_t cmd_id = _rx_doc["cmd_id"] | 0;
        RemoteLightsCache *cache = findRemoteLightsCacheByCmd_(cmd_id);
        if (!cache || !cache->items)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (_rx_doc["ok"] | false);
        JsonArrayConst items = _rx_doc["data"]["items"].as<JsonArrayConst>();
        cache->pending = false;
        cache->updated_ms = millis();
        cache->last_ok = false;
        cache->last_error = "";
        if (!ok)
        {
            cache->last_error = _rx_doc["error"] | "error";
            return;
        }
        if (items.isNull())
            return;
        cache->item_count = 0;
        for (JsonObjectConst item : items)
        {
            if (cache->item_count >= SocketController::kLightCount)
                break;
            if (!item["id"].is<unsigned>())
                continue;
            RemoteLightItem &dst = cache->items[cache->item_count++];
            dst.id = (uint8_t)item["id"].as<unsigned>();
            dst.enabled = item["enabled"] | false;
            dst.state = item["state"] | false;
            copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
        }
        cache->has_data = true;
        cache->last_ok = true;
    }

    void handleRemoteSepticReply_(const StackFrame &frame)
    {
        _rx_doc.clear();
        DeserializationError err = deserializeJson(_rx_doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint16_t cmd_id = _rx_doc["cmd_id"] | 0;
        RemoteSepticCache *cache = findRemoteSepticCacheByCmd_(cmd_id);
        if (!cache || !cache->items)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (_rx_doc["ok"] | false);
        JsonArrayConst items = _rx_doc["data"]["items"].as<JsonArrayConst>();
        cache->pending = false;
        cache->updated_ms = millis();
        cache->last_ok = false;
        cache->last_error = "";
        if (!ok)
        {
            cache->last_error = _rx_doc["error"] | "error";
            return;
        }
        if (items.isNull())
            return;
        cache->item_count = 0;
        for (JsonObjectConst item : items)
        {
            if (cache->item_count >= SepticController::kSepticCount)
                break;
            if (!item["id"].is<unsigned>())
                continue;
            RemoteSepticItem &dst = cache->items[cache->item_count++];
            dst.id = (uint8_t)item["id"].as<unsigned>();
            dst.enabled = item["enabled"] | false;
            dst.warning = item["warning"] | false;
            dst.alarm = item["alarm"] | false;
        }
        cache->has_data = true;
        cache->last_ok = true;
    }

    void handleRemoteThermoReply_(const StackFrame &frame)
    {
        _rx_doc.clear();
        DeserializationError err = deserializeJson(_rx_doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint16_t cmd_id = _rx_doc["cmd_id"] | 0;
        RemoteThermoCache *cache = findRemoteThermoCacheByCmd_(cmd_id);
        if (!cache || !cache->items)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (_rx_doc["ok"] | false);
        JsonArrayConst items = _rx_doc["data"]["items"].as<JsonArrayConst>();
        cache->pending = false;
        cache->updated_ms = millis();
        cache->last_ok = false;
        cache->last_error = "";
        if (!ok)
        {
            cache->last_error = _rx_doc["error"] | "error";
            return;
        }
        if (items.isNull())
            return;
        cache->item_count = 0;
        for (JsonObjectConst item : items)
        {
            if (cache->item_count >= ThermoController::kDeviceCount)
                break;
            if (!item["id"].is<unsigned>())
                continue;
            RemoteThermoItem &dst = cache->items[cache->item_count++];
            dst.id = (uint8_t)item["id"].as<unsigned>();
            dst.enabled = item["enabled"] | false;
            dst.power_on = item["power_on"] | false;
            dst.heat_on = item["heat_on"] | false;
            dst.cool_on = item["cool_on"] | false;
            copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
        }
        cache->has_data = true;
        cache->last_ok = true;
    }

    void handleRemoteTanksReply_(const StackFrame &frame)
    {
        _rx_doc.clear();
        DeserializationError err = deserializeJson(_rx_doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint16_t cmd_id = _rx_doc["cmd_id"] | 0;
        RemoteTanksCache *cache = findRemoteTanksCacheByCmd_(cmd_id);
        if (!cache || !cache->items)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (_rx_doc["ok"] | false);
        JsonArrayConst items = _rx_doc["data"]["items"].as<JsonArrayConst>();
        cache->pending = false;
        cache->updated_ms = millis();
        cache->last_ok = false;
        cache->last_error = "";
        if (!ok)
        {
            cache->last_error = _rx_doc["error"] | "error";
            return;
        }
        if (items.isNull())
            return;
        cache->item_count = 0;
        for (JsonObjectConst item : items)
        {
            if (cache->item_count >= TankController::kTankCount)
                break;
            if (!item["id"].is<unsigned>())
                continue;
            RemoteTankItem &dst = cache->items[cache->item_count++];
            dst.id = (uint8_t)item["id"].as<unsigned>();
            dst.enabled = item["enabled"] | false;
            dst.levels_ok = item["levels_ok"] | false;
            dst.level_low = item["level_low"] | false;
            dst.level_mid = item["level_mid"] | false;
            dst.level_full = item["level_full"] | false;
        }
        cache->has_data = true;
        cache->last_ok = true;
    }

    void handleRemoteSecurityReply_(const StackFrame &frame)
    {
        _rx_doc.clear();
        DeserializationError err = deserializeJson(_rx_doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint16_t cmd_id = _rx_doc["cmd_id"] | 0;
        RemoteSecurityCache *cache = findRemoteSecurityCacheByCmd_(cmd_id);
        if (!cache || !cache->items)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (_rx_doc["ok"] | false);
        JsonObjectConst data = _rx_doc["data"];
        JsonArrayConst items = data["items"].as<JsonArrayConst>();
        cache->pending = false;
        cache->updated_ms = millis();
        cache->last_ok = false;
        cache->last_error = "";
        if (!ok)
        {
            cache->last_error = _rx_doc["error"] | "error";
            return;
        }
        cache->enabled = data["enabled"] | false;
        cache->armed = data["armed"] | false;
        cache->alarm = data["alarm"] | false;
        if (!items.isNull())
        {
            cache->item_count = 0;
            for (JsonObjectConst item : items)
            {
                if (cache->item_count >= SecurityController::kSensorCount)
                    break;
                if (!item["id"].is<unsigned>())
                    continue;
                RemoteSecurityItem &dst = cache->items[cache->item_count++];
                dst.id = (uint8_t)item["id"].as<unsigned>();
                dst.enabled = item["enabled"] | false;
                dst.detect = item["detect"] | false;
                dst.silent = item["silent"] | false;
                dst.port = (uint8_t)(item["port"] | SecurityController::kInvalidPort);
                copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
            }
        }
        cache->has_data = true;
        cache->last_ok = true;
    }

    uint16_t nextRemoteCmdId_()
    {
        ++_remote_cmd_id;
        if (_remote_cmd_id == 0)
            _remote_cmd_id = 1;
        return _remote_cmd_id;
    }

    void sendAck_(uint16_t cmd_id)
    {
        _msg_doc.clear();
        _msg_doc["cmd_id"] = cmd_id;
        _msg_doc["ok"] = true;
        sendJson_((uint8_t)StackMsgType::Ack, _msg_doc);
    }

    void sendAck_(uint16_t cmd_id, const JsonDocument &data)
    {
        _msg_doc.clear();
        _msg_doc["cmd_id"] = cmd_id;
        _msg_doc["ok"] = true;
        if (!data.isNull())
            _msg_doc["data"] = data.as<JsonVariantConst>();
        sendJson_((uint8_t)StackMsgType::Ack, _msg_doc);
    }

    void sendErr_(uint16_t cmd_id, const char *msg)
    {
        _msg_doc.clear();
        _msg_doc["cmd_id"] = cmd_id;
        _msg_doc["ok"] = false;
        _msg_doc["error"] = msg ? msg : "error";
        sendJson_((uint8_t)StackMsgType::Err, _msg_doc);
    }

    void sendJson_(uint8_t type, JsonDocument &doc)
    {
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0 || len > sizeof(_tx_payload_buf))
            return;
        _node->send(type, _tx_payload_buf, len);
    }

    bool isSlave_() const
    {
        if (!_configs)
            return false;
        return _configs->stackRole() == ConfigsManagerIface::StackRole::Slave;
    }

    void ensureRfidIo_()
    {
        if (_rfid_io_ready)
            return;
        _io.pinMode(ActiveBoardProfile::STATUS_PIN, PortIO::PortMode::Output);
        _io.pinMode(ActiveBoardProfile::BUZZER_PIN, PortIO::PortMode::Output);
        _rfid_io_ready = true;
    }

    void updateRfidLeds_(bool armed)
    {
        ensureRfidIo_();
        _io.write(ActiveBoardProfile::STATUS_PIN, armed);
    }

    void buzzerOn_(bool on)
    {
        ensureRfidIo_();
        _io.write(ActiveBoardProfile::BUZZER_PIN, on);
    }

    void startBeep_(uint8_t count, uint16_t on_ms, uint16_t off_ms)
    {
        if (count == 0)
            return;
        _beep_remaining = count;
        _beep_on_ms = on_ms;
        _beep_off_ms = off_ms;
        _beep_state_on = true;
        _beep_next_ms = millis() + on_ms;
        _beep_active = true;
        buzzerOn_(true);
    }

    void updateBuzzer_()
    {
        if (!_beep_active || _beep_remaining == 0)
            return;
        const uint32_t now = millis();
        if ((int32_t)(now - _beep_next_ms) < 0)
            return;
        if (_beep_state_on)
        {
            _beep_state_on = false;
            buzzerOn_(false);
            if (_beep_off_ms == 0)
            {
                if (_beep_remaining > 0)
                    --_beep_remaining;
                if (_beep_remaining == 0)
                {
                    _beep_active = false;
                    return;
                }
                _beep_state_on = true;
                buzzerOn_(true);
                _beep_next_ms = now + _beep_on_ms;
                return;
            }
            _beep_next_ms = now + _beep_off_ms;
            return;
        }
        if (_beep_remaining > 0)
            --_beep_remaining;
        if (_beep_remaining == 0)
        {
            _beep_active = false;
            return;
        }
        _beep_state_on = true;
        buzzerOn_(true);
        _beep_next_ms = now + _beep_on_ms;
    }

    void beepArm_() { startBeep_(2, 120, 120); }
    void beepDisarm_() { startBeep_(1, 420, 0); }
    void beepReject_() { startBeep_(3, 60, 80); }

    void requestSecurityStatus_()
    {
        if (!_node || !_node->connected())
            return;
        _msg_doc.clear();
        _msg_doc["cmd_id"] = 0;
        _msg_doc["feature"] = (uint8_t)StackFeature::Security;
        _msg_doc["action"] = "status_req";
        sendJson_((uint8_t)StackMsgType::CmdSet, _msg_doc);
    }
};
