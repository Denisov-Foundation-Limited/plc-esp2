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

#include "core/network/web/handlers/display_handler.hpp"

#include "core/network/web/web_interface.hpp"

void DisplayHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/display", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleDisplaySave(web, request); });
        server.on("/display", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleDisplay(web, request); });
    }

void DisplayHandler::handleDisplay(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceDisplayHtml);
        page.reserve(page.length() + 2048);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%DISPLAY_PAGE_TITLE%", WebUiRu::DisplayPage::kPageTitle);
        page.replace("%DISPLAY_TITLE%", WebUiRu::DisplayPage::kTitle);
        page.replace("%DISPLAY_SRC_TIME%", WebUiRu::DisplayPage::kSrcTime);
        page.replace("%DISPLAY_SRC_SOCKET%", WebUiRu::DisplayPage::kSrcSocket);
        page.replace("%DISPLAY_SRC_LIGHT%", WebUiRu::DisplayPage::kSrcLight);
        page.replace("%DISPLAY_SRC_METEO%", WebUiRu::DisplayPage::kSrcMeteo);
        page.replace("%DISPLAY_SRC_THERMO%", WebUiRu::DisplayPage::kSrcThermo);
        page.replace("%DISPLAY_SRC_TANK%", WebUiRu::DisplayPage::kSrcTank);
        page.replace("%DISPLAY_SRC_SEPTIC%", WebUiRu::DisplayPage::kSrcSeptic);
        page.replace("%DISPLAY_SRC_SECURITY%", WebUiRu::DisplayPage::kSrcSecurity);
        page.replace("%DISPLAY_SRC_AVR%", WebUiRu::DisplayPage::kSrcAvr);
        page.replace("%DISPLAY_SRC_LEAK%", WebUiRu::DisplayPage::kSrcLeak);
        page.replace("%DISPLAY_SRC_TEXT%", WebUiRu::DisplayPage::kSrcText);
        page.replace("%DISPLAY_FIELD_STATE%", WebUiRu::DisplayPage::kFieldState);
        page.replace("%DISPLAY_FIELD_TEMP%", WebUiRu::DisplayPage::kFieldTemp);
        page.replace("%DISPLAY_FIELD_HUM%", WebUiRu::DisplayPage::kFieldHum);
        page.replace("%DISPLAY_FIELD_STATUS%", WebUiRu::DisplayPage::kFieldStatus);
        page.replace("%DISPLAY_FIELD_LEVEL%", WebUiRu::DisplayPage::kFieldLevel);
        page.replace("%DISPLAY_FIELD_SOURCE%", WebUiRu::DisplayPage::kFieldSource);
        page.replace("%DISPLAY_FIELD_MAIN_POWER%", WebUiRu::DisplayPage::kFieldMainPower);
        page.replace("%DISPLAY_FIELD_RESERVE_POWER%", WebUiRu::DisplayPage::kFieldReservePower);
        page.replace("%DISPLAY_FIELD_TEXT%", WebUiRu::DisplayPage::kFieldText);
        page.replace("%DISPLAY_SLOTS%", web.displaySlotsHtml_());
        page.replace("%DISPLAY_DEVICE_JSON%", web.displayDeviceOptionsJson_());
        page.replace("%DISPLAY_SOCKET_JSON%", web.displaySocketOptionsJson_());
        page.replace("%DISPLAY_LIGHT_JSON%", web.displayLightOptionsJson_());
        page.replace("%DISPLAY_METEO_JSON%", web.displayMeteoOptionsJson_());
        page.replace("%DISPLAY_THERMO_JSON%", web.displayThermoOptionsJson_());
        page.replace("%DISPLAY_TANK_JSON%", web.displayTankOptionsJson_());
        page.replace("%DISPLAY_SEPTIC_JSON%", web.displaySepticOptionsJson_());
        page.replace("%DISPLAY_AVR_JSON%", web.displayAvrOptionsJson_());
        page.replace("%DISPLAY_LEAK_JSON%", web.displayLeakOptionsJson_());
        page.replace("%SAVE_TEXT%", WebUiRu::kSave);
        if (!web._display_status.length())
            web._display_status = "OK";
        page.replace("%DISPLAY_STATUS%", web._display_status);
        web.sendHtml_(request, page, set_cookie);
    }

void DisplayHandler::handleDisplaySave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web._configs_manager)
        {
            web._display_status = "Config manager missing";
            web.sendRedirect_(request, "/display", set_cookie);
            return;
        }
        bool changed = false;
        const size_t count = web._configs_manager->displaySlotCount();
        for (size_t i = 0; i < Display::kSlotCount && i < count; ++i)
        {
            DisplaySlotConfig slot{};
            DisplaySlotConfig current{};
            web._configs_manager->displaySlot(i, current);
            const String prefix = String("ds") + String((unsigned)i);
            const String kind_str = web.paramValue_(request, prefix + "_kind");
            const String field_str = web.paramValue_(request, prefix + "_field");
            const String idx_str = web.paramValue_(request, prefix + "_index");
            const String node_str = web.paramValue_(request, prefix + "_node");
            DisplaySlotKind kind = DisplaySlotKind::None;
            DisplaySlotField field = DisplaySlotField::None;
            if (!web.parseDisplaySlotKind_(kind_str, kind))
                kind = DisplaySlotKind::None;
            if (!web.parseDisplaySlotField_(field_str, field))
                field = DisplaySlotField::None;
            if (kind == DisplaySlotKind::Light && field == DisplaySlotField::SocketState)
                field = DisplaySlotField::LightState;
            if (kind == DisplaySlotKind::Thermo && field == DisplaySlotField::SocketState)
                field = DisplaySlotField::ThermoState;
            if (kind == DisplaySlotKind::Septic && field == DisplaySlotField::TankLevel)
                field = DisplaySlotField::SepticLevel;
            uint16_t idx = 0;
            if (web.parseUint_(idx_str, idx))
                slot.index = (uint8_t)idx;
            uint32_t node_id = 0;
            if (web.parseUint_(node_str, node_id))
                slot.node_id = node_id;
            slot.kind = kind;
            slot.field = field;
            const String text_str = web.paramValue_(request, prefix + "_text");
            if (text_str.length())
            {
                String t = text_str;
                if (t.length() > 4)
                    t.remove(4);
                strncpy(slot.text, t.c_str(), sizeof(slot.text) - 1);
                slot.text[sizeof(slot.text) - 1] = '\0';
            }
            if (!displaySlotEqual_(slot, current))
            {
                web._configs_manager->setDisplaySlot(i, slot);
                changed = true;
            }
        }
        bool ok = true;
        if (changed)
        {
            if (!web._configs_manager->save())
            {
                ok = false;
                web._display_status = "Save failed";
            }
        }
        if (ok)
            web._display_status = changed ? "Saved" : "No changes";
        web.sendRedirect_(request, "/display", set_cookie);
    }

bool DisplayHandler::displaySlotEqual_(const DisplaySlotConfig &a, const DisplaySlotConfig &b) {
        if (a.kind != b.kind || a.node_id != b.node_id || a.index != b.index || a.field != b.field)
            return false;
        return strncmp(a.text, b.text, sizeof(a.text)) == 0;
    }
