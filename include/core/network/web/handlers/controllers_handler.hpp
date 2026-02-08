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

class ControllersHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/controllers", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleControllersSave(web, request); });
        server.on("/controllers", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleControllers(web, request); });
    }

    static void handleControllers(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceControllersHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", web.navHtml_());
        if (web._controllers)
        {
            const bool enabled = web._controllers->sockets().controllerEnabled();
            page.replace("%SOCKETS_ENABLED_CHECKED%", enabled ? "checked" : "");
            page.replace("%SOCKETS_ENABLED_LABEL%", enabled ? "включены" : "выключены");
            const bool lights_enabled = web._controllers->sockets().lightsEnabled();
            page.replace("%LIGHTS_ENABLED_CHECKED%", lights_enabled ? "checked" : "");
            page.replace("%LIGHTS_ENABLED_LABEL%", lights_enabled ? "включены" : "выключены");
            const bool meteo_enabled = web._controllers->meteo().controllerEnabled();
            page.replace("%METEO_ENABLED_CHECKED%", meteo_enabled ? "checked" : "");
            page.replace("%METEO_ENABLED_LABEL%", meteo_enabled ? "включено" : "выключено");
            const bool thermo_enabled = web._controllers->thermo().controllerEnabled();
            page.replace("%THERMO_ENABLED_CHECKED%", thermo_enabled ? "checked" : "");
            page.replace("%THERMO_ENABLED_LABEL%", thermo_enabled ? "включено" : "выключено");
            const bool tanks_enabled = web._controllers->tanks().controllerEnabled();
            page.replace("%TANKS_ENABLED_CHECKED%", tanks_enabled ? "checked" : "");
            page.replace("%TANKS_ENABLED_LABEL%", tanks_enabled ? "включены" : "выключены");
            const bool septic_enabled = web._controllers->septic().controllerEnabled();
            page.replace("%SEPTIC_ENABLED_CHECKED%", septic_enabled ? "checked" : "");
            page.replace("%SEPTIC_ENABLED_LABEL%", septic_enabled ? "включены" : "выключены");
            const bool ring_enabled = web._controllers->ring().controllerEnabled();
            page.replace("%RING_ENABLED_CHECKED%", ring_enabled ? "checked" : "");
            page.replace("%RING_ENABLED_LABEL%", ring_enabled ? "включен" : "выключен");
            const bool security_enabled = web._controllers->security().controllerEnabled();
            page.replace("%SECURITY_ENABLED_CHECKED%", security_enabled ? "checked" : "");
            const bool watering_enabled = web._controllers->watering().controllerEnabled();
            page.replace("%WATERING_ENABLED_CHECKED%", watering_enabled ? "checked" : "");
            page.replace("%SECURITY_ENABLED_LABEL%", security_enabled ? "включена" : "выключена");
            page.replace("%WATERING_ENABLED_LABEL%", watering_enabled ? "включен" : "выключен");
        }
        else
        {
            page.replace("%SOCKETS_ENABLED_CHECKED%", "");
            page.replace("%SOCKETS_ENABLED_LABEL%", "недоступно");
            page.replace("%LIGHTS_ENABLED_CHECKED%", "");
            page.replace("%LIGHTS_ENABLED_LABEL%", "недоступно");
            page.replace("%METEO_ENABLED_CHECKED%", "");
            page.replace("%METEO_ENABLED_LABEL%", "недоступно");
            page.replace("%THERMO_ENABLED_CHECKED%", "");
            page.replace("%THERMO_ENABLED_LABEL%", "недоступно");
            page.replace("%TANKS_ENABLED_CHECKED%", "");
            page.replace("%TANKS_ENABLED_LABEL%", "недоступно");
            page.replace("%SEPTIC_ENABLED_CHECKED%", "");
            page.replace("%SEPTIC_ENABLED_LABEL%", "недоступно");
            page.replace("%RING_ENABLED_CHECKED%", "");
            page.replace("%RING_ENABLED_LABEL%", "");
            page.replace("%SECURITY_ENABLED_CHECKED%", "");
            page.replace("%SECURITY_ENABLED_LABEL%", "недоступно");
            page.replace("%WATERING_ENABLED_CHECKED%", "");
            page.replace("%WATERING_ENABLED_LABEL%", "недоступно");
        }
        page.replace("%SOCKETS_STATUS%", web._sockets_status);
        page.replace("%LIGHTS_STATUS%", web._lights_status);
        page.replace("%METEO_STATUS%", web._meteo_status);
        page.replace("%THERMO_STATUS%", web._thermo_status);
        page.replace("%TANKS_STATUS%", web._tanks_status);
        page.replace("%SEPTIC_STATUS%", web._septic_status);
        page.replace("%RING_STATUS%", web._ring_status);
        page.replace("%SECURITY_STATUS%", web._security_status);
        page.replace("%WATERING_STATUS%", web._watering_status);
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleControllersSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web._controllers)
        {
            web._controllers_status = "Контроллеры недоступны";
            web.sendRedirect_(request, "/controllers", set_cookie);
            return;
        }
        const String ctrl = web.paramValue_(request, "ctrl");
        bool changed = false;
        if (ctrl.length() == 0 || ctrl == "sockets")
        {
            const bool enabled = request->hasParam("sockets_enabled", true);
            if (web._controllers->sockets().controllerEnabled() != enabled)
            {
                web._controllers->sockets().setControllerEnabled(enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "lights")
        {
            const bool lights_enabled = request->hasParam("lights_enabled", true);
            if (web._controllers->sockets().lightsEnabled() != lights_enabled)
            {
                web._controllers->sockets().setLightsEnabled(lights_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "meteo")
        {
            const bool meteo_enabled = request->hasParam("meteo_enabled", true);
            if (web._controllers->meteo().controllerEnabled() != meteo_enabled)
            {
                web._controllers->meteo().setControllerEnabled(meteo_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "thermo")
        {
            const bool thermo_enabled = request->hasParam("thermo_enabled", true);
            if (web._controllers->thermo().controllerEnabled() != thermo_enabled)
            {
                web._controllers->thermo().setControllerEnabled(thermo_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "tanks")
        {
            const bool tanks_enabled = request->hasParam("tanks_enabled", true);
            if (web._controllers->tanks().controllerEnabled() != tanks_enabled)
            {
                web._controllers->tanks().setControllerEnabled(tanks_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "septic")
        {
            const bool septic_enabled = request->hasParam("septic_enabled", true);
            if (web._controllers->septic().controllerEnabled() != septic_enabled)
            {
                web._controllers->septic().setControllerEnabled(septic_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "ring")
        {
            const bool ring_enabled = request->hasParam("ring_enabled", true);
            if (web._controllers->ring().controllerEnabled() != ring_enabled)
            {
                web._controllers->ring().setControllerEnabled(ring_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "watering")
        {
            const bool watering_enabled = request->hasParam("watering_enabled", true);
            if (web._controllers->watering().controllerEnabled() != watering_enabled)
            {
                web._controllers->watering().setControllerEnabled(watering_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "security")
        {
            const bool security_enabled = request->hasParam("security_enabled", true);
            if (web._controllers->security().controllerEnabled() != security_enabled)
            {
                web._controllers->security().setControllerEnabled(security_enabled);
                changed = true;
            }
        }
        bool ok = true;
        if (changed)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._controllers_status = "Config manager missing";
            }
            else if (!web._configs_manager->save())
            {
                ok = false;
                web._controllers_status = "Save failed";
            }
        }
        if (ok)
            web._controllers_status = changed ? "Updated" : "No changes";
        web._sockets_status = web._controllers_status;
        web._lights_status = web._controllers_status;
        web._meteo_status = web._controllers_status;
        web._thermo_status = web._controllers_status;
        web._tanks_status = web._controllers_status;
        web._septic_status = web._controllers_status;
        web._ring_status = web._controllers_status;
        web._security_status = web._controllers_status;
        web._watering_status = web._controllers_status;
        web.sendRedirect_(request, "/controllers", set_cookie);
    }
};
