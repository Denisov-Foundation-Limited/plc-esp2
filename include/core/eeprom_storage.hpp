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

#include <stdint.h>

class At24lc512;

class EepromStorage
{
public:
    static constexpr uint16_t kDefaultBase = 0;
    static constexpr uint16_t kSocketCount = 72;
    static constexpr uint16_t kSocketMaskBytes = (kSocketCount + 7) / 8;
    static constexpr uint16_t kLightCount = 72;
    static constexpr uint16_t kLightMaskBytes = (kLightCount + 7) / 8;
    static constexpr uint16_t kThermoCount = 20;
    static constexpr uint16_t kThermoMaskBytes = (kThermoCount + 7) / 8;
    static constexpr uint16_t kThermoTargetBytes = (uint16_t)(kThermoCount * sizeof(int16_t));
    static constexpr uint16_t kTankCount = 20;
    static constexpr uint16_t kTankMaskBytes = (kTankCount + 7) / 8;
    static constexpr uint16_t kWateringCount = 30;
    static constexpr uint16_t kWateringMaskBytes = (kWateringCount + 7) / 8;

    struct SocketSnapshot
    {
        uint8_t enabled_mask[kSocketMaskBytes] = {};
        uint8_t state_mask[kSocketMaskBytes] = {};
    };

    struct LightSnapshot
    {
        uint8_t enabled_mask[kLightMaskBytes] = {};
        uint8_t state_mask[kLightMaskBytes] = {};
    };

    struct ThermoSnapshot
    {
        uint8_t power_mask[kThermoMaskBytes] = {};
        int16_t target_t10[kThermoCount] = {};
    };

    struct TankSnapshot
    {
        uint8_t power_mask[kTankMaskBytes] = {};
    };

    struct SecuritySnapshot
    {
        uint8_t flags = 0;
    };

    struct WateringSnapshot
    {
        uint8_t status_mask[kWateringMaskBytes] = {};
    };

    struct WateringRuntimeSnapshot
    {
        uint8_t active_mask[kWateringMaskBytes] = {};
        uint8_t paused_mask[kWateringMaskBytes] = {};
        uint32_t remaining_ms[kWateringCount] = {};
        uint32_t last_start_key[kWateringCount] = {};
    };

    static constexpr uint8_t kSecurityArmedMask = 0x01;
    static constexpr uint8_t kSecurityAlarmMask = 0x02;

    EepromStorage() = default;
    explicit EepromStorage(At24lc512 &eeprom);

    void bind(At24lc512 &eeprom);
    void setBase(uint16_t base);
    void setWearLevelSlots(uint16_t slots);
    void setThermoBase(uint16_t base);
    void setLightsBase(uint16_t base);
    void setTankBase(uint16_t base);
    void setSecurityBase(uint16_t base);
    void setWateringBase(uint16_t base);
    void setWateringRuntimeBase(uint16_t base);
    void setReady(bool ready);
    bool isReady() const;

    bool saveSockets(const SocketSnapshot &snap);

    bool loadSockets(SocketSnapshot &out);

    bool saveLights(const LightSnapshot &snap);

    bool loadLights(LightSnapshot &out);

    bool saveThermo(const ThermoSnapshot &snap);

    bool loadThermo(ThermoSnapshot &out);

    bool saveTanks(const TankSnapshot &snap);

    bool loadTanks(TankSnapshot &out);

    bool saveSecurity(const SecuritySnapshot &snap);

    bool loadSecurity(SecuritySnapshot &out);

    bool saveWatering(const WateringSnapshot &snap);

    bool loadWatering(WateringSnapshot &out);

    bool saveWateringRuntime(const WateringRuntimeSnapshot &snap);

    bool loadWateringRuntime(WateringRuntimeSnapshot &out);

private:
    bool available_() const;

    struct StorageHeader
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t socket_count = 0;
        uint32_t seq = 0;
    };

    struct LightHeader
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t light_count = 0;
        uint32_t seq = 0;
    };

    struct ThermoHeader
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t thermo_count = 0;
        uint32_t seq = 0;
    };

    struct TankHeader
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t tank_count = 0;
        uint32_t seq = 0;
    };

    struct SecurityHeader
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t reserved = 0;
        uint32_t seq = 0;
    };

    struct WateringHeader
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t watering_count = 0;
        uint32_t seq = 0;
    };

    struct WateringRuntimeHeader
    {
        uint32_t magic = 0;
        uint16_t version = 0;
        uint16_t watering_count = 0;
        uint32_t seq = 0;
    };

    static constexpr uint32_t kMagic = 0x45535031u; // "ESP1"
    static constexpr uint16_t kVersion = 2;
    static constexpr uint16_t kLegacyVersion = 1;
    static constexpr uint32_t kLightMagic = 0x45535035u; // "ESP5"
    static constexpr uint16_t kLightVersion = 1;
    static constexpr uint32_t kThermoMagic = 0x45535032u; // "ESP2"
    static constexpr uint16_t kThermoVersion = 2;
    static constexpr uint32_t kTankMagic = 0x45535033u; // "ESP3"
    static constexpr uint16_t kTankVersion = 1;
    static constexpr uint32_t kSecurityMagic = 0x45535034u; // "ESP4"
    static constexpr uint16_t kSecurityVersion = 1;
    static constexpr uint32_t kWateringMagic = 0x45535036u; // "ESP6"
    static constexpr uint16_t kWateringVersion = 1;
    static constexpr uint32_t kWateringRuntimeMagic = 0x45535037u; // "ESP7"
    static constexpr uint16_t kWateringRuntimeVersion = 1;

    At24lc512 *_eeprom = nullptr;
    uint16_t _base = kDefaultBase;
    bool _ready = false;
    uint16_t _wl_slots = 1;
    uint16_t _last_slot = 0;
    uint32_t _last_seq = 0;
    bool _has_seq = false;
    bool _lights_base_set = false;
    uint16_t _lights_base = 0;
    uint16_t _lights_last_slot = 0;
    uint32_t _lights_last_seq = 0;
    bool _lights_has_seq = false;
    bool _thermo_base_set = false;
    uint16_t _thermo_base = 0;
    uint16_t _thermo_last_slot = 0;
    uint32_t _thermo_last_seq = 0;
    bool _thermo_has_seq = false;
    bool _tank_base_set = false;
    uint16_t _tank_base = 0;
    uint16_t _tank_last_slot = 0;
    uint32_t _tank_last_seq = 0;
    bool _tank_has_seq = false;
    bool _security_base_set = false;
    uint16_t _security_base = 0;
    uint16_t _security_last_slot = 0;
    uint32_t _security_last_seq = 0;
    bool _security_has_seq = false;
    bool _watering_base_set = false;
    uint16_t _watering_base = 0;
    uint16_t _watering_last_slot = 0;
    uint32_t _watering_last_seq = 0;
    bool _watering_has_seq = false;
    bool _watering_runtime_base_set = false;
    uint16_t _watering_runtime_base = 0;
    uint16_t _watering_runtime_last_slot = 0;
    uint32_t _watering_runtime_last_seq = 0;
    bool _watering_runtime_has_seq = false;

    uint16_t slotBase_(uint16_t slot) const;

    uint16_t slotSize_() const;

    uint16_t lightSlotSize_() const;

    uint16_t thermoSlotSize_() const;

    uint16_t tankSlotSize_() const;

    uint16_t securitySlotSize_() const;

    uint16_t wateringSlotSize_() const;

    uint16_t wateringRuntimeSlotSize_() const;

    uint16_t lightsBase_() const;

    uint16_t thermoBase_() const;

    uint16_t tankBase_() const;

    uint16_t securityBase_() const;

    uint16_t wateringBase_() const;

    uint16_t wateringRuntimeBase_() const;

    uint16_t thermoSlotBase_(uint16_t slot) const;

    uint16_t lightSlotBase_(uint16_t slot) const;

    uint16_t tankSlotBase_(uint16_t slot) const;

    uint16_t securitySlotBase_(uint16_t slot) const;

    uint16_t wateringSlotBase_(uint16_t slot) const;

    uint16_t wateringRuntimeSlotBase_(uint16_t slot) const;

    static bool isSeqNewer_(uint32_t a, uint32_t b);

    bool loadLegacy_(SocketSnapshot &out);
};
