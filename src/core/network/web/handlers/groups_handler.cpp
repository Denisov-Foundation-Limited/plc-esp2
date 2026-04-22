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

#include "core/network/web/handlers/groups_handler.hpp"

#include "core/network/web/web_interface.hpp"

void GroupsHandler::registerRoutes(WebInterface &web, AsyncWebServer &server) {
        server.on("/groups", HTTP_POST, [&web](AsyncWebServerRequest *request) { handleGroupsSave(web, request); });
        server.on("/groups", HTTP_GET, [&web](AsyncWebServerRequest *request) { handleGroups(web, request); });
    }

void GroupsHandler::handleGroups(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;

        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = isStackGroupsView_(web, node_id);

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
        page.replace("%GROUPS_DEVICE_SELECT%", web.indexDeviceSelectHtml_(node_id, stack_view));
        page.replace("%GROUPS_AUTO_REFRESH%", "");

        const String hidden = stack_view
                                  ? String("<input type=\"hidden\" name=\"unit\" value=\"stack\"><input type=\"hidden\" name=\"node\" value=\"") +
                                        String((unsigned long)node_id) + "\">"
                                  : "";
        page.replace("%GROUPS_SAVE_HIDDEN%", hidden);
        page.replace("%GROUPS_ADD_HIDDEN%", hidden);

        String rows;
        rows.reserve(2048);
        if (stack_view)
        {
            DynamicJsonDocument doc(64);
            web.network()->stackRoute().sendRequestSelected(ConfigsManagerIface::StackPayloadMode::Json, node_id,
                                                            "groups", "snapshot_req", &doc, true);
            StackUnitSnapshot::CacheState cache{};
            if (web.network() && web.network()->stackIndexCacheState(node_id, cache) && cache.group_count > 0)
            {
                for (uint8_t i = 0; i < cache.group_count; ++i)
                {
                    StackUnitSnapshot::GroupItem group{};
                    if (!web.network()->stackIndexGroupAt(node_id, i, group) || group.id == 0 || group.name[0] == '\0')
                        continue;
                    appendGroupRow_(web, rows, group.id, group.name, group.sort);
                }
            }
            if (rows.length() == 0)
                rows = String("<tr><td colspan=\"4\" class=\"muted\"><strong>") +
                       WebUiRu::GroupsPage::kEmptyList + "</strong></td></tr>";
        }
        else if (!web._configs_manager || web._configs_manager->groupCount() == 0)
        {
            rows = String("<tr><td colspan=\"4\" class=\"muted\"><strong>") +
                   WebUiRu::GroupsPage::kEmptyList + "</strong></td></tr>";
        }
        else
        {
            appendRowsFromLocalConfig_(web, rows);
        }

        page.replace("%GROUPS_ROWS%", rows);
        web.sendHtml_(request, page, set_cookie);
    }

void GroupsHandler::handleGroupsSave(WebInterface &web, AsyncWebServerRequest *request) {
        bool set_cookie = false;
        if (!web.checkAuth_(request, &set_cookie))
            return;
        if (!web.requireWebAdmin_(request, &set_cookie))
            return;

        const uint32_t node_id = web.parseStackNodeIdParam_(request);
        const bool stack_view = isStackGroupsView_(web, node_id);
        const String back = groupsPath_(node_id, stack_view);
        if (stack_view)
        {
            handleGroupsSaveStack_(web, request, node_id, back, set_cookie);
            return;
        }
        if (!web._configs_manager)
        {
            web._groups_status = WebUiRu::kConfigManagerUnavailable;
            web.sendRedirect_(request, back, set_cookie);
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
            web.sendRedirect_(request, back, set_cookie);
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
        web.sendRedirect_(request, back, set_cookie);
    }

bool GroupsHandler::isStackGroupsView_(WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

String GroupsHandler::groupsPath_(uint32_t node_id, bool stack_view) {
        if (!stack_view || node_id == 0)
            return "/groups";
        return String("/groups?unit=stack&node=") + String((unsigned long)node_id);
    }

void GroupsHandler::appendGroupRow_(WebInterface &web, String &rows, uint8_t id, const char *name, uint16_t sort) {
        rows += "<tr><td class=\"right\"><strong>";
        rows += String((unsigned)id);
        rows += "</strong><input type=\"hidden\" name=\"g";
        rows += String((unsigned)id);
        rows += "_id\" value=\"";
        rows += String((unsigned)id);
        rows += "\"></td><td><input class=\"field\" type=\"text\" name=\"g";
        rows += String((unsigned)id);
        rows += "_name\" value=\"";
        web.appendHtmlEscaped_(rows, name ? name : "");
        rows += "\"></td><td class=\"right\"><input class=\"field mini\" type=\"number\" min=\"0\" max=\"65535\" name=\"g";
        rows += String((unsigned)id);
        rows += "_sort\" value=\"";
        rows += String((unsigned)sort);
        rows += "\"></td><td class=\"right\"><label><input type=\"checkbox\" name=\"g";
        rows += String((unsigned)id);
        rows += "_delete\"> ";
        rows += WebUiRu::GroupsPage::kDeleteLower;
        rows += "</label></td></tr>";
    }

void GroupsHandler::appendRowsFromLocalConfig_(WebInterface &web, String &rows) {
        for (size_t i = 0; i < web._configs_manager->groupCount(); ++i)
        {
            ConfigsManagerIface::GroupConfig g;
            if (!web._configs_manager->groupByIndex(i, g) || g.id == 0)
                continue;
            appendGroupRow_(web, rows, g.id, g.name.c_str(), g.sort);
        }
    }

template <typename TCache>
void GroupsHandler::appendRowsFromStackCache_(WebInterface &web, String &rows, const TCache *cache)
{
    if (!cache || !cache->items)
        return;
    for (size_t i = 0; i < cache->item_count; ++i)
    {
        const auto &g = cache->items[i];
        if (g.id == 0 || !g.name[0])
            continue;
        appendGroupRow_(web, rows, g.id, g.name, g.sort);
    }
}

void GroupsHandler::handleGroupsSaveStack_(WebInterface &web, AsyncWebServerRequest *request,
                                       uint32_t node_id, const String &back, bool set_cookie) {
        if (!web.network())
        {
            web._groups_status = WebUiRu::kNoDataFromSlave;
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        DynamicJsonDocument doc(2048);
        JsonArray items = doc["items"].to<JsonArray>();
        String action = web.paramValue_(request, "action");
        action.trim();
        action.toLowerCase();
        if (action == "add")
        {
            String name = web.paramValue_(request, "name");
            name.trim();
            if (!name.length())
            {
                web._groups_status = WebUiRu::GroupsPage::kAddFailed;
                web.sendRedirect_(request, back, set_cookie);
                return;
            }
            StackUnitSnapshot::CacheState cache{};
            uint8_t next_id = 1;
            if (web.network()->stackIndexCacheState(node_id, cache))
            {
                for (uint8_t i = 0; i < cache.group_count; ++i)
                {
                    StackUnitSnapshot::GroupItem group{};
                    if (web.network()->stackIndexGroupAt(node_id, i, group) && group.id >= next_id)
                        next_id = (uint8_t)(group.id + 1u);
                }
            }
            JsonObject item = items.add<JsonObject>();
            item["id"] = next_id;
            item["name"] = name;
            item["sort"] = (uint16_t)web.paramValue_(request, "sort").toInt();
        }
        else
        {
            StackUnitSnapshot::CacheState cache{};
            if (web.network()->stackIndexCacheState(node_id, cache))
            {
                for (uint8_t i = 0; i < cache.group_count; ++i)
                {
                    StackUnitSnapshot::GroupItem group{};
                    if (!web.network()->stackIndexGroupAt(node_id, i, group) || group.id == 0)
                        continue;
                    const String prefix = String("g") + String((unsigned)group.id) + "_";
                    JsonObject item = items.add<JsonObject>();
                    item["id"] = group.id;
                    item["name"] = web.paramValue_(request, prefix + "name");
                    item["sort"] = (uint16_t)web.paramValue_(request, prefix + "sort").toInt();
                    if (request->hasParam(prefix + "delete", true))
                        item["delete"] = true;
                }
            }
        }
        const bool sent = web.network()->stackRoute().sendEventSelected(ConfigsManagerIface::StackPayloadMode::Json,
                                                                        node_id, "groups", "set", &doc);
        if (sent)
        {
            DynamicJsonDocument refresh_doc(32);
            web.network()->stackRoute().sendRequestSelected(ConfigsManagerIface::StackPayloadMode::Json, node_id,
                                                            "groups", "snapshot_req", &refresh_doc, true);
        }
        web._groups_status = sent ? WebUiRu::Common::kUpdated : "Stack send failed";
        web.sendRedirect_(request, back, set_cookie);
    }
