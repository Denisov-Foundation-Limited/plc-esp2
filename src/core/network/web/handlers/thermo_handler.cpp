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

#include "core/network/web/handlers/thermo_handler.hpp"

#include "core/network/web/web_interface.hpp"

void ThermoHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/thermo/list", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleThermoList(web, request); });
        server.on("/thermo/toggle", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleThermoToggle(web, request); });
        server.on("/thermo/toggle", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleThermoToggle(web, request); });
        server.on("/thermo", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleThermoSave(web, request); });
        server.on("/thermo", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleThermo(web, request); });
    }

void ThermoHandler::handleThermo(WebInterface &web, AsyncWebServerRequest *request) {
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
        const String initial_html = stack_view
                                        ? web.listStackThermoHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                                   groups_available ? SIZE_MAX : page_size)
                                        : web.listThermoHtml_(groups_available ? 0u : (size_t)page_idx * page_size,
                                                              groups_available ? SIZE_MAX : page_size);
        page.replace("%THERMO_ROWS%", initial_html);
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
        if (!stack_view && web._controllers)
        {
            ThermoController &thermo = web._controllers->thermo();
            auto thermo_guard = thermo.lockGuard();
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

void ThermoHandler::handleThermoList(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
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
        if (stack_view)
        {
            if (!groups_available)
            {
                const size_t visible = web.stackThermoVisibleCount_(node_id);
                const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
                if (page_idx >= max_pages)
                    page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            }
            web.sendText_(request, 200, "text/html; charset=utf-8",
                          web.listStackThermoHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                   groups_available ? SIZE_MAX : page_size),
                          set_cookie);
            return;
        }
        if (!groups_available)
        {
            const size_t visible = web.thermoLocalRenderCount_();
            const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        web.sendText_(request, 200, "text/html; charset=utf-8",
                      web.listThermoHtml_(groups_available ? 0u : (size_t)page_idx * page_size,
                                          groups_available ? SIZE_MAX : page_size),
                      set_cookie);
    }

void ThermoHandler::handleThermoSave(WebInterface &web, AsyncWebServerRequest *request) {
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
            if (!web.network())
            {
                web._thermo_status = "Stack unavailable";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            DynamicJsonDocument doc(1536);
            doc["source"] = "localweb";
            if (const auto *u = web.sessionUser_())
                doc["source_user"] = u->username;
            JsonArray items = doc["items"].to<JsonArray>();
            for (uint8_t i = 1; i <= ThermoController::kDeviceCount; ++i)
            {
                const String idx = String((unsigned)i);
                const String prefix = String("t") + idx + "_";
                const bool has_any = request->hasParam(prefix + "en", true) ||
                                     request->hasParam(prefix + "name", true) ||
                                     request->hasParam(prefix + "sensor", true) ||
                                     request->hasParam(prefix + "mode", true) ||
                                     request->hasParam(prefix + "target", true) ||
                                     request->hasParam(prefix + "hyst", true) ||
                                     request->hasParam(prefix + "heat", true) ||
                                     request->hasParam(prefix + "cool", true) ||
                                     request->hasParam(prefix + "button", true) ||
                                     request->hasParam(prefix + "group", true) ||
                                     request->hasParam(prefix + "power", true);
                if (!has_any || !web.webAclCanControlItem_(UsersRegistry::AclController::Thermo, i, node_id))
                    continue;
                JsonObject o = items.add<JsonObject>();
                o["id"] = i;
                const String en_force_str = web.paramValue_(request, prefix + "en_force");
                const bool en_force_known = (en_force_str == "1" || en_force_str == "0" ||
                                             en_force_str == "true" || en_force_str == "false" ||
                                             en_force_str == "on" || en_force_str == "off");
                if (en_force_known)
                    o["enabled"] = (en_force_str == "1" || en_force_str == "true" || en_force_str == "on");
                const String name = web.paramValue_(request, prefix + "name");
                if (name.length())
                    o["name"] = name;
                o["group_id"] = web.parseGroupIdParam_(request, prefix + "group");
                uint8_t sensor_id = ThermoController::kInvalidSensor;
                uint32_t sensor_node_id = 0;
                const String sensor_str = web.paramValue_(request, prefix + "sensor");
                if (web.parseThermoSensor_(sensor_str, sensor_id, sensor_node_id))
                {
                    o["sensor_id"] = sensor_id;
                    o["sensor_node_id"] = sensor_node_id;
                }
                ThermoController::Mode mode = ThermoController::Mode::Off;
                if (web.parseThermoMode_(web.paramValue_(request, prefix + "mode"), mode))
                    o["mode_id"] = (uint8_t)mode;
                float target = 0.0f;
                if (web.parseThermoFloat_(web.paramValue_(request, prefix + "target"), target))
                    o["target_c"] = target;
                float hyst = 0.0f;
                if (web.parseThermoFloat_(web.paramValue_(request, prefix + "hyst"), hyst))
                    o["hyst"] = hyst;
                uint8_t heat_port = ThermoController::kInvalidPort;
                uint8_t cool_port = ThermoController::kInvalidPort;
                uint8_t button_port = ThermoController::kInvalidPort;
                if (web.parseSocketPort_(web.paramValue_(request, prefix + "heat"), heat_port))
                    o["heat_port"] = heat_port;
                if (web.parseSocketPort_(web.paramValue_(request, prefix + "cool"), cool_port))
                    o["cool_port"] = cool_port;
                if (web.parseSocketPort_(web.paramValue_(request, prefix + "button"), button_port))
                    o["button_port"] = button_port;
                const String power_str = web.paramValue_(request, prefix + "power");
                if (power_str == "on" || power_str == "off" || power_str == "1" || power_str == "0" ||
                    power_str == "true" || power_str == "false")
                    o["power_on"] = (power_str == "on" || power_str == "1" || power_str == "true");
            }
            if (items.size() == 0)
            {
                web._thermo_status = "No changes";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            if (!web.network()->stackRoute().sendEvent(node_id, "thermo", "set", &doc, StackRouteAdapter::Mode::Json))
            {
                web._thermo_status = "Send failed";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            web.requestStackThermo_(node_id);
            web.requestStackIndexState_(node_id);
            web._thermo_status = "Updated";
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        ThermoController &thermo = web._controllers->thermo();
        auto thermo_guard = thermo.lockGuard();
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

void ThermoHandler::handleThermoToggle(WebInterface &web, AsyncWebServerRequest *request) {
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
            if (!web.network())
            {
                web.sendText_(request, 400, "text/plain", "Stack unavailable", set_cookie);
                return;
            }
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::CacheState cache{};
            StackUnitSnapshot::RequestState request_state{};
            const bool has_snapshot = web.network()->stackIndexState(node_id, snapshot);
            const bool has_cache = web.network()->stackIndexCacheState(node_id, cache);
            const bool has_request = web.network()->stackIndexRequestState(node_id, request_state);
            StackUnitSnapshot::ThermoItem item{};
            const bool has_item = has_snapshot && web.network()->stackIndexThermoById(node_id, (uint8_t)id, item);
            if (action == "state")
            {
                const bool stale = !has_snapshot || snapshot.updated_ms == 0 ||
                    (uint32_t)(millis() - snapshot.updated_ms) > 1500u;
                const bool partial = has_snapshot && has_cache && snapshot.thermo_enabled > cache.thermo_count;
                if (stale || partial)
                {
                    web.requestStackThermo_(node_id);
                    web.sendText_(request, 200, "text/plain", "pending", set_cookie);
                    return;
                }
                if (has_request && request_state.pending)
                {
                    web.sendText_(request, 200, "text/plain", "pending", set_cookie);
                    return;
                }
                if (!has_item)
                {
                    web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
                    return;
                }
                send_state(item.power_on, item.heat_on, item.cool_on);
                return;
            }
            StaticJsonDocument<224> doc;
            doc["source"] = "localweb";
            if (const auto *u = web.sessionUser_())
                doc["source_user"] = u->username;
            JsonArray items = doc["items"].to<JsonArray>();
            JsonObject o = items.add<JsonObject>();
            o["id"] = id;
            if (action == "on")
                o["power_on"] = true;
            else if (action == "off")
                o["power_on"] = false;
            else
                o["toggle"] = true;
            if (!web.network()->stackRoute().sendEvent(node_id, "thermo", "set", &doc, StackRouteAdapter::Mode::Json))
            {
                web.sendText_(request, 400, "text/plain", "Send failed", set_cookie);
                return;
            }
            web.requestStackThermo_(node_id);
            web.requestStackIndexState_(node_id);
            web.sendText_(request, 200, "text/plain", "OK", set_cookie);
            return;
        }

        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        ThermoController &thermo = web._controllers->thermo();
        auto thermo_guard = thermo.lockGuard();
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
