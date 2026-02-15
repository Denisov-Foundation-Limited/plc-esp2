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
        // Functional handlers first, page handlers after them.
        server.on("/users/acl", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleUsersAclSave(web, request); });
        server.on("/users", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleUsersSave(web, request); });
        server.on("/users/acl", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleUsersAcl(web, request); });
        server.on("/users", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleUsers(web, request); });
    }

    static void handleUsers(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
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
        if (!web.requireWebAdmin_(request, &set_cookie))
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
            const String k_is_admin = String("u") + p + "_is_admin";
            const String k_tg_notify = String("u") + p + "_tg_notify";
            const String k_username = String("u") + p + "_username";
            const String k_tg_username = String("u") + p + "_tg_username";
            const String k_web_password = String("u") + p + "_web_password";
            const String k_gsm_phone = String("u") + p + "_gsm_phone";
            const String k_gsm_sms = String("u") + p + "_gsm_sms";
            const String k_gsm_call = String("u") + p + "_gsm_call";
            const String k_ibutton = String("u") + p + "_ibutton";
            const String k_rfid = String("u") + p + "_rfid";
            u.enabled = request->hasParam(k_enabled, true);
            u.tg_admin = request->hasParam(k_is_admin, true);
            u.tg_notify = request->hasParam(k_tg_notify, true);
            u.username = web.paramValue_(request, k_username);
            String web_pass = web.paramValue_(request, k_web_password);
            web_pass.trim();
            if (web_pass.length() > 0)
                u.setWebPassword(web_pass);
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

    static void handleUsersAcl(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web._users)
        {
            web._users_status = "Реестр пользователей недоступен";
            web.sendRedirect_(request, "/users", set_cookie);
            return;
        }

        size_t user_idx = 0;
        uint8_t unit = 0;
        parseAclRouteParams_(web, request, user_idx, unit);
        const bool read_only = (web.stackRole_() == ConfigsManagerIface::StackRole::Slave);

        String page;
        page.reserve(12288);
        page += "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
        page += "<title>Users ACL</title><style>";
        page += "body{margin:0;font-family:Segoe UI,Tahoma,Arial,sans-serif;background:radial-gradient(1200px 600px at 10% -10%,#1f2937 0%,#0b1220 60%,#080d17 100%);color:#e5e7eb}";
        page += ".wrap{max-width:1280px;margin:24px auto;padding:0 12px}.card{background:linear-gradient(180deg,#0f172a 0%,#0b1220 100%);border:1px solid #1f2937;border-radius:12px;padding:16px}";
        page += "a{color:#7dd3fc;text-decoration:none}.status{color:#94a3b8;margin:8px 0;font-size:12px}.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}";
        page += ".select{background:#0b1220;color:#e5e7eb;border:1px solid #1f2937;border-radius:8px;padding:8px;min-height:36px}";
        page += ".tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(360px,1fr));gap:12px;margin-top:12px}";
        page += ".tile{background:#0b1220;border:1px solid #1f2937;border-radius:12px;padding:12px}.tile-head{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:8px}";
        page += ".items{max-height:360px;overflow:auto;border:1px solid #1f2937;border-radius:8px}.item{display:grid;grid-template-columns:1fr auto auto;gap:8px;padding:6px 8px;border-bottom:1px solid #1f2937}";
        page += ".item:last-child{border-bottom:none}.muted{color:#94a3b8;font-size:12px}.btn{border:none;border-radius:8px;padding:10px 16px;font-weight:700;background:#38bdf8;color:#0b1220;cursor:pointer}";
        page += ".btn-lite{border:1px solid #334155;background:#111827;color:#e5e7eb;border-radius:8px;padding:6px 10px;cursor:pointer}.row-actions{display:flex;gap:8px;align-items:center;flex-wrap:wrap}";
        page += ".tile-actions{display:inline-flex;gap:6px;align-items:center}";
        page += ".switch{display:inline-flex;align-items:center}.switch input{display:none}.track{width:44px;height:24px;background:#334155;border-radius:999px;position:relative;transition:background .15s ease;display:inline-block;vertical-align:middle}";
        page += ".knob{width:18px;height:18px;border-radius:50%;background:#e5e7eb;position:absolute;top:3px;left:3px;transition:transform .15s ease}.switch input:checked+.track{background:#0ea5e9}.switch input:checked+.track .knob{transform:translateX(20px)}";
        page += "@media (max-width:720px){.items{max-height:none}}";
        page += "</style></head><body><div class=\"wrap\"><div class=\"card\">";
        page += web.navHtml_();
        page += "<div class=\"row\"><a href=\"/users\">&larr; Пользователи</a><strong>ACL: user #";
        page += String((unsigned)(user_idx + 1));
        page += "</strong></div>";
        if (!web._users_status.length())
            web._users_status = "Готово";
        page += "<p class=\"status\">";
        WebInterface::appendHtmlEscaped_(page, web._users_status.c_str());
        page += "</p>";
        if (read_only)
            page += "<p class=\"status\">Редактирование ACL доступно только на master-устройстве</p>";
        page += "<form method=\"GET\" action=\"/users/acl\" class=\"row\" style=\"margin-top:8px\"><input type=\"hidden\" name=\"uid\" value=\"";
        page += String((unsigned)(user_idx + 1));
        page += "\"><label class=\"muted\">Юнит</label><select class=\"select\" name=\"unit\" onchange=\"this.form.submit()\">";
        for (uint8_t i = 0; i < UsersRegistry::kAclUnitCount; ++i)
        {
            page += "<option value=\"";
            page += String((unsigned)(i + 1));
            page += "\"";
            if (unit == i)
                page += " selected";
            page += ">";
            if (i == 0)
            {
                page += "Локальный";
            }
            else
            {
                String label;
                if (web._stack_master)
                {
                    const size_t node_idx = (size_t)(i - 1);
                    if (node_idx < web._stack_master->nodeCount())
                    {
                        const uint32_t node_id = web._stack_master->nodeIdAt(node_idx);
                        const String node_name = web._stack_master->nodeNameAt(node_idx);
                        label = "Unit: ";
                        if (node_name.length())
                            label += node_name;
                        else
                            label += String((unsigned long)node_id);
                    }
                }
                if (!label.length())
                {
                    label = "Unit: ";
                    label += String((unsigned)i);
                }
                WebInterface::appendHtmlEscaped_(page, label.c_str());
            }
            page += "</option>";
        }
        page += "</select></form>";
        page += "<form method=\"POST\" action=\"/users/acl\">";
        page += "<input type=\"hidden\" name=\"uid\" value=\"";
        page += String((unsigned)(user_idx + 1));
        page += "\"><input type=\"hidden\" name=\"unit\" value=\"";
        page += String((unsigned)(unit + 1));
        page += "\">";
        page += "<div class=\"row-actions\"><button class=\"btn-lite\" type=\"button\" onclick=\"aclToggleAll(true)\">Выбрать все</button><button class=\"btn-lite\" type=\"button\" onclick=\"aclToggleAll(false)\">Снять все</button></div>";
        page += "<div class=\"tiles\">";
        page += aclTiles_(web, web._users->user(user_idx), unit, read_only);
        page += "</div><button class=\"btn\" type=\"submit\"";
        if (read_only)
            page += " disabled";
        page += ">Сохранить ACL</button></form>";
        page += "<script>function aclSetScope(root,val){if(!root)return;root.querySelectorAll('input[type=checkbox]').forEach(function(cb){if(!cb.disabled)cb.checked=val;});}function aclToggleAll(val){aclSetScope(document,val);}function aclToggleTile(btn,val){var t=btn.closest('.tile');aclSetScope(t,val);}</script>";
        if (page.indexOf("Загрузка данных юнита...") >= 0)
            page += "<script>setTimeout(function(){window.location.reload();},1200);</script>";
        page += "</div></div></body></html>";
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleUsersAclSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave)
        {
            web._users_status = "Редактирование ACL доступно только на master-устройстве";
            web.sendRedirect_(request, "/users", set_cookie);
            return;
        }
        if (!web._users)
        {
            web._users_status = "Реестр пользователей недоступен";
            web.sendRedirect_(request, "/users", set_cookie);
            return;
        }

        size_t user_idx = 0;
        uint8_t unit = 0;
        parseAclRouteParams_(web, request, user_idx, unit);
        auto &u = web._users->user(user_idx);

        forEachAclController_([&](UsersRegistry::AclController ctrl) {
            const size_t ci = (size_t)ctrl;
            const String controller_key = String("c_") + String((unsigned)ci);
            const bool allow_controller = request->hasParam(controller_key, true);
            u.setControllerAllowed(unit, ctrl, allow_controller);

            forEachAclItem_(web, unit, ctrl, [&](uint16_t item_id, const String &) {
                const String rv_key = String("rv_") + String((unsigned)ci) + "_" + String((unsigned)item_id);
                const String rw_key = String("rw_") + String((unsigned)ci) + "_" + String((unsigned)item_id);
                u.setItemView(unit, ctrl, item_id, request->hasParam(rv_key, true));
                u.setItemControl(unit, ctrl, item_id, request->hasParam(rw_key, true));
            });
        });

        if (web._configs_manager && !web._configs_manager->save())
            web._users_status = "Ошибка сохранения ACL";
        else
            web._users_status = "ACL сохранен";

        String to = "/users/acl?uid=";
        to += String((unsigned)(user_idx + 1));
        to += "&unit=";
        to += String((unsigned)(unit + 1));
        web.sendRedirect_(request, to, set_cookie);
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
            out += "<a href=\"/users/acl?uid=";
            out += String((unsigned)(i + 1));
            out += "&unit=1\">ACL</a>";
            out += "</div>";

            out += "<div class=\"grid\">";

            out += "<div class=\"form-row full\"><label>Имя пользователя</label><input class=\"field\" name=\"u";
            out += p;
            out += "_username\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.username.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row full\"><label>Пароль</label><input class=\"field\" type=\"password\" name=\"u";
            out += p;
            out += "_web_password\" value=\"\" placeholder=\"оставьте пустым, чтобы не менять\" autocomplete=\"new-password\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row full\"><label>Telegram логин</label><input class=\"field\" name=\"u";
            out += p;
            out += "_tg_username\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.tg_username.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row\"><label>Админ</label><label class=\"switch\"><input type=\"checkbox\" name=\"u";
            out += p;
            out += "_is_admin\"";
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

    template <typename FnT>
    static void forEachAclController_(FnT fn)
    {
        fn(UsersRegistry::AclController::Sockets);
        fn(UsersRegistry::AclController::Lights);
        fn(UsersRegistry::AclController::Meteo);
        fn(UsersRegistry::AclController::Thermo);
        fn(UsersRegistry::AclController::Tanks);
        fn(UsersRegistry::AclController::Septic);
        fn(UsersRegistry::AclController::Security);
        fn(UsersRegistry::AclController::Watering);
        fn(UsersRegistry::AclController::Leak);
        fn(UsersRegistry::AclController::Avr);
        fn(UsersRegistry::AclController::Ring);
    }

    static const char *aclControllerTitle_(UsersRegistry::AclController ctrl)
    {
        switch (ctrl)
        {
        case UsersRegistry::AclController::Sockets:
            return "Розетки";
        case UsersRegistry::AclController::Lights:
            return "Свет";
        case UsersRegistry::AclController::Meteo:
            return "Метео";
        case UsersRegistry::AclController::Thermo:
            return "Термо";
        case UsersRegistry::AclController::Tanks:
            return "Баки";
        case UsersRegistry::AclController::Septic:
            return "Септик";
        case UsersRegistry::AclController::Security:
            return "Охрана";
        case UsersRegistry::AclController::Watering:
            return "Полив";
        case UsersRegistry::AclController::Leak:
            return "Протечки";
        case UsersRegistry::AclController::Avr:
            return "АВР";
        case UsersRegistry::AclController::Ring:
            return "Звонок";
        default:
            return "Controller";
        }
    }

    static bool parseAclRouteParams_(WebInterface &web, AsyncWebServerRequest *request, size_t &user_idx, uint8_t &unit)
    {
        user_idx = 0;
        unit = 0;
        if (!web._users || web._users->size() == 0)
            return false;

        String uid = web.paramValue_(request, "uid");
        if (!uid.length())
            uid = web.paramValueAny_(request, "uid");
        int uid_val = uid.toInt();
        if (uid_val < 1 || uid_val > (int)web._users->size())
            uid_val = 1;
        user_idx = (size_t)(uid_val - 1);

        String unit_raw = web.paramValue_(request, "unit");
        if (!unit_raw.length())
            unit_raw = web.paramValueAny_(request, "unit");
        int unit_val = unit_raw.toInt();
        if (unit_val < 1 || unit_val > (int)UsersRegistry::kAclUnitCount)
            unit_val = 1;
        unit = (uint8_t)(unit_val - 1);
        return true;
    }

    static bool aclUnitNodeId_(WebInterface &web, uint8_t unit, uint32_t &node_id)
    {
        node_id = 0;
        if (unit == 0)
            return false;
        if (!web._stack_master)
            return false;
        const size_t idx = (size_t)(unit - 1);
        if (idx >= web._stack_master->nodeCount())
            return false;
        node_id = web._stack_master->nodeIdAt(idx);
        return node_id != 0;
    }

    static String aclItemLabel_(const String &prefix, uint16_t id, const String &name)
    {
        String out = "#";
        out += String((unsigned)id);
        out += " - ";
        if (name.length())
            out += name;
        else
            out += prefix;
        return out;
    }

    template <typename FnT>
    static void forEachAclItem_(WebInterface &web, uint8_t unit, UsersRegistry::AclController ctrl, FnT fn, bool *loading = nullptr)
    {
        if (loading)
            *loading = false;
        const bool local_unit = (unit == 0);
        if (local_unit && web._controllers)
        {
            Controllers &c = *web._controllers;
            switch (ctrl)
            {
            case UsersRegistry::AclController::Sockets:
                for (size_t i = 0; i < SocketController::kSocketCount; ++i)
                {
                    const auto *cfg = c.sockets().configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    const String label = aclItemLabel_("Розетка", cfg->id, cfg->name);
                    fn(cfg->id, label);
                }
                return;
            case UsersRegistry::AclController::Lights:
                for (size_t i = 0; i < SocketController::kLightCount; ++i)
                {
                    const auto *cfg = c.sockets().lightConfigByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    const String label = aclItemLabel_("Свет", cfg->id, cfg->name);
                    fn(cfg->id, label);
                }
                return;
            case UsersRegistry::AclController::Meteo:
                for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
                {
                    const auto *cfg = c.meteo().configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    const String label = aclItemLabel_("Метео", cfg->id, cfg->name);
                    fn(cfg->id, label);
                }
                return;
            case UsersRegistry::AclController::Thermo:
                for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
                {
                    const auto *cfg = c.thermo().configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    const String label = aclItemLabel_("Термо", cfg->id, cfg->name);
                    fn(cfg->id, label);
                }
                return;
            case UsersRegistry::AclController::Tanks:
                for (size_t i = 0; i < TankController::kTankCount; ++i)
                {
                    const auto *cfg = c.tanks().configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    const String label = aclItemLabel_("Бак", cfg->id, cfg->name);
                    fn(cfg->id, label);
                }
                return;
            case UsersRegistry::AclController::Septic:
                for (size_t i = 0; i < SepticController::kSepticCount; ++i)
                {
                    const auto *cfg = c.septic().configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    const String label = aclItemLabel_("Септик", cfg->id, cfg->name);
                    fn(cfg->id, label);
                }
                return;
            case UsersRegistry::AclController::Security:
                for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
                {
                    const auto *cfg = c.security().configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    const String label = aclItemLabel_("Сенсор", cfg->id, cfg->name);
                    fn(cfg->id, label);
                }
                return;
            case UsersRegistry::AclController::Watering:
                for (size_t i = 0; i < WateringController::kRuleCount; ++i)
                {
                    const auto *cfg = c.watering().configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    const String label = aclItemLabel_("Правило", cfg->id, cfg->name);
                    fn(cfg->id, label);
                }
                return;
            case UsersRegistry::AclController::Leak:
                for (size_t i = 0; i < LeakController::kZoneCount; ++i)
                {
                    const auto *cfg = c.leak().configByIndex(i);
                    if (!cfg || !cfg->enabled)
                        continue;
                    const String label = aclItemLabel_("Протечка", cfg->id, cfg->name);
                    fn(cfg->id, label);
                }
                return;
            case UsersRegistry::AclController::Avr:
                if (c.avr().controllerEnabled())
                    fn(1, String("AVR"));
                return;
            case UsersRegistry::AclController::Ring:
                if (c.ring().controllerEnabled())
                    fn(1, String("Ring"));
                return;
            default:
                break;
            }
        }

        uint32_t node_id = 0;
        if (aclUnitNodeId_(web, unit, node_id) && web._stack_cache)
        {
            switch (ctrl)
            {
            case UsersRegistry::AclController::Sockets:
            {
                const auto *cache = web._stack_cache->socketsCache(node_id);
                if (cache && cache->has_data)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        fn(it.id, aclItemLabel_("Розетка", it.id, it.name[0] ? String(it.name) : String()));
                    }
                }
                else
                {
                    web._stack_cache->requestSockets(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Lights:
            {
                const auto *cache = web._stack_cache->lightsCache(node_id);
                if (cache && cache->has_data)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        fn(it.id, aclItemLabel_("Свет", it.id, it.name[0] ? String(it.name) : String()));
                    }
                }
                else
                {
                    web._stack_cache->requestLights(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Meteo:
            {
                const auto *cache = web._stack_cache->meteoCache(node_id);
                if (cache && cache->has_data)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        fn(it.id, aclItemLabel_("Метео", it.id, it.name[0] ? String(it.name) : String()));
                    }
                }
                else
                {
                    web._stack_cache->requestMeteo(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Thermo:
            {
                const auto *cache = web._stack_cache->thermoCache(node_id);
                if (cache && cache->has_data)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        fn(it.id, aclItemLabel_("Термо", it.id, it.name[0] ? String(it.name) : String()));
                    }
                }
                else
                {
                    web._stack_cache->requestThermo(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Tanks:
            {
                const auto *cache = web._stack_cache->tanksCache(node_id);
                if (cache && cache->has_data)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        fn(it.id, aclItemLabel_("Бак", it.id, it.name[0] ? String(it.name) : String()));
                    }
                }
                else
                {
                    web._stack_cache->requestTanks(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Septic:
            {
                const auto *cache = web._stack_cache->septicCache(node_id);
                if (cache && cache->has_data)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        fn(it.id, aclItemLabel_("Септик", it.id, String()));
                    }
                }
                else
                {
                    web._stack_cache->requestSeptic(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Security:
            {
                const auto *cache = web._stack_cache->securityCache(node_id);
                if (cache && cache->has_data)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        fn(it.id, aclItemLabel_("Сенсор", it.id, it.name[0] ? String(it.name) : String()));
                    }
                }
                else
                {
                    web._stack_cache->requestSecurity(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Watering:
            {
                const auto *cache = web._stack_cache->wateringCache(node_id);
                if (cache && cache->has_data)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        fn(it.id, aclItemLabel_("Правило", it.id, it.name[0] ? String(it.name) : String()));
                    }
                }
                else
                {
                    web._stack_cache->requestWatering(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Leak:
            {
                const auto *cache = web._stack_cache->leakCache(node_id);
                if (cache && cache->has_data)
                {
                    for (size_t i = 0; i < cache->item_count; ++i)
                    {
                        const auto &it = cache->items[i];
                        if (!it.enabled)
                            continue;
                        fn(it.id, aclItemLabel_("Протечка", it.id, it.name[0] ? String(it.name) : String()));
                    }
                }
                else
                {
                    web._stack_cache->requestLeak(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Avr:
            {
                const auto *cache = web._stack_cache->avrCache(node_id);
                if (cache && cache->has_data)
                {
                    if (cache->enabled)
                        fn(1, String("AVR"));
                }
                else
                {
                    web._stack_cache->requestAvr(node_id);
                    if (loading)
                        *loading = true;
                }
                return;
            }
            case UsersRegistry::AclController::Ring:
                fn(1, String("Ring"));
                return;
            default:
                break;
            }
        }

        return;
    }

    static String aclTiles_(WebInterface &web, const UsersRegistry::User &u, uint8_t unit, bool read_only)
    {
        String out;
        out.reserve(32768);
        const String dis = read_only ? " disabled" : "";
        forEachAclController_([&](UsersRegistry::AclController ctrl) {
            const size_t ci = (size_t)ctrl;
            const bool allow_ctrl = u.controllerAllowed(unit, ctrl);
            out += "<div class=\"tile\"><div class=\"tile-head\"><strong>";
            out += aclControllerTitle_(ctrl);
            out += "</strong><div class=\"tile-actions\"><button class=\"btn-lite\" type=\"button\" onclick=\"aclToggleTile(this,true)\">Все</button><button class=\"btn-lite\" type=\"button\" onclick=\"aclToggleTile(this,false)\">Ничего</button><label class=\"switch\"><input type=\"checkbox\" name=\"c_";
            out += String((unsigned)ci);
            out += "\"";
            if (allow_ctrl)
                out += " checked";
            out += dis;
            out += "><span class=\"track\"><span class=\"knob\"></span></span></label></div></div>";
            out += "<div class=\"items\">";
            bool any = false;
            bool loading = false;
            forEachAclItem_(web, unit, ctrl, [&](uint16_t item_id, const String &label) {
                any = true;
                out += "<div class=\"item\"><div>";
                WebInterface::appendHtmlEscaped_(out, label.c_str());
                out += "</div><label class=\"muted\"><input type=\"checkbox\" name=\"rv_";
                out += String((unsigned)ci);
                out += "_";
                out += String((unsigned)item_id);
                out += "\"";
                if (u.itemViewAllowedRaw(unit, ctrl, item_id))
                    out += " checked";
                out += dis;
                out += "> read</label><label class=\"muted\"><input type=\"checkbox\" name=\"rw_";
                out += String((unsigned)ci);
                out += "_";
                out += String((unsigned)item_id);
                out += "\"";
                if (u.itemControlAllowedRaw(unit, ctrl, item_id))
                    out += " checked";
                out += dis;
                out += "> write</label></div>";
            }, &loading);
            if (!any)
            {
                if (loading)
                    out += "<div class=\"item\"><div class=\"muted\">Загрузка данных юнита...</div><div></div><div></div></div>";
                else
                    out += "<div class=\"item\"><div class=\"muted\">Нет enabled элементов</div><div></div><div></div></div>";
            }
            out += "</div></div>";
        });
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

