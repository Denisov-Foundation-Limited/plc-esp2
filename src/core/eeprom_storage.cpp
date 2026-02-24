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

#include "core/eeprom_storage.hpp"

#include "hal/at24lc512.hpp"

EepromStorage::EepromStorage(At24lc512 &eeprom) : _eeprom(&eeprom)
{}
void EepromStorage::bind(At24lc512 &eeprom)
{ _eeprom = &eeprom; }
void EepromStorage::setBase(uint16_t base)
{ _base = base; }
void EepromStorage::setWearLevelSlots(uint16_t slots)
{ _wl_slots = (slots == 0) ? 1 : slots; }
void EepromStorage::setThermoBase(uint16_t base)
{
    _thermo_base = base;
    _thermo_base_set = true;
}
void EepromStorage::setLightsBase(uint16_t base)
{
    _lights_base = base;
    _lights_base_set = true;
}
void EepromStorage::setTankBase(uint16_t base)
{
    _tank_base = base;
    _tank_base_set = true;
}
void EepromStorage::setSecurityBase(uint16_t base)
{
    _security_base = base;
    _security_base_set = true;
}
void EepromStorage::setWateringBase(uint16_t base)
{
    _watering_base = base;
    _watering_base_set = true;
}
void EepromStorage::setWateringRuntimeBase(uint16_t base)
{
    _watering_runtime_base = base;
    _watering_runtime_base_set = true;
}
void EepromStorage::setReady(bool ready)
{ _ready = ready; }
bool EepromStorage::isReady() const
{ return _ready; }
bool EepromStorage::saveSockets(const SocketSnapshot &snap)
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
bool EepromStorage::loadSockets(SocketSnapshot &out)
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
bool EepromStorage::saveLights(const LightSnapshot &snap)
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
bool EepromStorage::loadLights(LightSnapshot &out)
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
bool EepromStorage::saveThermo(const ThermoSnapshot &snap)
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
bool EepromStorage::loadThermo(ThermoSnapshot &out)
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
bool EepromStorage::saveTanks(const TankSnapshot &snap)
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
bool EepromStorage::loadTanks(TankSnapshot &out)
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
bool EepromStorage::saveSecurity(const SecuritySnapshot &snap)
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
bool EepromStorage::loadSecurity(SecuritySnapshot &out)
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
bool EepromStorage::saveWatering(const WateringSnapshot &snap)
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
bool EepromStorage::loadWatering(WateringSnapshot &out)
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
bool EepromStorage::saveWateringRuntime(const WateringRuntimeSnapshot &snap)
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
bool EepromStorage::loadWateringRuntime(WateringRuntimeSnapshot &out)
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
bool EepromStorage::available_() const
{ return _ready && _eeprom; }
uint16_t EepromStorage::slotBase_(uint16_t slot) const
{
    const uint32_t base = (uint32_t)_base + (uint32_t)slot * slotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::slotSize_() const
{
    return (uint16_t)(sizeof(StorageHeader) + 2u * kSocketMaskBytes);
}
uint16_t EepromStorage::lightSlotSize_() const
{
    return (uint16_t)(sizeof(LightHeader) + 2u * kLightMaskBytes);
}
uint16_t EepromStorage::thermoSlotSize_() const
{
    return (uint16_t)(sizeof(ThermoHeader) + kThermoMaskBytes + kThermoTargetBytes);
}
uint16_t EepromStorage::tankSlotSize_() const
{
    return (uint16_t)(sizeof(TankHeader) + kTankMaskBytes);
}
uint16_t EepromStorage::securitySlotSize_() const
{
    return (uint16_t)(sizeof(SecurityHeader) + sizeof(SecuritySnapshot));
}
uint16_t EepromStorage::wateringSlotSize_() const
{
    return (uint16_t)(sizeof(WateringHeader) + kWateringMaskBytes);
}
uint16_t EepromStorage::wateringRuntimeSlotSize_() const
{
    return (uint16_t)(sizeof(WateringRuntimeHeader) + 2u * kWateringMaskBytes +
                      sizeof(WateringRuntimeSnapshot::remaining_ms) +
                      sizeof(WateringRuntimeSnapshot::last_start_key));
}
uint16_t EepromStorage::lightsBase_() const
{
    if (_lights_base_set)
        return _lights_base;
    const uint32_t base = (uint32_t)_base + (uint32_t)_wl_slots * slotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::thermoBase_() const
{
    if (_thermo_base_set)
        return _thermo_base;
    const uint32_t base = (uint32_t)lightsBase_() + (uint32_t)_wl_slots * lightSlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::tankBase_() const
{
    if (_tank_base_set)
        return _tank_base;
    const uint32_t base = (uint32_t)thermoBase_() + (uint32_t)_wl_slots * thermoSlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::securityBase_() const
{
    if (_security_base_set)
        return _security_base;
    const uint32_t base = (uint32_t)tankBase_() + (uint32_t)_wl_slots * tankSlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::wateringBase_() const
{
    if (_watering_base_set)
        return _watering_base;
    const uint32_t base = (uint32_t)securityBase_() + (uint32_t)_wl_slots * securitySlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::wateringRuntimeBase_() const
{
    if (_watering_runtime_base_set)
        return _watering_runtime_base;
    const uint32_t base = (uint32_t)wateringBase_() + (uint32_t)_wl_slots * wateringSlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::thermoSlotBase_(uint16_t slot) const
{
    const uint32_t base = (uint32_t)thermoBase_() + (uint32_t)slot * thermoSlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::lightSlotBase_(uint16_t slot) const
{
    const uint32_t base = (uint32_t)lightsBase_() + (uint32_t)slot * lightSlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::tankSlotBase_(uint16_t slot) const
{
    const uint32_t base = (uint32_t)tankBase_() + (uint32_t)slot * tankSlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::securitySlotBase_(uint16_t slot) const
{
    const uint32_t base = (uint32_t)securityBase_() + (uint32_t)slot * securitySlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::wateringSlotBase_(uint16_t slot) const
{
    const uint32_t base = (uint32_t)wateringBase_() + (uint32_t)slot * wateringSlotSize_();
    return (uint16_t)base;
}
uint16_t EepromStorage::wateringRuntimeSlotBase_(uint16_t slot) const
{
    const uint32_t base = (uint32_t)wateringRuntimeBase_() + (uint32_t)slot * wateringRuntimeSlotSize_();
    return (uint16_t)base;
}
bool EepromStorage::isSeqNewer_(uint32_t a, uint32_t b)
{
    return (uint32_t)(a - b) < 0x80000000u;
}
bool EepromStorage::loadLegacy_(SocketSnapshot &out)
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
