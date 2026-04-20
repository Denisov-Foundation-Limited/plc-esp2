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

#include "core/network/web/handlers/users_handler.hpp"

#include "core/network/web/web_interface.hpp"

void UsersHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        // Functional handlers first, page handlers after them.
        server.on("/users/acl", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleUsersAclSave(web, request); });
        server.on("/users", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleUsersSave(web, request); });
        server.on("/users/acl", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleUsersAcl(web, request); });
        server.on("/users", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleUsers(web, request); });
    }

void UsersHandler::handleUsers(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        const bool read_only = (web.stackRole_() == ConfigsManagerIface::StackRole::Slave);
        String page = FPSTR(kWebInterfaceUsersHtml);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%USERS_PAGE_TITLE%", WebUiRu::Users::kUsersLink);
        page.replace("%USERS_PAGE_H1%", WebUiRu::Users::kUsersLink);
        if (!web._users_status.length())
            web._users_status = WebUiRu::Users::kReady;
        page.replace("%USERS_STATUS%", web._users_status);
        page.replace("%USERS_CARDS%", usersCards_(web, read_only));
        page.replace("%USERS_FORM_DISABLED%", read_only ? "disabled" : "");
        page.replace("%SAVE_TEXT%", WebUiRu::kSave);
        page.replace("%USERS_READONLY_NOTE%",
                     read_only ? (String("<p class=\"status\">") + WebUiRu::Users::kMasterOnlyEdit + "</p>") : "");
        page.replace("%USERS_IBUTTON_DATALIST%", ibuttonDatalist_(web));
        page.replace("%USERS_RFID_DATALIST%", rfidDatalist_(web));
        web.sendHtml_(request, page, set_cookie);
    }

void UsersHandler::handleUsersSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave)
        {
            web._users_status = WebUiRu::Users::kMasterOnlyEdit;
            web.sendRedirect_(request, "/users", set_cookie);
            return;
        }
        if (!web._users)
        {
            web._users_status = WebUiRu::Users::kRegistryUnavailable;
            web.sendRedirect_(request, "/users", set_cookie);
            return;
        }
        for (size_t i = 0; i < web._users->size(); ++i)
        {
            auto &u = web._users->user(i);
            const String p = String((unsigned)i);
            const String k_enabled = String("u") + p + "_enabled";
            const String k_username = String("u") + p + "_username";
            const String k_web_password = String("u") + p + "_web_password";
            const String k_gsm_phone = String("u") + p + "_gsm_phone";
            const String k_gsm_sms = String("u") + p + "_gsm_sms";
            const String k_gsm_call = String("u") + p + "_gsm_call";
            const String k_ibutton = String("u") + p + "_ibutton";
            const String k_rfid = String("u") + p + "_rfid";
            u.enabled = request->hasParam(k_enabled, true);
            u.username = web.paramValue_(request, k_username);
            String web_pass = web.paramValue_(request, k_web_password);
            web_pass.trim();
            const bool masked_unchanged = u.hasWebPassword() && web_pass == "********";
            if (web_pass.length() > 0 && !masked_unchanged)
                u.setWebPassword(web_pass);
            u.gsm_phone = UsersRegistry::normalizePhone(web.paramValue_(request, k_gsm_phone));
            u.gsm_sms = request->hasParam(k_gsm_sms, true);
            u.gsm_call = request->hasParam(k_gsm_call, true);
            u.ibutton_key = UsersRegistry::normalizeHex(web.paramValue_(request, k_ibutton), 16);
            u.rfid_key = UsersRegistry::normalizeHex(web.paramValue_(request, k_rfid), 20);
        }
        if (web._configs_manager && !web._configs_manager->save())
            web._users_status = WebUiRu::Common::kSaveFailed;
        else
            web._users_status = WebUiRu::Common::kSaved;
        web.sendRedirect_(request, "/users", set_cookie);
    }

void UsersHandler::handleUsersAcl(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web._users)
        {
            web._users_status = WebUiRu::Users::kRegistryUnavailable;
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
        page += "<div class=\"row\"><a href=\"/users\">&larr; ";
        page += WebUiRu::Users::kUsersLink;
        page += "</a><strong>ACL: user #";
        page += String((unsigned)(user_idx + 1));
        page += "</strong></div>";
        if (!web._users_status.length())
            web._users_status = WebUiRu::Users::kReady;
        page += "<p class=\"status\">";
        WebInterface::appendHtmlEscaped_(page, web._users_status.c_str());
        page += "</p>";
        if (read_only)
            page += "<p class=\"status\">";
            page += WebUiRu::Users::kMasterOnlyAclEdit;
            page += "</p>";
        page += "<form method=\"GET\" action=\"/users/acl\" class=\"row\" style=\"margin-top:8px\"><input type=\"hidden\" name=\"uid\" value=\"";
        page += String((unsigned)(user_idx + 1));
        page += "\"><label class=\"muted\">";
        page += WebUiRu::Users::kUnit;
        page += "</label><select class=\"select\" name=\"unit\" onchange=\"this.form.submit()\">";
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
                page += WebUiRu::Users::kUnitLocal;
            }
            else
            {
                String label;
                if (web.network())
                {
                    const size_t node_idx = (size_t)(i - 1);
                    StackDeviceRegistry::DeviceInfo device{};
                    if (web.network()->stackDeviceSnapshotAt(node_idx, device) && device.online && device.node_id != 0)
                    {
                        label = "Unit: ";
                        if (device.name[0])
                            label += String(device.name);
                        else
                            label += String((unsigned long)device.node_id);
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
        page += "<div class=\"row-actions\"><button class=\"btn-lite\" type=\"button\" onclick=\"aclToggleAll(true)\">";
        page += WebUiRu::Users::kSelectAll;
        page += "</button><button class=\"btn-lite\" type=\"button\" onclick=\"aclToggleAll(false)\">";
        page += WebUiRu::Users::kClearAll;
        page += "</button></div>";
        page += "<div class=\"tiles\">";
        page += aclTiles_(web, web._users->user(user_idx), unit, read_only);
        page += "</div><button class=\"btn\" type=\"submit\"";
        if (read_only)
            page += " disabled";
        page += ">";
        page += WebUiRu::kSaveAcl;
        page += "</button></form>";
        page += "<script>function aclSetScope(root,val){if(!root)return;root.querySelectorAll('input[type=checkbox]').forEach(function(cb){if(!cb.disabled)cb.checked=val;});}function aclToggleAll(val){aclSetScope(document,val);}function aclToggleTile(btn,val){var t=btn.closest('.tile');aclSetScope(t,val);}</script>";
        if (page.indexOf(WebUiRu::Users::kAclLoadingUnit) >= 0)
            page += "<script>setTimeout(function(){window.location.reload();},1200);</script>";
        page += "</div></div></body></html>";
        web.sendHtml_(request, page, set_cookie);
    }

void UsersHandler::handleUsersAclSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave)
        {
            web._users_status = WebUiRu::Users::kMasterOnlyAclEdit;
            web.sendRedirect_(request, "/users", set_cookie);
            return;
        }
        if (!web._users)
        {
            web._users_status = WebUiRu::Users::kRegistryUnavailable;
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
            web._users_status = WebUiRu::Users::kAclSaveFailed;
        else
            web._users_status = WebUiRu::Users::kAclSaved;

        String to = "/users/acl?uid=";
        to += String((unsigned)(user_idx + 1));
        to += "&unit=";
        to += String((unsigned)(unit + 1));
        web.sendRedirect_(request, to, set_cookie);
    }

String UsersHandler::usersCards_(WebInterface &web, bool read_only) {
        if (!web._users)
            return String("<div class=\"tile empty\"><strong>") + WebUiRu::Users::kRegistryUnavailable + "</strong></div>";

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

            out += "<div class=\"form-row full\"><label>";
            out += WebUiRu::Users::kLabelUsername;
            out += "</label><input class=\"field\" name=\"u";
            out += p;
            out += "_username\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.username.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row full\"><label>";
            out += WebUiRu::Users::kLabelPassword;
            out += "</label><input class=\"field\" type=\"password\" name=\"u";
            out += p;
            out += "_web_password\" value=\"";
            if (u.hasWebPassword())
                out += "********";
            out += "\" placeholder=\"web_password\"";
            out += " title=\"";
            out += WebUiRu::Users::kPasswordPlaceholder;
            out += "\" autocomplete=\"new-password\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row full\"><label>";
            out += WebUiRu::Users::kLabelIButtonKey;
            out += "</label><input class=\"field\" list=\"users-ibutton-last\" name=\"u";
            out += p;
            out += "_ibutton\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.ibutton_key.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";

            out += "<div class=\"form-row full\"><label>";
            out += WebUiRu::Users::kLabelRfidKey;
            out += "</label><input class=\"field\" list=\"users-rfid-last\" name=\"u";
            out += p;
            out += "_rfid\" value=\"";
            WebInterface::appendHtmlEscaped_(out, u.rfid_key.c_str());
            out += "\"";
            out += disabled;
            out += "></div>";
            out += "<div class=\"form-row full\"><label>";
            out += WebUiRu::Users::kLabelPhoneGsm;
            out += "</label><input class=\"field\" name=\"u";
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

            out += "<div class=\"form-row\"><label>";
            out += WebUiRu::Users::kLabelCall;
            out += "</label><label class=\"switch\"><input type=\"checkbox\" name=\"u";
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
void UsersHandler::forEachAclController_(FnT fn)
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

const char * UsersHandler::aclControllerTitle_(UsersRegistry::AclController ctrl) {
        switch (ctrl)
        {
        case UsersRegistry::AclController::Sockets:
            return WebUiRu::Users::kCtrlSockets;
        case UsersRegistry::AclController::Lights:
            return WebUiRu::Users::kCtrlLights;
        case UsersRegistry::AclController::Meteo:
            return WebUiRu::Users::kCtrlMeteo;
        case UsersRegistry::AclController::Thermo:
            return WebUiRu::Users::kCtrlThermo;
        case UsersRegistry::AclController::Tanks:
            return WebUiRu::Users::kCtrlTanks;
        case UsersRegistry::AclController::Septic:
            return WebUiRu::Users::kCtrlSeptic;
        case UsersRegistry::AclController::Security:
            return WebUiRu::Users::kCtrlSecurity;
        case UsersRegistry::AclController::Watering:
            return WebUiRu::Users::kCtrlWatering;
        case UsersRegistry::AclController::Leak:
            return WebUiRu::Users::kCtrlLeak;
        case UsersRegistry::AclController::Avr:
            return WebUiRu::Users::kCtrlAvr;
        case UsersRegistry::AclController::Ring:
            return WebUiRu::Users::kCtrlRing;
        default:
            return "Controller";
        }
    }

bool UsersHandler::parseAclRouteParams_(WebInterface &web, AsyncWebServerRequest *request, size_t &user_idx, uint8_t &unit) {
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

bool UsersHandler::aclUnitNodeId_(WebInterface &web, uint8_t unit, uint32_t &node_id) {
        node_id = 0;
        if (unit == 0)
            return false;
        if (!web.network())
            return false;
        const size_t idx = (size_t)(unit - 1);
        StackDeviceRegistry::DeviceInfo device{};
        if (!web.network()->stackDeviceSnapshotAt(idx, device) || !device.online || device.node_id == 0)
            return false;
        node_id = device.node_id;
        return node_id != 0;
    }

String UsersHandler::aclItemLabel_(const String &prefix, uint16_t id, const String &name) {
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
void UsersHandler::forEachAclItem_(WebInterface &web, uint8_t unit, UsersRegistry::AclController ctrl, FnT fn, bool *loading)
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
        {
            auto guard = c.sockets().lockGuard();
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = c.sockets().configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const String label = aclItemLabel_(WebUiRu::Users::kItemSocket, cfg->id, cfg->name);
                fn(cfg->id, label);
            }
            return;
        }
        case UsersRegistry::AclController::Lights:
        {
            auto guard = c.sockets().lockGuard();
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = c.sockets().lightConfigByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const String label = aclItemLabel_(WebUiRu::Users::kItemLight, cfg->id, cfg->name);
                fn(cfg->id, label);
            }
            return;
        }
        case UsersRegistry::AclController::Meteo:
        {
            auto guard = c.meteo().lockGuard();
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = c.meteo().configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const String label = aclItemLabel_(WebUiRu::Users::kItemMeteo, cfg->id, cfg->name);
                fn(cfg->id, label);
            }
            return;
        }
        case UsersRegistry::AclController::Thermo:
        {
            auto guard = c.thermo().lockGuard();
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = c.thermo().configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const String label = aclItemLabel_(WebUiRu::Users::kItemThermo, cfg->id, cfg->name);
                fn(cfg->id, label);
            }
            return;
        }
        case UsersRegistry::AclController::Tanks:
        {
            auto guard = c.tanks().lockGuard();
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = c.tanks().configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const String label = aclItemLabel_(WebUiRu::Users::kItemTank, cfg->id, cfg->name);
                fn(cfg->id, label);
            }
            return;
        }
        case UsersRegistry::AclController::Septic:
        {
            auto guard = c.septic().lockGuard();
            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = c.septic().configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const String label = aclItemLabel_(WebUiRu::Users::kItemSeptic, cfg->id, cfg->name);
                fn(cfg->id, label);
            }
            return;
        }
        case UsersRegistry::AclController::Security:
        {
            auto guard = c.security().lockGuard();
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                const auto *cfg = c.security().configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const String label = aclItemLabel_(WebUiRu::Users::kItemSensor, cfg->id, cfg->name);
                fn(cfg->id, label);
            }
            return;
        }
        case UsersRegistry::AclController::Watering:
        {
            auto guard = c.watering().lockGuard();
            for (size_t i = 0; i < WateringController::kRuleCount; ++i)
            {
                const auto *cfg = c.watering().configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const String label = aclItemLabel_(WebUiRu::Users::kItemRule, cfg->id, cfg->name);
                fn(cfg->id, label);
            }
            return;
        }
        case UsersRegistry::AclController::Leak:
        {
            auto guard = c.leak().lockGuard();
            for (size_t i = 0; i < LeakController::kZoneCount; ++i)
            {
                const auto *cfg = c.leak().configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const String label = aclItemLabel_(WebUiRu::Users::kItemLeak, cfg->id, cfg->name);
                fn(cfg->id, label);
            }
            return;
        }
        case UsersRegistry::AclController::Avr:
        {
            auto guard = c.avr().lockGuard();
            if (c.avr().controllerEnabled())
                fn(1, String("AVR"));
            return;
        }
        case UsersRegistry::AclController::Ring:
        {
            auto guard = c.ring().lockGuard();
            if (c.ring().controllerEnabled())
                fn(1, String("Ring"));
            return;
        }
        default:
            break;
        }
    }

    uint32_t node_id = 0;
    if (aclUnitNodeId_(web, unit, node_id))
    {
        switch (ctrl)
        {
        case UsersRegistry::AclController::Sockets:
        {
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::CacheState cache{};
            if (web.network() && web.network()->stackIndexState(node_id, snapshot) &&
                web.network()->stackIndexCacheState(node_id, cache) && snapshot.updated_ms != 0)
            {
                web.network()->forEachStackSocket(node_id, cache.socket_count, [&](uint8_t, const StackUnitSnapshot::SocketItem &it) {
                    if (!it.enabled)
                        return;
                    fn(it.id, aclItemLabel_(WebUiRu::Users::kItemSocket, it.id, it.name[0] ? String(it.name) : String()));
                });
                if (snapshot.sockets_enabled > cache.socket_count)
                {
                    web.requestStackSockets_(node_id);
                    if (loading)
                        *loading = true;
                }
            }
            else
            {
                web.requestStackSockets_(node_id);
                if (loading)
                    *loading = true;
            }
            return;
        }
        case UsersRegistry::AclController::Lights:
        {
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::CacheState cache{};
            if (web.network() && web.network()->stackIndexState(node_id, snapshot) &&
                web.network()->stackIndexCacheState(node_id, cache) && snapshot.updated_ms != 0)
            {
                web.network()->forEachStackLight(node_id, cache.light_count, [&](uint8_t, const StackUnitSnapshot::SocketItem &it) {
                    if (!it.enabled)
                        return;
                    fn(it.id, aclItemLabel_(WebUiRu::Users::kItemLight, it.id, it.name[0] ? String(it.name) : String()));
                });
                if (snapshot.lights_enabled > cache.light_count)
                {
                    web.requestStackLights_(node_id);
                    if (loading)
                        *loading = true;
                }
            }
            else
            {
                web.requestStackLights_(node_id);
                if (loading)
                    *loading = true;
            }
            return;
        }
        case UsersRegistry::AclController::Meteo:
            return;
        case UsersRegistry::AclController::Thermo:
            return;
        case UsersRegistry::AclController::Tanks:
            return;
        case UsersRegistry::AclController::Septic:
            return;
        case UsersRegistry::AclController::Security:
            return;
        case UsersRegistry::AclController::Watering:
            return;
        case UsersRegistry::AclController::Leak:
            return;
        case UsersRegistry::AclController::Avr:
            return;
        case UsersRegistry::AclController::Ring:
            fn(1, String("Ring"));
            return;
        default:
            break;
        }
    }
}

String UsersHandler::aclTiles_(WebInterface &web, const UsersRegistry::User &u, uint8_t unit, bool read_only) {
        String out;
        out.reserve(32768);
        const String dis = read_only ? " disabled" : "";
        forEachAclController_([&](UsersRegistry::AclController ctrl) {
            const size_t ci = (size_t)ctrl;
            const bool allow_ctrl = u.controllerAllowed(unit, ctrl);
            out += "<div class=\"tile\"><div class=\"tile-head\"><strong>";
            out += aclControllerTitle_(ctrl);
            out += "</strong><div class=\"tile-actions\"><button class=\"btn-lite\" type=\"button\" onclick=\"aclToggleTile(this,true)\">";
            out += WebUiRu::Users::kAclAll;
            out += "</button><button class=\"btn-lite\" type=\"button\" onclick=\"aclToggleTile(this,false)\">";
            out += WebUiRu::Users::kAclNone;
            out += "</button><label class=\"switch\"><input type=\"checkbox\" name=\"c_";
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
                {
                    out += "<div class=\"item\"><div class=\"muted\">";
                    out += WebUiRu::Users::kAclLoadingUnit;
                    out += "</div><div></div><div></div></div>";
                }
                else
                {
                    out += "<div class=\"item\"><div class=\"muted\">";
                    out += WebUiRu::Users::kAclNoEnabledItems;
                    out += "</div><div></div><div></div></div>";
                }
            }
            out += "</div></div>";
        });
        return out;
    }

String UsersHandler::ibuttonDatalist_(WebInterface &web) {
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

String UsersHandler::rfidDatalist_(WebInterface &web) {
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
