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
        server.on("/tanks", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleTanksSave(web, request); });
        server.on("/tanks", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleTanks(web, request); });
    }

    static void handleTanks(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
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
        page.replace("%TANK_SAVE_BTN%", stack_view ? "" : "<button type=\"submit\">Сохранить</button>");
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleTanksSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackTanksView_(node_id))
        {
            web._tanks_status = "Доступно только на локальном устройстве";
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
                web._tanks_status = String("Неверный порт для бака ") + idx;
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
                    web._tanks_status = "Менеджер конфигурации недоступен";
                }
                else if (!web._configs_manager->save())
                {
                    ok = false;
                    web._tanks_status = "Сохранение не удалось";
                }
            }
        }
        if (ok)
            web._tanks_status = changed ? "Обновлено" : "Сохранено";
        web.sendRedirect_(request, "/tanks", set_cookie);
    }
};
