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

#include "core/network/web/handlers/rules_handler.hpp"

#include "core/network/stack/stack_device_registry.hpp"
#include "core/network/web/web_interface.hpp"

void RulesHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/rules", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleRules(web, request); });
        server.on("/rules", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleRulesSave(web, request); });
    }

void RulesHandler::handleRules(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;

        String page = FPSTR(kWebInterfaceRulesHtml);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%RULES_PAGE_TITLE%", WebUiRu::Rules::kRules);
        page.replace("%RULES_PAGE_H1%", WebUiRu::Rules::kRules);
        if (!web._rules_status.length())
            web._rules_status = WebUiRu::Rules::kReady;
        page.replace("%RULES_STATUS%", web._rules_status);

        if (!web._rules)
        {
            page.replace("%RULES_BODY%", "<p>Rules controller unavailable</p>");
            web.sendHtml_(request, page, set_cookie);
            return;
        }

        const uint8_t rule_id = parseUInt8_(request->hasParam("rule") ? request->getParam("rule")->value() : "");
        const uint8_t action_id = parseUInt8_(request->hasParam("action") ? request->getParam("action")->value() : "");
        const uint32_t cond_node = parseUInt32_(request->hasParam("cond_node") ? request->getParam("cond_node")->value() : "");
        const uint32_t action_node = parseUInt32_(request->hasParam("action_node") ? request->getParam("action_node")->value() : "");
        String cond_controller = request->hasParam("cond_controller") ? request->getParam("cond_controller")->value() : "";
        String body;
        if (rule_id == 0)
            body = rulesGridHtml_(web);
        else if (action_id == 0)
            body = actionsGridHtml_(web, rule_id, cond_node, cond_controller);
        else
            body = actionEditHtml_(web, rule_id, action_id, action_node);

        page.replace("%RULES_BODY%", body);
        web.sendHtml_(request, page, set_cookie);
    }

void RulesHandler::handleRulesSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web._rules)
        {
            web._rules_status = "Rules controller unavailable";
            web.sendRedirect_(request, "/rules", set_cookie);
            return;
        }

        const uint8_t rule_id = parseUInt8_(web.paramValue_(request, "rule_id"));
        const uint8_t action_id = parseUInt8_(web.paramValue_(request, "action_id"));
        if (rule_id == 0)
        {
            web._rules_status = WebUiRu::Rules::kInvalidRuleId;
            web.sendRedirect_(request, "/rules", set_cookie);
            return;
        }

        RulesController::Rule *r = web._rules->rule(rule_id);
        if (!r)
        {
            web._rules_status = WebUiRu::Rules::kRuleNotFound;
            web.sendRedirect_(request, "/rules", set_cookie);
            return;
        }

        const bool quick_toggle = request->hasParam("quick_toggle", true);
        if (quick_toggle)
        {
            String back = web.paramValue_(request, "return_to");
            if (!back.length() || !back.startsWith("/rules"))
            {
                back = String("/rules?rule=") + String((unsigned)rule_id);
                if (action_id)
                    back += String("&action=") + String((unsigned)action_id);
            }
            if (action_id == 0)
            {
                r->enabled = request->hasParam("enabled", true);
                web._rules_status = saveRules_(web) ? WebUiRu::Common::kSaved : WebUiRu::Common::kSaveFailed;
                web.sendRedirect_(request, back, set_cookie);
                return;
            }

            RulesController::RuleAction *qa = web._rules->action(rule_id, action_id);
            if (!qa)
            {
                web._rules_status = WebUiRu::Rules::kActionNotFound;
                web.sendRedirect_(request, String("/rules?rule=") + String((unsigned)rule_id), set_cookie);
                return;
            }
            qa->enabled = request->hasParam("enabled", true);
            web._rules_status = saveRules_(web) ? WebUiRu::Common::kSaved : WebUiRu::Common::kSaveFailed;
            web.sendRedirect_(request, back, set_cookie);
            return;
        }

        if (action_id == 0)
        {
            r->enabled = request->hasParam("enabled", true);
            if (request->hasParam("name", true))
                r->name = web.paramValue_(request, "name");
            if (!r->name.length())
                r->name = String("Rule ") + String((unsigned)rule_id);
            r->condition_enabled = request->hasParam("cond_enabled", true);
            r->condition_node_id = parseUInt32_(web.paramValue_(request, "cond_node_id"));
            r->condition_controller = web.paramValue_(request, "cond_controller");
            r->condition_item_id = parseUInt8_(web.paramValue_(request, "cond_item_id"));
            r->condition_parameter = web.paramValue_(request, "cond_param");
            r->condition_op = web.paramValue_(request, "cond_op");
            r->condition_value = web.paramValue_(request, "cond_value");
            web._rules_status = saveRules_(web) ? WebUiRu::Common::kSaved : WebUiRu::Common::kSaveFailed;
            web.sendRedirect_(request, String("/rules?rule=") + String((unsigned)rule_id), set_cookie);
            return;
        }

        RulesController::RuleAction *a = web._rules->action(rule_id, action_id);
        if (!a)
        {
            web._rules_status = WebUiRu::Rules::kActionNotFound;
            web.sendRedirect_(request, String("/rules?rule=") + String((unsigned)rule_id), set_cookie);
            return;
        }
        a->enabled = request->hasParam("enabled", true);
        String kind = web.paramValue_(request, "kind");
        kind.toLowerCase();
        if (kind == "pause" || kind == "delay")
            a->kind = RulesController::ActionKind::Pause;
        else if (kind == "notify" || kind == "telegram" || kind == "tg")
            a->kind = RulesController::ActionKind::Notify;
        else
            a->kind = RulesController::ActionKind::Controller;
        a->delay_ms = (uint32_t)web.paramValue_(request, "delay_ms").toInt();
        a->node_id = parseUInt32_(web.paramValue_(request, "node_id"));
        a->controller = web.paramValue_(request, "controller");
        a->parameter = web.paramValue_(request, "parameter");
        a->value = web.paramValue_(request, "value");

        web._rules_status = saveRules_(web) ? WebUiRu::Common::kSaved : WebUiRu::Common::kSaveFailed;
        String back = String("/rules?rule=") + String((unsigned)rule_id) + "&action=" + String((unsigned)action_id);
        web.sendRedirect_(request, back, set_cookie);
    }

const RulesHandler::RuleControllerSpec * RulesHandler::controllerSpec_(const String &controller) {
        static const RuleControllerSpec kSpecs[] = {
            {"sockets", {"set", "toggle"}, 2},
            {"lights", {"set", "toggle"}, 2},
            {"meteo", {"read"}, 1},
            {"thermo", {"power", "mode", "target"}, 3},
            {"tanks", {"power"}, 1},
            {"septic", {"monitor"}, 1},
            {"security", {"arm", "disarm", "clear", "rfid", "ibutton"}, 5},
            {"watering", {"status"}, 1},
            {"ring", {"hold"}, 1},
            {"avr", {"auto", "source", "clear_fault"}, 3},
            {"leak", {"power", "ack", "ack_all"}, 3},
        };
        for (size_t i = 0; i < (sizeof(kSpecs) / sizeof(kSpecs[0])); ++i)
        {
            if (controller.equalsIgnoreCase(kSpecs[i].name))
                return &kSpecs[i];
        }
        return nullptr;
    }

bool RulesHandler::controllerEnabled_(WebInterface &web, const char *controller) {
        if (!web._controllers || !controller)
            return false;
        if (strcmp(controller, "sockets") == 0)
            return web._controllers->sockets().controllerEnabled();
        if (strcmp(controller, "lights") == 0)
            return web._controllers->sockets().lightsEnabled();
        if (strcmp(controller, "meteo") == 0)
            return web._controllers->meteo().controllerEnabled();
        if (strcmp(controller, "thermo") == 0)
            return web._controllers->thermo().controllerEnabled();
        if (strcmp(controller, "tanks") == 0)
            return web._controllers->tanks().controllerEnabled();
        if (strcmp(controller, "septic") == 0)
            return web._controllers->septic().controllerEnabled();
        if (strcmp(controller, "security") == 0)
            return web._controllers->security().controllerEnabled();
        if (strcmp(controller, "watering") == 0)
            return web._controllers->watering().controllerEnabled();
        if (strcmp(controller, "ring") == 0)
            return web._controllers->ring().controllerEnabled();
        if (strcmp(controller, "avr") == 0)
            return web._controllers->avr().controllerEnabled();
        if (strcmp(controller, "leak") == 0)
            return web._controllers->leak().controllerEnabled();
        return false;
    }

bool RulesHandler::remoteConditionControllerEnabled_(WebInterface &web, uint32_t node_id, const char *controller) {
        if (node_id == 0 || !controller)
            return false;
        if (strcmp(controller, "sockets") == 0)
        {
            StackUnitSnapshot::Snapshot snapshot{};
            if (!web.network() || !web.network()->stackIndexStateSnapshot(node_id, snapshot) || snapshot.updated_ms == 0)
            {
                web.requestStackSockets_(node_id);
                return false;
            }
            if (snapshot.sockets_enabled > snapshot.socket_count)
                web.requestStackSockets_(node_id);
            for (uint8_t i = 0; i < snapshot.socket_count && i < StackUnitSnapshot::kSocketCount; ++i)
                if (snapshot.sockets[i].enabled)
                    return true;
            return false;
        }
        if (strcmp(controller, "lights") == 0)
        {
            StackUnitSnapshot::Snapshot snapshot{};
            if (!web.network() || !web.network()->stackIndexStateSnapshot(node_id, snapshot) || snapshot.updated_ms == 0)
            {
                web.requestStackLights_(node_id);
                return false;
            }
            if (snapshot.lights_enabled > snapshot.light_count)
                web.requestStackLights_(node_id);
            for (uint8_t i = 0; i < snapshot.light_count && i < StackUnitSnapshot::kSocketCount; ++i)
                if (snapshot.lights[i].enabled)
                    return true;
            return false;
        }
        return false;
    }

bool RulesHandler::remoteActionControllerEnabled_(WebInterface &web, uint32_t node_id, const char *controller) {
        if (!controller)
            return false;
        if (strcmp(controller, "security") == 0)
            return remoteConditionControllerEnabled_(web, node_id, controller);
        if (strcmp(controller, "sockets") == 0 || strcmp(controller, "lights") == 0)
            return remoteConditionControllerEnabled_(web, node_id, controller);
        return false;
    }

bool RulesHandler::controllerEnabledForNode_(WebInterface &web, uint32_t node_id, const char *controller, bool condition_mode) {
        if (node_id == 0)
            return controllerEnabled_(web, controller);
        return condition_mode ? remoteConditionControllerEnabled_(web, node_id, controller)
                              : remoteActionControllerEnabled_(web, node_id, controller);
    }

String RulesHandler::nodeLabel_(WebInterface &web, uint32_t node_id) {
        if (!node_id)
            return "local";
        if (web.network())
        {
            StackDeviceRegistry::DeviceInfo device{};
            if (web.network()->stackDeviceSnapshotByNodeId(node_id, device))
            {
                String name = device.name;
                name.trim();
                if (name.length())
                    return name;
            }
        }
        String fallback = "node ";
        fallback += String((unsigned long)node_id);
        return fallback;
    }

void RulesHandler::appendNodeSelectOptions_(WebInterface &web, String &out, uint32_t selected) {
        out += "<option value=\"0\"";
        if (selected == 0)
            out += " selected";
        out += ">local</option>";

        bool selected_added = (selected == 0);
        if (web.network())
        {
            const size_t n = web.network()->stackOnlineDeviceCount();
            for (size_t i = 0; i < n; ++i)
            {
                StackDeviceRegistry::DeviceInfo device{};
                if (!web.network()->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
                    continue;
                if ((device.caps & kStackCapController) == 0)
                    continue;
                const uint32_t node_id = device.node_id;
                if (!node_id)
                    continue;
                out += "<option value=\"";
                out += String((unsigned long)node_id);
                out += "\"";
                if (node_id == selected)
                {
                    out += " selected";
                    selected_added = true;
                }
                out += ">";
                const String label = nodeLabel_(web, node_id);
                WebInterface::appendHtmlEscaped_(out, label.c_str());
                out += "</option>";
            }
        }
        if (!selected_added && selected != 0)
        {
            out += "<option value=\"";
            out += String((unsigned long)selected);
            out += "\" selected>";
            const String label = nodeLabel_(web, selected);
            WebInterface::appendHtmlEscaped_(out, label.c_str());
            out += "</option>";
        }
    }

bool RulesHandler::isConditionControllerSupported_(const char *controller) {
        if (!controller)
            return false;
        return strcmp(controller, "sockets") == 0 ||
               strcmp(controller, "lights") == 0 ||
               strcmp(controller, "meteo") == 0 ||
               strcmp(controller, "tanks") == 0 ||
               strcmp(controller, "septic") == 0 ||
               strcmp(controller, "security") == 0;
    }

void RulesHandler::appendOption_(String &out, const char *value, bool selected) {
        out += "<option value=\"";
        out += value;
        out += "\"";
        if (selected)
            out += " selected";
        out += ">";
        out += value;
        out += "</option>";
    }

void RulesHandler::appendControllerSelectOptions_(WebInterface &web, String &out, uint32_t node_id, const String &selected) {
        static const char *kControllersLocal[] = {
            "sockets", "lights", "meteo", "thermo", "tanks", "septic",
            "security", "watering", "ring", "avr", "leak"};
        static const char *kControllersRemote[] = {"sockets", "lights", "security"};
        const char *const *controllers = node_id == 0 ? kControllersLocal : kControllersRemote;
        const size_t controllers_count = node_id == 0 ? (sizeof(kControllersLocal) / sizeof(kControllersLocal[0]))
                                                      : (sizeof(kControllersRemote) / sizeof(kControllersRemote[0]));
        bool any_enabled = false;
        bool selected_added = false;
        for (size_t i = 0; i < controllers_count; ++i)
        {
            const char *ctrl = controllers[i];
            if (!controllerEnabledForNode_(web, node_id, ctrl, false))
                continue;
            const bool is_selected = selected.equalsIgnoreCase(ctrl);
            appendOption_(out, ctrl, is_selected);
            any_enabled = true;
            if (is_selected)
                selected_added = true;
        }
        if (!selected_added && selected.length())
        {
            out += "<option value=\"";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "\" selected>";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "</option>";
            any_enabled = true;
        }
        if (!any_enabled)
        {
            if (node_id != 0)
            {
                for (size_t i = 0; i < controllers_count; ++i)
                    appendOption_(out, controllers[i], selected.equalsIgnoreCase(controllers[i]));
            }
            else
            {
                out += "<option value=\"\" selected>no_enabled_controllers</option>";
            }
        }
    }

void RulesHandler::appendConditionControllerSelectOptions_(WebInterface &web, String &out, uint32_t node_id, const String &selected) {
        static const char *kControllers[] = {"sockets", "lights", "meteo", "tanks", "septic", "security"};
        bool any_enabled = false;
        bool selected_added = false;
        for (size_t i = 0; i < (sizeof(kControllers) / sizeof(kControllers[0])); ++i)
        {
            const char *ctrl = kControllers[i];
            if (!controllerEnabledForNode_(web, node_id, ctrl, true))
                continue;
            const bool is_selected = selected.equalsIgnoreCase(ctrl);
            appendOption_(out, ctrl, is_selected);
            any_enabled = true;
            if (is_selected)
                selected_added = true;
        }
        if (!selected_added && selected.length() && isConditionControllerSupported_(selected.c_str()))
        {
            out += "<option value=\"";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "\" selected>";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "</option>";
            any_enabled = true;
        }
        if (!any_enabled)
        {
            if (node_id != 0)
            {
                for (size_t i = 0; i < (sizeof(kControllers) / sizeof(kControllers[0])); ++i)
                    appendOption_(out, kControllers[i], selected.equalsIgnoreCase(kControllers[i]));
            }
            else
            {
                out += "<option value=\"\" selected>-</option>";
            }
        }
    }

void RulesHandler::appendParameterSelectOptions_(String &out, const String &controller, const String &selected) {
        const RuleControllerSpec *spec = controllerSpec_(controller);
        bool selected_added = false;
        if (spec)
        {
            for (size_t i = 0; i < spec->param_count; ++i)
            {
                const char *param = spec->params[i];
                const bool is_selected = selected.equalsIgnoreCase(param);
                appendOption_(out, param, is_selected);
                if (is_selected)
                    selected_added = true;
            }
        }
        if (!selected_added && selected.length())
        {
            out += "<option value=\"";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "\" selected>";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "</option>";
        }
        if (!spec && !selected.length())
            out += "<option value=\"\" selected>-</option>";
    }

void RulesHandler::appendConditionOperatorOptions_(String &out, const String &selected) {
        static const char *kOps[] = {"", "==", "!=", ">", "<", ">=", "<="};
        String sel = selected;
        sel.trim();
        sel.toLowerCase();
        if (sel == "eq")
            sel = "==";
        else if (sel == "ne")
            sel = "!=";
        else if (sel == "gt")
            sel = ">";
        else if (sel == "lt")
            sel = "<";
        else if (sel == "ge")
            sel = ">=";
        else if (sel == "le")
            sel = "<=";
        bool selected_added = false;
        for (size_t i = 0; i < (sizeof(kOps) / sizeof(kOps[0])); ++i)
        {
            const String v = kOps[i];
            out += "<option value=\"";
            out += v;
            out += "\"";
            if (v == sel)
            {
                out += " selected";
                selected_added = true;
            }
            out += ">";
            out += v.length() ? v : "-";
            out += "</option>";
        }
        if (!selected_added && selected.length())
        {
            out += "<option value=\"";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "\" selected>";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "</option>";
        }
    }

void RulesHandler::appendConditionParamOptions_(String &out, const String &controller, const String &selected) {
        const char *params[8] = {};
        size_t count = 0;
        if (controller.equalsIgnoreCase("sockets") || controller.equalsIgnoreCase("lights"))
        {
            params[0] = "relay_on";
            count = 1;
        }
        else if (controller.equalsIgnoreCase("meteo"))
        {
            params[0] = "ok";
            params[1] = "temp_c";
            params[2] = "humidity";
            count = 3;
        }
        else if (controller.equalsIgnoreCase("security"))
        {
            params[0] = "detect";
            params[1] = "armed";
            params[2] = "alarm_on";
            count = 3;
        }
        else if (controller.equalsIgnoreCase("tanks"))
        {
            params[0] = "level";
            params[1] = "empty";
            params[2] = "valve_on";
            params[3] = "pump_on";
            params[4] = "alarm_on";
            count = 5;
        }
        else if (controller.equalsIgnoreCase("septic"))
        {
            params[0] = "status";
            params[1] = "warning";
            params[2] = "alarm";
            count = 3;
        }
        bool selected_added = false;
        for (size_t i = 0; i < count; ++i)
        {
            const bool is_selected = selected.equalsIgnoreCase(params[i]);
            appendOption_(out, params[i], is_selected);
            if (is_selected)
                selected_added = true;
        }
        if (!selected_added && selected.length())
        {
            out += "<option value=\"";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "\" selected>";
            WebInterface::appendHtmlEscaped_(out, selected.c_str());
            out += "</option>";
            selected_added = true;
        }
        if (!selected_added)
            out += "<option value=\"\" selected>-</option>";
    }

void RulesHandler::appendConditionValueOptions_(String &out, const String &controller, const String &param, const String &selected) {
        auto appendBool = [&](const String &sel) {
            appendOption_(out, "off", sel.equalsIgnoreCase("off") || sel == "0" || sel.equalsIgnoreCase("false"));
            appendOption_(out, "on", sel.equalsIgnoreCase("on") || sel == "1" || sel.equalsIgnoreCase("true"));
        };

        if (controller.equalsIgnoreCase("sockets") || controller.equalsIgnoreCase("lights") ||
            (controller.equalsIgnoreCase("meteo") && param.equalsIgnoreCase("ok")) ||
            (controller.equalsIgnoreCase("security") && (param.equalsIgnoreCase("detect") || param.equalsIgnoreCase("armed") || param.equalsIgnoreCase("alarm_on"))) ||
            (controller.equalsIgnoreCase("tanks") && (param.equalsIgnoreCase("empty") || param.equalsIgnoreCase("valve_on") || param.equalsIgnoreCase("pump_on") || param.equalsIgnoreCase("alarm_on"))) ||
            (controller.equalsIgnoreCase("septic") && (param.equalsIgnoreCase("warning") || param.equalsIgnoreCase("alarm"))))
        {
            appendBool(selected);
            return;
        }

        if (controller.equalsIgnoreCase("tanks") && param.equalsIgnoreCase("level"))
        {
            appendOption_(out, "empty", selected.equalsIgnoreCase("empty"));
            appendOption_(out, "low", selected.equalsIgnoreCase("low"));
            appendOption_(out, "mid", selected.equalsIgnoreCase("mid"));
            appendOption_(out, "full", selected.equalsIgnoreCase("full"));
            return;
        }

        if (controller.equalsIgnoreCase("septic") && param.equalsIgnoreCase("status"))
        {
            appendOption_(out, "ok", selected.equalsIgnoreCase("ok"));
            appendOption_(out, "warn", selected.equalsIgnoreCase("warn"));
            appendOption_(out, "alarm", selected.equalsIgnoreCase("alarm"));
            return;
        }

        if (controller.equalsIgnoreCase("meteo") && param.equalsIgnoreCase("humidity"))
        {
            bool selected_added = false;
            for (int h = 0; h <= 100; h += 5)
            {
                const String v = String(h);
                out += "<option value=\"";
                out += v;
                out += "\"";
                if (selected == v)
                {
                    out += " selected";
                    selected_added = true;
                }
                out += ">";
                out += v;
                out += "</option>";
            }
            if (!selected_added && selected.length())
            {
                out += "<option value=\"";
                WebInterface::appendHtmlEscaped_(out, selected.c_str());
                out += "\" selected>";
                WebInterface::appendHtmlEscaped_(out, selected.c_str());
                out += "</option>";
            }
            return;
        }

        if (controller.equalsIgnoreCase("meteo") && param.equalsIgnoreCase("temp_c"))
        {
            bool selected_added = false;
            for (int t = -40; t <= 80; ++t)
            {
                const String v = String(t);
                out += "<option value=\"";
                out += v;
                out += "\"";
                if (selected == v)
                {
                    out += " selected";
                    selected_added = true;
                }
                out += ">";
                out += v;
                out += "</option>";
            }
            if (!selected_added && selected.length())
            {
                out += "<option value=\"";
                WebInterface::appendHtmlEscaped_(out, selected.c_str());
                out += "\" selected>";
                WebInterface::appendHtmlEscaped_(out, selected.c_str());
                out += "</option>";
            }
            return;
        }

        out += "<option value=\"\" selected>-</option>";
    }

void RulesHandler::appendConditionItemOptions_(WebInterface &web, String &out, uint32_t node_id, const String &controller, uint8_t selected_id) {
        if (node_id == 0 && !web._controllers)
        {
            out += "<option value=\"0\" selected>-</option>";
            return;
        }
        bool any = false;
        auto appendItem = [&](uint8_t id, const String &name, bool selected) {
            any = true;
            out += "<option value=\"";
            out += String((unsigned)id);
            out += "\"";
            if (selected)
                out += " selected";
            out += ">";
            if (name.length())
                WebInterface::appendHtmlEscaped_(out, name.c_str());
            else
                out += String("ID ") + String((unsigned)id);
            out += "</option>";
        };

        if (node_id != 0)
        {
            if (controller.equalsIgnoreCase("sockets"))
            {
                StackUnitSnapshot::Snapshot snapshot{};
                if (web.network() && web.network()->stackIndexStateSnapshot(node_id, snapshot) && snapshot.updated_ms != 0)
                {
                    for (uint8_t i = 0; i < snapshot.socket_count && i < StackUnitSnapshot::kSocketCount; ++i)
                    {
                        const auto &it = snapshot.sockets[i];
                        if (!it.enabled)
                            continue;
                        appendItem(it.id, it.name[0] ? String(it.name) : String(), it.id == selected_id);
                    }
                    if (snapshot.sockets_enabled > snapshot.socket_count)
                        web.requestStackSockets_(node_id);
                }
                else
                    web.requestStackSockets_(node_id);
            }
            else if (controller.equalsIgnoreCase("lights"))
            {
                StackUnitSnapshot::Snapshot snapshot{};
                if (web.network() && web.network()->stackIndexStateSnapshot(node_id, snapshot) && snapshot.updated_ms != 0)
                {
                    for (uint8_t i = 0; i < snapshot.light_count && i < StackUnitSnapshot::kSocketCount; ++i)
                    {
                        const auto &it = snapshot.lights[i];
                        if (!it.enabled)
                            continue;
                        appendItem(it.id, it.name[0] ? String(it.name) : String(), it.id == selected_id);
                    }
                    if (snapshot.lights_enabled > snapshot.light_count)
                        web.requestStackLights_(node_id);
                }
                else
                    web.requestStackLights_(node_id);
            }
            else if (controller.equalsIgnoreCase("meteo"))
            {
            }
            else if (controller.equalsIgnoreCase("security"))
            {
            }
            else if (controller.equalsIgnoreCase("tanks"))
            {
            }
            else if (controller.equalsIgnoreCase("septic"))
            {
            }
        }
        else if (controller.equalsIgnoreCase("sockets"))
        {
            SocketController &s = web._controllers->sockets();
            auto guard = s.lockGuard();
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = s.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                appendItem(cfg->id, cfg->name, cfg->id == selected_id);
            }
        }
        else if (controller.equalsIgnoreCase("lights"))
        {
            SocketController &s = web._controllers->sockets();
            auto guard = s.lockGuard();
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = s.lightConfigByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                appendItem(cfg->id, cfg->name, cfg->id == selected_id);
            }
        }
        else if (controller.equalsIgnoreCase("meteo"))
        {
            MeteoController &m = web._controllers->meteo();
            auto guard = m.lockGuard();
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = m.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                appendItem(cfg->id, cfg->name, cfg->id == selected_id);
            }
        }
        else if (controller.equalsIgnoreCase("security"))
        {
            SecurityController &s = web._controllers->security();
            auto guard = s.lockGuard();
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                const auto *cfg = s.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                appendItem(cfg->id, cfg->name, cfg->id == selected_id);
            }
        }
        else if (controller.equalsIgnoreCase("tanks"))
        {
            TankController &t = web._controllers->tanks();
            auto guard = t.lockGuard();
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = t.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                appendItem(cfg->id, cfg->name, cfg->id == selected_id);
            }
        }
        else if (controller.equalsIgnoreCase("septic"))
        {
            SepticController &s = web._controllers->septic();
            auto guard = s.lockGuard();
            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = s.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                appendItem(cfg->id, cfg->name, cfg->id == selected_id);
            }
        }

        if (!any)
            out += "<option value=\"0\" selected>-</option>";
    }

void RulesHandler::appendJsStringLiteral_(String &out, const String &value) {
        out += '\'';
        for (size_t i = 0; i < (size_t)value.length(); ++i)
        {
            const char ch = value[i];
            if (ch == '\\' || ch == '\'')
            {
                out += '\\';
                out += ch;
                continue;
            }
            if (ch == '\n')
            {
                out += "\\n";
                continue;
            }
            if (ch == '\r')
            {
                out += "\\r";
                continue;
            }
            out += ch;
        }
        out += '\'';
    }

void RulesHandler::appendSocketValueOptions_(WebInterface &web, String &out, uint32_t node_id) {
        if (node_id != 0)
        {
            bool any_remote = false;
            StackUnitSnapshot::Snapshot snapshot{};
            if (web.network() && web.network()->stackIndexStateSnapshot(node_id, snapshot) && snapshot.updated_ms != 0)
            {
                for (uint8_t i = 0; i < snapshot.socket_count && i < StackUnitSnapshot::kSocketCount; ++i)
                {
                    const auto &it = snapshot.sockets[i];
                    if (!it.enabled)
                        continue;
                    any_remote = true;
                    out += "<option value=\"";
                    out += String((unsigned)it.id);
                    out += "\">";
                    if (it.name[0])
                        WebInterface::appendHtmlEscaped_(out, it.name);
                    else
                        out += String("Socket #") + String((unsigned)it.id);
                    out += "</option>";
                }
                if (snapshot.sockets_enabled > snapshot.socket_count)
                    web.requestStackSockets_(node_id);
            }
            else
                web.requestStackSockets_(node_id);
            if (!any_remote)
                out += "<option value=\"\">-</option>";
            return;
        }
        if (!web._controllers)
        {
            out += "<option value=\"\">-</option>";
            return;
        }
        SocketController &sockets = web._controllers->sockets();
        auto sockets_guard = sockets.lockGuard();
        bool any = false;
        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const auto *cfg = sockets.configByIndex(i);
            if (!cfg)
                continue;
            any = true;
            out += "<option value=\"";
            out += String((unsigned)cfg->id);
            out += "\">";
            if (cfg->name.length())
                WebInterface::appendHtmlEscaped_(out, cfg->name.c_str());
            else
                out += String(WebUiRu::Rules::kSocketPrefix) + String((unsigned)cfg->id);
            out += "</option>";
        }
        if (!any)
            out += "<option value=\"\">-</option>";
    }

void RulesHandler::appendLightValueOptions_(WebInterface &web, String &out, uint32_t node_id) {
        if (node_id != 0)
        {
            bool any_remote = false;
            StackUnitSnapshot::Snapshot snapshot{};
            if (web.network() && web.network()->stackIndexStateSnapshot(node_id, snapshot) && snapshot.updated_ms != 0)
            {
                for (uint8_t i = 0; i < snapshot.light_count && i < StackUnitSnapshot::kSocketCount; ++i)
                {
                    const auto &it = snapshot.lights[i];
                    if (!it.enabled)
                        continue;
                    any_remote = true;
                    out += "<option value=\"";
                    out += String((unsigned)it.id);
                    out += "\">";
                    if (it.name[0])
                        WebInterface::appendHtmlEscaped_(out, it.name);
                    else
                        out += String("Light #") + String((unsigned)it.id);
                    out += "</option>";
                }
                if (snapshot.lights_enabled > snapshot.light_count)
                    web.requestStackLights_(node_id);
            }
            else
                web.requestStackLights_(node_id);
            if (!any_remote)
                out += "<option value=\"\">-</option>";
            return;
        }
        if (!web._controllers)
        {
            out += "<option value=\"\">-</option>";
            return;
        }
        SocketController &sockets = web._controllers->sockets();
        auto sockets_guard = sockets.lockGuard();
        bool any = false;
        for (size_t i = 0; i < SocketController::kLightCount; ++i)
        {
            const auto *cfg = sockets.lightConfigByIndex(i);
            if (!cfg)
                continue;
            any = true;
            out += "<option value=\"";
            out += String((unsigned)cfg->id);
            out += "\">";
            if (cfg->name.length())
                WebInterface::appendHtmlEscaped_(out, cfg->name.c_str());
            else
                out += String(WebUiRu::Rules::kLightPrefix) + String((unsigned)cfg->id);
            out += "</option>";
        }
        if (!any)
            out += "<option value=\"\">-</option>";
    }

bool RulesHandler::saveRules_(WebInterface &web) {
        if (!web._configs_manager)
            return true;
        return web._configs_manager->save();
    }

uint8_t RulesHandler::parseUInt8_(const String &s) {
        if (!s.length())
            return 0;
        for (size_t i = 0; i < (size_t)s.length(); ++i)
        {
            const char c = s[i];
            if (c < '0' || c > '9')
                return 0;
        }
        const int v = s.toInt();
        if (v < 0 || v > 255)
            return 0;
        return (uint8_t)v;
    }

uint32_t RulesHandler::parseUInt32_(const String &s) {
        if (!s.length())
            return 0;
        for (size_t i = 0; i < (size_t)s.length(); ++i)
        {
            const char c = s[i];
            if (c < '0' || c > '9')
                return 0;
        }
        return (uint32_t)strtoul(s.c_str(), nullptr, 10);
    }

String RulesHandler::rulesGridHtml_(WebInterface &web) {
        String out;
        out.reserve(8192);
        out += "<div class=\"crumbs\">";
        out += WebUiRu::Rules::kSelectRule;
        out += "</div><div class=\"grid\">";
        size_t render_count = RulesController::kRuleCount;
        if (render_count > 0)
        {
            int last_enabled = -1;
            for (size_t i = 0; i < RulesController::kRuleCount; ++i)
            {
                const auto *r = web._rules->ruleByIndex(i);
                if (r && r->enabled)
                    last_enabled = (int)i;
            }
            if (last_enabled < 0)
                render_count = 1;
            else
            {
                render_count = (size_t)last_enabled + 2;
                if (render_count > RulesController::kRuleCount)
                    render_count = RulesController::kRuleCount;
            }
        }
        for (size_t i = 0; i < render_count; ++i)
        {
            const auto *r = web._rules->ruleByIndex(i);
            if (!r)
                continue;
            out += "<div class=\"tile\"><div class=\"tile-head\"><a class=\"tile-link\" href=\"/rules?rule=";
            out += String((unsigned)r->id);
            out += "\"><strong>";
            out += String((unsigned)r->id);
            out += ". ";
            WebInterface::appendHtmlEscaped_(out, r->name.c_str());
            out += "</strong></a>";
            out += "<form method=\"POST\" action=\"/rules\" class=\"inline-toggle\">";
            out += "<input type=\"hidden\" name=\"rule_id\" value=\"";
            out += String((unsigned)r->id);
            out += "\">";
            out += "<input type=\"hidden\" name=\"quick_toggle\" value=\"1\">";
            out += "<input type=\"hidden\" name=\"return_to\" value=\"/rules\">";
            out += "<label class=\"switch\"><input type=\"checkbox\" name=\"enabled\"";
            if (r->enabled)
            out += " checked";
            out += " onchange=\"this.form.submit()\"><span class=\"track\"><span class=\"knob\"></span></span></label>";
            out += "</form></div><span class=\"meta\">";
            out += WebUiRu::Rules::kRule;
            out += "</span></div>";
        }
        out += "</div>";
        return out;
    }

String RulesHandler::actionsGridHtml_(WebInterface &web, uint8_t rule_id, uint32_t preview_cond_node, const String &preview_cond_controller) {
        const auto *r = web._rules->rule(rule_id);
        if (!r)
            return String("<p>") + WebUiRu::Rules::kRuleNotFound + "</p><p><a href=\"/rules\">" + WebUiRu::Rules::kBack + "</a></p>";
        const uint32_t cond_node_id = preview_cond_node ? preview_cond_node : r->condition_node_id;
        const String cond_controller = preview_cond_controller.length() ? preview_cond_controller : r->condition_controller;
        String out;
        out.reserve(12288);
        out += "<div class=\"crumbs\"><a href=\"/rules\">";
        out += WebUiRu::Rules::kRules;
        out += "</a> / ";
        out += String((unsigned)r->id);
        out += ". ";
        WebInterface::appendHtmlEscaped_(out, r->name.c_str());
        out += "</div>";

        out += "<form method=\"POST\" action=\"/rules\">";
        out += "<input type=\"hidden\" name=\"rule_id\" value=\"";
        out += String((unsigned)r->id);
        out += "\">";
        out += "<div class=\"row\">";
        out += "<div><label>";
        out += WebUiRu::Rules::kLabelName;
        out += "</label><input type=\"text\" name=\"name\" value=\"";
        WebInterface::appendHtmlEscaped_(out, r->name.c_str());
        out += "\"></div>";
        out += "<div><label>Enabled</label><label class=\"switch\"><input type=\"checkbox\" name=\"enabled\"";
        if (r->enabled)
            out += " checked";
        out += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
        out += "</div>";
        out += "<div class=\"row\">";
        out += "<div><label>";
        out += WebUiRu::Rules::kLabelConditionEnabled;
        out += "</label><label class=\"switch\"><input type=\"checkbox\" name=\"cond_enabled\"";
        if (r->condition_enabled)
            out += " checked";
        out += " id=\"rule-cond-enabled\"><span class=\"track\"><span class=\"knob\"></span></span></label></div></div>";
        out += "<div id=\"rule-cond-block\">";
        out += "<div class=\"row\"><div><label>";
        out += WebUiRu::Rules::kLabelUnit;
        out += "</label><select name=\"cond_node_id\" id=\"rule-cond-node\">";
        appendNodeSelectOptions_(web, out, cond_node_id);
        out += "</select></div><div><label>";
        out += WebUiRu::Rules::kLabelController;
        out += "</label><select name=\"cond_controller\" id=\"rule-cond-controller\">";
        appendConditionControllerSelectOptions_(web, out, cond_node_id, cond_controller);
        out += "</select></div></div>";
        out += "<div class=\"row\">";
        out += "<div><label>";
        out += WebUiRu::Rules::kLabelItem;
        out += "</label><select name=\"cond_item_id\" id=\"rule-cond-item\">";
        appendConditionItemOptions_(web, out, cond_node_id, cond_controller, r->condition_item_id);
        out += "</select></div>";
        out += "<div><label>";
        out += WebUiRu::Rules::kLabelParameter;
        out += "</label><select name=\"cond_param\" id=\"rule-cond-param\">";
        appendConditionParamOptions_(out, cond_controller, r->condition_parameter);
        out += "</select></div></div>";
        out += "<div class=\"row\">";
        out += "<div><label>";
        out += WebUiRu::Rules::kLabelOperator;
        out += "</label><select name=\"cond_op\" id=\"rule-cond-op\">";
        appendConditionOperatorOptions_(out, r->condition_op);
        out += "</select></div>";
        out += "<div><label>";
        out += WebUiRu::Rules::kLabelValue;
        out += "</label><select name=\"cond_value\" id=\"rule-cond-value\">";
        appendConditionValueOptions_(out, cond_controller, r->condition_parameter, r->condition_value);
        out += "</select></div></div>";
        out += "</div>";
        out += "<button type=\"submit\">";
        out += WebUiRu::kSaveRule;
        out += "</button></form>";

        out += "<script>(function(){";
        out += "const n=document.getElementById('rule-cond-node');";
        out += "const c=document.getElementById('rule-cond-controller');";
        out += "const e=document.getElementById('rule-cond-enabled');";
        out += "const b=document.getElementById('rule-cond-block');";
        out += "const i=document.getElementById('rule-cond-item');";
        out += "const p=document.getElementById('rule-cond-param');";
        out += "const v=document.getElementById('rule-cond-value');";
        out += "if(!c||!i||!p||!v)return;";
        out += "const items={";
        out += "sockets:[";
        if (web._controllers)
        {
            SocketController &s = web._controllers->sockets();
            auto guard = s.lockGuard();
            bool first = true;
            for (size_t idx = 0; idx < SocketController::kSocketCount; ++idx)
            {
                const auto *cfg = s.configByIndex(idx);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    out += ",";
                out += "{id:";
                out += String((unsigned)cfg->id);
                out += ",name:";
                appendJsStringLiteral_(out, cfg->name.length() ? cfg->name : (String(WebUiRu::Rules::kSocketPrefix) + String((unsigned)cfg->id)));
                out += "}";
                first = false;
            }
        }
        out += "],lights:[";
        if (web._controllers)
        {
            SocketController &s = web._controllers->sockets();
            auto guard = s.lockGuard();
            bool first = true;
            for (size_t idx = 0; idx < SocketController::kLightCount; ++idx)
            {
                const auto *cfg = s.lightConfigByIndex(idx);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    out += ",";
                out += "{id:";
                out += String((unsigned)cfg->id);
                out += ",name:";
                appendJsStringLiteral_(out, cfg->name.length() ? cfg->name : (String(WebUiRu::Rules::kLightPrefix) + String((unsigned)cfg->id)));
                out += "}";
                first = false;
            }
        }
        out += "],meteo:[";
        if (web._controllers)
        {
            MeteoController &m = web._controllers->meteo();
            auto guard = m.lockGuard();
            bool first = true;
            for (size_t idx = 0; idx < MeteoController::kSensorCount; ++idx)
            {
                const auto *cfg = m.configByIndex(idx);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    out += ",";
                out += "{id:";
                out += String((unsigned)cfg->id);
                out += ",name:";
                appendJsStringLiteral_(out, cfg->name.length() ? cfg->name : (String(WebUiRu::Rules::kSensorPrefix) + String((unsigned)cfg->id)));
                out += "}";
                first = false;
            }
        }
        out += "],security:[";
        if (web._controllers)
        {
            SecurityController &s = web._controllers->security();
            auto guard = s.lockGuard();
            bool first = true;
            for (size_t idx = 0; idx < SecurityController::kSensorCount; ++idx)
            {
                const auto *cfg = s.configByIndex(idx);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    out += ",";
                out += "{id:";
                out += String((unsigned)cfg->id);
                out += ",name:";
                appendJsStringLiteral_(out, cfg->name.length() ? cfg->name : (String(WebUiRu::Rules::kMeteoSensorPrefix) + String((unsigned)cfg->id)));
                out += "}";
                first = false;
            }
        }
        out += "],tanks:[";
        if (web._controllers)
        {
            TankController &t = web._controllers->tanks();
            auto guard = t.lockGuard();
            bool first = true;
            for (size_t idx = 0; idx < TankController::kTankCount; ++idx)
            {
                const auto *cfg = t.configByIndex(idx);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    out += ",";
                out += "{id:";
                out += String((unsigned)cfg->id);
                out += ",name:";
                appendJsStringLiteral_(out, cfg->name.length() ? cfg->name : (String(WebUiRu::Rules::kTankPrefix) + String((unsigned)cfg->id)));
                out += "}";
                first = false;
            }
        }
        out += "],septic:[";
        if (web._controllers)
        {
            SepticController &s = web._controllers->septic();
            auto guard = s.lockGuard();
            bool first = true;
            for (size_t idx = 0; idx < SepticController::kSepticCount; ++idx)
            {
                const auto *cfg = s.configByIndex(idx);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    out += ",";
                out += "{id:";
                out += String((unsigned)cfg->id);
                out += ",name:";
                appendJsStringLiteral_(out, cfg->name.length() ? cfg->name : (String(WebUiRu::Rules::kSepticPrefix) + String((unsigned)cfg->id)));
                out += "}";
                first = false;
            }
        }
        out += "]};";
        out += "const params={";
        out += "sockets:['relay_on'],";
        out += "lights:['relay_on'],";
        out += "meteo:['ok','temp_c','humidity'],";
        out += "security:['detect','armed','alarm_on'],";
        out += "tanks:['level','empty','valve_on','pump_on','alarm_on'],";
        out += "septic:['status','warning','alarm']";
        out += "};";
        out += "function vals(ctrl,param){";
        out += "if((ctrl==='sockets'||ctrl==='lights')&&param==='relay_on')return ['off','on'];";
        out += "if(ctrl==='meteo'&&param==='ok')return ['off','on'];";
        out += "if(ctrl==='meteo'&&param==='temp_c'){const a=[];for(let t=-40;t<=80;t++)a.push(String(t));return a;}";
        out += "if(ctrl==='meteo'&&param==='humidity'){const a=[];for(let h=0;h<=100;h+=5)a.push(String(h));return a;}";
        out += "if(ctrl==='security'&&(param==='detect'||param==='armed'||param==='alarm_on'))return ['off','on'];";
        out += "if(ctrl==='tanks'&&param==='level')return ['empty','low','mid','full'];";
        out += "if(ctrl==='tanks'&&(param==='empty'||param==='valve_on'||param==='pump_on'||param==='alarm_on'))return ['off','on'];";
        out += "if(ctrl==='septic'&&param==='status')return ['ok','warn','alarm'];";
        out += "if(ctrl==='septic'&&(param==='warning'||param==='alarm'))return ['off','on'];";
        out += "return [''];";
        out += "}";
        out += "const selItem=";
        appendJsStringLiteral_(out, String((unsigned)r->condition_item_id));
        out += ";";
        out += "const selParam=";
        appendJsStringLiteral_(out, r->condition_parameter);
        out += ";";
        out += "const selValue=";
        appendJsStringLiteral_(out, r->condition_value);
        out += ";";
        out += "const remoteNode=";
        out += (cond_node_id != 0) ? "true;" : "false;";
        out += "let first=true;";
        out += "function refillItems(ctrl){const arr=items[ctrl]||[];const cur=i.value||'';i.innerHTML='';let hit=false;";
        out += "for(let k=0;k<arr.length;k++){const o=document.createElement('option');o.value=String(arr[k].id);o.textContent=arr[k].name;const want=first?selItem:cur;if(o.value===want){o.selected=true;hit=true;}i.appendChild(o);}if(!arr.length){const o=document.createElement('option');o.value='0';o.textContent='-';o.selected=true;i.appendChild(o);}else if(!hit){i.selectedIndex=0;}}";
        out += "function refillParams(ctrl){const arr=params[ctrl]||[];const cur=p.value||'';p.innerHTML='';let hit=false;";
        out += "for(let k=0;k<arr.length;k++){const x=arr[k];const o=document.createElement('option');o.value=x;o.textContent=x;const want=first?selParam:cur;if(x===want){o.selected=true;hit=true;}p.appendChild(o);}if(!arr.length){const o=document.createElement('option');o.value='';o.textContent='-';o.selected=true;p.appendChild(o);}else if(!hit){p.selectedIndex=0;}}";
        out += "function refillValues(ctrl,param){const arr=vals(ctrl,param);const cur=v.value||'';v.innerHTML='';let hit=false;";
        out += "for(let k=0;k<arr.length;k++){const x=arr[k];const o=document.createElement('option');o.value=x;o.textContent=x||'-';const want=first?selValue:cur;if(x===want){o.selected=true;hit=true;}v.appendChild(o);}if(!hit&&arr.length)v.selectedIndex=0;}";
        out += "function sync(){const ctrl=(c.value||'').toLowerCase();if(!remoteNode)refillItems(ctrl);refillParams(ctrl);refillValues(ctrl,p.value||'');first=false;}";
        out += "function toggleCondUi(){if(!b||!e)return;b.style.display=e.checked?'block':'none';}";
        out += "c.addEventListener('change',sync);";
        out += "p.addEventListener('change',function(){refillValues((c.value||'').toLowerCase(),p.value||'');});";
        out += "if(e)e.addEventListener('change',toggleCondUi);";
        out += "sync();";
        out += "toggleCondUi();";
        out += "})();</script>";

        out += "<div class=\"grid\">";
        size_t render_count = RulesController::kActionCount;
        if (render_count > 0)
        {
            int last_enabled = -1;
            for (size_t i = 0; i < RulesController::kActionCount; ++i)
            {
                if (r->actions[i].enabled)
                    last_enabled = (int)i;
            }
            if (last_enabled < 0)
                render_count = 1;
            else
            {
                render_count = (size_t)last_enabled + 2;
                if (render_count > RulesController::kActionCount)
                    render_count = RulesController::kActionCount;
            }
        }
        for (size_t i = 0; i < render_count; ++i)
        {
            const auto &a = r->actions[i];
            out += "<div class=\"tile\"><div class=\"tile-head\"><a class=\"tile-link\" href=\"/rules?rule=";
            out += String((unsigned)r->id);
            out += "&action=";
            out += String((unsigned)a.id);
            out += "\"><strong>";
            out += WebUiRu::Rules::kAction;
            out += String((unsigned)a.id);
            out += "</strong></a>";
            out += "<form method=\"POST\" action=\"/rules\" class=\"inline-toggle\">";
            out += "<input type=\"hidden\" name=\"rule_id\" value=\"";
            out += String((unsigned)r->id);
            out += "\"><input type=\"hidden\" name=\"action_id\" value=\"";
            out += String((unsigned)a.id);
            out += "\"><input type=\"hidden\" name=\"quick_toggle\" value=\"1\"><input type=\"hidden\" name=\"return_to\" value=\"/rules?rule=";
            out += String((unsigned)r->id);
            out += "\"><label class=\"switch\"><input type=\"checkbox\" name=\"enabled\"";
            if (a.enabled)
                out += " checked";
            out += " onchange=\"this.form.submit()\"><span class=\"track\"><span class=\"knob\"></span></span></label></form></div><span class=\"meta\">";
            out += WebUiRu::Rules::kActionType;
            if (a.kind == RulesController::ActionKind::Pause)
                out += WebUiRu::Rules::kPause;
            else if (a.kind == RulesController::ActionKind::Notify)
                out += "notify";
            else
                out += "controller";
            out += "</span></div>";
        }
        out += "</div>";
        return out;
    }

String RulesHandler::actionEditHtml_(WebInterface &web, uint8_t rule_id, uint8_t action_id, uint32_t preview_action_node) {
        const auto *r = web._rules->rule(rule_id);
        const auto *a = web._rules->action(rule_id, action_id);
        if (!r || !a)
            return String("<p>") + WebUiRu::Rules::kInvalidRuleActionId + "</p><p><a href=\"/rules\">" + WebUiRu::Rules::kBack + "</a></p>";
        const uint32_t action_node_id = preview_action_node ? preview_action_node : a->node_id;
        String out;
        out.reserve(8192);
        out += "<div class=\"crumbs\"><a href=\"/rules\">";
        out += WebUiRu::Rules::kRules;
        out += "</a> / <a href=\"/rules?rule=";
        out += String((unsigned)r->id);
        out += "\">";
        out += String((unsigned)r->id);
        out += ". ";
        WebInterface::appendHtmlEscaped_(out, r->name.c_str());
        out += "</a> / ";
        out += WebUiRu::Rules::kActionLower;
        out += String((unsigned)a->id);
        out += "</div>";

        out += "<form method=\"POST\" action=\"/rules\">";
        out += "<input type=\"hidden\" name=\"rule_id\" value=\"";
        out += String((unsigned)r->id);
        out += "\"><input type=\"hidden\" name=\"action_id\" value=\"";
        out += String((unsigned)a->id);
        out += "\">";

        out += "<div class=\"row single\"><div><label>Enabled</label><label class=\"switch\"><input type=\"checkbox\" name=\"enabled\"";
        if (a->enabled)
            out += " checked";
        out += "><span class=\"track\"><span class=\"knob\"></span></span></label></div></div>";

        out += "<div class=\"row\"><div><label>";
        out += WebUiRu::Rules::kLabelType;
        out += "</label><select name=\"kind\">";
        out += "<option value=\"controller\"";
        if (a->kind == RulesController::ActionKind::Controller)
            out += " selected";
        out += ">controller</option>";
        out += "<option value=\"pause\"";
        if (a->kind == RulesController::ActionKind::Pause)
            out += " selected";
        out += ">pause</option>";
        out += "<option value=\"notify\"";
        if (a->kind == RulesController::ActionKind::Notify)
            out += " selected";
        out += ">notify</option></select></div>";
        out += "<div><label>";
        out += WebUiRu::Rules::kLabelDelayMs;
        out += "</label><input type=\"number\" name=\"delay_ms\" min=\"0\" value=\"";
        out += String((unsigned long)a->delay_ms);
        out += "\"></div></div>";

        out += "<div class=\"row\"><div><label>";
        out += WebUiRu::Rules::kLabelUnit;
        out += "</label><select name=\"node_id\" id=\"rule-node\">";
        appendNodeSelectOptions_(web, out, action_node_id);
        out += "</select></div><div><label>Controller</label><select name=\"controller\" id=\"rule-controller\">";
        appendControllerSelectOptions_(web, out, action_node_id, a->controller);
        out += "</select></div>";
        out += "<div><label>";
        out += WebUiRu::Rules::kLabelParameter;
        out += "</label><select name=\"parameter\" id=\"rule-parameter\">";
        appendParameterSelectOptions_(out, a->controller, a->parameter);
        out += "</select></div></div>";

        out += "<div class=\"row\" id=\"rule-value-manual-row\"><div><label>";
        out += WebUiRu::Rules::kLabelValue;
        out += "</label><input type=\"text\" id=\"rule-value-manual\" name=\"value\" value=\"";
        WebInterface::appendHtmlEscaped_(out, a->value.c_str());
        out += "\"></div></div>";

        out += "<div class=\"row\" id=\"rule-value-sockets-row\" style=\"display:none\"><div><label>";
        out += WebUiRu::Users::kItemSocket;
        out += "</label><select id=\"rule-socket-id\">";
        appendSocketValueOptions_(web, out, action_node_id);
        out += "</select></div><div id=\"rule-socket-state-wrap\"><label>";
        out += WebUiRu::Rules::kLabelState;
        out += "</label><select id=\"rule-socket-state\"><option value=\"on\">on</option><option value=\"off\">off</option></select></div></div>";
        out += "<div class=\"row\" id=\"rule-value-lights-row\" style=\"display:none\"><div><label>";
        out += WebUiRu::Users::kItemLight;
        out += "</label><select id=\"rule-light-id\">";
        appendLightValueOptions_(web, out, action_node_id);
        out += "</select></div><div id=\"rule-light-state-wrap\"><label>";
        out += WebUiRu::Rules::kLabelState;
        out += "</label><select id=\"rule-light-state\"><option value=\"on\">on</option><option value=\"off\">off</option></select></div></div>";

        out += "<button type=\"submit\">";
        out += WebUiRu::kSaveAction;
        out += "</button>";
        out += "<script>(function(){";
        out += "const m={";
        out += "sockets:['set','toggle'],";
        out += "lights:['set','toggle'],";
        out += "meteo:['read'],";
        out += "thermo:['power','mode','target'],";
        out += "tanks:['power'],";
        out += "septic:['monitor'],";
        out += "security:['arm','disarm','clear','rfid','ibutton'],";
        out += "watering:['status'],";
        out += "ring:['hold'],";
        out += "avr:['auto','source','clear_fault'],";
        out += "leak:['power','ack','ack_all']";
        out += "};";
        out += "const node=document.getElementById('rule-node');";
        out += "const c=document.getElementById('rule-controller');";
        out += "const p=document.getElementById('rule-parameter');";
        out += "const v=document.getElementById('rule-value-manual');";
        out += "const vm=document.getElementById('rule-value-manual-row');";
        out += "const vs=document.getElementById('rule-value-sockets-row');";
        out += "const vl=document.getElementById('rule-value-lights-row');";
        out += "const sid=document.getElementById('rule-socket-id');";
        out += "const sw=document.getElementById('rule-socket-state-wrap');";
        out += "const sst=document.getElementById('rule-socket-state');";
        out += "const lid=document.getElementById('rule-light-id');";
        out += "const lw=document.getElementById('rule-light-state-wrap');";
        out += "const lst=document.getElementById('rule-light-state');";
        out += "if(!c||!p||!v)return;";
        out += "if(node)node.addEventListener('change',function(){window.location='/rules?rule=";
        out += String((unsigned)r->id);
        out += "&action=";
        out += String((unsigned)a->id);
        out += "&action_node='+encodeURIComponent(node.value||'0');});";
        out += "const prev=";
        appendJsStringLiteral_(out, a->parameter);
        out += ";";
        out += "function parseSocketValue(raw){";
        out += "raw=(raw||'').trim();";
        out += "if(!raw)return null;";
        out += "const i=raw.indexOf(',');";
        out += "if(i<0)return{id:raw,state:''};";
        out += "return{id:raw.substring(0,i).trim(),state:raw.substring(i+1).trim().toLowerCase()};";
        out += "}";
        out += "function syncSwitchRaw(){";
        out += "const ctrl=(c.value||'').toLowerCase();";
        out += "const prm=(p.value||'').toLowerCase();";
        out += "if(ctrl!=='sockets'&&ctrl!=='lights')return;";
        out += "const isSocket=ctrl==='sockets';";
        out += "const id=((isSocket?sid:lid)&&(isSocket?sid:lid).value)||'';";
        out += "if(!id){v.value='';return;}";
        out += "if(prm==='set'){const st=((isSocket?sst:lst)&&(isSocket?sst:lst).value)?(isSocket?sst:lst).value:'off';v.value=id+','+st;return;}";
        out += "if(prm==='toggle'){v.value=id;return;}";
        out += "}";
        out += "function applySwitchUi(){";
        out += "const ctrl=(c.value||'').toLowerCase();";
        out += "const prm=(p.value||'').toLowerCase();";
        out += "const socketsMode=(ctrl==='sockets'&&(prm==='set'||prm==='toggle'));";
        out += "const lightsMode=(ctrl==='lights'&&(prm==='set'||prm==='toggle'));";
        out += "const autoMode=socketsMode||lightsMode;";
        out += "if(vm)vm.style.display=autoMode?'none':'grid';";
        out += "if(vs)vs.style.display=socketsMode?'grid':'none';";
        out += "if(vl)vl.style.display=lightsMode?'grid':'none';";
        out += "if(sw)sw.style.display=(socketsMode&&prm==='set')?'block':'none';";
        out += "if(lw)lw.style.display=(lightsMode&&prm==='set')?'block':'none';";
        out += "if(!autoMode)return;";
        out += "const parsed=parseSocketValue(v.value);";
        out += "if(parsed){";
        out += "const idSel=socketsMode?sid:lid;";
        out += "const stSel=socketsMode?sst:lst;";
        out += "if(idSel){for(let i=0;i<idSel.options.length;i++){if(idSel.options[i].value===parsed.id){idSel.selectedIndex=i;break;}}}";
        out += "if(parsed.state&&stSel){stSel.value=(parsed.state==='on'||parsed.state==='1'||parsed.state==='true')?'on':'off';}";
        out += "}";
        out += "syncSwitchRaw();";
        out += "}";
        out += "let first=true;";
        out += "function fill(sel){";
        out += "const key=(sel||'').toLowerCase();";
        out += "const arr=m[key]||[];";
        out += "const cur=(p.value||'').toLowerCase();";
        out += "const want=first?(prev||'').toLowerCase():cur;";
        out += "p.innerHTML='';";
        out += "let hit=false;";
        out += "for(let i=0;i<arr.length;i++){";
        out += "const v=arr[i];const o=document.createElement('option');";
        out += "o.value=v;o.textContent=v;";
        out += "if(v.toLowerCase()===want){o.selected=true;hit=true;}";
        out += "p.appendChild(o);}";
        out += "if(!arr.length){const o=document.createElement('option');";
        out += "o.value='';o.textContent='-';o.selected=true;p.appendChild(o);}";
        out += "if(first&&!hit&&prev){const o=document.createElement('option');";
        out += "o.value=prev;o.textContent=prev;o.selected=true;p.appendChild(o);}";
        out += "first=false;";
        out += "applySwitchUi();";
        out += "}";
        out += "c.addEventListener('change',function(){fill(c.value);});";
        out += "p.addEventListener('change',function(){applySwitchUi();});";
        out += "if(sid)sid.addEventListener('change',syncSwitchRaw);";
        out += "if(sst)sst.addEventListener('change',syncSwitchRaw);";
        out += "if(lid)lid.addEventListener('change',syncSwitchRaw);";
        out += "if(lst)lst.addEventListener('change',syncSwitchRaw);";
        out += "fill(c.value);";
        out += "})();</script>";
        out += "</form>";
        return out;
    }
