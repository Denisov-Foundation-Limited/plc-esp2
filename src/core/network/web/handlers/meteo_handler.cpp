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

#include "core/network/web/handlers/meteo_handler.hpp"

#include "core/network/web/web_interface.hpp"

void MeteoHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/meteo/list", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleMeteoList(web, request); });
        server.on("/meteo/remote_sources", HTTP_GET,
                  [&web](AsyncWebServerRequest *request) { handleRemoteSources(web, request); });
        server.on("/meteo/state", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleMeteoState(web, request); });
        server.on("/meteo", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleMeteoSave(web, request); });
        server.on("/meteo", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleMeteo(web, request); });
    }

void MeteoHandler::handleRemoteSources(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Meteo, node_id))
            return;
        if (web.isStackMeteoView_(node_id))
        {
            web.sendText_(request, 200, "text/html; charset=utf-8", "", set_cookie);
            return;
        }
        web.sendText_(request, 200, "text/html; charset=utf-8", web.meteoRemoteSensorOptionsHtml_(0, 0), set_cookie);
    }

void MeteoHandler::handleMeteo(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Meteo, node_id))
            return;
        const bool stack_view = web.isStackMeteoView_(node_id);
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
                const size_t visible = web.stackMeteoVisibleCount_(node_id);
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
            const size_t visible = web.meteoLocalRenderCount_();
            max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        else
        {
            page_idx = 0;
            max_pages = 1;
        }
        String page = FPSTR(kWebInterfaceMeteoHtml);
        page.reserve(page.length() + 16384);
        String pagination = "";
        if (stack_view && max_pages > 1)
        {
            pagination.reserve(256);
            pagination += "<div class=\"pagination\">";
            if (page_idx > 0)
            {
                pagination += "<a class=\"page-btn\" href=\"/meteo?unit=stack&node=";
                pagination += String((unsigned long)node_id);
                pagination += "&page=";
                pagination += String((unsigned)page_idx);
                pagination += "\">";
                pagination += WebUiRu::Meteo::kPagePrev;
                pagination += "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Meteo::kPagePrev + "</span>";
            pagination += String("<span class=\"page-info\">") + WebUiRu::Meteo::kPagePage + " ";
            pagination += String((unsigned)(page_idx + 1));
            pagination += " / ";
            pagination += String((unsigned)max_pages);
            pagination += "</span>";
            if ((page_idx + 1u) < max_pages)
            {
                pagination += "<a class=\"page-btn\" href=\"/meteo?unit=stack&node=";
                pagination += String((unsigned long)node_id);
                pagination += "&page=";
                pagination += String((unsigned)(page_idx + 2u));
                pagination += "\">";
                pagination += WebUiRu::Meteo::kPageNext;
                pagination += "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Meteo::kPageNext + "</span>";
            pagination += "</div>";
        }
        if (!stack_view && !groups_available && max_pages > 1)
        {
            pagination.reserve(256);
            pagination += "<div class=\"pagination\">";
            if (page_idx > 0)
            {
                pagination += "<a class=\"page-btn\" href=\"/meteo?page=";
                pagination += String((unsigned)page_idx);
                pagination += "\">";
                pagination += WebUiRu::Meteo::kPagePrev;
                pagination += "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Meteo::kPagePrev + "</span>";
            pagination += String("<span class=\"page-info\">") + WebUiRu::Meteo::kPagePage + " ";
            pagination += String((unsigned)(page_idx + 1));
            pagination += " / ";
            pagination += String((unsigned)max_pages);
            pagination += "</span>";
            if ((page_idx + 1u) < max_pages)
            {
                pagination += "<a class=\"page-btn\" href=\"/meteo?page=";
                pagination += String((unsigned)(page_idx + 2u));
                pagination += "\">";
                pagination += WebUiRu::Meteo::kPageNext;
                pagination += "</a>";
            }
            else
                pagination += String("<span class=\"page-btn disabled\">") + WebUiRu::Meteo::kPageNext + "</span>";
            pagination += "</div>";
        }
        page.replace("%METEO_PAGE_TITLE%", WebUiRu::Meteo::kPageTitle);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%METEO_TILES%", "<div class=\"tile empty\">Loading...</div>");
        page.replace("%METEO_PAGINATION%", pagination);
        page.replace("%METEO_STATUS%", stack_view ? web.stackMeteoStatusText_(node_id) : web._meteo_status);
        page.replace("%SENSOR_JSON%", stack_view ? web.stackPortOptionsJson_(node_id, PortIO::PinType::Sensor)
                                                 : web.meteoPortOptionsJson_());
        page.replace("%SENSOR_USED_JSON%",
                     stack_view ? web.stackUsedPortsJson_(node_id, PortIO::PinType::Sensor)
                                : web.globalUsedPortsJson_(PortIO::PinType::Sensor));
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
            page.replace("%METEO_FORM_HIDDEN%", hidden);
        }
        else
        {
            page.replace("%METEO_FORM_HIDDEN%", "");
        }
        page.replace("%METEO_DEVICE_SELECT%",
                     web.composeTopFiltersHtml_(web.meteoDeviceSelectHtml_(node_id, stack_view),
                                                groups_available ? web.groupFilterHtml_("meteo-group-filter", stack_view ? node_id : 0u) : String("")));
        page.replace("%METEO_SAVE_BTN%",
                     web.webSessionIsAdmin_() ? (String("<button class=\"btn\" type=\"submit\">") + WebUiRu::kSave + "</button>")
                                              : String(""));
        page.replace("%METEO_STATUS_OFF_TEXT%", WebUiRu::Meteo::kText4);
        page.replace("%METEO_STATUS_NODATA_TEXT%", WebUiRu::Meteo::kText5);
        page.replace("%METEO_STATUS_OK_TEXT%", WebUiRu::Meteo::kText6);
        page.replace("%METEO_STATUS_ERR_TEXT%", WebUiRu::Meteo::kText7);
        page.replace("%METEO_STATUS_AGE_PREFIX%", WebUiRu::Meteo::kText13);
        page.replace("%METEO_CAN_EDIT%", web.webSessionIsAdmin_() ? "true" : "false");
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

void MeteoHandler::handleMeteoList(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Meteo, node_id))
            return;
        const bool stack_view = web.isStackMeteoView_(node_id);
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
                const size_t visible = web.stackMeteoVisibleCount_(node_id);
                const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
                if (page_idx >= max_pages)
                    page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            }
            web.sendText_(request, 200, "text/html; charset=utf-8",
                          web.listStackMeteoHtml_(node_id, groups_available ? 0u : (size_t)page_idx * page_size,
                                                  groups_available ? SIZE_MAX : page_size),
                          set_cookie);
            return;
        }
        if (!groups_available)
        {
            const size_t visible = web.meteoLocalRenderCount_();
            const uint8_t max_pages = (uint8_t)(((visible ? visible : 1u) + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
        }
        web.sendText_(request, 200, "text/html; charset=utf-8",
                      web.listMeteoHtml_(groups_available ? 0u : (size_t)page_idx * page_size,
                                         groups_available ? SIZE_MAX : page_size),
                      set_cookie);
    }

void MeteoHandler::handleMeteoState(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuthApi_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Meteo, node_id))
            return;

        StaticJsonDocument<4096> doc;
        JsonArray items = doc["items"].to<JsonArray>();

        if (web.isStackMeteoView_(node_id))
        {
            auto *cache = web._stack_cache ? web._stack_cache->meteoCache(node_id) : nullptr;
            if (!cache || !cache->has_data)
            {
                if (web._stack_cache)
                    web._stack_cache->requestMeteo(node_id);
                doc["pending"] = true;
            }
            else
            {
                const bool stale = (cache->pending || (uint32_t)(millis() - cache->updated_ms) > 1500u);
                if (stale && web._stack_cache)
                    web._stack_cache->requestMeteo(node_id);
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &cfg = cache->items[i];
                    if (!web.webAclCanViewItem_(UsersRegistry::AclController::Meteo, cfg.id, node_id))
                        continue;
                    JsonObject o = items.add<JsonObject>();
                    o["id"] = cfg.id;
                    o["enabled"] = cfg.enabled;
                    o["ok"] = cfg.ok;
                    o["has_temp"] = cfg.has_temp;
                    o["temp"] = cfg.temp_c;
                    o["has_hum"] = cfg.has_hum;
                    o["hum"] = cfg.hum;
                    o["has_read"] = cfg.has_read;
                    o["age_s"] = cfg.age_s;
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
            MeteoController &meteo = web._controllers->meteo();
            struct MeteoStateRow
            {
                bool present = false;
                MeteoController::SensorConfig cfg{};
                MeteoController::SensorState st{};
            };
            MeteoStateRow rows[MeteoController::kSensorCount] = {};
            auto meteo_guard = meteo.lockGuard();
            if (!meteo_guard.locked())
            {
                web.sendText_(request, 503, "text/plain", "Meteo busy", set_cookie);
                return;
            }
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = meteo.configByIndex(i);
                const auto *st = meteo.stateByIndex(i);
                if (!cfg || !st)
                    continue;
                rows[i].present = true;
                rows[i].cfg = *cfg;
                rows[i].st = *st;
            }
            const uint32_t now = millis();
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                if (!rows[i].present)
                    continue;
                const auto *cfg = &rows[i].cfg;
                const auto *st = &rows[i].st;
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Meteo, cfg->id))
                    continue;
                JsonObject o = items.add<JsonObject>();
                o["id"] = cfg->id;
                o["enabled"] = cfg->enabled;
                o["ok"] = st->ok;
                o["has_temp"] = st->has_temp;
                o["temp"] = st->temp_c;
                o["has_hum"] = st->has_humidity;
                o["hum"] = st->humidity;
                const uint32_t age_s = st->last_read_ms ? (uint32_t)((now - st->last_read_ms) / 1000u) : 0u;
                o["age_s"] = age_s;
                o["has_read"] = st->last_read_ms != 0;
            }
        }

        String body;
        serializeJson(doc, body);
        web.sendText_(request, 200, "application/json", body, set_cookie);
    }

void MeteoHandler::handleMeteoSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Meteo))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackMeteoView_(node_id))
        {
            String back = String("/meteo?unit=stack&node=") + String((unsigned long)node_id);
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
                web._meteo_status = "Stack unavailable";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            const auto *cache = web._stack_cache->meteoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
            {
                web._meteo_status = "No data";
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            auto *cache_mut = web._stack_cache->meteoCache(node_id);
            bool changed = false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                const String idx = String((unsigned)it.id);
                const String prefix = String("m") + idx + "_";
                const String en_key = prefix + "en";
                const String name_key = prefix + "name";
                const String type_key = prefix + "type";
                const String pin_key = prefix + "pin";
                const String addr_key = prefix + "addr";
                const String group_key = prefix + "group";
                const bool has_any = request->hasParam(en_key, true) ||
                                     request->hasParam(name_key, true) ||
                                     request->hasParam(type_key, true) ||
                                     request->hasParam(pin_key, true) ||
                                     request->hasParam(addr_key, true) ||
                                     request->hasParam(group_key, true);
                if (!has_any)
                    continue;
                if (!web.webAclCanControlItem_(UsersRegistry::AclController::Meteo, it.id, node_id))
                {
                    web._meteo_status = String("ACL deny item: ") + idx;
                    web.sendRedirect_(request, back, set_cookie);
                    return;
                }

                const bool can_admin = web.webSessionIsAdmin_();
                const bool enabled = request->hasParam(en_key, true);
                String name = web.paramValue_(request, name_key);
                name.trim();
                const String type_str = web.paramValue_(request, type_key);
                const String pin_str = web.paramValue_(request, pin_key);
                const String addr_str = web.paramValue_(request, addr_key);

                bool set_enabled = false;
                bool set_name = false;
                bool set_type = false;
                bool set_pin = false;
                bool set_addr = false;
                bool set_group = false;
                bool item_changed = false;
                bool is_dht22 = (strcmp(it.type, "dht22") == 0);
                bool is_ds18 = (strcmp(it.type, "ds18b20") == 0);
                String new_type = it.type[0] ? String(it.type) : String("none");
                String new_name = it.name[0] ? String(it.name) : String();
                int new_pin = it.pin;
                String new_addr = it.addr[0] ? String(it.addr) : String();

                if (enabled != it.enabled)
                {
                    item_changed = true;
                    set_enabled = true;
                }
                if (can_admin && name != new_name)
                {
                    item_changed = true;
                    set_name = true;
                    new_name = name;
                }
                if (can_admin && request->hasParam(type_key, true))
                {
                    MeteoController::SensorType t = MeteoController::SensorType::None;
                    if (!web.parseMeteoType_(type_str, t))
                    {
                        web._meteo_status = String("Invalid type for sensor ") + idx;
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    const char *tn = (t == MeteoController::SensorType::Dht22) ? "dht22"
                                      : (t == MeteoController::SensorType::Ds18b20) ? "ds18b20"
                                                                                     : "none";
                    if (new_type != tn)
                    {
                        item_changed = true;
                        set_type = true;
                        new_type = tn;
                    }
                    is_dht22 = (t == MeteoController::SensorType::Dht22);
                    is_ds18 = (t == MeteoController::SensorType::Ds18b20);
                }
                if (can_admin && request->hasParam(pin_key, true))
                {
                    uint8_t pin = MeteoController::kInvalidPin;
                    if (!web.parseMeteoPin_(pin_str, pin))
                    {
                        web._meteo_status = String("Invalid pin for sensor ") + idx;
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    const int pin_i = (pin == MeteoController::kInvalidPin) ? -1 : (int)pin;
                    if (pin_i != new_pin)
                    {
                        item_changed = true;
                        set_pin = true;
                        new_pin = pin_i;
                    }
                }
                if (can_admin && request->hasParam(addr_key, true))
                {
                    uint8_t addr[MeteoController::kAddrLen] = {};
                    bool addr_set = false;
                    if (!web.parseMeteoAddr_(addr_str, addr, addr_set))
                    {
                        web._meteo_status = String("Invalid addr for sensor ") + idx;
                        web.sendRedirect_(request, back, set_cookie);
                        return;
                    }
                    String addr_norm;
                    if (addr_set)
                    {
                        char hex[17] = {};
                        MeteoController::formatHexAddr(addr, hex);
                        addr_norm = hex;
                    }
                    if (addr_norm != new_addr)
                    {
                        item_changed = true;
                        set_addr = true;
                        new_addr = addr_norm;
                    }
                }
                const uint8_t group_id = web.parseGroupIdParam_(request, group_key);
                if (it.group_id != group_id)
                {
                    item_changed = true;
                    set_group = true;
                }

                if (!item_changed)
                    continue;

                StaticJsonDocument<256> doc;
                doc["cmd_id"] = 0;
                doc["feature"] = (uint8_t)StackFeature::Meteo;
                doc["action"] = "set";
                JsonArray arr = doc["params"]["items"].to<JsonArray>();
                JsonObject obj = arr.add<JsonObject>();
                obj["id"] = (unsigned)it.id;
                if (set_name)
                    obj["name"] = new_name;
                if (set_type)
                    obj["type"] = new_type;
                if (set_pin)
                {
                    if (new_pin < 0)
                        obj["pin"] = -1;
                    else
                        obj["pin"] = (unsigned)new_pin;
                }
                if (set_addr)
                    obj["addr"] = new_addr;
                if (set_enabled)
                    obj["enabled"] = enabled;
                if (set_group)
                    obj["group_id"] = group_id;
                char payload[256] = {};
                const size_t len = serializeJson(doc, payload, sizeof(payload));
                if (len == 0 || !web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                           reinterpret_cast<const uint8_t *>(payload), len))
                {
                    web._meteo_status = String("Send failed for sensor ") + idx;
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
                        if (set_enabled)
                            dst.enabled = enabled;
                        if (set_group)
                            dst.group_id = group_id;
                        if (set_name)
                        {
                            const char *src = new_name.c_str();
                            size_t p = 0;
                            for (; p + 1 < sizeof(dst.name) && src[p]; ++p)
                                dst.name[p] = src[p];
                            dst.name[p] = '\0';
                        }
                        if (set_type)
                        {
                            const char *src = new_type.c_str();
                            size_t p = 0;
                            for (; p + 1 < sizeof(dst.type) && src[p]; ++p)
                                dst.type[p] = src[p];
                            dst.type[p] = '\0';
                        }
                        if (set_pin)
                            dst.pin = new_pin;
                        if (set_addr)
                        {
                            const char *src = new_addr.c_str();
                            size_t p = 0;
                            for (; p + 1 < sizeof(dst.addr) && src[p]; ++p)
                                dst.addr[p] = src[p];
                            dst.addr[p] = '\0';
                        }
                        dst.group_id = group_id;
                        break;
                    }
                    cache_mut->updated_ms = millis();
                }
            }
            if (changed)
            {
                web._meteo_status = "Updated";
            }
            else
            {
                web._meteo_status = "Saved";
            }
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        MeteoController &meteo = web._controllers->meteo();
        auto meteo_guard = meteo.lockGuard();
        bool ok = true;
        bool changed = false;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("m") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String type_key = prefix + "type";
            const String pin_key = prefix + "pin";
            const String addr_key = prefix + "addr";
            const String src_key = prefix + "src";
            const String group_key = prefix + "group";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(type_key, true) ||
                                 request->hasParam(pin_key, true) ||
                                 request->hasParam(addr_key, true) ||
                                 request->hasParam(group_key, true) ||
                                 request->hasParam(src_key, true);
            if (!has_any)
                continue;
            if (!web.webAclCanControlItem_(UsersRegistry::AclController::Meteo, cfg->id))
            {
                web._meteo_status = String("ACL deny item: ") + idx;
                web.sendRedirect_(request, "/meteo", set_cookie);
                return;
            }

            const bool enabled = request->hasParam(en_key, true);
            String name = web.paramValue_(request, name_key);
            name.trim();
            const String type_str = web.paramValue_(request, type_key);
            const String pin_str = web.paramValue_(request, pin_key);
            const String addr_str = web.paramValue_(request, addr_key);
            const String src_str = web.paramValue_(request, src_key);
            const uint8_t group_id = web.parseGroupIdParam_(request, group_key);

            MeteoController::SensorType type = MeteoController::SensorType::None;
            if (!enabled)
            {
                if (cfg->enabled != enabled)
                {
                    meteo.setEnabled(cfg->id, enabled);
                    changed = true;
                }
                continue;
            }
            if (!web.parseMeteoType_(type_str, type))
            {
                ok = false;
                web._meteo_status = String("Invalid type for sensor ") + idx;
                break;
            }

            uint8_t pin = MeteoController::kInvalidPin;
            if (!web.parseMeteoPin_(pin_str, pin))
            {
                ok = false;
                web._meteo_status = String("Invalid pin for sensor ") + idx;
                break;
            }

            uint8_t addr[MeteoController::kAddrLen] = {};
            bool addr_set = false;
            if (!web.parseMeteoAddr_(addr_str, addr, addr_set))
            {
                ok = false;
                web._meteo_status = String("Invalid addr for sensor ") + idx;
                break;
            }

            uint8_t src_id = 0;
            uint32_t src_node = 0;
            if (src_str.length() > 0 && !web.parseThermoSensor_(src_str, src_id, src_node))
            {
                ok = false;
                web._meteo_status = String("Invalid source for sensor ") + idx;
                break;
            }
            if (src_node != 0 && name.length() == 0)
            {
                const String remote_name = web.meteoRemoteSensorName_(src_node, src_id);
                if (remote_name.length())
                    name = remote_name;
            }

            if (cfg->enabled != enabled)
            {
                meteo.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->name != name)
            {
                meteo.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->group_id != group_id)
            {
                meteo.setGroupId(cfg->id, group_id);
                changed = true;
            }
            if (cfg->type != type)
            {
                meteo.setType(cfg->id, type);
                changed = true;
            }
            if (type == MeteoController::SensorType::Dht22)
            {
                if (cfg->dht_pin != pin)
                {
                    meteo.setDht22Pin(cfg->id, pin);
                    changed = true;
                }
            }
            else if (type == MeteoController::SensorType::Ds18b20)
            {
                const bool addr_equal = (cfg->ds18_addr_set == addr_set) &&
                                        (!addr_set || (memcmp(cfg->ds18_addr, addr, MeteoController::kAddrLen) == 0));
                if (!addr_equal)
                {
                    meteo.setDs18b20Addr(cfg->id, addr, addr_set);
                    changed = true;
                }
            }
            if (cfg->source_node_id != src_node || cfg->source_sensor_id != src_id)
            {
                meteo.setRemoteSource(cfg->id, src_node, src_id);
                changed = true;
            }
        }

        if (ok)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._meteo_status = "Config manager missing";
            }
            else if (changed && !web._configs_manager->save())
            {
                ok = false;
                web._meteo_status = "Save failed";
            }
        }
        if (ok)
            web._meteo_status = changed ? "Updated" : "Saved";
        web.sendRedirect_(request, "/meteo", set_cookie);
    }
