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

class SepticHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/septic", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleSepticSave(web, request); });
        server.on("/septic", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleSeptic(web, request); });
    }

    static void handleSeptic(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = web.isStackSepticView_(node_id);
        if (stack_view)
            web.requestStackSeptic_(node_id);
        String page = FPSTR(kWebInterfaceSepticHtml);
        page.reserve(page.length() + 4096);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        page.replace("%SEPTIC_STATUS%", stack_view ? web.stackSepticStatusText_(node_id) : web._septic_status);
        page.replace("%SEPTIC_DEVICE_SELECT%", web.septicDeviceSelectHtml_(node_id, stack_view));
        page.replace("%SEPTIC_SAVE_BTN%", stack_view ? "" : "<button type=\"submit\">Сохранить</button>");
        if (!web._controllers)
        {
            page.replace("%SEPTIC_ITEMS%", stack_view ? web.listStackSepticHtml_(node_id) : "");
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
            page.replace("%SEPTIC_WATER_LABEL%", "Уровень: 20%");
            web.sendHtml_(request, page, set_cookie);
            return;
        }
        page.replace("%SEPTIC_ITEMS%", stack_view ? web.listStackSepticHtml_(node_id) : web.listSepticHtml_());
        page.replace("%SEPTIC_DINPUT_JSON%", stack_view ? "[]" : web.septicPortOptionsJson_(PortIO::PinType::DInput));
        page.replace("%SEPTIC_RELAY_JSON%", stack_view ? "[]" : web.septicPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%SEPTIC_DINPUT_USED_JSON%", stack_view ? "[]" : web.septicUsedPortsJson_(PortIO::PinType::DInput));
        page.replace("%SEPTIC_RELAY_USED_JSON%", stack_view ? "[]" : web.septicUsedPortsJson_(PortIO::PinType::Relay));
        if (!stack_view)
        {
            SepticController &septic = web._controllers->septic();
            const auto *cfg = septic.configByIndex(0);
            const auto *st = septic.stateByIndex(0);
            const bool warn = st ? st->warning : false;
            const bool alarm = st ? st->alarm : false;
            const bool relay_warn = st ? st->relay_warning : false;
            const bool relay_alarm = st ? st->relay_alarm : false;
            const char *water_class = "water-low";
            const char *water_level = "20%";
            const char *water_label = "Уровень: 20%";
            if (alarm)
            {
                water_class = "water-alarm";
                water_level = "100%";
                water_label = "Уровень: 100%";
            }
            else if (warn)
            {
                water_class = "water-warn";
                water_level = "80%";
                water_label = "Уровень: 80%";
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
                page.replace("%SEPTIC_STATUS%", "Септик выключен");
        }
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleSepticSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackSepticView_(node_id))
        {
            web._septic_status = "Доступно только на локальном устройстве";
            web.sendRedirect_(request, "/septic", set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "Controllers unavailable", set_cookie);
            return;
        }
        SepticController &septic = web._controllers->septic();
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
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(warn_key, true) ||
                                 request->hasParam(alarm_key, true) ||
                                 request->hasParam(relay_warn_key, true) ||
                                 request->hasParam(relay_alarm_key, true) ||
                                 request->hasParam(monitor_key, true);
            if (!has_any)
                continue;
            const bool enabled = request->hasParam(en_key, true);
            const String monitor_val = web.paramValue_(request, monitor_key);
            const bool monitoring = monitor_val == "on" || monitor_val == "1" || monitor_val == "true";
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
        if (ok)
            web._septic_status = changed ? "Updated" : "Saved";
        web.sendRedirect_(request, "/septic", set_cookie);
    }
};
