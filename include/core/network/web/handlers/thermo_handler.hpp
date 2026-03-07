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

class WebInterface;
class AsyncWebServer;
class AsyncWebServerRequest;

class ThermoHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/thermo/toggle", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleThermoToggle(web, request); });
        server.on("/thermo/toggle", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleThermoToggle(web, request); });
        server.on("/thermo", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleThermoSave(web, request); });
        server.on("/thermo", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleThermo(web, request); });
    }

    static void handleThermo(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Thermo, node_id))
            return;
        const bool stack_view = web.isStackThermoView_(node_id);
        const bool groups_available = stack_view ? web.hasGroups_(node_id) : web.hasGroups_();
        const uint8_t page_size = 8u;
        const String page_str = web.paramValueAny_(request, "page");
        uint8_t page_idx = 0;
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (uint8_t)(v - 1);
        }
        uint8_t max_pages = 1;
        if (stack_view)
        {
            web.requestStackThermo_(node_id);
            web.requestStackMeteo_(node_id);
            web.requestStackPorts_(node_id);
            if (!groups_available)
            {
                const size_t visible = web.stackThermoVisibleCount_(node_id);
                max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
                if (page_idx >= max_pages)
                    page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            }
            else
            {
                page_idx = 0;
                max_pages = 1;
            }
        }
        else if (!groups_available)
        {
            const size_t visible = web.thermoLocalRenderCount_();
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        else
        {
            page_idx = 0;
            max_pages = 1;
        }
        if (!stack_view && web.stackRole_() == ConfigsManagerIface::StackRole::Slave)
        {
            if (web._stack_slave)
                web._stack_slave->requestRemoteMeteoAll();
        }
        else if (!stack_view && web.stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = web._stack_master ? web._stack_master->nodeCount() : 0;
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t id = web._stack_master->nodeIdAt(i);
                if (id != 0)
                    web.stackCache().requestMeteo(id);
            }
        }
        String page = FPSTR(kWebInterfaceThermoHtml);
        page.reserve(page.length() + 16384);
        page.replace("%THERMO_PAGE_TITLE%", WebUiRu::Thermo::kPageTitle);
        String pagination = "";
        if (stack_view && max_pages > 1)
        {
            pagination.reserve(256);
            pagination += "<div class=\"pagination\">";
            if (page_idx > 0)
            {
                pagination += "<a class=\"page-btn\" href=\"/thermo?unit=stack&node=";
                pagination += String((unsigned long)node_id);
                pagination += "&page=";
                pagination += String((unsigned)page_idx);
                pagination += String("\">") + WebUiRu::Common::kPagePrev + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPagePrev + "</span>";
            pagination += String("<span class=\"page-info\">") + WebUiRu::Common::kPagePage + " ";
            pagination += String((unsigned)(page_idx + 1));
            pagination += " / ";
            pagination += String((unsigned)max_pages);
            pagination += "</span>";
            if ((page_idx + 1u) < max_pages)
            {
                pagination += "<a class=\"page-btn\" href=\"/thermo?unit=stack&node=";
                pagination += String((unsigned long)node_id);
                pagination += "&page=";
                pagination += String((unsigned)(page_idx + 2u));
                pagination += String("\">") + WebUiRu::Common::kPageNext + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPageNext + "</span>";
            pagination += "</div>";
        }
        else if (!stack_view && !groups_available && max_pages > 1)
        {
            pagination.reserve(256);
            pagination += "<div class=\"pagination\">";
            if (page_idx > 0)
            {
                pagination += "<a class=\"page-btn\" href=\"/thermo?page=";
                pagination += String((unsigned)page_idx);
                pagination += String("\">") + WebUiRu::Common::kPagePrev + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPagePrev + "</span>";
            pagination += String("<span class=\"page-info\">") + WebUiRu::Common::kPagePage + " ";
            pagination += String((unsigned)(page_idx + 1));
            pagination += " / ";
            pagination += String((unsigned)max_pages);
            pagination += "</span>";
            if ((page_idx + 1u) < max_pages)
            {
                pagination += "<a class=\"page-btn\" href=\"/thermo?page=";
                pagination += String((unsigned)(page_idx + 2u));
                pagination += String("\">") + WebUiRu::Common::kPageNext + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPageNext + "</span>";
            pagination += "</div>";
        }
        page.replace("%NAV%", web.navHtml_());
        page.replace("%THERMO_ROWS%", stack_view ? web.listStackThermoHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size, groups_available ? SIZE_MAX : page_size)
                                                 : web.listThermoHtml_(groups_available ? 0u : (size_t)page_idx * page_size,
                                                                       groups_available ? SIZE_MAX : page_size));
        page.replace("%THERMO_PAGINATION%", pagination);
        page.replace("%THERMO_STATUS%", stack_view ? web.stackThermoStatusText_(node_id) : web._thermo_status);
        page.replace("%THERMO_DINPUT_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::DInput)
                                                        : web.thermoPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%THERMO_RELAY_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::Relay)
                                                       : web.thermoPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%THERMO_DINPUT_USED_JSON%",
                     stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::DInput)
                                : web.globalUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%THERMO_RELAY_USED_JSON%",
                     stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::Relay)
                                : web.globalUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%THERMO_JS_HEAT%", WebUiRu::Thermo::kText5);
        page.replace("%THERMO_JS_COOL%", WebUiRu::Thermo::kText6);
        page.replace("%THERMO_JS_IDLE%", WebUiRu::Thermo::kText7);
        if (stack_view)
        {
            String hidden;
            hidden.reserve(96);
            hidden += "<input type=\"hidden\" name=\"unit\" value=\"stack\">";
            hidden += "<input type=\"hidden\" name=\"node\" value=\"";
            hidden += String((unsigned long)node_id);
            hidden += "\">";
            hidden += "<input type=\"hidden\" name=\"page\" value=\"";
            hidden += String((unsigned)(page_idx + 1));
            hidden += "\">";
            page.replace("%THERMO_FORM_HIDDEN%", hidden);
        }
        else
        {
            page.replace("%THERMO_FORM_HIDDEN%", "");
        }
        bool can_save = web.webSessionIsAdmin_();
        const bool can_view_disabled = web.webSessionIsAdmin_();
        if (stack_view)
        {
            const auto *cache = web._stack_cache ? web._stack_cache->thermoCache(node_id) : nullptr;
            if (cache && cache->has_data && cache->items)
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!web.webAclCanViewItem_(UsersRegistry::AclController::Thermo, it.id, node_id))
                        continue;
                    if (!can_view_disabled && !it.enabled)
                        continue;
                    if (web.webAclCanControlItem_(UsersRegistry::AclController::Thermo, it.id, node_id))
                    {
                        can_save = true;
                        break;
                    }
                }
            }
        }
        else if (web._controllers)
        {
            ThermoController &thermo = web._controllers->thermo();
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = thermo.configByIndex(i);
                if (!cfg)
                    continue;
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Thermo, cfg->id))
                    continue;
                if (!can_view_disabled && !cfg->enabled)
                    continue;
                if (web.webAclCanControlItem_(UsersRegistry::AclController::Thermo, cfg->id))
                {
                    can_save = true;
                    break;
                }
            }
        }
        page.replace("%THERMO_DEVICE_SELECT%",
                     web.composeTopFiltersHtml_(web.thermoDeviceSelectHtml_(node_id, stack_view),
                                                groups_available ? web.groupFilterHtml_("thermo-group-filter", stack_view ? node_id : 0u) : String("")));
        page.replace("%THERMO_SAVE_BTN%",
                     can_save ? (String("<button type=\"submit\">") + WebUiRu::kSave + "</button>")
                              : (String("<button type=\"submit\" disabled>") + WebUiRu::kSave + "</button>"));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleThermoSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Thermo))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackThermoView_(node_id))
        {
            String back = String("/thermo?unit=stack&node=") + String((unsigned long)node_id);
            const String page_str = web.paramValueAny_(request, "page");
            if (page_str.length())
            {
                const int pv = page_str.toInt();
                if (pv > 0)
                {
                    back += "&page=";
                    back += String((unsigned)pv);
                }
            }
            if (!web._stack_master || !web._stack_cache)
            {
                web._thermo_status = "Stack unavailable";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            const auto *cache = web._stack_cache->thermoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                web._stack_cache->requestThermo(node_id);
                web._thermo_status = "No data";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            auto *cache_mut = web._stack_cache->thermoCache(node_id);
            bool changed = false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                const String group_key = String("t") + String((unsigned)it.id) + "_group";
                const String power_key = String("t") + String((unsigned)it.id) + "_power";
                const String mode_key = String("t") + String((unsigned)it.id) + "_mode";
                const String target_key = String("t") + String((unsigned)it.id) + "_target";
                const String hyst_key = String("t") + String((unsigned)it.id) + "_hyst";
                const String heat_key = String("t") + String((unsigned)it.id) + "_heat";
                const String cool_key = String("t") + String((unsigned)it.id) + "_cool";
                const String button_key = String("t") + String((unsigned)it.id) + "_button";
                const bool has_any = request->hasParam(power_key, true) ||
                                     request->hasParam(group_key, true) ||
                                     request->hasParam(mode_key, true) ||
                                     request->hasParam(target_key, true) ||
                                     request->hasParam(hyst_key, true) ||
                                     request->hasParam(heat_key, true) ||
                                     request->hasParam(cool_key, true) ||
                                     request->hasParam(button_key, true);
                if (!has_any)
                    continue;
                if (!web.webAclCanControlItem_(UsersRegistry::AclController::Thermo, it.id, node_id))
                {
                    web._thermo_status = String("ACL deny item: ") + String((unsigned)it.id);
                    web.sendRedirect_(request, back, set_cookie);
                    return;
                }
                bool item_changed = false;
                bool set_power = false;
                bool new_power_on = it.power_on;
                bool set_mode = false;
                ThermoController::Mode new_mode = ThermoController::Mode::Off;
                bool set_target = false;
                float new_target = it.target;
                bool set_hyst = false;
                float new_hyst = it.hyst;
                bool set_heat = false;
                uint8_t new_heat = it.heat;
                bool set_cool = false;
                uint8_t new_cool = it.cool;
                bool set_button = false;
                uint8_t new_button = it.button;
                bool set_group = false;
                uint8_t new_group_id = it.group_id;

                if (request->hasParam(group_key, true))
                {
                    const uint8_t group_id = web.parseGroupIdParam_(request, group_key);
                    if (group_id != it.group_id)
                    {
                        item_changed = true;
                        set_group = true;
                        new_group_id = group_id;
                    }
                }

                if (request->hasParam(power_key, true))
                {
                    const String power_str = web.paramValue_(request, power_key);
                    const bool has_power = (power_str == "on" || power_str == "off" || power_str == "1" || power_str == "0" ||
                                            power_str == "true" || power_str == "false");
                    if (!has_power)
                    {
                        web._thermo_status = String("Invalid power for device ") + String((unsigned)it.id);
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    const bool power_on = (power_str == "on" || power_str == "1" || power_str == "true");
                    if (power_on != it.power_on)
                    {
                        item_changed = true;
                        set_power = true;
                        new_power_on = power_on;
                    }
                }

                if (request->hasParam(mode_key, true))
                {
                    ThermoController::Mode mode = ThermoController::Mode::Off;
                    const String mode_str = web.paramValue_(request, mode_key);
                    if (!web.parseThermoMode_(mode_str, mode))
                    {
                        web._thermo_status = String("Invalid mode for device ") + String((unsigned)it.id);
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    ThermoController::Mode current_mode = ThermoController::Mode::Off;
                    (void)web.parseThermoMode_(String(it.mode), current_mode);
                    if (mode != current_mode)
                    {
                        item_changed = true;
                        set_mode = true;
                        new_mode = mode;
                    }
                }

                if (request->hasParam(target_key, true))
                {
                    float target = it.target;
                    const String target_str = web.paramValue_(request, target_key);
                    if (!web.parseThermoFloat_(target_str, target))
                    {
                        web._thermo_status = String("Invalid target for device ") + String((unsigned)it.id);
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    const float delta = (target > it.target) ? (target - it.target) : (it.target - target);
                    if (delta > 0.01f)
                    {
                        item_changed = true;
                        set_target = true;
                        new_target = target;
                    }
                }

                if (request->hasParam(hyst_key, true))
                {
                    float hyst = it.hyst;
                    const String hyst_str = web.paramValue_(request, hyst_key);
                    if (!web.parseThermoFloat_(hyst_str, hyst))
                    {
                        web._thermo_status = String("Invalid hyst for device ") + String((unsigned)it.id);
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    const float delta = (hyst > it.hyst) ? (hyst - it.hyst) : (it.hyst - hyst);
                    if (delta > 0.01f)
                    {
                        item_changed = true;
                        set_hyst = true;
                        new_hyst = hyst;
                    }
                }

                if (request->hasParam(heat_key, true))
                {
                    uint8_t port = it.heat;
                    const String port_str = web.paramValue_(request, heat_key);
                    if (!web.parseSocketPort_(port_str, port))
                    {
                        web._thermo_status = String("Invalid heat port for device ") + String((unsigned)it.id);
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    if (port != it.heat)
                    {
                        item_changed = true;
                        set_heat = true;
                        new_heat = port;
                    }
                }

                if (request->hasParam(cool_key, true))
                {
                    uint8_t port = it.cool;
                    const String port_str = web.paramValue_(request, cool_key);
                    if (!web.parseSocketPort_(port_str, port))
                    {
                        web._thermo_status = String("Invalid cool port for device ") + String((unsigned)it.id);
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    if (port != it.cool)
                    {
                        item_changed = true;
                        set_cool = true;
                        new_cool = port;
                    }
                }

                if (request->hasParam(button_key, true))
                {
                    uint8_t port = it.button;
                    const String port_str = web.paramValue_(request, button_key);
                    if (!web.parseSocketPort_(port_str, port))
                    {
                        web._thermo_status = String("Invalid button port for device ") + String((unsigned)it.id);
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    if (port != it.button)
                    {
                        item_changed = true;
                        set_button = true;
                        new_button = port;
                    }
                }

                if (!item_changed)
                    continue;

                bool sent_any = false;
                // Slave handler applies one field per set-item, so send separate frames.
                if (set_mode)
                {
                    StaticJsonDocument<192> doc;
                    doc["cmd_id"] = 0;
                    doc["feature"] = (uint8_t)StackFeature::Thermo;
                    doc["action"] = "set";
                    JsonArray items = doc["params"]["items"].to<JsonArray>();
                    JsonObject obj = items.add<JsonObject>();
                    obj["id"] = (unsigned)it.id;
                    obj["mode"] = ThermoController::modeName(new_mode);
                    char payload[192] = {};
                    const size_t len = serializeJson(doc, payload, sizeof(payload));
                    if (len > 0 && web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                             reinterpret_cast<const uint8_t *>(payload), len))
                        sent_any = true;
                }
                if (set_group)
                {
                    StaticJsonDocument<192> doc;
                    doc["cmd_id"] = 0;
                    doc["feature"] = (uint8_t)StackFeature::Thermo;
                    doc["action"] = "set";
                    JsonArray items = doc["params"]["items"].to<JsonArray>();
                    JsonObject obj = items.add<JsonObject>();
                    obj["id"] = (unsigned)it.id;
                    obj["group_id"] = new_group_id;
                    char payload[192] = {};
                    const size_t len = serializeJson(doc, payload, sizeof(payload));
                    if (len > 0 && web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                             reinterpret_cast<const uint8_t *>(payload), len))
                        sent_any = true;
                }
                if (set_target)
                {
                    StaticJsonDocument<192> doc;
                    doc["cmd_id"] = 0;
                    doc["feature"] = (uint8_t)StackFeature::Thermo;
                    doc["action"] = "set";
                    JsonArray items = doc["params"]["items"].to<JsonArray>();
                    JsonObject obj = items.add<JsonObject>();
                    obj["id"] = (unsigned)it.id;
                    obj["target"] = new_target;
                    char payload[192] = {};
                    const size_t len = serializeJson(doc, payload, sizeof(payload));
                    if (len > 0 && web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                             reinterpret_cast<const uint8_t *>(payload), len))
                        sent_any = true;
                }
                if (set_hyst)
                {
                    StaticJsonDocument<192> doc;
                    doc["cmd_id"] = 0;
                    doc["feature"] = (uint8_t)StackFeature::Thermo;
                    doc["action"] = "set";
                    JsonArray items = doc["params"]["items"].to<JsonArray>();
                    JsonObject obj = items.add<JsonObject>();
                    obj["id"] = (unsigned)it.id;
                    obj["hyst"] = new_hyst;
                    char payload[192] = {};
                    const size_t len = serializeJson(doc, payload, sizeof(payload));
                    if (len > 0 && web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                             reinterpret_cast<const uint8_t *>(payload), len))
                        sent_any = true;
                }
                if (set_power)
                {
                    StaticJsonDocument<192> doc;
                    doc["cmd_id"] = 0;
                    doc["feature"] = (uint8_t)StackFeature::Thermo;
                    doc["action"] = "set";
                    JsonArray items = doc["params"]["items"].to<JsonArray>();
                    JsonObject obj = items.add<JsonObject>();
                    obj["id"] = (unsigned)it.id;
                    obj["power"] = new_power_on;
                    char payload[192] = {};
                    const size_t len = serializeJson(doc, payload, sizeof(payload));
                    if (len > 0 && web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                             reinterpret_cast<const uint8_t *>(payload), len))
                        sent_any = true;
                }
                if (set_heat)
                {
                    StaticJsonDocument<192> doc;
                    doc["cmd_id"] = 0;
                    doc["feature"] = (uint8_t)StackFeature::Thermo;
                    doc["action"] = "set";
                    JsonArray items = doc["params"]["items"].to<JsonArray>();
                    JsonObject obj = items.add<JsonObject>();
                    obj["id"] = (unsigned)it.id;
                    if (new_heat == ThermoController::kInvalidPort)
                        obj["heat"] = -1;
                    else
                        obj["heat"] = (unsigned)new_heat;
                    char payload[192] = {};
                    const size_t len = serializeJson(doc, payload, sizeof(payload));
                    if (len > 0 && web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                             reinterpret_cast<const uint8_t *>(payload), len))
                        sent_any = true;
                }
                if (set_cool)
                {
                    StaticJsonDocument<192> doc;
                    doc["cmd_id"] = 0;
                    doc["feature"] = (uint8_t)StackFeature::Thermo;
                    doc["action"] = "set";
                    JsonArray items = doc["params"]["items"].to<JsonArray>();
                    JsonObject obj = items.add<JsonObject>();
                    obj["id"] = (unsigned)it.id;
                    if (new_cool == ThermoController::kInvalidPort)
                        obj["cool"] = -1;
                    else
                        obj["cool"] = (unsigned)new_cool;
                    char payload[192] = {};
                    const size_t len = serializeJson(doc, payload, sizeof(payload));
                    if (len > 0 && web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                             reinterpret_cast<const uint8_t *>(payload), len))
                        sent_any = true;
                }
                if (set_button)
                {
                    StaticJsonDocument<192> doc;
                    doc["cmd_id"] = 0;
                    doc["feature"] = (uint8_t)StackFeature::Thermo;
                    doc["action"] = "set";
                    JsonArray items = doc["params"]["items"].to<JsonArray>();
                    JsonObject obj = items.add<JsonObject>();
                    obj["id"] = (unsigned)it.id;
                    if (new_button == ThermoController::kInvalidPort)
                        obj["button"] = -1;
                    else
                        obj["button"] = (unsigned)new_button;
                    char payload[192] = {};
                    const size_t len = serializeJson(doc, payload, sizeof(payload));
                    if (len > 0 && web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                             reinterpret_cast<const uint8_t *>(payload), len))
                        sent_any = true;
                }

                if (sent_any)
                {
                    changed = true;
                    if (cache_mut && cache_mut->items)
                    {
                        for (size_t k = 0; k < cache_mut->item_count; ++k)
                        {
                            auto &dst = cache_mut->items[k];
                            if (dst.id != it.id)
                                continue;
                            if (set_power)
                                dst.power_on = new_power_on;
                            if (set_group)
                                dst.group_id = new_group_id;
                            if (set_mode)
                            {
                                const char *mode_name = ThermoController::modeName(new_mode);
                                size_t m = 0;
                                for (; m + 1 < sizeof(dst.mode) && mode_name[m]; ++m)
                                    dst.mode[m] = mode_name[m];
                                dst.mode[m] = '\0';
                            }
                            if (set_target)
                                dst.target = new_target;
                            if (set_hyst)
                                dst.hyst = new_hyst;
                            if (set_heat)
                                dst.heat = new_heat;
                            if (set_cool)
                                dst.cool = new_cool;
                            if (set_button)
                                dst.button = new_button;
                            // Update visual state immediately so UI does not lag until next stack poll/ack.
                            bool eff_power = dst.power_on;
                            if (set_power)
                                eff_power = new_power_on;
                            ThermoController::Mode eff_mode = ThermoController::Mode::Off;
                            (void)web.parseThermoMode_(String(dst.mode), eff_mode);
                            if (set_mode)
                                eff_mode = new_mode;
                            if (!eff_power || eff_mode == ThermoController::Mode::Off)
                            {
                                dst.heat_on = false;
                                dst.cool_on = false;
                            }
                            else if (eff_mode == ThermoController::Mode::Heat)
                            {
                                dst.heat_on = true;
                                dst.cool_on = false;
                            }
                            else if (eff_mode == ThermoController::Mode::Cool)
                            {
                                dst.heat_on = false;
                                dst.cool_on = true;
                            }
                            cache_mut->updated_ms = millis();
                            cache_mut->has_data = true;
                            break;
                        }
                    }
                }
            }
            if (changed)
            {
                web._stack_cache->requestThermo(node_id);
                web.refreshStackPorts_(node_id);
            }
            web._thermo_status = changed ? "Updated" : "No changes";
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        ThermoController &thermo = web._controllers->thermo();
        if (!web.webSessionIsAdmin_())
        {
            bool power_changed = false;
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = thermo.configByIndex(i);
                if (!cfg)
                    continue;
                const String idx = String((unsigned)cfg->id);
                const String power_key = String("t") + idx + "_power";
                if (!request->hasParam(power_key, true))
                    continue;
                if (!web.webAclCanControlItem_(UsersRegistry::AclController::Thermo, cfg->id))
                {
                    web._thermo_status = String("ACL deny item: ") + idx;
                    web.sendRedirect_(request, "/thermo", set_cookie);
                    return;
                }
                const String power_str = web.paramValue_(request, power_key);
                const bool has_power = (power_str == "on" || power_str == "off" || power_str == "1" || power_str == "0" ||
                                        power_str == "true" || power_str == "false");
                if (!has_power)
                    continue;
                const bool power_on = (power_str == "on" || power_str == "1" || power_str == "true");
                if (thermo.setPower(cfg->id, power_on, "web"))
                    power_changed = true;
            }
            web._thermo_status = power_changed ? "Updated" : "No changes";
            web.sendRedirect_(request, "/thermo", set_cookie);
            return;
        }
        uint8_t sensor_used[MeteoController::kSensorCount + 1] = {};
        uint32_t remote_used[ThermoController::kDeviceCount] = {};
        size_t remote_used_count = 0;
        bool ok = true;
        bool changed = false;
        bool power_changed = false;
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("t") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String sensor_key = prefix + "sensor";
            const String mode_key = prefix + "mode";
            const String target_key = prefix + "target";
            const String hyst_key = prefix + "hyst";
            const String heat_key = prefix + "heat";
            const String cool_key = prefix + "cool";
            const String button_key = prefix + "button";
            const String group_key = prefix + "group";
            const String power_key = prefix + "power";
            const String en_force_key = prefix + "en_force";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(sensor_key, true) ||
                                 request->hasParam(mode_key, true) ||
                                 request->hasParam(target_key, true) ||
                                 request->hasParam(hyst_key, true) ||
                                 request->hasParam(heat_key, true) ||
                                 request->hasParam(cool_key, true) ||
                                 request->hasParam(button_key, true) ||
                                 request->hasParam(group_key, true) ||
                                 request->hasParam(power_key, true);
            if (!has_any)
                continue;
            if (!web.webAclCanControlItem_(UsersRegistry::AclController::Thermo, cfg->id))
            {
                web._thermo_status = String("ACL deny item: ") + idx;
                web.sendRedirect_(request, "/thermo", set_cookie);
                return;
            }

            const String en_force_str = web.paramValue_(request, en_force_key);
            const bool en_force_known = (en_force_str == "1" || en_force_str == "0" ||
                                         en_force_str == "true" || en_force_str == "false" ||
                                         en_force_str == "on" || en_force_str == "off");
            const bool en_force_on = (en_force_str == "1" || en_force_str == "true" || en_force_str == "on");
            const bool enabled = en_force_known ? en_force_on : request->hasParam(en_key, true);
            if (!enabled)
            {
                if (cfg->enabled != enabled)
                {
                    thermo.setEnabled(cfg->id, enabled);
                    changed = true;
                }
                continue;
            }
            const String sensor_str = web.paramValue_(request, sensor_key);
            const String mode_str = web.paramValue_(request, mode_key);
            const String target_str = web.paramValue_(request, target_key);
            const String hyst_str = web.paramValue_(request, hyst_key);
            const String heat_str = web.paramValue_(request, heat_key);
            const String cool_str = web.paramValue_(request, cool_key);
            const String button_str = web.paramValue_(request, button_key);
            const uint8_t group_id = web.parseGroupIdParam_(request, group_key);
            const String power_str = web.paramValue_(request, power_key);
            String name = web.paramValue_(request, name_key);
            name.trim();
            const bool has_power = (power_str == "on" || power_str == "off" || power_str == "1" || power_str == "0" ||
                                    power_str == "true" || power_str == "false");
            const bool power_on = (power_str == "on" || power_str == "1" || power_str == "true");

            uint8_t sensor_id = ThermoController::kInvalidSensor;
            uint32_t sensor_node_id = 0;
            if (!web.parseThermoSensor_(sensor_str, sensor_id, sensor_node_id))
            {
                ok = false;
                web._thermo_status = String("Invalid sensor for device ") + idx;
                break;
            }
            if (enabled && sensor_id != ThermoController::kInvalidSensor)
            {
                if (sensor_node_id == 0)
                {
                    if (!web.isMeteoSensorActive_(sensor_id))
                    {
                        ok = false;
                        web._thermo_status = String("?????? ?? ??????? (") + idx + ")";
                        break;
                    }
                    if (sensor_id <= MeteoController::kSensorCount && sensor_used[sensor_id])
                    {
                        ok = false;
                        web._thermo_status = String("?????? ??? ???????????? (") + idx + ")";
                        break;
                    }
                    if (sensor_id <= MeteoController::kSensorCount)
                        sensor_used[sensor_id] = 1;
                }
                else
                {
                    if (web.stackRole_() != ConfigsManagerIface::StackRole::Master &&
                        web.stackRole_() != ConfigsManagerIface::StackRole::Slave)
                    {
                        ok = false;
                        web._thermo_status = String("?????? ?????????? (") + idx + ")";
                        break;
                    }
                    if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && !web._stack_slave)
                    {
                        ok = false;
                        web._thermo_status = String("?????? ?????????? (") + idx + ")";
                        break;
                    }
                    if (!web.isRemoteMeteoSensorActive_(sensor_node_id, sensor_id))
                    {
                        ok = false;
                        web._thermo_status = String("?????? ?? ??????? (") + idx + ")";
                        break;
                    }
                    const uint32_t key = (sensor_node_id << 8) | sensor_id;
                    bool used = false;
                    for (size_t k = 0; k < remote_used_count; ++k)
                    {
                        if (remote_used[k] == key)
                        {
                            used = true;
                            break;
                        }
                    }
                    if (used)
                    {
                        ok = false;
                        web._thermo_status = String("?????? ??? ???????????? (") + idx + ")";
                        break;
                    }
                    if (remote_used_count < ThermoController::kDeviceCount)
                        remote_used[remote_used_count++] = key;
                }
            }

            ThermoController::Mode mode = ThermoController::Mode::Off;
            if (!web.parseThermoMode_(mode_str, mode))
            {
                ok = false;
                web._thermo_status = String("Invalid mode for device ") + idx;
                break;
            }

            float target = cfg->target_c;
            if (!web.parseThermoFloat_(target_str, target))
            {
                ok = false;
                web._thermo_status = String("Invalid target for device ") + idx;
                break;
            }

            float hyst = cfg->hysteresis;
            if (!web.parseThermoFloat_(hyst_str, hyst))
            {
                ok = false;
                web._thermo_status = String("Invalid hyst for device ") + idx;
                break;
            }

            uint8_t heat_port = ThermoController::kInvalidPort;
            uint8_t cool_port = ThermoController::kInvalidPort;
            uint8_t button_port = ThermoController::kInvalidPort;
            if (!web.parseSocketPort_(heat_str, heat_port) ||
                !web.parseSocketPort_(cool_str, cool_port) ||
                !web.parseSocketPort_(button_str, button_port))
            {
                ok = false;
                web._thermo_status = String("Invalid port for device ") + idx;
                break;
            }

            if (cfg->enabled != enabled)
            {
                thermo.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->name != name)
            {
                thermo.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->group_id != group_id)
            {
                thermo.setGroupId(cfg->id, group_id);
                changed = true;
            }
            if (cfg->sensor_id != sensor_id || cfg->sensor_node_id != sensor_node_id)
            {
                if (sensor_node_id != 0)
                    thermo.setSensorSource(cfg->id, sensor_node_id, sensor_id);
                else
                    thermo.setSensor(cfg->id, sensor_id);
                changed = true;
            }
            if (cfg->mode != mode)
            {
                thermo.setMode(cfg->id, mode);
                changed = true;
            }
            if (cfg->target_c != target)
            {
                thermo.setTarget(cfg->id, target);
                changed = true;
            }
            if (cfg->hysteresis != hyst)
            {
                thermo.setHysteresis(cfg->id, hyst);
                changed = true;
            }
            if (cfg->heat_port != heat_port)
            {
                thermo.setHeatPort(cfg->id, heat_port);
                changed = true;
            }
            if (cfg->cool_port != cool_port)
            {
                thermo.setCoolPort(cfg->id, cool_port);
                changed = true;
            }
            if (cfg->button_port != button_port)
            {
                thermo.setButtonPort(cfg->id, button_port);
                changed = true;
            }
            if (has_power)
            {
                const auto *st = thermo.state(cfg->id);
                const bool cur_power = st ? st->power_on : true;
                if (cur_power != power_on)
                {
                    thermo.setPower(cfg->id, power_on, "web");
                    power_changed = true;
                }
            }
        }

        if (ok)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._thermo_status = "Config manager missing";
            }
            else if (changed && !web._configs_manager->save())
            {
                ok = false;
                web._thermo_status = "Save failed";
            }
        }
        if (ok)
            web._thermo_status = (changed || power_changed) ? "Updated" : "Saved";
        web.sendRedirect_(request, "/thermo", set_cookie);
    }

    static void handleThermoToggle(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Thermo, node_id))
            return;

        const String id_str = web.paramValueAny_(request, "id");
        if (!id_str.length())
        {
            web.sendText_(request, 400, "text/plain", "Missing id", set_cookie);
            return;
        }
        const uint16_t id = (uint16_t)id_str.toInt();
        if (id == 0)
        {
            web.sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Thermo, id, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }

        String action = web.paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        auto send_state = [&](bool power_on, bool heat_on, bool cool_on) {
            StaticJsonDocument<96> out;
            out["power"] = power_on;
            out["heat"] = heat_on;
            out["cool"] = cool_on;
            String body;
            serializeJson(out, body);
            web.sendText_(request, 200, "application/json", body, set_cookie);
        };

        if (web.isStackThermoView_(node_id))
        {
            if (!web._stack_master)
            {
                web.sendText_(request, 400, "text/plain", "Stack master missing", set_cookie);
                return;
            }
            auto *cache = web._stack_cache ? web._stack_cache->thermoCache(node_id) : nullptr;
            StackCache::StackThermoItem *item = nullptr;
            if (cache && cache->items)
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    if (cache->items[i].id == id)
                    {
                        item = &cache->items[i];
                        break;
                    }
                }
            }

            if (action == "state")
            {
                if (!cache || !cache->has_data)
                {
                    if (web._stack_cache)
                        web._stack_cache->requestThermo(node_id);
                    web.sendText_(request, 200, "text/plain", "pending", set_cookie);
                    return;
                }
                const bool stale = (cache->pending || (uint32_t)(millis() - cache->updated_ms) > 1500u);
                if (stale)
                {
                    if (web._stack_cache)
                        web._stack_cache->requestThermo(node_id);
                    web.sendText_(request, 200, "text/plain", "pending", set_cookie);
                    return;
                }
                if (!item)
                {
                    web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
                    return;
                }
                send_state(item->power_on, item->heat_on, item->cool_on);
                return;
            }

            StaticJsonDocument<192> doc;
            doc["cmd_id"] = 0;
            doc["feature"] = (uint8_t)StackFeature::Thermo;
            doc["action"] = "set";
            JsonArray items = doc["params"]["items"].to<JsonArray>();
            JsonObject o = items.add<JsonObject>();
            o["id"] = id;
            if (action == "on" || action == "off")
            {
                o["power"] = (action == "on");
            }
            else
            {
                o["toggle"] = true;
            }
            char payload[192] = {};
            const size_t len = serializeJson(doc, payload, sizeof(payload));
            if (len == 0 || !web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                       reinterpret_cast<const uint8_t *>(payload), len))
            {
                web.sendText_(request, 400, "text/plain", "Send failed", set_cookie);
                return;
            }
            if (cache && item)
            {
                bool new_power = item->power_on;
                if (action == "on")
                    new_power = true;
                else if (action == "off")
                    new_power = false;
                else
                    new_power = !item->power_on;
                item->power_on = new_power;
                ThermoController::Mode mode = ThermoController::Mode::Off;
                (void)web.parseThermoMode_(String(item->mode), mode);
                if (!new_power || mode == ThermoController::Mode::Off)
                {
                    item->heat_on = false;
                    item->cool_on = false;
                }
                else if (mode == ThermoController::Mode::Heat)
                {
                    item->heat_on = true;
                    item->cool_on = false;
                }
                else if (mode == ThermoController::Mode::Cool)
                {
                    item->heat_on = false;
                    item->cool_on = true;
                }
                cache->updated_ms = millis();
                cache->has_data = true;
            }
            if (web._stack_cache)
                web._stack_cache->requestThermo(node_id);
            if (item)
            {
                send_state(item->power_on, item->heat_on, item->cool_on);
                return;
            }
            web.sendText_(request, 200, "text/plain", "pending", set_cookie);
            return;
        }

        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        ThermoController &thermo = web._controllers->thermo();
        if (!thermo.config(id))
        {
            web.sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        if (action == "state")
        {
            const auto *st = thermo.state(id);
            if (!st)
            {
                web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
                return;
            }
            send_state(st->power_on, st->heat_on, st->cool_on);
            return;
        }
        bool ok = false;
        if (action == "on")
            ok = thermo.setPower(id, true, "web");
        else if (action == "off")
            ok = thermo.setPower(id, false, "web");
        else
            ok = thermo.togglePower(id, "web");
        if (!ok)
        {
            web.sendText_(request, 400, "text/plain", "Toggle failed", set_cookie);
            return;
        }
        const auto *st = thermo.state(id);
        if (!st)
        {
            web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
            return;
        }
        send_state(st->power_on, st->heat_on, st->cool_on);
    }
};
