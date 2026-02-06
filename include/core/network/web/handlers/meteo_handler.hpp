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

class MeteoHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/meteo", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleMeteoSave(web, request); });
        server.on("/meteo", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleMeteo(web, request); });
    }

    static void handleMeteo(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = web.isStackMeteoView_(node_id);
        if (stack_view)
            web.requestStackMeteo_(node_id);
        if (!stack_view)
        {
            if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
            {
                web._stack_slave->requestRemoteMeteoAll();
            }
            else if (web.stackRole_() == ConfigsManagerIface::StackRole::Master)
            {
                const size_t count = web._stack_master ? web._stack_master->nodeCount() : 0;
                for (size_t i = 0; i < count; ++i)
                {
                    const uint32_t id = web._stack_master->nodeIdAt(i);
                    if (id != 0)
                        web.requestStackMeteo_(id);
                }
            }
        }
        String page = FPSTR(kWebInterfaceMeteoHtml);
        page.reserve(page.length() + 16384);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%METEO_TILES%", stack_view ? web.listStackMeteoHtml_(node_id) : web.listMeteoHtml_());
        page.replace("%METEO_STATUS%", stack_view ? web.stackMeteoStatusText_(node_id) : web._meteo_status);
        page.replace("%SENSOR_JSON%", stack_view ? "[]" : web.meteoPortOptionsJson_());
        page.replace("%SENSOR_USED_JSON%",
                     stack_view ? "[]" : web.globalUsedPortsJson_(PortIO::PinType::Sensor));
        page.replace("%METEO_DEVICE_SELECT%", web.meteoDeviceSelectHtml_(node_id, stack_view));
        page.replace("%METEO_SAVE_BTN%", stack_view ? "" : "<button class=\"btn\" type=\"submit\">Сохранить</button>");
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleMeteoSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackMeteoView_(node_id))
        {
            web._meteo_status = "Доступно только на локальном устройстве";
            web.sendRedirect_(request, "/meteo", set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        MeteoController &meteo = web._controllers->meteo();
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
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(type_key, true) ||
                                 request->hasParam(pin_key, true) ||
                                 request->hasParam(addr_key, true) ||
                                 request->hasParam(src_key, true);
            if (!has_any)
                continue;

            const bool enabled = request->hasParam(en_key, true);
            String name = web.paramValue_(request, name_key);
            name.trim();
            const String type_str = web.paramValue_(request, type_key);
            const String pin_str = web.paramValue_(request, pin_key);
            const String addr_str = web.paramValue_(request, addr_key);
            const String src_str = web.paramValue_(request, src_key);

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
};
