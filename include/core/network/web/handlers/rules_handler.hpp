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
#include "core/network/web/interfaces/web_interface_handler_base.hpp"

#include <string.h>
#include <stdlib.h>

#include "core/rules_controller.hpp"
#include "controllers/socket_controller.hpp"
#include "controllers/meteo_controller.hpp"
#include "controllers/tank_controller.hpp"
#include "controllers/septic_controller.hpp"
#include "controllers/security_controller.hpp"


class RulesHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server);

    static void handleRules(WebInterface &web, AsyncWebServerRequest *request);

    static void handleRulesSave(WebInterface &web, AsyncWebServerRequest *request);

private:
    struct RuleControllerSpec
    {
        const char *name;
        const char *params[8];
        size_t param_count;
    };

    static const RuleControllerSpec *controllerSpec_(const String &controller);

    static bool controllerEnabled_(WebInterface &web, const char *controller);

    static bool remoteConditionControllerEnabled_(WebInterface &web, uint32_t node_id, const char *controller);

    static bool remoteActionControllerEnabled_(WebInterface &web, uint32_t node_id, const char *controller);

    static bool controllerEnabledForNode_(WebInterface &web, uint32_t node_id, const char *controller, bool condition_mode);

    static String nodeLabel_(WebInterface &web, uint32_t node_id);

    static void appendNodeSelectOptions_(WebInterface &web, String &out, uint32_t selected);

    static bool isConditionControllerSupported_(const char *controller);

    static void appendOption_(String &out, const char *value, bool selected);

    static void appendControllerSelectOptions_(WebInterface &web, String &out, uint32_t node_id, const String &selected);

    static void appendConditionControllerSelectOptions_(WebInterface &web, String &out, uint32_t node_id, const String &selected);

    static void appendParameterSelectOptions_(String &out, const String &controller, const String &selected);

    static void appendConditionOperatorOptions_(String &out, const String &selected);

    static void appendConditionParamOptions_(String &out, const String &controller, const String &selected);

    static void appendConditionValueOptions_(String &out, const String &controller, const String &param, const String &selected);

    static void appendConditionItemOptions_(WebInterface &web, String &out, uint32_t node_id, const String &controller, uint8_t selected_id);

    static void appendJsStringLiteral_(String &out, const String &value);

    static void appendSocketValueOptions_(WebInterface &web, String &out, uint32_t node_id);

    static void appendLightValueOptions_(WebInterface &web, String &out, uint32_t node_id);

    static bool saveRules_(WebInterface &web);

    static uint8_t parseUInt8_(const String &s);

    static uint32_t parseUInt32_(const String &s);

    static String rulesGridHtml_(WebInterface &web);

    static String actionsGridHtml_(WebInterface &web, uint8_t rule_id, uint32_t preview_cond_node, const String &preview_cond_controller);

    static String actionEditHtml_(WebInterface &web, uint8_t rule_id, uint8_t action_id, uint32_t preview_action_node);
};
