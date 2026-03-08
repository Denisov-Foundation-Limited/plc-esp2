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
            const auto *cache = web._stack_cache ? web._stack_cache->groupsCache(node_id) : nullptr;
            if ((!cache || (!cache->has_data && !cache->pending && !cache->last_error.length())) && web._stack_cache)
            {
                web._stack_cache->requestGroups(node_id);
                cache = web._stack_cache->groupsCache(node_id);
            }
            if (cache && cache->pending && cache->updated_ms &&
                (uint32_t)(millis() - cache->updated_ms) > 4500u)
            {
                rows = String("<tr><td colspan=\"4\" class=\"muted\"><strong>") +
                       WebUiRu::kErrorPrefix + "timeout" + "</strong></td></tr>";
            }
            else if (cache && !cache->pending && !cache->last_ok && cache->last_error.length())
            {
                rows = String("<tr><td colspan=\"4\" class=\"muted\"><strong>") +
                       WebUiRu::kErrorPrefix + cache->last_error + "</strong></td></tr>";
            }
            else if (!cache || cache->pending || !cache->has_data)
            {
                rows = String("<tr><td colspan=\"4\" class=\"muted\"><strong>") +
                       WebUiRu::GroupsPage::kWaitingSlave + "</strong></td></tr>";
                page.replace("%GROUPS_AUTO_REFRESH%",
                             "<meta http-equiv=\"refresh\" content=\"1\">"
                             "<script>setTimeout(function(){ window.location.reload(); }, 1000);</script>");
            }
            else if (cache->item_count == 0)
            {
                rows = String("<tr><td colspan=\"4\" class=\"muted\"><strong>") +
                       WebUiRu::GroupsPage::kEmptyList + "</strong></td></tr>";
            }
            else
            {
                appendRowsFromStackCache_(web, rows, cache);
            }
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
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
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
        if (!web._stack_master || !web._stack_cache)
        {
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        const auto *cache = web._stack_cache->groupsCache(node_id);
        if (!cache || !cache->has_data || !cache->items)
        {
            web._stack_cache->requestGroups(node_id);
            web.sendRedirect_(request, back, set_cookie);
            return;
        }

        String action = web.paramValue_(request, "action");
        action.trim();
        action.toLowerCase();

        DynamicJsonDocument doc(2048);
        doc["cmd_id"] = 0;
        doc["feature"] = (uint8_t)StackFeature::Groups;
        doc["action"] = "set";
        JsonArray groups = doc["params"]["groups"].to<JsonArray>();

        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &g = cache->items[i];
            if (g.id == 0)
                continue;
            const String prefix = String("g") + String((unsigned)g.id) + "_";
            if (action != "add" && request->hasParam(prefix + "delete", true))
                continue;
            String name = (action == "add") ? String(g.name) : web.paramValue_(request, prefix + "name");
            name.trim();
            if (!name.length())
                continue;
            const uint16_t sort = (uint16_t)((action == "add") ? g.sort : web.paramValue_(request, prefix + "sort").toInt());
            JsonObject item = groups.add<JsonObject>();
            item["id"] = (unsigned)g.id;
            item["name"] = name;
            item["sort"] = (unsigned)sort;
        }
        if (action == "add")
        {
            String name = web.paramValue_(request, "name");
            name.trim();
            if (name.length())
            {
                JsonObject item = groups.add<JsonObject>();
                item["id"] = 0;
                item["name"] = name;
                item["sort"] = (unsigned)((uint16_t)web.paramValue_(request, "sort").toInt());
            }
        }

        char payload[2048] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0 || !web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                   reinterpret_cast<const uint8_t *>(payload), len))
        {
            web.sendRedirect_(request, back, set_cookie);
            return;
        }
        if (auto *cache_mut = web._stack_cache->groupsCache(node_id))
        {
            cache_mut->has_data = false;
            cache_mut->pending = false;
            cache_mut->pending_cmd_id = 0;
            cache_mut->updated_ms = 0;
            cache_mut->item_count = 0;
            cache_mut->last_error = "";
        }
        web._stack_cache->requestGroups(node_id);
        web.sendRedirect_(request, back, set_cookie);
    }
