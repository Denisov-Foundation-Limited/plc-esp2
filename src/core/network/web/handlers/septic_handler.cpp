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

#include "core/network/web/handlers/septic_handler.hpp"

#include "core/network/web/web_interface.hpp"

void SepticHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/septic/list", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleSepticList(web, request); });
        server.on("/septic/toggle", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleSepticToggle(web, request); });
        server.on("/septic/toggle", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleSepticToggle(web, request); });
        server.on("/septic", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleSepticSave(web, request); });
        server.on("/septic", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleSeptic(web, request); });
    }

void SepticHandler::handleSeptic(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Septic, node_id))
            return;
        const bool stack_view = web.isStackSepticView_(node_id);
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
            web.requestStackSeptic_(node_id);
            web.requestStackPorts_(node_id);
            if (!groups_available)
            {
                const size_t visible = web.stackSepticVisibleCount_(node_id);
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
            const size_t visible = web.septicLocalRenderCount_();
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        else
        {
            page_idx = 0;
            max_pages = 1;
        }
        String page = FPSTR(kWebInterfaceSepticHtml);
        page.reserve(page.length() + 4096);
        String pagination = "";
        if (stack_view && max_pages > 1)
        {
            pagination.reserve(256);
            pagination += "<div class=\"pagination\">";
            if (page_idx > 0)
            {
                pagination += "<a class=\"page-btn\" href=\"/septic?unit=stack&node=";
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
                pagination += "<a class=\"page-btn\" href=\"/septic?unit=stack&node=";
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
                pagination += "<a class=\"page-btn\" href=\"/septic?page=";
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
                pagination += "<a class=\"page-btn\" href=\"/septic?page=";
                pagination += String((unsigned)(page_idx + 2u));
                pagination += String("\">") + WebUiRu::Common::kPageNext + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPageNext + "</span>";
            pagination += "</div>";
        }
        page.replace("%NAV%", web.navHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        page.replace("%SEPTIC_PAGE_TITLE%", WebUiRu::Septic::kPageTitle);
        page.replace("%SEPTIC_WATER_LABEL_20%", WebUiRu::Septic::kText20);
        page.replace("%SEPTIC_WATER_LABEL_80%", WebUiRu::Septic::kText80);
        page.replace("%SEPTIC_WATER_LABEL_100%", WebUiRu::Septic::kText100);
        page.replace("%SEPTIC_STATUS%", stack_view ? web.stackSepticStatusText_(node_id) : web._septic_status);
        page.replace("%SEPTIC_DEVICE_SELECT%",
                     web.composeTopFiltersHtml_(web.septicDeviceSelectHtml_(node_id, stack_view),
                                                groups_available ? web.groupFilterHtml_("septic-group-filter", stack_view ? node_id : 0u) : String("")));
        page.replace("%SEPTIC_PAGINATION%", pagination);
        page.replace("%SEPTIC_SAVE_BTN%",
                     web.webSessionIsAdmin_() ? (String("<button type=\"submit\">") + WebUiRu::kSave + "</button>") : String(""));
        if (!web._controllers)
        {
            page.replace("%SEPTIC_ITEMS%", WebUiRu::Septic::kText9);
            page.replace("%SEPTIC_DINPUT_JSON%", "[]");
            page.replace("%SEPTIC_RELAY_JSON%", "[]");
            page.replace("%SEPTIC_DINPUT_USED_JSON%", "[]");
            page.replace("%SEPTIC_RELAY_USED_JSON%", "[]");
            page.replace("%SEPTIC_WARN_CLASS%", "status-off");
            page.replace("%SEPTIC_ALARM_CLASS%", "status-off");
            page.replace("%SEPTIC_RELAY_WARN_CLASS%", "status-off");
            page.replace("%SEPTIC_RELAY_ALARM_CLASS%", "status-off");
            page.replace("%SEPTIC_WARN_LABEL%", "off");
            page.replace("%SEPTIC_ALARM_LABEL%", "off");
            page.replace("%SEPTIC_RELAY_WARN_LABEL%", "off");
            page.replace("%SEPTIC_RELAY_ALARM_LABEL%", "off");
            page.replace("%SEPTIC_WATER_CLASS%", "water-low");
            page.replace("%SEPTIC_WATER_LEVEL%", "20%");
            page.replace("%SEPTIC_WATER_LABEL%", WebUiRu::Septic::kText20);
            web.sendHtml_(request, page, set_cookie);
            return;
        }
        const String initial_html = stack_view
                                        ? web.listStackSepticHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                                   groups_available ? SIZE_MAX : page_size)
                                        : web.listSepticHtml_(groups_available ? 0u : (size_t)page_idx * page_size,
                                                              groups_available ? SIZE_MAX : page_size);
        page.replace("%SEPTIC_ITEMS%", initial_html);
        page.replace("%SEPTIC_DINPUT_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::DInput)
                                                        : web.septicPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%SEPTIC_RELAY_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::Relay)
                                                       : web.septicPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%SEPTIC_DINPUT_USED_JSON%",
                     stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::DInput)
                                : web.globalUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%SEPTIC_RELAY_USED_JSON%",
                     stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::Relay)
                                : web.globalUsedPortsJson_(PortIO::PinType::Relay));
        if (!stack_view)
        {
            SepticController &septic = web._controllers->septic();
            auto septic_guard = septic.lockGuard();
            const auto *cfg = septic.configByIndex(0);
            const auto *st = septic.stateByIndex(0);
            const bool warn = st ? st->warning : false;
            const bool alarm = st ? st->alarm : false;
            const bool relay_warn = st ? st->relay_warning : false;
            const bool relay_alarm = st ? st->relay_alarm : false;
            const char *water_class = "water-low";
            const char *water_level = "20%";
            const char *water_label = WebUiRu::Septic::kText20;
            if (alarm)
            {
                water_class = "water-alarm";
                water_level = "100%";
                water_label = WebUiRu::Septic::kText100;
            }
            else if (warn)
            {
                water_class = "water-warn";
                water_level = "80%";
                water_label = WebUiRu::Septic::kText80;
            }
            page.replace("%SEPTIC_WARN_CLASS%", warn ? "status-on" : "status-off");
            page.replace("%SEPTIC_ALARM_CLASS%", alarm ? "status-on" : "status-off");
            page.replace("%SEPTIC_RELAY_WARN_CLASS%", relay_warn ? "status-on" : "status-off");
            page.replace("%SEPTIC_RELAY_ALARM_CLASS%", relay_alarm ? "status-on" : "status-off");
            page.replace("%SEPTIC_WARN_LABEL%", warn ? "on" : "off");
            page.replace("%SEPTIC_ALARM_LABEL%", alarm ? "on" : "off");
            page.replace("%SEPTIC_RELAY_WARN_LABEL%", relay_warn ? "on" : "off");
            page.replace("%SEPTIC_RELAY_ALARM_LABEL%", relay_alarm ? "on" : "off");
            page.replace("%SEPTIC_WATER_CLASS%", water_class);
            page.replace("%SEPTIC_WATER_LEVEL%", water_level);
            page.replace("%SEPTIC_WATER_LABEL%", water_label);
            if (!cfg || !cfg->enabled)
                page.replace("%SEPTIC_STATUS%", WebUiRu::Septic::kDisabledStatus);
        }
        web.sendHtml_(request, page, set_cookie);
    }

void SepticHandler::handleSepticList(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Septic, node_id))
            return;
        const bool stack_view = web.isStackSepticView_(node_id);
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
                const size_t visible = web.stackSepticVisibleCount_(node_id);
                const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
                if (page_idx >= max_pages)
                    page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            }
            web.sendText_(request, 200, "text/html; charset=utf-8",
                          web.listStackSepticHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                   groups_available ? SIZE_MAX : page_size),
                          set_cookie);
            return;
        }
        if (!groups_available)
        {
            const size_t visible = web.septicLocalRenderCount_();
            const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        web.sendText_(request, 200, "text/html; charset=utf-8",
                      web.listSepticHtml_(groups_available ? 0u : (size_t)page_idx * page_size,
                                          groups_available ? SIZE_MAX : page_size),
                      set_cookie);
    }

void SepticHandler::handleSepticSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Septic))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackSepticView_(node_id))
        {
            String back = String("/septic?unit=stack&node=") + String((unsigned long)node_id);
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
            web._septic_status = "not migrated";
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        SepticController &septic = web._controllers->septic();
        auto septic_guard = septic.lockGuard();
        bool ok = true;
        bool changed = false;
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = septic.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("sep") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String warn_key = prefix + "warn";
            const String alarm_key = prefix + "alarm";
            const String relay_warn_key = prefix + "relay_warn";
            const String relay_alarm_key = prefix + "relay_alarm";
            const String monitor_key = prefix + "mon";
            const String group_key = prefix + "group";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(warn_key, true) ||
                                 request->hasParam(alarm_key, true) ||
                                 request->hasParam(relay_warn_key, true) ||
                                 request->hasParam(relay_alarm_key, true) ||
                                 request->hasParam(group_key, true) ||
                                 request->hasParam(monitor_key, true);
            if (!has_any)
                continue;
            if (!web.webAclCanControlItem_(UsersRegistry::AclController::Septic, cfg->id))
            {
                web._septic_status = String("ACL deny item: ") + idx;
                web.sendRedirect_(request, "/septic", set_cookie);
                return;
            }
            const bool enabled = request->hasParam(en_key, true);
            if (!enabled)
            {
                if (cfg->enabled != enabled)
                {
                    septic.setEnabled(cfg->id, enabled);
                    changed = true;
                }
                continue;
            }
            const String monitor_val = web.paramValue_(request, monitor_key);
            const bool monitoring = monitor_val == "on" || monitor_val == "1" || monitor_val == "true";
            const uint8_t group_id = web.parseGroupIdParam_(request, group_key);
            String name = web.paramValue_(request, name_key);
            String warn = web.paramValue_(request, warn_key);
            String alarm = web.paramValue_(request, alarm_key);
            String relay_warn = web.paramValue_(request, relay_warn_key);
            String relay_alarm = web.paramValue_(request, relay_alarm_key);
            name.trim();
            uint8_t warn_port = SepticController::kInvalidPort;
            uint8_t alarm_port = SepticController::kInvalidPort;
            uint8_t relay_warn_port = SepticController::kInvalidPort;
            uint8_t relay_alarm_port = SepticController::kInvalidPort;
            if (!web.parseSocketPort_(warn, warn_port) ||
                !web.parseSocketPort_(alarm, alarm_port) ||
                !web.parseSocketPort_(relay_warn, relay_warn_port) ||
                !web.parseSocketPort_(relay_alarm, relay_alarm_port))
            {
                ok = false;
                web._septic_status = String("Invalid port for septic ") + idx;
                break;
            }
            if (cfg->name != name)
                septic.setName(cfg->id, name);
            if (cfg->group_id != group_id)
                septic.setGroupId(cfg->id, group_id);
            if (cfg->warning_port != warn_port)
                septic.setWarningPort(cfg->id, warn_port);
            if (cfg->alarm_port != alarm_port)
                septic.setAlarmPort(cfg->id, alarm_port);
            if (cfg->relay_warning != relay_warn_port)
                septic.setWarningRelay(cfg->id, relay_warn_port);
            if (cfg->relay_alarm != relay_alarm_port)
                septic.setAlarmRelay(cfg->id, relay_alarm_port);
            if (cfg->enabled != enabled)
                septic.setEnabled(cfg->id, enabled);
            if (cfg->monitoring_on != monitoring)
                septic.setMonitoring(cfg->id, monitoring);
            changed = true;
        }
        if (ok)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._septic_status = "Config manager missing";
            }
            else if (changed && !web._configs_manager->save())
            {
                ok = false;
                web._septic_status = "Save failed";
            }
        }
        if (ok && changed && web._controllers)
            web._controllers->invalidateGpioUsageCache();
        if (ok)
            web._septic_status = changed ? "Updated" : "Saved";
        web.sendRedirect_(request, "/septic", set_cookie);
    }

void SepticHandler::handleSepticToggle(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Septic, node_id))
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
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Septic, id, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }

        String action = web.paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        auto send_state = [&](bool monitor_on, bool warning, bool alarm, bool relay_warning, bool relay_alarm) {
            StaticJsonDocument<160> out;
            out["monitor"] = monitor_on;
            out["warning"] = warning;
            out["alarm"] = alarm;
            out["relay_warning"] = relay_warning;
            out["relay_alarm"] = relay_alarm;
            String body;
            serializeJson(out, body);
            web.sendText_(request, 200, "application/json", body, set_cookie);
        };

        if (web.isStackSepticView_(node_id))
        {
            StackUnitSnapshot::State snapshot{};
            if (action == "state")
            {
                if (!web.network() || !web.network()->stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
                {
                    web.requestStackSeptic_(node_id);
                    web.sendText_(request, 200, "text/plain", "pending", set_cookie);
                    return;
                }
                send_state(snapshot.septic_monitoring_on,
                           snapshot.septic_warning > 0,
                           snapshot.septic_alert > 0,
                           snapshot.septic_relay_warning_on,
                           snapshot.septic_relay_alarm_on);
                return;
            }

            DynamicJsonDocument doc(96);
            doc["id"] = id;
            if (action == "on")
                doc["monitor"] = true;
            else if (action == "off")
                doc["monitor"] = false;
            else if (web.network() && web.network()->stackIndexState(node_id, snapshot) && snapshot.updated_ms != 0)
                doc["monitor"] = !snapshot.septic_monitoring_on;
            else
                doc["monitor"] = true;

            if (!web.network() ||
                !web.network()->stackRoute().sendEventSelected(web.stackPayloadMode(), node_id, "septic", "set", &doc))
            {
                web.sendText_(request, 400, "text/plain", "Send failed", set_cookie);
                return;
            }
            web.requestStackSeptic_(node_id);
            web.sendText_(request, 200, "text/plain", "pending", set_cookie);
            return;
        }

        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        SepticController &septic = web._controllers->septic();
        auto septic_guard = septic.lockGuard();
        const auto *cfg = septic.configByIndex((size_t)(id - 1));
        const auto *st = septic.stateByIndex((size_t)(id - 1));
        if (!cfg || !st)
        {
            web.sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        if (action == "state")
        {
            send_state(cfg->monitoring_on, st->warning, st->alarm, st->relay_warning, st->relay_alarm);
            return;
        }

        bool ok = false;
        if (action == "on")
            ok = septic.setMonitoring(id, true);
        else if (action == "off")
            ok = septic.setMonitoring(id, false);
        else
            ok = septic.setMonitoring(id, !cfg->monitoring_on);
        if (!ok)
        {
            web.sendText_(request, 400, "text/plain", "Toggle failed", set_cookie);
            return;
        }
        const auto *cfg2 = septic.configByIndex((size_t)(id - 1));
        const auto *st2 = septic.stateByIndex((size_t)(id - 1));
        if (!cfg2 || !st2)
        {
            web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
            return;
        }
        send_state(cfg2->monitoring_on, st2->warning, st2->alarm, st2->relay_warning, st2->relay_alarm);
    }
