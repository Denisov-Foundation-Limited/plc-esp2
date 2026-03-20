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

#include <AsyncTCP.h>

#include "controllers/avr_controller.hpp"
#include "controllers/controllers.hpp"
#include "controllers/leak_controller.hpp"
#include "controllers/meteo_controller.hpp"
#include "controllers/ring_controller.hpp"
#include "controllers/security_controller.hpp"
#include "controllers/septic_controller.hpp"
#include "controllers/socket_controller.hpp"
#include "controllers/tank_controller.hpp"
#include "controllers/thermo_controller.hpp"
#include "controllers/watering_controller.hpp"
#include "core/rtc.hpp"
#include "hal/bus/i2c.hpp"
#include "hal/bus/onewire.hpp"
#include "hal/ds18b20.hpp"
#include "hal/gpio/extender.hpp"
#include "hal/gpio/portio.hpp"
#include "hal/io_stack.hpp"
#include "plc/plc_control.hpp"
#include "utils/configs_manager_iface.hpp"
#include "utils/logger.hpp"

enum class StackMsgType : uint8_t
{
    Hello = 1,
    Features = 2,
    Status = 3,
    CmdSet = 4,
    CmdGet = 5,
    Ack = 6,
    Err = 7
};

enum class StackFeature : uint8_t
{
    System = 0x01,
    Ports = 0x02,
    TempSensors = 0x03,
    I2cScan = 0x04,
    OwScan = 0x05,
    Fan = 0x06,
    Rtc = 0x07,
    PlcStatus = 0x08,
    Relays = 0x09,
    DigitalInputs = 0x0A,
    Telegram = 0x0B,
    Storage = 0x0C,
    Extenders = 0x0D,
    Sockets = 0x0E,
    Meteo = 0x0F,
    Thermo = 0x10,
    Security = 0x11,
    Septic = 0x12,
    Tanks = 0x13,
    Ring = 0x14,
    Watering = 0x15,
    Avr = 0x16,
    Leak = 0x17,
    Groups = 0x18
};

enum StackCaps : uint32_t
{
    StackCapController = 1u << 0
};

struct StackFrame
{
    uint8_t type = 0;
    const uint8_t *payload = nullptr;
    size_t payload_len = 0;
};

class StackCodec
{
public:
    static constexpr size_t kMaxPayload = 1024;
};

class StackMaster
{
public:
    static constexpr size_t MAX_SESSIONS = 8;

    using FrameHandler = void (*)(void *ctx, uint32_t node_id, const StackFrame &frame);
    using EventHandler = void (*)(void *ctx, uint32_t node_id, bool online);

    StackMaster(AsyncServer &server, Logger &log) : _server(&server), _log(&log) {}

    void setFrameHandler(FrameHandler cb, void *ctx)
    {
        _frame_cb = cb;
        _frame_ctx = ctx;
    }

    void setFrameHandlerSecondary(FrameHandler cb, void *ctx)
    {
        _frame_cb_secondary = cb;
        _frame_ctx_secondary = ctx;
    }

    void setFrameHandlerTertiary(FrameHandler cb, void *ctx)
    {
        _frame_cb_tertiary = cb;
        _frame_ctx_tertiary = ctx;
    }

    void setEventHandler(EventHandler cb, void *ctx)
    {
        _event_cb = cb;
        _event_ctx = ctx;
    }

    void setConfigsManager(ConfigsManagerIface &cfg) { _configs = &cfg; }

    void begin() {}
    void loop() {}
    void setSessionSilenceTimeoutMs(uint32_t ms) { (void)ms; }

    size_t nodeCount() const { return 0; }
    uint32_t nodeIdAt(size_t idx) const
    {
        (void)idx;
        return 0;
    }
    String nodeNameAt(size_t idx) const
    {
        (void)idx;
        return String();
    }
    uint32_t nodeCapsAt(size_t idx) const
    {
        (void)idx;
        return 0;
    }
    bool nodeIsControllerAt(size_t idx) const
    {
        (void)idx;
        return false;
    }
    bool nodeIsController(uint32_t node_id) const
    {
        (void)node_id;
        return false;
    }
    String nodeIpAt(size_t idx) const
    {
        (void)idx;
        return String();
    }
    bool nodeInfo(uint32_t node_id, String &name, String &ip, uint16_t &fw_ver) const
    {
        (void)node_id;
        name = "";
        ip = "";
        fw_ver = 0;
        return false;
    }
    bool nodeIsOnline(uint32_t node_id, uint32_t max_silence_ms = 0) const
    {
        (void)node_id;
        (void)max_silence_ms;
        return false;
    }
    bool sendTo(uint32_t node_id, uint8_t type, const uint8_t *payload, size_t len)
    {
        (void)node_id;
        (void)type;
        (void)payload;
        (void)len;
        return false;
    }
    void broadcast(uint8_t type, const uint8_t *payload, size_t len)
    {
        (void)type;
        (void)payload;
        (void)len;
    }

private:
    AsyncServer *_server = nullptr;
    Logger *_log = nullptr;
    ConfigsManagerIface *_configs = nullptr;
    FrameHandler _frame_cb = nullptr;
    void *_frame_ctx = nullptr;
    FrameHandler _frame_cb_secondary = nullptr;
    void *_frame_ctx_secondary = nullptr;
    FrameHandler _frame_cb_tertiary = nullptr;
    void *_frame_ctx_tertiary = nullptr;
    EventHandler _event_cb = nullptr;
    void *_event_ctx = nullptr;
};

class StackNode
{
public:
    using FrameHandler = void (*)(void *ctx, const StackFrame &frame);
    using StatusProvider = size_t (*)(void *ctx, uint8_t *out, size_t cap);

    explicit StackNode(Logger &log) : _log(&log) {}

    void setFrameHandler(FrameHandler cb, void *ctx)
    {
        _frame_cb = cb;
        _frame_ctx = ctx;
    }

    void setNodeId(uint32_t id) { _node_id = id; }
    void setDeviceName(const String &name) { _device_name = name; }
    void setCaps(uint32_t caps) { _caps = caps; }
    void setServer(const String &host, uint16_t port)
    {
        _host = host;
        _port = port;
    }

    void setReconnectMs(uint32_t ms) { _reconnect_ms = ms; }
    void setHelloIntervalMs(uint32_t ms) { _hello_interval_ms = ms; }
    void setStatusIntervalMs(uint32_t ms) { _status_interval_ms = ms; }

    void setStatusProvider(StatusProvider cb, void *ctx)
    {
        _status_cb = cb;
        _status_ctx = ctx;
    }

    void disconnect() {}

    const String &host() const { return _host; }
    uint16_t port() const { return _port; }

    void begin() {}
    void loop() {}

    bool send(uint8_t type, const uint8_t *payload, size_t len)
    {
        (void)type;
        (void)payload;
        (void)len;
        return false;
    }

    bool connected() const { return false; }
    bool helloSentCurrentConnection() const { return false; }

    bool sendHello(uint16_t fw_ver = 0, uint32_t caps = 0xFFFFFFFFu)
    {
        (void)fw_ver;
        (void)caps;
        return false;
    }

private:
    String _host;
    uint16_t _port = 0;
    uint32_t _node_id = 0;
    String _device_name;
    uint32_t _caps = 0;
    uint32_t _reconnect_ms = 3000;
    uint32_t _hello_interval_ms = 15000;
    uint32_t _status_interval_ms = 2000;
    FrameHandler _frame_cb = nullptr;
    void *_frame_ctx = nullptr;
    StatusProvider _status_cb = nullptr;
    void *_status_ctx = nullptr;
    Logger *_log = nullptr;
};

class StackCache
{
public:
    static constexpr uint16_t kWateringPageSize = 1;

    struct StackSocketItem
    {
        uint8_t id = 0;
        uint8_t group_id = 0;
        bool enabled = false;
        bool state = false;
        uint8_t button_port = SocketController::kInvalidPort;
        uint8_t relay_port = SocketController::kInvalidPort;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };

    struct StackGroupItem
    {
        uint8_t id = 0;
        uint16_t sort = 0;
        static constexpr size_t kNameLen = 48;
        char name[kNameLen] = {};
    };

    struct StackGroupsCache
    {
        static constexpr size_t kCapacity = 16;
        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        uint16_t pending_cmd_id = 0;
        bool pending = false;
        bool has_data = false;
        bool last_ok = false;
        String last_error;
        StackGroupItem *items = nullptr;
        size_t capacity = kCapacity;
        size_t item_count = 0;
        void reset()
        {
            *this = StackGroupsCache{};
        }
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
            *this = StackSocketsCache{};
        }
    };

    struct StackLightItem
    {
        uint8_t id = 0;
        uint8_t group_id = 0;
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
            *this = StackLightsCache{};
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
            *this = StackPortsCache{};
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
            *this = StackExtendersCache{};
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
            *this = StackI2cCache{};
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
            *this = StackOwCache{};
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
            *this = StackTempSensorsCache{};
        }
    };

    struct StackSecuritySensorItem
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
            *this = StackSecurityPrearmCache{};
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
            *this = StackSecurityCache{};
            siren = SecurityController::kInvalidPort;
        }
    };

    struct StackMeteoItem
    {
        uint8_t id = 0;
        uint8_t group_id = 0;
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
            *this = StackMeteoCache{};
        }
    };

    struct StackThermoItem
    {
        uint8_t id = 0;
        uint8_t group_id = 0;
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
            *this = StackThermoCache{};
        }
    };

    struct StackSepticItem
    {
        uint8_t id = 0;
        uint8_t group_id = 0;
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
            *this = StackSepticCache{};
        }
    };

    struct StackTankItem
    {
        uint8_t id = 0;
        uint8_t group_id = 0;
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
            *this = StackTankCache{};
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
        static constexpr size_t kTankNameLen = 48;
        char tank_name[kTankNameLen] = {};
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
            *this = StackWateringCache{};
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
            *this = StackAvrCache{};
            auto_mode = true;
            prefer_main = true;
            auto_return_main = true;
            main_ok_port = AvrController::kInvalidPort;
            reserve_ok_port = AvrController::kInvalidPort;
            relay_main_port = AvrController::kInvalidPort;
            relay_reserve_port = AvrController::kInvalidPort;
            feedback_main_port = AvrController::kInvalidPort;
            feedback_reserve_port = AvrController::kInvalidPort;
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
            *this = StackLeakCache{};
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
        bool fan_on = false;
        float fan_on_c = 0.0f;
        float fan_hyst_c = 0.0f;
        String rtc_date;
        String rtc_time;
        float rtc_temp = 0.0f;
        uint8_t rtc_weekday = 0;
        void reset()
        {
            *this = StackNodeStatusCache{};
        }
    };

    StackCache() { resetAll_(); }

    void setStackMaster(StackMaster *master) { _stack_master = master; }
    void setConfigsManager(ConfigsManagerIface *cfg) { _configs = cfg; }
    void setLogger(Logger *log) { _log = log; }
    void setMasterOverride(bool enabled) { (void)enabled; }
    void initAllocations() { resetAll_(); }
    void logAllocations() {}

    StackSocketsCache *socketsCache(uint32_t node_id) { return findCache_(_stack_sockets_cache, node_id); }
    const StackSocketsCache *socketsCache(uint32_t node_id) const { return findCache_(_stack_sockets_cache, node_id); }
    bool requestSockets(uint32_t node_id) { return markDisabled_(socketsCache(node_id)); }

    StackGroupsCache *groupsCache(uint32_t node_id) { return findCache_(_stack_groups_cache, node_id); }
    const StackGroupsCache *groupsCache(uint32_t node_id) const { return findCache_(_stack_groups_cache, node_id); }
    bool requestGroups(uint32_t node_id) { return markDisabled_(groupsCache(node_id)); }

    StackLightsCache *lightsCache(uint32_t node_id) { return findCache_(_stack_lights_cache, node_id); }
    const StackLightsCache *lightsCache(uint32_t node_id) const { return findCache_(_stack_lights_cache, node_id); }
    bool requestLights(uint32_t node_id) { return markDisabled_(lightsCache(node_id)); }

    StackPortsCache *portsCache(uint32_t node_id) { return findCache_(_stack_ports_cache, node_id); }
    const StackPortsCache *portsCache(uint32_t node_id) const { return findCache_(_stack_ports_cache, node_id); }
    bool requestPorts(uint32_t node_id) { return markDisabled_(portsCache(node_id)); }
    void invalidatePorts(uint32_t node_id)
    {
        if (auto *cache = portsCache(node_id))
            cache->reset();
    }

    StackExtendersCache *extendersCache(uint32_t node_id) { return findCache_(_stack_ext_cache, node_id); }
    const StackExtendersCache *extendersCache(uint32_t node_id) const { return findCache_(_stack_ext_cache, node_id); }
    bool requestExtenders(uint32_t node_id) { return markDisabled_(extendersCache(node_id)); }

    StackI2cCache *i2cCache(uint32_t node_id) { return findCache_(_stack_i2c_cache, node_id); }
    const StackI2cCache *i2cCache(uint32_t node_id) const { return findCache_(_stack_i2c_cache, node_id); }
    bool requestI2c(uint32_t node_id, bool run)
    {
        (void)run;
        return markDisabled_(i2cCache(node_id));
    }

    StackOwCache *owCache(uint32_t node_id) { return findCache_(_stack_ow_cache, node_id); }
    const StackOwCache *owCache(uint32_t node_id) const { return findCache_(_stack_ow_cache, node_id); }
    bool requestOw(uint32_t node_id, bool run)
    {
        (void)run;
        return markDisabled_(owCache(node_id));
    }

    StackTempSensorsCache *tempSensorsCache(uint32_t node_id) { return findCache_(_stack_temp_sensors_cache, node_id); }
    const StackTempSensorsCache *tempSensorsCache(uint32_t node_id) const { return findCache_(_stack_temp_sensors_cache, node_id); }
    bool requestTempSensors(uint32_t node_id) { return markDisabled_(tempSensorsCache(node_id)); }
    void invalidateTempSensors(uint32_t node_id)
    {
        if (auto *cache = tempSensorsCache(node_id))
            cache->reset();
    }

    StackSecurityCache *securityCache(uint32_t node_id) { return findCache_(_stack_security_cache, node_id); }
    const StackSecurityCache *securityCache(uint32_t node_id) const { return findCache_(_stack_security_cache, node_id); }
    StackSecurityPrearmCache *securityPrearmCache(uint32_t node_id) { return findCache_(_stack_security_prearm_cache, node_id); }
    const StackSecurityPrearmCache *securityPrearmCache(uint32_t node_id) const { return findCache_(_stack_security_prearm_cache, node_id); }
    bool requestSecurity(uint32_t node_id) { return markDisabled_(securityCache(node_id)); }
    bool requestSecurityPrearm(uint32_t node_id) { return markDisabled_(securityPrearmCache(node_id)); }
    bool requestSecurityPrearmForce(uint32_t node_id) { return markDisabled_(securityPrearmCache(node_id)); }

    StackMeteoCache *meteoCache(uint32_t node_id) { return findCache_(_stack_meteo_cache, node_id); }
    const StackMeteoCache *meteoCache(uint32_t node_id) const { return findCache_(_stack_meteo_cache, node_id); }
    size_t meteoCacheSlots() const { return MAX_SESSIONS; }
    StackMeteoCache &meteoCacheAt(size_t idx) { return _stack_meteo_cache[(idx < MAX_SESSIONS) ? idx : 0]; }
    const StackMeteoCache &meteoCacheAt(size_t idx) const { return _stack_meteo_cache[(idx < MAX_SESSIONS) ? idx : 0]; }
    bool requestMeteo(uint32_t node_id) { return markDisabled_(meteoCache(node_id)); }

    StackThermoCache *thermoCache(uint32_t node_id) { return findCache_(_stack_thermo_cache, node_id); }
    const StackThermoCache *thermoCache(uint32_t node_id) const { return findCache_(_stack_thermo_cache, node_id); }
    bool requestThermo(uint32_t node_id) { return markDisabled_(thermoCache(node_id)); }

    StackSepticCache *septicCache(uint32_t node_id) { return findCache_(_stack_septic_cache, node_id); }
    const StackSepticCache *septicCache(uint32_t node_id) const { return findCache_(_stack_septic_cache, node_id); }
    bool requestSeptic(uint32_t node_id) { return markDisabled_(septicCache(node_id)); }

    StackTankCache *tanksCache(uint32_t node_id) { return findCache_(_stack_tanks_cache, node_id); }
    const StackTankCache *tanksCache(uint32_t node_id) const { return findCache_(_stack_tanks_cache, node_id); }
    bool requestTanks(uint32_t node_id) { return markDisabled_(tanksCache(node_id)); }

    StackWateringCache *wateringCache(uint32_t node_id) { return findCache_(_stack_watering_cache, node_id); }
    const StackWateringCache *wateringCache(uint32_t node_id) const { return findCache_(_stack_watering_cache, node_id); }
    bool requestWatering(uint32_t node_id) { return markDisabled_(wateringCache(node_id)); }

    StackAvrCache *avrCache(uint32_t node_id) { return findCache_(_stack_avr_cache, node_id); }
    const StackAvrCache *avrCache(uint32_t node_id) const { return findCache_(_stack_avr_cache, node_id); }
    bool requestAvr(uint32_t node_id) { return markDisabled_(avrCache(node_id)); }

    StackLeakCache *leakCache(uint32_t node_id) { return findCache_(_stack_leak_cache, node_id); }
    const StackLeakCache *leakCache(uint32_t node_id) const { return findCache_(_stack_leak_cache, node_id); }
    bool requestLeak(uint32_t node_id) { return markDisabled_(leakCache(node_id)); }

    StackNodeStatusCache *statusCache(uint32_t node_id) { return findCache_(_stack_status_cache, node_id); }
    const StackNodeStatusCache *statusCache(uint32_t node_id) const { return findCache_(_stack_status_cache, node_id); }
    bool requestPlcStatus(uint32_t node_id)
    {
        auto *cache = statusCache(node_id);
        if (!cache)
            return false;
        cache->pending_plc = false;
        cache->has_plc = false;
        cache->last_plc_ok = false;
        cache->plc_updated_ms = millis();
        cache->last_plc_error = "Stack disabled";
        return false;
    }
    bool requestRtcStatus(uint32_t node_id)
    {
        auto *cache = statusCache(node_id);
        if (!cache)
            return false;
        cache->pending_rtc = false;
        cache->has_rtc = false;
        cache->last_rtc_ok = false;
        cache->rtc_updated_ms = millis();
        cache->last_rtc_error = "Stack disabled";
        return false;
    }

    StackSocketItem *socketItem(StackSocketsCache &cache_ref, uint8_t id) { return findItem_(cache_ref, id); }
    StackLightItem *lightItem(StackLightsCache &cache_ref, uint8_t id) { return findItem_(cache_ref, id); }

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        (void)ctx;
        (void)node_id;
        (void)frame;
    }

private:
    template <typename CacheT, size_t N>
    static CacheT *findCache_(CacheT (&caches)[N], uint32_t node_id)
    {
        if (node_id == 0)
            return &caches[0];
        for (size_t i = 1; i < N; ++i)
        {
            if (caches[i].node_id == node_id)
                return &caches[i];
        }
        for (size_t i = 1; i < N; ++i)
        {
            if (caches[i].node_id == 0)
            {
                caches[i].reset();
                caches[i].node_id = node_id;
                return &caches[i];
            }
        }
        return nullptr;
    }

    template <typename CacheT, size_t N>
    static const CacheT *findCache_(const CacheT (&caches)[N], uint32_t node_id)
    {
        if (node_id == 0)
            return &caches[0];
        for (size_t i = 1; i < N; ++i)
        {
            if (caches[i].node_id == node_id)
                return &caches[i];
        }
        return nullptr;
    }

    template <typename CacheT>
    static bool markDisabled_(CacheT *cache)
    {
        if (!cache)
            return false;
        cache->pending = false;
        cache->has_data = false;
        cache->last_ok = false;
        cache->updated_ms = millis();
        cache->last_error = "Stack disabled";
        return false;
    }

    template <typename CacheT, typename ItemT>
    static ItemT *findItemById_(CacheT &cache_ref, uint8_t id)
    {
        if (!cache_ref.items)
            return nullptr;
        for (size_t i = 0; i < cache_ref.item_count; ++i)
        {
            if (cache_ref.items[i].id == id)
                return &cache_ref.items[i];
        }
        return nullptr;
    }

    static StackSocketItem *findItem_(StackSocketsCache &cache_ref, uint8_t id)
    {
        return findItemById_<StackSocketsCache, StackSocketItem>(cache_ref, id);
    }

    static StackLightItem *findItem_(StackLightsCache &cache_ref, uint8_t id)
    {
        return findItemById_<StackLightsCache, StackLightItem>(cache_ref, id);
    }

    void resetAll_()
    {
        for (size_t i = 0; i < MAX_SESSIONS; ++i)
        {
            _stack_groups_cache[i].reset();
            _stack_sockets_cache[i].reset();
            _stack_lights_cache[i].reset();
            _stack_ports_cache[i].reset();
            _stack_ext_cache[i].reset();
            _stack_i2c_cache[i].reset();
            _stack_ow_cache[i].reset();
            _stack_temp_sensors_cache[i].reset();
            _stack_security_cache[i].reset();
            _stack_security_prearm_cache[i].reset();
            _stack_meteo_cache[i].reset();
            _stack_thermo_cache[i].reset();
            _stack_septic_cache[i].reset();
            _stack_tanks_cache[i].reset();
            _stack_watering_cache[i].reset();
            _stack_avr_cache[i].reset();
            _stack_leak_cache[i].reset();
            _stack_status_cache[i] = StackNodeStatusCache{};
        }
    }

    static constexpr size_t MAX_SESSIONS = StackMaster::MAX_SESSIONS;

    StackMaster *_stack_master = nullptr;
    ConfigsManagerIface *_configs = nullptr;
    Logger *_log = nullptr;

    StackGroupsCache _stack_groups_cache[MAX_SESSIONS]{};
    StackSocketsCache _stack_sockets_cache[MAX_SESSIONS]{};
    StackLightsCache _stack_lights_cache[MAX_SESSIONS]{};
    StackPortsCache _stack_ports_cache[MAX_SESSIONS]{};
    StackExtendersCache _stack_ext_cache[MAX_SESSIONS]{};
    StackI2cCache _stack_i2c_cache[MAX_SESSIONS]{};
    StackOwCache _stack_ow_cache[MAX_SESSIONS]{};
    StackTempSensorsCache _stack_temp_sensors_cache[MAX_SESSIONS]{};
    StackSecurityCache _stack_security_cache[MAX_SESSIONS]{};
    StackSecurityPrearmCache _stack_security_prearm_cache[MAX_SESSIONS]{};
    StackMeteoCache _stack_meteo_cache[MAX_SESSIONS]{};
    StackThermoCache _stack_thermo_cache[MAX_SESSIONS]{};
    StackSepticCache _stack_septic_cache[MAX_SESSIONS]{};
    StackTankCache _stack_tanks_cache[MAX_SESSIONS]{};
    StackWateringCache _stack_watering_cache[MAX_SESSIONS]{};
    StackAvrCache _stack_avr_cache[MAX_SESSIONS]{};
    StackLeakCache _stack_leak_cache[MAX_SESSIONS]{};
    StackNodeStatusCache _stack_status_cache[MAX_SESSIONS]{};
};

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
            *this = RemoteMeteoCache{};
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
            *this = RemoteSocketsCache{};
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
            *this = RemoteLightsCache{};
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
            *this = RemoteSepticCache{};
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
            *this = RemoteThermoCache{};
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
            *this = RemoteTanksCache{};
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
            *this = RemoteSecurityCache{};
        }
    };

    StackSlaveHandler(IoStack &io, Ds18b20 &ds18b20, OneWireManager &ow, I2CManager &i2c,
                      PlcControl &plc, RTC &rtc, Logger &logs,
                      Extender &ext, SocketController &sockets, MeteoController &meteo,
                      ThermoController &thermo, SepticController &septic, SecurityController &security,
                      TankController &tanks, WateringController &watering, RingController &ring,
                      AvrController &avr, LeakController &leak, Controllers &controllers)
    {
        (void)io;
        (void)ds18b20;
        (void)ow;
        (void)i2c;
        (void)plc;
        (void)rtc;
        (void)logs;
        (void)ext;
        (void)sockets;
        (void)meteo;
        (void)thermo;
        (void)septic;
        (void)security;
        (void)tanks;
        (void)watering;
        (void)ring;
        (void)avr;
        (void)leak;
        (void)controllers;
        resetAll_();
    }

    void attach(StackNode &node)
    {
        (void)node;
        _node = &node;
    }

    void setTraceHandler(TraceHandler cb, void *ctx)
    {
        _trace_cb = cb;
        _trace_ctx = ctx;
    }

    bool nodeConnected() const { return false; }
    bool linkReadyAfterHello() const { return false; }
    void setConfigsManager(ConfigsManagerIface &cfg) { _configs = &cfg; }
    void initAllocations() { resetAll_(); }
    void loop() {}

    const RemoteMeteoCache *remoteMeteoCache(uint32_t node_id) const { return findCache_(_remote_meteo_cache, node_id); }
    size_t remoteMeteoCacheSlots() const { return StackMaster::MAX_SESSIONS; }
    const RemoteMeteoCache &remoteMeteoCacheAt(size_t idx) const { return _remote_meteo_cache[(idx < StackMaster::MAX_SESSIONS) ? idx : 0]; }
    bool requestRemoteMeteoAll() { return false; }

    const RemoteSocketsCache *remoteSocketsCache(uint32_t node_id) const { return findCache_(_remote_sockets_cache, node_id); }
    const RemoteLightsCache *remoteLightsCache(uint32_t node_id) const { return findCache_(_remote_lights_cache, node_id); }
    const RemoteSepticCache *remoteSepticCache(uint32_t node_id) const { return findCache_(_remote_septic_cache, node_id); }
    const RemoteThermoCache *remoteThermoCache(uint32_t node_id) const { return findCache_(_remote_thermo_cache, node_id); }
    const RemoteTanksCache *remoteTanksCache(uint32_t node_id) const { return findCache_(_remote_tanks_cache, node_id); }
    const RemoteSecurityCache *remoteSecurityCache(uint32_t node_id) const { return findCache_(_remote_security_cache, node_id); }

    bool requestRemoteSockets(uint32_t node_id) { return markDisabled_(findCache_(_remote_sockets_cache, node_id)); }
    bool requestRemoteLights(uint32_t node_id) { return markDisabled_(findCache_(_remote_lights_cache, node_id)); }
    bool requestRemoteSeptic(uint32_t node_id) { return markDisabled_(findCache_(_remote_septic_cache, node_id)); }
    bool requestRemoteThermo(uint32_t node_id) { return markDisabled_(findCache_(_remote_thermo_cache, node_id)); }
    bool requestRemoteTanks(uint32_t node_id) { return markDisabled_(findCache_(_remote_tanks_cache, node_id)); }
    bool requestRemoteSecurity(uint32_t node_id) { return markDisabled_(findCache_(_remote_security_cache, node_id)); }

    bool remoteMeteoTemp(uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp) const
    {
        (void)node_id;
        (void)sensor_id;
        temp_c = 0.0f;
        has_temp = false;
        return false;
    }

    bool remoteMeteoRead(uint32_t node_id, uint8_t sensor_id, float &temp_c, bool &has_temp,
                         float &hum, bool &has_hum, bool &ok) const
    {
        (void)node_id;
        (void)sensor_id;
        temp_c = 0.0f;
        hum = 0.0f;
        has_temp = false;
        has_hum = false;
        ok = false;
        return false;
    }

private:
    template <typename CacheT, size_t N>
    static CacheT *findCache_(CacheT (&caches)[N], uint32_t node_id)
    {
        if (node_id == 0)
            return &caches[0];
        for (size_t i = 1; i < N; ++i)
        {
            if (caches[i].node_id == node_id)
                return &caches[i];
        }
        for (size_t i = 1; i < N; ++i)
        {
            if (caches[i].node_id == 0)
            {
                caches[i].reset();
                caches[i].node_id = node_id;
                return &caches[i];
            }
        }
        return nullptr;
    }

    template <typename CacheT, size_t N>
    static const CacheT *findCache_(const CacheT (&caches)[N], uint32_t node_id)
    {
        if (node_id == 0)
            return &caches[0];
        for (size_t i = 1; i < N; ++i)
        {
            if (caches[i].node_id == node_id)
                return &caches[i];
        }
        return nullptr;
    }

    template <typename CacheT>
    static bool markDisabled_(CacheT *cache)
    {
        if (!cache)
            return false;
        cache->pending = false;
        cache->has_data = false;
        cache->last_ok = false;
        cache->updated_ms = millis();
        cache->last_error = "Stack disabled";
        return false;
    }

    void resetAll_()
    {
        for (size_t i = 0; i < StackMaster::MAX_SESSIONS; ++i)
        {
            _remote_meteo_cache[i].reset();
            _remote_sockets_cache[i].reset();
            _remote_lights_cache[i].reset();
            _remote_septic_cache[i].reset();
            _remote_thermo_cache[i].reset();
            _remote_tanks_cache[i].reset();
            _remote_security_cache[i].reset();
        }
    }

    StackNode *_node = nullptr;
    TraceHandler _trace_cb = nullptr;
    void *_trace_ctx = nullptr;
    ConfigsManagerIface *_configs = nullptr;
    RemoteMeteoCache _remote_meteo_cache[StackMaster::MAX_SESSIONS]{};
    RemoteSocketsCache _remote_sockets_cache[StackMaster::MAX_SESSIONS]{};
    RemoteLightsCache _remote_lights_cache[StackMaster::MAX_SESSIONS]{};
    RemoteSepticCache _remote_septic_cache[StackMaster::MAX_SESSIONS]{};
    RemoteThermoCache _remote_thermo_cache[StackMaster::MAX_SESSIONS]{};
    RemoteTanksCache _remote_tanks_cache[StackMaster::MAX_SESSIONS]{};
    RemoteSecurityCache _remote_security_cache[StackMaster::MAX_SESSIONS]{};
};
