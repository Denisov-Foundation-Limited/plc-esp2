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

class SecurityHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/security/arm", HTTP_POST,
                  [&web](AsyncWebServerRequest *request) { handleSecurityArm(web, request); });
        server.on("/security", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleSecuritySave(web, request); });
        server.on("/security", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleSecurity(web, request); });
    }

    static void handleSecurity(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceSecurityHtml);
        const uint8_t page_size = 8u;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = web.isStackSecurityView_(node_id);
        if (stack_view)
            web.requestStackSecurity_(node_id);
        const String page_str = web.paramValueAny_(request, "page");
        uint8_t page_idx = 0;
        if (page_str.length())
        {
            const int v = page_str.toInt();
            if (v > 0)
                page_idx = (uint8_t)(v - 1);
        }
        uint8_t max_pages = 1;
        uint8_t start = 0;
        uint8_t end = SecurityController::kSensorCount;
        if (!stack_view)
        {
            max_pages = (uint8_t)((SecurityController::kSensorCount + page_size - 1) / page_size);
            if (page_idx >= max_pages)
                page_idx = max_pages ? (uint8_t)(max_pages - 1) : 0;
            start = (uint8_t)(page_idx * page_size);
            end = (uint8_t)(start + page_size - 1);
        }
        else
        {
            page_idx = 0;
        }
        page.reserve(page.length() + 16384);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%BOARD_NAME%", ActiveBoardProfile::UI_NAME);
        if (!web._controllers)
        {
            page.replace("%SECURITY_ENABLED_CHECKED%", "");
            page.replace("%SECURITY_ENABLED_LABEL%", "недоступно");
            page.replace("%SECURITY_ARMED_LABEL%", "недоступно");
            page.replace("%SECURITY_ARMED_CHECKED%", "");
            page.replace("%SECURITY_ALARM_LABEL%", "недоступно");
            page.replace("%SECURITY_GSM_LABEL%", web.gsmStatusLabel_());
            page.replace("%SECURITY_SIREN%", "");
            page.replace("%SECURITY_KEYS_ROWS%", "");
            page.replace("%SECURITY_RFID_KEYS_ROWS%", "");
            page.replace("%SECURITY_PHONES_ROWS%", "");
            page.replace("%SECURITY_SENSORS%", "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>");
            page.replace("%SECURITY_SENSORS_PAGE%", "1");
            page.replace("%SECURITY_SENSORS_PAGES%", String((unsigned)(max_pages ? max_pages : 1)));
            page.replace("%SECURITY_SENSOR_JSON%", "[]");
            page.replace("%SECURITY_SENSOR_USED_JSON%", "[]");
            page.replace("%SECURITY_SIREN_JSON%", "[]");
            page.replace("%SECURITY_SIREN_USED_JSON%", "[]");
            page.replace("%SECURITY_STATUS%", web._security_status);
            page.replace("%SECURITY_SENSORS_TITLE%", "Датчики");
            page.replace("%SECURITY_SENSORS_PAGINATION_STYLE%", "");
            page.replace("%SECURITY_SAVE_BTN%", "");
            page.replace("%SECURITY_DEVICE_SELECT%", "");
            web.sendHtml_(request, page, set_cookie);
            return;
        }

        SecurityController &sec = web._controllers->security();
        page.replace("%SECURITY_ENABLED_CHECKED%", sec.controllerEnabled() ? "checked" : "");
        page.replace("%SECURITY_ENABLED_LABEL%", sec.controllerEnabled() ? "включено" : "выключено");
        page.replace("%SECURITY_ARMED_LABEL%", sec.armed() ? "под охраной" : "снято");
        page.replace("%SECURITY_ARMED_CHECKED%", sec.armed() ? "checked" : "");
        page.replace("%SECURITY_ALARM_LABEL%", sec.alarmOn() ? "on" : "off");
        page.replace("%SECURITY_GSM_LABEL%", web.gsmStatusLabel_());
        if (sec.sirenPort() != SecurityController::kInvalidPort)
            page.replace("%SECURITY_SIREN%", String((unsigned)sec.sirenPort()));
        else
            page.replace("%SECURITY_SIREN%", "");
        page.replace("%SECURITY_KEYS_ROWS%", web.listSecurityKeysHtml_());
        page.replace("%SECURITY_RFID_KEYS_ROWS%", web.listSecurityRfidKeysHtml_());
        page.replace("%SECURITY_PHONES_ROWS%", web.listSecurityPhonesHtml_());
        page.replace("%SECURITY_SENSORS%",
                     stack_view ? web.listStackSecuritySensorsTiles_(node_id) : web.listSecuritySensorsTiles_(start, end));
        page.replace("%SECURITY_SENSORS_PAGE%", String((unsigned)(page_idx + 1)));
        page.replace("%SECURITY_SENSORS_PAGES%", String((unsigned)max_pages));
        page.replace("%SECURITY_SENSOR_JSON%", web.securityPortOptionsJson_());
        page.replace("%SECURITY_SENSOR_USED_JSON%", web.securityUsedPinsJson_());
        page.replace("%SECURITY_SIREN_JSON%", web.socketPortOptionsJson_(PortIO::PinType::Relay));
        page.replace("%SECURITY_SIREN_USED_JSON%", web.socketUsedPortsJson_(PortIO::PinType::Relay));
        page.replace("%SECURITY_STATUS%", stack_view ? web.stackSecurityStatusText_(node_id) : web._security_status);
        page.replace("%SECURITY_SENSORS_TITLE%", stack_view ? web.stackSecurityTitle_(node_id) : String("Датчики"));
        page.replace("%SECURITY_SENSORS_PAGINATION_STYLE%", stack_view ? "style=\"display:none\"" : "");
        page.replace("%SECURITY_SAVE_BTN%",
                     stack_view ? "" : "<button class=\"primary\" name=\"action\" value=\"save\">Сохранить</button>");
        page.replace("%SECURITY_DEVICE_SELECT%", web.securityDeviceSelectHtml_(node_id, stack_view));
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleSecuritySave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        if (web.isStackSecurityView_(node_id))
        {
            web._security_status = "Доступно только на локальном устройстве";
            web.sendRedirect_(request, "/security", set_cookie);
            return;
        }
        if (!web._controllers)
        {
            web._security_status = "Security unavailable";
            web.sendRedirect_(request, "/security", set_cookie);
            return;
        }
        SecurityController &sec = web._controllers->security();
        const String action = web.paramValue_(request, "action");
        if (action == "arm")
        {
            if (sec.armFrom("web", "admin"))
                web._security_status = "Armed";
            else
                web._security_status = "Security disabled";
            web.sendRedirect_(request, "/security", set_cookie);
            return;
        }
        if (action == "disarm")
        {
            sec.disarmFrom("web", "admin");
            web._security_status = "Disarmed";
            web.sendRedirect_(request, "/security", set_cookie);
            return;
        }
        if (action == "clear")
        {
            sec.clearDetect();
            web._security_status = "Detections cleared";
            web.sendRedirect_(request, "/security", set_cookie);
            return;
        }

        bool changed = false;
        if (request->hasParam("security_enabled", true))
        {
            const bool enabled = request->hasParam("security_enabled", true);
            if (sec.controllerEnabled() != enabled)
            {
                sec.setControllerEnabled(enabled);
                changed = true;
            }
        }

        String siren_str = web.paramValue_(request, "security_siren");
        siren_str.trim();
        if (siren_str.length() > 0)
        {
            uint8_t siren_port = sec.sirenPort();
            bool set_siren = false;
            if (siren_str == "none")
            {
                siren_port = SecurityController::kInvalidPort;
                set_siren = true;
            }
            else
            {
                const int v = siren_str.toInt();
                if (v >= 0 && v <= 255)
                {
                    siren_port = (uint8_t)v;
                    set_siren = true;
                }
            }
            if (set_siren && sec.sirenPort() != siren_port)
            {
                sec.setSirenPort(siren_port);
                changed = true;
            }
        }

        uint8_t new_keys[SecurityController::kKeyCount][8] = {};
        bool new_set[SecurityController::kKeyCount] = {};
        String new_key_names[SecurityController::kKeyCount];
        for (size_t i = 0; i < SecurityController::kKeyCount; ++i)
        {
            const String idx = String((unsigned)(i + 1));
            const String en_key = String("k") + idx + "_en";
            const String serial_key = String("k") + idx + "_serial";
            const String name_key = String("k") + idx + "_name";
            const bool enabled = request->hasParam(en_key, true);
            String serial = web.paramValue_(request, serial_key);
            serial.trim();
            String name = web.paramValue_(request, name_key);
            name.trim();
            if (!enabled)
            {
                new_key_names[i] = name;
                continue;
            }
            if (!web.parseSecurityKeyHex_(serial, new_keys[i]))
            {
                web._security_status = String("Invalid key ") + idx;
                web.sendRedirect_(request, "/security", set_cookie);
                return;
            }
            new_set[i] = true;
            new_key_names[i] = name;
        }

        bool keys_changed = false;
        for (size_t i = 0; i < SecurityController::kKeyCount; ++i)
        {
            uint8_t old_addr[8] = {};
            bool old_enabled = false;
            sec.keySlot(i, old_addr, old_enabled);
            if (old_enabled != new_set[i])
            {
                keys_changed = true;
                break;
            }
            if (old_enabled && memcmp(old_addr, new_keys[i], 8) != 0)
            {
                keys_changed = true;
                break;
            }
            if (sec.keyNameByIndex(i) != new_key_names[i])
            {
                keys_changed = true;
                break;
            }
        }
        if (keys_changed)
        {
            for (size_t i = 0; i < SecurityController::kKeyCount; ++i)
                sec.setKeySlot(i, new_keys[i], new_set[i], new_key_names[i]);
            changed = true;
        }

        uint8_t new_rfid[SecurityController::kRfidKeyCount][10] = {};
        uint8_t new_rfid_len[SecurityController::kRfidKeyCount] = {};
        bool new_rfid_set[SecurityController::kRfidKeyCount] = {};
        String new_rfid_names[SecurityController::kRfidKeyCount];
        for (size_t i = 0; i < SecurityController::kRfidKeyCount; ++i)
        {
            const String idx = String((unsigned)(i + 1));
            const String en_key = String("rk") + idx + "_en";
            const String serial_key = String("rk") + idx + "_serial";
            const String name_key = String("rk") + idx + "_name";
            const bool enabled = request->hasParam(en_key, true);
            String serial = web.paramValue_(request, serial_key);
            serial.trim();
            String name = web.paramValue_(request, name_key);
            name.trim();
            if (!enabled)
            {
                new_rfid_names[i] = name;
                continue;
            }
            if (!SecurityController::parseRfidSerial(serial.c_str(), new_rfid[i], new_rfid_len[i]))
            {
                web._security_status = String("Invalid RFID key ") + idx;
                web.sendRedirect_(request, "/security", set_cookie);
                return;
            }
            new_rfid_set[i] = true;
            new_rfid_names[i] = name;
        }

        bool rfid_changed = false;
        for (size_t i = 0; i < SecurityController::kRfidKeyCount; ++i)
        {
            uint8_t old_bytes[10] = {};
            uint8_t old_len = 0;
            bool old_enabled = false;
            sec.rfidKeySlot(i, old_bytes, old_len, old_enabled);
            if (old_enabled != new_rfid_set[i])
            {
                rfid_changed = true;
                break;
            }
            if (old_enabled && (old_len != new_rfid_len[i] ||
                                memcmp(old_bytes, new_rfid[i], old_len) != 0))
            {
                rfid_changed = true;
                break;
            }
            if (sec.rfidKeyNameByIndex(i) != new_rfid_names[i])
            {
                rfid_changed = true;
                break;
            }
        }
        if (rfid_changed)
        {
            for (size_t i = 0; i < SecurityController::kRfidKeyCount; ++i)
                sec.setRfidKeySlot(i, new_rfid[i], new_rfid_len[i], new_rfid_set[i], new_rfid_names[i]);
            changed = true;
        }

        auto normalizePhone = [](const String &number) -> String {
            String out;
            out.reserve(number.length());
            for (size_t i = 0; i < number.length(); ++i)
            {
                const char c = number.charAt(i);
                if (c >= '0' && c <= '9')
                    out += c;
            }
            return out;
        };
        for (size_t i = 0; i < SecurityController::kPhoneCount; ++i)
        {
            const String idx = String((unsigned)(i + 1));
            const String en_key = String("p") + idx + "_en";
            const String num_key = String("p") + idx + "_num";
            const String name_key = String("p") + idx + "_name";
            const String notify_key = String("p") + idx + "_notify";
            const String call_key = String("p") + idx + "_call";
            const bool enabled = request->hasParam(en_key, true);
            const bool notify = request->hasParam(notify_key, true);
            const bool call = request->hasParam(call_key, true);
            String number = web.paramValue_(request, num_key);
            number.trim();
            String name = web.paramValue_(request, name_key);
            name.trim();
            String norm = enabled ? normalizePhone(number) : String();
            if (enabled && norm.length() == 0)
            {
                web._security_status = String("Invalid phone ") + idx;
                web.sendRedirect_(request, "/security", set_cookie);
                return;
            }
            String old_number;
            bool old_enabled = false;
            sec.phoneSlot(i, old_number, old_enabled);
            if (old_enabled != enabled || old_number != norm ||
                sec.phoneNameByIndex(i) != name || sec.phoneNotifyByIndex(i) != notify ||
                sec.phoneCallByIndex(i) != call)
            {
                sec.setPhone(i, norm);
                sec.setPhoneEnabled(i, enabled);
                sec.setPhoneName(i, name);
                sec.setPhoneNotify(i, notify);
                sec.setPhoneCall(i, call);
                changed = true;
            }
        }

        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = sec.configByIndex(i);
            if (!cfg)
                continue;
            const String idx = String((unsigned)cfg->id);
            const String prefix = String("sec") + idx + "_";
            const String en_key = prefix + "en";
            const String name_key = prefix + "name";
            const String type_key = prefix + "type";
            const String port_key = prefix + "port";
            const String silent_key = prefix + "silent";
            const bool has_any = request->hasParam(en_key, true) ||
                                 request->hasParam(name_key, true) ||
                                 request->hasParam(type_key, true) ||
                                 request->hasParam(port_key, true) ||
                                 request->hasParam(silent_key, true);
            if (!has_any)
                continue;
            const bool enabled = request->hasParam(en_key, true);
            const bool silent = request->hasParam(silent_key, true);
            String name = web.paramValue_(request, name_key);
            name.trim();
            SecurityController::SensorType type = SecurityController::SensorType::Pir;
            if (!web.parseSecurityType_(web.paramValue_(request, type_key), type))
            {
                web._security_status = String("Invalid type for sensor ") + idx;
                web.sendRedirect_(request, "/security", set_cookie);
                return;
            }
            uint8_t port = SecurityController::kInvalidPort;
            if (!web.parseSocketPort_(web.paramValue_(request, port_key), port))
            {
                web._security_status = String("Invalid port for sensor ") + idx;
                web.sendRedirect_(request, "/security", set_cookie);
                return;
            }
            if (cfg->enabled != enabled)
            {
                sec.setEnabled(cfg->id, enabled);
                changed = true;
            }
            if (cfg->name != name)
            {
                sec.setName(cfg->id, name);
                changed = true;
            }
            if (cfg->type != type)
            {
                sec.setType(cfg->id, type);
                changed = true;
            }
            if (cfg->port != port)
            {
                sec.setPort(cfg->id, port);
                changed = true;
            }
            if (cfg->silent != silent)
            {
                sec.setSilent(cfg->id, silent);
                changed = true;
            }
        }

        bool ok = true;
        if (changed)
        {
            if (!web._configs_manager)
            {
                ok = false;
                web._security_status = "Config manager missing";
            }
            else if (!web._configs_manager->save())
            {
                ok = false;
                web._security_status = "Save failed";
            }
        }
        if (ok)
            web._security_status = changed ? "Updated" : "No changes";
        web.sendRedirect_(request, "/security", set_cookie);
    }

    static void handleSecurityArm(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web._controllers)
        {
            web.sendText_(request, 500, "text/plain", "0", set_cookie);
            return;
        }
        SecurityController &sec = web._controllers->security();
        const String armed_str = web.paramValueAny_(request, "armed");
        if (armed_str.length() == 0)
        {
            web._security_status = "Bad request";
            web.sendText_(request, 400, "text/plain", "err", set_cookie);
            return;
        }
        const bool desired = (armed_str == "1" || armed_str == "true" || armed_str == "on");
        bool ok = true;
        if (sec.armed() != desired)
        {
            if (desired)
            {
                if (!sec.controllerEnabled())
                    sec.setControllerEnabled(true);
                ok = sec.armFrom("web", "admin");
            }
            else
            {
                sec.disarmFrom("web", "admin");
                ok = true;
            }
        }
        if (!ok)
            web._security_status = "Security disabled";
        if (desired && !sec.armed())
        {
            web._security_status = "Arm blocked";
            web.sendText_(request, 200, "text/plain", "blocked", set_cookie);
        }
        else
        {
            web.sendText_(request, 200, "text/plain", sec.armed() ? "1" : "0", set_cookie);
        }
    }
};
