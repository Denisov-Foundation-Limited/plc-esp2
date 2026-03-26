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

#include "core/runtime/app_runtime.hpp"

#include "app.hpp"

namespace
{
uint8_t displayDaysInMonth_(uint16_t year, uint8_t month)
{
    switch (month)
    {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
        return 31;
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    case 2:
        return (year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u)) ? 29 : 28;
    default:
        return 31;
    }
}

void advanceDisplayDateTime_(Ds3231Mz::DateTime &dt, uint32_t delta_sec)
{
    uint32_t sec_of_day = (uint32_t)dt.hour * 3600u + (uint32_t)dt.minute * 60u + (uint32_t)dt.second;
    uint32_t total_sec = sec_of_day + delta_sec;
    uint32_t day_carry = total_sec / 86400u;
    total_sec %= 86400u;

    dt.hour = (uint8_t)(total_sec / 3600u);
    total_sec %= 3600u;
    dt.minute = (uint8_t)(total_sec / 60u);
    dt.second = (uint8_t)(total_sec % 60u);

    while (day_carry > 0)
    {
        const uint8_t dim = displayDaysInMonth_(dt.year, dt.month);
        if (dt.day < dim)
            ++dt.day;
        else
        {
            dt.day = 1;
            if (dt.month < 12)
                ++dt.month;
            else
            {
                dt.month = 1;
                ++dt.year;
            }
        }
        if (dt.day_of_week >= 1 && dt.day_of_week <= 7)
            dt.day_of_week = (uint8_t)((dt.day_of_week % 7u) + 1u);
        --day_carry;
    }
}

} // namespace

bool AppRuntime::onDisplaySlot_(void *ctx, const DisplaySlotConfig &slot, char out[5]){
    if (!ctx)
        return false;
    return static_cast<AppRuntime *>(ctx)->renderDisplaySlot_(slot, out);
}

void AppRuntime::updateDisplayLayout_(){
    const size_t count = cfg.configs_manager.displaySlotCount();
    for (size_t i = 0; i < Display::kSlotCount && i < count; ++i)
    {
        DisplaySlotConfig slot{};
        if (!cfg.configs_manager.displaySlot(i, slot))
            continue;
        if (!displaySlotEqual_(_display_slots[i], slot))
        {
            _display_slots[i] = slot;
            hw.display.setSlot(i, slot);
        }
    }
}

bool AppRuntime::renderDisplaySlot_(const DisplaySlotConfig &slot, char out[5]){
    if (!out)
        return false;
    const uint32_t node_id = slot.node_id;
    const bool local = (node_id == 0);
    const bool is_master = stackMasterActive_();
    const bool is_slave = stackSlaveActive_();
    for (size_t i = 0; i < 4; ++i)
        out[i] = ' ';
    out[4] = '\0';
    switch (slot.kind)
    {
    case DisplaySlotKind::Time:
    {
        Ds3231Mz::DateTime dt{};
        const uint32_t now_ms = millis();
        if (hw.rtc.Time(dt))
        {
            _display_rtc_cache = dt;
            _display_rtc_cache_ms = now_ms;
            _display_rtc_cache_valid = true;
        }
        else if (_display_rtc_cache_valid)
        {
            dt = _display_rtc_cache;
            advanceDisplayDateTime_(dt, (uint32_t)((now_ms - _display_rtc_cache_ms) / 1000u));
        }
        else
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (slot.field == DisplaySlotField::TimeMin)
            snprintf(out, 5, "%02u ", (unsigned)dt.minute);
        else
            snprintf(out, 5, "%02u:", (unsigned)dt.hour);
        return true;
    }
    case DisplaySlotKind::Security:
    {
        if (local)
        {
            auto &sec = control.controllers.security();
            auto sec_guard = sec.lockGuard();
            const bool armed = sec.armed();
            const char *txt = armed ? "ARM " : "DIS ";
            memcpy(out, txt, 4);
            return true;
        }
        if (is_master)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (!is_slave)
            return false;
        memcpy(out, "ERR ", 4);
        return true;
    }
    case DisplaySlotKind::Socket:
    {
        if (slot.index == 0)
            return false;
        if (local)
        {
            auto &sockets = control.controllers.sockets();
            auto sockets_guard = sockets.lockGuard();
            const SocketController::SocketState *st = sockets.state(slot.index);
            if (!st)
                return false;
            const char *txt = st->relay_on ? "ON  " : "OFF ";
            memcpy(out, txt, 4);
            return true;
        }
        if (is_master)
        {
            StackUnitSnapshot::State snapshot{};
            if (!net.network.stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
            {
                queueDisplayStackSnapshotPage_(node_id, "sockets", 0);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - snapshot.updated_ms);
            if (age_ms > 3000u)
                queueDisplayStackSnapshotPage_(node_id, "sockets", 0);
            if (age_ms > kStackNodeStaleMs)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            StackUnitSnapshot::SocketItem item{};
            if (!net.network.stackIndexSocketById(node_id, slot.index, item))
            {
                if (snapshot.sockets_enabled > snapshot.socket_count)
                    queueDisplayStackSnapshotPage_(node_id, "sockets", snapshot.socket_count);
                return false;
            }
            if (!item.enabled)
                return false;
            const char *txt = item.state ? "ON  " : "OFF ";
            memcpy(out, txt, 4);
            return true;
        }
        if (!is_slave)
            return false;
        memcpy(out, "ERR ", 4);
        return true;
    }
    case DisplaySlotKind::Light:
    {
        if (slot.index == 0)
            return false;
        if (local)
        {
            auto &sockets = control.controllers.sockets();
            auto sockets_guard = sockets.lockGuard();
            const SocketController::LightState *st = sockets.lightState(slot.index);
            if (!st)
                return false;
            const char *txt = st->relay_on ? "ON  " : "OFF ";
            memcpy(out, txt, 4);
            return true;
        }
        if (is_master)
        {
            StackUnitSnapshot::State snapshot{};
            if (!net.network.stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
            {
                queueDisplayStackSnapshotPage_(node_id, "lights", 0);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - snapshot.updated_ms);
            if (age_ms > 3000u)
                queueDisplayStackSnapshotPage_(node_id, "lights", 0);
            if (age_ms > kStackNodeStaleMs)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            StackUnitSnapshot::SocketItem item{};
            if (!net.network.stackIndexLightById(node_id, slot.index, item))
            {
                if (snapshot.lights_enabled > snapshot.light_count)
                    queueDisplayStackSnapshotPage_(node_id, "lights", snapshot.light_count);
                return false;
            }
            if (!item.enabled)
                return false;
            const char *txt = item.state ? "ON  " : "OFF ";
            memcpy(out, txt, 4);
            return true;
        }
        if (!is_slave)
            return false;
        memcpy(out, "ERR ", 4);
        return true;
    }
    case DisplaySlotKind::Meteo:
    {
        if (slot.index == 0)
            return false;
        if (local)
        {
            auto &meteo = control.controllers.meteo();
            auto meteo_guard = meteo.lockGuard();
            const MeteoController::SensorState *st = meteo.state(slot.index);
            if (!st || !st->ok)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            if (slot.field == DisplaySlotField::MeteoHum)
            {
                if (!st->has_humidity)
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                const int h = (int)roundf(st->humidity);
                snprintf(out, 5, "%2d%%", h);
            }
            else
            {
                if (!st->has_temp)
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                const int t = (int)roundf(st->temp_c);
                formatTemp3_(out, t);
            }
        }
        else if (is_master)
        {
            StackUnitSnapshot::State snapshot{};
            if (!net.network.stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
            {
                queueDisplayStackSnapshotPage_(node_id, "meteo", 0);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - snapshot.updated_ms);
            if (age_ms > 3000u)
                queueDisplayStackSnapshotPage_(node_id, "meteo", 0);
            if (age_ms > kStackNodeStaleMs)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            StackUnitSnapshot::MeteoItem item{};
            if (!net.network.stackIndexMeteoById(node_id, slot.index, item))
            {
                if (snapshot.meteo_enabled > snapshot.meteo_count)
                    queueDisplayStackSnapshotPage_(node_id, "meteo", snapshot.meteo_count);
                return false;
            }
            if (!item.enabled || !item.ok)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            if (slot.field == DisplaySlotField::MeteoHum)
            {
                if (!item.has_humidity)
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                const int h = (int)roundf(item.humidity);
                snprintf(out, 5, "%2d%%", h);
            }
            else
            {
                if (!item.has_temp)
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                const int t = (int)roundf(item.temp_c);
                formatTemp3_(out, t);
            }
            if (strlen(out) < 4)
            {
                size_t len = strlen(out);
                while (len < 4)
                    out[len++] = ' ';
                out[4] = '\0';
            }
            return true;
        }
        else
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (strlen(out) < 4)
        {
            size_t len = strlen(out);
            while (len < 4)
                out[len++] = ' ';
            out[4] = '\0';
        }
        return true;
    }
    case DisplaySlotKind::Thermo:
    {
        if (slot.index == 0)
            return false;
        if (local)
        {
            auto &thermo = control.controllers.thermo();
            auto thermo_guard = thermo.lockGuard();
            const ThermoController::DeviceState *st = thermo.state(slot.index);
            if (!st)
                return false;
            if (!st->power_on)
                memcpy(out, "IDL ", 4);
            else if (st->heat_on)
                memcpy(out, "HET ", 4);
            else if (st->cool_on)
                memcpy(out, "COL ", 4);
            else
                memcpy(out, "IDL ", 4);
            return true;
        }
        if (is_master)
        {
            StackUnitSnapshot::State snapshot{};
            if (!net.network.stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
            {
                queueDisplayStackSnapshotPage_(node_id, "thermo", 0);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - snapshot.updated_ms);
            if (age_ms > 3000u)
                queueDisplayStackSnapshotPage_(node_id, "thermo", 0);
            if (age_ms > kStackNodeStaleMs)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            StackUnitSnapshot::ThermoItem item{};
            if (!net.network.stackIndexThermoById(node_id, slot.index, item))
            {
                if (snapshot.thermo_enabled > snapshot.thermo_count)
                    queueDisplayStackSnapshotPage_(node_id, "thermo", snapshot.thermo_count);
                return false;
            }
            if (!item.enabled)
                return false;
            if (!item.power_on)
                memcpy(out, "IDL ", 4);
            else if (item.heat_on)
                memcpy(out, "HET ", 4);
            else if (item.cool_on)
                memcpy(out, "COL ", 4);
            else
                memcpy(out, "IDL ", 4);
            return true;
        }
        if (!is_slave)
            return false;
        memcpy(out, "ERR ", 4);
        return true;
    }
    case DisplaySlotKind::Tank:
    {
        if (slot.index == 0)
            return false;
        if (local)
        {
            auto &tanks = control.controllers.tanks();
            auto tanks_guard = tanks.lockGuard();
            const TankController::TankState *st = tanks.state(slot.index);
            if (!st || !st->levels_ok)
                return false;
            if (st->level_full)
                memcpy(out, "99% ", 4);
            else if (st->level_mid)
                memcpy(out, "66% ", 4);
            else if (st->level_low)
                memcpy(out, "33% ", 4);
            else
                memcpy(out, "0%  ", 4);
            return true;
        }
        if (is_master)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (!is_slave)
            return false;
        memcpy(out, "ERR ", 4);
        return true;
    }
    case DisplaySlotKind::Septic:
    {
        if (slot.index == 0)
            return false;
        if (local)
        {
            const size_t idx = (size_t)(slot.index - 1);
            auto &septic = control.controllers.septic();
            auto septic_guard = septic.lockGuard();
            const SepticController::SepticState *st = septic.stateByIndex(idx);
            if (!st)
                return false;
            if (st->alarm)
                memcpy(out, "ALM ", 4);
            else if (st->warning)
                memcpy(out, "WRN ", 4);
            else
                memcpy(out, "OK  ", 4);
            return true;
        }
        if (is_master)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (!is_slave)
            return false;
        memcpy(out, "ERR ", 4);
        return true;
    }
    case DisplaySlotKind::Avr:
    {
        if (local)
        {
            auto &avr = control.controllers.avr();
            auto avr_guard = avr.lockGuard();
            const auto &st = avr.state();
            if (slot.field == DisplaySlotField::AvrMainOk)
                memcpy(out, st.main_ok ? "ON  " : "OFF ", 4);
            else if (slot.field == DisplaySlotField::AvrReserveOk)
                memcpy(out, st.reserve_ok ? "ON  " : "OFF ", 4);
            else if (st.active_source == AvrController::Source::Main)
                memcpy(out, "MAN ", 4);
            else if (st.active_source == AvrController::Source::Reserve)
                memcpy(out, "RES ", 4);
            else
                memcpy(out, "OFF ", 4);
            return true;
        }
        if (is_master)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        return false;
    }
    case DisplaySlotKind::Leak:
    {
        if (slot.index == 0)
            return false;
        if (local)
        {
            auto &leak = control.controllers.leak();
            auto leak_guard = leak.lockGuard();
            const auto *st = leak.state(slot.index);
            if (!st)
                return false;
            if (st->wet || st->alarm_latched)
                memcpy(out, "ALRM", 4);
            else
                memcpy(out, "DRY ", 4);
            return true;
        }
        if (is_master)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        return false;
    }
    case DisplaySlotKind::Text:
    {
        if (!slot.text[0])
            return false;
        for (size_t i = 0; i < 4; ++i)
            out[i] = slot.text[i] ? slot.text[i] : ' ';
        out[4] = '\0';
        return true;
    }
    case DisplaySlotKind::None:
    default:
        return false;
    }
}

void AppRuntime::formatTemp3_(char out[5], int t){
    if (!out)
        return;
    if (t <= -10)
        snprintf(out, 5, "%3d", t);
    else
        snprintf(out, 5, "%2d%c", t, Display::kDegreeChar);
}

bool AppRuntime::displaySlotEqual_(const DisplaySlotConfig &a, const DisplaySlotConfig &b){
    if (a.kind != b.kind || a.node_id != b.node_id || a.index != b.index || a.field != b.field)
        return false;
    return strncmp(a.text, b.text, sizeof(a.text)) == 0;
}

