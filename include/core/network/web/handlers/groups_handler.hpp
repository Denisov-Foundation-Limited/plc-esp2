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

class GroupsHandler
{
public:
    static void registerRoutes(WebInterface &web, AsyncWebServer &server)
    {
        server.on("/groups", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleGroupsSave(web, request); });
        server.on("/groups", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleGroups(web, request); });
    }

    static void handleGroups(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        String page = FPSTR(kWebInterfaceGroupsHtml);
        page.replace("%NAV%", web.navHtml_());
        page.replace("%GROUPS_PAGE_TITLE%", WebUiRu::GroupsPage::kPageTitle);
        page.replace("%GROUPS_TITLE%", WebUiRu::GroupsPage::kTitle);
        page.replace("%GROUPS_DESCRIPTION%", WebUiRu::GroupsPage::kDescription);
        page.replace("%GROUPS_NAME%", WebUiRu::GroupsPage::kName);
        page.replace("%GROUPS_SORT%", WebUiRu::GroupsPage::kSort);
        page.replace("%GROUPS_DELETE%", WebUiRu::GroupsPage::kDelete);
        page.replace("%GROUPS_NEW_GROUP%", WebUiRu::GroupsPage::kNewGroup);
        page.replace("%GROUPS_NAME_PLACEHOLDER%", WebUiRu::GroupsPage::kNamePlaceholder);
        page.replace("%GROUPS_ADD%", WebUiRu::GroupsPage::kAdd);
        page.replace("%GROUPS_SAVE%", WebUiRu::kSave);

        String rows;
        rows.reserve(2048);
        if (!web._configs_manager || web._configs_manager->groupCount() == 0)
        {
            rows = String("<tr><td colspan=\"4\" class=\"muted\"><strong>") +
                   WebUiRu::GroupsPage::kEmptyList + "</strong></td></tr>";
        }
        else
        {
            for (size_t i = 0; i < web._configs_manager->groupCount(); ++i)
            {
                ConfigsManagerIface::GroupConfig g;
                if (!web._configs_manager->groupByIndex(i, g) || g.id == 0)
                    continue;
                rows += "<tr><td class=\"right\"><strong>";
                rows += String((unsigned)g.id);
                rows += "</strong><input type=\"hidden\" name=\"g";
                rows += String((unsigned)g.id);
                rows += "_id\" value=\"";
                rows += String((unsigned)g.id);
                rows += "\"></td><td><input class=\"field\" type=\"text\" name=\"g";
                rows += String((unsigned)g.id);
                rows += "_name\" value=\"";
                web.appendHtmlEscaped_(rows, g.name);
                rows += "\"></td><td class=\"right\"><input class=\"field mini\" type=\"number\" min=\"0\" max=\"65535\" name=\"g";
                rows += String((unsigned)g.id);
                rows += "_sort\" value=\"";
                rows += String((unsigned)g.sort);
                rows += "\"></td><td class=\"right\"><label><input type=\"checkbox\" name=\"g";
                rows += String((unsigned)g.id);
                rows += "_delete\"> ";
                rows += WebUiRu::GroupsPage::kDeleteLower;
                rows += "</label></td></tr>";
            }
        }

        page.replace("%GROUPS_ROWS%", rows);
        web.sendHtml_(request, page, set_cookie);
    }

    static void handleGroupsSave(WebInterface &web, AsyncWebServerRequest *request)
    {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;
        if (!web._configs_manager)
        {
            web._groups_status = WebUiRu::kConfigManagerUnavailable;
            web.sendRedirect_(request, "/groups", set_cookie);
            return;
        }

        String action = web.paramValue_(request, "action");
        action.trim();
        action.toLowerCase();

        if (action == "add")
        {
            String name = web.paramValue_(request, "name");
            name.trim();
            const uint16_t sort = (uint16_t)web.paramValue_(request, "sort").toInt();
            const uint8_t id = web._configs_manager->allocateGroupId();
            if (!name.length() || id == 0 || !web._configs_manager->setGroup(id, name, sort))
                web._groups_status = WebUiRu::GroupsPage::kAddFailed;
            else if (!web._configs_manager->save())
                web._groups_status = WebUiRu::kSaveFailed;
            else
                web._groups_status = WebUiRu::GroupsPage::kAdded;
            web.sendRedirect_(request, "/groups", set_cookie);
            return;
        }

        bool changed = false;
        for (size_t i = 0; i < web._configs_manager->groupCount(); ++i)
        {
            ConfigsManagerIface::GroupConfig g;
            if (!web._configs_manager->groupByIndex(i, g) || g.id == 0)
                continue;
            const String prefix = String("g") + String((unsigned)g.id) + "_";
            if (request->hasParam(prefix + "delete", true))
            {
                changed = web._configs_manager->removeGroup(g.id) || changed;
                continue;
            }
            String name = web.paramValue_(request, prefix + "name");
            name.trim();
            const uint16_t sort = (uint16_t)web.paramValue_(request, prefix + "sort").toInt();
            changed = web._configs_manager->setGroup(g.id, name, sort) || changed;
        }
        if (changed && !web._configs_manager->save())
            web._groups_status = WebUiRu::kSaveFailed;
        else
            web._groups_status = changed ? WebUiRu::kUpdated : WebUiRu::kNoChangesAlt;
        web.sendRedirect_(request, "/groups", set_cookie);
    }
};
