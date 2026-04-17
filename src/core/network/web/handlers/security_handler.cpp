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

#include "core/network/web/handlers/security_handler.hpp"

#include "core/network/web/web_interface.hpp"

void SecurityHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/security/list", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleSecurityList(web, request); });
        server.on("/security/arm", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleSecurityArm(web, request); });
        server.on("/security/state", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleSecurityState(web, request); });
        server.on("/security", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleSecuritySave(web, request); });
        server.on("/security", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleSecurity(web, request); });
    }

void SecurityHandler::handleSecurityState(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Security, node_id))
            return;
        StaticJsonDocument<4096> doc;
        JsonArray items = doc["items"].to<JsonArray>();
        const bool stack_view = web.isStackSecurityView_(node_id);
        if (stack_view)
        {
            StackUnitSnapshot::State snapshot{};
            if (web.network() && web.network()->stackIndexState(node_id, snapshot))
            {
                doc["enabled"] = snapshot.security_enabled;
                doc["armed"] = snapshot.security_armed;
                doc["alarm"] = snapshot.security_alarm;
                if (web._controllers)
                {
                    SecurityController &sec = web._controllers->security();
                    const size_t count = sec.remoteDetectCount(node_id);
                    for (size_t i = 0; i < count; ++i)
                    {
                        SecurityController::RemoteDetect remote{};
                        if (!sec.remoteDetectAt(i, remote, node_id))
                            continue;
                        JsonObject o = items.add<JsonObject>();
                        o["id"] = remote.sensor_id;
                        o["enabled"] = true;
                        o["detect"] = remote.active;
                    }
                    if (count == 0 && snapshot.security_detected > 0)
                    {
                        for (uint8_t i = 0; i < snapshot.security_detect_preview_count; ++i)
                        {
                            const auto &preview = snapshot.security_detect_preview[i];
                            if (preview.id == 0)
                                continue;
                            JsonObject o = items.add<JsonObject>();
                            o["id"] = preview.id;
                            o["enabled"] = true;
                            o["detect"] = true;
                        }
                    }
                }
                doc["pending"] = false;
            }
            else
            {
                web.requestStackSecurity_(node_id);
                doc["pending"] = true;
            }
        }
        else
        {
            if (!web._controllers)
            {
                web.sendText_(request, 500, "text/plain", WebUiRu::Common::kControllersUnavailable, set_cookie);
                return;
            }
            SecurityController &sec = web._controllers->security();
            auto sec_guard = sec.lockGuard();
            doc["enabled"] = sec.controllerEnabled();
            doc["armed"] = sec.armed();
            doc["alarm"] = sec.alarmOn();
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                const auto *cfg = sec.configByIndex(i);
                const auto *st = sec.stateByIndex(i);
                if (!cfg || !st)
                    continue;
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Security, cfg->id))
                    continue;
                JsonObject o = items.add<JsonObject>();
                o["id"] = cfg->id;
                o["enabled"] = cfg->enabled;
                o["detect"] = st->active;
            }
        }
        String body;
        serializeJson(doc, body);
        web.sendText_(request, 200, "application/json", body, set_cookie);
    }

void SecurityHandler::handleSecurity(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Security, node_id))
            return;
        String page = FPSTR(kWebInterfaceSecurityHtml);
        const uint8_t page_size = 8u;
        const bool stack_view = web.isStackSecurityView_(node_id);
        const bool groups_available = stack_view ? web.hasGroups_(node_id) : web.hasGroups_();
        const String page_str = web.paramValueAny_(request, "page");
        uint8_t page_idx = 0;
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (uint8_t)(v - 1);
        }
        uint8_t max_pages = 1;
        uint8_t start = 0;
        uint8_t end = SecurityController::kSensorCount;
        if (!stack_view && !groups_available)
        {
            const size_t visible = web.securityLocalRenderCount_();
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            start = (uint8_t)(page_idx * page_size);
            end = (uint8_t)(start + page_size - 1);
        }
        else if (!stack_view)
        {
            page_idx = 0;
            max_pages = 1;
            start = 0;
            end = SecurityController::kSensorCount;
        }
        else
        {
            const size_t visible = web.stackSecurityVisibleCount_(node_id);
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        page.reserve(page.length() + 16384);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        page.replace("%SECURITY_PAGE_TITLE%", WebUiRu::Security::kPageTitle);
        page.replace("%SECURITY_LABEL_STATUS%", WebUiRu::Security::kLabelStatus);
        page.replace("%SECURITY_LABEL_ALARM%", WebUiRu::Security::kLabelAlarm);
        page.replace("%SECURITY_BTN_ARM%", WebUiRu::Security::kBtnArm);
        page.replace("%SECURITY_BTN_DISARM%", WebUiRu::Security::kBtnDisarm);
        page.replace("%SECURITY_LABEL_SIREN_PORT%", WebUiRu::Security::kLabelSirenPort);
        page.replace("%SECURITY_PAGE_PREV%", WebUiRu::Common::kPagePrev);
        page.replace("%SECURITY_PAGE_PAGE%", WebUiRu::Common::kPagePage);
        page.replace("%SECURITY_PAGE_NEXT%", WebUiRu::Common::kPageNext);
        page.replace("%SECURITY_JS_ON1%", WebUiRu::WebCore::kOnShort);
        page.replace("%SECURITY_JS_ARMED%", WebUiRu::WebCore::kArmedPhrase);
        page.replace("%SECURITY_JS_ON2%", WebUiRu::WebCore::kEnablePrefix);
        page.replace("%SECURITY_JS_UNAVAILABLE%", WebUiRu::WebCore::kUnavailable);
        page.replace("%SECURITY_JS_OFF%", WebUiRu::WebCore::kOffShort);
        page.replace("%SECURITY_JS_DISABLED_TXT%", WebUiRu::Security::kText5);
        page.replace("%SECURITY_JS_DETECT_TXT%", WebUiRu::Security::kText6);
        page.replace("%SECURITY_JS_OK_TXT%", WebUiRu::Security::kText13);
        if (!web._controllers)
        {
            page.replace("%SECURITY_ENABLED_CHECKED%", "");
            page.replace("%SECURITY_ENABLED_LABEL%", WebUiRu::ControllersPage::kUnavailable);
            page.replace("%SECURITY_ARMED_LABEL%", WebUiRu::ControllersPage::kUnavailable);
            page.replace("%SECURITY_ARMED_CHECKED%", "");
            page.replace("%SECURITY_ALARM_LABEL%", WebUiRu::ControllersPage::kUnavailable);
            page.replace("%SECURITY_GSM_LABEL%", web.gsmStatusLabel_());
            page.replace("%SECURITY_SIREN%", "");
            page.replace("%SECURITY_SENSORS%", WebUiRu::Security::kText9);
            page.replace("%SECURITY_SENSORS_PAGE%", "1");
            page.replace("%SECURITY_SENSORS_PAGES%", String((unsigned)(max_pages ? max_pages : 1)));
            page.replace("%SECURITY_SENSOR_JSON%", "[]");
            page.replace("%SECURITY_SENSOR_USED_JSON%", "[]");
            page.replace("%SECURITY_SIREN_JSON%", "[]");
            page.replace("%SECURITY_SIREN_USED_JSON%", "[]");
            page.replace("%SECURITY_STATUS%", web._security_status);
            page.replace("%SECURITY_SENSORS_TITLE%", WebUiRu::Security::kText);
            page.replace("%SECURITY_SENSORS_PAGINATION_STYLE%", "");
            page.replace("%SECURITY_SAVE_BTN%", "");
        page.replace("%SECURITY_DEVICE_SELECT%", "");
            web.sendHtml_(request, page, set_cookie);
            return;
        }

        SecurityController &sec = web._controllers->security();
        auto sec_guard = sec.lockGuard();
        bool sec_enabled = sec.controllerEnabled();
        bool sec_armed = sec.armed();
        bool sec_alarm = sec.alarmOn();
        if (stack_view && web.network())
        {
            StackUnitSnapshot::State snapshot{};
            if (web.network()->stackIndexState(node_id, snapshot))
            {
                sec_enabled = snapshot.security_enabled;
                sec_armed = snapshot.security_armed;
                sec_alarm = snapshot.security_alarm;
            }
            else
            {
                web.requestStackSecurity_(node_id);
                sec_enabled = false;
                sec_armed = false;
                sec_alarm = false;
            }
        }
        const uint8_t sec_siren = stack_view ? SecurityController::kInvalidPort : sec.sirenPort();
        page.replace("%SECURITY_ENABLED_CHECKED%", sec_enabled ? "checked" : "");
            page.replace("%SECURITY_ENABLED_LABEL%", sec_enabled ? WebUiRu::ControllersPage::kEnabledNeut
                                                                 : WebUiRu::ControllersPage::kDisabledNeut);
            page.replace("%SECURITY_ARMED_LABEL%", sec_armed ? WebUiRu::Security::kArmedOn : WebUiRu::Security::kArmedOff);
        page.replace("%SECURITY_ARMED_CHECKED%", sec_armed ? "checked" : "");
        page.replace("%SECURITY_ALARM_LABEL%", sec_alarm ? "on" : "off");
        page.replace("%SECURITY_GSM_LABEL%", web.gsmStatusLabel_());
        if (sec_siren != SecurityController::kInvalidPort)
            page.replace("%SECURITY_SIREN%", String((unsigned)sec_siren));
        else
            page.replace("%SECURITY_SIREN%", "");
        const String initial_html = stack_view
                                        ? web.listStackSecuritySensorsTiles_(node_id, (size_t)page_idx * page_size, page_size)
                                        : web.listSecuritySensorsTiles_(groups_available ? 0 : start,
                                                                        groups_available ? SecurityController::kSensorCount : end);
        page.replace("%SECURITY_SENSORS%", initial_html);
        page.replace("%SECURITY_SENSORS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%SECURITY_SENSORS_PAGES%", String((unsigned)max_pages));
        page.replace("%SECURITY_SENSOR_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::DInput)
                                                          : web.securityPortOptionsJson_());
        page.replace("%SECURITY_SENSOR_USED_JSON%", stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::DInput)
                                                               : web.globalUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%SECURITY_SIREN_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::Relay)
                                                         : web.socketPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%SECURITY_SIREN_USED_JSON%", stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::Relay)
                                                              : web.globalUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%SECURITY_STATUS%", stack_view ? web.stackSecurityStatusText_(node_id) : web._security_status);
        page.replace("%SECURITY_SENSORS_TITLE%", stack_view ? web.stackSecurityTitle_(node_id) : String(WebUiRu::Security::kText));
        page.replace("%SECURITY_SENSORS_PAGINATION_STYLE%", (!groups_available && max_pages > 1) ? "" : "style=\"display:none\"");
        page.replace("%SECURITY_SAVE_BTN%",
                     (stack_view || !web.webSessionIsAdmin_()) ? String("") : (String("<button class=\"primary\" name=\"action\" value=\"save\">") + WebUiRu::kSave + "</button>"));
        page.replace("%SECURITY_DEVICE_SELECT%",
                     web.composeTopFiltersHtml_(web.securityDeviceSelectHtml_(node_id, stack_view),
                                                groups_available ? web.groupFilterHtml_("security-group-filter", stack_view ? node_id : 0u) : String("")));
        web.sendHtml_(request, page, set_cookie);
    }

void SecurityHandler::handleSecurityList(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Security, node_id))
            return;
        const bool stack_view = web.isStackSecurityView_(node_id);
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
        uint8_t start = 0;
        uint8_t end = SecurityController::kSensorCount;
        if (!stack_view && !groups_available)
        {
            const size_t visible = web.securityLocalRenderCount_();
            const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            start = (uint8_t)(page_idx * page_size);
            end = (uint8_t)(start + page_size - 1);
        }
        else if (stack_view)
        {
            const size_t visible = web.stackSecurityVisibleCount_(node_id);
            const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        const String html = stack_view ? web.listStackSecuritySensorsTiles_(node_id, (size_t)page_idx * page_size, page_size)
                                       : web.listSecuritySensorsTiles_(groups_available ? 0 : start,
                                                                       groups_available ? SecurityController::kSensorCount : end);
        web.sendText_(request, 200, "text/html; charset=utf-8", html, set_cookie);
    }

void SecurityHandler::handleSecuritySave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Security))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackSecurityView_(node_id))
        {
            StaticJsonDocument<128> doc;
            const String action = web.paramValue_(request, "action");
            if (action == "arm")
                doc["armed"] = true;
            else if (action == "disarm")
                doc["armed"] = false;
            else if (action == "clear")
                doc["clear"] = true;
            const bool sent = web.network() &&
                              web.network()->stackRoute().sendEventSelected(web.stackPayloadMode(), node_id, "security",
                                                                            "set", &doc);
            if (sent && web.network())
                web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "controllers",
                                                                "summary_req", nullptr, true);
            String back = String("/security?unit=stack&node=") + String((unsigned long)node_id);
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
            web._security_status = sent ? "Updated" : "Stack send failed";
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web._security_status = "Security unavailable";
            web.sendRedirect_(request, "/security", set_cookie);
            return;
        }
        SecurityController &sec = web._controllers->security();
        auto sec_guard = sec.lockGuard();
        const String action = web.paramValue_(request, "action");
        if (action == "arm")
        {
            if (sec.armFrom("web", "admin"))
                web._security_status = "Armed";
            else
                web._security_status = "Security disabled";
            web.sendRedirect_(request, "/security", set_cookie);
            return;
        }
        if (action == "disarm")
        {
            sec.disarmFrom("web", "admin");
            web._security_status = "Disarmed";
            web.sendRedirect_(request, "/security", set_cookie);
            return;
        }
        if (action == "clear")
        {
            sec.clearDetect();
            web._security_status = "Detections cleared";
            web.sendRedirect_(request, "/security", set_cookie);
            return;
        }

        bool changed = false;
        if (request->hasParam("security_enabled", true))
        {
            const bool enabled = request->hasParam("security_enabled", true);
            if (sec.controllerEnabled() != enabled)
            {
                sec.setControllerEnabled(enabled);
                changed = true;
            }
        }

        String siren_str = web.paramValue_(request, "security_siren");
        siren_str.trim();
        if (siren_str.length() > 0)
        {
            uint8_t siren_port = sec.sirenPort();
            bool set_siren = false;
            if (siren_str == "none")
            {
                siren_port = SecurityController::kInvalidPort;
                set_siren = true;
            }
            else
            {
                const int v = siren_str.toInt();
                if (v >= 0 && v <= 255)
                {
                    siren_port = (uint8_t)v;
                    set_siren = true;
                }
            }
            if (set_siren && sec.sirenPort() != siren_port)
            {
                sec.setSirenPort(siren_port);
                changed = true;
            }
        }

        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = sec.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("sec") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String type_key = prefix + "type";
            const String port_key = prefix + "port";
            const String silent_key = prefix + "silent";
            const String group_key = prefix + "group";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(type_key, true) ||
                                 request->hasParam(port_key, true) ||
                                 request->hasParam(group_key, true) ||
                                 request->hasParam(silent_key, true);
            if (!has_any)
                continue;
            if (!web.webAclCanControlItem_(UsersRegistry::AclController::Security, cfg->id))
            {
                web._security_status = String("ACL deny item: ") + idx;
                web.sendRedirect_(request, "/security", set_cookie);
                return;
            }
            const bool enabled = request->hasParam(en_key, true);
            if (!enabled)
            {
                if (cfg->enabled != enabled)
                {
                    sec.setEnabled(cfg->id, enabled);
                    changed = true;
                }
                continue;
            }
            const bool silent = request->hasParam(silent_key, true);
            const uint8_t group_id = web.parseGroupIdParam_(request, group_key);
            String name = web.paramValue_(request, name_key);
            name.trim();
            SecurityController::SensorType type = SecurityController::SensorType::Pir;
            if (!web.parseSecurityType_(web.paramValue_(request, type_key), type))
            {
                web._security_status = String("Invalid type for sensor ") + idx;
                web.sendRedirect_(request, "/security", set_cookie);
                return;
            }
            uint8_t port = SecurityController::kInvalidPort;
            if (!web.parseSocketPort_(web.paramValue_(request, port_key), port))
            {
                web._security_status = String("Invalid port for sensor ") + idx;
                web.sendRedirect_(request, "/security", set_cookie);
                return;
            }
            if (cfg->enabled != enabled)
            {
                sec.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->name != name)
            {
                sec.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->group_id != group_id)
            {
                sec.setGroupId(cfg->id, group_id);
                changed = true;
            }
            if (cfg->type != type)
            {
                sec.setType(cfg->id, type);
                changed = true;
            }
            if (cfg->port != port)
            {
                sec.setPort(cfg->id, port);
                changed = true;
            }
            if (cfg->silent != silent)
            {
                sec.setSilent(cfg->id, silent);
                changed = true;
            }
        }

        bool ok = true;
        if (changed)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._security_status = "Config manager missing";
            }
            else if (!web._configs_manager->save())
            {
                ok = false;
                web._security_status = "Save failed";
            }
        }
        if (ok)
            web._security_status = changed ? "Updated" : "No changes";
        web.sendRedirect_(request, "/security", set_cookie);
    }

void SecurityHandler::handleSecurityArm(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Security))
            return;
        if (!web.webAclCanControlItem_(UsersRegistry::AclController::Security, 1))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "0", set_cookie);
            return;
        }
        SecurityController &sec = web._controllers->security();
        const String armed_str = web.paramValueAny_(request, "armed");
        if (armed_str.length() == 0)
        {
            web._security_status = "Bad request";
            web.sendText_(request, 400, "text/plain", "err", set_cookie);
            return;
        }
        const bool desired = (armed_str == "1" || armed_str == "true" || armed_str == "on");
        bool ok = true;
        if (sec.armed() != desired)
        {
            if (desired)
            {
                if (!sec.controllerEnabled())
                    sec.setControllerEnabled(true);
                ok = sec.armFrom("web", "admin");
            }
            else
            {
                sec.disarmFrom("web", "admin");
                ok = true;
            }
        }
        if (!ok)
            web._security_status = "Security disabled";
        if (desired && !sec.armed())
        {
            web._security_status = "Arm blocked";
            web.sendText_(request, 200, "text/plain", "blocked", set_cookie);
        }
        else
        {
            web.sendText_(request, 200, "text/plain", sec.armed() ? "1" : "0", set_cookie);
        }
    }
