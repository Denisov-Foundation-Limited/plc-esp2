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
#include <string.h>

#include "hal/at24lc512.hpp"

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
    explicit EepromStorage(At24lc512 &eeprom) : _eeprom(&eeprom) {}

    void bind(At24lc512 &eeprom) { _eeprom = &eeprom; }
    void setBase(uint16_t base) { _base = base; }
    void setWearLevelSlots(uint16_t slots) { _wl_slots = (slots == 0) ? 1 : slots; }
    void setThermoBase(uint16_t base)
    {
        _thermo_base = base;
        _thermo_base_set = true;
    }
    void setLightsBase(uint16_t base)
    {
        _lights_base = base;
        _lights_base_set = true;
    }
    void setTankBase(uint16_t base)
    {
        _tank_base = base;
        _tank_base_set = true;
    }
    void setSecurityBase(uint16_t base)
    {
        _security_base = base;
        _security_base_set = true;
    }
    void setWateringBase(uint16_t base)
    {
        _watering_base = base;
        _watering_base_set = true;
    }
    void setWateringRuntimeBase(uint16_t base)
    {
        _watering_runtime_base = base;
        _watering_runtime_base_set = true;
    }
    void setReady(bool ready) { _ready = ready; }
    bool isReady() const { return _ready; }

    bool saveSockets(const SocketSnapshot &snap)
    {
        if (!available_())
            return false;
        uint16_t slot = 0;
        uint32_t seq = 1;
        if (_wl_slots > 1 && _has_seq)
        {
            slot = (uint16_t)((_last_slot + 1) % _wl_slots);
            seq = _last_seq + 1;
        }
        const uint16_t base = slotBase_(slot);
        StorageHeader hdr{};
        hdr.magic = kMagic;
        hdr.version = kVersion;
        hdr.socket_count = kSocketCount;
        hdr.seq = seq;
        if (!_eeprom->write(base, reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->write(off, snap.enabled_mask, kSocketMaskBytes))
            return false;
        if (!_eeprom->write(off + kSocketMaskBytes, snap.state_mask, kSocketMaskBytes))
            return false;
        _last_slot = slot;
        _last_seq = seq;
        _has_seq = true;
        return true;
    }

    bool loadSockets(SocketSnapshot &out)
    {
        if (!available_())
            return false;
        StorageHeader best_hdr{};
        uint16_t best_slot = 0;
        bool found = false;

        const uint16_t slots = (_wl_slots == 0) ? 1 : _wl_slots;
        for (uint16_t i = 0; i < slots; ++i)
        {
            StorageHeader hdr{};
            const uint16_t base = slotBase_(i);
            if (!_eeprom->read(base, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)))
                continue;
            if (hdr.magic != kMagic || hdr.version != kVersion || hdr.socket_count != kSocketCount)
                continue;
            if (!found || isSeqNewer_(hdr.seq, best_hdr.seq))
            {
                best_hdr = hdr;
                best_slot = i;
                found = true;
            }
        }

        if (found)
        {
            const uint16_t off = slotBase_(best_slot) + sizeof(best_hdr);
            if (!_eeprom->read(off, out.enabled_mask, kSocketMaskBytes))
                return false;
            if (!_eeprom->read(off + kSocketMaskBytes, out.state_mask, kSocketMaskBytes))
                return false;
            _last_slot = best_slot;
            _last_seq = best_hdr.seq;
            _has_seq = true;
            return true;
        }

        return loadLegacy_(out);
    }

    bool saveLights(const LightSnapshot &snap)
    {
        if (!available_())
            return false;
        uint16_t slot = 0;
        uint32_t seq = 1;
        if (_wl_slots > 1 && _lights_has_seq)
        {
            slot = (uint16_t)((_lights_last_slot + 1) % _wl_slots);
            seq = _lights_last_seq + 1;
        }
        const uint16_t base = lightSlotBase_(slot);
        LightHeader hdr{};
        hdr.magic = kLightMagic;
        hdr.version = kLightVersion;
        hdr.light_count = kLightCount;
        hdr.seq = seq;
        if (!_eeprom->write(base, reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->write(off, snap.enabled_mask, kLightMaskBytes))
            return false;
        if (!_eeprom->write(off + kLightMaskBytes, snap.state_mask, kLightMaskBytes))
            return false;
        _lights_last_slot = slot;
        _lights_last_seq = seq;
        _lights_has_seq = true;
        return true;
    }

    bool loadLights(LightSnapshot &out)
    {
        if (!available_())
            return false;
        LightHeader best_hdr{};
        uint16_t best_slot = 0;
        bool found = false;

        const uint16_t slots = (_wl_slots == 0) ? 1 : _wl_slots;
        for (uint16_t i = 0; i < slots; ++i)
        {
            LightHeader hdr{};
            const uint16_t base = lightSlotBase_(i);
            if (!_eeprom->read(base, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)))
                continue;
            if (hdr.magic != kLightMagic || hdr.version != kLightVersion || hdr.light_count != kLightCount)
                continue;
            if (!found || isSeqNewer_(hdr.seq, best_hdr.seq))
            {
                best_hdr = hdr;
                best_slot = i;
                found = true;
            }
        }

        if (!found)
            return false;

        const uint16_t off = lightSlotBase_(best_slot) + sizeof(best_hdr);
        if (!_eeprom->read(off, out.enabled_mask, kLightMaskBytes))
            return false;
        if (!_eeprom->read(off + kLightMaskBytes, out.state_mask, kLightMaskBytes))
            return false;
        _lights_last_slot = best_slot;
        _lights_last_seq = best_hdr.seq;
        _lights_has_seq = true;
        return true;
    }

    bool saveThermo(const ThermoSnapshot &snap)
    {
        if (!available_())
            return false;
        uint16_t slot = 0;
        uint32_t seq = 1;
        if (_wl_slots > 1 && _thermo_has_seq)
        {
            slot = (uint16_t)((_thermo_last_slot + 1) % _wl_slots);
            seq = _thermo_last_seq + 1;
        }
        const uint16_t base = thermoSlotBase_(slot);
        ThermoHeader hdr{};
        hdr.magic = kThermoMagic;
        hdr.version = kThermoVersion;
        hdr.thermo_count = kThermoCount;
        hdr.seq = seq;
        if (!_eeprom->write(base, reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->write(off, snap.power_mask, kThermoMaskBytes))
            return false;
        if (!_eeprom->write(off + kThermoMaskBytes, reinterpret_cast<const uint8_t *>(snap.target_t10),
                            kThermoTargetBytes))
            return false;
        _thermo_last_slot = slot;
        _thermo_last_seq = seq;
        _thermo_has_seq = true;
        return true;
    }

    bool loadThermo(ThermoSnapshot &out)
    {
        if (!available_())
            return false;
        ThermoHeader best_hdr{};
        uint16_t best_slot = 0;
        bool found = false;

        const uint16_t slots = (_wl_slots == 0) ? 1 : _wl_slots;
        for (uint16_t i = 0; i < slots; ++i)
        {
            ThermoHeader hdr{};
            const uint16_t base = thermoSlotBase_(i);
            if (!_eeprom->read(base, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)))
                continue;
            if (hdr.magic != kThermoMagic || hdr.version != kThermoVersion || hdr.thermo_count != kThermoCount)
                continue;
            if (!found || isSeqNewer_(hdr.seq, best_hdr.seq))
            {
                best_hdr = hdr;
                best_slot = i;
                found = true;
            }
        }

        if (!found)
            return false;

        const uint16_t off = thermoSlotBase_(best_slot) + sizeof(best_hdr);
        if (!_eeprom->read(off, out.power_mask, kThermoMaskBytes))
            return false;
        if (!_eeprom->read(off + kThermoMaskBytes, reinterpret_cast<uint8_t *>(out.target_t10),
                           kThermoTargetBytes))
            return false;
        _thermo_last_slot = best_slot;
        _thermo_last_seq = best_hdr.seq;
        _thermo_has_seq = true;
        return true;
    }

    bool saveTanks(const TankSnapshot &snap)
    {
        if (!available_())
            return false;
        uint16_t slot = 0;
        uint32_t seq = 1;
        if (_wl_slots > 1 && _tank_has_seq)
        {
            slot = (uint16_t)((_tank_last_slot + 1) % _wl_slots);
            seq = _tank_last_seq + 1;
        }
        const uint16_t base = tankSlotBase_(slot);
        TankHeader hdr{};
        hdr.magic = kTankMagic;
        hdr.version = kTankVersion;
        hdr.tank_count = kTankCount;
        hdr.seq = seq;
        if (!_eeprom->write(base, reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->write(off, snap.power_mask, kTankMaskBytes))
            return false;
        _tank_last_slot = slot;
        _tank_last_seq = seq;
        _tank_has_seq = true;
        return true;
    }

    bool loadTanks(TankSnapshot &out)
    {
        if (!available_())
            return false;
        TankHeader best_hdr{};
        uint16_t best_slot = 0;
        bool found = false;

        const uint16_t slots = (_wl_slots == 0) ? 1 : _wl_slots;
        for (uint16_t i = 0; i < slots; ++i)
        {
            TankHeader hdr{};
            const uint16_t base = tankSlotBase_(i);
            if (!_eeprom->read(base, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)))
                continue;
            if (hdr.magic != kTankMagic || hdr.version != kTankVersion || hdr.tank_count != kTankCount)
                continue;
            if (!found || isSeqNewer_(hdr.seq, best_hdr.seq))
            {
                best_hdr = hdr;
                best_slot = i;
                found = true;
            }
        }

        if (!found)
            return false;

        const uint16_t off = tankSlotBase_(best_slot) + sizeof(best_hdr);
        if (!_eeprom->read(off, out.power_mask, kTankMaskBytes))
            return false;
        _tank_last_slot = best_slot;
        _tank_last_seq = best_hdr.seq;
        _tank_has_seq = true;
        return true;
    }

    bool saveSecurity(const SecuritySnapshot &snap)
    {
        if (!available_())
            return false;
        uint16_t slot = 0;
        uint32_t seq = 1;
        if (_wl_slots > 1 && _security_has_seq)
        {
            slot = (uint16_t)((_security_last_slot + 1) % _wl_slots);
            seq = _security_last_seq + 1;
        }
        const uint16_t base = securitySlotBase_(slot);
        SecurityHeader hdr{};
        hdr.magic = kSecurityMagic;
        hdr.version = kSecurityVersion;
        hdr.seq = seq;
        if (!_eeprom->write(base, reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->write(off, reinterpret_cast<const uint8_t *>(&snap), sizeof(snap)))
            return false;
        _security_last_slot = slot;
        _security_last_seq = seq;
        _security_has_seq = true;
        return true;
    }

    bool loadSecurity(SecuritySnapshot &out)
    {
        if (!available_())
            return false;
        SecurityHeader best_hdr{};
        uint16_t best_slot = 0;
        bool found = false;

        const uint16_t slots = (_wl_slots == 0) ? 1 : _wl_slots;
        for (uint16_t i = 0; i < slots; ++i)
        {
            SecurityHeader hdr{};
            const uint16_t base = securitySlotBase_(i);
            if (!_eeprom->read(base, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)))
                continue;
            if (hdr.magic != kSecurityMagic || hdr.version != kSecurityVersion)
                continue;
            if (!found || isSeqNewer_(hdr.seq, best_hdr.seq))
            {
                best_hdr = hdr;
                best_slot = i;
                found = true;
            }
        }

        if (!found)
            return false;

        const uint16_t off = securitySlotBase_(best_slot) + sizeof(best_hdr);
        if (!_eeprom->read(off, reinterpret_cast<uint8_t *>(&out), sizeof(out)))
            return false;
        _security_last_slot = best_slot;
        _security_last_seq = best_hdr.seq;
        _security_has_seq = true;
        return true;
    }

    bool saveWatering(const WateringSnapshot &snap)
    {
        if (!available_())
            return false;
        uint16_t slot = 0;
        uint32_t seq = 1;
        if (_wl_slots > 1 && _watering_has_seq)
        {
            slot = (uint16_t)((_watering_last_slot + 1) % _wl_slots);
            seq = _watering_last_seq + 1;
        }
        const uint16_t base = wateringSlotBase_(slot);
        WateringHeader hdr{};
        hdr.magic = kWateringMagic;
        hdr.version = kWateringVersion;
        hdr.watering_count = kWateringCount;
        hdr.seq = seq;
        if (!_eeprom->write(base, reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->write(off, snap.status_mask, kWateringMaskBytes))
            return false;
        _watering_last_slot = slot;
        _watering_last_seq = seq;
        _watering_has_seq = true;
        return true;
    }

    bool loadWatering(WateringSnapshot &out)
    {
        if (!available_())
            return false;
        WateringHeader best_hdr{};
        uint16_t best_slot = 0;
        bool found = false;
        const uint16_t slots = (_wl_slots == 0) ? 1 : _wl_slots;
        for (uint16_t i = 0; i < slots; ++i)
        {
            WateringHeader hdr{};
            const uint16_t base = wateringSlotBase_(i);
            if (!_eeprom->read(base, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)))
                continue;
            if (hdr.magic != kWateringMagic || hdr.version != kWateringVersion ||
                hdr.watering_count != kWateringCount)
                continue;
            if (!found || isSeqNewer_(hdr.seq, best_hdr.seq))
            {
                best_hdr = hdr;
                best_slot = i;
                found = true;
            }
        }
        if (!found)
            return false;
        const uint16_t off = wateringSlotBase_(best_slot) + sizeof(best_hdr);
        if (!_eeprom->read(off, out.status_mask, kWateringMaskBytes))
            return false;
        _watering_last_slot = best_slot;
        _watering_last_seq = best_hdr.seq;
        _watering_has_seq = true;
        return true;
    }

    bool saveWateringRuntime(const WateringRuntimeSnapshot &snap)
    {
        if (!available_())
            return false;
        uint16_t slot = 0;
        uint32_t seq = 1;
        if (_wl_slots > 1 && _watering_runtime_has_seq)
        {
            slot = (uint16_t)((_watering_runtime_last_slot + 1) % _wl_slots);
            seq = _watering_runtime_last_seq + 1;
        }
        const uint16_t base = wateringRuntimeSlotBase_(slot);
        WateringRuntimeHeader hdr{};
        hdr.magic = kWateringRuntimeMagic;
        hdr.version = kWateringRuntimeVersion;
        hdr.watering_count = kWateringCount;
        hdr.seq = seq;
        if (!_eeprom->write(base, reinterpret_cast<const uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->write(off, snap.active_mask, kWateringMaskBytes))
            return false;
        if (!_eeprom->write(off + kWateringMaskBytes, snap.paused_mask, kWateringMaskBytes))
            return false;
        const uint16_t off2 = off + 2u * kWateringMaskBytes;
        if (!_eeprom->write(off2, reinterpret_cast<const uint8_t *>(snap.remaining_ms),
                            sizeof(snap.remaining_ms)))
            return false;
        if (!_eeprom->write(off2 + sizeof(snap.remaining_ms), reinterpret_cast<const uint8_t *>(snap.last_start_key),
                            sizeof(snap.last_start_key)))
            return false;
        _watering_runtime_last_slot = slot;
        _watering_runtime_last_seq = seq;
        _watering_runtime_has_seq = true;
        return true;
    }

    bool loadWateringRuntime(WateringRuntimeSnapshot &out)
    {
        if (!available_())
            return false;
        WateringRuntimeHeader best_hdr{};
        uint16_t best_slot = 0;
        bool found = false;
        const uint16_t slots = (_wl_slots == 0) ? 1 : _wl_slots;
        for (uint16_t i = 0; i < slots; ++i)
        {
            WateringRuntimeHeader hdr{};
            const uint16_t base = wateringRuntimeSlotBase_(i);
            if (!_eeprom->read(base, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)))
                continue;
            if (hdr.magic != kWateringRuntimeMagic || hdr.version != kWateringRuntimeVersion ||
                hdr.watering_count != kWateringCount)
                continue;
            if (!found || isSeqNewer_(hdr.seq, best_hdr.seq))
            {
                best_hdr = hdr;
                best_slot = i;
                found = true;
            }
        }
        if (!found)
            return false;
        const uint16_t off = wateringRuntimeSlotBase_(best_slot) + sizeof(best_hdr);
        if (!_eeprom->read(off, out.active_mask, kWateringMaskBytes))
            return false;
        if (!_eeprom->read(off + kWateringMaskBytes, out.paused_mask, kWateringMaskBytes))
            return false;
        const uint16_t off2 = off + 2u * kWateringMaskBytes;
        if (!_eeprom->read(off2, reinterpret_cast<uint8_t *>(out.remaining_ms), sizeof(out.remaining_ms)))
            return false;
        if (!_eeprom->read(off2 + sizeof(out.remaining_ms), reinterpret_cast<uint8_t *>(out.last_start_key),
                           sizeof(out.last_start_key)))
            return false;
        _watering_runtime_last_slot = best_slot;
        _watering_runtime_last_seq = best_hdr.seq;
        _watering_runtime_has_seq = true;
        return true;
    }

private:
    bool available_() const { return _ready && _eeprom; }

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

    uint16_t slotBase_(uint16_t slot) const
    {
        const uint32_t base = (uint32_t)_base + (uint32_t)slot * slotSize_();
        return (uint16_t)base;
    }

    uint16_t slotSize_() const
    {
        return (uint16_t)(sizeof(StorageHeader) + 2u * kSocketMaskBytes);
    }

    uint16_t lightSlotSize_() const
    {
        return (uint16_t)(sizeof(LightHeader) + 2u * kLightMaskBytes);
    }

    uint16_t thermoSlotSize_() const
    {
        return (uint16_t)(sizeof(ThermoHeader) + kThermoMaskBytes + kThermoTargetBytes);
    }

    uint16_t tankSlotSize_() const
    {
        return (uint16_t)(sizeof(TankHeader) + kTankMaskBytes);
    }

    uint16_t securitySlotSize_() const
    {
        return (uint16_t)(sizeof(SecurityHeader) + sizeof(SecuritySnapshot));
    }

    uint16_t wateringSlotSize_() const
    {
        return (uint16_t)(sizeof(WateringHeader) + kWateringMaskBytes);
    }

    uint16_t wateringRuntimeSlotSize_() const
    {
        return (uint16_t)(sizeof(WateringRuntimeHeader) + 2u * kWateringMaskBytes +
                          sizeof(WateringRuntimeSnapshot::remaining_ms) +
                          sizeof(WateringRuntimeSnapshot::last_start_key));
    }

    uint16_t lightsBase_() const
    {
        if (_lights_base_set)
            return _lights_base;
        const uint32_t base = (uint32_t)_base + (uint32_t)_wl_slots * slotSize_();
        return (uint16_t)base;
    }

    uint16_t thermoBase_() const
    {
        if (_thermo_base_set)
            return _thermo_base;
        const uint32_t base = (uint32_t)lightsBase_() + (uint32_t)_wl_slots * lightSlotSize_();
        return (uint16_t)base;
    }

    uint16_t tankBase_() const
    {
        if (_tank_base_set)
            return _tank_base;
        const uint32_t base = (uint32_t)thermoBase_() + (uint32_t)_wl_slots * thermoSlotSize_();
        return (uint16_t)base;
    }

    uint16_t securityBase_() const
    {
        if (_security_base_set)
            return _security_base;
        const uint32_t base = (uint32_t)tankBase_() + (uint32_t)_wl_slots * tankSlotSize_();
        return (uint16_t)base;
    }

    uint16_t wateringBase_() const
    {
        if (_watering_base_set)
            return _watering_base;
        const uint32_t base = (uint32_t)securityBase_() + (uint32_t)_wl_slots * securitySlotSize_();
        return (uint16_t)base;
    }

    uint16_t wateringRuntimeBase_() const
    {
        if (_watering_runtime_base_set)
            return _watering_runtime_base;
        const uint32_t base = (uint32_t)wateringBase_() + (uint32_t)_wl_slots * wateringSlotSize_();
        return (uint16_t)base;
    }

    uint16_t thermoSlotBase_(uint16_t slot) const
    {
        const uint32_t base = (uint32_t)thermoBase_() + (uint32_t)slot * thermoSlotSize_();
        return (uint16_t)base;
    }

    uint16_t lightSlotBase_(uint16_t slot) const
    {
        const uint32_t base = (uint32_t)lightsBase_() + (uint32_t)slot * lightSlotSize_();
        return (uint16_t)base;
    }

    uint16_t tankSlotBase_(uint16_t slot) const
    {
        const uint32_t base = (uint32_t)tankBase_() + (uint32_t)slot * tankSlotSize_();
        return (uint16_t)base;
    }

    uint16_t securitySlotBase_(uint16_t slot) const
    {
        const uint32_t base = (uint32_t)securityBase_() + (uint32_t)slot * securitySlotSize_();
        return (uint16_t)base;
    }

    uint16_t wateringSlotBase_(uint16_t slot) const
    {
        const uint32_t base = (uint32_t)wateringBase_() + (uint32_t)slot * wateringSlotSize_();
        return (uint16_t)base;
    }

    uint16_t wateringRuntimeSlotBase_(uint16_t slot) const
    {
        const uint32_t base = (uint32_t)wateringRuntimeBase_() + (uint32_t)slot * wateringRuntimeSlotSize_();
        return (uint16_t)base;
    }

    static bool isSeqNewer_(uint32_t a, uint32_t b)
    {
        return (uint32_t)(a - b) < 0x80000000u;
    }

    bool loadLegacy_(SocketSnapshot &out)
    {
        if (!available_())
            return false;
        StorageHeader hdr{};
        const uint16_t base = _base;
        if (!_eeprom->read(base, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr)))
            return false;
        if (hdr.magic != kMagic || hdr.version != kLegacyVersion || hdr.socket_count != kSocketCount)
            return false;
        const uint16_t off = base + sizeof(hdr);
        if (!_eeprom->read(off, out.enabled_mask, kSocketMaskBytes))
            return false;
        return _eeprom->read(off + kSocketMaskBytes, out.state_mask, kSocketMaskBytes);
    }
};
