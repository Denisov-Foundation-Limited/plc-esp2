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

#include "core/network/web/handlers/tank_handler.hpp"

#include "core/network/web/web_interface.hpp"

void TankHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/tanks/list", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleTanksList(web, request); });
        server.on("/tanks/toggle", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleTanksToggle(web, request); });
        server.on("/tanks/toggle", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleTanksToggle(web, request); });
        server.on("/tanks", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleTanksSave(web, request); });
        server.on("/tanks", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleTanks(web, request); });
    }

void TankHandler::handleTanks(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Tanks, node_id))
            return;
        const bool stack_view = web.isStackTanksView_(node_id);
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
            web.requestStackTanks_(node_id);
            web.requestStackPorts_(node_id);
            if (!groups_available)
            {
                const size_t visible = web.stackTanksVisibleCount_(node_id);
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
            const size_t visible = web.tanksLocalRenderCount_();
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        else
        {
            page_idx = 0;
            max_pages = 1;
        }
        String page = FPSTR(kWebInterfaceTanksHtml);
        page.reserve(page.length() + 16384);
        String pagination = "";
        if (stack_view && max_pages > 1)
        {
            pagination.reserve(256);
            pagination += "<div class=\"pagination\">";
            if (page_idx > 0)
            {
                pagination += "<a class=\"page-btn\" href=\"/tanks?unit=stack&node=";
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
                pagination += "<a class=\"page-btn\" href=\"/tanks?unit=stack&node=";
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
                pagination += "<a class=\"page-btn\" href=\"/tanks?page=";
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
                pagination += "<a class=\"page-btn\" href=\"/tanks?page=";
                pagination += String((unsigned)(page_idx + 2u));
                pagination += String("\">") + WebUiRu::Common::kPageNext + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPageNext + "</span>";
            pagination += "</div>";
        }
        page.replace("%NAV%", web.navHtml_());
        page.replace("%TANK_PAGE_TITLE%", WebUiRu::Tanks::kPageTitle);
        page.replace("%TANK_STATUS%", stack_view ? web.stackTanksStatusText_(node_id) : web._tanks_status);
        const String initial_html = stack_view
                                        ? web.listStackTanksHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                                  groups_available ? SIZE_MAX : page_size)
                                        : web.listTanksHtml_(groups_available ? 0u : (size_t)page_idx * page_size,
                                                             groups_available ? SIZE_MAX : page_size);
        page.replace("%TANK_ITEMS%", initial_html);
        page.replace("%TANK_PAGINATION%", pagination);
        page.replace("%TANK_DINPUT_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::DInput)
                                                      : web.tankPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%TANK_RELAY_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::Relay)
                                                     : web.tankPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%TANK_DINPUT_USED_JSON%",
                     stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::DInput)
                                : web.globalUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%TANK_RELAY_USED_JSON%",
                     stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::Relay)
                                : web.globalUsedPortsJson_(PortIO::PinType::Relay));
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
            page.replace("%TANK_FORM_HIDDEN%", hidden);
        }
        else
        {
            page.replace("%TANK_FORM_HIDDEN%", "");
        }
        page.replace("%TANK_DEVICE_SELECT%",
                     web.composeTopFiltersHtml_(web.tanksDeviceSelectHtml_(node_id, stack_view),
                                                groups_available ? web.groupFilterHtml_("tanks-group-filter", stack_view ? node_id : 0u) : String("")));
        page.replace("%TANK_SAVE_BTN%",
                     web.webSessionIsAdmin_() ? (String("<button type=\"submit\">") + WebUiRu::kSave + "</button>") : String(""));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

void TankHandler::handleTanksList(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Tanks, node_id))
            return;
        const bool stack_view = web.isStackTanksView_(node_id);
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
                const size_t visible = web.stackTanksVisibleCount_(node_id);
                const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
                if (page_idx >= max_pages)
                    page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            }
            web.sendText_(request, 200, "text/html; charset=utf-8",
                          web.listStackTanksHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                  groups_available ? SIZE_MAX : page_size),
                          set_cookie);
            return;
        }
        if (!groups_available)
        {
            const size_t visible = web.tanksLocalRenderCount_();
            const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        web.sendText_(request, 200, "text/html; charset=utf-8",
                      web.listTanksHtml_(groups_available ? 0u : (size_t)page_idx * page_size,
                                         groups_available ? SIZE_MAX : page_size),
                      set_cookie);
    }

void TankHandler::handleTanksSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Tanks))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackTanksView_(node_id))
        {
            String back = String("/tanks?unit=stack&node=") + String((unsigned long)node_id);
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
                web._tanks_status = "Stack unavailable";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            DynamicJsonDocument doc(1536);
            doc["source"] = "localweb";
            if (const auto *u = web.sessionUser_())
                doc["source_user"] = u->username;
            JsonArray items = doc["items"].to<JsonArray>();
            for (uint8_t i = 1; i <= TankController::kTankCount; ++i)
            {
                const String idx = String((unsigned)i);
                const String prefix = String("k") + idx + "_";
                const bool has_any = request->hasParam(prefix + "en", true) ||
                                     request->hasParam(prefix + "name", true) ||
                                     request->hasParam(prefix + "low", true) ||
                                     request->hasParam(prefix + "mid", true) ||
                                     request->hasParam(prefix + "full", true) ||
                                     request->hasParam(prefix + "valve", true) ||
                                     request->hasParam(prefix + "pump", true) ||
                                     request->hasParam(prefix + "alarm", true) ||
                                     request->hasParam(prefix + "group", true) ||
                                     request->hasParam(prefix + "power", true);
                if (!has_any || !web.webAclCanControlItem_(UsersRegistry::AclController::Tanks, i, node_id))
                    continue;
                JsonObject o = items.add<JsonObject>();
                o["id"] = i;
                o["enabled"] = request->hasParam(prefix + "en", true);

                String name = web.paramValue_(request, prefix + "name");
                name.trim();
                o["name"] = name;
                o["group_id"] = web.parseGroupIdParam_(request, prefix + "group");

                uint8_t low_port = TankController::kInvalidPort;
                uint8_t mid_port = TankController::kInvalidPort;
                uint8_t full_port = TankController::kInvalidPort;
                uint8_t valve_port = TankController::kInvalidPort;
                uint8_t pump_port = TankController::kInvalidPort;
                uint8_t alarm_port = TankController::kInvalidPort;
                if (web.parseSocketPort_(web.paramValue_(request, prefix + "low"), low_port))
                    o["low"] = low_port;
                if (web.parseSocketPort_(web.paramValue_(request, prefix + "mid"), mid_port))
                    o["mid"] = mid_port;
                if (web.parseSocketPort_(web.paramValue_(request, prefix + "full"), full_port))
                    o["full"] = full_port;
                if (web.parseSocketPort_(web.paramValue_(request, prefix + "valve"), valve_port))
                    o["valve"] = valve_port;
                if (web.parseSocketPort_(web.paramValue_(request, prefix + "pump"), pump_port))
                    o["pump"] = pump_port;
                if (web.parseSocketPort_(web.paramValue_(request, prefix + "alarm"), alarm_port))
                    o["alarm"] = alarm_port;

                const String power_str = web.paramValue_(request, prefix + "power");
                if (power_str == "on" || power_str == "off" || power_str == "1" || power_str == "0" ||
                    power_str == "true" || power_str == "false")
                    o["power_on"] = (power_str == "on" || power_str == "1" || power_str == "true");
            }
            if (items.size() == 0)
            {
                web._tanks_status = "No changes";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            if (!web.network()->stackRoute().sendEvent(node_id, "tanks", "set", &doc, StackRouteAdapter::Mode::Json))
            {
                web._tanks_status = "Send failed";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            web.requestStackTanks_(node_id);
            web.requestStackIndexState_(node_id);
            web._tanks_status = "Updated";
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", WebUiRu::Common::kControllersUnavailable, set_cookie);
            return;
        }
        TankController &tanks = web._controllers->tanks();
        auto tanks_guard = tanks.lockGuard();
        bool ok = true;
        bool changed = false;
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = tanks.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("k") + idx + "_";
            const String en_key = prefix + "en";
            const String power_key = prefix + "power";
            const String name_key = prefix + "name";
            const String low_key = prefix + "low";
            const String mid_key = prefix + "mid";
            const String full_key = prefix + "full";
            const String valve_key = prefix + "valve";
            const String pump_key = prefix + "pump";
            const String alarm_key = prefix + "alarm";
            const String group_key = prefix + "group";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(power_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(low_key, true) ||
                                 request->hasParam(mid_key, true) ||
                                 request->hasParam(full_key, true) ||
                                 request->hasParam(valve_key, true) ||
                                 request->hasParam(pump_key, true) ||
                                 request->hasParam(group_key, true) ||
                                 request->hasParam(alarm_key, true);
            if (!has_any)
                continue;
            if (!web.webAclCanControlItem_(UsersRegistry::AclController::Tanks, cfg->id))
            {
                web._tanks_status = String("ACL deny item: ") + idx;
                web.sendRedirect_(request, "/tanks", set_cookie);
                return;
            }

            const bool enabled = request->hasParam(en_key, true);
            if (!enabled)
            {
                if (cfg->enabled != enabled)
                {
                    tanks.setEnabled(cfg->id, enabled);
                    changed = true;
                }
                continue;
            }
            const String power_str = web.paramValue_(request, power_key);
            const bool power_on = (power_str == "on" || power_str == "1" || power_str == "true");
            String name = web.paramValue_(request, name_key);
            name.trim();
            const String low_str = web.paramValue_(request, low_key);
            const String mid_str = web.paramValue_(request, mid_key);
            const String full_str = web.paramValue_(request, full_key);
            const String valve_str = web.paramValue_(request, valve_key);
            const String pump_str = web.paramValue_(request, pump_key);
            const String alarm_str = web.paramValue_(request, alarm_key);
            const uint8_t group_id = web.parseGroupIdParam_(request, group_key);

            uint8_t low_port = TankController::kInvalidPort;
            uint8_t mid_port = TankController::kInvalidPort;
            uint8_t full_port = TankController::kInvalidPort;
            uint8_t valve_port = TankController::kInvalidPort;
            uint8_t pump_port = TankController::kInvalidPort;
            uint8_t alarm_port = TankController::kInvalidPort;
            if (!web.parseSocketPort_(low_str, low_port) ||
                !web.parseSocketPort_(mid_str, mid_port) ||
                !web.parseSocketPort_(full_str, full_port) ||
                !web.parseSocketPort_(valve_str, valve_port) ||
                !web.parseSocketPort_(pump_str, pump_port) ||
                !web.parseSocketPort_(alarm_str, alarm_port))
            {
                ok = false;
                web._tanks_status = String(WebUiRu::Tanks::kInvalidPortForTankPrefix) + idx;
                break;
            }

            if (cfg->enabled != enabled)
            {
                tanks.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->power_on != power_on)
            {
                tanks.setPower(cfg->id, power_on);
                changed = true;
            }
            if (cfg->name != name)
            {
                tanks.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->group_id != group_id)
            {
                tanks.setGroupId(cfg->id, group_id);
                changed = true;
            }
            if (cfg->level_low != low_port)
            {
                tanks.setLevelLow(cfg->id, low_port);
                changed = true;
            }
            if (cfg->level_mid != mid_port)
            {
                tanks.setLevelMid(cfg->id, mid_port);
                changed = true;
            }
            if (cfg->level_full != full_port)
            {
                tanks.setLevelFull(cfg->id, full_port);
                changed = true;
            }
            if (cfg->relay_valve != valve_port)
            {
                tanks.setValveRelay(cfg->id, valve_port);
                changed = true;
            }
            if (cfg->relay_pump != pump_port)
            {
                tanks.setPumpRelay(cfg->id, pump_port);
                changed = true;
            }
            if (cfg->relay_alarm != alarm_port)
            {
                tanks.setAlarmRelay(cfg->id, alarm_port);
                changed = true;
            }
        }

        if (ok)
        {
            if (changed)
            {
                if (!web._configs_manager)
                {
                    ok = false;
                    web._tanks_status = WebUiRu::Common::kConfigManagerUnavailable;
                }
                else if (!web._configs_manager->save())
                {
                    ok = false;
                    web._tanks_status = WebUiRu::Common::kSaveFailed;
                }
            }
        }
        if (ok)
            web._tanks_status = changed ? WebUiRu::Common::kUpdated : WebUiRu::Common::kSaved;
        web.sendRedirect_(request, "/tanks", set_cookie);
    }

void TankHandler::handleTanksToggle(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Tanks, node_id))
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
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Tanks, id, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }

        String action = web.paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        auto send_state = [&](bool power_on, bool level_low, bool level_mid, bool level_full,
                              bool levels_ok, bool valve_on, bool pump_on, bool alarm_on) {
            StaticJsonDocument<224> out;
            out["power"] = power_on;
            out["level_low"] = level_low;
            out["level_mid"] = level_mid;
            out["level_full"] = level_full;
            out["levels_ok"] = levels_ok;
            out["valve"] = valve_on;
            out["pump"] = pump_on;
            out["alarm"] = alarm_on;
            String body;
            serializeJson(out, body);
            web.sendText_(request, 200, "application/json", body, set_cookie);
        };

        if (web.isStackTanksView_(node_id))
        {
            if (action == "state")
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
                StackUnitSnapshot::TankItem item{};
                const bool has_item = has_snapshot && web.network()->stackIndexTankById(node_id, (uint8_t)id, item);
                const bool stale = !has_snapshot || snapshot.updated_ms == 0 ||
                                   (uint32_t)(millis() - snapshot.updated_ms) > 1500u;
                const bool partial = has_snapshot && has_cache && snapshot.tanks_enabled > cache.tank_count;
                if (stale || partial)
                {
                    web.requestStackTanks_(node_id);
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

                StaticJsonDocument<224> out;
                out["power"] = item.power_on;
                out["level_low"] = item.level_low;
                out["level_mid"] = item.level_mid;
                out["level_full"] = item.level_full;
                out["levels_ok"] = item.levels_ok;
                out["valve"] = item.valve_on;
                out["pump"] = item.pump_on;
                out["alarm"] = item.alarm_on;
                String body;
                serializeJson(out, body);
                web.sendText_(request, 200, "application/json", body, set_cookie);
                return;
            }
            if (!web.network())
            {
                web.sendText_(request, 400, "text/plain", "Stack unavailable", set_cookie);
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
            if (!web.network()->stackRoute().sendEvent(node_id, "tanks", "set", &doc, StackRouteAdapter::Mode::Json))
            {
                web.sendText_(request, 400, "text/plain", "Send failed", set_cookie);
                return;
            }
            web.requestStackTanks_(node_id);
            web.requestStackIndexState_(node_id);
            web.sendText_(request, 200, "text/plain", "pending", set_cookie);
            return;
        }

        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        TankController &tanks = web._controllers->tanks();
        auto tanks_guard = tanks.lockGuard();
        if (!tanks.config(id))
        {
            web.sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        if (action == "state")
        {
            const auto *cfg = tanks.config(id);
            const auto *st = tanks.state(id);
            if (!cfg || !st)
            {
                web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
                return;
            }
            send_state(cfg->power_on, st->level_low, st->level_mid, st->level_full,
                       st->levels_ok, st->valve_on, st->pump_on, st->alarm_on);
            return;
        }

        bool ok = false;
        const auto *cfg = tanks.config(id);
        if (!cfg)
        {
            web.sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        if (action == "on")
            ok = tanks.setPower(id, true);
        else if (action == "off")
            ok = tanks.setPower(id, false);
        else
            ok = tanks.setPower(id, !cfg->power_on);
        if (!ok)
        {
            web.sendText_(request, 400, "text/plain", "Toggle failed", set_cookie);
            return;
        }
        const auto *cfg2 = tanks.config(id);
        const auto *st = tanks.state(id);
        if (!cfg2 || !st)
        {
            web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
            return;
        }
        send_state(cfg2->power_on, st->level_low, st->level_mid, st->level_full,
                   st->levels_ok, st->valve_on, st->pump_on, st->alarm_on);
    }
