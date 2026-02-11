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

class LeakHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/leak", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleLeakSave(web, request); });
        server.on("/leak", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleLeak(web, request); });
    }

    static void handleLeak(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = isStackLeakView_(web, node_id);
        if (stack_view && web._stack_cache)
            web.stackCache().requestLeak(node_id);
        String page = FPSTR(kWebInterfaceLeakHtml);
        page.reserve(page.length() + 8192);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%LEAK_DEVICE_SELECT%", leakDeviceSelectHtml_(web, node_id, stack_view));
        page.replace("%LEAK_STATUS%", stack_view ? stackLeakStatusText_(web, node_id) : web._leak_status);
        page.replace("%LEAK_DINPUT_JSON%", web.socketPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%LEAK_RELAY_JSON%", web.socketPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%LEAK_DINPUT_USED_JSON%", stack_view ? "[]" : web.globalUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%LEAK_RELAY_USED_JSON%", stack_view ? "[]" : web.globalUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%LEAK_ROWS%", buildRows_(web, node_id, stack_view));
        page.replace("%LEAK_FORM_ACTION%", stack_view ? leakRedirectPath_(node_id, true) : "/leak");
        page.replace("%LEAK_ACK_FORM_ACTION%", stack_view ? leakRedirectPath_(node_id, true) : "/leak");
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleLeakSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = isStackLeakView_(web, node_id);
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Контроллеры недоступны", set_cookie);
            return;
        }
        LeakController &leak = web._controllers->leak();
        if (request->hasParam("leak_ack_all", true))
        {
            if (stack_view)
            {
                if (sendStackLeakSet_(web, node_id, nullptr, true))
                    web._leak_status = "Команда отправлена";
                else
                    web._leak_status = "Ошибка отправки";
            }
            else
            {
                leak.ackAll();
                web._leak_status = "Сброс тревог выполнен";
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
                err = "Некорректный порт датчика";
                break;
            }
            if (!web.parseSocketPort_(web.paramValue_(request, valve_name.c_str()), valve))
            {
                ok = false;
                err = "Некорректный порт крана";
                break;
            }
            if (!web.parseSocketPort_(web.paramValue_(request, alarm_name.c_str()), alarm))
            {
                ok = false;
                err = "Некорректный порт тревоги";
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
                web._leak_status = "Команда отправлена";
            else
                web._leak_status = "Ошибка отправки";
            web.sendRedirect_(request, leakRedirectPath_(node_id, true), set_cookie);
            return;
        }

        bool saved = true;
        if (changed)
        {
            if (!web._configs_manager)
            {
                saved = false;
                web._leak_status = "Менеджер конфигурации недоступен";
            }
            else if (!web._configs_manager->save())
            {
                saved = false;
                web._leak_status = "Ошибка сохранения";
            }
        }
        if (saved)
            web._leak_status = changed ? "Обновлено" : "Без изменений";
        web.sendRedirect_(request, leakRedirectPath_(node_id, false), set_cookie);
    }

private:
    static bool isStackLeakView_(WebInterface &web, uint32_t node_id)
    {
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

    static String leakRedirectPath_(uint32_t node_id, bool stack_view)
    {
        if (!stack_view)
            return "/leak";
        String path = "/leak?node=";
        path += String((unsigned long)node_id);
        path += "&unit=stack";
        return path;
    }

    static String leakDeviceSelectHtml_(WebInterface &web, uint32_t selected_node_id, bool stack_view)
    {
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\" style=\"margin-bottom:10px;\">";
        html += "<label>Устройство</label>";
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

    static String stackLeakStatusText_(WebInterface &web, uint32_t node_id)
    {
        if (!web._stack_cache)
            return "Стек-кэш недоступен";
        const auto *cache = web.stackCache().leakCache(node_id);
        if (!cache)
            return "Нет данных со слейва";
        if (cache->pending)
            return "Запрос данных со слейва...";
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = "Ошибка: ";
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return "Нет данных со слейва";
        return "OK";
    }

    static bool sendStackLeakSet_(WebInterface &web, uint32_t node_id, JsonArray *zones, bool ack_all)
    {
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

    static String paramName_(const char *prefix, size_t id)
    {
        String out(prefix);
        out += String((unsigned)id);
        return out;
    }

    static String checked_(bool value)
    {
        return value ? " checked" : "";
    }

    static String portValue_(uint8_t port)
    {
        if (port == LeakController::kInvalidPort)
            return String();
        return String((unsigned)port);
    }

    static String buildRows_(WebInterface &web, uint32_t node_id, bool stack_view)
    {
        if (!web._controllers)
            return "<div class=\"tile tile-empty\">Контроллеры недоступны</div>";
        LeakController &leak = web._controllers->leak();
        String rows;
        rows.reserve(LeakController::kZoneCount * 1200);
        const StackCache::StackLeakCache *stack_cache = nullptr;
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
        for (size_t i = 0; i < render_count; ++i)
        {
            const size_t id = i + 1;
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
            const bool alert = st_wet || st_latched;
            rows += "<div class=\"tile";
            rows += cfg_enabled ? "" : " disabled";
            rows += alert ? " alert" : "";
            rows += "\"><div class=\"tile-head\"><div class=\"tile-left\"><svg class=\"leak-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            rows += "<path fill=\"currentColor\" d=\"M32 8c8 12 18 24 18 34 0 9.9-8.1 18-18 18s-18-8.1-18-18c0-10 10-22 18-34z\"/>";
            rows += "<path d=\"M32 14l14 5v11c0 9.8-5.8 18.4-14 21.8-8.2-3.4-14-12-14-21.8V19l14-5z\" fill=\"none\" stroke=\"#0b1220\" stroke-width=\"3\"/>";
            rows += "</svg><div class=\"tile-id\">Зона ";
            rows += String((unsigned)id);
            rows += "</div></div><div class=\"badge-row\"><span class=\"badge\">Вода:";
            rows += st_wet ? "1" : "0";
            rows += "</span><span class=\"badge\">Фиксация:";
            rows += st_latched ? "1" : "0";
            rows += "</span></div></div><div class=\"tile-grid\">";

            rows += "<div class=\"field-row full\"><label class=\"field-label\">Имя</label><input type=\"text\" maxlength=\"28\" name=\"leak_name_";
            rows += String((unsigned)id);
            rows += "\" value=\"";
            web.appendHtmlEscaped_(rows, cfg_name.c_str());
            rows += "\"></div>";

            rows += "<div class=\"field-row\"><label class=\"field-label\">Датчик</label><select class=\"leak-select\" data-type=\"dinput\" data-selected=\"";
            rows += portValue_(cfg_sensor);
            rows += "\" name=\"leak_sensor_";
            rows += String((unsigned)id);
            rows += "\"></select></div>";

            rows += "<div class=\"field-row\"><label class=\"field-label\">Кран</label><select class=\"leak-select\" data-type=\"relay\" data-selected=\"";
            rows += portValue_(cfg_valve);
            rows += "\" name=\"leak_valve_";
            rows += String((unsigned)id);
            rows += "\"></select></div>";

            rows += "<div class=\"field-row\"><label class=\"field-label\">Тревога</label><select class=\"leak-select\" data-type=\"relay\" data-selected=\"";
            rows += portValue_(cfg_alarm);
            rows += "\" name=\"leak_alarm_";
            rows += String((unsigned)id);
            rows += "\"></select></div>";

            rows += "<div class=\"checks\">";
            rows += "<label><input type=\"checkbox\" name=\"leak_en_";
            rows += String((unsigned)id);
            rows += "\"";
            rows += checked_(cfg_enabled);
            rows += "> ВКЛ</label>";

            rows += "<label><input type=\"checkbox\" name=\"leak_pwr_";
            rows += String((unsigned)id);
            rows += "\"";
            rows += checked_(cfg_power);
            rows += "> ПИТ</label>";

            rows += "<label><input type=\"checkbox\" name=\"leak_al_";
            rows += String((unsigned)id);
            rows += "\"";
            rows += checked_(cfg_active_low);
            rows += "> Активный ноль</label>";

            rows += "</div></div></div>";
        }
        if (rows.length() == 0 && stack_view)
            return "<div class=\"tile tile-empty\">Ожидаем данные со слейва</div>";
        if (rows.length() == 0)
            return "<div class=\"tile tile-empty\">Нет зон протечки</div>";
        return rows;
    }
};

