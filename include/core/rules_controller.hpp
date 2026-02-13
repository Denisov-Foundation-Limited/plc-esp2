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
#include <array>
#include <new>
#include <stdlib.h>

#if defined(ESP32)
#include "esp32-hal-psram.h"
#include "esp_heap_caps.h"
#endif

class RulesController
{
public:
    static constexpr size_t kRuleCount = 30;
    static constexpr size_t kActionCount = 30;

    enum class ActionKind : uint8_t
    {
        Controller = 0,
        Pause = 1,
        Telegram = 2
    };

    struct RuleAction
    {
        uint8_t id = 1;
        bool enabled = false;
        ActionKind kind = ActionKind::Controller;
        uint32_t delay_ms = 0;
        uint32_t node_id = 0;
        String controller;
        String parameter;
        String value;
    };

    struct Rule
    {
        uint8_t id = 1;
        bool enabled = false;
        String name;
        bool condition_enabled = false;
        uint32_t condition_node_id = 0;
        String condition_controller;
        uint8_t condition_item_id = 0;
        String condition_parameter;
        String condition_op;
        String condition_value;
        std::array<RuleAction, kActionCount> actions = {};
    };

    RulesController()
    {
        initStorage_();
        resetDefaults();
    }

    ~RulesController()
    {
        releaseStorage_();
    }

    RulesController(const RulesController &) = delete;
    RulesController &operator=(const RulesController &) = delete;

    void resetDefaults()
    {
        if (!_rules)
            return;
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            Rule &r = _rules[i];
            r = Rule{};
            r.id = (uint8_t)(i + 1);
            r.enabled = false;
            r.name = String("Rule ") + String((unsigned)(i + 1));
            for (size_t a = 0; a < kActionCount; ++a)
            {
                RuleAction &act = r.actions[a];
                act = RuleAction{};
                act.id = (uint8_t)(a + 1);
            }
        }
        _rules[0].enabled = true;
        _rules[0].name = "Я дома";
        _rules[0].actions[0].enabled = true;
        _rules[0].actions[0].kind = ActionKind::Controller;
        _rules[0].actions[0].controller = "security";
        _rules[0].actions[0].parameter = "disarm";
        _rules[1].enabled = true;
        _rules[1].name = "Собираюсь";
        _rules[2].enabled = true;
        _rules[2].name = "Ушёл";
        _rules[2].actions[0].enabled = true;
        _rules[2].actions[0].kind = ActionKind::Controller;
        _rules[2].actions[0].controller = "security";
        _rules[2].actions[0].parameter = "arm";
        _dirty = true;
    }

    bool takeDirty()
    {
        if (!_dirty)
            return false;
        _dirty = false;
        return true;
    }

    Rule *rule(size_t id)
    {
        if (!_rules || id == 0 || id > kRuleCount)
            return nullptr;
        return &_rules[id - 1];
    }

    const Rule *rule(size_t id) const
    {
        if (!_rules || id == 0 || id > kRuleCount)
            return nullptr;
        return &_rules[id - 1];
    }

    const Rule *ruleByIndex(size_t idx) const
    {
        if (!_rules || idx >= kRuleCount)
            return nullptr;
        return &_rules[idx];
    }

    RuleAction *action(size_t rule_id, size_t action_id)
    {
        Rule *r = rule(rule_id);
        if (!r || action_id == 0 || action_id > kActionCount)
            return nullptr;
        return &r->actions[action_id - 1];
    }

    const RuleAction *action(size_t rule_id, size_t action_id) const
    {
        const Rule *r = rule(rule_id);
        if (!r || action_id == 0 || action_id > kActionCount)
            return nullptr;
        return &r->actions[action_id - 1];
    }

    void applyConfig(JsonArrayConst arr)
    {
        if (!_rules)
            return;
        resetDefaults();
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            Rule &r = _rules[i];
            r.enabled = false;
            r.condition_enabled = false;
            r.condition_node_id = 0;
            r.condition_controller = "";
            r.condition_item_id = 0;
            r.condition_parameter = "";
            r.condition_op = "";
            r.condition_value = "";
            for (size_t a = 0; a < kActionCount; ++a)
            {
                RuleAction &act = r.actions[a];
                act.enabled = false;
                act.kind = ActionKind::Controller;
                act.delay_ms = 0;
                act.node_id = 0;
                act.controller = "";
                act.parameter = "";
                act.value = "";
            }
        }
        for (JsonVariantConst v : arr)
        {
            if (!v.is<JsonObjectConst>())
                continue;
            JsonObjectConst obj = v.as<JsonObjectConst>();
            const uint8_t id = (uint8_t)(obj["id"] | 0u);
            Rule *r = rule(id);
            if (!r)
                continue;
            if (obj["enabled"].is<bool>())
                r->enabled = obj["enabled"].as<bool>();
            if (obj["name"].is<const char *>())
                r->name = obj["name"].as<const char *>();
            if (obj["condition"].is<JsonObjectConst>())
            {
                JsonObjectConst cobj = obj["condition"].as<JsonObjectConst>();
                if (cobj["enabled"].is<bool>())
                    r->condition_enabled = cobj["enabled"].as<bool>();
                if (cobj["node_id"].is<unsigned>())
                    r->condition_node_id = (uint32_t)cobj["node_id"].as<unsigned>();
                if (cobj["controller"].is<const char *>())
                    r->condition_controller = cobj["controller"].as<const char *>();
                if (cobj["item_id"].is<unsigned>())
                    r->condition_item_id = (uint8_t)cobj["item_id"].as<unsigned>();
                if (cobj["parameter"].is<const char *>())
                    r->condition_parameter = cobj["parameter"].as<const char *>();
                if (cobj["op"].is<const char *>())
                    r->condition_op = cobj["op"].as<const char *>();
                if (cobj["value"].is<const char *>())
                    r->condition_value = cobj["value"].as<const char *>();
                else if (cobj["value"].is<int>())
                    r->condition_value = String(cobj["value"].as<int>());
                else if (cobj["value"].is<float>())
                    r->condition_value = String(cobj["value"].as<float>());
            }
            if (!obj["actions"].is<JsonArrayConst>())
                continue;
            for (JsonVariantConst av : obj["actions"].as<JsonArrayConst>())
            {
                if (!av.is<JsonObjectConst>())
                    continue;
                JsonObjectConst aobj = av.as<JsonObjectConst>();
                const uint8_t aid = (uint8_t)(aobj["id"] | 0u);
                RuleAction *act = action(id, aid);
                if (!act)
                    continue;
                if (aobj["enabled"].is<bool>())
                    act->enabled = aobj["enabled"].as<bool>();
                if (aobj["kind"].is<const char *>())
                {
                    String k = aobj["kind"].as<const char *>();
                    k.toLowerCase();
                    if (k == "pause" || k == "delay")
                        act->kind = ActionKind::Pause;
                    else if (k == "telegram" || k == "tg")
                        act->kind = ActionKind::Telegram;
                    else
                        act->kind = ActionKind::Controller;
                }
                if (aobj["delay_ms"].is<unsigned>())
                    act->delay_ms = (uint32_t)aobj["delay_ms"].as<unsigned>();
                if (aobj["node_id"].is<unsigned>())
                    act->node_id = (uint32_t)aobj["node_id"].as<unsigned>();
                if (aobj["controller"].is<const char *>())
                    act->controller = aobj["controller"].as<const char *>();
                if (aobj["parameter"].is<const char *>())
                    act->parameter = aobj["parameter"].as<const char *>();
                if (aobj["value"].is<const char *>())
                    act->value = aobj["value"].as<const char *>();
                else if (aobj["value"].is<int>())
                    act->value = String(aobj["value"].as<int>());
                else if (aobj["value"].is<float>())
                    act->value = String(aobj["value"].as<float>());
            }
        }
        _dirty = true;
    }

    void serialize(JsonArray out) const
    {
        if (!_rules)
            return;
        for (size_t i = 0; i < kRuleCount; ++i)
        {
            const Rule &r = _rules[i];
            if (!r.enabled)
                continue;
            JsonObject ro = out.add<JsonObject>();
            ro["id"] = (unsigned)r.id;
            ro["enabled"] = r.enabled;
            if (r.name.length())
                ro["name"] = r.name;
            if (r.condition_enabled || r.condition_node_id || r.condition_controller.length() ||
                r.condition_parameter.length() || r.condition_op.length() || r.condition_value.length() ||
                r.condition_item_id)
            {
                JsonObject co = ro["condition"].to<JsonObject>();
                co["enabled"] = r.condition_enabled;
                if (r.condition_node_id)
                    co["node_id"] = r.condition_node_id;
                if (r.condition_controller.length())
                    co["controller"] = r.condition_controller;
                if (r.condition_item_id)
                    co["item_id"] = (unsigned)r.condition_item_id;
                if (r.condition_parameter.length())
                    co["parameter"] = r.condition_parameter;
                if (r.condition_op.length())
                    co["op"] = r.condition_op;
                if (r.condition_value.length())
                    co["value"] = r.condition_value;
            }
            JsonArray aout = ro["actions"].to<JsonArray>();
            for (size_t j = 0; j < kActionCount; ++j)
            {
                const RuleAction &a = r.actions[j];
                const bool has_payload = a.delay_ms || a.node_id || a.controller.length() ||
                                         a.parameter.length() || a.value.length();
                if (!a.enabled && !has_payload)
                    continue;
                JsonObject ao = aout.add<JsonObject>();
                ao["id"] = (unsigned)a.id;
                ao["enabled"] = a.enabled;
                if (a.kind == ActionKind::Pause)
                    ao["kind"] = "pause";
                else if (a.kind == ActionKind::Telegram)
                    ao["kind"] = "telegram";
                else
                    ao["kind"] = "controller";
                if (a.delay_ms)
                    ao["delay_ms"] = (unsigned)a.delay_ms;
                if (a.node_id)
                    ao["node_id"] = a.node_id;
                if (a.controller.length())
                    ao["controller"] = a.controller;
                if (a.parameter.length())
                    ao["parameter"] = a.parameter;
                if (a.value.length())
                    ao["value"] = a.value;
            }
        }
    }

    bool triggerRule(uint8_t id)
    {
        Rule *r = rule(id);
        if (!r || !r->enabled)
            return false;
        _last_triggered_rule_id = id;
        _last_triggered_ms = millis();
        return true;
    }

    uint8_t lastTriggeredRuleId() const { return _last_triggered_rule_id; }
    uint32_t lastTriggeredMs() const { return _last_triggered_ms; }

private:
    void initStorage_()
    {
        const size_t bytes = sizeof(Rule) * kRuleCount;
        void *mem = nullptr;
#if defined(ESP32)
        if (psramFound())
            mem = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
        if (!mem)
            mem = malloc(bytes);
        if (!mem)
            return;
        _rules = static_cast<Rule *>(mem);
        for (size_t i = 0; i < kRuleCount; ++i)
            new (&_rules[i]) Rule();
    }

    void releaseStorage_()
    {
        if (!_rules)
            return;
        for (size_t i = 0; i < kRuleCount; ++i)
            _rules[i].~Rule();
        free(_rules);
        _rules = nullptr;
    }

    Rule *_rules = nullptr;
    bool _dirty = false;
    uint8_t _last_triggered_rule_id = 0;
    uint32_t _last_triggered_ms = 0;
};
