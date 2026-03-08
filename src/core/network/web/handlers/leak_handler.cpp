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

#include "core/network/web/handlers/leak_handler.hpp"

#include "core/network/web/web_interface.hpp"

void LeakHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/leak", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleLeakSave(web, request); });
        server.on("/leak", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleLeak(web, request); });
    }

void LeakHandler::handleLeak(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Leak, node_id))
            return;
        const bool stack_view = isStackLeakView_(web, node_id);
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
        if (stack_view && web._stack_cache)
        {
            web.stackCache().requestLeak(node_id);
            const size_t visible = stackLeakVisibleCount_(web, node_id);
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        else if (!stack_view)
        {
            const size_t visible = localLeakVisibleCount_(web, node_id);
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        String page = FPSTR(kWebInterfaceLeakHtml);
        page.reserve(page.length() + 8192);
        page.replace("%LEAK_PAGE_TITLE%", WebUiRu::Leak::kPageTitle);
        page.replace("%LEAK_BTN_ACK_ALL%", WebUiRu::Leak::kBtnAckAll);
        page.replace("%LEAK_HELP_PORTS%", WebUiRu::Leak::kHelpPorts);
        String pagination = "";
        if (stack_view && max_pages > 1)
        {
            pagination.reserve(256);
            pagination += "<div class=\"pagination\">";
            if (page_idx > 0)
            {
                pagination += "<a class=\"page-btn\" href=\"/leak?unit=stack&node=";
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
                pagination += "<a class=\"page-btn\" href=\"/leak?unit=stack&node=";
                pagination += String((unsigned long)node_id);
                pagination += "&page=";
                pagination += String((unsigned)(page_idx + 2u));
                pagination += String("\">") + WebUiRu::Common::kPageNext + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPageNext + "</span>";
            pagination += "</div>";
        }
        else if (!stack_view && max_pages > 1)
        {
            pagination.reserve(256);
            pagination += "<div class=\"pagination\">";
            if (page_idx > 0)
            {
                pagination += "<a class=\"page-btn\" href=\"/leak?page=";
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
                pagination += "<a class=\"page-btn\" href=\"/leak?page=";
                pagination += String((unsigned)(page_idx + 2u));
                pagination += String("\">") + WebUiRu::Common::kPageNext + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPageNext + "</span>";
            pagination += "</div>";
        }
        page.replace("%NAV%", web.navHtml_());
        page.replace("%LEAK_DEVICE_SELECT%", leakDeviceSelectHtml_(web, node_id, stack_view));
        page.replace("%LEAK_STATUS%", stack_view ? stackLeakStatusText_(web, node_id) : web._leak_status);
        page.replace("%LEAK_PAGINATION%", pagination);
        page.replace("%LEAK_DINPUT_JSON%", web.socketPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%LEAK_RELAY_JSON%", web.socketPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%LEAK_DINPUT_USED_JSON%", stack_view ? "[]" : web.globalUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%LEAK_RELAY_USED_JSON%", stack_view ? "[]" : web.globalUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%LEAK_ROWS%", buildRows_(web, node_id, stack_view, (size_t)page_idx * page_size, page_size));
        String leak_form_action = stack_view ? leakRedirectPath_(node_id, true) : String("/leak");
        if (stack_view)
        {
            leak_form_action += "&page=";
            leak_form_action += String((unsigned)(page_idx + 1));
        }
        page.replace("%LEAK_FORM_ACTION%", leak_form_action);
        page.replace("%LEAK_ACK_FORM_ACTION%", leak_form_action);
        page.replace("%SAVE_TEXT%", WebUiRu::kSave);
        web.sendHtml_(request, page, set_cookie);
    }

void LeakHandler::handleLeakSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Leak))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = isStackLeakView_(web, node_id);
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", WebUiRu::Common::kControllersUnavailable, set_cookie);
            return;
        }
        LeakController &leak = web._controllers->leak();
        if (request->hasParam("leak_ack_all", true))
        {
            if (stack_view)
            {
                if (sendStackLeakSet_(web, node_id, nullptr, true))
                    web._leak_status = WebUiRu::Leak::kCmdSent;
                else
                    web._leak_status = WebUiRu::Leak::kSendFailed;
            }
            else
            {
                leak.ackAll();
                web._leak_status = WebUiRu::Leak::kAckDone;
            }
            web.sendRedirect_(request, leakRedirectPath_(node_id, stack_view), set_cookie);
            return;
        }
        if (!request->hasParam("leak_save", true))
        {
            web.sendRedirect_(request, leakRedirectPath_(node_id, stack_view), set_cookie);
            return;
        }

        bool changed = false;
        bool ok = true;
        String err;
        DynamicJsonDocument stack_doc(4096);
        JsonArray stack_zones = stack_doc.to<JsonArray>();
        for (size_t i = 0; i < LeakController::kZoneCount && ok; ++i)
        {
            const size_t id = i + 1;
            const LeakController::ZoneConfig *cfg = leak.config(id);
            if (!cfg)
                continue;

            const String en_name = paramName_("leak_en_", id);
            const String pwr_name = paramName_("leak_pwr_", id);
            const String al_name = paramName_("leak_al_", id);
            const String n_name = paramName_("leak_name_", id);
            const String sensor_name = paramName_("leak_sensor_", id);
            const String valve_name = paramName_("leak_valve_", id);
            const String alarm_name = paramName_("leak_alarm_", id);
            const bool has_any = request->hasParam(en_name.c_str(), true) ||
                                 request->hasParam(pwr_name.c_str(), true) ||
                                 request->hasParam(al_name.c_str(), true) ||
                                 request->hasParam(n_name.c_str(), true) ||
                                 request->hasParam(sensor_name.c_str(), true) ||
                                 request->hasParam(valve_name.c_str(), true) ||
                                 request->hasParam(alarm_name.c_str(), true);
            if (!has_any)
                continue;
            if (!web.webAclCanControlItem_(UsersRegistry::AclController::Leak, (uint16_t)id, node_id))
            {
                web._leak_status = String("ACL deny item: ") + String((unsigned)id);
                web.sendRedirect_(request, leakRedirectPath_(node_id, stack_view), set_cookie);
                return;
            }

            const bool enabled = request->hasParam(en_name.c_str(), true);
            const bool power_on = request->hasParam(pwr_name.c_str(), true);
            const bool active_low = request->hasParam(al_name.c_str(), true);
            String name = web.paramValue_(request, n_name.c_str());
            name.trim();

            uint8_t sensor = cfg->sensor_port;
            uint8_t valve = cfg->valve_port;
            uint8_t alarm = cfg->alarm_port;
            if (!web.parseSocketPort_(web.paramValue_(request, sensor_name.c_str()), sensor))
            {
                ok = false;
                err = WebUiRu::Leak::kInvalidSensorPort;
                break;
            }
            if (!web.parseSocketPort_(web.paramValue_(request, valve_name.c_str()), valve))
            {
                ok = false;
                err = WebUiRu::Leak::kInvalidValvePort;
                break;
            }
            if (!web.parseSocketPort_(web.paramValue_(request, alarm_name.c_str()), alarm))
            {
                ok = false;
                err = WebUiRu::Leak::kInvalidAlarmPort;
                break;
            }

            if (stack_view)
            {
                JsonObject z = stack_zones.add<JsonObject>();
                z["id"] = (unsigned)id;
                z["enabled"] = enabled;
                z["power_on"] = power_on;
                z["sensor_active_low"] = active_low;
                z["name"] = name;
                if (sensor != LeakController::kInvalidPort)
                    z["sensor"] = sensor;
                if (valve != LeakController::kInvalidPort)
                    z["valve"] = valve;
                if (alarm != LeakController::kInvalidPort)
                    z["alarm"] = alarm;
            }
            else
            {
                changed = leak.setEnabled(id, enabled) || changed;
                changed = leak.setPower(id, power_on) || changed;
                changed = leak.setSensorActiveLow(id, active_low) || changed;
                changed = leak.setValveOpenOnPower(id, true) || changed;
                changed = leak.setName(id, name) || changed;
                changed = leak.setSensorPort(id, sensor) || changed;
                changed = leak.setValvePort(id, valve) || changed;
                changed = leak.setAlarmPort(id, alarm) || changed;
            }
        }

        if (!ok)
        {
            web._leak_status = err;
            web.sendRedirect_(request, leakRedirectPath_(node_id, stack_view), set_cookie);
            return;
        }

        if (stack_view)
        {
            if (sendStackLeakSet_(web, node_id, &stack_zones, false))
                web._leak_status = WebUiRu::Leak::kCmdSent;
            else
                web._leak_status = WebUiRu::Leak::kSendFailed;
            web.sendRedirect_(request, leakRedirectPath_(node_id, true), set_cookie);
            return;
        }

        bool saved = true;
        if (changed)
        {
            if (!web._configs_manager)
            {
                saved = false;
                web._leak_status = WebUiRu::Common::kConfigManagerUnavailable;
            }
            else if (!web._configs_manager->save())
            {
                saved = false;
                web._leak_status = WebUiRu::Leak::kSaveError;
            }
        }
        if (saved)
            web._leak_status = changed ? WebUiRu::Common::kUpdated : WebUiRu::Common::kNoChanges;
        web.sendRedirect_(request, leakRedirectPath_(node_id, false), set_cookie);
    }

bool LeakHandler::isStackLeakView_(WebInterface &web, uint32_t node_id) {
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

String LeakHandler::leakRedirectPath_(uint32_t node_id, bool stack_view) {
        if (!stack_view)
            return "/leak";
        String path = "/leak?node=";
        path += String((unsigned long)node_id);
        path += "&unit=stack";
        return path;
    }

String LeakHandler::leakDeviceSelectHtml_(WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\" style=\"margin-bottom:10px;\">";
        html += "<label>";
        html += WebUiRu::Common::kDevice;
        html += "</label>";
        html += "<select id=\"leak-device\" class=\"field\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = web._stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = web._stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = web._stack_master->nodeNameAt(i);
            if (name.length() > 0)
                web.appendHtmlEscaped_(html, name.c_str());
            else
                html += web.stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

String LeakHandler::stackLeakStatusText_(WebInterface &web, uint32_t node_id) {
        if (!web._stack_cache)
            return WebUiRu::Leak::kStackCacheUnavailable;
        const auto *cache = web.stackCache().leakCache(node_id);
        if (!cache)
            return WebUiRu::Common::kNoDataFromSlave;
        if (cache->pending)
            return WebUiRu::Leak::kRequestingSlaveData;
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = WebUiRu::Common::kErrorPrefix;
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return WebUiRu::Common::kNoDataFromSlave;
        return "OK";
    }

bool LeakHandler::sendStackLeakSet_(WebInterface &web, uint32_t node_id, JsonArray *zones, bool ack_all) {
        if (!web._stack_master || node_id == 0)
            return false;
        StaticJsonDocument<4096> doc;
        doc["cmd_id"] = web.nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Leak;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        if (ack_all)
            params["ack_all"] = true;
        if (zones)
        {
            JsonArray dst = params["zones"].to<JsonArray>();
            for (JsonObjectConst zone : *zones)
            {
                JsonObject out = dst.add<JsonObject>();
                for (JsonPairConst kv : zone)
                    out[kv.key()] = kv.value();
            }
        }
        char payload[3800] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        return web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                         (const uint8_t *)payload, len);
    }

String LeakHandler::paramName_(const char *prefix, size_t id) {
        String out(prefix);
        out += String((unsigned)id);
        return out;
    }

String LeakHandler::checked_(bool value) {
        return value ? " checked" : "";
    }

String LeakHandler::portValue_(uint8_t port) {
        if (port == LeakController::kInvalidPort)
            return String();
        return String((unsigned)port);
    }

size_t LeakHandler::stackLeakVisibleCount_(WebInterface &web, uint32_t node_id) {
        if (!web._controllers)
            return 0;
        LeakController &leak = web._controllers->leak();
        (void)leak;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        const StackCache::StackLeakCache *stack_cache = web._stack_cache ? web.stackCache().leakCache(node_id) : nullptr;
        if (!stack_cache || !stack_cache->has_data || !stack_cache->items)
            return 0;
        size_t render_count = LeakController::kZoneCount ? 1u : 0u;
        if (stack_cache->item_count)
        {
            size_t last_enabled_id = 0;
            for (size_t i = 0; i < stack_cache->item_count; ++i)
            {
                const auto &it = stack_cache->items[i];
                if (it.enabled && it.id > last_enabled_id)
                    last_enabled_id = it.id;
            }
            if (last_enabled_id > 0)
            {
                const size_t count = last_enabled_id + 1u;
                render_count = count > LeakController::kZoneCount ? LeakController::kZoneCount : count;
            }
        }
        size_t visible = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            const size_t id = i + 1;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Leak, (uint16_t)id, node_id))
                continue;
            const StackCache::StackLeakItem *it = nullptr;
            for (size_t k = 0; k < stack_cache->item_count; ++k)
            {
                if (stack_cache->items[k].id != id)
                    continue;
                it = &stack_cache->items[k];
                break;
            }
            if (!it || (!can_view_disabled && !it->enabled))
                continue;
            ++visible;
        }
        return visible;
    }

size_t LeakHandler::localLeakVisibleCount_(WebInterface &web, uint32_t node_id) {
        if (!web._controllers)
            return 0;
        LeakController &leak = web._controllers->leak();
        size_t render_count = LeakController::kZoneCount ? 1u : 0u;
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < LeakController::kZoneCount; ++i)
        {
            const LeakController::ZoneConfig *cfg = leak.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx != SIZE_MAX)
        {
            const size_t count = last_enabled_idx + 2u;
            render_count = count > LeakController::kZoneCount ? LeakController::kZoneCount : count;
        }
        size_t visible = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            const size_t id = i + 1;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Leak, (uint16_t)id, node_id))
                continue;
            ++visible;
        }
        return visible;
    }

String LeakHandler::buildRows_(WebInterface &web, uint32_t node_id, bool stack_view, size_t offset, size_t limit) {
        if (!web._controllers)
            return String("<div class=\"tile tile-empty\">") + WebUiRu::Common::kControllersUnavailable + "</div>";
        LeakController &leak = web._controllers->leak();
        String rows;
        rows.reserve(LeakController::kZoneCount * 1200);
        const StackCache::StackLeakCache *stack_cache = nullptr;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        if (stack_view && web._stack_cache)
            stack_cache = web.stackCache().leakCache(node_id);
        size_t render_count = LeakController::kZoneCount ? 1u : 0u;
        if (stack_view)
        {
            if (stack_cache && stack_cache->has_data && stack_cache->items && stack_cache->item_count)
            {
                size_t last_enabled_id = 0;
                for (size_t i = 0; i < stack_cache->item_count; ++i)
                {
                    const auto &it = stack_cache->items[i];
                    if (it.enabled && it.id > last_enabled_id)
                        last_enabled_id = it.id;
                }
                if (last_enabled_id > 0)
                {
                    const size_t count = last_enabled_id + 1u;
                    render_count = count > LeakController::kZoneCount ? LeakController::kZoneCount : count;
                }
            }
        }
        else
        {
            size_t last_enabled_idx = SIZE_MAX;
            for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            {
                const LeakController::ZoneConfig *cfg = leak.configByIndex(i);
                if (cfg && cfg->enabled)
                    last_enabled_idx = i;
            }
            if (last_enabled_idx != SIZE_MAX)
            {
                const size_t count = last_enabled_idx + 2u;
                render_count = count > LeakController::kZoneCount ? LeakController::kZoneCount : count;
            }
        }
        const size_t page_limit = (limit == 0) ? SIZE_MAX : limit;
        size_t rendered = 0;
        size_t visible_idx = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            if (rendered >= page_limit)
                break;
            const size_t id = i + 1;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Leak, (uint16_t)id, node_id))
                continue;
            bool cfg_enabled = false;
            bool cfg_power = false;
            bool cfg_active_low = true;
            String cfg_name;
            uint8_t cfg_sensor = LeakController::kInvalidPort;
            uint8_t cfg_valve = LeakController::kInvalidPort;
            uint8_t cfg_alarm = LeakController::kInvalidPort;
            bool st_wet = false;
            bool st_latched = false;
            if (stack_view)
            {
                if (!stack_cache || !stack_cache->has_data || !stack_cache->items)
                    continue;
                const StackCache::StackLeakItem *it = nullptr;
                for (size_t k = 0; k < stack_cache->item_count; ++k)
                {
                    if (stack_cache->items[k].id != id)
                        continue;
                    it = &stack_cache->items[k];
                    break;
                }
                if (!it)
                    continue;
                cfg_enabled = it->enabled;
                if (!can_view_disabled && !cfg_enabled)
                    continue;
                cfg_power = it->power_on;
                cfg_active_low = it->sensor_active_low;
                if (it->name[0])
                    cfg_name = it->name;
                cfg_sensor = it->sensor;
                cfg_valve = it->valve;
                cfg_alarm = it->alarm;
                st_wet = it->wet;
                st_latched = it->alarm_latched;
            }
            else
            {
                const LeakController::ZoneConfig *cfg = leak.configByIndex(i);
                const LeakController::ZoneState *st = leak.stateByIndex(i);
                if (!cfg || !st)
                    continue;
                cfg_enabled = cfg->enabled;
                cfg_power = cfg->power_on;
                cfg_active_low = cfg->sensor_active_low;
                cfg_name = cfg->name;
                cfg_sensor = cfg->sensor_port;
                cfg_valve = cfg->valve_port;
                cfg_alarm = cfg->alarm_port;
                st_wet = st->wet;
                st_latched = st->alarm_latched;
            }
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            const bool alert = st_wet || st_latched;
            rows += "<div class=\"tile";
            rows += cfg_enabled ? "" : " disabled";
            rows += alert ? " alert" : "";
            rows += "\"><div class=\"tile-head\"><div class=\"tile-left\"><svg class=\"leak-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            rows += "<path fill=\"currentColor\" d=\"M32 8c8 12 18 24 18 34 0 9.9-8.1 18-18 18s-18-8.1-18-18c0-10 10-22 18-34z\"/>";
            rows += "<path d=\"M32 14l14 5v11c0 9.8-5.8 18.4-14 21.8-8.2-3.4-14-12-14-21.8V19l14-5z\" fill=\"none\" stroke=\"#0b1220\" stroke-width=\"3\"/>";
            rows += "</svg><div class=\"tile-id\">";
            rows += WebUiRu::Leak::kLabelZonePrefix;
            rows += String((unsigned)id);
            rows += "</div></div><div class=\"badge-row\"><span class=\"badge\">";
            rows += WebUiRu::Leak::kBadgeWater;
            rows += st_wet ? "1" : "0";
            rows += "</span><span class=\"badge\">";
            rows += WebUiRu::Leak::kBadgeLatch;
            rows += st_latched ? "1" : "0";
            rows += "</span></div></div><div class=\"tile-grid\">";

            rows += "<div class=\"field-row full\"><label class=\"field-label\">";
            rows += WebUiRu::Leak::kLabelName;
            rows += "</label><input type=\"text\" maxlength=\"28\" name=\"leak_name_";
            rows += String((unsigned)id);
            rows += "\" value=\"";
            web.appendHtmlEscaped_(rows, cfg_name.c_str());
            rows += "\"></div>";

            rows += "<div class=\"field-row\"><label class=\"field-label\">";
            rows += WebUiRu::Leak::kLabelSensor;
            rows += "</label><select class=\"leak-select\" data-type=\"dinput\" data-selected=\"";
            rows += portValue_(cfg_sensor);
            rows += "\" name=\"leak_sensor_";
            rows += String((unsigned)id);
            rows += "\"></select></div>";

            rows += "<div class=\"field-row\"><label class=\"field-label\">";
            rows += WebUiRu::Leak::kLabelValve;
            rows += "</label><select class=\"leak-select\" data-type=\"relay\" data-selected=\"";
            rows += portValue_(cfg_valve);
            rows += "\" name=\"leak_valve_";
            rows += String((unsigned)id);
            rows += "\"></select></div>";

            rows += "<div class=\"field-row\"><label class=\"field-label\">";
            rows += WebUiRu::Leak::kLabelAlarm;
            rows += "</label><select class=\"leak-select\" data-type=\"relay\" data-selected=\"";
            rows += portValue_(cfg_alarm);
            rows += "\" name=\"leak_alarm_";
            rows += String((unsigned)id);
            rows += "\"></select></div>";

            rows += "<div class=\"checks\">";
            rows += "<label><input type=\"checkbox\" name=\"leak_en_";
            rows += String((unsigned)id);
            rows += "\"";
            rows += checked_(cfg_enabled);
            rows += "> ";
            rows += WebUiRu::Leak::kToggleOn;
            rows += "</label>";

            rows += "<label><input type=\"checkbox\" name=\"leak_pwr_";
            rows += String((unsigned)id);
            rows += "\"";
            rows += checked_(cfg_power);
            rows += "> ";
            rows += WebUiRu::Leak::kTogglePower;
            rows += "</label>";

            rows += "<label><input type=\"checkbox\" name=\"leak_al_";
            rows += String((unsigned)id);
            rows += "\"";
            rows += checked_(cfg_active_low);
            rows += "> ";
            rows += WebUiRu::Leak::kToggleActiveLow;
            rows += "</label>";

            rows += "</div></div></div>";
            ++rendered;
        }
        if (rows.length() == 0 && stack_view)
            return String("<div class=\"tile tile-empty\">") + WebUiRu::Leak::kWaitingSlave + "</div>";
        if (rows.length() == 0)
            return String("<div class=\"tile tile-empty\">") + WebUiRu::Leak::kNoLeakZones + "</div>";
        return rows;
    }
