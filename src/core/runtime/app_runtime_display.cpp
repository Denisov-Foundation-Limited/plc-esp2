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
            const auto *cache = _stack_cache.securityCache(node_id);
            if (!cache || !cache->has_data)
            {
                _stack_cache.requestSecurity(node_id);
                return false;
            }
            if (!cache->last_ok && cache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            const char *txt = cache->armed ? "ARM " : "DIS ";
            memcpy(out, txt, 4);
            return true;
        }
        if (!is_slave)
            return false;
        const auto *rcache = net.stack_slave.remoteSecurityCache(node_id);
        if (!rcache || !rcache->has_data)
        {
            net.stack_slave.requestRemoteSecurity(node_id);
            return false;
        }
        const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
        if (age_ms > 3000u)
            net.stack_slave.requestRemoteSecurity(node_id);
        if (age_ms > 8000u)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (!rcache->last_ok && rcache->last_error.length())
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        const char *txt = rcache->armed ? "ARM " : "DIS ";
        memcpy(out, txt, 4);
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
            const auto *cache = _stack_cache.socketsCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                _stack_cache.requestSockets(node_id);
                return false;
            }
            if (!cache->last_ok && cache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                const char *txt = it.state ? "ON  " : "OFF ";
                memcpy(out, txt, 4);
                return true;
            }
            return false;
        }
        if (!is_slave)
            return false;
        const auto *rcache = net.stack_slave.remoteSocketsCache(node_id);
        if (!rcache || !rcache->has_data || !rcache->items)
        {
            net.stack_slave.requestRemoteSockets(node_id);
            return false;
        }
        const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
        if (age_ms > 3000u)
            net.stack_slave.requestRemoteSockets(node_id);
        if (age_ms > 8000u)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (!rcache->last_ok && rcache->last_error.length())
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        for (size_t i = 0; i < rcache->item_count; ++i)
        {
            const auto &it = rcache->items[i];
            if (it.id != slot.index || !it.enabled)
                continue;
            const char *txt = it.state ? "ON  " : "OFF ";
            memcpy(out, txt, 4);
            return true;
        }
        return false;
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
            const auto *cache = _stack_cache.lightsCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                _stack_cache.requestLights(node_id);
                return false;
            }
            if (!cache->last_ok && cache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                const char *txt = it.state ? "ON  " : "OFF ";
                memcpy(out, txt, 4);
                return true;
            }
            return false;
        }
        if (!is_slave)
            return false;
        const auto *rcache = net.stack_slave.remoteLightsCache(node_id);
        if (!rcache || !rcache->has_data || !rcache->items)
        {
            net.stack_slave.requestRemoteLights(node_id);
            return false;
        }
        const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
        if (age_ms > 3000u)
            net.stack_slave.requestRemoteLights(node_id);
        if (age_ms > 8000u)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (!rcache->last_ok && rcache->last_error.length())
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        for (size_t i = 0; i < rcache->item_count; ++i)
        {
            const auto &it = rcache->items[i];
            if (it.id != slot.index || !it.enabled)
                continue;
            const char *txt = it.state ? "ON  " : "OFF ";
            memcpy(out, txt, 4);
            return true;
        }
        return false;
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
            const auto *cache = _stack_cache.meteoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                _stack_cache.requestMeteo(node_id);
                return false;
            }
            if (!cache->last_ok && cache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            const StackCache::StackMeteoItem *found = nullptr;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                if (cache->items[i].id == slot.index && cache->items[i].enabled)
                {
                    found = &cache->items[i];
                    break;
                }
            }
            if (!found || !found->ok)
                return false;
            if (slot.field == DisplaySlotField::MeteoHum)
            {
                if (!found->has_hum)
                    return false;
                const int h = (int)roundf(found->hum);
                snprintf(out, 5, "%2d%%", h);
            }
            else
            {
            if (!found->has_temp)
                return false;
            const int t = (int)roundf(found->temp_c);
            formatTemp3_(out, t);
        }
    }
        else
        {
            const auto *cache = net.stack_slave.remoteMeteoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                net.stack_slave.requestRemoteMeteoAll();
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - cache->updated_ms);
            if (age_ms > 3000u)
                net.stack_slave.requestRemoteMeteoAll();
            if (age_ms > kStackNodeStaleMs)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            if (!cache->last_ok && cache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            const StackSlaveHandler::RemoteMeteoItem *found = nullptr;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                if (cache->items[i].id == slot.index)
                {
                    found = &cache->items[i];
                    break;
                }
            }
            if (!found || !found->ok)
                return false;
            if (slot.field == DisplaySlotField::MeteoHum)
            {
                if (!found->has_hum)
                    return false;
                const int h = (int)roundf(found->hum);
                snprintf(out, 5, "%2d%%", h);
            }
            else
            {
            if (!found->has_temp)
                return false;
            const int t = (int)roundf(found->temp_c);
            formatTemp3_(out, t);
        }
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
            const auto *cache = _stack_cache.thermoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                _stack_cache.requestThermo(node_id);
                return false;
            }
            if (!cache->last_ok && cache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                if (!it.power_on)
                    memcpy(out, "IDL ", 4);
                else if (it.heat_on)
                    memcpy(out, "HET ", 4);
                else if (it.cool_on)
                    memcpy(out, "COL ", 4);
                else
                    memcpy(out, "IDL ", 4);
                return true;
            }
            return false;
        }
        if (!is_slave)
            return false;
        const auto *rcache = net.stack_slave.remoteThermoCache(node_id);
        if (!rcache || !rcache->has_data || !rcache->items)
        {
            net.stack_slave.requestRemoteThermo(node_id);
            return false;
        }
        const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
        if (age_ms > 3000u)
            net.stack_slave.requestRemoteThermo(node_id);
        if (age_ms > kStackNodeStaleMs)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (!rcache->last_ok && rcache->last_error.length())
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        for (size_t i = 0; i < rcache->item_count; ++i)
        {
            const auto &it = rcache->items[i];
            if (it.id != slot.index || !it.enabled)
                continue;
            if (!it.power_on)
                memcpy(out, "IDL ", 4);
            else if (it.heat_on)
                memcpy(out, "HET ", 4);
            else if (it.cool_on)
                memcpy(out, "COL ", 4);
            else
                memcpy(out, "IDL ", 4);
            return true;
        }
        return false;
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
            const auto *cache = _stack_cache.tanksCache(node_id);
            if (!cache || !cache->items)
            {
                _stack_cache.requestTanks(node_id);
                return false;
            }
            const bool node_online = net.network.stackMaster().nodeIsOnline(node_id, kStackNodeStaleMs);
            const uint32_t now = millis();
            const uint32_t age_ms = (uint32_t)(now - cache->updated_ms);
            if (age_ms > 3000u)
                _stack_cache.requestTanks(node_id);
            if (!cache->has_data)
            {
                bool no_data_long = false;
                if (cache->pending_since_ms)
                    no_data_long = (uint32_t)(now - cache->pending_since_ms) > kDisplayNoDataErrMs;
                else if (cache->updated_ms)
                    no_data_long = (uint32_t)(now - cache->updated_ms) > kDisplayNoDataErrMs;
                else
                    no_data_long = now > kDisplayNoDataErrMs;
                if (no_data_long && !node_online)
                {
                    memcpy(out, "ERR ", 4);
                    return true;
                }
                return false;
            }
            if (age_ms > kDisplayNoDataErrMs && !node_online)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                if (!it.levels_ok)
                    return false;
                if (it.level_full)
                    memcpy(out, "99% ", 4);
                else if (it.level_mid)
                    memcpy(out, "66% ", 4);
                else if (it.level_low)
                    memcpy(out, "33% ", 4);
                else
                    memcpy(out, "0%  ", 4);
                return true;
            }
            return false;
        }
        if (!is_slave)
            return false;
        const auto *rcache = net.stack_slave.remoteTanksCache(node_id);
        if (!rcache || !rcache->items)
        {
            net.stack_slave.requestRemoteTanks(node_id);
            return false;
        }
        const bool master_connected = net.network.stackNode().connected();
        const uint32_t now = millis();
        const uint32_t age_ms = (uint32_t)(now - rcache->updated_ms);
        if (age_ms > 3000u)
            net.stack_slave.requestRemoteTanks(node_id);
        if (!rcache->has_data)
        {
            bool no_data_long = false;
            if (rcache->pending_since_ms)
                no_data_long = (uint32_t)(now - rcache->pending_since_ms) > kDisplayNoDataErrMs;
            else if (rcache->updated_ms)
                no_data_long = (uint32_t)(now - rcache->updated_ms) > kDisplayNoDataErrMs;
            else
                no_data_long = now > kDisplayNoDataErrMs;
            if (no_data_long && !master_connected)
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            return false;
        }
        if (age_ms > kDisplayNoDataErrMs && !master_connected)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        for (size_t i = 0; i < rcache->item_count; ++i)
        {
            const auto &it = rcache->items[i];
            if (it.id != slot.index || !it.enabled)
                continue;
            if (!it.levels_ok)
                return false;
            if (it.level_full)
                memcpy(out, "99% ", 4);
            else if (it.level_mid)
                memcpy(out, "66% ", 4);
            else if (it.level_low)
                memcpy(out, "33% ", 4);
            else
                memcpy(out, "0%  ", 4);
            return true;
        }
        return false;
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
            const auto *cache = _stack_cache.septicCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                _stack_cache.requestSeptic(node_id);
                return false;
            }
            if (!cache->last_ok && cache->last_error.length())
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                if (it.alarm)
                    memcpy(out, "ALM ", 4);
                else if (it.warning)
                    memcpy(out, "WRN ", 4);
                else
                    memcpy(out, "OK  ", 4);
                return true;
            }
            return false;
        }
        if (!is_slave)
            return false;
        const auto *rcache = net.stack_slave.remoteSepticCache(node_id);
        if (!rcache || !rcache->has_data || !rcache->items)
        {
            net.stack_slave.requestRemoteSeptic(node_id);
            return false;
        }
        const uint32_t age_ms = (uint32_t)(millis() - rcache->updated_ms);
        if (age_ms > 3000u)
            net.stack_slave.requestRemoteSeptic(node_id);
        if (age_ms > 8000u)
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        if (!rcache->last_ok && rcache->last_error.length())
        {
            memcpy(out, "ERR ", 4);
            return true;
        }
        for (size_t i = 0; i < rcache->item_count; ++i)
        {
                const auto &it = rcache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                if (it.alarm)
                    memcpy(out, "ALM ", 4);
                else if (it.warning)
                    memcpy(out, "WRN ", 4);
                else
                    memcpy(out, "OK  ", 4);
                return true;
        }
        return false;
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
            const auto *cache = _stack_cache.avrCache(node_id);
            if (!cache || !cache->has_data)
            {
                _stack_cache.requestAvr(node_id);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - cache->updated_ms);
            if (age_ms > 3000u)
                _stack_cache.requestAvr(node_id);
            if (age_ms > 8000u || (!cache->last_ok && cache->last_error.length()))
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            if (slot.field == DisplaySlotField::AvrMainOk)
                memcpy(out, cache->main_ok ? "ON  " : "OFF ", 4);
            else if (slot.field == DisplaySlotField::AvrReserveOk)
                memcpy(out, cache->reserve_ok ? "ON  " : "OFF ", 4);
            else if (strcmp(cache->active_source, "main") == 0)
                memcpy(out, "MAN ", 4);
            else if (strcmp(cache->active_source, "reserve") == 0)
                memcpy(out, "RES ", 4);
            else
                memcpy(out, "OFF ", 4);
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
            const auto *cache = _stack_cache.leakCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                _stack_cache.requestLeak(node_id);
                return false;
            }
            const uint32_t age_ms = (uint32_t)(millis() - cache->updated_ms);
            if (age_ms > 3000u)
                _stack_cache.requestLeak(node_id);
            if (age_ms > 8000u || (!cache->last_ok && cache->last_error.length()))
            {
                memcpy(out, "ERR ", 4);
                return true;
            }
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != slot.index || !it.enabled)
                    continue;
                if (it.wet || it.alarm_latched)
                    memcpy(out, "ALRM", 4);
                else
                    memcpy(out, "DRY ", 4);
                return true;
            }
            return false;
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

