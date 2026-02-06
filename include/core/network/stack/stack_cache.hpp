#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <new>

#if defined(ARDUINO_ARCH_ESP32)
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"
#include "soc/soc_memory_types.h"
#endif

#include "controllers/controllers.hpp"
#include "core/network/stack/stack_features.hpp"
#include "core/network/stack/stack_master.hpp"
#include "core/network/stack/stack_protocol.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"

class StackCache
{
public:
    // Types
    struct StackSocketItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool state = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackSocketsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackSocketItem *items = nullptr;
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
                items[i] = StackSocketItem{};
        }
    };
    struct StackLightItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool state = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackLightsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackLightItem *items = nullptr;
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
                items[i] = StackLightItem{};
        }
    };
    struct StackPortItem
    {
        uint8_t id = 0;
        bool ctrl = false;
        bool is_extender = false;
        int16_t dev = -1;
        int16_t pin = -1;
        static constexpr size_t kBackendLen = 32;
        static constexpr size_t kLocLen = 32;
        static constexpr size_t kTypeLen = 32;
        static constexpr size_t kHwLen = 32;
        char backend[kBackendLen] = {};
        char loc[kLocLen] = {};
        char type[kTypeLen] = {};
        char hw[kHwLen] = {};
    };
    struct StackPortsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackPortItem *items = nullptr;
        size_t capacity = PortIO::PORT_COUNT;
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
                items[i] = StackPortItem{};
        }
    };
    struct StackExtenderItem
    {
        uint8_t id = 0;
        uint8_t bus = 0;
        bool present = false;
        static constexpr size_t kAddrLen = 24;
        static constexpr size_t kTypeLen = 24;
        char addr[kAddrLen] = {};
        char type[kTypeLen] = {};
    };
    struct StackExtendersCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackExtenderItem *items = nullptr;
        size_t capacity = Extender::MAX_DEVS;
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
                items[i] = StackExtenderItem{};
        }
    };
    struct StackI2cItem
    {
        uint8_t bus = 0;
        uint8_t addr = 0;
    };
    struct StackI2cCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackI2cItem *items = nullptr;
        size_t capacity = 127;
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
                items[i] = StackI2cItem{};
        }
    };
    struct StackOwItem
    {
        uint8_t bus = 0;
        char addr[17] = {};
        char type[8] = {};
    };
    struct StackOwCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackOwItem *items = nullptr;
        size_t capacity = 64;
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
                items[i] = StackOwItem{};
        }
    };
    struct StackSecuritySensorItem
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
    struct StackSecurityCache
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
        StackSecuritySensorItem *items = nullptr;
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
                items[i] = StackSecuritySensorItem{};
        }
    };
    struct StackMeteoItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool ok = false;
        bool has_temp = false;
        bool has_hum = false;
        float temp_c = 0.0f;
        float hum = 0.0f;
        static constexpr size_t kNameLen = 48;
        static constexpr size_t kTypeLen = 24;
        static constexpr size_t kAddrLen = 24;
        char name[kNameLen] = {};
        char type[kTypeLen] = {};
        char addr[kAddrLen] = {};
        int pin = -1;
    };
    struct StackMeteoCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackMeteoItem *items = nullptr;
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
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = StackMeteoItem{};
        }
    };
    struct StackThermoItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool power_on = false;
        bool heat_on = false;
        bool cool_on = false;
        uint8_t sensor = 0;
        uint32_t sensor_node = 0;
        float target = 0.0f;
        float hyst = 0.0f;
        uint8_t heat = ThermoController::kInvalidPort;
        uint8_t cool = ThermoController::kInvalidPort;
        uint8_t button = ThermoController::kInvalidPort;
        static constexpr size_t kNameLen = 48;
        static constexpr size_t kModeLen = 24;
        char name[kNameLen] = {};
        char mode[kModeLen] = {};
    };
    struct StackThermoCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackThermoItem *items = nullptr;
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
                items[i] = StackThermoItem{};
        }
    };
    struct StackSepticItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool monitor = false;
        uint8_t warning_port = SepticController::kInvalidPort;
        uint8_t alarm_port = SepticController::kInvalidPort;
        uint8_t relay_warning = SepticController::kInvalidPort;
        uint8_t relay_alarm = SepticController::kInvalidPort;
        bool warning = false;
        bool alarm = false;
    };
    struct StackSepticCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackSepticItem *items = nullptr;
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
                items[i] = StackSepticItem{};
        }
    };
    struct StackTankItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool power_on = false;
        uint8_t low = TankController::kInvalidPort;
        uint8_t mid = TankController::kInvalidPort;
        uint8_t full = TankController::kInvalidPort;
        uint8_t valve = TankController::kInvalidPort;
        uint8_t pump = TankController::kInvalidPort;
        uint8_t alarm = TankController::kInvalidPort;
        bool level_low = false;
        bool level_mid = false;
        bool level_full = false;
        bool levels_ok = false;
        bool valve_on = false;
        bool pump_on = false;
        bool alarm_on = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackTankCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackTankItem *items = nullptr;
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
                items[i] = StackTankItem{};
        }
    };
    struct StackNodeStatusCache
    {
        uint32_t node_id = 0;
        uint32_t plc_updated_ms = 0;
        uint32_t rtc_updated_ms = 0;
        uint16_t pending_plc_cmd_id = 0;
        uint16_t pending_rtc_cmd_id = 0;
        bool pending_plc = false;
        bool pending_rtc = false;
        bool has_plc = false;
        bool has_rtc = false;
        bool last_plc_ok = false;
        bool last_rtc_ok = false;
        String last_plc_error;
        String last_rtc_error;
        float board_temp = 0.0f;
        float cpu_temp = 0.0f;
        bool fan_on = false;
        float fan_on_c = 0.0f;
        float fan_hyst_c = 0.0f;
        String rtc_date;
        String rtc_time;
        float rtc_temp = 0.0f;
        uint8_t rtc_weekday = 0;
    };

    StackCache() = default;
    ~StackCache()
    {
        releaseCaches_();
    }
    StackCache(const StackCache &) = delete;
    StackCache &operator=(const StackCache &) = delete;

    // Public API
    void setStackMaster(StackMaster *master) { _stack_master = master; }
    void setConfigsManager(ConfigsManagerIface *cfg) { _configs = cfg; }
    void setLogger(Logger *log) { _log = log; }
    void setMasterOverride(bool enabled) { _force_master = enabled; }
    void initAllocations()
    {
        if (_alloc_ready)
            return;
        initCaches_();
        _alloc_ready = true;
    }
    void logAllocations()
    {
        _alloc_logged = false;
        logAllocations_();
    }
    StackSocketsCache &socketsLocal() { return _stack_sockets_cache[0]; }
    const StackSocketsCache &socketsLocal() const { return _stack_sockets_cache[0]; }
    StackSocketsCache *socketsCache(uint32_t node_id) { return findStackSocketsCache_(node_id, false); }
    const StackSocketsCache *socketsCache(uint32_t node_id) const { return findStackSocketsCache_(node_id, false); }
    bool requestSockets(uint32_t node_id) { return requestStackSockets_(node_id); }
    StackSocketItem *socketItem(StackSocketsCache &cache_ref, uint8_t id) { return findStackSocketItem_(cache_ref, id); }

    StackLightsCache &lightsLocal() { return _stack_lights_cache[0]; }
    const StackLightsCache &lightsLocal() const { return _stack_lights_cache[0]; }
    StackLightsCache *lightsCache(uint32_t node_id) { return findStackLightsCache_(node_id, false); }
    const StackLightsCache *lightsCache(uint32_t node_id) const { return findStackLightsCache_(node_id, false); }
    bool requestLights(uint32_t node_id) { return requestStackLights_(node_id); }
    StackLightItem *lightItem(StackLightsCache &cache_ref, uint8_t id) { return findStackLightItem_(cache_ref, id); }

    StackPortsCache &portsLocal() { return _stack_ports_cache[0]; }
    const StackPortsCache &portsLocal() const { return _stack_ports_cache[0]; }
    StackPortsCache *portsCache(uint32_t node_id) { return findStackPortsCache_(node_id, false); }
    const StackPortsCache *portsCache(uint32_t node_id) const { return findStackPortsCache_(node_id, false); }
    bool requestPorts(uint32_t node_id) { return requestStackPorts_(node_id); }

    StackExtendersCache &extendersLocal() { return _stack_ext_cache[0]; }
    const StackExtendersCache &extendersLocal() const { return _stack_ext_cache[0]; }
    StackExtendersCache *extendersCache(uint32_t node_id) { return findStackExtendersCache_(node_id, false); }
    const StackExtendersCache *extendersCache(uint32_t node_id) const { return findStackExtendersCache_(node_id, false); }
    bool requestExtenders(uint32_t node_id) { return requestStackExtenders_(node_id); }

    StackI2cCache &i2cLocal() { return _stack_i2c_cache[0]; }
    const StackI2cCache &i2cLocal() const { return _stack_i2c_cache[0]; }
    StackI2cCache *i2cCache(uint32_t node_id) { return findStackI2cCache_(node_id, false); }
    const StackI2cCache *i2cCache(uint32_t node_id) const { return findStackI2cCache_(node_id, false); }
    bool requestI2c(uint32_t node_id, bool run) { return requestStackI2c_(node_id, run); }

    StackOwCache &owLocal() { return _stack_ow_cache[0]; }
    const StackOwCache &owLocal() const { return _stack_ow_cache[0]; }
    StackOwCache *owCache(uint32_t node_id) { return findStackOwCache_(node_id, false); }
    const StackOwCache *owCache(uint32_t node_id) const { return findStackOwCache_(node_id, false); }
    bool requestOw(uint32_t node_id, bool run) { return requestStackOw_(node_id, run); }

    StackSecurityCache &securityLocal() { return _stack_security_cache[0]; }
    const StackSecurityCache &securityLocal() const { return _stack_security_cache[0]; }
    StackSecurityCache *securityCache(uint32_t node_id) { return findStackSecurityCache_(node_id, false); }
    const StackSecurityCache *securityCache(uint32_t node_id) const { return findStackSecurityCache_(node_id, false); }
    bool requestSecurity(uint32_t node_id) { return requestStackSecurity_(node_id); }

    StackMeteoCache &meteoLocal() { return _stack_meteo_cache[0]; }
    const StackMeteoCache &meteoLocal() const { return _stack_meteo_cache[0]; }
    size_t meteoCacheSlots() const { return StackMaster::MAX_SESSIONS; }
    StackMeteoCache &meteoCacheAt(size_t idx) { return _stack_meteo_cache[idx]; }
    const StackMeteoCache &meteoCacheAt(size_t idx) const { return _stack_meteo_cache[idx]; }
    StackMeteoCache *meteoCache(uint32_t node_id) { return findStackMeteoCache_(node_id, false); }
    const StackMeteoCache *meteoCache(uint32_t node_id) const { return findStackMeteoCache_(node_id, false); }
    bool requestMeteo(uint32_t node_id) { return requestStackMeteo_(node_id); }

    StackThermoCache &thermoLocal() { return _stack_thermo_cache[0]; }
    const StackThermoCache &thermoLocal() const { return _stack_thermo_cache[0]; }
    StackThermoCache *thermoCache(uint32_t node_id) { return findStackThermoCache_(node_id, false); }
    const StackThermoCache *thermoCache(uint32_t node_id) const { return findStackThermoCache_(node_id, false); }
    bool requestThermo(uint32_t node_id) { return requestStackThermo_(node_id); }

    StackSepticCache &septicLocal() { return _stack_septic_cache[0]; }
    const StackSepticCache &septicLocal() const { return _stack_septic_cache[0]; }
    StackSepticCache *septicCache(uint32_t node_id) { return findStackSepticCache_(node_id, false); }
    const StackSepticCache *septicCache(uint32_t node_id) const { return findStackSepticCache_(node_id, false); }
    bool requestSeptic(uint32_t node_id) { return requestStackSeptic_(node_id); }

    StackTankCache &tanksLocal() { return _stack_tanks_cache[0]; }
    const StackTankCache &tanksLocal() const { return _stack_tanks_cache[0]; }
    StackTankCache *tanksCache(uint32_t node_id) { return findStackTanksCache_(node_id, false); }
    const StackTankCache *tanksCache(uint32_t node_id) const { return findStackTanksCache_(node_id, false); }
    bool requestTanks(uint32_t node_id) { return requestStackTanks_(node_id); }

    StackNodeStatusCache *statusCache(uint32_t node_id) { return findStackNodeStatusCache_(node_id, false); }
    const StackNodeStatusCache *statusCache(uint32_t node_id) const { return findStackNodeStatusCache_(node_id, false); }
    bool requestPlcStatus(uint32_t node_id) { return requestStackPlcStatus_(node_id); }
    bool requestRtcStatus(uint32_t node_id) { return requestStackRtcStatus_(node_id); }

    uint16_t nextCmdId() { return nextStackCmdId_(); }
    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        if (!ctx)
            return;
        static_cast<StackCache *>(ctx)->handleStackFrame_(node_id, frame);
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

    static void *allocMem_(size_t bytes, const char *tag, size_t idx, Logger *log, bool psram_only)
    {
#if defined(ARDUINO_ARCH_ESP32)
        if (psramFound())
        {
            void *ptr = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (ptr)
                return ptr;
            if (log)
            {
                log->warn(F("STACK"), F("PSRAM alloc failed: %s[%u] bytes=%u free=%u largest=%u"),
                          tag,
                          static_cast<unsigned>(idx),
                          static_cast<unsigned>(bytes),
                          (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
            }
        }
        if (psram_only)
            return nullptr;
#endif
        if (psram_only)
        {
            if (log)
                log->error(F("STACK"), F("PSRAM only: %s[%u] bytes=%u (no psram)"),
                           tag, static_cast<unsigned>(idx), static_cast<unsigned>(bytes));
            return nullptr;
        }
        return malloc(bytes);
    }

    static bool isPsram_(const void *ptr)
    {
#if defined(ARDUINO_ARCH_ESP32)
        return ptr && esp_ptr_external_ram(ptr);
#else
        return false;
#endif
    }

    template <typename T>
    static T *allocItems_(size_t count, const char *tag, size_t idx, Logger *log, bool psram_only)
    {
        if (count == 0)
            return nullptr;
        void *mem = allocMem_(sizeof(T) * count, tag, idx, log, psram_only);
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

    void initCaches_()
    {
        for (auto &cache : _stack_sockets_cache)
        {
            cache.items = allocItems_<StackSocketItem>(cache.capacity, "sockets", &cache - _stack_sockets_cache,
                                                       _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_lights_cache)
        {
            cache.items = allocItems_<StackLightItem>(cache.capacity, "lights", &cache - _stack_lights_cache,
                                                      _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_ports_cache)
        {
            cache.items = allocItems_<StackPortItem>(cache.capacity, "ports", &cache - _stack_ports_cache,
                                                     _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_ext_cache)
        {
            cache.items = allocItems_<StackExtenderItem>(cache.capacity, "ext", &cache - _stack_ext_cache,
                                                         _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_i2c_cache)
        {
            cache.items = allocItems_<StackI2cItem>(cache.capacity, "i2c", &cache - _stack_i2c_cache,
                                                    _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_ow_cache)
        {
            cache.items = allocItems_<StackOwItem>(cache.capacity, "ow", &cache - _stack_ow_cache,
                                                   _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_security_cache)
        {
            cache.items = allocItems_<StackSecuritySensorItem>(cache.capacity, "security",
                                                               &cache - _stack_security_cache, _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_meteo_cache)
        {
            cache.items = allocItems_<StackMeteoItem>(cache.capacity, "meteo", &cache - _stack_meteo_cache,
                                                      _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_thermo_cache)
        {
            cache.items = allocItems_<StackThermoItem>(cache.capacity, "thermo",
                                                       &cache - _stack_thermo_cache, _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_septic_cache)
        {
            cache.items = allocItems_<StackSepticItem>(cache.capacity, "septic",
                                                       &cache - _stack_septic_cache, _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_tanks_cache)
        {
            cache.items = allocItems_<StackTankItem>(cache.capacity, "tanks", &cache - _stack_tanks_cache,
                                                     _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
    }

    void releaseCaches_()
    {
        for (auto &cache : _stack_sockets_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_lights_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_ports_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_ext_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_i2c_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_ow_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_security_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_meteo_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_thermo_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_septic_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_tanks_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        _alloc_ready = false;
    }

    void logAllocations_()
    {
        if (_alloc_logged)
            return;
        if (!_log)
            return;
        _alloc_logged = true;
        auto log_fail = [this](const char *name, size_t idx, const void *items, size_t cap) {
            if (cap == 0)
                return;
            if (items)
                return;
            _log->error(F("STACK"), F("Cache alloc failed: %s[%u] items=%u"), name,
                        static_cast<unsigned>(idx), static_cast<unsigned>(cap));
        };
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("sockets", i, _stack_sockets_cache[i].items, _stack_sockets_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("lights", i, _stack_lights_cache[i].items, _stack_lights_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("ports", i, _stack_ports_cache[i].items, _stack_ports_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("ext", i, _stack_ext_cache[i].items, _stack_ext_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("i2c", i, _stack_i2c_cache[i].items, _stack_i2c_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("ow", i, _stack_ow_cache[i].items, _stack_ow_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("security", i, _stack_security_cache[i].items, _stack_security_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("meteo", i, _stack_meteo_cache[i].items, _stack_meteo_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("thermo", i, _stack_thermo_cache[i].items, _stack_thermo_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("septic", i, _stack_septic_cache[i].items, _stack_septic_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("tanks", i, _stack_tanks_cache[i].items, _stack_tanks_cache[i].capacity);
    }

    void handleStackFrame_(uint32_t node_id, const StackFrame &frame)
    {
        if (frame.type == (uint8_t)StackMsgType::CmdGet)
        {
            if (!_stack_master || stackRole_() != ConfigsManagerIface::StackRole::Master)
                return;
            DynamicJsonDocument req(512);
            DeserializationError err = deserializeJson(req, frame.payload, frame.payload_len);
            if (err)
                return;
            const uint16_t cmd_id = req["cmd_id"] | 0;
            if (_configs)
            {
                const String key = _configs->stackApiKey();
                if (key.length() > 0)
                {
                    const String provided = req["api_key"] | "";
                    if (provided != key)
                    {
                        StaticJsonDocument<192> err_doc;
                        err_doc["cmd_id"] = cmd_id;
                        err_doc["ok"] = false;
                        err_doc["error"] = "auth";
                        char payload[128] = {};
                        const size_t len = serializeJson(err_doc, payload, sizeof(payload));
                        if (len > 0)
                            _stack_master->sendTo(node_id, (uint8_t)StackMsgType::Err,
                                                  (const uint8_t *)payload, len);
                        return;
                    }
                }
            }
            const uint8_t feature = (uint8_t)(req["feature"] | 0);
            String action = req["action"] | "";
            action.toLowerCase();
            JsonVariantConst params = req["params"];
            const bool all = params["all"] | false;
            uint32_t target = params["node"] | 0u;
            if (target == 0)
                target = params["node_id"] | 0u;
            if (target == 0)
                target = node_id;

            auto send_err = [&](const char *err_text) {
                StaticJsonDocument<192> err_doc;
                err_doc["cmd_id"] = cmd_id;
                err_doc["ok"] = false;
                err_doc["error"] = err_text;
                char payload[128] = {};
                const size_t len = serializeJson(err_doc, payload, sizeof(payload));
                if (len > 0)
                    _stack_master->sendTo(node_id, (uint8_t)StackMsgType::Err,
                                          (const uint8_t *)payload, len);
            };

            auto send_ok = [&](JsonDocument &data) {
                DynamicJsonDocument out(4096);
                out["cmd_id"] = cmd_id;
                out["ok"] = true;
                out["data"] = data.as<JsonVariantConst>();
                char payload[StackCodec::kMaxPayload] = {};
                const size_t len = serializeJson(out, payload, sizeof(payload));
                if (len > 0)
                    _stack_master->sendTo(node_id, (uint8_t)StackMsgType::Ack,
                                          (const uint8_t *)payload, len);
            };

            if ((StackFeature)feature == StackFeature::Meteo && action == "get")
            {
                if (all)
                {
                    DynamicJsonDocument data(4096);
                    JsonArray nodes = data["nodes"].to<JsonArray>();
                    for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
                    {
                        const StackMeteoCache &cache = _stack_meteo_cache[i];
                        if (cache.node_id == 0 || !cache.has_data || !cache.items)
                            continue;
                        String node_name;
                        if (_stack_master)
                        {
                            String ip;
                            uint16_t fw_ver = 0;
                            if (_stack_master->nodeInfo(cache.node_id, node_name, ip, fw_ver))
                            {
                            }
                        }
                        JsonObject n = nodes.add<JsonObject>();
                        n["node_id"] = (unsigned long)cache.node_id;
                        if (node_name.length())
                            n["node_name"] = node_name;
                        JsonArray items = n["items"].to<JsonArray>();
                        for (size_t j = 0; j < cache.item_count; ++j)
                        {
                            const StackMeteoItem &it = cache.items[j];
                            JsonObject o = items.add<JsonObject>();
                            o["id"] = (unsigned)it.id;
                            o["enabled"] = it.enabled;
                            if (it.name[0])
                                o["name"] = it.name;
                            if (it.has_temp)
                                o["temp_c"] = it.temp_c;
                            if (it.has_hum)
                                o["hum"] = it.hum;
                            o["has_temp"] = it.has_temp;
                            o["has_hum"] = it.has_hum;
                            o["ok"] = it.ok;
                        }
                    }
                    send_ok(data);
                    return;
                }
                StackMeteoCache *cache = findStackMeteoCache_(target, false);
                if (!cache || !cache->has_data || !cache->items)
                {
                    requestStackMeteo_(target);
                    send_err("no_data");
                    return;
                }
                String node_name;
                if (_stack_master)
                {
                    String ip;
                    uint16_t fw_ver = 0;
                    if (_stack_master->nodeInfo(target, node_name, ip, fw_ver))
                    {
                    }
                }
                DynamicJsonDocument data(4096);
                if (node_name.length())
                    data["node_name"] = node_name;
                JsonArray items = data["items"].to<JsonArray>();
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const StackMeteoItem &it = cache->items[i];
                    JsonObject o = items.add<JsonObject>();
                    o["id"] = (unsigned)it.id;
                    o["enabled"] = it.enabled;
                    if (it.name[0])
                        o["name"] = it.name;
                    if (it.has_temp)
                        o["temp_c"] = it.temp_c;
                    if (it.has_hum)
                        o["hum"] = it.hum;
                    o["has_temp"] = it.has_temp;
                    o["has_hum"] = it.has_hum;
                    o["ok"] = it.ok;
                }
                send_ok(data);
                return;
            }
            if ((StackFeature)feature == StackFeature::Security && action == "get")
            {
                StackSecurityCache *cache = findStackSecurityCache_(target, false);
                if (!cache || !cache->has_data || !cache->items)
                {
                    requestStackSecurity_(target);
                    send_err("no_data");
                    return;
                }
                DynamicJsonDocument data(2048);
                data["enabled"] = cache->enabled;
                data["armed"] = cache->armed;
                data["alarm"] = cache->alarm;
                JsonArray items = data["items"].to<JsonArray>();
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const StackSecuritySensorItem &it = cache->items[i];
                    JsonObject o = items.add<JsonObject>();
                    o["id"] = (unsigned)it.id;
                    o["enabled"] = it.enabled;
                    o["detect"] = it.detect;
                    o["silent"] = it.silent;
                    if (it.port != SecurityController::kInvalidPort)
                        o["port"] = it.port;
                    if (it.type[0])
                        o["type"] = it.type;
                    if (it.name[0])
                        o["name"] = it.name;
                }
                send_ok(data);
                return;
            }
            if ((StackFeature)feature == StackFeature::Sockets &&
                (action == "get" || action == "get_lights"))
            {
                const bool lights = (action == "get_lights");
                if (lights)
                {
                    StackLightsCache *cache = findStackLightsCache_(target, false);
                    if (!cache || !cache->has_data || !cache->items)
                    {
                        requestStackLights_(target);
                        send_err("no_data");
                        return;
                    }
                    DynamicJsonDocument data(2048);
                    JsonArray items = data["items"].to<JsonArray>();
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const StackLightItem &it = cache->items[i];
                        JsonObject o = items.add<JsonObject>();
                        o["id"] = (unsigned)it.id;
                        o["enabled"] = it.enabled;
                        o["state"] = it.state;
                        if (it.name[0])
                            o["name"] = it.name;
                    }
                    send_ok(data);
                    return;
                }
                StackSocketsCache *cache = findStackSocketsCache_(target, false);
                if (!cache || !cache->has_data || !cache->items)
                {
                    requestStackSockets_(target);
                    send_err("no_data");
                    return;
                }
                DynamicJsonDocument data(2048);
                JsonArray items = data["items"].to<JsonArray>();
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const StackSocketItem &it = cache->items[i];
                    JsonObject o = items.add<JsonObject>();
                    o["id"] = (unsigned)it.id;
                    o["enabled"] = it.enabled;
                    o["state"] = it.state;
                    if (it.name[0])
                        o["name"] = it.name;
                }
                send_ok(data);
                return;
            }
            if ((StackFeature)feature == StackFeature::Septic && action == "get")
            {
                StackSepticCache *cache = findStackSepticCache_(target, false);
                if (!cache || !cache->has_data || !cache->items)
                {
                    requestStackSeptic_(target);
                    send_err("no_data");
                    return;
                }
                DynamicJsonDocument data(2048);
                JsonArray items = data["items"].to<JsonArray>();
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const StackSepticItem &it = cache->items[i];
                    JsonObject o = items.add<JsonObject>();
                    o["id"] = (unsigned)it.id;
                    o["enabled"] = it.enabled;
                    o["warning"] = it.warning;
                    o["alarm"] = it.alarm;
                }
                send_ok(data);
                return;
            }
            if ((StackFeature)feature == StackFeature::Tanks && action == "get")
            {
                StackTankCache *cache = findStackTanksCache_(target, false);
                if (!cache || !cache->has_data || !cache->items)
                {
                    requestStackTanks_(target);
                    send_err("no_data");
                    return;
                }
                DynamicJsonDocument data(2048);
                JsonArray items = data["items"].to<JsonArray>();
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const StackTankItem &it = cache->items[i];
                    JsonObject o = items.add<JsonObject>();
                    o["id"] = (unsigned)it.id;
                    o["enabled"] = it.enabled;
                    o["levels_ok"] = it.levels_ok;
                    o["level_low"] = it.level_low;
                    o["level_mid"] = it.level_mid;
                    o["level_full"] = it.level_full;
                }
                send_ok(data);
                return;
            }
            return;
        }
        if (frame.type != (uint8_t)StackMsgType::Ack &&
            frame.type != (uint8_t)StackMsgType::Err)
            return;
        DynamicJsonDocument doc(4096);
        DeserializationError err = deserializeJson(doc, frame.payload, frame.payload_len);
        if (err)
            return;
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        StackSocketsCache *sock_cache = findStackSocketsCacheByCmd_(cmd_id);
        StackLightsCache *light_cache = findStackLightsCacheByCmd_(cmd_id);
        StackPortsCache *ports_cache = findStackPortsCacheByCmd_(cmd_id);
        StackExtendersCache *ext_cache = findStackExtendersCacheByCmd_(cmd_id);
        StackSecurityCache *sec_cache = findStackSecurityCacheByCmd_(cmd_id);
        StackMeteoCache *meteo_cache = findStackMeteoCacheByCmd_(cmd_id);
        StackThermoCache *thermo_cache = findStackThermoCacheByCmd_(cmd_id);
        StackSepticCache *septic_cache = findStackSepticCacheByCmd_(cmd_id);
        StackTankCache *tanks_cache = findStackTanksCacheByCmd_(cmd_id);
        StackI2cCache *i2c_cache = findStackI2cCacheByCmd_(cmd_id);
        StackOwCache *ow_cache = findStackOwCacheByCmd_(cmd_id);
        bool status_is_plc = false;
        bool status_is_rtc = false;
        StackNodeStatusCache *status_cache = findStackNodeStatusCacheByCmd_(cmd_id, status_is_plc, status_is_rtc);
        if (!sock_cache && !light_cache && !ports_cache && !ext_cache && !sec_cache && !meteo_cache &&
            !thermo_cache && !septic_cache && !tanks_cache && !i2c_cache && !ow_cache && !status_cache)
            return;
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (doc["ok"] | false);
        JsonArrayConst items = doc["data"]["items"].as<JsonArrayConst>();

        if (sock_cache)
        {
            sock_cache->pending = false;
            sock_cache->updated_ms = millis();
            sock_cache->last_ok = false;
            sock_cache->last_error = "";
            if (!ok)
            {
                sock_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                sock_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (sock_cache->item_count >= SocketController::kSocketCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackSocketItem &dst = sock_cache->items[sock_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.state = item["state"] | false;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                }
                sock_cache->has_data = true;
                sock_cache->last_ok = true;
                sock_cache->node_id = node_id;
            }
        }

        if (light_cache)
        {
            light_cache->pending = false;
            light_cache->updated_ms = millis();
            light_cache->last_ok = false;
            light_cache->last_error = "";
            if (!ok)
            {
                light_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                light_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (light_cache->item_count >= SocketController::kLightCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackLightItem &dst = light_cache->items[light_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.state = item["state"] | false;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                }
                light_cache->has_data = true;
                light_cache->last_ok = true;
                light_cache->node_id = node_id;
            }
        }

        if (ports_cache)
        {
            ports_cache->pending = false;
            ports_cache->updated_ms = millis();
            ports_cache->last_ok = false;
            ports_cache->last_error = "";
            if (!ok)
            {
                ports_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                ports_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (ports_cache->item_count >= PortIO::PORT_COUNT)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackPortItem &dst = ports_cache->items[ports_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.ctrl = item["ctrl"] | false;
                    dst.is_extender = item["ext"] | false;
                    dst.dev = item["dev"] | -1;
                    dst.pin = item["pin"] | -1;
                    copyStr_(dst.backend, sizeof(dst.backend), item["backend"].as<const char *>());
                    copyStr_(dst.loc, sizeof(dst.loc), item["loc"].as<const char *>());
                    copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                    copyStr_(dst.hw, sizeof(dst.hw), item["hw"].as<const char *>());
                }
                ports_cache->has_data = true;
                ports_cache->last_ok = true;
                ports_cache->node_id = node_id;
            }
        }

        if (ext_cache)
        {
            ext_cache->pending = false;
            ext_cache->updated_ms = millis();
            ext_cache->last_ok = false;
            ext_cache->last_error = "";
            if (!ok)
            {
                ext_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                ext_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (ext_cache->item_count >= Extender::MAX_DEVS)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackExtenderItem &dst = ext_cache->items[ext_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.bus = (uint8_t)(item["bus"] | 0u);
                    dst.present = item["present"] | false;
                    copyStr_(dst.addr, sizeof(dst.addr), item["addr"].as<const char *>());
                    copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                }
                ext_cache->has_data = true;
                ext_cache->last_ok = true;
                ext_cache->node_id = node_id;
            }
        }

        if (sec_cache)
        {
            sec_cache->pending = false;
            sec_cache->updated_ms = millis();
            sec_cache->last_ok = false;
            sec_cache->last_error = "";
            if (!ok)
            {
                sec_cache->last_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"];
                sec_cache->enabled = data["enabled"] | false;
                sec_cache->armed = data["armed"] | false;
                sec_cache->alarm = data["alarm"] | false;
                if (!items.isNull())
                {
                    sec_cache->item_count = 0;
                    for (JsonObjectConst item : items)
                    {
                        if (sec_cache->item_count >= SecurityController::kSensorCount)
                            break;
                        if (!item["id"].is<unsigned>())
                            continue;
                        StackSecuritySensorItem &dst = sec_cache->items[sec_cache->item_count++];
                        dst.id = (uint8_t)item["id"].as<unsigned>();
                        dst.enabled = item["enabled"] | false;
                        dst.detect = item["detect"] | false;
                        dst.silent = item["silent"] | false;
                        dst.port = (uint8_t)(item["port"] | SecurityController::kInvalidPort);
                        copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                        copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    }
                }
                sec_cache->has_data = true;
                sec_cache->last_ok = true;
                sec_cache->node_id = node_id;
            }
        }

        if (meteo_cache)
        {
            meteo_cache->pending = false;
            meteo_cache->updated_ms = millis();
            meteo_cache->last_ok = false;
            meteo_cache->last_error = "";
            if (!ok)
            {
                meteo_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                meteo_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (meteo_cache->item_count >= MeteoController::kSensorCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackMeteoItem &dst = meteo_cache->items[meteo_cache->item_count++];
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
                    if (item["pin"].is<int>() || item["pin"].is<unsigned>())
                        dst.pin = item["pin"].as<int>();
                    else
                        dst.pin = -1;
                }
                meteo_cache->has_data = true;
                meteo_cache->last_ok = true;
                meteo_cache->node_id = node_id;
            }
        }

        if (thermo_cache)
        {
            thermo_cache->pending = false;
            thermo_cache->updated_ms = millis();
            thermo_cache->last_ok = false;
            thermo_cache->last_error = "";
            if (!ok)
            {
                thermo_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                thermo_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (thermo_cache->item_count >= ThermoController::kDeviceCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackThermoItem &dst = thermo_cache->items[thermo_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.power_on = item["power_on"] | false;
                    dst.heat_on = item["heat_on"] | false;
                    dst.cool_on = item["cool_on"] | false;
                    dst.sensor = (uint8_t)(item["sensor"] | 0u);
                    dst.sensor_node = (uint32_t)(item["sensor_node"] | 0u);
                    dst.target = item["target"] | 0.0f;
                    dst.hyst = item["hyst"] | 0.0f;
                    dst.heat = (uint8_t)(item["heat"] | ThermoController::kInvalidPort);
                    dst.cool = (uint8_t)(item["cool"] | ThermoController::kInvalidPort);
                    dst.button = (uint8_t)(item["button"] | ThermoController::kInvalidPort);
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    copyStr_(dst.mode, sizeof(dst.mode), item["mode"].as<const char *>());
                }
                thermo_cache->has_data = true;
                thermo_cache->last_ok = true;
                thermo_cache->node_id = node_id;
            }
        }

        if (septic_cache)
        {
            septic_cache->pending = false;
            septic_cache->updated_ms = millis();
            septic_cache->last_ok = false;
            septic_cache->last_error = "";
            if (!ok)
            {
                septic_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                septic_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (septic_cache->item_count >= SepticController::kSepticCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackSepticItem &dst = septic_cache->items[septic_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.monitor = item["monitor"] | false;
                    dst.warning_port = (uint8_t)(item["warning_port"] | SepticController::kInvalidPort);
                    dst.alarm_port = (uint8_t)(item["alarm_port"] | SepticController::kInvalidPort);
                    dst.relay_warning = (uint8_t)(item["relay_warning"] | SepticController::kInvalidPort);
                    dst.relay_alarm = (uint8_t)(item["relay_alarm"] | SepticController::kInvalidPort);
                    dst.warning = item["warning"] | false;
                    dst.alarm = item["alarm"] | false;
                }
                septic_cache->has_data = true;
                septic_cache->last_ok = true;
                septic_cache->node_id = node_id;
            }
        }

        if (tanks_cache)
        {
            tanks_cache->pending = false;
            tanks_cache->updated_ms = millis();
            tanks_cache->last_ok = false;
            tanks_cache->last_error = "";
            if (!ok)
            {
                tanks_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                tanks_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (tanks_cache->item_count >= TankController::kTankCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackTankItem &dst = tanks_cache->items[tanks_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.power_on = item["power_on"] | false;
                    dst.low = (uint8_t)(item["low"] | TankController::kInvalidPort);
                    dst.mid = (uint8_t)(item["mid"] | TankController::kInvalidPort);
                    dst.full = (uint8_t)(item["full"] | TankController::kInvalidPort);
                    dst.valve = (uint8_t)(item["valve"] | TankController::kInvalidPort);
                    dst.pump = (uint8_t)(item["pump"] | TankController::kInvalidPort);
                    dst.alarm = (uint8_t)(item["alarm"] | TankController::kInvalidPort);
                    dst.level_low = item["level_low"] | false;
                    dst.level_mid = item["level_mid"] | false;
                    dst.level_full = item["level_full"] | false;
                    dst.levels_ok = item["levels_ok"] | false;
                    dst.valve_on = item["valve_on"] | false;
                    dst.pump_on = item["pump_on"] | false;
                    dst.alarm_on = item["alarm_on"] | false;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                }
                tanks_cache->has_data = true;
                tanks_cache->last_ok = true;
                tanks_cache->node_id = node_id;
            }
        }

        if (i2c_cache)
        {
            i2c_cache->pending = false;
            i2c_cache->updated_ms = millis();
            i2c_cache->last_ok = false;
            i2c_cache->last_error = "";
            if (!ok)
            {
                i2c_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                i2c_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (i2c_cache->item_count >= 127)
                        break;
                    if (!item["bus"].is<unsigned>() || !item["addr"].is<unsigned>())
                        continue;
                    StackI2cItem &dst = i2c_cache->items[i2c_cache->item_count++];
                    dst.bus = (uint8_t)item["bus"].as<unsigned>();
                    dst.addr = (uint8_t)item["addr"].as<unsigned>();
                }
                i2c_cache->has_data = true;
                i2c_cache->last_ok = true;
                i2c_cache->node_id = node_id;
            }
        }

        if (ow_cache)
        {
            ow_cache->pending = false;
            ow_cache->updated_ms = millis();
            ow_cache->last_ok = false;
            ow_cache->last_error = "";
            if (!ok)
            {
                ow_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                ow_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (ow_cache->item_count >= 64)
                        break;
                    if (!item["bus"].is<unsigned>() || !item["addr"].is<const char *>() ||
                        !item["type"].is<const char *>())
                        continue;
                    StackOwItem &dst = ow_cache->items[ow_cache->item_count++];
                    dst.bus = (uint8_t)item["bus"].as<unsigned>();
                    strlcpy(dst.addr, item["addr"].as<const char *>(), sizeof(dst.addr));
                    strlcpy(dst.type, item["type"].as<const char *>(), sizeof(dst.type));
                }
                ow_cache->has_data = true;
                ow_cache->last_ok = true;
                ow_cache->node_id = node_id;
            }
        }

        if (status_cache && status_is_plc)
        {
            status_cache->pending_plc = false;
            status_cache->plc_updated_ms = millis();
            status_cache->last_plc_ok = false;
            status_cache->last_plc_error = "";
            if (!ok)
            {
                status_cache->last_plc_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"];
                status_cache->board_temp = data["board_temp"] | 0.0f;
                status_cache->cpu_temp = data["cpu_temp"] | 0.0f;
                status_cache->fan_on = data["fan_on"] | false;
                status_cache->fan_on_c = data["fan_on_c"] | 0.0f;
                status_cache->fan_hyst_c = data["fan_hyst_c"] | 0.0f;
                status_cache->has_plc = true;
                status_cache->last_plc_ok = true;
                status_cache->node_id = node_id;
            }
        }

        if (status_cache && status_is_rtc)
        {
            status_cache->pending_rtc = false;
            status_cache->rtc_updated_ms = millis();
            status_cache->last_rtc_ok = false;
            status_cache->last_rtc_error = "";
            if (!ok)
            {
                status_cache->last_rtc_error = doc["error"] | "error";
            }
            else
            {
                JsonObjectConst data = doc["data"];
                status_cache->rtc_date = data["rtc_date"] | "";
                status_cache->rtc_time = data["rtc_time"] | "";
                status_cache->rtc_temp = data["rtc_temp"] | 0.0f;
                status_cache->rtc_weekday = (uint8_t)(data["rtc_weekday"] | 0u);
                status_cache->has_rtc = true;
                status_cache->last_rtc_ok = true;
                status_cache->node_id = node_id;
            }
        }
    }

private:
    bool requestStackSockets_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSocketsCache *cache = findStackSocketsCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackLights_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackLightsCache *cache = findStackLightsCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "get_lights";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackSecurity_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSecurityCache *cache = findStackSecurityCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackMeteo_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackMeteoCache *cache = findStackMeteoCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Meteo;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }
    bool requestStackThermo_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackThermoCache *cache = findStackThermoCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Thermo;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackSeptic_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSepticCache *cache = findStackSepticCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Septic;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackTanks_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackTankCache *cache = findStackTanksCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Tanks;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackPorts_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackPortsCache *cache = findStackPortsCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Ports;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackExtenders_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackExtendersCache *cache = findStackExtendersCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Extenders;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackI2c_(uint32_t node_id, bool run)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackI2cCache *cache = findStackI2cCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u && !run)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::I2cScan;
        doc["action"] = "get";
        if (run)
            doc["action"] = "run";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }

    bool requestStackOw_(uint32_t node_id, bool run)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackOwCache *cache = findStackOwCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u && !run)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::OwScan;
        doc["action"] = "get";
        if (run)
            doc["action"] = "run";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        return true;
    }
    bool requestStackPlcStatus_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackNodeStatusCache *cache = findStackNodeStatusCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending_plc)
            return false;
        if (cache->has_plc && (uint32_t)(now - cache->plc_updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Rtc;
        doc["action"] = "get_plc";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending_plc = true;
        cache->pending_plc_cmd_id = cmd_id;
        return true;
    }

    bool requestStackRtcStatus_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackNodeStatusCache *cache = findStackNodeStatusCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending_rtc)
            return false;
        if (cache->has_rtc && (uint32_t)(now - cache->rtc_updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::PlcStatus;
        doc["action"] = "get_rtc";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending_rtc = true;
        cache->pending_rtc_cmd_id = cmd_id;
        return true;
    }

    StackSocketsCache *findStackSocketsCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_sockets_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_sockets_cache)
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

    const StackSocketsCache *findStackSocketsCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackSocketsCache_(node_id, create);
    }

    StackSocketsCache *findStackSocketsCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_sockets_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackSocketItem *findStackSocketItem_(StackSocketsCache &cache, uint8_t id)
    {
        if (id == 0)
            return nullptr;
        for (size_t i = 0; i < cache.item_count; ++i)
            if (cache.items[i].id == id)
                return &cache.items[i];
        return nullptr;
    }

    StackLightsCache *findStackLightsCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_lights_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_lights_cache)
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

    const StackLightsCache *findStackLightsCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackLightsCache_(node_id, create);
    }

    StackLightsCache *findStackLightsCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_lights_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackPortsCache *findStackPortsCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_ports_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_ports_cache)
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

    const StackPortsCache *findStackPortsCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackPortsCache_(node_id, create);
    }

    StackPortsCache *findStackPortsCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_ports_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }
    StackExtendersCache *findStackExtendersCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_ext_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_ext_cache)
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

    const StackExtendersCache *findStackExtendersCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackExtendersCache_(node_id, create);
    }

    StackExtendersCache *findStackExtendersCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_ext_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackI2cCache *findStackI2cCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_i2c_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_i2c_cache)
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

    const StackI2cCache *findStackI2cCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackI2cCache_(node_id, create);
    }

    StackI2cCache *findStackI2cCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_i2c_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackOwCache *findStackOwCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_ow_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_ow_cache)
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

    const StackOwCache *findStackOwCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackOwCache_(node_id, create);
    }

    StackOwCache *findStackOwCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_ow_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackLightItem *findStackLightItem_(StackLightsCache &cache, uint8_t id)
    {
        if (id == 0)
            return nullptr;
        for (size_t i = 0; i < cache.item_count; ++i)
            if (cache.items[i].id == id)
                return &cache.items[i];
        return nullptr;
    }

    StackSecurityCache *findStackSecurityCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_security_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_security_cache)
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

    const StackSecurityCache *findStackSecurityCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackSecurityCache_(node_id, create);
    }

    StackSecurityCache *findStackSecurityCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_security_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackMeteoCache *findStackMeteoCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_meteo_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_meteo_cache)
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

    const StackMeteoCache *findStackMeteoCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackMeteoCache_(node_id, create);
    }

    StackMeteoCache *findStackMeteoCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_meteo_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackThermoCache *findStackThermoCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_thermo_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_thermo_cache)
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

    const StackThermoCache *findStackThermoCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackThermoCache_(node_id, create);
    }

    StackThermoCache *findStackThermoCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_thermo_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackSepticCache *findStackSepticCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_septic_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_septic_cache)
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

    const StackSepticCache *findStackSepticCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackSepticCache_(node_id, create);
    }

    StackSepticCache *findStackSepticCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_septic_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackTankCache *findStackTanksCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_tanks_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_tanks_cache)
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

    const StackTankCache *findStackTanksCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackTanksCache_(node_id, create);
    }

    StackTankCache *findStackTanksCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_tanks_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackNodeStatusCache *findStackNodeStatusCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_status_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_status_cache)
        {
            if (c.node_id == 0)
            {
                c = StackNodeStatusCache{};
                c.node_id = node_id;
                return &c;
            }
        }
        return nullptr;
    }

    const StackNodeStatusCache *findStackNodeStatusCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackNodeStatusCache_(node_id, create);
    }

    StackNodeStatusCache *findStackNodeStatusCacheByCmd_(uint16_t cmd_id, bool &is_plc, bool &is_rtc)
    {
        is_plc = false;
        is_rtc = false;
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_status_cache)
        {
            if (c.pending_plc && c.pending_plc_cmd_id == cmd_id)
            {
                is_plc = true;
                return &c;
            }
            if (c.pending_rtc && c.pending_rtc_cmd_id == cmd_id)
            {
                is_rtc = true;
                return &c;
            }
        }
        return nullptr;
    }

    uint16_t nextStackCmdId_()
    {
        ++_stack_cmd_id;
        if (_stack_cmd_id == 0)
            _stack_cmd_id = 1;
        return _stack_cmd_id;
    }

private:
    ConfigsManagerIface::StackRole stackRole_() const
    {
        if (_force_master)
            return ConfigsManagerIface::StackRole::Master;
        if (!_configs)
            return ConfigsManagerIface::StackRole::Master;
        return _configs->stackRole();
    }

    StackMaster *_stack_master = nullptr;
    ConfigsManagerIface *_configs = nullptr;
    Logger *_log = nullptr;
    bool _force_master = false;
    bool _alloc_ready = false;
    bool _alloc_logged = false;
    StackSocketsCache _stack_sockets_cache[StackMaster::MAX_SESSIONS] = {};
    StackLightsCache _stack_lights_cache[StackMaster::MAX_SESSIONS] = {};
    StackPortsCache _stack_ports_cache[StackMaster::MAX_SESSIONS] = {};
    StackExtendersCache _stack_ext_cache[StackMaster::MAX_SESSIONS] = {};
    StackI2cCache _stack_i2c_cache[StackMaster::MAX_SESSIONS] = {};
    StackOwCache _stack_ow_cache[StackMaster::MAX_SESSIONS] = {};
    StackSecurityCache _stack_security_cache[StackMaster::MAX_SESSIONS] = {};
    StackMeteoCache _stack_meteo_cache[StackMaster::MAX_SESSIONS] = {};
    StackThermoCache _stack_thermo_cache[StackMaster::MAX_SESSIONS] = {};
    StackSepticCache _stack_septic_cache[StackMaster::MAX_SESSIONS] = {};
    StackTankCache _stack_tanks_cache[StackMaster::MAX_SESSIONS] = {};
    StackNodeStatusCache _stack_status_cache[StackMaster::MAX_SESSIONS] = {};
    uint16_t _stack_cmd_id = 0;
};
