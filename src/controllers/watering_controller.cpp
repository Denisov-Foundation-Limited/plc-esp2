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

#include "controllers/watering_controller.hpp"

WateringController::WateringController(Gpio &gpio, TankController &tanks, RTC &rtc, Logger &logs)
 : _gpio(gpio), _tanks(tanks), _rtc(rtc), _logs(logs){
    reset_();
}

bool WateringController::begin(){
    auto guard = _lock.guard();
    _logs.info(F("WATER"), F("Controller init"));
    return true;
}

void WateringController::setEventHandler(WateringController::EventHandler cb, void *ctx){
    auto guard = _lock.guard();
    _event_cb = cb;
    _event_ctx = ctx;
}

void WateringController::setEventHandlerSecondary(WateringController::EventHandler cb, void *ctx){
    auto guard = _lock.guard();
    _event_cb_secondary = cb;
    _event_ctx_secondary = ctx;
}

void WateringController::task(){
    struct PendingEvent
    {
        Event ev;
        RuleConfig cfg;
        RuleState st;
    };
    PendingEvent pending[kRuleCount]{};
    size_t pending_count = 0;
    auto push_event = [&](Event ev, const RuleConfig &cfg, const RuleState &st){
        if (pending_count >= kRuleCount)
            return;
        pending[pending_count].ev = ev;
        pending[pending_count].cfg = cfg;
        pending[pending_count].st = st;
        ++pending_count;
    };
    Ds3231Mz::DateTime now{};
    bool have_time = false;
    {
        auto guard = _lock.guard();
        if (!_controller_enabled)
            return;
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            RuleConfig &cfg = _cfg[i];
            RuleState &st = _state[i];

            if (!cfg.enabled || !st.status)
            {
                const bool was_active = st.active;
                stopIfActive_(cfg, st, Event::Stop, false);
                if (was_active)
                    push_event(Event::Stop, cfg, st);
                st.paused = false;
                st.remaining_ms = 0;
                continue;
            }

            const bool tank_empty = isTankEmpty_(cfg);
            if (tank_empty && st.active)
            {
                const Event ev = (cfg.resume_after_refill && timeAfterOrEqual_(st.end_ms, millis())) ? Event::PauseEmpty : Event::StopEmpty;
                stopForEmpty_(cfg, st, false);
                push_event(ev, cfg, st);
                continue;
            }

            if (st.paused)
            {
                if (!tank_empty && cfg.resume_after_refill && isResumeLevelReached_(cfg))
                {
                    resumeAfterRefill_(cfg, st, false);
                    push_event(Event::Resume, cfg, st);
                }
                continue;
            }

            if (st.active)
            {
                if (timeAfterOrEqual_(millis(), st.end_ms))
                {
                    stopIfActive_(cfg, st, Event::StopDone, false);
                    push_event(Event::StopDone, cfg, st);
                }
                continue;
            }

            if (!isStartValid_(cfg) || cfg.port == kInvalidPort)
                continue;
            if (tank_empty)
                continue;

            if (!have_time)
            {
                if (!_rtc.Time(now))
                    return;
                have_time = true;
            }

            if (!isWeekdayAllowed_(cfg, now.day_of_week))
                continue;
            for (uint8_t slot = 0; slot < kTimeSlotCount; ++slot)
            {
                uint8_t slot_hour = 0;
                uint8_t slot_minute = 0;
                uint32_t slot_duration_sec = 0;
                bool slot_enabled = false;
                getSlot_(cfg, slot, slot_enabled, slot_hour, slot_minute, slot_duration_sec);
                if (!slot_enabled || slot_duration_sec == 0)
                    continue;
                if (slot_hour > 23 || slot_minute > 59)
                    continue;
                if (now.hour != slot_hour || now.minute != slot_minute)
                    continue;

                const uint32_t key = makeStartKey_(now.year, now.month, now.day, slot_hour, slot_minute, slot);
                if (st.last_start_key == key)
                    continue;

                st.last_start_key = key;
                st.active = true;
                st.paused = false;
                st.remaining_ms = 0;
                st.end_ms = millis() + slot_duration_sec * 1000u;
                writePort_(cfg.port, true);
                _logs.info(F("WATER"), F("start: rule: %u slot: %u port: %u tank: %u duration_s: %lu"),
                           (unsigned)cfg.id, (unsigned)(slot + 1u), (unsigned)cfg.port, (unsigned)cfg.tank_id,
                           (unsigned long)slot_duration_sec);
                push_event(Event::Start, cfg, st);
                _runtime_dirty = true;
                break;
            }
        }
    }
    for (size_t i = 0; i < pending_count; ++i)
        notifyEvent_(pending[i].ev, pending[i].cfg, pending[i].st);
}

void WateringController::applyConfig(JsonArrayConst rules){
    auto guard = _lock.guard();
    reset_();
    size_t idx = 0;
    for (JsonVariantConst v : rules)
    {
        if (idx >= kRuleCount)
            break;
        if (!v.is<JsonObjectConst>())
        {
            ++idx;
            continue;
        }
        JsonObjectConst obj = v.as<JsonObjectConst>();
        uint8_t id = (uint8_t)(idx + 1);
        if (obj["id"].is<unsigned>())
        {
            const unsigned raw = obj["id"].as<unsigned>();
            if (raw <= 0xFFu)
                id = (uint8_t)raw;
        }
        size_t dst = 0;
        if (!indexById_(id, dst))
        {
            ++idx;
            continue;
        }
        RuleConfig &cfg = _cfg[dst];
        cfg.id = id;
        bool enabled_set = false;
        if (obj["enabled"].is<bool>())
        {
            cfg.enabled = obj["enabled"].as<bool>();
            enabled_set = true;
        }
        if (obj["name"].is<const char *>())
            cfg.name = obj["name"].as<const char *>();
        if (obj["port"].is<unsigned>())
        {
            const unsigned raw = obj["port"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.port = (uint8_t)raw;
        }
        if (obj["tank_id"].is<unsigned>())
        {
            const unsigned raw = obj["tank_id"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.tank_id = (uint8_t)raw;
        }
        if (obj["weekdays_mask"].is<unsigned>())
        {
            const unsigned raw = obj["weekdays_mask"].as<unsigned>();
            cfg.weekdays_mask = (uint8_t)(raw & 0x7Fu);
        }
        else if (obj["weekdays"].is<JsonArrayConst>())
        {
            uint8_t mask = 0;
            JsonArrayConst days = obj["weekdays"].as<JsonArrayConst>();
            for (JsonVariantConst dv : days)
            {
                if (!dv.is<unsigned>())
                    continue;
                const unsigned raw = dv.as<unsigned>();
                if (raw >= 1 && raw <= 7)
                    mask |= (uint8_t)(1u << (raw - 1u));
            }
            cfg.weekdays_mask = mask;
        }
        else if (obj["year"].is<unsigned>() && obj["month"].is<unsigned>() && obj["day"].is<unsigned>())
        {
            const unsigned raw_y = obj["year"].as<unsigned>();
            const unsigned raw_m = obj["month"].as<unsigned>();
            const unsigned raw_d = obj["day"].as<unsigned>();
            if (raw_y >= 2000 && raw_y <= 2099 && raw_m >= 1 && raw_m <= 12 && raw_d >= 1 && raw_d <= 31)
            {
                const uint8_t dow = calcDow_((uint16_t)raw_y, (uint8_t)raw_m, (uint8_t)raw_d);
                if (dow >= 1 && dow <= 7)
                    cfg.weekdays_mask = (uint8_t)(1u << (dow - 1u));
            }
        }
        if (obj["hour"].is<unsigned>())
        {
            const unsigned raw = obj["hour"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.hour = (uint8_t)raw;
        }
        if (obj["minute"].is<unsigned>())
        {
            const unsigned raw = obj["minute"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.minute = (uint8_t)raw;
        }
        bool slot1_enabled_set = false;
        bool slot2_enabled_set = false;
        bool slot3_enabled_set = false;
        if (obj["duration_s"].is<unsigned long>())
            cfg.duration_sec = (uint32_t)obj["duration_s"].as<unsigned long>();
        if (obj["slot1_enabled"].is<bool>())
        {
            cfg.slot1_enabled = obj["slot1_enabled"].as<bool>();
            slot1_enabled_set = true;
        }
        if (obj["hour2"].is<unsigned>())
        {
            const unsigned raw = obj["hour2"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.hour2 = (uint8_t)raw;
        }
        if (obj["minute2"].is<unsigned>())
        {
            const unsigned raw = obj["minute2"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.minute2 = (uint8_t)raw;
        }
        if (obj["duration2_s"].is<unsigned long>())
            cfg.duration2_sec = (uint32_t)obj["duration2_s"].as<unsigned long>();
        if (obj["slot2_enabled"].is<bool>())
        {
            cfg.slot2_enabled = obj["slot2_enabled"].as<bool>();
            slot2_enabled_set = true;
        }
        if (obj["hour3"].is<unsigned>())
        {
            const unsigned raw = obj["hour3"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.hour3 = (uint8_t)raw;
        }
        if (obj["minute3"].is<unsigned>())
        {
            const unsigned raw = obj["minute3"].as<unsigned>();
            if (raw <= 0xFFu)
                cfg.minute3 = (uint8_t)raw;
        }
        if (obj["duration3_s"].is<unsigned long>())
            cfg.duration3_sec = (uint32_t)obj["duration3_s"].as<unsigned long>();
        if (obj["slot3_enabled"].is<bool>())
        {
            cfg.slot3_enabled = obj["slot3_enabled"].as<bool>();
            slot3_enabled_set = true;
        }
        if (obj["resume_after_refill"].is<bool>())
            cfg.resume_after_refill = obj["resume_after_refill"].as<bool>();
        if (obj["resume_level"].is<const char *>())
        {
            const char *lvl = obj["resume_level"].as<const char *>();
            if (lvl)
            {
                String t = lvl;
                t.toLowerCase();
                if (t == "mid")
                    cfg.resume_level = 1;
                else if (t == "full")
                    cfg.resume_level = 2;
                else
                    cfg.resume_level = 0;
            }
        }
        else if (obj["resume_level"].is<unsigned>())
        {
            const unsigned raw = obj["resume_level"].as<unsigned>();
            if (raw <= 2u)
                cfg.resume_level = (uint8_t)raw;
        }
        if (!enabled_set)
            cfg.enabled = true;
        if (!slot1_enabled_set)
            cfg.slot1_enabled = slotConfigured_(cfg, 0);
        if (!slot2_enabled_set)
            cfg.slot2_enabled = slotConfigured_(cfg, 1);
        if (!slot3_enabled_set)
            cfg.slot3_enabled = slotConfigured_(cfg, 2);
        ++idx;
    }
}

void WateringController::serialize(JsonArray out) const{
    auto guard = _lock.guard();
    for (size_t i = 0; i < kRuleCount; ++i)
    {
        const RuleConfig &cfg = _cfg[i];
        if (!cfg.enabled)
            continue;
        JsonObject obj = out.add<JsonObject>();
        obj["id"] = cfg.id;
        obj["enabled"] = cfg.enabled;
        if (cfg.name.length())
            obj["name"] = cfg.name;
        if (cfg.port != kInvalidPort)
            obj["port"] = cfg.port;
        if (cfg.tank_id != 0)
            obj["tank_id"] = cfg.tank_id;
        if (cfg.weekdays_mask)
            obj["weekdays_mask"] = cfg.weekdays_mask;
        if (cfg.duration_sec && cfg.hour <= 23 && cfg.minute <= 59)
        {
            obj["hour"] = cfg.hour;
            obj["minute"] = cfg.minute;
            obj["duration_s"] = cfg.duration_sec;
            obj["slot1_enabled"] = cfg.slot1_enabled;
        }
        else if (cfg.slot1_enabled)
        {
            obj["slot1_enabled"] = cfg.slot1_enabled;
        }
        if (cfg.duration2_sec && cfg.hour2 <= 23 && cfg.minute2 <= 59)
        {
            obj["hour2"] = cfg.hour2;
            obj["minute2"] = cfg.minute2;
            obj["duration2_s"] = cfg.duration2_sec;
            obj["slot2_enabled"] = cfg.slot2_enabled;
        }
        else if (cfg.slot2_enabled)
        {
            obj["slot2_enabled"] = cfg.slot2_enabled;
        }
        if (cfg.duration3_sec && cfg.hour3 <= 23 && cfg.minute3 <= 59)
        {
            obj["hour3"] = cfg.hour3;
            obj["minute3"] = cfg.minute3;
            obj["duration3_s"] = cfg.duration3_sec;
            obj["slot3_enabled"] = cfg.slot3_enabled;
        }
        else if (cfg.slot3_enabled)
        {
            obj["slot3_enabled"] = cfg.slot3_enabled;
        }
        obj["resume_after_refill"] = cfg.resume_after_refill;
        if (cfg.resume_level == 1)
            obj["resume_level"] = "mid";
        else if (cfg.resume_level == 2)
            obj["resume_level"] = "full";
        else
            obj["resume_level"] = "low";
    }
}

void WateringController::applySnapshot(const uint8_t *status_mask, size_t bytes){
    auto guard = _lock.guard();
    if (!status_mask || bytes == 0)
        return;
    for (size_t i = 0; i < kRuleCount; ++i)
    {
        const size_t byte = i / 8u;
        const uint8_t bit = (uint8_t)(1u << (i % 8u));
        if (byte >= bytes)
            break;
        _state[i].status = (status_mask[byte] & bit) != 0;
    }
    _dirty = false;
}

void WateringController::buildSnapshot(uint8_t *status_mask, size_t bytes) const{
    auto guard = _lock.guard();
    if (!status_mask || bytes == 0)
        return;
    memset(status_mask, 0, bytes);
    for (size_t i = 0; i < kRuleCount; ++i)
    {
        if (!_state[i].status)
            continue;
        const size_t byte = i / 8u;
        const uint8_t bit = (uint8_t)(1u << (i % 8u));
        if (byte >= bytes)
            break;
        status_mask[byte] |= bit;
    }
}

void WateringController::applyRuntimeSnapshot(const uint8_t *active_mask, const uint8_t *paused_mask, const uint32_t *remaining_ms,
 const uint32_t *last_start_key, size_t bytes){
    auto guard = _lock.guard();
    if (!active_mask || !paused_mask || !remaining_ms || !last_start_key || bytes == 0)
        return;
    for (size_t i = 0; i < kRuleCount; ++i)
    {
        RuleConfig &cfg = _cfg[i];
        RuleState &st = _state[i];
        const size_t byte = i / 8u;
        const uint8_t bit = (uint8_t)(1u << (i % 8u));
        if (byte >= bytes)
            break;
        st.last_start_key = last_start_key[i];
        st.remaining_ms = remaining_ms[i];
        st.active = (active_mask[byte] & bit) != 0;
        st.paused = (paused_mask[byte] & bit) != 0;
        if (!cfg.enabled || !st.status || st.remaining_ms == 0)
        {
            st.active = false;
            st.paused = false;
            st.remaining_ms = 0;
            continue;
        }
        if (st.active)
        {
            st.end_ms = millis() + st.remaining_ms;
            writePort_(cfg.port, true);
        }
    }
    _runtime_dirty = false;
}

void WateringController::buildRuntimeSnapshot(uint8_t *active_mask, uint8_t *paused_mask, uint32_t *remaining_ms,
 uint32_t *last_start_key, size_t bytes) const{
    auto guard = _lock.guard();
    if (!active_mask || !paused_mask || !remaining_ms || !last_start_key || bytes == 0)
        return;
    memset(active_mask, 0, bytes);
    memset(paused_mask, 0, bytes);
    for (size_t i = 0; i < kRuleCount; ++i)
    {
        const RuleState &st = _state[i];
        const size_t byte = i / 8u;
        const uint8_t bit = (uint8_t)(1u << (i % 8u));
        if (byte >= bytes)
            break;
        if (st.active)
            active_mask[byte] |= bit;
        if (st.paused)
            paused_mask[byte] |= bit;
        last_start_key[i] = st.last_start_key;
        if (st.active)
        {
            const uint32_t now = millis();
            remaining_ms[i] = timeAfterOrEqual_(st.end_ms, now) ? (st.end_ms - now) : 0;
        }
        else
        {
            remaining_ms[i] = st.remaining_ms;
        }
    }
}

bool WateringController::setEnabled(size_t id, bool enabled){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].enabled = enabled;
    if (!enabled)
        stopIfActive_(_cfg[idx], _state[idx], Event::Stop);
    return true;
}

bool WateringController::setName(size_t id, const String &name){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].name = name;
    return true;
}

bool WateringController::setPort(size_t id, uint8_t port){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].port = port;
    if (port == kInvalidPort)
        stopIfActive_(_cfg[idx], _state[idx], Event::Stop);
    return true;
}

bool WateringController::setStartDate(size_t id, uint16_t year, uint8_t month, uint8_t day){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31)
        return false;
    const uint8_t dow = calcDow_(year, month, day);
    if (dow < 1 || dow > 7)
        return false;
    _cfg[idx].weekdays_mask = (uint8_t)(1u << (dow - 1u));
    return true;
}

bool WateringController::setStartTime(size_t id, uint8_t hour, uint8_t minute){
    auto guard = _lock.guard();
    return setStartTimeSlot(id, 0, hour, minute);
}

bool WateringController::setStartTimeSlot(size_t id, uint8_t slot, uint8_t hour, uint8_t minute){
    auto guard = _lock.guard();
    if (slot >= kTimeSlotCount)
        return false;
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    setSlotTime_(slot, _cfg[idx], hour, minute);
    if (slotConfigured_(_cfg[idx], slot))
        setSlotEnabled_(slot, _cfg[idx], true);
    return true;
}

bool WateringController::setWeekdaysMask(size_t id, uint8_t mask){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].weekdays_mask = (uint8_t)(mask & 0x7Fu);
    return true;
}

bool WateringController::setDuration(size_t id, uint32_t duration_sec){
    auto guard = _lock.guard();
    return setDurationSlot(id, 0, duration_sec);
}

bool WateringController::setDurationSlot(size_t id, uint8_t slot, uint32_t duration_sec){
    auto guard = _lock.guard();
    if (slot >= kTimeSlotCount)
        return false;
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    setSlotDuration_(slot, _cfg[idx], duration_sec);
    if (slotConfigured_(_cfg[idx], slot))
        setSlotEnabled_(slot, _cfg[idx], true);
    return true;
}

bool WateringController::setSlotEnabled(size_t id, uint8_t slot, bool enabled){
    auto guard = _lock.guard();
    if (slot >= kTimeSlotCount)
        return false;
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    setSlotEnabled_(slot, _cfg[idx], enabled);
    return true;
}

bool WateringController::setStatus(size_t id, bool status){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    if (_state[idx].status == status)
        return true;
    _state[idx].status = status;
    if (!status)
        stopIfActive_(_cfg[idx], _state[idx], Event::Stop);
    _state[idx].paused = false;
    _state[idx].remaining_ms = 0;
    _runtime_dirty = true;
    _dirty = true;
    return true;
}

bool WateringController::controllerEnabled() const{
    auto guard = _lock.guard();
    return _controller_enabled;
}

void WateringController::setControllerEnabled(bool enabled){
    auto guard = _lock.guard();
    if (_controller_enabled == enabled)
        return;
    _controller_enabled = enabled;
    if (!_controller_enabled)
    {
        for (size_t i = 0; i < kRuleCount; ++i)
            stopIfActive_(_cfg[i], _state[i], Event::Stop);
        reset_();
    }
}

bool WateringController::setTankId(size_t id, uint8_t tank_id){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].tank_id = tank_id;
    if (tank_id == 0)
        return true;
    if (isTankEmpty_(_cfg[idx]))
        stopForEmpty_(_cfg[idx], _state[idx]);
    return true;
}

bool WateringController::setResumeAfterRefill(size_t id, bool enable){
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].resume_after_refill = enable;
    if (!enable && _state[idx].paused)
    {
        _state[idx].paused = false;
        _state[idx].remaining_ms = 0;
        _runtime_dirty = true;
    }
    return true;
}

bool WateringController::setResumeLevel(size_t id, uint8_t level){
    auto guard = _lock.guard();
    if (level > 2)
        return false;
    size_t idx = 0;
    if (!indexById_(id, idx))
        return false;
    _cfg[idx].resume_level = level;
    return true;
}

bool WateringController::takeDirty(){
    auto guard = _lock.guard();
    const bool v = _dirty;
    _dirty = false;
    return v;
}

bool WateringController::takeRuntimeDirty(){
    auto guard = _lock.guard();
    const bool v = _runtime_dirty;
    _runtime_dirty = false;
    return v;
}

const WateringController::RuleConfig *WateringController::config(size_t id) const{
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_cfg[idx];
}

const WateringController::RuleState *WateringController::state(size_t id) const{
    auto guard = _lock.guard();
    size_t idx = 0;
    if (!indexById_(id, idx))
        return nullptr;
    return &_state[idx];
}

const WateringController::RuleConfig *WateringController::configByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kRuleCount)
        return nullptr;
    return &_cfg[idx];
}

const WateringController::RuleState *WateringController::stateByIndex(size_t idx) const{
    auto guard = _lock.guard();
    if (idx >= kRuleCount)
        return nullptr;
    return &_state[idx];
}

void WateringController::reset_(){
    for (size_t i = 0; i < kRuleCount; ++i)
    {
        _cfg[i] = RuleConfig{};
        _cfg[i].id = (uint8_t)(i + 1);
        _cfg[i].duration_sec = 0;
        _state[i] = RuleState{};
    }
}

bool WateringController::isStartValid_(const WateringController::RuleConfig &cfg){
    if (cfg.weekdays_mask == 0)
        return false;
    for (uint8_t slot = 0; slot < kTimeSlotCount; ++slot)
    {
        bool enabled = false;
        uint8_t h = 0;
        uint8_t m = 0;
        uint32_t d = 0;
        getSlot_(cfg, slot, enabled, h, m, d);
        if (!enabled || d == 0)
            continue;
        if (h <= 23 && m <= 59)
            return true;
    }
    return false;
}

uint32_t WateringController::makeStartKey_(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t slot){
    const uint32_t base = (uint32_t)year * 100000000u + (uint32_t)month * 1000000u + (uint32_t)day * 10000u +
                          (uint32_t)hour * 100u + (uint32_t)minute;
    return base * 10u + (uint32_t)(slot % kTimeSlotCount);
}

void WateringController::getSlot_(const WateringController::RuleConfig &cfg, uint8_t slot, bool &enabled, uint8_t &hour,
                                  uint8_t &minute, uint32_t &duration_sec){
    if (slot == 0)
    {
        enabled = cfg.slot1_enabled;
        hour = cfg.hour;
        minute = cfg.minute;
        duration_sec = cfg.duration_sec;
        return;
    }
    if (slot == 1)
    {
        enabled = cfg.slot2_enabled;
        hour = cfg.hour2;
        minute = cfg.minute2;
        duration_sec = cfg.duration2_sec;
        return;
    }
    enabled = cfg.slot3_enabled;
    hour = cfg.hour3;
    minute = cfg.minute3;
    duration_sec = cfg.duration3_sec;
}

void WateringController::setSlotTime_(uint8_t slot, WateringController::RuleConfig &cfg, uint8_t hour, uint8_t minute){
    if (slot == 0)
    {
        cfg.hour = hour;
        cfg.minute = minute;
        return;
    }
    if (slot == 1)
    {
        cfg.hour2 = hour;
        cfg.minute2 = minute;
        return;
    }
    cfg.hour3 = hour;
    cfg.minute3 = minute;
}

void WateringController::setSlotDuration_(uint8_t slot, WateringController::RuleConfig &cfg, uint32_t duration_sec){
    if (slot == 0)
    {
        cfg.duration_sec = duration_sec;
        return;
    }
    if (slot == 1)
    {
        cfg.duration2_sec = duration_sec;
        return;
    }
    cfg.duration3_sec = duration_sec;
}

void WateringController::setSlotEnabled_(uint8_t slot, WateringController::RuleConfig &cfg, bool enabled){
    if (slot == 0)
    {
        cfg.slot1_enabled = enabled;
        return;
    }
    if (slot == 1)
    {
        cfg.slot2_enabled = enabled;
        return;
    }
    cfg.slot3_enabled = enabled;
}

bool WateringController::slotEnabled_(const WateringController::RuleConfig &cfg, uint8_t slot){
    if (slot == 0)
        return cfg.slot1_enabled;
    if (slot == 1)
        return cfg.slot2_enabled;
    return cfg.slot3_enabled;
}

bool WateringController::slotConfigured_(uint8_t hour, uint8_t minute, uint32_t duration_sec){
    return duration_sec > 0 && hour <= 23 && minute <= 59;
}

bool WateringController::slotConfigured_(const WateringController::RuleConfig &cfg, uint8_t slot){
    if (slot == 0)
        return slotConfigured_(cfg.hour, cfg.minute, cfg.duration_sec);
    if (slot == 1)
        return slotConfigured_(cfg.hour2, cfg.minute2, cfg.duration2_sec);
    return slotConfigured_(cfg.hour3, cfg.minute3, cfg.duration3_sec);
}

uint8_t WateringController::calcDow_(uint16_t y, uint8_t m, uint8_t d){
    static const uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y -= 1;
    const uint8_t dow = (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
    return (uint8_t)(dow + 1);
}

bool WateringController::isWeekdayAllowed_(const WateringController::RuleConfig &cfg, uint8_t day_of_week){
    if (day_of_week < 1 || day_of_week > 7)
        return false;
    const uint8_t bit = (uint8_t)(1u << (day_of_week - 1u));
    return (cfg.weekdays_mask & bit) != 0;
}

bool WateringController::timeAfterOrEqual_(uint32_t now, uint32_t target){
    return (uint32_t)(now - target) < 0x80000000u;
}

void WateringController::stopIfActive_(const WateringController::RuleConfig &cfg, WateringController::RuleState &st, WateringController::Event reason, bool notify){
    if (!st.active)
        return;
    st.active = false;
    st.end_ms = 0;
    st.remaining_ms = 0;
    writePort_(cfg.port, false);
    _logs.info(F("WATER"), F("stop: rule: %u port: %u tank: %u"),
               (unsigned)cfg.id, (unsigned)cfg.port, (unsigned)cfg.tank_id);
    if (notify)
        notifyEvent_(reason, cfg, st);
    _runtime_dirty = true;
}

void WateringController::stopForEmpty_(const WateringController::RuleConfig &cfg, WateringController::RuleState &st, bool notify){
    if (!st.active)
        return;
    Event ev = Event::StopEmpty;
    const uint32_t now = millis();
    if (cfg.resume_after_refill && timeAfterOrEqual_(st.end_ms, now))
    {
        st.remaining_ms = st.end_ms - now;
        st.paused = true;
        _logs.warn(F("WATER"),
                   F("pause: empty tank: rule: %u tank: %u remaining_ms: %lu resume_level: %s"),
                   (unsigned)cfg.id, (unsigned)cfg.tank_id, (unsigned long)st.remaining_ms,
                   cfg.resume_level == 2 ? "full" : (cfg.resume_level == 1 ? "mid" : "low"));
        ev = Event::PauseEmpty;
    }
    else
    {
        st.remaining_ms = 0;
        st.paused = false;
        _logs.warn(F("WATER"), F("stop: empty tank: rule: %u tank: %u"),
                   (unsigned)cfg.id, (unsigned)cfg.tank_id);
    }
    st.active = false;
    st.end_ms = 0;
    writePort_(cfg.port, false);
    if (notify)
        notifyEvent_(ev, cfg, st);
    _runtime_dirty = true;
}

void WateringController::resumeAfterRefill_(const WateringController::RuleConfig &cfg, WateringController::RuleState &st, bool notify){
    if (!st.paused || st.remaining_ms == 0)
        return;
    st.paused = false;
    st.active = true;
    st.end_ms = millis() + st.remaining_ms;
    st.remaining_ms = 0;
    writePort_(cfg.port, true);
    _logs.info(F("WATER"), F("resume: rule: %u port: %u tank: %u"),
               (unsigned)cfg.id, (unsigned)cfg.port, (unsigned)cfg.tank_id);
    if (notify)
        notifyEvent_(Event::Resume, cfg, st);
    _runtime_dirty = true;
}

void WateringController::writePort_(uint8_t port, bool on){
    if (port == kInvalidPort)
        return;
    _gpio.writeDyn(port, on);
}

bool WateringController::isTankEmpty_(const WateringController::RuleConfig &cfg) const{
    if (cfg.tank_id == 0)
        return false;
    auto tanks_guard = _tanks.lockGuard();
    const TankController::TankState *st = _tanks.state(cfg.tank_id);
    if (!st)
        return true;
    const bool empty = !(st->level_low || st->level_mid || st->level_full);
    return !st->levels_ok || empty;
}

bool WateringController::isResumeLevelReached_(const WateringController::RuleConfig &cfg) const{
    if (cfg.tank_id == 0)
        return true;
    auto tanks_guard = _tanks.lockGuard();
    const TankController::TankState *st = _tanks.state(cfg.tank_id);
    if (!st || !st->levels_ok)
        return false;
    if (cfg.resume_level == 2)
        return st->level_full;
    if (cfg.resume_level == 1)
        return st->level_mid || st->level_full;
    return st->level_low || st->level_mid || st->level_full;
}

bool WateringController::indexById_(size_t id, size_t &out) const{
    if (id == 0)
        return false;
    for (size_t i = 0; i < kRuleCount; ++i)
    {
        if (_cfg[i].id == id)
        {
            out = i;
            return true;
        }
    }
    return false;
}

void WateringController::notifyEvent_(WateringController::Event ev, const WateringController::RuleConfig &cfg, const WateringController::RuleState &st){
    if (_event_cb)
        _event_cb(_event_ctx, ev, cfg, st);
    if (_event_cb_secondary)
        _event_cb_secondary(_event_ctx_secondary, ev, cfg, st);
}
