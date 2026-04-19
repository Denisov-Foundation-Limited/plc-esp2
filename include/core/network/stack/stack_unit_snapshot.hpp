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
#include <stdint.h>

#include "core/network/stack/stack_device_registry.hpp"
#include "hal/gpio/portio.hpp"
#include "utils/rtos_lock.hpp"

class StackUnitSnapshot
{
public:
    static constexpr size_t kDateLen = 16;
    static constexpr size_t kTimeLen = 16;
    static constexpr size_t kWifiModeLen = 16;
    static constexpr size_t kWifiSsidLen = 33;
    static constexpr size_t kIpLen = 20;
    static constexpr size_t kMacLen = 18;
    static constexpr size_t kGsmImeiLen = 24;
    static constexpr size_t kGsmImsiLen = 24;
    static constexpr size_t kGsmOperatorLen = 32;
    static constexpr size_t kGsmSignalLen = 16;
    static constexpr size_t kGsmRegStatusLen = 24;
    static constexpr size_t kGsmErrorLen = 48;
    static constexpr size_t kGsmUrcLen = 48;
    static constexpr size_t kGsmCallLen = 32;
    static constexpr size_t kGsmUssdLen = 48;
    static constexpr size_t kSocketNameLen = 24;
    static constexpr size_t kSocketCount = 72;
    static constexpr size_t kMeteoNameLen = 24;
    static constexpr size_t kMeteoCount = 50;
    static constexpr size_t kThermoNameLen = 24;
    static constexpr size_t kThermoCount = 20;
    static constexpr size_t kTankNameLen = 24;
    static constexpr size_t kTankCount = 20;
    static constexpr size_t kWateringNameLen = 24;
    static constexpr size_t kWateringCount = 30;
    static constexpr size_t kRuleNameLen = 24;
    static constexpr size_t kRuleCount = 30;
    static constexpr size_t kSepticNameLen = 24;
    static constexpr size_t kLeakNameLen = 24;
    static constexpr size_t kLeakCount = 16;
    static constexpr size_t kSecurityDetectNameLen = 24;
    static constexpr size_t kSecurityDetectPreviewCount = 4;
    static constexpr size_t kPortMaskBytes = (PortIO::PORT_COUNT + 7u) / 8u;
    static constexpr uint8_t kPageSize = 8;

    struct SocketItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool state = false;
        uint8_t button_port = 0xFF;
        uint8_t relay_port = 0xFF;
        uint8_t group_id = 0;
        char name[kSocketNameLen] = {};
    };

    struct MeteoItem
    {
        uint8_t id = 0;
        bool enabled = false;
        uint8_t group_id = 0;
        uint8_t type = 0;
        uint8_t dht_pin = 0xFF;
        uint8_t ds18_addr[8] = {};
        bool ds18_addr_set = false;
        uint32_t source_node_id = 0;
        uint8_t source_sensor_id = 0;
        bool has_read = false;
        bool ok = false;
        bool has_temp = false;
        bool has_humidity = false;
        float temp_c = 0.0f;
        float humidity = 0.0f;
        uint16_t age_s = 0;
        char name[kMeteoNameLen] = {};
    };

    struct ThermoItem
    {
        uint8_t id = 0;
        bool enabled = false;
        uint8_t group_id = 0;
        uint8_t sensor_id = 0;
        uint32_t sensor_node_id = 0;
        uint8_t heat_port = 0xFF;
        uint8_t cool_port = 0xFF;
        uint8_t button_port = 0xFF;
        uint8_t mode = 0;
        int16_t target_c = 0;
        float hysteresis = 0.0f;
        bool power_on = false;
        bool heat_on = false;
        bool cool_on = false;
        char name[kThermoNameLen] = {};
    };

    struct TankItem
    {
        uint8_t id = 0;
        bool enabled = false;
        uint8_t group_id = 0;
        bool power_on = false;
        uint8_t level_low_port = 0xFF;
        uint8_t level_mid_port = 0xFF;
        uint8_t level_full_port = 0xFF;
        uint8_t relay_valve_port = 0xFF;
        uint8_t relay_pump_port = 0xFF;
        uint8_t relay_alarm_port = 0xFF;
        bool level_low = false;
        bool level_mid = false;
        bool level_full = false;
        bool levels_ok = false;
        bool valve_on = false;
        bool pump_on = false;
        bool alarm_on = false;
        char name[kTankNameLen] = {};
    };

    struct LeakItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool power_on = false;
        bool sensor_active_low = true;
        uint8_t sensor_port = 0xFF;
        uint8_t valve_port = 0xFF;
        uint8_t alarm_port = 0xFF;
        bool wet = false;
        bool alarm_latched = false;
        bool valve_closed = false;
        bool alarm_on = false;
        char name[kLeakNameLen] = {};
    };

    struct WateringItem
    {
        uint8_t id = 0;
        bool enabled = false;
        bool status = false;
        bool force = false;
        bool active = false;
        bool paused = false;
        uint8_t port = 0xFF;
        uint8_t tank_id = 0;
        uint8_t weekdays_mask = 0;
        uint8_t hour = 0xFF;
        uint8_t minute = 0xFF;
        uint32_t duration_sec = 0;
        bool slot1_enabled = false;
        uint8_t hour2 = 0xFF;
        uint8_t minute2 = 0xFF;
        uint32_t duration2_sec = 0;
        bool slot2_enabled = false;
        uint8_t hour3 = 0xFF;
        uint8_t minute3 = 0xFF;
        uint32_t duration3_sec = 0;
        bool slot3_enabled = false;
        bool resume_after_refill = false;
        uint8_t resume_level = 0;
        uint32_t remaining_ms = 0;
        char name[kWateringNameLen] = {};
    };

    struct RuleItem
    {
        uint8_t id = 0;
        bool enabled = false;
        char name[kRuleNameLen] = {};
    };

    struct State
    {
        struct SecurityDetectPreview
        {
            uint8_t id = 0;
            char name[kSecurityDetectNameLen] = {};
        };

        uint32_t node_id = 0;
        uint32_t updated_ms = 0;
        bool has_plc = false;
        bool has_rtc = false;
        bool has_wifi = false;
        bool has_gsm = false;
        bool rtc_temp_ok = false;
        float board_temp = 0.0f;
        bool fan_on = false;
        float rtc_temp = 0.0f;
        char rtc_date[kDateLen] = {};
        char rtc_time[kTimeLen] = {};
        char wifi_mode[kWifiModeLen] = {};
        char wifi_ssid[kWifiSsidLen] = {};
        char wifi_ap_ssid[kWifiSsidLen] = {};
        char wifi_ip[kIpLen] = {};
        char wifi_mac[kMacLen] = {};
        bool gsm_enabled = false;
        bool gsm_started = false;
        char gsm_imei[kGsmImeiLen] = {};
        char gsm_imsi[kGsmImsiLen] = {};
        char gsm_operator[kGsmOperatorLen] = {};
        char gsm_signal[kGsmSignalLen] = {};
        char gsm_reg_status[kGsmRegStatusLen] = {};
        char gsm_last_error[kGsmErrorLen] = {};
        char gsm_last_urc[kGsmUrcLen] = {};
        char gsm_last_call[kGsmCallLen] = {};
        char gsm_last_ussd[kGsmUssdLen] = {};
        int32_t gsm_last_http_status = -1;
        int32_t gsm_last_http_len = -1;
        uint16_t sockets_enabled = 0;
        uint16_t sockets_on = 0;
        uint16_t lights_enabled = 0;
        uint16_t lights_on = 0;
        uint16_t meteo_enabled = 0;
        uint16_t meteo_ok = 0;
        uint16_t thermo_enabled = 0;
        uint16_t thermo_active = 0;
        uint16_t tanks_enabled = 0;
        uint16_t tanks_alert = 0;
        uint16_t septic_enabled = 0;
        uint16_t septic_warning = 0;
        uint16_t septic_alert = 0;
        bool septic_monitoring_on = false;
        bool septic_relay_warning_on = false;
        bool septic_relay_alarm_on = false;
        uint8_t septic_group_id = 0;
        uint8_t septic_warning_port = 0xFF;
        uint8_t septic_alarm_port = 0xFF;
        uint8_t septic_relay_warning_port = 0xFF;
        uint8_t septic_relay_alarm_port = 0xFF;
        char septic_name[kSepticNameLen] = {};
        uint16_t watering_enabled = 0;
        uint16_t watering_active = 0;
        uint16_t rules_enabled = 0;
        uint16_t security_sensors_enabled = 0;
        uint16_t security_detected = 0;
        uint8_t security_detect_preview_count = 0;
        SecurityDetectPreview security_detect_preview[kSecurityDetectPreviewCount]{};
        uint16_t leak_enabled = 0;
        uint16_t leak_alert = 0;
        bool security_enabled = false;
        bool security_armed = false;
        bool security_alarm = false;
        bool ring_enabled = false;
        bool ring_on = false;
        bool avr_enabled = false;
        bool avr_main_ok = false;
        bool avr_reserve_ok = false;
        bool avr_fault = false;
        uint8_t avr_active_source = 0;
        bool ports_state_valid = false;
        uint8_t relay_used_bits[kPortMaskBytes] = {};
        uint8_t dinput_used_bits[kPortMaskBytes] = {};
        uint8_t sensor_used_bits[kPortMaskBytes] = {};
    };

    struct CacheState
    {
        uint8_t socket_count = 0;
        uint8_t light_count = 0;
        uint8_t meteo_count = 0;
        uint8_t thermo_count = 0;
        uint8_t tank_count = 0;
        uint8_t watering_count = 0;
        uint8_t rule_count = 0;
        uint8_t leak_count = 0;
    };

    struct RequestState
    {
        uint32_t started_ms = 0;
        bool pending = false;
    };

    struct PageRequestState
    {
        uint32_t started_ms = 0;
        uint16_t offset = 0;
        bool pending = false;
    };

    enum class PageKind : uint8_t
    {
        Sockets = 0,
        Lights,
        Meteo,
        Thermo,
        Tanks,
        Watering,
        Leak,
    };

    StackUnitSnapshot();
    ~StackUnitSnapshot();

    bool prepareRequest(uint32_t node_id, uint32_t now_ms, uint32_t fresh_ms, uint32_t pending_ms);
    bool state(uint32_t node_id, State &out) const;
    bool cacheState(uint32_t node_id, CacheState &out) const;
    bool requestState(uint32_t node_id, RequestState &out) const;
    bool pageRequestState(uint32_t node_id, PageKind kind, PageRequestState &out) const;
    bool socketById(uint32_t node_id, uint8_t id, SocketItem &out) const;
    bool socketAt(uint32_t node_id, uint8_t index, SocketItem &out) const;
    bool socketsPage(uint32_t node_id, uint8_t offset, SocketItem *out, uint8_t capacity, uint8_t &out_count) const;
    bool lightById(uint32_t node_id, uint8_t id, SocketItem &out) const;
    bool lightAt(uint32_t node_id, uint8_t index, SocketItem &out) const;
    bool lightsPage(uint32_t node_id, uint8_t offset, SocketItem *out, uint8_t capacity, uint8_t &out_count) const;
    bool meteoById(uint32_t node_id, uint8_t id, MeteoItem &out) const;
    bool meteoAt(uint32_t node_id, uint8_t index, MeteoItem &out) const;
    bool meteoPage(uint32_t node_id, uint8_t offset, MeteoItem *out, uint8_t capacity, uint8_t &out_count) const;
    bool thermoById(uint32_t node_id, uint8_t id, ThermoItem &out) const;
    bool thermoAt(uint32_t node_id, uint8_t index, ThermoItem &out) const;
    bool thermoPage(uint32_t node_id, uint8_t offset, ThermoItem *out, uint8_t capacity, uint8_t &out_count) const;
    bool tankById(uint32_t node_id, uint8_t id, TankItem &out) const;
    bool tankAt(uint32_t node_id, uint8_t index, TankItem &out) const;
    bool tanksPage(uint32_t node_id, uint8_t offset, TankItem *out, uint8_t capacity, uint8_t &out_count) const;
    bool wateringById(uint32_t node_id, uint8_t id, WateringItem &out) const;
    bool wateringAt(uint32_t node_id, uint8_t index, WateringItem &out) const;
    bool wateringPage(uint32_t node_id, uint8_t offset, WateringItem *out, uint8_t capacity, uint8_t &out_count) const;
    bool ruleAt(uint32_t node_id, uint8_t index, RuleItem &out) const;
    bool rulesPage(uint32_t node_id, uint8_t offset, RuleItem *out, uint8_t capacity, uint8_t &out_count) const;
    bool leakById(uint32_t node_id, uint8_t id, LeakItem &out) const;
    bool leakAt(uint32_t node_id, uint8_t index, LeakItem &out) const;
    bool leaksPage(uint32_t node_id, uint8_t offset, LeakItem *out, uint8_t capacity, uint8_t &out_count) const;
    bool prepareSocketsPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    bool prepareLightsPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    bool prepareMeteoPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    bool prepareThermoPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    bool prepareTanksPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    bool prepareWateringPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    bool prepareLeakPageRequest(uint32_t node_id, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    void completeSocketsPageRequest(uint32_t node_id, uint16_t offset);
    void completeLightsPageRequest(uint32_t node_id, uint16_t offset);
    void completeMeteoPageRequest(uint32_t node_id, uint16_t offset);
    void completeThermoPageRequest(uint32_t node_id, uint16_t offset);
    void completeTanksPageRequest(uint32_t node_id, uint16_t offset);
    void completeWateringPageRequest(uint32_t node_id, uint16_t offset);
    void completeLeakPageRequest(uint32_t node_id, uint16_t offset);
    void clearSocketsPageRequest(uint32_t node_id);
    void clearLightsPageRequest(uint32_t node_id);
    void clearMeteoPageRequest(uint32_t node_id);
    void clearThermoPageRequest(uint32_t node_id);
    void clearTanksPageRequest(uint32_t node_id);
    void clearWateringPageRequest(uint32_t node_id);
    void clearLeakPageRequest(uint32_t node_id);
    void clearPending(uint32_t node_id);
    void applySystemState(uint32_t node_id, const State &state);
    void applyControllerSummary(uint32_t node_id, const State &state);
    void applySocketsPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t on_total,
                          const SocketItem *items, uint8_t item_count, uint32_t updated_ms);
    void applyLightsPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t on_total,
                         const SocketItem *items, uint8_t item_count, uint32_t updated_ms);
    void applyMeteoPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t ok_total,
                        const MeteoItem *items, uint8_t item_count, uint32_t updated_ms);
    void applyThermoPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t active_total,
                         const ThermoItem *items, uint8_t item_count, uint32_t updated_ms);
    void applyTanksPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t alert_total,
                        const TankItem *items, uint8_t item_count, uint32_t updated_ms);
    void applyWateringPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t active_total,
                           const WateringItem *items, uint8_t item_count, uint32_t updated_ms);
    void applyRulesSummary(uint32_t node_id, uint16_t enabled_total, const RuleItem *items, uint8_t item_count,
                           uint32_t updated_ms);
    void applyLeaksPage(uint32_t node_id, uint16_t offset, uint16_t enabled_total, uint16_t alert_total,
                        const LeakItem *items, uint8_t item_count, uint32_t updated_ms);
    void invalidate(uint32_t node_id);

private:
    template <typename ItemT, size_t N>
    struct PageCache
    {
        ItemT items[N]{};
    };

    struct Entry
    {
        bool used = false;
        State state{};
        CacheState cache{};
        RequestState request{};
        PageRequestState sockets_request{};
        PageRequestState lights_request{};
        PageRequestState meteo_request{};
        PageRequestState thermo_request{};
        PageRequestState tanks_request{};
        PageRequestState watering_request{};
        PageRequestState leak_request{};
        PageCache<SocketItem, kSocketCount> sockets{};
        PageCache<SocketItem, kSocketCount> lights{};
        PageCache<MeteoItem, kMeteoCount> meteo{};
        PageCache<ThermoItem, kThermoCount> thermo{};
        PageCache<TankItem, kTankCount> tanks{};
        PageCache<WateringItem, kWateringCount> watering{};
        PageCache<RuleItem, kRuleCount> rules{};
        PageCache<LeakItem, kLeakCount> leaks{};
    };

    static void copyState_(State &dst, const State &src);
    static void copyCacheState_(CacheState &dst, const CacheState &src);
    static void copyRequestState_(RequestState &dst, const RequestState &src);
    static void copyPageRequestState_(PageRequestState &dst, const PageRequestState &src);
    template <typename ItemT, size_t N>
    static bool copyItemById_(const ItemT *items, uint8_t count, uint8_t id, ItemT &out);
    template <typename ItemT, size_t N>
    static bool copyItemAt_(const ItemT *items, uint8_t count, uint8_t index, ItemT &out);
    template <typename ItemT, size_t N>
    static bool copyPage_(const ItemT *items, uint8_t count, uint8_t offset, ItemT *out, uint8_t capacity, uint8_t &out_count);
    static void mergeSystemState_(State &dst, const State &src);
    static void mergeControllerSummary_(State &dst, const State &src);
    static uint8_t clampCount_(uint16_t count, size_t max_count);
    static PageRequestState &pageRequestState_(Entry &entry, PageKind kind);
    static const PageRequestState &pageRequestState_(const Entry &entry, PageKind kind);
    template <typename ItemT, size_t N>
    static void clearPageCache_(PageCache<ItemT, N> &cache);
    template <typename ItemT, size_t N>
    static uint8_t applyPage_(PageCache<ItemT, N> &cache, uint16_t offset, const ItemT *items, uint8_t item_count);
    bool preparePageRequest_(uint32_t node_id, PageKind kind, uint32_t now_ms, uint16_t offset, uint32_t pending_ms);
    void completePageRequest_(uint32_t node_id, PageKind kind, uint16_t offset);
    void clearPageRequest_(uint32_t node_id, PageKind kind);

    Entry *findEntry_(uint32_t node_id);
    const Entry *findEntry_(uint32_t node_id) const;
    Entry *allocEntry_(uint32_t node_id);
    bool ensureStorage_() const;
    void releaseStorage_();

    mutable RtosRecursiveLock _lock;
    mutable Entry *_entries = nullptr;
};
