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

class UsersHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/users", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleUsers(web, request); });
        server.on("/users", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleUsersSave(web, request); });
    }

    static void handleUsers(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const bool read_only = (web.stackRole_() == ConfigsManagerIface::StackRole::Slave);
        String page = FPSTR(kWebInterfaceUsersHtml);
        page.replace("%NAV%", web.navHtml_());
        if (!web._users_status.length())
            web._users_status = "Готово";
        page.replace("%USERS_STATUS%", web._users_status);
        page.replace("%USERS_CARDS%", usersCards_(web, read_only));
        page.replace("%USERS_FORM_DISABLED%", read_only ? "disabled" : "");
        page.replace("%USERS_READONLY_NOTE%",
                     read_only ? "<p class=\"status\">Редактирование доступно только на master-устройстве</p>" : "");
        page.replace("%USERS_IBUTTON_DATALIST%", ibuttonDatalist_(web));
        page.replace("%USERS_RFID_DATALIST%", rfidDatalist_(web));
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleUsersSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave)
        {
            web._users_status = "Редактирование доступно только на master-устройстве";
            web.sendRedirect_(request, "/users", set_cookie);
            return;
        }
        if (!web._users)
        {
            web._users_status = "Реестр пользователей недоступен";
            web.sendRedirect_(request, "/users", set_cookie);
            return;
        }
        for (size_t i = 0; i < web._users->size(); ++i)
        {
            auto &u = web._users->user(i);
            const String p = String((unsigned)i);
            const String k_enabled = String("u") + p + "_enabled";
            const String k_tg_admin = String("u") + p + "_tg_admin";
            const String k_tg_notify = String("u") + p + "_tg_notify";
            const String k_username = String("u") + p + "_username";
            const String k_tg_username = String("u") + p + "_tg_username";
            const String k_gsm_phone = String("u") + p + "_gsm_phone";
            const String k_gsm_sms = String("u") + p + "_gsm_sms";
            const String k_gsm_call = String("u") + p + "_gsm_call";
            const String k_ibutton = String("u") + p + "_ibutton";
            const String k_rfid = String("u") + p + "_rfid";
            u.enabled = request->hasParam(k_enabled, true);
            u.tg_admin = request->hasParam(k_tg_admin, true);
            u.tg_notify = request->hasParam(k_tg_notify, true);
            u.username = web.paramValue_(request, k_username);
            u.tg_username = UsersRegistry::normalizeTgUsername(web.paramValue_(request, k_tg_username));
            u.gsm_phone = UsersRegistry::normalizePhone(web.paramValue_(request, k_gsm_phone));
            u.gsm_sms = request->hasParam(k_gsm_sms, true);
            u.gsm_call = request->hasParam(k_gsm_call, true);
            u.ibutton_key = UsersRegistry::normalizeHex(web.paramValue_(request, k_ibutton), 16);
            u.rfid_key = UsersRegistry::normalizeHex(web.paramValue_(request, k_rfid), 20);
        }
        if (web._configs_manager && !web._configs_manager->save())
            web._users_status = "Ошибка сохранения";
        else
            web._users_status = "Сохранено";
        web.sendRedirect_(request, "/users", set_cookie);
    }

private:
    static String usersCards_(WebInterface &web, bool read_only)
    {
        if (!web._users)
            return "<div class=\"tile empty\"><strong>Реестр пользователей недоступен</strong></div>";

        String out;
        out.reserve(16384);
        const String disabled = read_only ? " disabled" : "";

        size_t render_count = web._users->size();
        if (render_count > 0)
        {
            int last_enabled = -1;
            for (size_t i = 0; i < web._users->size(); ++i)
            {
                if (web._users->user(i).enabled)
                    last_enabled = (int)i;
            }
            if (last_enabled < 0)
            {
                render_count = 1;
            }
            else
            {
                render_count = (size_t)last_enabled + 2;
                if (render_count > web._users->size())
                    render_count = web._users->size();
            }
        }

        for (size_t i = 0; i < render_count; ++i)
        {
            const auto &u = web._users->user(i);
            const String p = String((unsigned)i);

            out += "<div class=\"tile";
            if (!u.enabled)
                out += " disabled";
            out += "\">";

            out += "<div class=\"tile-head\">";
            out += "<div class=\"head-left\"><span class=\"user-icon\" aria-hidden=\"true\">";
            out += "<svg viewBox=\"0 0 24 24\" focusable=\"false\"><path fill=\"currentColor\" d=\"M12 12a5 5 0 1 0-5-5 5 5 0 0 0 5 5zm0 2c-4.42 0-8 2.24-8 5v1h16v-1c0-2.76-3.58-5-8-5z\"/></svg>";
            out += "</span><strong>ID ";
            out += String((unsigned)(i + 1));
            out += "</strong></div>";

            out += "<label class=\"switch\"><input type=\"checkbox\" name=\"u";
            out += p;
            out += "_enabled\"";
            if (u.enabled)
                out += " checked";
            out += disabled;
            out += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            out += "</div>";

            out += "<div class=\"grid\">";

            out += "<div class=\"form-row full\"><label>Имя пользователя</label><input class=\"field\" name=\"u";
            out += p;
            out += "_username\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.username.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row full\"><label>Telegram логин</label><input class=\"field\" name=\"u";
            out += p;
            out += "_tg_username\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.tg_username.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row\"><label>Админ Telegram</label><label class=\"switch\"><input type=\"checkbox\" name=\"u";
            out += p;
            out += "_tg_admin\"";
            if (u.tg_admin)
                out += " checked";
            out += disabled;
            out += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";

            out += "<div class=\"form-row\"><label>Уведомления Telegram</label><label class=\"switch\"><input type=\"checkbox\" name=\"u";
            out += p;
            out += "_tg_notify\"";
            if (u.tg_notify)
                out += " checked";
            out += disabled;
            out += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";


            out += "<div class=\"form-row full\"><label>Ключ iButton</label><input class=\"field\" list=\"users-ibutton-last\" name=\"u";
            out += p;
            out += "_ibutton\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.ibutton_key.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row full\"><label>Ключ RFID</label><input class=\"field\" list=\"users-rfid-last\" name=\"u";
            out += p;
            out += "_rfid\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.rfid_key.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";
            out += "<div class=\"form-row full\"><label>Телефон (GSM)</label><input class=\"field\" name=\"u";
            out += p;
            out += "_gsm_phone\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.gsm_phone.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row\"><label>SMS</label><label class=\"switch\"><input type=\"checkbox\" name=\"u";
            out += p;
            out += "_gsm_sms\"";
            if (u.gsm_sms)
                out += " checked";
            out += disabled;
            out += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";

            out += "<div class=\"form-row\"><label>Звонок</label><label class=\"switch\"><input type=\"checkbox\" name=\"u";
            out += p;
            out += "_gsm_call\"";
            if (u.gsm_call)
                out += " checked";
            out += disabled;
            out += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";

            out += "</div></div>";
        }

        return out;
    }

    static String ibuttonDatalist_(WebInterface &web)
    {
        if (!web._controllers)
            return "";
        char last_hex[17] = {};
        if (!web._controllers->security().lastKeyHex(last_hex))
            return "";
        String out;
        out.reserve(80);
        out += "<datalist id=\"users-ibutton-last\"><option value=\"";
        out += last_hex;
        out += "\"></option></datalist>";
        return out;
    }

    static String rfidDatalist_(WebInterface &web)
    {
        if (!web._controllers)
            return "";
        String last;
        if (!web._controllers->security().lastRfidSerial(last) || last.length() == 0)
            return "";
        String out;
        out.reserve(96);
        out += "<datalist id=\"users-rfid-last\"><option value=\"";
        out += last;
        out += "\"></option></datalist>";
        return out;
    }
};

