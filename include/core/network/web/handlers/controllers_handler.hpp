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
#include <LittleFS.h>
#include "utils/configs.hpp"

class WebInterface;
class AsyncWebServer;
class AsyncWebServerRequest;

class ControllersHandler
{
public:
    static bool hasStartupConfig_()
    {
        return LittleFS.exists(Configs::kPath);
    }

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
        const bool can_edit = web.webSessionIsAdmin_();
        const bool has_config = hasStartupConfig_();
        String page = FPSTR(kWebInterfaceControllersHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", web.navHtml_());
        const bool allow_sockets = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Sockets);
        const bool allow_lights = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Lights);
        const bool allow_meteo = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Meteo);
        const bool allow_thermo = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Thermo);
        const bool allow_tanks = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Tanks);
        const bool allow_watering = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Watering);
        const bool allow_septic = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Septic);
        const bool allow_ring = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Ring);
        const bool allow_security = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Security);
        const bool allow_avr = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Avr);
        const bool allow_leak = can_edit || web.webAclControllerAllowed_(UsersRegistry::AclController::Leak);

        bool show_sockets = allow_sockets;
        bool show_lights = allow_lights;
        bool show_meteo = allow_meteo;
        bool show_thermo = allow_thermo;
        bool show_tanks = allow_tanks;
        bool show_watering = allow_watering;
        bool show_septic = allow_septic;
        bool show_ring = allow_ring;
        bool show_security = allow_security;
        bool show_avr = allow_avr;
        bool show_leak = allow_leak;

        if (!can_edit)
        {
            const bool has_runtime = has_config && web._controllers;
            show_sockets = allow_sockets && has_runtime && web._controllers->sockets().controllerEnabled();
            show_lights = allow_lights && has_runtime && web._controllers->sockets().lightsEnabled();
            show_meteo = allow_meteo && has_runtime && web._controllers->meteo().controllerEnabled();
            show_thermo = allow_thermo && has_runtime && web._controllers->thermo().controllerEnabled();
            show_tanks = allow_tanks && has_runtime && web._controllers->tanks().controllerEnabled();
            show_watering = allow_watering && has_runtime && web._controllers->watering().controllerEnabled();
            show_septic = allow_septic && has_runtime && web._controllers->septic().controllerEnabled();
            show_ring = allow_ring && has_runtime && web._controllers->ring().controllerEnabled();
            show_security = allow_security && has_runtime && web._controllers->security().controllerEnabled();
            show_avr = allow_avr && has_runtime && web._controllers->avr().controllerEnabled();
            show_leak = allow_leak && has_runtime && web._controllers->leak().controllerEnabled();
        }

        const bool any_visible = show_sockets || show_lights || show_meteo || show_thermo || show_tanks ||
                                 show_watering || show_septic || show_ring || show_security || show_avr || show_leak;
        page.replace("%CONTROLLERS_EMPTY_HINT%",
                     (!can_edit && !any_visible)
                         ? "<p class=\"status\">Нет доступных контроллеров по ACL.</p>"
                         : "");
        page.replace("%ACL_HIDE_SOCKETS%", show_sockets ? "" : "display:none;");
        page.replace("%ACL_HIDE_LIGHTS%", show_lights ? "" : "display:none;");
        page.replace("%ACL_HIDE_METEO%", show_meteo ? "" : "display:none;");
        page.replace("%ACL_HIDE_THERMO%", show_thermo ? "" : "display:none;");
        page.replace("%ACL_HIDE_TANKS%", show_tanks ? "" : "display:none;");
        page.replace("%ACL_HIDE_WATERING%", show_watering ? "" : "display:none;");
        page.replace("%ACL_HIDE_SEPTIC%", show_septic ? "" : "display:none;");
        page.replace("%ACL_HIDE_RING%", show_ring ? "" : "display:none;");
        page.replace("%ACL_HIDE_SECURITY%", show_security ? "" : "display:none;");
        page.replace("%ACL_HIDE_AVR%", show_avr ? "" : "display:none;");
        page.replace("%ACL_HIDE_LEAK%", show_leak ? "" : "display:none;");
        if (web._controllers)
        {
            const bool enabled = has_config && web._controllers->sockets().controllerEnabled();
            page.replace("%SOCKETS_ENABLED_CHECKED%", enabled ? "checked" : "");
            page.replace("%SOCKETS_ENABLED_LABEL%", enabled ? "включены" : "выключены");
            const bool lights_enabled = has_config && web._controllers->sockets().lightsEnabled();
            page.replace("%LIGHTS_ENABLED_CHECKED%", lights_enabled ? "checked" : "");
            page.replace("%LIGHTS_ENABLED_LABEL%", lights_enabled ? "включены" : "выключены");
            const bool meteo_enabled = has_config && web._controllers->meteo().controllerEnabled();
            page.replace("%METEO_ENABLED_CHECKED%", meteo_enabled ? "checked" : "");
            page.replace("%METEO_ENABLED_LABEL%", meteo_enabled ? "включено" : "выключено");
            const bool thermo_enabled = has_config && web._controllers->thermo().controllerEnabled();
            page.replace("%THERMO_ENABLED_CHECKED%", thermo_enabled ? "checked" : "");
            page.replace("%THERMO_ENABLED_LABEL%", thermo_enabled ? "включено" : "выключено");
            const bool tanks_enabled = has_config && web._controllers->tanks().controllerEnabled();
            page.replace("%TANKS_ENABLED_CHECKED%", tanks_enabled ? "checked" : "");
            page.replace("%TANKS_ENABLED_LABEL%", tanks_enabled ? "включены" : "выключены");
            const bool septic_enabled = has_config && web._controllers->septic().controllerEnabled();
            page.replace("%SEPTIC_ENABLED_CHECKED%", septic_enabled ? "checked" : "");
            page.replace("%SEPTIC_ENABLED_LABEL%", septic_enabled ? "включены" : "выключены");
            const bool ring_enabled = has_config && web._controllers->ring().controllerEnabled();
            page.replace("%RING_ENABLED_CHECKED%", ring_enabled ? "checked" : "");
            page.replace("%RING_ENABLED_LABEL%", ring_enabled ? "включен" : "выключен");
            const bool security_enabled = has_config && web._controllers->security().controllerEnabled();
            page.replace("%SECURITY_ENABLED_CHECKED%", security_enabled ? "checked" : "");
            const bool watering_enabled = has_config && web._controllers->watering().controllerEnabled();
            page.replace("%WATERING_ENABLED_CHECKED%", watering_enabled ? "checked" : "");
            const bool avr_enabled = has_config && web._controllers->avr().controllerEnabled();
            page.replace("%AVR_ENABLED_CHECKED%", avr_enabled ? "checked" : "");
            page.replace("%AVR_ENABLED_LABEL%", avr_enabled ? "включен" : "выключен");
            const bool leak_enabled = has_config && web._controllers->leak().controllerEnabled();
            page.replace("%LEAK_ENABLED_CHECKED%", leak_enabled ? "checked" : "");
            page.replace("%LEAK_ENABLED_LABEL%", leak_enabled ? "включены" : "выключены");
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
            page.replace("%AVR_ENABLED_CHECKED%", "");
            page.replace("%AVR_ENABLED_LABEL%", "недоступно");
            page.replace("%LEAK_ENABLED_CHECKED%", "");
            page.replace("%LEAK_ENABLED_LABEL%", "недоступно");
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
        page.replace("%AVR_STATUS%", web._avr_status);
        page.replace("%LEAK_STATUS%", web._leak_status);
        page.replace("%CONTROLLERS_SWITCH_DISABLED%", (has_config && can_edit) ? "" : "disabled");
        page.replace("%CONTROLLERS_SWITCH_LOCK%", (has_config && can_edit) ? "0" : "1");
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleControllersSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!hasStartupConfig_())
        {
            web._controllers_status = "Недоступно до сохранения startup-config";
            web.sendRedirect_(request, "/controllers", set_cookie);
            return;
        }
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
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Sockets))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool enabled = request->hasParam("sockets_enabled", true);
            if (web._controllers->sockets().controllerEnabled() != enabled)
            {
                web._controllers->sockets().setControllerEnabled(enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "lights")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Lights))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool lights_enabled = request->hasParam("lights_enabled", true);
            if (web._controllers->sockets().lightsEnabled() != lights_enabled)
            {
                web._controllers->sockets().setLightsEnabled(lights_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "meteo")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Meteo))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool meteo_enabled = request->hasParam("meteo_enabled", true);
            if (web._controllers->meteo().controllerEnabled() != meteo_enabled)
            {
                web._controllers->meteo().setControllerEnabled(meteo_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "thermo")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Thermo))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool thermo_enabled = request->hasParam("thermo_enabled", true);
            if (web._controllers->thermo().controllerEnabled() != thermo_enabled)
            {
                web._controllers->thermo().setControllerEnabled(thermo_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "tanks")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Tanks))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool tanks_enabled = request->hasParam("tanks_enabled", true);
            if (web._controllers->tanks().controllerEnabled() != tanks_enabled)
            {
                web._controllers->tanks().setControllerEnabled(tanks_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "septic")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Septic))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool septic_enabled = request->hasParam("septic_enabled", true);
            if (web._controllers->septic().controllerEnabled() != septic_enabled)
            {
                web._controllers->septic().setControllerEnabled(septic_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "ring")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Ring))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool ring_enabled = request->hasParam("ring_enabled", true);
            if (web._controllers->ring().controllerEnabled() != ring_enabled)
            {
                web._controllers->ring().setControllerEnabled(ring_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "watering")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Watering))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool watering_enabled = request->hasParam("watering_enabled", true);
            if (web._controllers->watering().controllerEnabled() != watering_enabled)
            {
                web._controllers->watering().setControllerEnabled(watering_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "security")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Security))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool security_enabled = request->hasParam("security_enabled", true);
            if (web._controllers->security().controllerEnabled() != security_enabled)
            {
                web._controllers->security().setControllerEnabled(security_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "avr")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Avr))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool avr_enabled = request->hasParam("avr_enabled", true);
            if (web._controllers->avr().controllerEnabled() != avr_enabled)
            {
                web._controllers->avr().setControllerEnabled(avr_enabled);
                changed = true;
            }
        }
        if (ctrl.length() == 0 || ctrl == "leak")
        {
            if (!web.webAclControllerAllowed_(UsersRegistry::AclController::Leak))
            {
                web._controllers_status = "ACL deny";
                web.sendRedirect_(request, "/controllers", set_cookie);
                return;
            }
            const bool leak_enabled = request->hasParam("leak_enabled", true);
            if (web._controllers->leak().controllerEnabled() != leak_enabled)
            {
                web._controllers->leak().setControllerEnabled(leak_enabled);
                changed = true;
            }
        }
        bool ok = true;
        if (changed)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._controllers_status = "Менеджер конфигурации недоступен";
            }
            else if (!web._configs_manager->save())
            {
                ok = false;
                web._controllers_status = "Ошибка сохранения";
            }
        }
        if (ok)
            web._controllers_status = changed ? "Обновлено" : "Без изменений";
        web._sockets_status = web._controllers_status;
        web._lights_status = web._controllers_status;
        web._meteo_status = web._controllers_status;
        web._thermo_status = web._controllers_status;
        web._tanks_status = web._controllers_status;
        web._septic_status = web._controllers_status;
        web._ring_status = web._controllers_status;
        web._avr_status = web._controllers_status;
        web._leak_status = web._controllers_status;
        web._security_status = web._controllers_status;
        web._watering_status = web._controllers_status;
        web.sendRedirect_(request, "/controllers", set_cookie);
    }
};


