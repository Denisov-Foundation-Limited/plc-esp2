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

class RulesController
{
public:
    static constexpr size_t kRuleCount = 30;
    static constexpr size_t kActionCount = 30;

    enum class ActionKind : uint8_t
    {
        Controller = 0,
        Pause = 1,
        Notify = 2
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

    using TriggerHandler = void (*)(void *ctx, const Rule &rule);

    RulesController();

    ~RulesController();

    RulesController(const RulesController &) = delete;
    RulesController &operator=(const RulesController &) = delete;

    void resetDefaults();

    bool takeDirty();

    Rule *rule(size_t id);

    const Rule *rule(size_t id) const;

    const Rule *ruleByIndex(size_t idx) const;

    RuleAction *action(size_t rule_id, size_t action_id);

    const RuleAction *action(size_t rule_id, size_t action_id) const;

    void applyConfig(JsonArrayConst arr);

    void serialize(JsonArray out) const;

    void setTriggerHandler(TriggerHandler cb, void *ctx);

    bool triggerRule(uint8_t id);

    uint8_t lastTriggeredRuleId() const;
    uint32_t lastTriggeredMs() const;

private:
    void initStorage_();

    void releaseStorage_();

    Rule *_rules = nullptr;
    bool _dirty = false;
    uint8_t _last_triggered_rule_id = 0;
    uint32_t _last_triggered_ms = 0;
    TriggerHandler _trigger_handler = nullptr;
    void *_trigger_ctx = nullptr;
};
