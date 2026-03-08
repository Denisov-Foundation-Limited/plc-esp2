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

#include "core/network/web/handlers/watering_handler.hpp"

#include "core/network/web/web_interface.hpp"

void WateringHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/watering/state", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleWateringState(web, request); });
        server.on("/watering", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleWateringSave(web, request); });
        server.on("/watering", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleWatering(web, request); });
    }

void WateringHandler::handleWateringState(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Watering, node_id))
            return;
        StaticJsonDocument<4096> doc;
        JsonArray items = doc["items"].to<JsonArray>();
        const bool stack_view = web.isStackWateringView_(node_id);
        if (stack_view)
        {
            auto *cache = web._stack_cache ? web._stack_cache->wateringCache(node_id) : nullptr;
            if (!cache || !cache->has_data)
            {
                if (web._stack_cache)
                    web._stack_cache->requestWatering(node_id);
                doc["pending"] = true;
            }
            else
            {
                const bool stale = (cache->pending || (uint32_t)(millis() - cache->updated_ms) > 1500u);
                if (stale && web._stack_cache)
                    web._stack_cache->requestWatering(node_id);
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (!web.webAclCanViewItem_(UsersRegistry::AclController::Watering, it.id, node_id))
                        continue;
                    JsonObject o = items.add<JsonObject>();
                    o["id"] = it.id;
                    o["enabled"] = it.enabled;
                    o["status"] = it.status;
                    o["active"] = it.active;
                    o["paused"] = it.paused;
                }
            }
        }
        else
        {
            if (!web._controllers)
            {
                web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
                return;
            }
            WateringController &watering = web._controllers->watering();
            for (size_t i = 0; i < WateringController::kRuleCount; ++i)
            {
                const auto *cfg = watering.configByIndex(i);
                const auto *st = watering.stateByIndex(i);
                if (!cfg || !st)
                    continue;
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Watering, cfg->id))
                    continue;
                JsonObject o = items.add<JsonObject>();
                o["id"] = cfg->id;
                o["enabled"] = cfg->enabled;
                o["status"] = st->status;
                o["active"] = st->active;
                o["paused"] = st->paused;
            }
        }
        String body;
        serializeJson(doc, body);
        web.sendText_(request, 200, "application/json", body, set_cookie);
    }

void WateringHandler::handleWatering(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Watering, node_id))
            return;
        const bool stack_view = web.isStackWateringView_(node_id);
        size_t page_idx = 0;
        const size_t page_size = 8u;
        const String page_str = web.paramValueAny_(request, "page");
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (size_t)(v - 1);
        }
        size_t max_pages = 1;
        size_t page_offset = 0;
        if (stack_view)
        {
            web.requestStackWatering_(node_id);
            web.requestStackTanks_(node_id);
            web.requestStackPorts_(node_id);
            const size_t visible = web.stackWateringVisibleCount_(node_id);
            max_pages = (visible == 0) ? 1u : ((visible + page_size - 1u) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (max_pages - 1u) : 0u;
            page_offset = page_idx * page_size;
        }
        else
        {
            const size_t visible = web.wateringLocalRenderCount_();
            max_pages = (visible == 0) ? 1u : ((visible + page_size - 1u) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (max_pages - 1u) : 0u;
            page_offset = page_idx * page_size;
        }
        String page = FPSTR(kWebInterfaceWateringHtml);
        page.reserve(page.length() + 32768);
        String rows = stack_view ? web.listStackWateringHtml_(node_id, page_offset, page_size) : web.listWateringHtml_(page_offset, page_size);
        if (stack_view && rows.length() == 0)
            rows = WebUiRu::Watering::kText;
        String pagination = "";
        if (max_pages > 1u)
        {
            const bool has_prev = page_idx > 0u;
            const bool has_next = (page_idx + 1u) < max_pages;
            pagination.reserve(256);
            pagination += "<div class=\"pagination\">";
            if (has_prev)
            {
                pagination += "<a class=\"page-btn\" href=\"/watering?";
                if (stack_view)
                {
                    pagination += "unit=stack&node=";
                    pagination += String((unsigned long)node_id);
                    pagination += "&";
                }
                pagination += "page=";
                pagination += String((unsigned)(page_idx));
                pagination += String("\">") + WebUiRu::Common::kPagePrev + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPagePrev + "</span>";
            pagination += String("<span class=\"page-info\">") + WebUiRu::Common::kPagePage + " ";
            pagination += String((unsigned)(page_idx + 1u));
            pagination += " / ";
            pagination += String((unsigned)max_pages);
            pagination += "</span>";
            if (has_next)
            {
                pagination += "<a class=\"page-btn\" href=\"/watering?";
                if (stack_view)
                {
                    pagination += "unit=stack&node=";
                    pagination += String((unsigned long)node_id);
                    pagination += "&";
                }
                pagination += "page=";
                pagination += String((unsigned)(page_idx + 2u));
                pagination += String("\">") + WebUiRu::Common::kPageNext + "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Common::kPageNext + "</span>";
            pagination += "</div>";
        }
        String form_action = "/watering";
        if (stack_view)
        {
            form_action += "?unit=stack&node=";
            form_action += String((unsigned long)node_id);
            form_action += "&page=";
            form_action += String((unsigned)(page_idx + 1u));
        }
        page.replace("%NAV%", web.navHtml_());
        page.replace("%WATERING_PAGE_TITLE%", WebUiRu::Watering::kPageTitle);
        page.replace("%WATERING_ROWS%", rows);
        page.replace("%WATERING_PAGINATION%", pagination);
        page.replace("%WATERING_FORM_ACTION%", form_action);
        page.replace("%WATERING_STATUS%", stack_view ? web.stackWateringStatusText_(node_id) : web._watering_status);
        page.replace("%WATERING_RELAY_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::Relay)
                                                         : web.wateringPortOptionsJson_());
        page.replace("%WATERING_RELAY_USED_JSON%",
                     stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::Relay)
                                : web.globalUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%WATERING_TANK_JSON%",
                     stack_view ? web.stackWateringTankOptionsJson_(node_id) : web.wateringTankOptionsJson_());
        page.replace("%WATERING_DEVICE_SELECT%", web.wateringDeviceSelectHtml_(node_id, stack_view));
        page.replace("%WATERING_JS_STATE_ACTIVE%", WebUiRu::Watering::kText3);
        page.replace("%WATERING_JS_STATE_PAUSED%", WebUiRu::Watering::kText4);
        page.replace("%WATERING_JS_STATE_WAIT%", WebUiRu::Watering::kText5);
        page.replace("%WATERING_JS_ON%", WebUiRu::Watering::kText7);
        page.replace("%WATERING_JS_OFF%", WebUiRu::Watering::kText8);
        bool can_save = web.webSessionIsAdmin_();
        if (!can_save)
        {
            if (stack_view)
            {
                const auto *cache = web._stack_cache ? web._stack_cache->wateringCache(node_id) : nullptr;
                if (cache && cache->has_data && cache->items)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!web.webAclCanViewItem_(UsersRegistry::AclController::Watering, it.id, node_id))
                            continue;
                        if (web.webAclCanControlItem_(UsersRegistry::AclController::Watering, it.id, node_id))
                        {
                            can_save = true;
                            break;
                        }
                    }
                }
            }
            else if (web._controllers)
            {
                WateringController &watering = web._controllers->watering();
                for (size_t i = 0; i < WateringController::kRuleCount; ++i)
                {
                    const auto *cfg = watering.configByIndex(i);
                    if (!cfg)
                        continue;
                    if (!web.webAclCanViewItem_(UsersRegistry::AclController::Watering, cfg->id))
                        continue;
                    if (web.webAclCanControlItem_(UsersRegistry::AclController::Watering, cfg->id))
                    {
                        can_save = true;
                        break;
                    }
                }
            }
        }
        page.replace("%WATERING_SAVE_BTN%",
                     can_save ? (String("<button type=\"submit\">") + WebUiRu::kSave + "</button>") : String(""));
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

void WateringHandler::handleWateringSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Watering))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackWateringView_(node_id))
        {
            String back = String("/watering?unit=stack&node=") + String((unsigned long)node_id);
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
                web._watering_status = "Stack unavailable";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            auto *cache = web._stack_cache->wateringCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                web._stack_cache->requestWatering(node_id);
                web._watering_status = "No data";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            auto *cache_mut = web._stack_cache->wateringCache(node_id);
            bool changed = false;
            static const uint8_t kWeekdayMap[7] = {2, 3, 4, 5, 6, 7, 1};
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                const String idx = String((unsigned)it.id);
                const String prefix = String("w") + idx + "_";
                const String en_key = prefix + "en";
                const String status_key = prefix + "status";
                const String name_key = prefix + "name";
                const String port_key = prefix + "port";
                const String tank_key = prefix + "tank";
                const String time_key = prefix + "time";
                const String time2_key = prefix + "time2";
                const String time3_key = prefix + "time3";
                const String dur_key = prefix + "dur";
                const String dur2_key = prefix + "dur2";
                const String dur3_key = prefix + "dur3";
                const String resume_key = prefix + "resume";
                const String resume_level_key = prefix + "resume_level";
                bool has_any = request->hasParam(en_key, true) ||
                               request->hasParam(status_key, true) ||
                               request->hasParam(name_key, true) ||
                               request->hasParam(port_key, true) ||
                               request->hasParam(tank_key, true) ||
                               request->hasParam(time_key, true) ||
                               request->hasParam(time2_key, true) ||
                               request->hasParam(time3_key, true) ||
                               request->hasParam(dur_key, true) ||
                               request->hasParam(dur2_key, true) ||
                               request->hasParam(dur3_key, true) ||
                               request->hasParam(resume_key, true) ||
                               request->hasParam(resume_level_key, true);
                for (uint8_t dow = 1; dow <= 7 && !has_any; ++dow)
                {
                    if (request->hasParam(prefix + "d" + String((unsigned)dow), true))
                        has_any = true;
                }
                if (!has_any)
                    continue;
                if (!web.webAclCanControlItem_(UsersRegistry::AclController::Watering, it.id, node_id))
                {
                    web._watering_status = String("ACL deny item: ") + idx;
                    web.sendRedirect_(request, back, set_cookie);
                    return;
                }

                const bool enabled = paramChecked_(request, en_key);
                const bool status_on = request->hasParam(status_key, true);
                String name = web.paramValue_(request, name_key);
                name.trim();
                uint8_t port = WateringController::kInvalidPort;
                if (!parsePort_(web.paramValue_(request, port_key), port))
                    port = WateringController::kInvalidPort;
                uint8_t tank_id = 0;
                if (!parseTank_(web.paramValue_(request, tank_key), tank_id))
                    tank_id = 0;

                uint8_t weekdays_mask = 0;
                for (size_t wi = 0; wi < 7; ++wi)
                {
                    const uint8_t dow = kWeekdayMap[wi];
                    const String key = prefix + "d" + String((unsigned)dow);
                    if (request->hasParam(key, true))
                        weekdays_mask |= (uint8_t)(1u << (dow - 1u));
                }

                uint8_t hour = 0xFF;
                uint8_t minute = 0xFF;
                const bool time_ok = parseTime_(web.paramValue_(request, time_key), hour, minute);
                uint8_t hour2 = 0xFF;
                uint8_t minute2 = 0xFF;
                const bool time2_ok = parseTime_(web.paramValue_(request, time2_key), hour2, minute2);
                uint8_t hour3 = 0xFF;
                uint8_t minute3 = 0xFF;
                const bool time3_ok = parseTime_(web.paramValue_(request, time3_key), hour3, minute3);

                uint32_t dur_min = 0;
                parseDuration_(web.paramValue_(request, dur_key), dur_min);
                uint32_t duration_sec = time_ok ? (dur_min * 60u) : 0u;
                uint32_t dur2_min = 0;
                parseDuration_(web.paramValue_(request, dur2_key), dur2_min);
                uint32_t duration2_sec = time2_ok ? (dur2_min * 60u) : 0u;
                uint32_t dur3_min = 0;
                parseDuration_(web.paramValue_(request, dur3_key), dur3_min);
                uint32_t duration3_sec = time3_ok ? (dur3_min * 60u) : 0u;

                const bool resume_on = request->hasParam(resume_key, true);
                uint8_t resume_level = it.resume_level;
                (void)parseResumeLevel_(web.paramValue_(request, resume_level_key), resume_level);

                const bool item_changed = (enabled != it.enabled) ||
                                          (status_on != it.status) ||
                                          (name != String(it.name)) ||
                                          (port != it.port) ||
                                          (tank_id != it.tank_id) ||
                                          (weekdays_mask != it.weekdays_mask) ||
                                          (hour != it.hour) || (minute != it.minute) ||
                                          (duration_sec != it.duration_sec) ||
                                          (hour2 != it.hour2) || (minute2 != it.minute2) ||
                                          (duration2_sec != it.duration2_sec) ||
                                          (hour3 != it.hour3) || (minute3 != it.minute3) ||
                                          (duration3_sec != it.duration3_sec) ||
                                          (resume_on != it.resume_after_refill) ||
                                          (resume_level != it.resume_level);
                if (!item_changed)
                    continue;

                StaticJsonDocument<512> doc;
                doc["cmd_id"] = 0;
                doc["feature"] = (uint8_t)StackFeature::Watering;
                doc["action"] = "set";
                JsonObject p = doc["params"].to<JsonObject>();
                p["id"] = (unsigned)it.id;
                p["enabled"] = enabled;
                p["status"] = status_on;
                p["name"] = name;
                p["port"] = (port == WateringController::kInvalidPort) ? -1 : (int)port;
                p["tank"] = tank_id;
                p["weekdays_mask"] = weekdays_mask;
                p["hour"] = hour;
                p["minute"] = minute;
                p["duration_s"] = duration_sec;
                p["hour2"] = hour2;
                p["minute2"] = minute2;
                p["duration2_s"] = duration2_sec;
                p["hour3"] = hour3;
                p["minute3"] = minute3;
                p["duration3_s"] = duration3_sec;
                p["resume"] = resume_on;
                p["resume_level"] = resume_level;
                char payload[640] = {};
                const size_t len = serializeJson(doc, payload, sizeof(payload));
                if (len == 0 || !web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                            reinterpret_cast<const uint8_t *>(payload), len))
                {
                    web._watering_status = String("Send failed item: ") + idx;
                    web.sendRedirect_(request, back, set_cookie);
                    return;
                }
                changed = true;

                if (cache_mut && cache_mut->items)
                {
                    for (size_t k = 0; k < cache_mut->item_count; ++k)
                    {
                        auto &dst = cache_mut->items[k];
                        if (dst.id != it.id)
                            continue;
                        dst.enabled = enabled;
                        dst.status = status_on;
                        dst.tank_id = tank_id;
                        dst.port = port;
                        dst.weekdays_mask = weekdays_mask;
                        dst.hour = hour;
                        dst.minute = minute;
                        dst.duration_sec = duration_sec;
                        dst.hour2 = hour2;
                        dst.minute2 = minute2;
                        dst.duration2_sec = duration2_sec;
                        dst.hour3 = hour3;
                        dst.minute3 = minute3;
                        dst.duration3_sec = duration3_sec;
                        dst.resume_after_refill = resume_on;
                        dst.resume_level = resume_level;
                        size_t n = 0;
                        for (; n + 1 < sizeof(dst.name) && n < name.length(); ++n)
                            dst.name[n] = name[n];
                        dst.name[n] = '\0';
                        cache_mut->updated_ms = millis();
                        cache_mut->has_data = true;
                        break;
                    }
                }
            }
            if (changed)
            {
                web._stack_cache->requestWatering(node_id);
                web.refreshStackPorts_(node_id);
            }
            web._watering_status = changed ? "Updated" : "No changes";
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", WebUiRu::Common::kControllersUnavailable, set_cookie);
            return;
        }
        WateringController &watering = web._controllers->watering();
        bool changed = false;
        for (size_t i = 0; i < WateringController::kRuleCount; ++i)
        {
            const auto *cfg = watering.configByIndex(i);
            const auto *st = watering.stateByIndex(i);
            if (!cfg || !st)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("w") + idx + "_";
            const String en_key = prefix + "en";
            const String status_key = prefix + "status";
            const String name_key = prefix + "name";
            const String port_key = prefix + "port";
            const String tank_key = prefix + "tank";
            const String time_key = prefix + "time";
            const String time2_key = prefix + "time2";
            const String time3_key = prefix + "time3";
            const String dur_key = prefix + "dur";
            const String dur2_key = prefix + "dur2";
            const String dur3_key = prefix + "dur3";
            const String resume_key = prefix + "resume";
            const String resume_level_key = prefix + "resume_level";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(status_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(port_key, true) ||
                                 request->hasParam(tank_key, true) ||
                                 request->hasParam(time_key, true) ||
                                 request->hasParam(time2_key, true) ||
                                 request->hasParam(time3_key, true) ||
                                 request->hasParam(dur_key, true) ||
                                 request->hasParam(dur2_key, true) ||
                                 request->hasParam(dur3_key, true) ||
                                 request->hasParam(resume_key, true) ||
                                 request->hasParam(resume_level_key, true);
            if (!has_any)
                continue;
            if (!web.webAclCanControlItem_(UsersRegistry::AclController::Watering, cfg->id))
            {
                web._watering_status = String("ACL deny item: ") + idx;
                web.sendRedirect_(request, "/watering", set_cookie);
                return;
            }

            const bool enabled = paramChecked_(request, en_key);
            if (cfg->enabled != enabled)
            {
                watering.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (!enabled)
                continue;

            const bool status_on = request->hasParam(status_key, true);
            if (st->status != status_on)
            {
                watering.setStatus(cfg->id, status_on);
                changed = true;
            }

            String name = web.paramValue_(request, name_key);
            name.trim();
            if (name != cfg->name)
            {
                watering.setName(cfg->id, name);
                changed = true;
            }

            const String port_str = web.paramValue_(request, port_key);
            uint8_t port = WateringController::kInvalidPort;
            if (!parsePort_(port_str, port))
                port = WateringController::kInvalidPort;
            if (port != cfg->port)
            {
                watering.setPort(cfg->id, port);
                changed = true;
            }

            const String tank_str = web.paramValue_(request, tank_key);
            uint8_t tank_id = 0;
            if (!parseTank_(tank_str, tank_id))
                tank_id = 0;
            if (tank_id != cfg->tank_id)
            {
                watering.setTankId(cfg->id, tank_id);
                changed = true;
            }

            uint8_t weekdays_mask = 0;
            for (uint8_t dow = 1; dow <= 7; ++dow)
            {
                const String key = prefix + "d" + String((unsigned)dow);
                if (request->hasParam(key, true))
                    weekdays_mask |= (uint8_t)(1u << (dow - 1u));
            }
            if (weekdays_mask != cfg->weekdays_mask)
            {
                watering.setWeekdaysMask(cfg->id, weekdays_mask);
                changed = true;
            }

            const String time_str = web.paramValue_(request, time_key);
            uint8_t hour = 0;
            uint8_t minute = 0;
            const bool time_ok = parseTime_(time_str, hour, minute);
            if (!time_ok)
            {
                hour = 0xFF;
                minute = 0xFF;
            }
            if (hour != cfg->hour || minute != cfg->minute)
            {
                watering.setStartTime(cfg->id, hour, minute);
                changed = true;
            }
            const String time2_str = web.paramValue_(request, time2_key);
            uint8_t hour2 = 0;
            uint8_t minute2 = 0;
            const bool time2_ok = parseTime_(time2_str, hour2, minute2);
            if (!time2_ok)
            {
                hour2 = 0xFF;
                minute2 = 0xFF;
            }
            if (hour2 != cfg->hour2 || minute2 != cfg->minute2)
            {
                watering.setStartTimeSlot(cfg->id, 1, hour2, minute2);
                changed = true;
            }
            const String time3_str = web.paramValue_(request, time3_key);
            uint8_t hour3 = 0;
            uint8_t minute3 = 0;
            const bool time3_ok = parseTime_(time3_str, hour3, minute3);
            if (!time3_ok)
            {
                hour3 = 0xFF;
                minute3 = 0xFF;
            }
            if (hour3 != cfg->hour3 || minute3 != cfg->minute3)
            {
                watering.setStartTimeSlot(cfg->id, 2, hour3, minute3);
                changed = true;
            }

            const String dur_str = web.paramValue_(request, dur_key);
            uint32_t dur_min = 0;
            parseDuration_(dur_str, dur_min);
            uint32_t dur_sec = dur_min * 60u;
            if (!time_ok)
                dur_sec = 0;
            if (dur_sec != cfg->duration_sec)
            {
                watering.setDuration(cfg->id, dur_sec);
                changed = true;
            }
            const String dur2_str = web.paramValue_(request, dur2_key);
            uint32_t dur2_min = 0;
            parseDuration_(dur2_str, dur2_min);
            uint32_t dur2_sec = dur2_min * 60u;
            if (!time2_ok)
                dur2_sec = 0;
            if (dur2_sec != cfg->duration2_sec)
            {
                watering.setDurationSlot(cfg->id, 1, dur2_sec);
                changed = true;
            }
            const String dur3_str = web.paramValue_(request, dur3_key);
            uint32_t dur3_min = 0;
            parseDuration_(dur3_str, dur3_min);
            uint32_t dur3_sec = dur3_min * 60u;
            if (!time3_ok)
                dur3_sec = 0;
            if (dur3_sec != cfg->duration3_sec)
            {
                watering.setDurationSlot(cfg->id, 2, dur3_sec);
                changed = true;
            }

            const bool resume_on = request->hasParam(resume_key, true);
            if (resume_on != cfg->resume_after_refill)
            {
                watering.setResumeAfterRefill(cfg->id, resume_on);
                changed = true;
            }

            const String resume_level_str = web.paramValue_(request, resume_level_key);
            uint8_t resume_level = cfg->resume_level;
            if (parseResumeLevel_(resume_level_str, resume_level) && resume_level != cfg->resume_level)
            {
                watering.setResumeLevel(cfg->id, resume_level);
                changed = true;
            }
        }

        bool ok = true;
        if (changed)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._watering_status = WebUiRu::Common::kConfigManagerUnavailable;
            }
            else if (!web._configs_manager->save())
            {
                ok = false;
                web._watering_status = WebUiRu::Common::kSaveFailed;
            }
        }
        if (ok)
            web._watering_status = changed ? WebUiRu::Common::kUpdated : WebUiRu::Common::kNoChanges;
        web.sendRedirect_(request, "/watering", set_cookie);
    }

bool WateringHandler::paramChecked_(AsyncWebServerRequest *request, const String &name) {
        if (!request)
            return false;
        const int count = request->params();
        for (int i = 0; i < count; ++i)
        {
            const auto *param = request->getParam(i);
            if (!param || param->name() != name)
                continue;
            String v = param->value();
            v.toLowerCase();
            if (v == "1" || v == "on" || v == "true")
                return true;
        }
        return false;
    }

bool WateringHandler::parsePort_(const String &s, uint8_t &out) {
        String t = s;
        t.trim();
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const int v = t.toInt();
        if (v < 0 || v > 255)
            return false;
        out = (uint8_t)v;
        return true;
    }

bool WateringHandler::parseTank_(const String &s, uint8_t &out) {
        String t = s;
        t.trim();
        if (t.length() == 0)
            return false;
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        const int v = t.toInt();
        if (v < 0 || v > 255)
            return false;
        out = (uint8_t)v;
        return true;
    }

bool WateringHandler::parseTime_(const String &s, uint8_t &hour, uint8_t &minute) {
        const int p1 = s.indexOf(':');
        if (p1 <= 0)
            return false;
        const int h = s.substring(0, p1).toInt();
        const int m = s.substring(p1 + 1).toInt();
        if (h < 0 || h > 23)
            return false;
        if (m < 0 || m > 59)
            return false;
        hour = (uint8_t)h;
        minute = (uint8_t)m;
        return true;
    }

bool WateringHandler::parseDuration_(const String &s, uint32_t &out) {
        String t = s;
        t.trim();
        if (t.length() == 0)
        {
            out = 0;
            return false;
        }
        for (size_t i = 0; i < t.length(); ++i)
            if (t[i] < '0' || t[i] > '9')
                return false;
        out = (uint32_t)t.toInt();
        return true;
    }

bool WateringHandler::parseResumeLevel_(const String &s, uint8_t &out) {
        String t = s;
        t.trim();
        t.toLowerCase();
        if (t == "low")
            out = 0;
        else if (t == "mid")
            out = 1;
        else if (t == "full")
            out = 2;
        else
            return false;
        return true;
    }
