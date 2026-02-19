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

class TankHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/tanks/toggle", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleTanksToggle(web, request); });
        server.on("/tanks/toggle", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleTanksToggle(web, request); });
        server.on("/tanks", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleTanksSave(web, request); });
        server.on("/tanks", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleTanks(web, request); });
    }

    static void handleTanks(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (!web.requireWebAclController_(request, &set_cookie, UsersRegistry::AclController::Tanks, node_id))
            return;
        const bool stack_view = web.isStackTanksView_(node_id);
        if (stack_view)
            web.requestStackTanks_(node_id);
        String page = FPSTR(kWebInterfaceTanksHtml);
        page.reserve(page.length() + 16384);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%TANK_STATUS%", stack_view ? web.stackTanksStatusText_(node_id) : web._tanks_status);
        page.replace("%TANK_ITEMS%", stack_view ? web.listStackTanksHtml_(node_id) : web.listTanksHtml_());
        page.replace("%TANK_DINPUT_JSON%", stack_view ? "[]" : web.tankPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%TANK_RELAY_JSON%", stack_view ? "[]" : web.tankPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%TANK_DINPUT_USED_JSON%",
                     stack_view ? "[]" : web.globalUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%TANK_RELAY_USED_JSON%",
                     stack_view ? "[]" : web.globalUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%TANK_DEVICE_SELECT%", web.tanksDeviceSelectHtml_(node_id, stack_view));
        page.replace("%TANK_SAVE_BTN%",
                     (stack_view || !web.webSessionIsAdmin_()) ? "" : "<button type=\"submit\">Сохранить</button>");
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleTanksSave(WebInterface &web, AsyncWebServerRequest *request)
    {
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
            web._tanks_status = "Р”РѕСЃС‚СѓРїРЅРѕ С‚РѕР»СЊРєРѕ РЅР° Р»РѕРєР°Р»СЊРЅРѕРј СѓСЃС‚СЂРѕР№СЃС‚РІРµ";
            web.sendRedirect_(request, "/tanks", set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        TankController &tanks = web._controllers->tanks();
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
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(power_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(low_key, true) ||
                                 request->hasParam(mid_key, true) ||
                                 request->hasParam(full_key, true) ||
                                 request->hasParam(valve_key, true) ||
                                 request->hasParam(pump_key, true) ||
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
                web._tanks_status = String("РќРµРІРµСЂРЅС‹Р№ РїРѕСЂС‚ РґР»СЏ Р±Р°РєР° ") + idx;
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
                    web._tanks_status = "РњРµРЅРµРґР¶РµСЂ РєРѕРЅС„РёРіСѓСЂР°С†РёРё РЅРµРґРѕСЃС‚СѓРїРµРЅ";
                }
                else if (!web._configs_manager->save())
                {
                    ok = false;
                    web._tanks_status = "РЎРѕС…СЂР°РЅРµРЅРёРµ РЅРµ СѓРґР°Р»РѕСЃСЊ";
                }
            }
        }
        if (ok)
            web._tanks_status = changed ? "РћР±РЅРѕРІР»РµРЅРѕ" : "РЎРѕС…СЂР°РЅРµРЅРѕ";
        web.sendRedirect_(request, "/tanks", set_cookie);
    }

    static void handleTanksToggle(WebInterface &web, AsyncWebServerRequest *request)
    {
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
            if (!web._stack_master)
            {
                web.sendText_(request, 400, "text/plain", "Stack master missing", set_cookie);
                return;
            }
            auto *cache = web._stack_cache ? web._stack_cache->tanksCache(node_id) : nullptr;
            StackCache::StackTankItem *item = nullptr;
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
                const bool stale = (!cache || !cache->has_data || cache->pending ||
                                    (uint32_t)(millis() - cache->updated_ms) > 1500u);
                if (stale)
                {
                    if (web._stack_cache)
                        web._stack_cache->requestTanks(node_id);
                    web.sendText_(request, 200, "text/plain", "pending", set_cookie);
                    return;
                }
                if (!item)
                {
                    web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
                    return;
                }
                send_state(item->power_on, item->level_low, item->level_mid, item->level_full,
                           item->levels_ok, item->valve_on, item->pump_on, item->alarm_on);
                return;
            }

            StaticJsonDocument<192> doc;
            doc["cmd_id"] = 0;
            doc["feature"] = (uint8_t)StackFeature::Tanks;
            doc["action"] = "set";
            JsonArray items = doc["params"]["items"].to<JsonArray>();
            JsonObject o = items.add<JsonObject>();
            o["id"] = id;
            if (action == "on" || action == "off")
                o["power"] = (action == "on");
            else
                o["toggle"] = true;
            char payload[192] = {};
            const size_t len = serializeJson(doc, payload, sizeof(payload));
            if (len == 0 || !web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                       reinterpret_cast<const uint8_t *>(payload), len))
            {
                web.sendText_(request, 400, "text/plain", "Send failed", set_cookie);
                return;
            }
            if (web._stack_cache)
                web._stack_cache->requestTanks(node_id);
            web.sendText_(request, 200, "text/plain", "pending", set_cookie);
            return;
        }

        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        TankController &tanks = web._controllers->tanks();
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
};
