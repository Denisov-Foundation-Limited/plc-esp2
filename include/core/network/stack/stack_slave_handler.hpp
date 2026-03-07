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
#include "controllers/watering_controller.hpp"
#include "controllers/ring_controller.hpp"
#include "controllers/avr_controller.hpp"
#include "controllers/leak_controller.hpp"
#include "controllers/controllers.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"

class StackSlaveHandler
{
public:
    using TraceHandler = void (*)(void *ctx, bool outgoing, const StackFrame &frame);
    struct RemoteMeteoItem
    {
        uint8_t id = 0;
        uint8_t group_id = 0;
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
        uint32_t pending_since_ms = 0;
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
            pending_since_ms = 0;
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
        uint8_t group_id = 0;
        bool enabled = false;
        bool state = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct RemoteSocketsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint32_t pending_since_ms = 0;
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
            pending_since_ms = 0;
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
        uint8_t group_id = 0;
        bool enabled = false;
        bool state = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct RemoteLightsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint32_t pending_since_ms = 0;
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
            pending_since_ms = 0;
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
        uint8_t group_id = 0;
        bool enabled = false;
        bool warning = false;
        bool alarm = false;
    };
    struct RemoteSepticCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint32_t pending_since_ms = 0;
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
            pending_since_ms = 0;
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
        uint8_t group_id = 0;
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
        uint32_t pending_since_ms = 0;
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
            pending_since_ms = 0;
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
        uint8_t group_id = 0;
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
        uint32_t pending_since_ms = 0;
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
            pending_since_ms = 0;
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
        uint8_t group_id = 0;
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
        uint32_t pending_since_ms = 0;
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
            pending_since_ms = 0;
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
                      TankController &tanks, WateringController &watering, RingController &ring,
                      AvrController &avr, LeakController &leak, Controllers &controllers)
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
          _watering(watering),
          _ring(ring),
          _avr(avr),
          _leak(leak),
          _controllers(controllers)
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
    void setTraceHandler(TraceHandler cb, void *ctx)
    {
        _trace_cb = cb;
        _trace_ctx = ctx;
    }
    bool nodeConnected() const { return _node && _node->connected(); }
    bool linkReadyAfterHello() const
    {
        return _node && _node->connected() && _node->helloSentCurrentConnection();
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
    WateringController &_watering;
    RingController &_ring;
    AvrController &_avr;
    LeakController &_leak;
    Controllers &_controllers;
    StackNode *_node = nullptr;
    TraceHandler _trace_cb = nullptr;
    void *_trace_ctx = nullptr;
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
    uint32_t _remote_all_pending_ms = 0;
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
        traceFrame_(false, frame);
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
            handleMeteo_(cmd_id, action, params);
            break;
        case StackFeature::Thermo:
            handleThermo_(cmd_id, action, params);
            break;
        case StackFeature::Septic:
            handleSeptic_(cmd_id, action, params);
            break;
        case StackFeature::Tanks:
            handleTanks_(cmd_id, action, params);
            break;
        case StackFeature::Watering:
            handleWatering_(cmd_id, action, params);
            break;
        case StackFeature::Avr:
            handleAvr_(cmd_id, action, params);
            break;
        case StackFeature::Leak:
            handleLeak_(cmd_id, action, params);
            break;
        case StackFeature::Groups:
            handleGroups_(cmd_id, action, params);
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
            static constexpr size_t kDefaultChunk = 5;
            static constexpr size_t kMaxChunk = 8;
            const bool has_ids = params.is<JsonObjectConst>() && params["ids"].is<JsonArrayConst>();
            const bool brief = params.is<JsonObjectConst>() && (params["brief"] | false);
            const JsonArrayConst ids = has_ids ? params["ids"].as<JsonArrayConst>() : JsonArrayConst();
            size_t chunk = kDefaultChunk;
            if (params.is<JsonObjectConst>() && params["chunk"].is<unsigned>())
            {
                const unsigned raw = params["chunk"].as<unsigned>();
                if (raw > 0)
                    chunk = raw;
            }
            if (chunk == 0)
                chunk = kDefaultChunk;
            if (chunk > kMaxChunk)
                chunk = kMaxChunk;
            size_t total = 0;
            if (has_ids)
            {
                for (JsonVariantConst v : ids)
                {
                    if (!v.is<unsigned>())
                        continue;
                    const uint8_t id = (uint8_t)v.as<unsigned>();
                    if (!portVisibleInPortsList_(id))
                        continue;
                    ++total;
                }
            }
            else
            {
                for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
                {
                    if (!portVisibleInPortsList_(i))
                        continue;
                    ++total;
                }
            }

            const size_t offset = (size_t)(params["offset"] | 0u);
            size_t limit = (size_t)(params["limit"] | (unsigned)chunk);
            if (limit == 0)
                limit = chunk;
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["ports"].to<JsonArray>();
            const size_t from = offset;
            const size_t to = offset + limit;
            size_t pos = 0;
            if (has_ids)
            {
                for (JsonVariantConst v : ids)
                {
                    if (!v.is<unsigned>())
                        continue;
                    const uint8_t id = (uint8_t)v.as<unsigned>();
                    if (!portVisibleInPortsList_(id))
                        continue;
                    if (pos >= from && pos < to)
                    {
                        JsonObject o = arr.add<JsonObject>();
                        fillPortItem_(o, id, _io.read(id), brief);
                    }
                    ++pos;
                    if (pos >= to)
                        break;
                }
            }
            else
            {
                uint8_t relay_ids[IoStack::PORT_COUNT] = {};
                uint8_t dinput_ids[IoStack::PORT_COUNT] = {};
                uint8_t other_ids[IoStack::PORT_COUNT] = {};
                size_t relay_n = 0;
                size_t dinput_n = 0;
                size_t other_n = 0;
                for (uint8_t i = 0; i < IoStack::PORT_COUNT; ++i)
                {
                    if (!portVisibleInPortsList_(i))
                        continue;
                    const auto &p = ActiveBoardProfile::PORTS[i];
                    if (p.type == PortIO::PinType::Relay)
                        relay_ids[relay_n++] = i;
                    else if (p.type == PortIO::PinType::DInput || p.type == PortIO::PinType::Button)
                        dinput_ids[dinput_n++] = i;
                    else
                        other_ids[other_n++] = i;
                }
                size_t ri = 0;
                size_t di = 0;
                while ((ri < relay_n || di < dinput_n) && pos < to)
                {
                    if (ri < relay_n)
                    {
                        const uint8_t id = relay_ids[ri++];
                        if (pos >= from && pos < to)
                        {
                            JsonObject o = arr.add<JsonObject>();
                            fillPortItem_(o, id, _io.read(id), brief);
                        }
                        ++pos;
                        if (pos >= to)
                            break;
                    }
                    if (di < dinput_n)
                    {
                        const uint8_t id = dinput_ids[di++];
                        if (pos >= from && pos < to)
                        {
                            JsonObject o = arr.add<JsonObject>();
                            fillPortItem_(o, id, _io.read(id), brief);
                        }
                        ++pos;
                        if (pos >= to)
                            break;
                    }
                }
                for (size_t oi = 0; oi < other_n && pos < to; ++oi)
                {
                    const uint8_t id = other_ids[oi];
                    if (pos >= from && pos < to)
                    {
                        JsonObject o = arr.add<JsonObject>();
                        fillPortItem_(o, id, _io.read(id), brief);
                    }
                    ++pos;
                }
            }
            doc["offset"] = (unsigned)offset;
            doc["limit"] = (unsigned)limit;
            doc["total"] = (unsigned)total;
            const size_t next = offset + arr.size();
            const bool done_page = (next >= total);
            doc["next_offset"] = (unsigned)next;
            doc["done"] = done_page;
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

    void handleTempSensors_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        static constexpr size_t kMaxSerials = 50;
        char serials[kMaxSerials][17] = {};
        size_t serial_count = 0;
        _ds18b20.listSerials(serials, kMaxSerials, serial_count);
        if (action == "list_page")
        {
            uint16_t offset = 0;
            uint16_t limit = 8;
            if (params.is<JsonObjectConst>())
            {
                offset = (uint16_t)(params["offset"] | 0u);
                const uint16_t raw_limit = (uint16_t)(params["limit"] | 8u);
                if (raw_limit > 0)
                    limit = raw_limit;
            }
            if (limit == 0)
                limit = 8;
            if (limit > 16)
                limit = 16;
            if (offset > serial_count)
                offset = (uint16_t)serial_count;

            char used_list[MeteoController::kSensorCount][17] = {};
            size_t used_count = 0;
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = _meteo.configByIndex(i);
                if (!cfg || !cfg->enabled || cfg->type != MeteoController::SensorType::Ds18b20 || !cfg->ds18_addr_set)
                    continue;
                char hex[17] = {};
                MeteoController::formatHexAddr(cfg->ds18_addr, hex);
                bool exists = false;
                for (size_t j = 0; j < used_count; ++j)
                {
                    if (strcmp(used_list[j], hex) == 0)
                    {
                        exists = true;
                        break;
                    }
                }
                if (!exists && used_count < MeteoController::kSensorCount)
                    strlcpy(used_list[used_count++], hex, sizeof(used_list[0]));
            }

            const size_t to = min((size_t)serial_count, (size_t)offset + (size_t)limit);
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (size_t i = offset; i < to; ++i)
            {
                JsonObject o = arr.add<JsonObject>();
                o["addr"] = serials[i];
                bool used = false;
                for (size_t j = 0; j < used_count; ++j)
                {
                    if (strcmp(used_list[j], serials[i]) == 0)
                    {
                        used = true;
                        break;
                    }
                }
                o["used"] = used;
            }
            doc["offset"] = (unsigned)offset;
            doc["limit"] = (unsigned)limit;
            doc["total"] = (unsigned)serial_count;
            doc["next_offset"] = (unsigned)to;
            doc["done"] = (to >= serial_count);
            sendAck_(cmd_id, doc);
            return;
        }
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

    void appendLocalGroups_(JsonDocument &doc)
    {
        if (!_configs)
            return;
        JsonArray groups = doc["groups"].to<JsonArray>();
        for (size_t i = 0; i < _configs->groupCount(); ++i)
        {
            ConfigsManagerIface::GroupConfig g;
            if (!_configs->groupByIndex(i, g) || g.id == 0 || g.name.length() == 0)
                continue;
            JsonObject o = groups.add<JsonObject>();
            o["id"] = (unsigned)g.id;
            o["sort"] = (unsigned)g.sort;
            o["name"] = g.name;
        }
    }

    void handleGroups_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (!_configs)
        {
            sendErr_(cmd_id, "cfg");
            return;
        }
        if (action == "get")
        {
            _tx_doc.clear();
            appendLocalGroups_(_tx_doc);
            sendAck_(cmd_id, _tx_doc);
            return;
        }
        if (action != "set")
        {
            sendErr_(cmd_id, "unsupported");
            return;
        }

        JsonArrayConst groups = params["groups"].as<JsonArrayConst>();
        if (groups.isNull())
        {
            sendErr_(cmd_id, "bad params");
            return;
        }

        bool keep_ids[256] = {};
        for (JsonObjectConst g : groups)
        {
            String name = g["name"] | "";
            name.trim();
            if (!name.length())
                continue;
            uint8_t id = (uint8_t)(g["id"] | 0u);
            if (id == 0)
                id = _configs->allocateGroupId();
            if (id == 0 || !_configs->setGroup(id, name, (uint16_t)(g["sort"] | 0u)))
            {
                sendErr_(cmd_id, "set failed");
                return;
            }
            keep_ids[id] = true;
        }

        ConfigsManagerIface::GroupConfig existing[16] = {};
        size_t existing_count = 0;
        for (size_t i = 0; i < _configs->groupCount() && existing_count < 16; ++i)
        {
            ConfigsManagerIface::GroupConfig g;
            if (!_configs->groupByIndex(i, g) || g.id == 0)
                continue;
            existing[existing_count++] = g;
        }
        for (size_t i = 0; i < existing_count; ++i)
        {
            const uint8_t id = existing[i].id;
            if (id != 0 && !keep_ids[id])
                _configs->removeGroup(id);
        }
        if (!_configs->save())
        {
            sendErr_(cmd_id, "save failed");
            return;
        }
        _tx_doc.clear();
        appendLocalGroups_(_tx_doc);
        sendAck_(cmd_id, _tx_doc);
    }

    void handleSockets_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            static constexpr size_t kDefaultChunk = 3;
            static constexpr size_t kMaxChunk = 16;
            size_t chunk = kDefaultChunk;
            if (params.is<JsonObjectConst>() && params["chunk"].is<unsigned>())
            {
                const unsigned raw = params["chunk"].as<unsigned>();
                if (raw > 0)
                    chunk = raw;
            }
            if (chunk > kMaxChunk)
                chunk = kMaxChunk;

            size_t total = 0;
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = _sockets.configByIndex(i);
                const auto *st = _sockets.stateByIndex(i);
                if (cfg && st && cfg->enabled)
                    ++total;
            }

            const size_t parts = total ? ((total + chunk - 1) / chunk) : 1;
            for (size_t part = 0; part < parts; ++part)
            {
                const size_t from = part * chunk;
                const size_t to = from + chunk;
                _tx_doc.clear();
                JsonDocument &doc = _tx_doc;
                appendLocalGroups_(doc);
                JsonArray arr = doc["items"].to<JsonArray>();
                size_t pos = 0;
                for (size_t i = 0; i < SocketController::kSocketCount; ++i)
                {
                    const auto *cfg = _sockets.configByIndex(i);
                    const auto *st = _sockets.stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (pos >= from && pos < to)
                    {
                        JsonObject o = arr.add<JsonObject>();
                        o["id"] = (unsigned)cfg->id;
                        o["group_id"] = (unsigned)cfg->group_id;
                        o["enabled"] = cfg->enabled;
                        if (cfg->name.length())
                            o["name"] = cfg->name;
                        if (cfg->button_port != SocketController::kInvalidPort)
                            o["button"] = cfg->button_port;
                        if (cfg->relay_port != SocketController::kInvalidPort)
                            o["relay"] = cfg->relay_port;
                        o["state"] = st->relay_on;
                    }
                    ++pos;
                    if (pos >= to)
                        break;
                }
                doc["part"] = (unsigned)(part + 1);
                doc["parts"] = (unsigned)parts;
                doc["done"] = (part + 1) >= parts;
                sendAck_(cmd_id, doc);
            }
            return;
        }
        if (action == "get_lights")
        {
            static constexpr size_t kDefaultChunk = 6;
            static constexpr size_t kMaxChunk = 16;
            size_t chunk = kDefaultChunk;
            if (params.is<JsonObjectConst>() && params["chunk"].is<unsigned>())
            {
                const unsigned raw = params["chunk"].as<unsigned>();
                if (raw > 0)
                    chunk = raw;
            }
            if (chunk > kMaxChunk)
                chunk = kMaxChunk;

            size_t total = 0;
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = _sockets.lightConfigByIndex(i);
                const auto *st = _sockets.lightStateByIndex(i);
                if (cfg && st && cfg->enabled)
                    ++total;
            }

            const size_t parts = total ? ((total + chunk - 1) / chunk) : 1;
            for (size_t part = 0; part < parts; ++part)
            {
                const size_t from = part * chunk;
                const size_t to = from + chunk;
                _tx_doc.clear();
                JsonDocument &doc = _tx_doc;
                appendLocalGroups_(doc);
                JsonArray arr = doc["items"].to<JsonArray>();
                size_t pos = 0;
                for (size_t i = 0; i < SocketController::kLightCount; ++i)
                {
                    const auto *cfg = _sockets.lightConfigByIndex(i);
                    const auto *st = _sockets.lightStateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (pos >= from && pos < to)
                    {
                        JsonObject o = arr.add<JsonObject>();
                        o["id"] = (unsigned)cfg->id;
                        o["group_id"] = (unsigned)cfg->group_id;
                        o["enabled"] = cfg->enabled;
                        if (cfg->name.length())
                            o["name"] = cfg->name;
                        if (cfg->button_port != SocketController::kInvalidPort)
                            o["button"] = cfg->button_port;
                        if (cfg->relay_port != SocketController::kInvalidPort)
                            o["relay"] = cfg->relay_port;
                        o["state"] = st->relay_on;
                    }
                    ++pos;
                    if (pos >= to)
                        break;
                }
                doc["part"] = (unsigned)(part + 1);
                doc["parts"] = (unsigned)parts;
                doc["done"] = (part + 1) >= parts;
                sendAck_(cmd_id, doc);
            }
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
            bool gpio_usage_changed = false;
            for (JsonVariantConst v : items)
            {
                if (!v.is<JsonObjectConst>())
                    continue;
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                if (item["name"].is<const char *>())
                    _sockets.setName(id, String(item["name"].as<const char *>()));
                if (item["group_id"].is<unsigned>() || item["group_id"].is<int>())
                    _sockets.setGroupId(id, (uint8_t)(item["group_id"] | 0u));
                if (item["button"].is<unsigned>())
                {
                    _sockets.setButtonPort(id, (uint8_t)item["button"].as<unsigned>());
                    gpio_usage_changed = true;
                }
                else if (item["button"].is<int>() && item["button"].as<int>() < 0)
                {
                    _sockets.setButtonPort(id, SocketController::kInvalidPort);
                    gpio_usage_changed = true;
                }
                if (item["relay"].is<unsigned>())
                {
                    _sockets.setRelayPort(id, (uint8_t)item["relay"].as<unsigned>());
                    gpio_usage_changed = true;
                }
                else if (item["relay"].is<int>() && item["relay"].as<int>() < 0)
                {
                    _sockets.setRelayPort(id, SocketController::kInvalidPort);
                    gpio_usage_changed = true;
                }
                if (item["enabled"].is<bool>())
                {
                    _sockets.setEnabled(id, item["enabled"].as<bool>());
                    gpio_usage_changed = true;
                }
                else if (item["enabled"].is<int>())
                {
                    _sockets.setEnabled(id, item["enabled"].as<int>() != 0);
                    gpio_usage_changed = true;
                }
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
            if (gpio_usage_changed)
                _controllers.invalidateGpioUsageCache();
            DynamicJsonDocument doc(1024);
            JsonArray out_items = doc["items"].to<JsonArray>();
            for (JsonVariantConst v : items)
            {
                if (!v.is<JsonObjectConst>())
                    continue;
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                JsonObject o = out_items.add<JsonObject>();
                o["id"] = (unsigned)id;
                if (const auto *cfg = _sockets.config(id))
                {
                    o["enabled"] = cfg->enabled;
                    o["group_id"] = (unsigned)cfg->group_id;
                }
                bool relay_on = false;
                if (_sockets.relayStateById(id, relay_on))
                    o["state"] = relay_on;
            }
            sendAck_(cmd_id, doc);
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
                if (item["group_id"].is<unsigned>() || item["group_id"].is<int>())
                    _sockets.setLightGroupId(id, (uint8_t)(item["group_id"] | 0u));
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
            DynamicJsonDocument doc(1024);
            JsonArray out_items = doc["items"].to<JsonArray>();
            for (JsonVariantConst v : items)
            {
                if (!v.is<JsonObjectConst>())
                    continue;
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                JsonObject o = out_items.add<JsonObject>();
                o["id"] = (unsigned)id;
                if (const auto *cfg = _sockets.lightConfig(id))
                {
                    o["enabled"] = cfg->enabled;
                    o["group_id"] = (unsigned)cfg->group_id;
                }
                bool relay_on = false;
                if (_sockets.lightRelayStateById(id, relay_on))
                    o["state"] = relay_on;
            }
            sendAck_(cmd_id, doc);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleMeteo_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "set")
        {
            if (!params.is<JsonObjectConst>() || !params["items"].is<JsonArrayConst>())
            {
                sendErr_(cmd_id, "missing items");
                return;
            }
            JsonArrayConst items = params["items"].as<JsonArrayConst>();
            bool gpio_usage_changed = false;
            for (JsonVariantConst v : items)
            {
                if (!v.is<JsonObjectConst>())
                    continue;
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();

                if (item["name"].is<const char *>())
                    _meteo.setName(id, String(item["name"].as<const char *>()));
                if (item["group_id"].is<unsigned>() || item["group_id"].is<int>())
                    _meteo.setGroupId(id, (uint8_t)(item["group_id"] | 0u));

                if (item["type"].is<const char *>())
                {
                    const char *t = item["type"].as<const char *>();
                    if (t)
                    {
                        String ts(t);
                        ts.toLowerCase();
                        if (ts == "none")
                            _meteo.setType(id, MeteoController::SensorType::None);
                        else if (ts == "dht22")
                            _meteo.setType(id, MeteoController::SensorType::Dht22);
                        else if (ts == "ds18b20")
                            _meteo.setType(id, MeteoController::SensorType::Ds18b20);
                        gpio_usage_changed = true;
                    }
                }

                if (item["pin"].is<unsigned>())
                {
                    _meteo.setDht22Pin(id, (uint8_t)item["pin"].as<unsigned>());
                    gpio_usage_changed = true;
                }
                else if (item["pin"].is<int>() && item["pin"].as<int>() < 0)
                {
                    _meteo.setDht22Pin(id, MeteoController::kInvalidPin);
                    gpio_usage_changed = true;
                }

                if (item["addr"].is<const char *>())
                {
                    const char *hex = item["addr"].as<const char *>();
                    uint8_t addr[MeteoController::kAddrLen] = {};
                    bool addr_set = false;
                    if (hex && hex[0])
                    {
                        addr_set = MeteoController::parseHexAddr(hex, addr);
                    }
                    _meteo.setDs18b20Addr(id, addr, addr_set);
                }

                if (item["enabled"].is<bool>())
                {
                    _meteo.setEnabled(id, item["enabled"].as<bool>());
                    gpio_usage_changed = true;
                }
                else if (item["enabled"].is<int>())
                {
                    _meteo.setEnabled(id, item["enabled"].as<int>() != 0);
                    gpio_usage_changed = true;
                }
            }
            if (gpio_usage_changed)
                _controllers.invalidateGpioUsageCache();
            sendAck_(cmd_id);
            return;
        }
        if (action != "get")
        {
            sendErr_(cmd_id, "unsupported");
            return;
        }
        static constexpr size_t kDefaultChunk = 6;
        static constexpr size_t kMaxChunk = 16;
        size_t chunk = kDefaultChunk;
        if (params.is<JsonObjectConst>() && params["chunk"].is<unsigned>())
        {
            const unsigned raw = params["chunk"].as<unsigned>();
            if (raw > 0)
                chunk = raw;
        }
        if (chunk > kMaxChunk)
            chunk = kMaxChunk;

        size_t total = 0;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = _meteo.configByIndex(i);
            const auto *st = _meteo.stateByIndex(i);
            if (cfg && st && cfg->enabled)
                ++total;
        }

        const size_t parts = total ? ((total + chunk - 1) / chunk) : 1;
        for (size_t part = 0; part < parts; ++part)
        {
            const size_t from = part * chunk;
            const size_t to = from + chunk;
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            appendLocalGroups_(doc);
            JsonArray arr = doc["items"].to<JsonArray>();
            size_t pos = 0;
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = _meteo.configByIndex(i);
                const auto *st = _meteo.stateByIndex(i);
                if (!cfg || !st || !cfg->enabled)
                    continue;
                if (pos >= from && pos < to)
                {
                    JsonObject o = arr.add<JsonObject>();
                    const bool has_read = st->last_read_ms != 0;
                    const uint32_t age_s = has_read ? (uint32_t)((millis() - st->last_read_ms) / 1000u) : 0u;
                    o["id"] = (unsigned)cfg->id;
                    o["group_id"] = (unsigned)cfg->group_id;
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
                    o["has_read"] = has_read;
                    o["age_s"] = age_s;
                }
                ++pos;
                if (pos >= to)
                    break;
            }
            doc["part"] = (unsigned)(part + 1);
            doc["parts"] = (unsigned)parts;
            doc["done"] = (part + 1) >= parts;
            sendAck_(cmd_id, doc);
        }
    }

    void handleThermo_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            static constexpr size_t kDefaultChunk = 6;
            static constexpr size_t kMaxChunk = 16;
            size_t chunk = kDefaultChunk;
            if (params.is<JsonObjectConst>() && params["chunk"].is<unsigned>())
            {
                const unsigned raw = params["chunk"].as<unsigned>();
                if (raw > 0)
                    chunk = raw;
            }
            if (chunk > kMaxChunk)
                chunk = kMaxChunk;

            size_t total = 0;
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = _thermo.configByIndex(i);
                const auto *st = _thermo.stateByIndex(i);
                if (cfg && st && cfg->enabled)
                    ++total;
            }

            const size_t parts = total ? ((total + chunk - 1) / chunk) : 1;
            for (size_t part = 0; part < parts; ++part)
            {
                const size_t from = part * chunk;
                const size_t to = from + chunk;
                _tx_doc.clear();
                JsonDocument &doc = _tx_doc;
                appendLocalGroups_(doc);
                JsonArray arr = doc["items"].to<JsonArray>();
                size_t pos = 0;
                for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
                {
                    const auto *cfg = _thermo.configByIndex(i);
                    const auto *st = _thermo.stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (pos >= from && pos < to)
                    {
                        JsonObject o = arr.add<JsonObject>();
                        o["id"] = (unsigned)cfg->id;
                        o["group_id"] = (unsigned)cfg->group_id;
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
                    ++pos;
                    if (pos >= to)
                        break;
                }
                doc["part"] = (unsigned)(part + 1);
                doc["parts"] = (unsigned)parts;
                doc["done"] = (part + 1) >= parts;
                sendAck_(cmd_id, doc);
            }
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
            uint8_t changed_ids[ThermoController::kDeviceCount] = {};
            size_t changed_count = 0;
            auto mark_changed = [&](uint8_t id) {
                for (size_t i = 0; i < changed_count; ++i)
                {
                    if (changed_ids[i] == id)
                        return;
                }
                if (changed_count < ThermoController::kDeviceCount)
                    changed_ids[changed_count++] = id;
            };
            for (JsonVariantConst v : items)
            {
                if (!v.is<JsonObjectConst>())
                    continue;
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                if (item["group_id"].is<unsigned>() || item["group_id"].is<int>())
                {
                    _thermo.setGroupId(id, (uint8_t)(item["group_id"] | 0u));
                    mark_changed(id);
                }
                if (item["toggle"].is<bool>() && item["toggle"].as<bool>())
                {
                    _thermo.togglePower(id, "stack");
                    mark_changed(id);
                    continue;
                }
                if (item["power"].is<bool>() || item["power"].is<int>())
                {
                    const bool on = item["power"].is<bool>() ? item["power"].as<bool>()
                                                             : (item["power"].as<int>() != 0);
                    _thermo.setPower(id, on, "stack");
                    mark_changed(id);
                    continue;
                }
                if (item["state"].is<bool>() || item["state"].is<int>())
                {
                    const bool on = item["state"].is<bool>() ? item["state"].as<bool>()
                                                             : (item["state"].as<int>() != 0);
                    _thermo.setPower(id, on, "stack");
                    mark_changed(id);
                    continue;
                }
                if (item["mode"].is<const char *>())
                {
                    const char *mode = item["mode"].as<const char *>();
                    if (mode)
                    {
                        String m(mode);
                        m.toLowerCase();
                        if (m == "off")
                            _thermo.setMode(id, ThermoController::Mode::Off);
                        else if (m == "heat")
                            _thermo.setMode(id, ThermoController::Mode::Heat);
                        else if (m == "cool")
                            _thermo.setMode(id, ThermoController::Mode::Cool);
                        else if (m == "auto")
                            _thermo.setMode(id, ThermoController::Mode::Auto);
                        mark_changed(id);
                    }
                    continue;
                }
                if (item["mode"].is<unsigned>() || item["mode"].is<int>())
                {
                    const int raw = item["mode"].is<unsigned>() ? (int)item["mode"].as<unsigned>()
                                                                : item["mode"].as<int>();
                    if (raw >= (int)ThermoController::Mode::Off && raw <= (int)ThermoController::Mode::Auto)
                    {
                        _thermo.setMode(id, (ThermoController::Mode)raw);
                        mark_changed(id);
                    }
                    continue;
                }
                if (item["target"].is<float>() || item["target"].is<double>() || item["target"].is<int>())
                {
                    const float target = item["target"].is<int>() ? (float)item["target"].as<int>()
                                                                   : item["target"].as<float>();
                    _thermo.setTarget(id, target);
                    mark_changed(id);
                    continue;
                }
                if (item["heat"].is<unsigned>() || item["heat"].is<int>())
                {
                    const uint8_t port = item["heat"].is<unsigned>() ? (uint8_t)item["heat"].as<unsigned>()
                                                                     : (uint8_t)item["heat"].as<int>();
                    _thermo.setHeatPort(id, port);
                    mark_changed(id);
                    continue;
                }
                if (item["cool"].is<unsigned>() || item["cool"].is<int>())
                {
                    const uint8_t port = item["cool"].is<unsigned>() ? (uint8_t)item["cool"].as<unsigned>()
                                                                     : (uint8_t)item["cool"].as<int>();
                    _thermo.setCoolPort(id, port);
                    mark_changed(id);
                    continue;
                }
                if (item["button"].is<unsigned>() || item["button"].is<int>())
                {
                    const uint8_t port = item["button"].is<unsigned>() ? (uint8_t)item["button"].as<unsigned>()
                                                                       : (uint8_t)item["button"].as<int>();
                    _thermo.setButtonPort(id, port);
                    mark_changed(id);
                    continue;
                }
                if (item["hyst"].is<float>() || item["hyst"].is<double>() || item["hyst"].is<int>())
                {
                    const float hyst = item["hyst"].is<int>() ? (float)item["hyst"].as<int>()
                                                               : item["hyst"].as<float>();
                    _thermo.setHysteresis(id, hyst);
                    mark_changed(id);
                }
            }
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray out_items = doc["items"].to<JsonArray>();
            for (size_t i = 0; i < changed_count; ++i)
            {
                const uint8_t id = changed_ids[i];
                const auto *cfg = _thermo.config(id);
                const auto *st = _thermo.state(id);
                if (!cfg || !st)
                    continue;
                JsonObject o = out_items.add<JsonObject>();
                o["id"] = (unsigned)id;
                o["group_id"] = (unsigned)cfg->group_id;
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
            static constexpr size_t kDefaultChunk = 6;
            static constexpr size_t kMaxChunk = 16;
            size_t chunk = kDefaultChunk;
            if (params.is<JsonObjectConst>() && params["chunk"].is<unsigned>())
            {
                const unsigned raw = params["chunk"].as<unsigned>();
                if (raw > 0)
                    chunk = raw;
            }
            if (chunk > kMaxChunk)
                chunk = kMaxChunk;

            size_t total = 0;
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                const auto *cfg = _security.configByIndex(i);
                const auto *st = _security.stateByIndex(i);
                if (cfg && st && cfg->enabled)
                    ++total;
            }

            const size_t parts = total ? ((total + chunk - 1) / chunk) : 1;
            for (size_t part = 0; part < parts; ++part)
            {
                const size_t from = part * chunk;
                const size_t to = from + chunk;
                _tx_doc.clear();
                JsonDocument &doc = _tx_doc;
                doc["enabled"] = _security.controllerEnabled();
                doc["armed"] = _security.armed();
                doc["alarm"] = _security.alarmOn();
                appendLocalGroups_(doc);
                if (_security.sirenPort() != SecurityController::kInvalidPort)
                    doc["siren"] = (unsigned)_security.sirenPort();
                JsonArray arr = doc["items"].to<JsonArray>();
                size_t pos = 0;
                for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
                {
                    const auto *cfg = _security.configByIndex(i);
                    const auto *st = _security.stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (pos >= from && pos < to)
                    {
                        JsonObject o = arr.add<JsonObject>();
                        o["id"] = (unsigned)cfg->id;
                        o["group_id"] = (unsigned)cfg->group_id;
                        o["enabled"] = cfg->enabled;
                        o["type"] = (cfg->type == SecurityController::SensorType::Reed) ? "reed" : "pir";
                        if (cfg->port != SecurityController::kInvalidPort)
                            o["port"] = cfg->port;
                        o["silent"] = cfg->silent;
                        if (cfg->name.length())
                            o["name"] = cfg->name;
                        o["detect"] = st->is_detect;
                    }
                    ++pos;
                    if (pos >= to)
                        break;
                }
                doc["part"] = (unsigned)(part + 1);
                doc["parts"] = (unsigned)parts;
                doc["done"] = (part + 1) >= parts;
                sendAck_(cmd_id, doc);
            }
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
            if (obj["siren"].is<unsigned>())
                _security.setSirenPort((uint8_t)obj["siren"].as<unsigned>());
            else if (obj["siren"].is<int>() && obj["siren"].as<int>() < 0)
                _security.setSirenPort(SecurityController::kInvalidPort);
            if (obj["items"].is<JsonArrayConst>())
            {
                JsonArrayConst items = obj["items"].as<JsonArrayConst>();
                bool gpio_usage_changed = false;
                for (JsonVariantConst v : items)
                {
                    if (!v.is<JsonObjectConst>())
                        continue;
                    JsonObjectConst item = v.as<JsonObjectConst>();
                    if (!item["id"].is<unsigned>())
                        continue;
                    const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                    if (item["name"].is<const char *>())
                        _security.setName(id, String(item["name"].as<const char *>()));
                    if (item["group_id"].is<unsigned>() || item["group_id"].is<int>())
                        _security.setGroupId(id, (uint8_t)(item["group_id"] | 0u));
                    if (item["type"].is<const char *>())
                    {
                        String t = item["type"].as<const char *>();
                        t.toLowerCase();
                        _security.setType(id, (t == "reed") ? SecurityController::SensorType::Reed : SecurityController::SensorType::Pir);
                    }
                    if (item["port"].is<unsigned>())
                    {
                        _security.setPort(id, (uint8_t)item["port"].as<unsigned>());
                        gpio_usage_changed = true;
                    }
                    else if (item["port"].is<int>() && item["port"].as<int>() < 0)
                    {
                        _security.setPort(id, SecurityController::kInvalidPort);
                        gpio_usage_changed = true;
                    }
                    if (item["silent"].is<bool>() || item["silent"].is<int>())
                    {
                        const bool silent = item["silent"].is<bool>() ? item["silent"].as<bool>()
                                                                      : (item["silent"].as<int>() != 0);
                        _security.setSilent(id, silent);
                    }
                    if (item["enabled"].is<bool>() || item["enabled"].is<int>())
                    {
                        const bool en = item["enabled"].is<bool>() ? item["enabled"].as<bool>()
                                                                   : (item["enabled"].as<int>() != 0);
                        _security.setEnabled(id, en);
                        gpio_usage_changed = true;
                    }
                }
                if (gpio_usage_changed)
                    _controllers.invalidateGpioUsageCache();
            }
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
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            doc["enabled"] = _security.controllerEnabled();
            doc["armed"] = _security.armed();
            doc["alarm"] = _security.alarmOn();
            if (_security.sirenPort() != SecurityController::kInvalidPort)
                doc["siren"] = (unsigned)_security.sirenPort();
            if (obj["items"].is<JsonArrayConst>())
            {
                JsonArray arr = doc["items"].to<JsonArray>();
                for (JsonVariantConst v : obj["items"].as<JsonArrayConst>())
                {
                    if (!v.is<JsonObjectConst>() || !v["id"].is<unsigned>())
                        continue;
                    const uint8_t id = (uint8_t)v["id"].as<unsigned>();
                    const auto *cfg = _security.configByIndex((size_t)(id - 1));
                    const auto *st = _security.stateByIndex((size_t)(id - 1));
                    if (!cfg || !st)
                        continue;
                    JsonObject o = arr.add<JsonObject>();
                    o["id"] = (unsigned)cfg->id;
                    o["group_id"] = (unsigned)cfg->group_id;
                    o["enabled"] = cfg->enabled;
                    o["type"] = (cfg->type == SecurityController::SensorType::Reed) ? "reed" : "pir";
                    if (cfg->port != SecurityController::kInvalidPort)
                        o["port"] = cfg->port;
                    o["silent"] = cfg->silent;
                    if (cfg->name.length())
                        o["name"] = cfg->name;
                    o["detect"] = st->is_detect;
                }
                doc["part"] = 1;
                doc["parts"] = 1;
                doc["done"] = true;
            }
            sendAck_(cmd_id, doc);
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
        if (action == "ibutton_result")
        {
            if (!params.is<JsonObjectConst>())
            {
                sendErr_(cmd_id, "missing params");
                return;
            }
            JsonObjectConst obj = params.as<JsonObjectConst>();
            const bool match = obj["match"].is<bool>() ? obj["match"].as<bool>()
                                                       : (obj["match"].as<int>() != 0);
            const String serial = obj["serial"] | "";
            const String result = obj["result"] | "";
            const bool armed = obj["armed"].is<bool>() ? obj["armed"].as<bool>()
                                                       : (obj["armed"].as<int>() != 0);
            if (match)
                _logs.info(F("SECURITY"), F("iButton match: %s"), serial.length() ? serial.c_str() : "-");
            else
                _logs.warn(F("SECURITY"), F("iButton not match: %s"), serial.length() ? serial.c_str() : "-");
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
            static constexpr size_t kDefaultChunk = 6;
            static constexpr size_t kMaxChunk = 16;
            size_t chunk = kDefaultChunk;
            if (params.is<JsonObjectConst>() && params["chunk"].is<unsigned>())
            {
                const unsigned raw = params["chunk"].as<unsigned>();
                if (raw > 0)
                    chunk = raw;
            }
            if (chunk > kMaxChunk)
                chunk = kMaxChunk;

            size_t total = 0;
            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = _septic.configByIndex(i);
                const auto *st = _septic.stateByIndex(i);
                if (cfg && st && cfg->enabled)
                    ++total;
            }

            const size_t parts = total ? ((total + chunk - 1) / chunk) : 1;
            for (size_t part = 0; part < parts; ++part)
            {
                const size_t from = part * chunk;
                const size_t to = from + chunk;
                _tx_doc.clear();
                JsonDocument &doc = _tx_doc;
                appendLocalGroups_(doc);
                JsonArray arr = doc["items"].to<JsonArray>();
                size_t pos = 0;
                for (size_t i = 0; i < SepticController::kSepticCount; ++i)
                {
                    const auto *cfg = _septic.configByIndex(i);
                    const auto *st = _septic.stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (pos >= from && pos < to)
                    {
                        JsonObject o = arr.add<JsonObject>();
                        o["id"] = (unsigned)cfg->id;
                        o["group_id"] = (unsigned)cfg->group_id;
                        o["enabled"] = cfg->enabled;
                        o["monitor"] = cfg->monitoring_on;
                        if (cfg->name.length())
                            o["name"] = cfg->name;
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
                    ++pos;
                    if (pos >= to)
                        break;
                }
                doc["part"] = (unsigned)(part + 1);
                doc["parts"] = (unsigned)parts;
                doc["done"] = (part + 1) >= parts;
                sendAck_(cmd_id, doc);
            }
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
            bool cfg_changed = false;
            bool gpio_usage_changed = false;
            if (obj["name"].is<const char *>())
                cfg_changed |= _septic.setName(id, String(obj["name"].as<const char *>()));
            if (obj["group_id"].is<unsigned>() || obj["group_id"].is<int>())
                cfg_changed |= _septic.setGroupId(id, (uint8_t)(obj["group_id"] | 0u));
            if (obj["enabled"].is<bool>() || obj["enabled"].is<int>())
            {
                const bool en = obj["enabled"].is<bool>() ? obj["enabled"].as<bool>()
                                                          : (obj["enabled"].as<int>() != 0);
                cfg_changed |= _septic.setEnabled(id, en);
                gpio_usage_changed = true;
            }
            if (obj["warning_port"].is<unsigned>())
            {
                cfg_changed |= _septic.setWarningPort(id, (uint8_t)obj["warning_port"].as<unsigned>());
                gpio_usage_changed = true;
            }
            else if (obj["warning_port"].is<int>() && obj["warning_port"].as<int>() < 0)
            {
                cfg_changed |= _septic.setWarningPort(id, SepticController::kInvalidPort);
                gpio_usage_changed = true;
            }
            if (obj["alarm_port"].is<unsigned>())
            {
                cfg_changed |= _septic.setAlarmPort(id, (uint8_t)obj["alarm_port"].as<unsigned>());
                gpio_usage_changed = true;
            }
            else if (obj["alarm_port"].is<int>() && obj["alarm_port"].as<int>() < 0)
            {
                cfg_changed |= _septic.setAlarmPort(id, SepticController::kInvalidPort);
                gpio_usage_changed = true;
            }
            if (obj["relay_warning"].is<unsigned>())
            {
                cfg_changed |= _septic.setWarningRelay(id, (uint8_t)obj["relay_warning"].as<unsigned>());
                gpio_usage_changed = true;
            }
            else if (obj["relay_warning"].is<int>() && obj["relay_warning"].as<int>() < 0)
            {
                cfg_changed |= _septic.setWarningRelay(id, SepticController::kInvalidPort);
                gpio_usage_changed = true;
            }
            if (obj["relay_alarm"].is<unsigned>())
            {
                cfg_changed |= _septic.setAlarmRelay(id, (uint8_t)obj["relay_alarm"].as<unsigned>());
                gpio_usage_changed = true;
            }
            else if (obj["relay_alarm"].is<int>() && obj["relay_alarm"].as<int>() < 0)
            {
                cfg_changed |= _septic.setAlarmRelay(id, SepticController::kInvalidPort);
                gpio_usage_changed = true;
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
                cfg_changed = true;
            }
            if (gpio_usage_changed)
                _controllers.invalidateGpioUsageCache();
            if (cfg_changed && _configs)
                _configs->save();
            if (obj["monitor"].is<bool>() || obj["monitor"].is<int>() ||
                obj["enabled"].is<bool>() || obj["enabled"].is<int>() ||
                obj["name"].is<const char *>() ||
                obj["warning_port"].is<unsigned>() || (obj["warning_port"].is<int>() && obj["warning_port"].as<int>() < 0) ||
                obj["alarm_port"].is<unsigned>() || (obj["alarm_port"].is<int>() && obj["alarm_port"].as<int>() < 0) ||
                obj["relay_warning"].is<unsigned>() || (obj["relay_warning"].is<int>() && obj["relay_warning"].as<int>() < 0) ||
                obj["relay_alarm"].is<unsigned>() || (obj["relay_alarm"].is<int>() && obj["relay_alarm"].as<int>() < 0))
            {
                _tx_doc.clear();
                JsonDocument &doc = _tx_doc;
                JsonArray arr = doc["items"].to<JsonArray>();
                const auto *cfg = _septic.configByIndex((size_t)(id - 1));
                const auto *st = _septic.stateByIndex((size_t)(id - 1));
                if (cfg && st)
                {
                    JsonObject o = arr.add<JsonObject>();
                    o["id"] = (unsigned)cfg->id;
                    o["group_id"] = (unsigned)cfg->group_id;
                    o["enabled"] = cfg->enabled;
                    o["monitor"] = cfg->monitoring_on;
                    if (cfg->name.length())
                        o["name"] = cfg->name;
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
                doc["part"] = 1;
                doc["parts"] = 1;
                doc["done"] = true;
                sendAck_(cmd_id, doc);
                return;
            }
            sendErr_(cmd_id, "missing params");
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleTanks_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            static constexpr size_t kDefaultChunk = 6;
            static constexpr size_t kMaxChunk = 16;
            size_t chunk = kDefaultChunk;
            if (params.is<JsonObjectConst>() && params["chunk"].is<unsigned>())
            {
                const unsigned raw = params["chunk"].as<unsigned>();
                if (raw > 0)
                    chunk = raw;
            }
            if (chunk > kMaxChunk)
                chunk = kMaxChunk;

            size_t total = 0;
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = _tanks.configByIndex(i);
                const auto *st = _tanks.stateByIndex(i);
                if (cfg && st && cfg->enabled)
                    ++total;
            }

            const size_t parts = total ? ((total + chunk - 1) / chunk) : 1;
            for (size_t part = 0; part < parts; ++part)
            {
                const size_t from = part * chunk;
                const size_t to = from + chunk;
                _tx_doc.clear();
                JsonDocument &doc = _tx_doc;
                appendLocalGroups_(doc);
                JsonArray arr = doc["items"].to<JsonArray>();
                size_t pos = 0;
                for (size_t i = 0; i < TankController::kTankCount; ++i)
                {
                    const auto *cfg = _tanks.configByIndex(i);
                    const auto *st = _tanks.stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (pos >= from && pos < to)
                    {
                        JsonObject o = arr.add<JsonObject>();
                        o["id"] = (unsigned)cfg->id;
                        o["group_id"] = (unsigned)cfg->group_id;
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
                    ++pos;
                    if (pos >= to)
                        break;
                }
                doc["part"] = (unsigned)(part + 1);
                doc["parts"] = (unsigned)parts;
                doc["done"] = (part + 1) >= parts;
                sendAck_(cmd_id, doc);
            }
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
            uint8_t changed_ids[TankController::kTankCount] = {};
            size_t changed_count = 0;
            bool gpio_usage_changed = false;
            for (JsonVariantConst v : items)
            {
                if (!v.is<JsonObjectConst>())
                    continue;
                JsonObjectConst item = v.as<JsonObjectConst>();
                if (!item["id"].is<unsigned>())
                    continue;
                const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                bool changed = false;
                if (item["name"].is<const char *>())
                {
                    changed |= _tanks.setName(id, String(item["name"].as<const char *>()));
                }
                if (item["group_id"].is<unsigned>() || item["group_id"].is<int>())
                    changed |= _tanks.setGroupId(id, (uint8_t)(item["group_id"] | 0u));
                if (item["low"].is<unsigned>())
                {
                    changed |= _tanks.setLevelLow(id, (uint8_t)item["low"].as<unsigned>());
                    gpio_usage_changed = true;
                }
                else if (item["low"].is<int>() && item["low"].as<int>() < 0)
                {
                    changed |= _tanks.setLevelLow(id, TankController::kInvalidPort);
                    gpio_usage_changed = true;
                }
                if (item["mid"].is<unsigned>())
                {
                    changed |= _tanks.setLevelMid(id, (uint8_t)item["mid"].as<unsigned>());
                    gpio_usage_changed = true;
                }
                else if (item["mid"].is<int>() && item["mid"].as<int>() < 0)
                {
                    changed |= _tanks.setLevelMid(id, TankController::kInvalidPort);
                    gpio_usage_changed = true;
                }
                if (item["full"].is<unsigned>())
                {
                    changed |= _tanks.setLevelFull(id, (uint8_t)item["full"].as<unsigned>());
                    gpio_usage_changed = true;
                }
                else if (item["full"].is<int>() && item["full"].as<int>() < 0)
                {
                    changed |= _tanks.setLevelFull(id, TankController::kInvalidPort);
                    gpio_usage_changed = true;
                }
                if (item["valve"].is<unsigned>())
                {
                    changed |= _tanks.setValveRelay(id, (uint8_t)item["valve"].as<unsigned>());
                    gpio_usage_changed = true;
                }
                else if (item["valve"].is<int>() && item["valve"].as<int>() < 0)
                {
                    changed |= _tanks.setValveRelay(id, TankController::kInvalidPort);
                    gpio_usage_changed = true;
                }
                if (item["pump"].is<unsigned>())
                {
                    changed |= _tanks.setPumpRelay(id, (uint8_t)item["pump"].as<unsigned>());
                    gpio_usage_changed = true;
                }
                else if (item["pump"].is<int>() && item["pump"].as<int>() < 0)
                {
                    changed |= _tanks.setPumpRelay(id, TankController::kInvalidPort);
                    gpio_usage_changed = true;
                }
                if (item["alarm"].is<unsigned>())
                {
                    changed |= _tanks.setAlarmRelay(id, (uint8_t)item["alarm"].as<unsigned>());
                    gpio_usage_changed = true;
                }
                else if (item["alarm"].is<int>() && item["alarm"].as<int>() < 0)
                {
                    changed |= _tanks.setAlarmRelay(id, TankController::kInvalidPort);
                    gpio_usage_changed = true;
                }
                if (item["enabled"].is<bool>())
                {
                    changed |= _tanks.setEnabled(id, item["enabled"].as<bool>());
                    gpio_usage_changed = true;
                }
                else if (item["enabled"].is<int>())
                {
                    changed |= _tanks.setEnabled(id, item["enabled"].as<int>() != 0);
                    gpio_usage_changed = true;
                }
                if (item["toggle"].is<bool>() && item["toggle"].as<bool>())
                {
                    const auto *cfg = _tanks.config(id);
                    if (cfg)
                        changed = _tanks.setPower(id, !cfg->power_on);
                }
                else if (item["power_on"].is<bool>() || item["power_on"].is<int>())
                {
                    const bool on = item["power_on"].is<bool>() ? item["power_on"].as<bool>()
                                                                : (item["power_on"].as<int>() != 0);
                    changed = _tanks.setPower(id, on);
                }
                else if (item["power"].is<bool>() || item["power"].is<int>())
                {
                    const bool on = item["power"].is<bool>() ? item["power"].as<bool>()
                                                             : (item["power"].as<int>() != 0);
                    changed = _tanks.setPower(id, on);
                }
                else if (item["state"].is<bool>() || item["state"].is<int>())
                {
                    const bool on = item["state"].is<bool>() ? item["state"].as<bool>()
                                                             : (item["state"].as<int>() != 0);
                    changed = _tanks.setPower(id, on);
                }
                if (!changed)
                    continue;
                bool exists = false;
                for (size_t i = 0; i < changed_count; ++i)
                {
                    if (changed_ids[i] == id)
                    {
                        exists = true;
                        break;
                    }
                }
                if (!exists && changed_count < TankController::kTankCount)
                    changed_ids[changed_count++] = id;
            }
            if (gpio_usage_changed)
                _controllers.invalidateGpioUsageCache();
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            for (size_t i = 0; i < changed_count; ++i)
            {
                const uint8_t id = changed_ids[i];
                const auto *cfg = _tanks.config(id);
                const auto *st = _tanks.state(id);
                if (!cfg || !st)
                    continue;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = (unsigned)cfg->id;
                o["group_id"] = (unsigned)cfg->group_id;
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
            doc["part"] = 1;
            doc["parts"] = 1;
            doc["done"] = true;
            sendAck_(cmd_id, doc);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleWatering_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            JsonArray arr = doc["items"].to<JsonArray>();
            const uint16_t offset = params["offset"] | 0u;
            const uint16_t limit = params["limit"] | 0u;
            static constexpr uint16_t kWateringMaxPageLimit = 1u;
            uint16_t page_limit = (limit == 0) ? kWateringMaxPageLimit : limit;
            if (page_limit > kWateringMaxPageLimit)
                page_limit = kWateringMaxPageLimit;
            uint16_t total_rules = 0;
            for (size_t i = 0; i < WateringController::kRuleCount; ++i)
            {
                const auto *cfg = _watering.configByIndex(i);
                const auto *st = _watering.stateByIndex(i);
                if (cfg && st)
                    ++total_rules;
            }
            doc["total"] = total_rules;
            doc["offset"] = offset;
            uint16_t sent = 0;
            uint16_t skipped = 0;
            for (size_t i = 0; i < WateringController::kRuleCount; ++i)
            {
                const auto *cfg = _watering.configByIndex(i);
                const auto *st = _watering.stateByIndex(i);
                if (!cfg || !st)
                    continue;
                if (skipped < offset)
                {
                    ++skipped;
                    continue;
                }
                if (sent >= page_limit)
                    break;
                JsonObject o = arr.add<JsonObject>();
                o["id"] = (unsigned)cfg->id;
                o["enabled"] = cfg->enabled;
                o["status"] = st->status;
                if (cfg->name.length())
                    o["name"] = cfg->name;
                if (cfg->port != WateringController::kInvalidPort)
                    o["port"] = cfg->port;
                if (cfg->tank_id)
                    o["tank"] = cfg->tank_id;
                if (cfg->weekdays_mask)
                    o["weekdays_mask"] = cfg->weekdays_mask;
                if (cfg->duration_sec && cfg->hour <= 23 && cfg->minute <= 59)
                {
                    o["hour"] = cfg->hour;
                    o["minute"] = cfg->minute;
                    o["duration_s"] = cfg->duration_sec;
                }
                if (cfg->duration2_sec && cfg->hour2 <= 23 && cfg->minute2 <= 59)
                {
                    o["hour2"] = cfg->hour2;
                    o["minute2"] = cfg->minute2;
                    o["duration2_s"] = cfg->duration2_sec;
                }
                if (cfg->duration3_sec && cfg->hour3 <= 23 && cfg->minute3 <= 59)
                {
                    o["hour3"] = cfg->hour3;
                    o["minute3"] = cfg->minute3;
                    o["duration3_s"] = cfg->duration3_sec;
                }
                o["resume"] = cfg->resume_after_refill;
                o["resume_level"] = cfg->resume_level;
                o["active"] = st->active;
                o["paused"] = st->paused;
                if (st->remaining_ms)
                    o["remaining_ms"] = st->remaining_ms;
                ++sent;
            }
            doc["count"] = sent;
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
            const uint8_t id = (uint8_t)(obj["id"] | 0u);
            if (id == 0 || id > WateringController::kRuleCount)
            {
                sendErr_(cmd_id, "invalid id");
                return;
            }
            bool cfg_changed = false;
            if (obj["enabled"].is<bool>() || obj["enabled"].is<int>())
            {
                const bool on = obj["enabled"].is<bool>() ? obj["enabled"].as<bool>()
                                                          : (obj["enabled"].as<int>() != 0);
                if (_watering.setEnabled(id, on))
                    cfg_changed = true;
            }
            if (obj["name"].is<const char *>())
            {
                if (_watering.setName(id, String(obj["name"].as<const char *>())))
                    cfg_changed = true;
            }
            if (obj["tank"].is<unsigned>())
            {
                const uint8_t tank = (uint8_t)obj["tank"].as<unsigned>();
                if (_watering.setTankId(id, tank))
                    cfg_changed = true;
            }
            bool gpio_usage_changed = false;
            if (obj["port"].is<unsigned>())
            {
                const uint8_t port = (uint8_t)obj["port"].as<unsigned>();
                if (_watering.setPort(id, port))
                {
                    cfg_changed = true;
                    gpio_usage_changed = true;
                }
            }
            else if (obj["port"].is<int>() && obj["port"].as<int>() < 0)
            {
                if (_watering.setPort(id, WateringController::kInvalidPort))
                {
                    cfg_changed = true;
                    gpio_usage_changed = true;
                }
            }
            if (obj["weekdays_mask"].is<unsigned>())
            {
                const uint8_t mask = (uint8_t)obj["weekdays_mask"].as<unsigned>();
                if (_watering.setWeekdaysMask(id, mask))
                    cfg_changed = true;
            }
            if (obj["hour"].is<unsigned>() && obj["minute"].is<unsigned>())
            {
                const uint8_t hour = (uint8_t)obj["hour"].as<unsigned>();
                const uint8_t minute = (uint8_t)obj["minute"].as<unsigned>();
                if (_watering.setStartTimeSlot(id, 0, hour, minute))
                    cfg_changed = true;
            }
            if (obj["duration_s"].is<unsigned long>())
            {
                const uint32_t sec = (uint32_t)obj["duration_s"].as<unsigned long>();
                if (_watering.setDurationSlot(id, 0, sec))
                    cfg_changed = true;
            }
            if (obj["hour2"].is<unsigned>() && obj["minute2"].is<unsigned>())
            {
                const uint8_t hour = (uint8_t)obj["hour2"].as<unsigned>();
                const uint8_t minute = (uint8_t)obj["minute2"].as<unsigned>();
                if (_watering.setStartTimeSlot(id, 1, hour, minute))
                    cfg_changed = true;
            }
            if (obj["duration2_s"].is<unsigned long>())
            {
                const uint32_t sec = (uint32_t)obj["duration2_s"].as<unsigned long>();
                if (_watering.setDurationSlot(id, 1, sec))
                    cfg_changed = true;
            }
            if (obj["hour3"].is<unsigned>() && obj["minute3"].is<unsigned>())
            {
                const uint8_t hour = (uint8_t)obj["hour3"].as<unsigned>();
                const uint8_t minute = (uint8_t)obj["minute3"].as<unsigned>();
                if (_watering.setStartTimeSlot(id, 2, hour, minute))
                    cfg_changed = true;
            }
            if (obj["duration3_s"].is<unsigned long>())
            {
                const uint32_t sec = (uint32_t)obj["duration3_s"].as<unsigned long>();
                if (_watering.setDurationSlot(id, 2, sec))
                    cfg_changed = true;
            }
            if (obj["resume"].is<bool>() || obj["resume"].is<int>())
            {
                const bool on = obj["resume"].is<bool>() ? obj["resume"].as<bool>()
                                                         : (obj["resume"].as<int>() != 0);
                if (_watering.setResumeAfterRefill(id, on))
                    cfg_changed = true;
            }
            if (obj["resume_level"].is<unsigned>())
            {
                const uint8_t lvl = (uint8_t)obj["resume_level"].as<unsigned>();
                if (_watering.setResumeLevel(id, lvl))
                    cfg_changed = true;
            }
            else if (obj["resume_level"].is<const char *>())
            {
                String lvl = obj["resume_level"].as<const char *>();
                lvl.toLowerCase();
                uint8_t v = 0;
                if (lvl == "mid")
                    v = 1;
                else if (lvl == "full")
                    v = 2;
                if (_watering.setResumeLevel(id, v))
                    cfg_changed = true;
            }

            bool on = false;
            bool has_state = false;
            if (obj["status"].is<bool>() || obj["status"].is<int>())
            {
                on = obj["status"].is<bool>() ? obj["status"].as<bool>() : (obj["status"].as<int>() != 0);
                has_state = true;
            }
            else if (obj["state"].is<bool>() || obj["state"].is<int>())
            {
                on = obj["state"].is<bool>() ? obj["state"].as<bool>() : (obj["state"].as<int>() != 0);
                has_state = true;
            }
            else if (obj["state"].is<const char *>())
            {
                String s = obj["state"].as<const char *>();
                s.toLowerCase();
                on = (s == "on");
                has_state = true;
            }
            bool state_changed = false;
            if (has_state)
            {
                if (!_watering.setStatus(id, on))
                {
                    sendErr_(cmd_id, "failed");
                    return;
                }
                state_changed = true;
            }
            if (!cfg_changed && !state_changed)
            {
                sendAck_(cmd_id);
                return;
            }
            if (gpio_usage_changed)
                _controllers.invalidateGpioUsageCache();
            if (cfg_changed && _configs)
                _configs->save();
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
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

    void handleAvr_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            _tx_doc.clear();
            JsonDocument &doc = _tx_doc;
            const auto &cfg = _avr.config();
            const auto &st = _avr.state();
            doc["enabled"] = cfg.enabled;
            doc["auto_mode"] = cfg.auto_mode;
            doc["prefer_main"] = cfg.prefer_main;
            doc["auto_return_main"] = cfg.auto_return_main;
            if (cfg.main_ok_port != AvrController::kInvalidPort)
                doc["main_ok_port"] = cfg.main_ok_port;
            if (cfg.reserve_ok_port != AvrController::kInvalidPort)
                doc["reserve_ok_port"] = cfg.reserve_ok_port;
            if (cfg.relay_main_port != AvrController::kInvalidPort)
                doc["relay_main_port"] = cfg.relay_main_port;
            if (cfg.relay_reserve_port != AvrController::kInvalidPort)
                doc["relay_reserve_port"] = cfg.relay_reserve_port;
            if (cfg.feedback_main_port != AvrController::kInvalidPort)
                doc["feedback_main_port"] = cfg.feedback_main_port;
            if (cfg.feedback_reserve_port != AvrController::kInvalidPort)
                doc["feedback_reserve_port"] = cfg.feedback_reserve_port;
            doc["main_ok"] = st.main_ok;
            doc["reserve_ok"] = st.reserve_ok;
            doc["relay_main_on"] = st.relay_main_on;
            doc["relay_reserve_on"] = st.relay_reserve_on;
            doc["active_source"] = AvrController::sourceName(st.active_source);
            doc["target_source"] = AvrController::sourceName(st.target_source);
            doc["fault"] = AvrController::faultName(st.fault);
            doc["transfer"] = st.transfer_in_progress;
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
            DynamicJsonDocument cfg_doc(1024);
            JsonObject cfg = cfg_doc.to<JsonObject>();
            if (obj["enabled"].is<bool>())
                cfg["enabled"] = obj["enabled"].as<bool>();
            if (obj["auto_mode"].is<bool>())
                cfg["auto_mode"] = obj["auto_mode"].as<bool>();
            if (obj["prefer_main"].is<bool>())
                cfg["prefer_main"] = obj["prefer_main"].as<bool>();
            if (obj["auto_return_main"].is<bool>())
                cfg["auto_return_main"] = obj["auto_return_main"].as<bool>();
            if (obj["main_ok"].is<unsigned>())
                cfg["main_ok"] = obj["main_ok"].as<unsigned>();
            if (obj["reserve_ok"].is<unsigned>())
                cfg["reserve_ok"] = obj["reserve_ok"].as<unsigned>();
            if (obj["relay_main"].is<unsigned>())
                cfg["relay_main"] = obj["relay_main"].as<unsigned>();
            if (obj["relay_reserve"].is<unsigned>())
                cfg["relay_reserve"] = obj["relay_reserve"].as<unsigned>();
            if (obj["feedback_main"].is<unsigned>())
                cfg["feedback_main"] = obj["feedback_main"].as<unsigned>();
            if (obj["feedback_reserve"].is<unsigned>())
                cfg["feedback_reserve"] = obj["feedback_reserve"].as<unsigned>();
            if (obj["main_ok_active_low"].is<bool>())
                cfg["main_ok_active_low"] = obj["main_ok_active_low"].as<bool>();
            if (obj["reserve_ok_active_low"].is<bool>())
                cfg["reserve_ok_active_low"] = obj["reserve_ok_active_low"].as<bool>();
            if (obj["feedback_main_active_low"].is<bool>())
                cfg["feedback_main_active_low"] = obj["feedback_main_active_low"].as<bool>();
            if (obj["feedback_reserve_active_low"].is<bool>())
                cfg["feedback_reserve_active_low"] = obj["feedback_reserve_active_low"].as<bool>();
            if (obj["relay_main_invert"].is<bool>())
                cfg["relay_main_invert"] = obj["relay_main_invert"].as<bool>();
            if (obj["relay_reserve_invert"].is<bool>())
                cfg["relay_reserve_invert"] = obj["relay_reserve_invert"].as<bool>();
            if (obj["debounce_ms"].is<unsigned>())
                cfg["debounce_ms"] = obj["debounce_ms"].as<unsigned>();
            if (obj["loss_delay_ms"].is<unsigned>())
                cfg["loss_delay_ms"] = obj["loss_delay_ms"].as<unsigned>();
            if (obj["return_delay_ms"].is<unsigned>())
                cfg["return_delay_ms"] = obj["return_delay_ms"].as<unsigned>();
            if (obj["break_ms"].is<unsigned>())
                cfg["break_ms"] = obj["break_ms"].as<unsigned>();
            if (obj["warmup_ms"].is<unsigned>())
                cfg["warmup_ms"] = obj["warmup_ms"].as<unsigned>();
            if (obj["transfer_timeout_ms"].is<unsigned>())
                cfg["transfer_timeout_ms"] = obj["transfer_timeout_ms"].as<unsigned>();
            _avr.applyConfig(cfg_doc.as<JsonObjectConst>());

            if (obj["manual_source"].is<const char *>())
            {
                String src = obj["manual_source"].as<const char *>();
                src.toLowerCase();
                if (src == "main")
                    _avr.setManualSource(AvrController::Source::Main);
                else if (src == "reserve")
                    _avr.setManualSource(AvrController::Source::Reserve);
                else
                    _avr.setManualSource(AvrController::Source::Off);
            }
            if (obj["clear_fault"].is<bool>() && obj["clear_fault"].as<bool>())
                _avr.clearFault();

            if (_configs)
                _configs->save();
            sendAck_(cmd_id);
            return;
        }
        sendErr_(cmd_id, "unsupported");
    }

    void handleLeak_(uint16_t cmd_id, const String &action, JsonVariantConst params)
    {
        if (action == "get")
        {
            static constexpr size_t kDefaultChunk = 6;
            static constexpr size_t kMaxChunk = 16;
            size_t chunk = kDefaultChunk;
            if (params.is<JsonObjectConst>() && params["chunk"].is<unsigned>())
            {
                const unsigned raw = params["chunk"].as<unsigned>();
                if (raw > 0)
                    chunk = raw;
            }
            if (chunk > kMaxChunk)
                chunk = kMaxChunk;

            size_t total = 0;
            for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            {
                const auto *cfg = _leak.configByIndex(i);
                const auto *st = _leak.stateByIndex(i);
                if (cfg && st && cfg->enabled)
                    ++total;
            }

            const size_t parts = total ? ((total + chunk - 1) / chunk) : 1;
            for (size_t part = 0; part < parts; ++part)
            {
                const size_t from = part * chunk;
                const size_t to = from + chunk;
                _tx_doc.clear();
                JsonDocument &doc = _tx_doc;
                JsonArray arr = doc["items"].to<JsonArray>();
                size_t pos = 0;
                for (size_t i = 0; i < LeakController::kZoneCount; ++i)
                {
                    const auto *cfg = _leak.configByIndex(i);
                    const auto *st = _leak.stateByIndex(i);
                    if (!cfg || !st || !cfg->enabled)
                        continue;
                    if (pos >= from && pos < to)
                    {
                        JsonObject o = arr.add<JsonObject>();
                        o["id"] = (unsigned)cfg->id;
                        o["enabled"] = cfg->enabled;
                        o["power_on"] = cfg->power_on;
                        o["sensor_active_low"] = cfg->sensor_active_low;
                        if (cfg->sensor_port != LeakController::kInvalidPort)
                            o["sensor"] = cfg->sensor_port;
                        if (cfg->valve_port != LeakController::kInvalidPort)
                            o["valve"] = cfg->valve_port;
                        if (cfg->alarm_port != LeakController::kInvalidPort)
                            o["alarm"] = cfg->alarm_port;
                        if (cfg->name.length())
                            o["name"] = cfg->name;
                        o["wet"] = st->wet;
                        o["alarm_latched"] = st->alarm_latched;
                    }
                    ++pos;
                    if (pos >= to)
                        break;
                }
                doc["part"] = (unsigned)(part + 1);
                doc["parts"] = (unsigned)parts;
                doc["done"] = (part + 1) >= parts;
                sendAck_(cmd_id, doc);
            }
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
            if (obj["ack_all"].is<bool>() && obj["ack_all"].as<bool>())
                _leak.ackAll();

            if (obj["zones"].is<JsonArrayConst>())
            {
                JsonArrayConst zones = obj["zones"].as<JsonArrayConst>();
                for (JsonObjectConst zone : zones)
                {
                    if (!zone["id"].is<unsigned>())
                        continue;
                    const size_t id = (size_t)zone["id"].as<unsigned>();
                    if (id == 0 || id > LeakController::kZoneCount)
                        continue;
                    if (zone["enabled"].is<bool>())
                        _leak.setEnabled(id, zone["enabled"].as<bool>());
                    if (zone["power_on"].is<bool>())
                        _leak.setPower(id, zone["power_on"].as<bool>());
                    if (zone["sensor_active_low"].is<bool>())
                        _leak.setSensorActiveLow(id, zone["sensor_active_low"].as<bool>());
                    _leak.setValveOpenOnPower(id, true);
                    if (zone["name"].is<const char *>())
                        _leak.setName(id, String(zone["name"].as<const char *>()));
                    if (zone["sensor"].is<unsigned>())
                        _leak.setSensorPort(id, (uint8_t)zone["sensor"].as<unsigned>());
                    if (zone["valve"].is<unsigned>())
                        _leak.setValvePort(id, (uint8_t)zone["valve"].as<unsigned>());
                    if (zone["alarm"].is<unsigned>())
                        _leak.setAlarmPort(id, (uint8_t)zone["alarm"].as<unsigned>());
                }
            }
            if (_configs)
                _configs->save();
            sendAck_(cmd_id);
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

    void fillPortItem_(JsonObject o, uint8_t id, bool state, bool brief = false) const
    {
        o["id"] = id;
        o["state"] = state;
        if (id >= IoStack::PORT_COUNT)
            return;
        const auto &p = ActiveBoardProfile::PORTS[id];
        if (p.caps == Cap::None)
            return;

        o["ptype"] = (uint8_t)p.type;
        o["ctrl"] = p.allow_control;
        o["used"] = isPortUsed_(id);
        char alias_buf[24] = {};
        makePortAlias_(alias_buf, sizeof(alias_buf), id, p);
        if (alias_buf[0])
            o["alias"] = alias_buf;
        if (brief)
        {
            if (p.backend == PortIO::Backend::Extender)
            {
                o["ext"] = true;
                o["dev"] = p.u.ext.dev;
                o["pin"] = p.u.ext.pin;
            }
            else
            {
                o["ext"] = false;
                o["dev"] = -1;
                o["pin"] = p.u.esp.gpio;
            }
            return;
        }

        o["backend"] = (p.backend == PortIO::Backend::Extender) ? "Extender" : "Esp32";
        o["loc"] = stackUnitName_(toStackUnit_(p.location));
        o["type"] = portTypeName_(p.type);
        if (p.backend == PortIO::Backend::Extender)
        {
            o["ext"] = true;
            o["dev"] = p.u.ext.dev;
            o["pin"] = p.u.ext.pin;
            o["hw"] = extDevTypeName_(p.u.ext.dev);
        }
        else
        {
            o["ext"] = false;
            o["dev"] = -1;
            o["pin"] = p.u.esp.gpio;
            o["hw"] = "CPU";
        }
    }

    bool isPortUsed_(uint8_t port) const
    {
        return _controllers.gpioPortUsed(port);
    }

    bool portVisibleInPortsList_(uint8_t id) const
    {
        if (id >= IoStack::PORT_COUNT)
            return false;
        const auto &p = ActiveBoardProfile::PORTS[id];
        if (p.caps == Cap::None)
            return false;
        if (p.backend != PortIO::Backend::Extender)
            return true;
        const uint8_t dev = p.u.ext.dev;
        const auto *devs = _ext.devs();
        if (!devs || dev >= _ext.devCount())
            return false;
        if (devs[dev].type != Extender::Type::MCP23017)
            return false;
        return _ext.isPresent(dev);
    }

    static uint8_t portLocationIndex_(PortIO::Location loc)
    {
        switch (loc)
        {
        case PortIO::Location::Cpu:
            return 0;
        case PortIO::Location::Ext1:
            return 1;
        case PortIO::Location::Ext2:
            return 2;
        case PortIO::Location::Ext3:
            return 3;
        case PortIO::Location::Ext4:
            return 4;
        case PortIO::Location::Ext5:
            return 5;
        case PortIO::Location::Ext6:
            return 6;
        case PortIO::Location::Ext7:
            return 7;
        case PortIO::Location::Ext8:
            return 8;
        case PortIO::Location::Ext9:
            return 9;
        case PortIO::Location::Ext10:
            return 10;
        default:
            return 0;
        }
    }

    static const char *portAliasPrefix_(PortIO::PinType type)
    {
        switch (type)
        {
        case PortIO::PinType::Relay:
            return "rly";
        case PortIO::PinType::DInput:
        case PortIO::PinType::Button:
            return "in";
        case PortIO::PinType::Sensor:
            return "sens";
        default:
            return "p";
        }
    }

    uint8_t portUiIdForAlias_(uint8_t id, const PortIO::PortDesc &p) const
    {
        if (p.ui_id != 0)
            return p.ui_id;
        uint8_t n = 0;
        for (uint8_t i = 0; i <= id && i < IoStack::PORT_COUNT; ++i)
        {
            const auto &q = ActiveBoardProfile::PORTS[i];
            if (q.caps == Cap::None)
                continue;
            if (q.type != p.type || q.location != p.location)
                continue;
            ++n;
        }
        return n;
    }

    void makePortAlias_(char *out, size_t out_len, uint8_t id, const PortIO::PortDesc &p) const
    {
        if (!out || out_len == 0)
            return;
        out[0] = '\0';
        const char *prefix = portAliasPrefix_(p.type);
        const uint8_t ui_id = portUiIdForAlias_(id, p);
        const uint8_t loc = portLocationIndex_(p.location);
        if (p.type == PortIO::PinType::Sensor && loc == 0)
            snprintf(out, out_len, "%s-%u", prefix, (unsigned)ui_id);
        else
            snprintf(out, out_len, "%s-%u/%u", prefix, (unsigned)loc, (unsigned)ui_id);
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 4000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
                cache->pending_since_ms = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->pending_since_ms = now;
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
        {
            if (_remote_all_pending_ms && (uint32_t)(now - _remote_all_pending_ms) > 4000u)
            {
                _remote_all_cmd_id = 0;
                _remote_all_pending_ms = 0;
            }
            else
            {
                return false;
            }
        }
        if (_remote_all_updated_ms && (uint32_t)(now - _remote_all_updated_ms) < 2000u)
            return false;
        const uint16_t cmd_id = nextRemoteCmdId_();
        _remote_all_cmd_id = cmd_id;
        _remote_all_pending_ms = now;
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
            _remote_all_pending_ms = 0;
            return false;
        }
        if (!_node->send((uint8_t)StackMsgType::CmdGet, _tx_payload_buf, len))
        {
            _remote_all_cmd_id = 0;
            _remote_all_pending_ms = 0;
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 4000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
                cache->pending_since_ms = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->pending_since_ms = now;
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 4000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
                cache->pending_since_ms = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->pending_since_ms = now;
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 4000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
                cache->pending_since_ms = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->pending_since_ms = now;
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 4000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
                cache->pending_since_ms = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->pending_since_ms = now;
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
        {
            // Drop stuck pending request to avoid long freezes on display updates.
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 4000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
                cache->pending_since_ms = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->pending_since_ms = now;
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 4000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
                cache->pending_since_ms = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->pending_since_ms = now;
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
            _remote_all_pending_ms = 0;
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
                cache->pending_since_ms = 0;
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
        cache->pending_since_ms = 0;
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
        cache->pending_since_ms = 0;
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
        cache->pending_since_ms = 0;
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
        cache->pending_since_ms = 0;
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
        cache->pending_since_ms = 0;
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
        cache->pending_since_ms = 0;
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
        cache->pending_since_ms = 0;
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
        if (_rx_doc["feature"].is<unsigned>())
            _msg_doc["feature"] = _rx_doc["feature"].as<unsigned>();
        const String action = _rx_doc["action"] | "";
        if (action.length())
            _msg_doc["action"] = action;
        sendJson_((uint8_t)StackMsgType::Ack, _msg_doc);
    }

    void sendAck_(uint16_t cmd_id, const JsonDocument &data)
    {
        _msg_doc.clear();
        _msg_doc["cmd_id"] = cmd_id;
        _msg_doc["ok"] = true;
        if (_rx_doc["feature"].is<unsigned>())
            _msg_doc["feature"] = _rx_doc["feature"].as<unsigned>();
        const String action = _rx_doc["action"] | "";
        if (action.length())
            _msg_doc["action"] = action;
        if (!data.isNull())
            _msg_doc["data"] = data.as<JsonVariantConst>();
        sendJson_((uint8_t)StackMsgType::Ack, _msg_doc);
    }

    void sendErr_(uint16_t cmd_id, const char *msg)
    {
        _msg_doc.clear();
        _msg_doc["cmd_id"] = cmd_id;
        _msg_doc["ok"] = false;
        if (_rx_doc["feature"].is<unsigned>())
            _msg_doc["feature"] = _rx_doc["feature"].as<unsigned>();
        const String action = _rx_doc["action"] | "";
        if (action.length())
            _msg_doc["action"] = action;
        _msg_doc["error"] = msg ? msg : "error";
        sendJson_((uint8_t)StackMsgType::Err, _msg_doc);
    }

    void sendJson_(uint8_t type, JsonDocument &doc)
    {
        const size_t len = serializeJson(doc, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
        if (len == 0 || len > sizeof(_tx_payload_buf))
        {
            _logs.warn(F("STACK"), F("Drop tx frame: json too large type: %u bytes: %u cap: %u"),
                       (unsigned)type,
                       (unsigned)len,
                       (unsigned)sizeof(_tx_payload_buf));
            return;
        }
        StackFrame frame{};
        frame.type = type;
        frame.payload = _tx_payload_buf;
        frame.payload_len = len;
        traceFrame_(true, frame);
        _node->send(type, _tx_payload_buf, len);
    }

    void traceFrame_(bool outgoing, const StackFrame &frame)
    {
        if (_trace_cb)
            _trace_cb(_trace_ctx, outgoing, frame);
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
        _io.write(ActiveBoardProfile::BUZZER_PIN, buzzerEnabled_() ? on : false);
    }

    void startBeep_(uint8_t count, uint16_t on_ms, uint16_t off_ms)
    {
        if (count == 0)
            return;
        if (!buzzerEnabled_())
        {
            _beep_active = false;
            _beep_remaining = 0;
            _beep_state_on = false;
            buzzerOn_(false);
            return;
        }
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
        if (!buzzerEnabled_())
        {
            _beep_active = false;
            _beep_remaining = 0;
            _beep_state_on = false;
            buzzerOn_(false);
            return;
        }
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

    bool buzzerEnabled_() const
    {
        return _plc.buzzerEnabled();
    }

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
