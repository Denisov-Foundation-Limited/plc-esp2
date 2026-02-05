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

class ThermoHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/thermo", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleThermoSave(web, request); });
        server.on("/thermo", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleThermo(web, request); });
    }

    static void handleThermo(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = web.isStackThermoView_(node_id);
        if (stack_view)
        {
            web.requestStackThermo_(node_id);
            web.requestStackMeteo_(node_id);
        }
        else if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave)
        {
            if (web._stack_slave)
                web._stack_slave->requestRemoteMeteoAll();
        }
        else if (web.stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = web._stack_master ? web._stack_master->nodeCount() : 0;
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t id = web._stack_master->nodeIdAt(i);
                if (id != 0)
                    web.stackCache().requestMeteo(id);
            }
        }
        String page = FPSTR(kWebInterfaceThermoHtml);
        page.reserve(page.length() + 16384);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%THERMO_ROWS%", stack_view ? web.listStackThermoHtml_(node_id) : web.listThermoHtml_());
        page.replace("%THERMO_STATUS%", stack_view ? web.stackThermoStatusText_(node_id) : web._thermo_status);
        page.replace("%THERMO_DINPUT_JSON%", stack_view ? "[]" : web.thermoPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%THERMO_RELAY_JSON%", stack_view ? "[]" : web.thermoPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%THERMO_DINPUT_USED_JSON%",
                     stack_view ? "[]" : web.thermoUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%THERMO_RELAY_USED_JSON%",
                     stack_view ? "[]" : web.thermoUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%THERMO_DEVICE_SELECT%", web.thermoDeviceSelectHtml_(node_id, stack_view));
        page.replace("%THERMO_SAVE_BTN%", stack_view ? "" : "<button type=\"submit\">Сохранить</button>");
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleThermoSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackThermoView_(node_id))
        {
            web._thermo_status = "Доступно только на локальном устройстве";
            web.sendRedirect_(request, "/thermo", set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        ThermoController &thermo = web._controllers->thermo();
        uint8_t sensor_used[MeteoController::kSensorCount + 1] = {};
        uint32_t remote_used[ThermoController::kDeviceCount] = {};
        size_t remote_used_count = 0;
        bool ok = true;
        bool changed = false;
        bool power_changed = false;
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("t") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String sensor_key = prefix + "sensor";
            const String mode_key = prefix + "mode";
            const String target_key = prefix + "target";
            const String hyst_key = prefix + "hyst";
            const String heat_key = prefix + "heat";
            const String cool_key = prefix + "cool";
            const String button_key = prefix + "button";
            const String power_key = prefix + "power";
            const String en_force_key = prefix + "en_force";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(sensor_key, true) ||
                                 request->hasParam(mode_key, true) ||
                                 request->hasParam(target_key, true) ||
                                 request->hasParam(hyst_key, true) ||
                                 request->hasParam(heat_key, true) ||
                                 request->hasParam(cool_key, true) ||
                                 request->hasParam(button_key, true) ||
                                 request->hasParam(power_key, true);
            if (!has_any)
                continue;

            const String en_force_str = web.paramValue_(request, en_force_key);
            const bool en_force_known = (en_force_str == "1" || en_force_str == "0" ||
                                         en_force_str == "true" || en_force_str == "false" ||
                                         en_force_str == "on" || en_force_str == "off");
            const bool en_force_on = (en_force_str == "1" || en_force_str == "true" || en_force_str == "on");
            const bool enabled = en_force_known ? en_force_on : request->hasParam(en_key, true);
            const String sensor_str = web.paramValue_(request, sensor_key);
            const String mode_str = web.paramValue_(request, mode_key);
            const String target_str = web.paramValue_(request, target_key);
            const String hyst_str = web.paramValue_(request, hyst_key);
            const String heat_str = web.paramValue_(request, heat_key);
            const String cool_str = web.paramValue_(request, cool_key);
            const String button_str = web.paramValue_(request, button_key);
            const String power_str = web.paramValue_(request, power_key);
            String name = web.paramValue_(request, name_key);
            name.trim();
            const bool has_power = (power_str == "on" || power_str == "off" || power_str == "1" || power_str == "0" ||
                                    power_str == "true" || power_str == "false");
            const bool power_on = (power_str == "on" || power_str == "1" || power_str == "true");

            uint8_t sensor_id = ThermoController::kInvalidSensor;
            uint32_t sensor_node_id = 0;
            if (!web.parseThermoSensor_(sensor_str, sensor_id, sensor_node_id))
            {
                ok = false;
                web._thermo_status = String("Invalid sensor for device ") + idx;
                break;
            }
            if (enabled && sensor_id != ThermoController::kInvalidSensor)
            {
                if (sensor_node_id == 0)
                {
                    if (!web.isMeteoSensorActive_(sensor_id))
                    {
                        ok = false;
                        web._thermo_status = String("?????? ?? ??????? (") + idx + ")";
                        break;
                    }
                    if (sensor_id <= MeteoController::kSensorCount && sensor_used[sensor_id])
                    {
                        ok = false;
                        web._thermo_status = String("?????? ??? ???????????? (") + idx + ")";
                        break;
                    }
                    if (sensor_id <= MeteoController::kSensorCount)
                        sensor_used[sensor_id] = 1;
                }
                else
                {
                    if (web.stackRole_() != ConfigsManagerIface::StackRole::Master &&
                        web.stackRole_() != ConfigsManagerIface::StackRole::Slave)
                    {
                        ok = false;
                        web._thermo_status = String("?????? ?????????? (") + idx + ")";
                        break;
                    }
                    if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && !web._stack_slave)
                    {
                        ok = false;
                        web._thermo_status = String("?????? ?????????? (") + idx + ")";
                        break;
                    }
                    if (!web.isRemoteMeteoSensorActive_(sensor_node_id, sensor_id))
                    {
                        ok = false;
                        web._thermo_status = String("?????? ?? ??????? (") + idx + ")";
                        break;
                    }
                    const uint32_t key = (sensor_node_id << 8) | sensor_id;
                    bool used = false;
                    for (size_t k = 0; k < remote_used_count; ++k)
                    {
                        if (remote_used[k] == key)
                        {
                            used = true;
                            break;
                        }
                    }
                    if (used)
                    {
                        ok = false;
                        web._thermo_status = String("?????? ??? ???????????? (") + idx + ")";
                        break;
                    }
                    if (remote_used_count < ThermoController::kDeviceCount)
                        remote_used[remote_used_count++] = key;
                }
            }

            ThermoController::Mode mode = ThermoController::Mode::Off;
            if (!web.parseThermoMode_(mode_str, mode))
            {
                ok = false;
                web._thermo_status = String("Invalid mode for device ") + idx;
                break;
            }

            float target = cfg->target_c;
            if (!web.parseThermoFloat_(target_str, target))
            {
                ok = false;
                web._thermo_status = String("Invalid target for device ") + idx;
                break;
            }

            float hyst = cfg->hysteresis;
            if (!web.parseThermoFloat_(hyst_str, hyst))
            {
                ok = false;
                web._thermo_status = String("Invalid hyst for device ") + idx;
                break;
            }

            uint8_t heat_port = ThermoController::kInvalidPort;
            uint8_t cool_port = ThermoController::kInvalidPort;
            uint8_t button_port = ThermoController::kInvalidPort;
            if (!web.parseSocketPort_(heat_str, heat_port) ||
                !web.parseSocketPort_(cool_str, cool_port) ||
                !web.parseSocketPort_(button_str, button_port))
            {
                ok = false;
                web._thermo_status = String("Invalid port for device ") + idx;
                break;
            }

            if (cfg->enabled != enabled)
            {
                thermo.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->name != name)
            {
                thermo.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->sensor_id != sensor_id || cfg->sensor_node_id != sensor_node_id)
            {
                if (sensor_node_id != 0)
                    thermo.setSensorSource(cfg->id, sensor_node_id, sensor_id);
                else
                    thermo.setSensor(cfg->id, sensor_id);
                changed = true;
            }
            if (cfg->mode != mode)
            {
                thermo.setMode(cfg->id, mode);
                changed = true;
            }
            if (cfg->target_c != target)
            {
                thermo.setTarget(cfg->id, target);
                changed = true;
            }
            if (cfg->hysteresis != hyst)
            {
                thermo.setHysteresis(cfg->id, hyst);
                changed = true;
            }
            if (cfg->heat_port != heat_port)
            {
                thermo.setHeatPort(cfg->id, heat_port);
                changed = true;
            }
            if (cfg->cool_port != cool_port)
            {
                thermo.setCoolPort(cfg->id, cool_port);
                changed = true;
            }
            if (cfg->button_port != button_port)
            {
                thermo.setButtonPort(cfg->id, button_port);
                changed = true;
            }
            if (has_power)
            {
                const auto *st = thermo.state(cfg->id);
                const bool cur_power = st ? st->power_on : true;
                if (cur_power != power_on)
                {
                    thermo.setPower(cfg->id, power_on, "web");
                    power_changed = true;
                }
            }
        }

        if (ok)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._thermo_status = "Config manager missing";
            }
            else if (changed && !web._configs_manager->save())
            {
                ok = false;
                web._thermo_status = "Save failed";
            }
        }
        if (ok)
            web._thermo_status = (changed || power_changed) ? "Updated" : "Saved";
        web.sendRedirect_(request, "/thermo", set_cookie);
    }
};
