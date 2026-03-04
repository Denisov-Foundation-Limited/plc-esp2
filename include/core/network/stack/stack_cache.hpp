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
    static constexpr uint16_t kWateringPageSize = 1;
    // Types
    struct StackSocketItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool state = false;
        uint8_t button_port = SocketController::kInvalidPort;
        uint8_t relay_port = SocketController::kInvalidPort;
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
        uint8_t button_port = SocketController::kInvalidPort;
        uint8_t relay_port = SocketController::kInvalidPort;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackLightsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint32_t pending_since_ms = 0;
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
                items[i] = StackLightItem{};
        }
    };
    struct StackPortItem
    {
        uint8_t id = 0;
        bool ctrl = false;
        bool used = false;
        bool is_extender = false;
        uint8_t pin_type = 0xFF;
        int16_t dev = -1;
        int16_t pin = -1;
        static constexpr size_t kBackendLen = 32;
        static constexpr size_t kLocLen = 32;
        static constexpr size_t kTypeLen = 32;
        static constexpr size_t kHwLen = 32;
        static constexpr size_t kAliasLen = 24;
        char backend[kBackendLen] = {};
        char loc[kLocLen] = {};
        char type[kTypeLen] = {};
        char hw[kHwLen] = {};
        char alias[kAliasLen] = {};
    };
    struct StackPortsCache
    {
        static constexpr size_t kMaxPartsTracked = 128;
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        uint16_t parts_expected = 0;
        uint16_t parts_received = 0;
        uint16_t next_offset = 0;
        uint16_t page_limit = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackPortItem *items = nullptr;
        size_t capacity = PortIO::PORT_COUNT;
        size_t item_count = 0;
        uint8_t part_seen[kMaxPartsTracked] = {};
        uint8_t present[PortIO::PORT_COUNT] = {};
        void reset()
        {
            node_id = 0;
            updated_ms = 0;
            pending_cmd_id = 0;
            parts_expected = 0;
            parts_received = 0;
            next_offset = 0;
            page_limit = 0;
            pending = false;
            has_data = false;
            last_ok = false;
            last_error = String();
            item_count = 0;
            memset(part_seen, 0, sizeof(part_seen));
            memset(present, 0, sizeof(present));
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
    struct StackTempSensorItem
    {
        char addr[17] = {};
        bool used = false;
    };
    struct StackTempSensorsCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        uint16_t next_offset = 0;
        uint16_t page_limit = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackTempSensorItem *items = nullptr;
        size_t capacity = 64;
        size_t item_count = 0;
        void reset()
        {
            node_id = 0;
            updated_ms = 0;
            pending_cmd_id = 0;
            next_offset = 0;
            page_limit = 0;
            pending = false;
            has_data = false;
            last_ok = false;
            last_error = String();
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = StackTempSensorItem{};
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
    struct StackSecurityPrearmItem
    {
        uint8_t id = 0;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackSecurityPrearmCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint32_t pending_since_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackSecurityPrearmItem *items = nullptr;
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
            item_count = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = StackSecurityPrearmItem{};
        }
    };
    struct StackSecurityCache
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
        uint8_t siren = SecurityController::kInvalidPort;
        StackSecuritySensorItem *items = nullptr;
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
            siren = SecurityController::kInvalidPort;
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
        bool has_read = false;
        float temp_c = 0.0f;
        float hum = 0.0f;
        uint32_t age_s = 0;
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
        uint32_t pending_since_ms = 0;
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
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackSepticCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint32_t pending_since_ms = 0;
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
        uint32_t pending_since_ms = 0;
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
                items[i] = StackTankItem{};
        }
    };
    struct StackWateringItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool status = false;
        uint8_t port = WateringController::kInvalidPort;
        uint8_t tank_id = 0;
        uint8_t weekdays_mask = 0;
        uint8_t hour = 0;
        uint8_t minute = 0;
        uint32_t duration_sec = 0;
        uint8_t hour2 = 0;
        uint8_t minute2 = 0;
        uint32_t duration2_sec = 0;
        uint8_t hour3 = 0;
        uint8_t minute3 = 0;
        uint32_t duration3_sec = 0;
        bool resume_after_refill = false;
        uint8_t resume_level = 0;
        bool active = false;
        bool paused = false;
        uint32_t remaining_ms = 0;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackWateringCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackWateringItem *items = nullptr;
        size_t capacity = WateringController::kRuleCount;
        size_t item_count = 0;
        uint16_t total_expected = 0;
        uint16_t next_offset = 0;
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
            total_expected = 0;
            next_offset = 0;
            if (!items)
                return;
            for (size_t i = 0; i < capacity; ++i)
                items[i] = StackWateringItem{};
        }
    };
    struct StackAvrCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        bool enabled = false;
        bool auto_mode = true;
        bool prefer_main = true;
        bool auto_return_main = true;
        bool main_ok = false;
        bool reserve_ok = false;
        bool relay_main_on = false;
        bool relay_reserve_on = false;
        bool transfer = false;
        uint8_t main_ok_port = AvrController::kInvalidPort;
        uint8_t reserve_ok_port = AvrController::kInvalidPort;
        uint8_t relay_main_port = AvrController::kInvalidPort;
        uint8_t relay_reserve_port = AvrController::kInvalidPort;
        uint8_t feedback_main_port = AvrController::kInvalidPort;
        uint8_t feedback_reserve_port = AvrController::kInvalidPort;
        static constexpr size_t kSourceLen = 16;
        static constexpr size_t kFaultLen = 32;
        char active_source[kSourceLen] = {};
        char target_source[kSourceLen] = {};
        char fault[kFaultLen] = {};
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
            auto_mode = true;
            prefer_main = true;
            auto_return_main = true;
            main_ok = false;
            reserve_ok = false;
            relay_main_on = false;
            relay_reserve_on = false;
            transfer = false;
            main_ok_port = AvrController::kInvalidPort;
            reserve_ok_port = AvrController::kInvalidPort;
            relay_main_port = AvrController::kInvalidPort;
            relay_reserve_port = AvrController::kInvalidPort;
            feedback_main_port = AvrController::kInvalidPort;
            feedback_reserve_port = AvrController::kInvalidPort;
            active_source[0] = '\0';
            target_source[0] = '\0';
            fault[0] = '\0';
        }
    };
    struct StackLeakItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool power_on = false;
        bool sensor_active_low = true;
        uint8_t sensor = LeakController::kInvalidPort;
        uint8_t valve = LeakController::kInvalidPort;
        uint8_t alarm = LeakController::kInvalidPort;
        bool wet = false;
        bool alarm_latched = false;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };
    struct StackLeakCache
    {
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackLeakItem *items = nullptr;
        size_t capacity = LeakController::kZoneCount;
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
                items[i] = StackLeakItem{};
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
    void invalidatePorts(uint32_t node_id) { invalidateStackPortsCache_(node_id); }

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

    StackTempSensorsCache &tempSensorsLocal() { return _stack_temp_sensors_cache[0]; }
    const StackTempSensorsCache &tempSensorsLocal() const { return _stack_temp_sensors_cache[0]; }
    StackTempSensorsCache *tempSensorsCache(uint32_t node_id) { return findStackTempSensorsCache_(node_id, false); }
    const StackTempSensorsCache *tempSensorsCache(uint32_t node_id) const { return findStackTempSensorsCache_(node_id, false); }
    bool requestTempSensors(uint32_t node_id) { return requestStackTempSensors_(node_id); }
    void invalidateTempSensors(uint32_t node_id) { invalidateStackTempSensorsCache_(node_id); }

    StackSecurityCache &securityLocal() { return _stack_security_cache[0]; }
    const StackSecurityCache &securityLocal() const { return _stack_security_cache[0]; }
    StackSecurityCache *securityCache(uint32_t node_id) { return findStackSecurityCache_(node_id, false); }
    const StackSecurityCache *securityCache(uint32_t node_id) const { return findStackSecurityCache_(node_id, false); }
    StackSecurityPrearmCache *securityPrearmCache(uint32_t node_id) { return findStackSecurityPrearmCache_(node_id, false); }
    const StackSecurityPrearmCache *securityPrearmCache(uint32_t node_id) const { return findStackSecurityPrearmCache_(node_id, false); }
    bool requestSecurity(uint32_t node_id) { return requestStackSecurity_(node_id); }
    bool requestSecurityPrearm(uint32_t node_id) { return requestStackSecurityPrearm_(node_id, false); }
    bool requestSecurityPrearmForce(uint32_t node_id) { return requestStackSecurityPrearm_(node_id, true); }

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
    StackWateringCache &wateringLocal() { return _stack_watering_cache[0]; }
    const StackWateringCache &wateringLocal() const { return _stack_watering_cache[0]; }
    StackTankCache *tanksCache(uint32_t node_id) { return findStackTanksCache_(node_id, false); }
    const StackTankCache *tanksCache(uint32_t node_id) const { return findStackTanksCache_(node_id, false); }
    bool requestTanks(uint32_t node_id) { return requestStackTanks_(node_id); }
    StackAvrCache &avrLocal() { return _stack_avr_cache[0]; }
    const StackAvrCache &avrLocal() const { return _stack_avr_cache[0]; }
    StackAvrCache *avrCache(uint32_t node_id) { return findStackAvrCache_(node_id, false); }
    const StackAvrCache *avrCache(uint32_t node_id) const { return findStackAvrCache_(node_id, false); }
    bool requestAvr(uint32_t node_id) { return requestStackAvr_(node_id); }
    StackLeakCache &leakLocal() { return _stack_leak_cache[0]; }
    const StackLeakCache &leakLocal() const { return _stack_leak_cache[0]; }
    StackLeakCache *leakCache(uint32_t node_id) { return findStackLeakCache_(node_id, false); }
    const StackLeakCache *leakCache(uint32_t node_id) const { return findStackLeakCache_(node_id, false); }
    bool requestLeak(uint32_t node_id) { return requestStackLeak_(node_id); }
    StackWateringCache *wateringCache(uint32_t node_id) { return findStackWateringCache_(node_id, false); }
    const StackWateringCache *wateringCache(uint32_t node_id) const { return findStackWateringCache_(node_id, false); }
    bool requestWatering(uint32_t node_id) { return requestStackWatering_(node_id); }

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
        for (auto &cache : _stack_temp_sensors_cache)
        {
            cache.items = allocItems_<StackTempSensorItem>(cache.capacity, "temp_sensors",
                                                           &cache - _stack_temp_sensors_cache, _log, true);
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
        for (auto &cache : _stack_security_prearm_cache)
        {
            cache.items = allocItems_<StackSecurityPrearmItem>(cache.capacity, "sec_prearm",
                                                              &cache - _stack_security_prearm_cache, _log, true);
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
        for (auto &cache : _stack_watering_cache)
        {
            cache.items = allocItems_<StackWateringItem>(cache.capacity, "watering",
                                                        &cache - _stack_watering_cache, _log, true);
            if (!cache.items)
                cache.capacity = 0;
            cache.reset();
        }
        for (auto &cache : _stack_avr_cache)
        {
            cache.reset();
        }
        for (auto &cache : _stack_leak_cache)
        {
            cache.items = allocItems_<StackLeakItem>(cache.capacity, "leak",
                                                     &cache - _stack_leak_cache, _log, true);
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
        for (auto &cache : _stack_temp_sensors_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_security_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_security_prearm_cache)
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
        for (auto &cache : _stack_watering_cache)
        {
            releaseItems_(cache.items, cache.capacity);
            cache.items = nullptr;
        }
        for (auto &cache : _stack_leak_cache)
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
            log_fail("temp_sensors", i, _stack_temp_sensors_cache[i].items, _stack_temp_sensors_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("security", i, _stack_security_cache[i].items, _stack_security_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("sec_prearm", i, _stack_security_prearm_cache[i].items, _stack_security_prearm_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("meteo", i, _stack_meteo_cache[i].items, _stack_meteo_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("thermo", i, _stack_thermo_cache[i].items, _stack_thermo_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("septic", i, _stack_septic_cache[i].items, _stack_septic_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("tanks", i, _stack_tanks_cache[i].items, _stack_tanks_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("watering", i, _stack_watering_cache[i].items, _stack_watering_cache[i].capacity);
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
            log_fail("leak", i, _stack_leak_cache[i].items, _stack_leak_cache[i].capacity);
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
                const size_t len = serializeJson(out, reinterpret_cast<char *>(_tx_payload_buf), sizeof(_tx_payload_buf));
                if (len > 0)
                    _stack_master->sendTo(node_id, (uint8_t)StackMsgType::Ack,
                                          _tx_payload_buf, len);
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
                        if (it.button_port != SocketController::kInvalidPort)
                            o["button"] = it.button_port;
                        if (it.relay_port != SocketController::kInvalidPort)
                            o["relay"] = it.relay_port;
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
                    if (it.button_port != SocketController::kInvalidPort)
                        o["button"] = it.button_port;
                    if (it.relay_port != SocketController::kInvalidPort)
                        o["relay"] = it.relay_port;
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
            if ((StackFeature)feature == StackFeature::Watering && action == "get")
            {
                StackWateringCache *cache = findStackWateringCache_(target, false);
                if (!cache || !cache->has_data || !cache->items)
                {
                    requestStackWatering_(target);
                    send_err("no_data");
                    return;
                }
                DynamicJsonDocument data(4096);
                const uint16_t offset = params["offset"] | 0u;
                const uint16_t limit = params["limit"] | 0u;
                const uint16_t page_limit = (limit == 0) ? kWateringPageSize : limit;
                const uint16_t total = (uint16_t)cache->item_count;
                data["total"] = total;
                data["offset"] = offset;
                JsonArray items = data["items"].to<JsonArray>();
                uint16_t sent = 0;
                for (size_t i = offset; i < cache->item_count && sent < page_limit; ++i)
                {
                    const StackWateringItem &it = cache->items[i];
                    JsonObject o = items.add<JsonObject>();
                    o["id"] = (unsigned)it.id;
                    o["enabled"] = it.enabled;
                    o["status"] = it.status;
                    if (it.name[0])
                        o["name"] = it.name;
                    if (it.port != WateringController::kInvalidPort)
                        o["port"] = it.port;
                    if (it.tank_id)
                        o["tank"] = it.tank_id;
                    if (it.weekdays_mask)
                        o["weekdays_mask"] = it.weekdays_mask;
                    if (it.duration_sec && it.hour <= 23 && it.minute <= 59)
                    {
                        o["hour"] = it.hour;
                        o["minute"] = it.minute;
                        o["duration_s"] = it.duration_sec;
                    }
                    if (it.duration2_sec && it.hour2 <= 23 && it.minute2 <= 59)
                    {
                        o["hour2"] = it.hour2;
                        o["minute2"] = it.minute2;
                        o["duration2_s"] = it.duration2_sec;
                    }
                    if (it.duration3_sec && it.hour3 <= 23 && it.minute3 <= 59)
                    {
                        o["hour3"] = it.hour3;
                        o["minute3"] = it.minute3;
                        o["duration3_s"] = it.duration3_sec;
                    }
                    o["resume"] = it.resume_after_refill;
                    o["resume_level"] = it.resume_level;
                    o["active"] = it.active;
                    o["paused"] = it.paused;
                    if (it.remaining_ms)
                        o["remaining_ms"] = it.remaining_ms;
                    ++sent;
                }
                data["count"] = sent;
                send_ok(data);
                return;
            }
            if ((StackFeature)feature == StackFeature::Avr && action == "get")
            {
                StackAvrCache *cache = findStackAvrCache_(target, false);
                if (!cache || !cache->has_data)
                {
                    requestStackAvr_(target);
                    send_err("no_data");
                    return;
                }
                DynamicJsonDocument data(1024);
                data["enabled"] = cache->enabled;
                data["auto_mode"] = cache->auto_mode;
                data["prefer_main"] = cache->prefer_main;
                data["auto_return_main"] = cache->auto_return_main;
                if (cache->main_ok_port != AvrController::kInvalidPort)
                    data["main_ok_port"] = cache->main_ok_port;
                if (cache->reserve_ok_port != AvrController::kInvalidPort)
                    data["reserve_ok_port"] = cache->reserve_ok_port;
                if (cache->relay_main_port != AvrController::kInvalidPort)
                    data["relay_main_port"] = cache->relay_main_port;
                if (cache->relay_reserve_port != AvrController::kInvalidPort)
                    data["relay_reserve_port"] = cache->relay_reserve_port;
                if (cache->feedback_main_port != AvrController::kInvalidPort)
                    data["feedback_main_port"] = cache->feedback_main_port;
                if (cache->feedback_reserve_port != AvrController::kInvalidPort)
                    data["feedback_reserve_port"] = cache->feedback_reserve_port;
                data["main_ok"] = cache->main_ok;
                data["reserve_ok"] = cache->reserve_ok;
                data["relay_main_on"] = cache->relay_main_on;
                data["relay_reserve_on"] = cache->relay_reserve_on;
                data["active_source"] = cache->active_source;
                data["target_source"] = cache->target_source;
                data["fault"] = cache->fault;
                data["transfer"] = cache->transfer;
                send_ok(data);
                return;
            }
            if ((StackFeature)feature == StackFeature::Leak && action == "get")
            {
                StackLeakCache *cache = findStackLeakCache_(target, false);
                if (!cache || !cache->has_data || !cache->items)
                {
                    requestStackLeak_(target);
                    send_err("no_data");
                    return;
                }
                DynamicJsonDocument data(4096);
                JsonArray items = data["items"].to<JsonArray>();
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const StackLeakItem &it = cache->items[i];
                    JsonObject o = items.add<JsonObject>();
                    o["id"] = (unsigned)it.id;
                    o["enabled"] = it.enabled;
                    o["power_on"] = it.power_on;
                    o["sensor_active_low"] = it.sensor_active_low;
                    if (it.sensor != LeakController::kInvalidPort)
                        o["sensor"] = it.sensor;
                    if (it.valve != LeakController::kInvalidPort)
                        o["valve"] = it.valve;
                    if (it.alarm != LeakController::kInvalidPort)
                        o["alarm"] = it.alarm;
                    if (it.name[0])
                        o["name"] = it.name;
                    o["wet"] = it.wet;
                    o["alarm_latched"] = it.alarm_latched;
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
        {
            if (_log)
                _log->warn(F("STACK"), F("Drop rx frame: json parse failed type: %u node: 0x%08lX bytes: %u"),
                           (unsigned)frame.type,
                           (unsigned long)node_id,
                           (unsigned)frame.payload_len);
            return;
        }
        const uint16_t cmd_id = doc["cmd_id"] | 0;
        String rx_action = doc["action"] | "";
        rx_action.toLowerCase();
        StackSocketsCache *sock_cache = findStackSocketsCacheByCmd_(cmd_id);
        StackLightsCache *light_cache = findStackLightsCacheByCmd_(cmd_id);
        StackPortsCache *ports_cache = findStackPortsCacheByCmd_(cmd_id);
        StackExtendersCache *ext_cache = findStackExtendersCacheByCmd_(cmd_id);
        StackSecurityCache *sec_cache = findStackSecurityCacheByCmd_(cmd_id);
        StackSecurityPrearmCache *sec_prearm_cache = findStackSecurityPrearmCacheByCmd_(cmd_id);
        StackMeteoCache *meteo_cache = findStackMeteoCacheByCmd_(cmd_id);
        StackThermoCache *thermo_cache = findStackThermoCacheByCmd_(cmd_id);
        StackSepticCache *septic_cache = findStackSepticCacheByCmd_(cmd_id);
        StackTankCache *tanks_cache = findStackTanksCacheByCmd_(cmd_id);
        StackAvrCache *avr_cache = findStackAvrCacheByCmd_(cmd_id);
        StackLeakCache *leak_cache = findStackLeakCacheByCmd_(cmd_id);
        StackWateringCache *watering_cache = findStackWateringCacheByCmd_(cmd_id);
        StackI2cCache *i2c_cache = findStackI2cCacheByCmd_(cmd_id);
        StackOwCache *ow_cache = findStackOwCacheByCmd_(cmd_id);
        StackTempSensorsCache *temp_sensors_cache = findStackTempSensorsCacheByCmd_(cmd_id);
        bool status_is_plc = false;
        bool status_is_rtc = false;
        StackNodeStatusCache *status_cache = findStackNodeStatusCacheByCmd_(cmd_id, status_is_plc, status_is_rtc);
        if (!sock_cache && !light_cache && !ports_cache && !ext_cache && !sec_cache && !sec_prearm_cache && !meteo_cache &&
            !thermo_cache && !septic_cache && !tanks_cache && !avr_cache && !leak_cache &&
            !watering_cache && !i2c_cache && !ow_cache && !temp_sensors_cache && !status_cache)
        {
            const uint8_t feature = (uint8_t)(doc["feature"] | 0u);
            switch ((StackFeature)feature)
            {
            case StackFeature::Sockets:
            {
                const String action = doc["action"] | "";
                if (action == "get_lights")
                {
                    StackLightsCache *c = findStackLightsCache_(node_id, false);
                    if (c && c->pending)
                        light_cache = c;
                }
                else if (action == "set_lights")
                {
                    StackLightsCache *c = findStackLightsCache_(node_id, false);
                    if (c)
                        light_cache = c;
                }
                else if (action == "set")
                {
                    StackSocketsCache *c = findStackSocketsCache_(node_id, false);
                    if (c)
                        sock_cache = c;
                }
                else
                {
                    StackSocketsCache *c = findStackSocketsCache_(node_id, false);
                    if (c && c->pending)
                        sock_cache = c;
                }
                break;
            }
            case StackFeature::Security:
            {
                const String action = doc["action"] | "";
                if (action == "prearm")
                {
                    StackSecurityPrearmCache *c = findStackSecurityPrearmCache_(node_id, false);
                    if (c && c->pending)
                        sec_prearm_cache = c;
                }
                else
                {
                    StackSecurityCache *sec_c = findStackSecurityCache_(node_id, false);
                    StackSecurityPrearmCache *prearm_c = findStackSecurityPrearmCache_(node_id, false);
                    if (sec_c && sec_c->pending)
                    {
                        sec_cache = sec_c;
                    }
                    else if ((!action.length() || action == "status") && prearm_c && prearm_c->pending)
                    {
                        // Old slave firmware may omit action in Ack/Err for prearm.
                        sec_prearm_cache = prearm_c;
                    }
                }
                break;
            }
            case StackFeature::Meteo:
            {
                StackMeteoCache *c = findStackMeteoCache_(node_id, false);
                if (c && c->pending)
                    meteo_cache = c;
                break;
            }
            case StackFeature::Thermo:
            {
                if (rx_action == "get")
                {
                    StackThermoCache *c = findStackThermoCache_(node_id, false);
                    if (c && c->pending)
                        thermo_cache = c;
                }
                else if (rx_action == "set")
                {
                    StackThermoCache *c = findStackThermoCache_(node_id, false);
                    if (c)
                        thermo_cache = c;
                }
                break;
            }
            case StackFeature::Septic:
            {
                StackSepticCache *c = findStackSepticCache_(node_id, false);
                if (rx_action == "get")
                {
                    if (c && c->pending)
                        septic_cache = c;
                }
                else if (rx_action == "set")
                {
                    if (c)
                        septic_cache = c;
                }
                break;
            }
            case StackFeature::Tanks:
            {
                StackTankCache *c = findStackTanksCache_(node_id, false);
                if (rx_action == "get")
                {
                    if (c && c->pending)
                        tanks_cache = c;
                }
                else if (rx_action == "set")
                {
                    if (c)
                        tanks_cache = c;
                }
                break;
            }
            case StackFeature::Watering:
            {
                StackWateringCache *c = findStackWateringCache_(node_id, false);
                if (c && c->pending)
                    watering_cache = c;
                break;
            }
            case StackFeature::I2cScan:
            {
                StackI2cCache *c = findStackI2cCache_(node_id, false);
                if (c && c->pending)
                    i2c_cache = c;
                break;
            }
            case StackFeature::Ports:
            {
                StackPortsCache *c = findStackPortsCache_(node_id, false);
                if (c && c->pending)
                    ports_cache = c;
                break;
            }
            case StackFeature::Extenders:
            {
                StackExtendersCache *c = findStackExtendersCache_(node_id, false);
                if (c && c->pending)
                    ext_cache = c;
                break;
            }
            case StackFeature::OwScan:
            {
                StackOwCache *c = findStackOwCache_(node_id, false);
                if (c && c->pending)
                    ow_cache = c;
                break;
            }
            case StackFeature::TempSensors:
            {
                StackTempSensorsCache *c = findStackTempSensorsCache_(node_id, false);
                if (c && c->pending)
                    temp_sensors_cache = c;
                break;
            }
            case StackFeature::Avr:
            {
                StackAvrCache *c = findStackAvrCache_(node_id, false);
                if (c && c->pending)
                    avr_cache = c;
                break;
            }
            case StackFeature::Leak:
            {
                StackLeakCache *c = findStackLeakCache_(node_id, false);
                if (c && c->pending)
                    leak_cache = c;
                break;
            }
            case StackFeature::PlcStatus:
            {
                StackNodeStatusCache *c = findStackNodeStatusCache_(node_id, false);
                if (c && c->pending_plc)
                {
                    status_cache = c;
                    status_is_plc = true;
                    status_is_rtc = false;
                }
                break;
            }
            case StackFeature::Rtc:
            {
                StackNodeStatusCache *c = findStackNodeStatusCache_(node_id, false);
                if (c && c->pending_rtc)
                {
                    status_cache = c;
                    status_is_plc = false;
                    status_is_rtc = true;
                }
                break;
            }
            default:
                break;
            }
        }
        if (!sock_cache && !light_cache && !ports_cache && !ext_cache && !sec_cache && !sec_prearm_cache && !meteo_cache &&
            !thermo_cache && !septic_cache && !tanks_cache && !avr_cache && !leak_cache &&
            !watering_cache && !i2c_cache && !ow_cache && !temp_sensors_cache && !status_cache)
        {
            if (cmd_id == 0)
                return;
            return;
        }
        const bool ok = (frame.type == (uint8_t)StackMsgType::Ack) && (doc["ok"] | false);
        JsonObjectConst data_obj = doc["data"].as<JsonObjectConst>();
        JsonArrayConst items = data_obj["items"].as<JsonArrayConst>();

        if (sock_cache)
        {
            const uint32_t sock_now = millis();
            const bool sock_set_update = (rx_action == "set");
            sock_cache->updated_ms = sock_now;
            if (!ok)
            {
                sock_cache->pending = false;
                sock_cache->pending_cmd_id = 0;
                sock_cache->last_ok = false;
                sock_cache->last_error = "";
                sock_cache->last_error = doc["error"] | "error";
            }
            else if (sock_set_update)
            {
                if (!items.isNull())
                {
                    for (JsonObjectConst item : items)
                    {
                        if (!item["id"].is<unsigned>())
                            continue;
                        const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                        StackSocketItem *dst = findStackSocketItem_(*sock_cache, id);
                        if (!dst)
                        {
                            if (sock_cache->item_count >= sock_cache->capacity)
                                continue;
                            dst = &sock_cache->items[sock_cache->item_count++];
                            *dst = StackSocketItem{};
                            dst->id = id;
                        }
                        if (item["enabled"].is<bool>() || item["enabled"].is<unsigned>() || item["enabled"].is<int>())
                            dst->enabled = item["enabled"].as<bool>();
                        if (item["state"].is<bool>() || item["state"].is<unsigned>() || item["state"].is<int>())
                            dst->state = item["state"].as<bool>();
                        if (item["button"].is<unsigned>())
                            dst->button_port = (uint8_t)item["button"].as<unsigned>();
                        if (item["relay"].is<unsigned>())
                            dst->relay_port = (uint8_t)item["relay"].as<unsigned>();
                        if (item["name"].is<const char *>())
                            copyStr_(dst->name, sizeof(dst->name), item["name"].as<const char *>());
                    }
                }
                sock_cache->pending = false;
                sock_cache->pending_cmd_id = 0;
                sock_cache->has_data = sock_cache->item_count > 0;
                sock_cache->last_ok = true;
                sock_cache->last_error = "";
                sock_cache->node_id = node_id;
            }
            else
            {
                const uint16_t part = data_obj["part"] | 1;
                const uint16_t parts = data_obj["parts"] | 1;
                const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    sock_cache->item_count = 0;
                    sock_cache->has_data = false;
                    sock_cache->last_ok = false;
                    sock_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                        dst.button_port = item["button"].is<unsigned>() ? (uint8_t)item["button"].as<unsigned>()
                                                                         : SocketController::kInvalidPort;
                        dst.relay_port = item["relay"].is<unsigned>() ? (uint8_t)item["relay"].as<unsigned>()
                                                                       : SocketController::kInvalidPort;
                        copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    }
                }
                if (done)
                {
                    sock_cache->pending = false;
                    sock_cache->pending_cmd_id = 0;
                    sock_cache->has_data = true;
                    sock_cache->last_ok = true;
                    sock_cache->node_id = node_id;
                }
                else
                {
                    sock_cache->pending = true;
                }
            }
        }

        if (light_cache)
        {
            const uint32_t light_now = millis();
            const bool light_set_update = (rx_action == "set_lights");
            light_cache->updated_ms = light_now;
            if (!ok)
            {
                light_cache->pending = false;
                light_cache->pending_cmd_id = 0;
                light_cache->pending_since_ms = 0;
                light_cache->last_ok = false;
                light_cache->last_error = "";
                light_cache->last_error = doc["error"] | "error";
            }
            else if (light_set_update)
            {
                if (!items.isNull())
                {
                    for (JsonObjectConst item : items)
                    {
                        if (!item["id"].is<unsigned>())
                            continue;
                        const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                        StackLightItem *dst = findStackLightItem_(*light_cache, id);
                        if (!dst)
                        {
                            if (light_cache->item_count >= light_cache->capacity)
                                continue;
                            dst = &light_cache->items[light_cache->item_count++];
                            *dst = StackLightItem{};
                            dst->id = id;
                        }
                        if (item["enabled"].is<bool>() || item["enabled"].is<unsigned>() || item["enabled"].is<int>())
                            dst->enabled = item["enabled"].as<bool>();
                        if (item["state"].is<bool>() || item["state"].is<unsigned>() || item["state"].is<int>())
                            dst->state = item["state"].as<bool>();
                        if (item["button"].is<unsigned>())
                            dst->button_port = (uint8_t)item["button"].as<unsigned>();
                        if (item["relay"].is<unsigned>())
                            dst->relay_port = (uint8_t)item["relay"].as<unsigned>();
                        if (item["name"].is<const char *>())
                            copyStr_(dst->name, sizeof(dst->name), item["name"].as<const char *>());
                    }
                }
                light_cache->pending = false;
                light_cache->pending_cmd_id = 0;
                light_cache->pending_since_ms = 0;
                light_cache->has_data = light_cache->item_count > 0;
                light_cache->last_ok = true;
                light_cache->last_error = "";
                light_cache->node_id = node_id;
            }
            else
            {
                const uint16_t part = data_obj["part"] | 1;
                const uint16_t parts = data_obj["parts"] | 1;
                const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    light_cache->item_count = 0;
                    light_cache->has_data = false;
                    light_cache->last_ok = false;
                    light_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                        dst.button_port = item["button"].is<unsigned>() ? (uint8_t)item["button"].as<unsigned>()
                                                                         : SocketController::kInvalidPort;
                        dst.relay_port = item["relay"].is<unsigned>() ? (uint8_t)item["relay"].as<unsigned>()
                                                                       : SocketController::kInvalidPort;
                        copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    }
                }
                if (done)
                {
                    light_cache->pending = false;
                    light_cache->pending_cmd_id = 0;
                    light_cache->pending_since_ms = 0;
                    light_cache->has_data = true;
                    light_cache->last_ok = true;
                    light_cache->node_id = node_id;
                }
                else
                {
                    light_cache->pending = true;
                    light_cache->pending_since_ms = light_now;
                }
            }
        }

        if (ports_cache)
        {
            ports_cache->updated_ms = millis();
            if (!ok)
            {
                ports_cache->pending = false;
                ports_cache->parts_expected = 0;
                ports_cache->parts_received = 0;
                memset(ports_cache->part_seen, 0, sizeof(ports_cache->part_seen));
                memset(ports_cache->present, 0, sizeof(ports_cache->present));
                ports_cache->last_ok = false;
                ports_cache->last_error = "";
                ports_cache->last_error = doc["error"] | "error";
            }
            else
            {
                const bool has_offset = data_obj["offset"].is<unsigned>() || data_obj["offset"].is<int>();
                const bool has_limit = data_obj["limit"].is<unsigned>() || data_obj["limit"].is<int>();
                JsonArrayConst port_items = data_obj["ports"].as<JsonArrayConst>();
                if (port_items.isNull())
                    port_items = items;
                if (!port_items.isNull())
                {
                    if (!(has_offset || has_limit))
                    {
                        ports_cache->pending = false;
                        ports_cache->pending_cmd_id = 0;
                        ports_cache->last_ok = false;
                        ports_cache->last_error = "ports page fields missing";
                        ports_cache->next_offset = 0;
                        ports_cache->page_limit = 0;
                    }
                    else
                    {
                        const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : false;
                        const uint16_t offset = (uint16_t)(data_obj["offset"] | 0u);
                        const uint16_t limit = (uint16_t)(data_obj["limit"] | (ports_cache->page_limit ? ports_cache->page_limit : 6u));
                        const uint16_t total = (uint16_t)(data_obj["total"] | 0u);
                        if (offset == 0)
                        {
                            ports_cache->item_count = 0;
                            ports_cache->has_data = false;
                            ports_cache->last_ok = false;
                            ports_cache->last_error = "";
                            memset(ports_cache->part_seen, 0, sizeof(ports_cache->part_seen));
                            memset(ports_cache->present, 0, sizeof(ports_cache->present));
                            ports_cache->parts_received = 0;
                            ports_cache->parts_expected = 0;
                        }
                        if (total > 0 && limit > 0)
                            ports_cache->parts_expected = (uint16_t)((total + limit - 1u) / limit);
                        ++ports_cache->parts_received;
                        for (JsonObjectConst item : port_items)
                        {
                            if (!item["id"].is<unsigned>())
                                continue;
                            const uint16_t id16 = (uint16_t)item["id"].as<unsigned>();
                            if (id16 >= PortIO::PORT_COUNT)
                                continue;
                            const size_t id = (size_t)id16;
                            StackPortItem &dst = ports_cache->items[id];
                            dst.id = (uint8_t)id16;
                            dst.ctrl = item["ctrl"] | false;
                            dst.used = item["used"] | false;
                            dst.is_extender = item["ext"] | false;
                            dst.pin_type = (uint8_t)(item["ptype"] | 0xFFu);
                            dst.dev = item["dev"] | -1;
                            dst.pin = item["pin"] | -1;
                            copyStr_(dst.backend, sizeof(dst.backend), item["backend"].as<const char *>());
                            copyStr_(dst.loc, sizeof(dst.loc), item["loc"].as<const char *>());
                            copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                            copyStr_(dst.hw, sizeof(dst.hw), item["hw"].as<const char *>());
                            copyStr_(dst.alias, sizeof(dst.alias), item["alias"].as<const char *>());
                            ports_cache->present[id] = 1;
                        }
                        size_t present_count = 0;
                        for (size_t i = 0; i < PortIO::PORT_COUNT; ++i)
                        {
                            if (ports_cache->present[i])
                                ++present_count;
                        }
                        ports_cache->item_count = present_count;
                        ports_cache->has_data = (present_count > 0);
                        ports_cache->node_id = node_id;
                        if (done)
                        {
                            size_t w = 0;
                            for (size_t i = 0; i < PortIO::PORT_COUNT; ++i)
                            {
                                if (!ports_cache->present[i])
                                    continue;
                                if (w != i)
                                    ports_cache->items[w] = ports_cache->items[i];
                                ++w;
                            }
                            ports_cache->item_count = w;
                            ports_cache->has_data = (w > 0);
                            ports_cache->pending = false;
                            ports_cache->last_ok = (w > 0);
                            ports_cache->last_error = "";
                            ports_cache->pending_cmd_id = 0;
                            ports_cache->next_offset = 0;
                            ports_cache->page_limit = 0;
                            ports_cache->parts_expected = 0;
                            ports_cache->parts_received = 0;
                            memset(ports_cache->part_seen, 0, sizeof(ports_cache->part_seen));
                            memset(ports_cache->present, 0, sizeof(ports_cache->present));
                        }
                        else
                        {
                            const uint16_t next_offset = (uint16_t)(data_obj["next_offset"] | (uint16_t)(offset + port_items.size()));
                            const uint16_t next_limit = limit ? limit : (ports_cache->page_limit ? ports_cache->page_limit : 6u);
                            ports_cache->pending = true;
                            ports_cache->last_ok = false;
                            ports_cache->last_error = "";
                            if (!sendStackPortsPage_(*ports_cache, node_id, next_offset, next_limit))
                            {
                                ports_cache->pending = false;
                                ports_cache->last_error = "ports next page send fail";
                            }
                        }
                    }
                }
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
            sec_cache->updated_ms = millis();
            if (!ok)
            {
                sec_cache->pending = false;
                sec_cache->pending_since_ms = 0;
                sec_cache->last_ok = false;
                sec_cache->last_error = "";
                sec_cache->last_error = doc["error"] | "error";
            }
            else
            {
                const bool merge_set = (rx_action == "set");
                const uint16_t part = data_obj["part"] | 1;
                const uint16_t parts = data_obj["parts"] | 1;
                const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : (part >= parts);
                sec_cache->enabled = data_obj["enabled"] | false;
                sec_cache->armed = data_obj["armed"] | false;
                sec_cache->alarm = data_obj["alarm"] | false;
                sec_cache->siren = (uint8_t)(data_obj["siren"] | SecurityController::kInvalidPort);
                if (part <= 1 && !merge_set)
                {
                    sec_cache->item_count = 0;
                    sec_cache->has_data = false;
                    sec_cache->last_ok = false;
                    sec_cache->last_error = "";
                }
                if (!items.isNull())
                {
                    for (JsonObjectConst item : items)
                    {
                        if (!item["id"].is<unsigned>())
                            continue;
                        const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                        StackSecuritySensorItem *dst_ptr = nullptr;
                        if (merge_set)
                        {
                            for (size_t i = 0; i < sec_cache->item_count; ++i)
                            {
                                if (sec_cache->items[i].id == id)
                                {
                                    dst_ptr = &sec_cache->items[i];
                                    break;
                                }
                            }
                            if (!dst_ptr && sec_cache->item_count < SecurityController::kSensorCount)
                            {
                                dst_ptr = &sec_cache->items[sec_cache->item_count++];
                                dst_ptr->id = id;
                            }
                        }
                        else
                        {
                            if (sec_cache->item_count >= SecurityController::kSensorCount)
                                break;
                            dst_ptr = &sec_cache->items[sec_cache->item_count++];
                            dst_ptr->id = id;
                        }
                        if (!dst_ptr)
                            continue;
                        StackSecuritySensorItem &dst = *dst_ptr;
                        dst.enabled = item["enabled"] | false;
                        dst.detect = item["detect"] | false;
                        dst.silent = item["silent"] | false;
                        dst.port = (uint8_t)(item["port"] | SecurityController::kInvalidPort);
                        copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                        copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    }
                }
                if (done)
                {
                    sec_cache->pending = false;
                    sec_cache->pending_since_ms = 0;
                    sec_cache->has_data = true;
                    sec_cache->last_ok = true;
                    sec_cache->node_id = node_id;
                }
                else
                {
                    sec_cache->pending = true;
                }
            }
        }

        if (sec_prearm_cache)
        {
            sec_prearm_cache->pending = false;
            sec_prearm_cache->pending_since_ms = 0;
            sec_prearm_cache->updated_ms = millis();
            sec_prearm_cache->last_ok = false;
            sec_prearm_cache->last_error = "";
            if (!ok)
            {
                sec_prearm_cache->last_error = doc["error"] | "error";
                if (_log)
                    _log->warn(F("STACK"), F("security prearm err: unit=0x%08lX err=%s"),
                               (unsigned long)node_id,
                               sec_prearm_cache->last_error.length() ? sec_prearm_cache->last_error.c_str() : "error");
            }
            else if (!items.isNull())
            {
                sec_prearm_cache->item_count = 0;
                for (JsonObjectConst item : items)
                {
                    if (sec_prearm_cache->item_count >= SecurityController::kSensorCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackSecurityPrearmItem &dst = sec_prearm_cache->items[sec_prearm_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                }
                sec_prearm_cache->has_data = true;
                sec_prearm_cache->last_ok = true;
                sec_prearm_cache->node_id = node_id;
            }
        }

        if (meteo_cache)
        {
            meteo_cache->updated_ms = millis();
            if (!ok)
            {
                meteo_cache->pending = false;
                meteo_cache->pending_since_ms = 0;
                meteo_cache->last_ok = false;
                meteo_cache->last_error = "";
                meteo_cache->last_error = doc["error"] | "error";
            }
            else
            {
                const uint16_t part = data_obj["part"] | 1;
                const uint16_t parts = data_obj["parts"] | 1;
                const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    meteo_cache->item_count = 0;
                    meteo_cache->has_data = false;
                    meteo_cache->last_ok = false;
                    meteo_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                        dst.has_read = item["has_read"] | false;
                        dst.temp_c = item["temp_c"] | 0.0f;
                        dst.hum = item["hum"] | 0.0f;
                        dst.age_s = item["age_s"] | 0u;
                        copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                        copyStr_(dst.type, sizeof(dst.type), item["type"].as<const char *>());
                        copyStr_(dst.addr, sizeof(dst.addr), item["addr"].as<const char *>());
                        if (item["pin"].is<int>() || item["pin"].is<unsigned>())
                            dst.pin = item["pin"].as<int>();
                        else
                            dst.pin = -1;
                    }
                }
                if (done)
                {
                    meteo_cache->pending = false;
                    meteo_cache->pending_since_ms = 0;
                    meteo_cache->has_data = true;
                    meteo_cache->last_ok = true;
                    meteo_cache->node_id = node_id;
                }
                else
                {
                    meteo_cache->pending = true;
                }
            }
        }

        if (thermo_cache)
        {
            thermo_cache->updated_ms = millis();
            if (!ok)
            {
                thermo_cache->pending = false;
                thermo_cache->last_ok = false;
                thermo_cache->last_error = "";
                thermo_cache->last_error = doc["error"] | "error";
            }
            else if (rx_action == "set")
            {
                if (!items.isNull())
                {
                    for (JsonObjectConst item : items)
                    {
                        if (!item["id"].is<unsigned>())
                            continue;
                        const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                        StackThermoItem *dst_ptr = nullptr;
                        for (size_t k = 0; k < thermo_cache->item_count; ++k)
                        {
                            if (thermo_cache->items[k].id == id)
                            {
                                dst_ptr = &thermo_cache->items[k];
                                break;
                            }
                        }
                        if (!dst_ptr)
                        {
                            if (thermo_cache->item_count >= ThermoController::kDeviceCount)
                                continue;
                            dst_ptr = &thermo_cache->items[thermo_cache->item_count++];
                            *dst_ptr = StackThermoItem{};
                            dst_ptr->id = id;
                        }
                        StackThermoItem &dst = *dst_ptr;
                        if (item["enabled"].is<bool>() || item["enabled"].is<int>() || item["enabled"].is<unsigned>())
                            dst.enabled = item["enabled"].is<bool>() ? item["enabled"].as<bool>() : (item["enabled"].as<int>() != 0);
                        if (item["power_on"].is<bool>() || item["power_on"].is<int>() || item["power_on"].is<unsigned>())
                            dst.power_on = item["power_on"].is<bool>() ? item["power_on"].as<bool>() : (item["power_on"].as<int>() != 0);
                        if (item["heat_on"].is<bool>() || item["heat_on"].is<int>() || item["heat_on"].is<unsigned>())
                            dst.heat_on = item["heat_on"].is<bool>() ? item["heat_on"].as<bool>() : (item["heat_on"].as<int>() != 0);
                        if (item["cool_on"].is<bool>() || item["cool_on"].is<int>() || item["cool_on"].is<unsigned>())
                            dst.cool_on = item["cool_on"].is<bool>() ? item["cool_on"].as<bool>() : (item["cool_on"].as<int>() != 0);
                        if (item["sensor"].is<unsigned>() || item["sensor"].is<int>())
                            dst.sensor = (uint8_t)item["sensor"].as<unsigned>();
                        if (item["sensor_node"].is<unsigned>() || item["sensor_node"].is<int>())
                            dst.sensor_node = (uint32_t)item["sensor_node"].as<unsigned>();
                        if (item["target"].is<float>() || item["target"].is<double>() || item["target"].is<int>())
                            dst.target = item["target"].as<float>();
                        if (item["hyst"].is<float>() || item["hyst"].is<double>() || item["hyst"].is<int>())
                            dst.hyst = item["hyst"].as<float>();
                        if (item["heat"].is<unsigned>() || item["heat"].is<int>())
                            dst.heat = (uint8_t)item["heat"].as<unsigned>();
                        if (item["cool"].is<unsigned>() || item["cool"].is<int>())
                            dst.cool = (uint8_t)item["cool"].as<unsigned>();
                        if (item["button"].is<unsigned>() || item["button"].is<int>())
                            dst.button = (uint8_t)item["button"].as<unsigned>();
                        if (item["name"].is<const char *>())
                            copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                        if (item["mode"].is<const char *>())
                            copyStr_(dst.mode, sizeof(dst.mode), item["mode"].as<const char *>());
                    }
                }
                thermo_cache->pending = false;
                thermo_cache->has_data = true;
                thermo_cache->last_ok = true;
                thermo_cache->node_id = node_id;
            }
            else
            {
                const uint16_t part = data_obj["part"] | 1;
                const uint16_t parts = data_obj["parts"] | 1;
                const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    thermo_cache->item_count = 0;
                    thermo_cache->has_data = false;
                    thermo_cache->last_ok = false;
                    thermo_cache->last_error = "";
                }
                if (!items.isNull())
                {
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
                }
                if (done)
                {
                    thermo_cache->pending = false;
                    thermo_cache->has_data = true;
                    thermo_cache->last_ok = true;
                    thermo_cache->node_id = node_id;
                }
                else
                {
                    thermo_cache->pending = true;
                }
            }
        }

        if (septic_cache)
        {
            septic_cache->updated_ms = millis();
            if (!ok)
            {
                septic_cache->pending = false;
                septic_cache->pending_since_ms = 0;
                septic_cache->last_ok = false;
                septic_cache->last_error = "";
                septic_cache->last_error = doc["error"] | "error";
            }
            else
            {
                const bool merge_set = (rx_action == "set");
                const uint16_t part = data_obj["part"] | 1;
                const uint16_t parts = data_obj["parts"] | 1;
                const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : (part >= parts);
                if (part <= 1 && !merge_set)
                {
                    septic_cache->item_count = 0;
                    septic_cache->has_data = false;
                    septic_cache->last_ok = false;
                    septic_cache->last_error = "";
                }
                if (!items.isNull())
                {
                    for (JsonObjectConst item : items)
                    {
                        if (!item["id"].is<unsigned>())
                            continue;
                        const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                        StackSepticItem *dst_ptr = nullptr;
                        if (merge_set)
                        {
                            for (size_t i = 0; i < septic_cache->item_count; ++i)
                            {
                                if (septic_cache->items[i].id == id)
                                {
                                    dst_ptr = &septic_cache->items[i];
                                    break;
                                }
                            }
                            if (!dst_ptr && septic_cache->item_count < SepticController::kSepticCount)
                            {
                                dst_ptr = &septic_cache->items[septic_cache->item_count++];
                                dst_ptr->id = id;
                            }
                        }
                        else
                        {
                            if (septic_cache->item_count >= SepticController::kSepticCount)
                                break;
                            dst_ptr = &septic_cache->items[septic_cache->item_count++];
                            dst_ptr->id = id;
                        }
                        if (!dst_ptr)
                            continue;
                        StackSepticItem &dst = *dst_ptr;
                        dst.enabled = item["enabled"] | false;
                        dst.monitor = item["monitor"] | false;
                        copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                        dst.warning_port = (uint8_t)(item["warning_port"] | SepticController::kInvalidPort);
                        dst.alarm_port = (uint8_t)(item["alarm_port"] | SepticController::kInvalidPort);
                        dst.relay_warning = (uint8_t)(item["relay_warning"] | SepticController::kInvalidPort);
                        dst.relay_alarm = (uint8_t)(item["relay_alarm"] | SepticController::kInvalidPort);
                        dst.warning = item["warning"] | false;
                        dst.alarm = item["alarm"] | false;
                    }
                }
                if (done)
                {
                    septic_cache->pending = false;
                    septic_cache->pending_since_ms = 0;
                    septic_cache->has_data = septic_cache->item_count > 0 || septic_cache->has_data;
                    septic_cache->last_ok = true;
                    septic_cache->node_id = node_id;
                }
                else
                {
                    septic_cache->pending = true;
                }
            }
        }

        if (tanks_cache)
        {
            tanks_cache->updated_ms = millis();
            if (!ok)
            {
                tanks_cache->pending = false;
                tanks_cache->pending_since_ms = 0;
                tanks_cache->last_ok = false;
                tanks_cache->last_error = "";
                tanks_cache->last_error = doc["error"] | "error";
            }
            else
            {
                const bool merge_set = (rx_action == "set");
                const uint16_t part = data_obj["part"] | 1;
                const uint16_t parts = data_obj["parts"] | 1;
                const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : (part >= parts);
                if (part <= 1 && !merge_set)
                {
                    tanks_cache->item_count = 0;
                    tanks_cache->has_data = false;
                    tanks_cache->last_ok = false;
                    tanks_cache->last_error = "";
                }
                if (!items.isNull())
                {
                    for (JsonObjectConst item : items)
                    {
                        if (!item["id"].is<unsigned>())
                            continue;
                        const uint8_t id = (uint8_t)item["id"].as<unsigned>();
                        StackTankItem *dst_ptr = nullptr;
                        if (merge_set)
                        {
                            for (size_t i = 0; i < tanks_cache->item_count; ++i)
                            {
                                if (tanks_cache->items[i].id == id)
                                {
                                    dst_ptr = &tanks_cache->items[i];
                                    break;
                                }
                            }
                            if (!dst_ptr && tanks_cache->item_count < TankController::kTankCount)
                            {
                                dst_ptr = &tanks_cache->items[tanks_cache->item_count++];
                                dst_ptr->id = id;
                            }
                        }
                        else
                        {
                            if (tanks_cache->item_count >= TankController::kTankCount)
                                break;
                            dst_ptr = &tanks_cache->items[tanks_cache->item_count++];
                            dst_ptr->id = id;
                        }
                        if (!dst_ptr)
                            continue;
                        StackTankItem &dst = *dst_ptr;
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
                }
                if (done)
                {
                    tanks_cache->pending = false;
                    tanks_cache->pending_since_ms = 0;
                    tanks_cache->has_data = tanks_cache->item_count > 0 || tanks_cache->has_data;
                    tanks_cache->last_ok = true;
                    tanks_cache->node_id = node_id;
                }
                else
                {
                    tanks_cache->pending = true;
                }
            }
        }

        if (avr_cache)
        {
            avr_cache->updated_ms = millis();
            avr_cache->pending = false;
            avr_cache->last_ok = false;
            avr_cache->last_error = "";
            if (!ok)
            {
                avr_cache->last_error = doc["error"] | "error";
            }
            else
            {
                avr_cache->enabled = data_obj["enabled"] | false;
                avr_cache->auto_mode = data_obj["auto_mode"] | true;
                avr_cache->prefer_main = data_obj["prefer_main"] | true;
                avr_cache->auto_return_main = data_obj["auto_return_main"] | true;
                avr_cache->main_ok = data_obj["main_ok"] | false;
                avr_cache->reserve_ok = data_obj["reserve_ok"] | false;
                avr_cache->relay_main_on = data_obj["relay_main_on"] | false;
                avr_cache->relay_reserve_on = data_obj["relay_reserve_on"] | false;
                avr_cache->transfer = data_obj["transfer"] | false;
                avr_cache->main_ok_port = (uint8_t)(data_obj["main_ok_port"] | AvrController::kInvalidPort);
                avr_cache->reserve_ok_port = (uint8_t)(data_obj["reserve_ok_port"] | AvrController::kInvalidPort);
                avr_cache->relay_main_port = (uint8_t)(data_obj["relay_main_port"] | AvrController::kInvalidPort);
                avr_cache->relay_reserve_port = (uint8_t)(data_obj["relay_reserve_port"] | AvrController::kInvalidPort);
                avr_cache->feedback_main_port = (uint8_t)(data_obj["feedback_main_port"] | AvrController::kInvalidPort);
                avr_cache->feedback_reserve_port = (uint8_t)(data_obj["feedback_reserve_port"] | AvrController::kInvalidPort);
                copyStr_(avr_cache->active_source, sizeof(avr_cache->active_source), data_obj["active_source"].as<const char *>());
                copyStr_(avr_cache->target_source, sizeof(avr_cache->target_source), data_obj["target_source"].as<const char *>());
                copyStr_(avr_cache->fault, sizeof(avr_cache->fault), data_obj["fault"].as<const char *>());
                avr_cache->has_data = true;
                avr_cache->last_ok = true;
                avr_cache->node_id = node_id;
            }
        }

        if (leak_cache)
        {
            leak_cache->updated_ms = millis();
            if (!ok)
            {
                leak_cache->pending = false;
                leak_cache->last_ok = false;
                leak_cache->last_error = "";
                leak_cache->last_error = doc["error"] | "error";
            }
            else
            {
                const uint16_t part = data_obj["part"] | 1;
                const uint16_t parts = data_obj["parts"] | 1;
                const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : (part >= parts);
                if (part <= 1)
                {
                    leak_cache->item_count = 0;
                    leak_cache->has_data = false;
                    leak_cache->last_ok = false;
                    leak_cache->last_error = "";
                }
                if (!items.isNull())
                {
                    for (JsonObjectConst item : items)
                    {
                        if (leak_cache->item_count >= LeakController::kZoneCount)
                            break;
                        if (!item["id"].is<unsigned>())
                            continue;
                        StackLeakItem &dst = leak_cache->items[leak_cache->item_count++];
                        dst.id = (uint8_t)item["id"].as<unsigned>();
                        dst.enabled = item["enabled"] | false;
                        dst.power_on = item["power_on"] | false;
                        dst.sensor_active_low = item["sensor_active_low"] | true;
                        dst.sensor = (uint8_t)(item["sensor"] | LeakController::kInvalidPort);
                        dst.valve = (uint8_t)(item["valve"] | LeakController::kInvalidPort);
                        dst.alarm = (uint8_t)(item["alarm"] | LeakController::kInvalidPort);
                        dst.wet = item["wet"] | false;
                        dst.alarm_latched = item["alarm_latched"] | false;
                        copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                    }
                }
                if (done)
                {
                    leak_cache->pending = false;
                    leak_cache->has_data = true;
                    leak_cache->last_ok = true;
                    leak_cache->node_id = node_id;
                }
                else
                {
                    leak_cache->pending = true;
                }
            }
        }

        if (watering_cache)
        {
            watering_cache->pending = false;
            watering_cache->updated_ms = millis();
            watering_cache->last_ok = false;
            watering_cache->last_error = "";
            if (!ok)
            {
                watering_cache->last_error = doc["error"] | "error";
            }
            else if (!items.isNull())
            {
                const uint16_t total = doc["data"]["total"] | 0u;
                const uint16_t offset = doc["data"]["offset"] | 0u;
                const uint16_t count = doc["data"]["count"] | (uint16_t)items.size();
                if (offset == 0)
                {
                    watering_cache->item_count = 0;
                    watering_cache->total_expected = total;
                    watering_cache->next_offset = 0;
                }
                for (JsonObjectConst item : items)
                {
                    if (watering_cache->item_count >= WateringController::kRuleCount)
                        break;
                    if (!item["id"].is<unsigned>())
                        continue;
                    StackWateringItem &dst = watering_cache->items[watering_cache->item_count++];
                    dst.id = (uint8_t)item["id"].as<unsigned>();
                    dst.enabled = item["enabled"] | false;
                    dst.status = item["status"] | false;
                    dst.port = (uint8_t)(item["port"] | WateringController::kInvalidPort);
                    dst.tank_id = (uint8_t)(item["tank"] | 0u);
                    dst.weekdays_mask = (uint8_t)(item["weekdays_mask"] | 0u);
                    dst.hour = (uint8_t)(item["hour"] | 0u);
                    dst.minute = (uint8_t)(item["minute"] | 0u);
                    dst.duration_sec = item["duration_s"] | 0u;
                    dst.hour2 = (uint8_t)(item["hour2"] | 0u);
                    dst.minute2 = (uint8_t)(item["minute2"] | 0u);
                    dst.duration2_sec = item["duration2_s"] | 0u;
                    dst.hour3 = (uint8_t)(item["hour3"] | 0u);
                    dst.minute3 = (uint8_t)(item["minute3"] | 0u);
                    dst.duration3_sec = item["duration3_s"] | 0u;
                    dst.resume_after_refill = item["resume"] | false;
                    dst.resume_level = (uint8_t)(item["resume_level"] | 0u);
                    dst.active = item["active"] | false;
                    dst.paused = item["paused"] | false;
                    dst.remaining_ms = item["remaining_ms"] | 0u;
                    copyStr_(dst.name, sizeof(dst.name), item["name"].as<const char *>());
                }
                watering_cache->has_data = true;
                watering_cache->last_ok = true;
                watering_cache->node_id = node_id;
                if (total > 0 && (uint16_t)watering_cache->item_count < total)
                {
                    watering_cache->next_offset = offset + count;
                    requestStackWateringPage_(node_id, *watering_cache);
                }
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

        if (temp_sensors_cache)
        {
            temp_sensors_cache->updated_ms = millis();
            if (!ok)
            {
                temp_sensors_cache->pending = false;
                temp_sensors_cache->pending_cmd_id = 0;
                temp_sensors_cache->last_ok = false;
                temp_sensors_cache->last_error = doc["error"] | "error";
                temp_sensors_cache->next_offset = 0;
                temp_sensors_cache->page_limit = 0;
            }
            else
            {
                const bool has_offset = data_obj["offset"].is<unsigned>() || data_obj["offset"].is<int>();
                const bool has_limit = data_obj["limit"].is<unsigned>() || data_obj["limit"].is<int>();
                JsonArrayConst ts_items = data_obj["items"].as<JsonArrayConst>();
                if (!(has_offset || has_limit))
                {
                    temp_sensors_cache->pending = false;
                    temp_sensors_cache->pending_cmd_id = 0;
                    temp_sensors_cache->last_ok = false;
                    temp_sensors_cache->last_error = "temp_sensors page fields missing";
                    temp_sensors_cache->next_offset = 0;
                    temp_sensors_cache->page_limit = 0;
                }
                else if (!ts_items.isNull())
                {
                    const uint16_t offset = (uint16_t)(data_obj["offset"] | 0u);
                    const uint16_t limit = (uint16_t)(data_obj["limit"] |
                                                     (temp_sensors_cache->page_limit ? temp_sensors_cache->page_limit : 6u));
                    const uint16_t total = (uint16_t)(data_obj["total"] | 0u);
                    const bool done = data_obj["done"].is<bool>() ? data_obj["done"].as<bool>() : false;
                    if (offset == 0)
                    {
                        temp_sensors_cache->item_count = 0;
                        temp_sensors_cache->has_data = false;
                        temp_sensors_cache->last_ok = false;
                        temp_sensors_cache->last_error = "";
                    }
                    if ((size_t)offset > temp_sensors_cache->item_count)
                    {
                        // Sequential page pull expected. If offset jumps ahead, resync from scratch on next cycle.
                        temp_sensors_cache->pending = false;
                        temp_sensors_cache->pending_cmd_id = 0;
                        temp_sensors_cache->last_ok = false;
                        temp_sensors_cache->last_error = "temp_sensors offset mismatch";
                        temp_sensors_cache->next_offset = 0;
                        temp_sensors_cache->page_limit = 0;
                    }
                    else
                    {
                        for (JsonObjectConst item : ts_items)
                        {
                            if (temp_sensors_cache->item_count >= temp_sensors_cache->capacity)
                                break;
                            if (!item["addr"].is<const char *>())
                                continue;
                            StackTempSensorItem &dst = temp_sensors_cache->items[temp_sensors_cache->item_count++];
                            copyStr_(dst.addr, sizeof(dst.addr), item["addr"].as<const char *>());
                            dst.used = item["used"] | false;
                        }
                        temp_sensors_cache->has_data = (temp_sensors_cache->item_count > 0);
                        temp_sensors_cache->node_id = node_id;
                        if (done || temp_sensors_cache->item_count >= temp_sensors_cache->capacity ||
                            (total > 0 && temp_sensors_cache->item_count >= total))
                        {
                            temp_sensors_cache->pending = false;
                            temp_sensors_cache->pending_cmd_id = 0;
                            temp_sensors_cache->last_ok = true;
                            temp_sensors_cache->last_error = "";
                            temp_sensors_cache->next_offset = 0;
                            temp_sensors_cache->page_limit = 0;
                        }
                        else
                        {
                            const uint16_t next_offset = (uint16_t)(data_obj["next_offset"] |
                                                                    (uint16_t)(offset + ts_items.size()));
                            const uint16_t next_limit = limit ? limit : (temp_sensors_cache->page_limit ? temp_sensors_cache->page_limit : 6u);
                            temp_sensors_cache->pending = true;
                            temp_sensors_cache->last_ok = false;
                            temp_sensors_cache->last_error = "";
                            if (!sendStackTempSensorsPage_(*temp_sensors_cache, node_id, next_offset, next_limit))
                            {
                                temp_sensors_cache->pending = false;
                                temp_sensors_cache->pending_cmd_id = 0;
                                temp_sensors_cache->last_error = "temp_sensors next page send fail";
                            }
                        }
                    }
                }
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
                status_cache->fan_on_c = data["fan_on_c"] | (data["on_c"] | 0.0f);
                status_cache->fan_hyst_c = data["fan_hyst_c"] | (data["hyst_c"] | 0.0f);
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
                status_cache->rtc_date = data["rtc_date"] | (data["date"] | "");
                status_cache->rtc_time = data["rtc_time"] | (data["time"] | "");
                status_cache->rtc_temp = data["rtc_temp"] | (data["temp_c"] | 0.0f);
                status_cache->rtc_weekday = (uint8_t)(data["rtc_weekday"] | (data["weekday"] | 0u));
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
        {
            if (cache->updated_ms && (uint32_t)(now - cache->updated_ms) > 6000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "get";
        doc["params"]["chunk"] = 16;
        char payload[160] = {};
        const size_t need = measureJson(doc);
        if (need >= sizeof(payload))
        {
            if (_log)
                _log->warn(F("STACK"), F("Sync req too large: item: sockets bytes: %u cap: %u"),
                           (unsigned)need, (unsigned)sizeof(payload));
            return false;
        }
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->updated_ms = now;
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
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "get_lights";
        doc["params"]["chunk"] = 3;
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->pending_since_ms = now;
        cache->updated_ms = now;
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 6000u)
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
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "get";
        doc["params"]["chunk"] = 3;
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->pending_since_ms = now;
        return true;
    }

    bool requestStackSecurityPrearm_(uint32_t node_id, bool force)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSecurityPrearmCache *cache = findStackSecurityPrearmCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 6000u)
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
        if (!force && cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Security;
        doc["action"] = "prearm";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->pending_since_ms = now;
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 6000u)
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
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Meteo;
        doc["action"] = "get";
        doc["params"]["chunk"] = 3;
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->pending_since_ms = now;
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
        {
            if ((uint32_t)(now - cache->updated_ms) > 6000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Thermo;
        doc["action"] = "get";
        doc["params"]["chunk"] = 3;
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->updated_ms = now;
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 6000u)
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
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Septic;
        doc["action"] = "get";
        doc["params"]["chunk"] = 3;
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->pending_since_ms = now;
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
        {
            if (cache->pending_since_ms && (uint32_t)(now - cache->pending_since_ms) > 6000u)
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
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Tanks;
        doc["action"] = "get";
        doc["params"]["chunk"] = 3;
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->pending_since_ms = now;
        return true;
    }

    bool requestStackAvr_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackAvrCache *cache = findStackAvrCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
        {
            if ((uint32_t)(now - cache->updated_ms) > 6000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Avr;
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
        cache->updated_ms = now;
        return true;
    }

    bool requestStackLeak_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackLeakCache *cache = findStackLeakCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
        {
            if ((uint32_t)(now - cache->updated_ms) > 6000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Leak;
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
        cache->updated_ms = now;
        return true;
    }

    bool requestStackWatering_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackWateringCache *cache = findStackWateringCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
        {
            if ((uint32_t)(now - cache->updated_ms) > 6000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        cache->next_offset = 0;
        cache->total_expected = 0;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Watering;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["offset"] = cache->next_offset;
        params["limit"] = kWateringPageSize;
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->updated_ms = now;
        return true;
    }

    bool requestStackWateringPage_(uint32_t node_id, StackWateringCache &cache)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        if (cache.pending)
        {
            const uint32_t now = millis();
            if ((uint32_t)(now - cache.updated_ms) > 6000u)
            {
                cache.pending = false;
                cache.pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Watering;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["offset"] = cache.next_offset;
        params["limit"] = kWateringPageSize;
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache.pending = true;
        cache.pending_cmd_id = cmd_id;
        cache.updated_ms = millis();
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
        {
            if ((uint32_t)(now - cache->updated_ms) > 6000u)
            {
                if (cache->parts_received > 0)
                {
                    size_t w = 0;
                    for (size_t i = 0; i < PortIO::PORT_COUNT; ++i)
                    {
                        if (!cache->present[i])
                            continue;
                        if (w != i)
                            cache->items[w] = cache->items[i];
                        ++w;
                    }
                    cache->item_count = w;
                    cache->has_data = (w > 0);
                    cache->last_ok = false;
                    cache->last_error = "ports timeout partial";
                    cache->updated_ms = now;
                }
                cache->pending = false;
                cache->pending_cmd_id = 0;
                cache->parts_expected = 0;
                cache->parts_received = 0;
                cache->next_offset = 0;
                memset(cache->part_seen, 0, sizeof(cache->part_seen));
                memset(cache->present, 0, sizeof(cache->present));
            }
            else
            {
                return false;
            }
        }
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        cache->parts_expected = 0;
        cache->parts_received = 0;
        cache->next_offset = 0;
        cache->page_limit = 6;
        // Keep the previous cache visible until the first page of a new request arrives.
        // The page parser (offset==0) will atomically clear and refill the cache.
        memset(cache->part_seen, 0, sizeof(cache->part_seen));
        if (!sendStackPortsPage_(*cache, node_id, 0, cache->page_limit))
            return false;
        cache->pending = true;
        cache->updated_ms = now;
        return true;
    }

    bool sendStackPortsPage_(StackPortsCache &cache, uint32_t node_id, uint16_t offset, uint16_t limit)
    {
        if (!_stack_master)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Ports;
        doc["action"] = "get_state";
        doc["params"]["brief"] = true;
        doc["params"]["offset"] = offset;
        doc["params"]["limit"] = limit;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache.pending_cmd_id = cmd_id;
        cache.pending = true;
        cache.next_offset = offset;
        cache.page_limit = limit;
        cache.updated_ms = millis();
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
        {
            if ((uint32_t)(now - cache->updated_ms) > 6000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->updated_ms = now;
        return true;
    }

    void invalidateStackPortsCache_(uint32_t node_id)
    {
        StackPortsCache *cache = findStackPortsCache_(node_id, false);
        if (!cache)
            return;
        cache->pending = false;
        cache->pending_cmd_id = 0;
        cache->parts_expected = 0;
        cache->parts_received = 0;
        cache->next_offset = 0;
        cache->page_limit = 0;
        cache->has_data = false;
        cache->last_ok = false;
        cache->last_error = "";
        cache->updated_ms = 0;
        cache->item_count = 0;
        memset(cache->part_seen, 0, sizeof(cache->part_seen));
        memset(cache->present, 0, sizeof(cache->present));
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
        {
            if ((uint32_t)(now - cache->updated_ms) > 6000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->updated_ms = now;
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
        {
            if ((uint32_t)(now - cache->updated_ms) > 6000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
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
        cache->updated_ms = now;
        return true;
    }

    bool requestStackTempSensors_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackTempSensorsCache *cache = findStackTempSensorsCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
        {
            if ((uint32_t)(now - cache->updated_ms) > 6000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
                cache->next_offset = 0;
                cache->page_limit = 0;
                if (cache->item_count > 0)
                {
                    cache->has_data = true;
                    cache->last_ok = false;
                    cache->last_error = "temp_sensors timeout partial";
                }
            }
            else
            {
                return false;
            }
        }
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        cache->next_offset = 0;
        cache->page_limit = 8;
        cache->last_error = "";
        cache->updated_ms = now;
        if (!sendStackTempSensorsPage_(*cache, node_id, 0, cache->page_limit))
            return false;
        cache->pending = true;
        return true;
    }

    bool sendStackTempSensorsPage_(StackTempSensorsCache &cache, uint32_t node_id, uint16_t offset, uint16_t limit)
    {
        if (!_stack_master)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::TempSensors;
        doc["action"] = "list_page";
        doc["params"]["offset"] = offset;
        doc["params"]["limit"] = limit;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache.pending = true;
        cache.pending_cmd_id = cmd_id;
        cache.next_offset = offset;
        cache.page_limit = limit;
        cache.updated_ms = millis();
        return true;
    }

    void invalidateStackTempSensorsCache_(uint32_t node_id)
    {
        StackTempSensorsCache *cache = findStackTempSensorsCache_(node_id, false);
        if (!cache)
            return;
        cache->pending = false;
        cache->pending_cmd_id = 0;
        cache->next_offset = 0;
        cache->page_limit = 0;
        cache->has_data = false;
        cache->last_ok = false;
        cache->last_error = "";
        cache->updated_ms = 0;
        cache->item_count = 0;
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
        {
            if (cache->plc_updated_ms && (uint32_t)(now - cache->plc_updated_ms) > 4000u)
            {
                cache->pending_plc = false;
                cache->pending_plc_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
        if (cache->has_plc && (uint32_t)(now - cache->plc_updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::PlcStatus;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending_plc = true;
        cache->pending_plc_cmd_id = cmd_id;
        cache->plc_updated_ms = now;
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
        {
            if (cache->rtc_updated_ms && (uint32_t)(now - cache->rtc_updated_ms) > 4000u)
            {
                cache->pending_rtc = false;
                cache->pending_rtc_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
        if (cache->has_rtc && (uint32_t)(now - cache->rtc_updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Rtc;
        doc["action"] = "get_time";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending_rtc = true;
        cache->pending_rtc_cmd_id = cmd_id;
        cache->rtc_updated_ms = now;
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

    StackTempSensorsCache *findStackTempSensorsCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_temp_sensors_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_temp_sensors_cache)
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

    const StackTempSensorsCache *findStackTempSensorsCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackTempSensorsCache_(node_id, create);
    }

    StackTempSensorsCache *findStackTempSensorsCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_temp_sensors_cache)
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

    StackSecurityPrearmCache *findStackSecurityPrearmCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_security_prearm_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_security_prearm_cache)
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

    const StackSecurityPrearmCache *findStackSecurityPrearmCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackSecurityPrearmCache_(node_id, create);
    }

    StackSecurityPrearmCache *findStackSecurityPrearmCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_security_prearm_cache)
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

    StackAvrCache *findStackAvrCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_avr_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_avr_cache)
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

    const StackAvrCache *findStackAvrCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackAvrCache_(node_id, create);
    }

    StackAvrCache *findStackAvrCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_avr_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackLeakCache *findStackLeakCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_leak_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_leak_cache)
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

    const StackLeakCache *findStackLeakCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackLeakCache_(node_id, create);
    }

    StackLeakCache *findStackLeakCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_leak_cache)
            if (c.pending && c.pending_cmd_id == cmd_id)
                return &c;
        return nullptr;
    }

    StackWateringCache *findStackWateringCache_(uint32_t node_id, bool create)
    {
        if (!_stack_master)
            return nullptr;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return nullptr;
        if (node_id == 0)
            return nullptr;
        for (auto &c : _stack_watering_cache)
            if (c.node_id == node_id)
                return &c;
        if (!create)
            return nullptr;
        for (auto &c : _stack_watering_cache)
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

    const StackWateringCache *findStackWateringCache_(uint32_t node_id, bool create) const
    {
        return const_cast<StackCache *>(this)->findStackWateringCache_(node_id, create);
    }

    StackWateringCache *findStackWateringCacheByCmd_(uint16_t cmd_id)
    {
        if (cmd_id == 0)
            return nullptr;
        for (auto &c : _stack_watering_cache)
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
    uint8_t _tx_payload_buf[StackCodec::kMaxPayload] = {};
    StackSocketsCache _stack_sockets_cache[StackMaster::MAX_SESSIONS] = {};
    StackLightsCache _stack_lights_cache[StackMaster::MAX_SESSIONS] = {};
    StackPortsCache _stack_ports_cache[StackMaster::MAX_SESSIONS] = {};
    StackExtendersCache _stack_ext_cache[StackMaster::MAX_SESSIONS] = {};
    StackI2cCache _stack_i2c_cache[StackMaster::MAX_SESSIONS] = {};
    StackOwCache _stack_ow_cache[StackMaster::MAX_SESSIONS] = {};
    StackTempSensorsCache _stack_temp_sensors_cache[StackMaster::MAX_SESSIONS] = {};
    StackSecurityCache _stack_security_cache[StackMaster::MAX_SESSIONS] = {};
    StackSecurityPrearmCache _stack_security_prearm_cache[StackMaster::MAX_SESSIONS] = {};
    StackMeteoCache _stack_meteo_cache[StackMaster::MAX_SESSIONS] = {};
    StackThermoCache _stack_thermo_cache[StackMaster::MAX_SESSIONS] = {};
    StackSepticCache _stack_septic_cache[StackMaster::MAX_SESSIONS] = {};
    StackTankCache _stack_tanks_cache[StackMaster::MAX_SESSIONS] = {};
    StackAvrCache _stack_avr_cache[StackMaster::MAX_SESSIONS] = {};
    StackLeakCache _stack_leak_cache[StackMaster::MAX_SESSIONS] = {};
    StackWateringCache _stack_watering_cache[StackMaster::MAX_SESSIONS] = {};
    StackNodeStatusCache _stack_status_cache[StackMaster::MAX_SESSIONS] = {};
    uint16_t _stack_cmd_id = 0;
};
