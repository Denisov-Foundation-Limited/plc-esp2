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

#include "core/network/web/web_interface.hpp"

size_t WebInterfaceControllersSocketsHelper::socketsLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        SocketController &sockets = web._controllers->sockets();
        auto guard = sockets.lockGuard();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const auto *cfg = sockets.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return SocketController::kSocketCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > SocketController::kSocketCount ? SocketController::kSocketCount : count;
    }

String WebInterfaceControllersSocketsHelper::listSocketsHtml_(WebInterface &web, uint8_t start_id, uint8_t end_id) {
        if (!web._controllers)
            return WebUiRu::Sockets::kText;
        String items;
        if (start_id == 0)
            start_id = 1;
        if (end_id < start_id)
            end_id = start_id;
        size_t reserve = 2048u + (size_t)(end_id - start_id + 1) * 700u;
        if (reserve < 16384u)
            reserve = 16384u;
        items.reserve(reserve);
        SocketController &sockets = web._controllers->sockets();
        SocketController::SocketConfig cfgs[SocketController::kSocketCount];
        bool cfg_valid[SocketController::kSocketCount] = {};
        bool relay_on[SocketController::kSocketCount] = {};
        size_t last_enabled_idx = SIZE_MAX;
        {
            auto guard = sockets.lockGuard();
            bool tmp_state = false;
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = sockets.configByIndex(i);
                if (!cfg)
                    continue;
                cfgs[i] = *cfg;
                cfg_valid[i] = true;
                if (cfg->enabled)
                {
                    last_enabled_idx = i;
                    relay_on[i] = sockets.relayState(cfg->id, tmp_state) ? tmp_state : false;
                }
            }
        }
        auto appendRow = [&](const SocketController::SocketConfig &cfg, bool enabled, bool on) {
            const bool can_edit = web.webSessionIsAdmin_();
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Sockets, cfg.id);
            const bool has_groups = web.hasGroups_();
            items += "<div class=\"tile js-group-item";
            if (!enabled)
                items += " disabled";
            items += "\" data-group-id=\"";
            items += String((unsigned)cfg.group_id);
            items += "\"";
            items += web.groupVisibilityStyleAttr_(cfg.group_id);
            items += ">";
            items += "<div class=\"sock-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sock-icon ";
            items += on ? "on" : "off";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            items += "<path fill=\"currentColor\" d=\"M16 10h32c3.3 0 6 2.7 6 6v32c0 3.3-2.7 6-6 6H16c-3.3 0-6-2.7-6-6V16c0-3.3 2.7-6 6-6zm0 4c-1.1 0-2 .9-2 2v32c0 1.1.9 2 2 2h32c1.1 0 2-.9 2-2V16c0-1.1-.9-2-2-2H16z\"/>";
            items += "<circle cx=\"24\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
            items += "<circle cx=\"40\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
            items += "<rect x=\"28\" y=\"36\" width=\"8\" height=\"10\" rx=\"2\" fill=\"currentColor\"/>";
            items += "</svg>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\">";
            items += "<strong>";
            items += WebUiRu::Sockets::kTitlePrefix;
            items += String((unsigned)cfg.id);
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-enable\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            if (!can_edit)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "</div>";
            items += String("<div class=\"form-row\"><label>") + WebUiRu::Sockets::kLabelName + "</label><input class=\"field name\" type=\"text\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            web.appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"";
            if (!can_edit)
                items += " readonly";
            items += "></div>";
            items += String("<div class=\"form-row\" style=\"margin-top:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field mini\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_group\"";
            if (!can_edit || !has_groups)
                items += " disabled";
            items += ">";
            items += web.groupOptionsHtml_(cfg.group_id, true, true);
            items += "</select></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += on ? "status-on" : "status-off";
            items += "\"></span>";
            items += "<span class=\"status-text\">";
            items += on ? WebUiRu::Sockets::kText3 : WebUiRu::Sockets::kText4;
            items += "</span>";
            items += "</div>";
            items += "<div class=\"form-grid\">";
            items += WebUiRu::Sockets::kText5;
            items += "<select class=\"field mini socket-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.button_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_btn\"";
            if (!can_edit)
                items += " disabled";
            items += "></select></div>";
            items += WebUiRu::Sockets::kText6;
            items += "<select class=\"field mini socket-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.relay_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_relay\"";
            if (!can_edit)
                items += " disabled";
            items += "></select></div>";
            items += WebUiRu::Sockets::kText7;
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-toggle\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\"";
            if (on)
                items += " checked";
            if (!enabled || !can_control)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div>";
            items += "<input type=\"hidden\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_action\" value=\"\">";
            items += "</div></div>";
        };
    
        const size_t render_count = (last_enabled_idx == SIZE_MAX)
                                        ? (SocketController::kSocketCount ? 1u : 0u)
                                        : ((last_enabled_idx + 2u) > SocketController::kSocketCount
                                               ? SocketController::kSocketCount
                                               : (last_enabled_idx + 2u));
        const bool can_view_disabled = web.webSessionIsAdmin_();
        for (size_t i = 0; i < render_count; ++i)
        {
            if (!cfg_valid[i])
                continue;
            const auto &cfg = cfgs[i];
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Sockets, cfg.id))
                continue;
            if (cfg.id < start_id || cfg.id > end_id)
                continue;
            if (!can_view_disabled && !cfg.enabled)
                continue;
            appendRow(cfg, cfg.enabled, relay_on[i]);
        }
        if (items.length() == 0)
            items = WebUiRu::Sockets::kText8;
        return items;
    }

size_t WebInterfaceControllersSocketsHelper::stackSocketsVisibleCount_(const WebInterface &web, uint32_t node_id) {
            const auto *cache = web._stack_cache ? web._stack_cache->socketsCache(node_id) : nullptr;
            if (!cache || !cache->has_data || !cache->items)
                return 0;
            const bool can_view_disabled = web.webSessionIsAdmin_();
            size_t render_count = cache->item_count;
            if (can_view_disabled)
            {
                size_t last_enabled_idx = SIZE_MAX;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    if (cache->items[i].enabled)
                        last_enabled_idx = i;
                }
                if (last_enabled_idx == SIZE_MAX)
                    render_count = cache->item_count ? 1u : 0u;
                else
                {
                    const size_t rc = last_enabled_idx + 2u;
                    render_count = rc > cache->item_count ? cache->item_count : rc;
                }
            }
            size_t count = 0;
            for (size_t i = 0; i < render_count; ++i)
            {
                const auto &cfg = cache->items[i];
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Sockets, cfg.id, node_id))
                    continue;
                if (!can_view_disabled && !cfg.enabled)
                    continue;
                ++count;
            }
            return count;
        
    }

String WebInterfaceControllersSocketsHelper::listStackSocketsHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
            const auto *cache = web._stack_cache->socketsCache(node_id);
            if (!cache || !cache->has_data)
                return WebUiRu::Sockets::kText9;
            if (cache->item_count == 0)
                return WebUiRu::Sockets::kText8;
            String items;
            const size_t page_limit = (limit == 0) ? 1u : limit;
            size_t reserve = 2048u + page_limit * 420u;
            if (reserve < 8192u)
                reserve = 8192u;
            items.reserve(reserve);
            const bool can_view_disabled = web.webSessionIsAdmin_();
            size_t render_count = cache->item_count;
            if (can_view_disabled)
            {
                size_t last_enabled_idx = SIZE_MAX;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    if (cache->items[i].enabled)
                        last_enabled_idx = i;
                }
                if (last_enabled_idx == SIZE_MAX)
                    render_count = cache->item_count ? 1u : 0u;
                else
                {
                    const size_t rc = last_enabled_idx + 2u;
                    render_count = rc > cache->item_count ? cache->item_count : rc;
                }
            }
            size_t rendered = 0;
            size_t visible_idx = 0;
            for (size_t i = 0; i < render_count && rendered < page_limit; ++i)
            {
                const auto &cfg = cache->items[i];
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Sockets, cfg.id, node_id))
                    continue;
                if (!can_view_disabled && !cfg.enabled)
                    continue;
                if (visible_idx < offset)
                {
                    ++visible_idx;
                    continue;
                }
                ++visible_idx;
                const bool can_edit = web.webSessionIsAdmin_();
                const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Sockets, cfg.id, node_id);
                const bool on = cfg.enabled && cfg.state;
                const bool has_groups = web.hasGroups_(node_id);
                items += "<div class=\"tile js-group-item";
                if (!cfg.enabled)
                    items += " disabled";
                items += "\" data-group-id=\"";
                items += String((unsigned)cfg.group_id);
                items += "\"";
                items += web.groupVisibilityStyleAttr_(cfg.group_id, node_id);
                items += ">";
                items += "<div class=\"sock-visual\">";
                items += "<span class=\"badge\">#";
                items += String((unsigned)cfg.id);
                items += "</span>";
                items += "<svg class=\"sock-icon ";
                items += on ? "on" : "off";
                items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<path fill=\"currentColor\" d=\"M16 10h32c3.3 0 6 2.7 6 6v32c0 3.3-2.7 6-6 6H16c-3.3 0-6-2.7-6-6V16c0-3.3 2.7-6 6-6zm0 4c-1.1 0-2 .9-2 2v32c0 1.1.9 2 2 2h32c1.1 0 2-.9 2-2V16c0-1.1-.9-2-2-2H16z\"/>";
                items += "<circle cx=\"24\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
                items += "<circle cx=\"40\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
                items += "<rect x=\"28\" y=\"36\" width=\"8\" height=\"10\" rx=\"2\" fill=\"currentColor\"/>";
                items += "</svg>";
                items += "</div>";
                items += "<div>";
                items += "<div class=\"tile-head\">";
                items += "<strong>";
                items += WebUiRu::Sockets::kTitlePrefix;
                items += String((unsigned)cfg.id);
                items += "</strong>";
                items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-enable\" data-id=\"";
                items += String((unsigned)cfg.id);
                items += "\" name=\"s";
                items += String((unsigned)cfg.id);
                items += "_en\"";
                if (cfg.enabled)
                    items += " checked";
                if (!can_edit)
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
                items += "</div>";
                items += String("<div class=\"form-row\"><label>") + WebUiRu::Sockets::kLabelName + "</label><input class=\"field name\" type=\"text\" name=\"s";
                items += String((unsigned)cfg.id);
                items += "_name\" value=\"";
                web.appendHtmlEscaped_(items, cfg.name);
                items += "\"";
                if (!can_edit)
                    items += " readonly";
                items += "></div>";
                items += String("<div class=\"form-row\" style=\"margin-top:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field mini\" name=\"s";
                items += String((unsigned)cfg.id);
                items += "_group\"";
                if (!can_edit || !has_groups)
                    items += " disabled";
                items += ">";
                items += web.groupOptionsHtml_(cfg.group_id, true, true, node_id);
                items += "</select></div>";
                items += "<div class=\"status-line\"><span class=\"status-dot ";
                items += on ? "status-on" : "status-off";
                items += "\"></span>";
                items += "<span class=\"status-text\">";
                items += on ? WebUiRu::Sockets::kText3 : WebUiRu::Sockets::kText4;
                items += "</span></div>";
                items += "<div class=\"form-grid\">";
                items += WebUiRu::Sockets::kText5;
                items += "<select class=\"field mini socket-select\" data-type=\"dinput\" data-selected=\"";
                if (cfg.button_port != SocketController::kInvalidPort)
                    items += String((unsigned)cfg.button_port);
                items += "\" name=\"s";
                items += String((unsigned)cfg.id);
                items += "_btn\"";
                if (!can_edit)
                    items += " disabled";
                items += "></select></div>";
                items += WebUiRu::Sockets::kText6;
                items += "<select class=\"field mini socket-select\" data-type=\"relay\" data-selected=\"";
                if (cfg.relay_port != SocketController::kInvalidPort)
                    items += String((unsigned)cfg.relay_port);
                items += "\" name=\"s";
                items += String((unsigned)cfg.id);
                items += "_relay\"";
                if (!can_edit)
                    items += " disabled";
                items += "></select></div>";
                items += WebUiRu::Sockets::kText7;
                items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-toggle\" data-id=\"";
                items += String((unsigned)cfg.id);
                items += "\"";
                if (on)
                    items += " checked";
                if (!cfg.enabled || !can_control)
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
                items += "</div>";
                items += "<input type=\"hidden\" name=\"s";
                items += String((unsigned)cfg.id);
                items += "_action\" value=\"\">";
                items += "</div></div>";
                ++rendered;
            }
            if (items.length() == 0)
                items = WebUiRu::Sockets::kText8;
            return items;
        
    }

String WebInterfaceControllersSocketsHelper::socketsDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"sockets-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = web.network()->stackOnlineDeviceCount();
        for (size_t i = 0; i < count; ++i)
        {
            StackDeviceRegistry::DeviceInfo device{};
            if (!web.network()->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
                continue;
            const uint32_t id = device.node_id;
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = device.name[0] ? String(device.name) : String();
            if (name.length() > 0)
                web.appendHtmlEscaped_(html, name.c_str());
            else
                html += web.stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

String WebInterfaceControllersSocketsHelper::stackSocketsStatusText_(const WebInterface &web, uint32_t node_id) {
            const auto *cache = web._stack_cache->socketsCache(node_id);
            if (!cache)
                return WebUiRu::kNoDataFromSlave;
            if (cache->pending &&
                (uint32_t)(millis() - cache->updated_ms) > 15000u)
                return WebUiRu::Sockets::kText10;
            if (cache->pending)
                return "";
            if (!cache->last_ok && cache->last_error.length())
            {
                String msg = WebUiRu::kErrorPrefix;
                msg += cache->last_error;
                return msg;
            }
            if (!cache->has_data)
                return WebUiRu::kNoDataFromSlave;
            return WebUiRu::kStatusOk;
        
    }

bool WebInterfaceControllersSocketsHelper::isStackSocketsView_(const WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

void WebInterfaceControllersSocketsHelper::handleStackSocketsToggle_(WebInterface &web, AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie) {
        if (!web._stack_master)
        {
            web.sendText_(request, 400, "text/plain", "Stack master missing", set_cookie);
            return;
        }
        const String id_str = web.paramValueAny_(request, "id");
        if (!id_str.length())
        {
            web.sendText_(request, 400, "text/plain", "Missing id", set_cookie);
            return;
        }
        const uint8_t id = (uint8_t)id_str.toInt();
        if (id == 0)
        {
            web.sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        String action = web.paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        const auto *cache = web._stack_cache ? web._stack_cache->socketsCache(node_id) : nullptr;
        const StackCache::StackSocketItem *item = nullptr;
        if (cache)
        {
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                if (cache->items[i].id == id)
                {
                    item = &cache->items[i];
                    break;
                }
            }
        }
    
        if (action == "state")
        {
            if (!cache || !cache->has_data ||
                (uint32_t)(millis() - cache->updated_ms) > 1500u)
            {
                web.requestStackSockets_(node_id);
            }
            if (cache && cache->pending)
            {
                web.sendText_(request, 200, "text/plain", "pending", set_cookie);
                return;
            }
            if (!item)
            {
                web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
                return;
            }
            web.sendText_(request, 200, "text/plain", (item->enabled && item->state) ? "on" : "off", set_cookie);
            return;
        }
    
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = web.nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        JsonArray items = params["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = id;
    
        bool desired_known = false;
        bool desired = false;
        if (action == "on" || action == "off")
        {
            desired = (action == "on");
            desired_known = true;
            o["state"] = desired;
        }
        else
        {
            o["toggle"] = true;
            if (item)
            {
                desired = !item->state;
                desired_known = true;
            }
        }
    
        char payload[160] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0 || !web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                               (const uint8_t *)payload, len))
        {
            web.sendText_(request, 400, "text/plain", "Send failed", set_cookie);
            return;
        }

        // Immediately schedule a fresh stack snapshot; the switch request itself returns simple ack.
        web.requestStackSockets_(node_id);
        (void)desired_known;
        (void)desired;
        web.sendText_(request, 200, "text/plain", "OK", set_cookie);
    }

void WebInterfaceControllersSocketsHelper::handleStackSocketsEnable_(WebInterface &web, AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie) {
        if (!web._stack_master || !web._stack_cache)
        {
            web.sendText_(request, 400, "text/plain", "Stack unavailable", set_cookie);
            return;
        }
        if (!web.webSessionIsAdmin_())
        {
            web.sendText_(request, 403, "text/plain", "Admin only", set_cookie);
            return;
        }
        const String id_str = web.paramValueAny_(request, "id");
        if (!id_str.length())
        {
            web.sendText_(request, 400, "text/plain", "Missing id", set_cookie);
            return;
        }
        const uint8_t id = (uint8_t)id_str.toInt();
        if (id == 0 || !web.webAclCanControlItem_(UsersRegistry::AclController::Sockets, id, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        const String enabled_str = web.paramValueAny_(request, "enabled");
        const bool enabled = (enabled_str == "1" || enabled_str == "true" || enabled_str == "on");
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = web.nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "set";
        JsonArray items = doc["params"]["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = id;
        o["enabled"] = enabled;
        char payload[160] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0 || !web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                                    (const uint8_t *)payload, len))
        {
            web.sendText_(request, 400, "text/plain", "Send failed", set_cookie);
            return;
        }
        auto *cache = web._stack_cache->socketsCache(node_id);
        if (cache && cache->items)
        {
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                if (cache->items[i].id == id)
                {
                    cache->items[i].enabled = enabled;
                    break;
                }
            }
            cache->updated_ms = millis();
        }
        web.requestStackSockets_(node_id);
        web.requestStackPorts_(node_id);
        String dbg = String("{\"ok\":true");
        dbg += ",\"id\":\"" + id_str + "\"";
        dbg += ",\"enabled\":\"" + enabled_str + "\"";
        dbg += ",\"parsed\":" + String(enabled ? "true" : "false");
        dbg += ",\"result\":\"" + String(enabled ? "1" : "0") + "\"";
        dbg += "}";
        web.sendText_(request, 200, "application/json", dbg, set_cookie);
    }

bool WebInterfaceControllersSocketsHelper::requestStackSockets_(WebInterface &web, uint32_t node_id) {
        return web._stack_cache && web._stack_cache->requestSockets(node_id);
    }

String WebInterfaceControllersSocketsHelper::socketPortOptionsJson_(const WebInterface &web, PortIO::PinType type) {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        uint8_t next_id[11] = {};
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = ActiveBoardProfile::PORTS[i];
            if (p.caps == Cap::None || p.type != type)
                continue;
            if (p.backend == PortIO::Backend::Extender)
            {
                if (!web._ext)
                    continue;
                const uint8_t dev = p.u.ext.dev;
                const auto *devs = web._ext->devs();
                if (!devs || dev >= web._ext->devCount())
                    continue;
                if (devs[dev].type != Extender::Type::MCP23017)
                    continue;
                if (!web._ext->isPresent(dev))
                    continue;
            }
            uint8_t ui_id = p.ui_id;
            if (ui_id == 0)
            {
                const uint8_t loc = web.locationIndex_(p.location);
                if (loc < 11)
                    ui_id = ++next_id[loc];
                else
                    ui_id = 0;
            }
            if (!first)
                out += ",";
            out += "{\"v\":";
            out += String((unsigned)i);
            out += ",\"l\":\"";
            web.appendPortLabel_(out, type, p, ui_id);
            out += "\"}";
            first = false;
        }
        out += "]";
        return out;
    }

String WebInterfaceControllersSocketsHelper::socketUsedPortsJson_(const WebInterface &web, PortIO::PinType type) {
        return web.globalUsedPortsJson_(type);
    }

size_t WebInterface::socketsLocalRenderCount_() const {
        return WebInterfaceControllersSocketsHelper::socketsLocalRenderCount_(*this);
    }

String WebInterface::listSocketsHtml_(uint8_t start_id, uint8_t end_id) {
        return WebInterfaceControllersSocketsHelper::listSocketsHtml_(*this, start_id, end_id);
    }

size_t WebInterface::stackSocketsVisibleCount_(uint32_t node_id) const {
        return WebInterfaceControllersSocketsHelper::stackSocketsVisibleCount_(*this, node_id);
    }

String WebInterface::listStackSocketsHtml_(uint32_t node_id, size_t offset, size_t limit) {
        return WebInterfaceControllersSocketsHelper::listStackSocketsHtml_(*this, node_id, offset, limit);
    }

String WebInterface::socketsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const {
        return WebInterfaceControllersSocketsHelper::socketsDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

String WebInterface::stackSocketsStatusText_(uint32_t node_id) const {
        return WebInterfaceControllersSocketsHelper::stackSocketsStatusText_(*this, node_id);
    }

bool WebInterface::isStackSocketsView_(uint32_t node_id) const {
        return WebInterfaceControllersSocketsHelper::isStackSocketsView_(*this, node_id);
    }

void WebInterface::handleStackSocketsToggle_(AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie) {
        WebInterfaceControllersSocketsHelper::handleStackSocketsToggle_(*this, request, node_id, set_cookie);
    }

void WebInterface::handleStackSocketsEnable_(AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie) {
        WebInterfaceControllersSocketsHelper::handleStackSocketsEnable_(*this, request, node_id, set_cookie);
    }

bool WebInterface::requestStackSockets_(uint32_t node_id) {
        return WebInterfaceControllersSocketsHelper::requestStackSockets_(*this, node_id);
    }

String WebInterface::socketPortOptionsJson_(PortIO::PinType type) const {
        return WebInterfaceControllersSocketsHelper::socketPortOptionsJson_(*this, type);
    }

String WebInterface::socketUsedPortsJson_(PortIO::PinType type) const {
        return WebInterfaceControllersSocketsHelper::socketUsedPortsJson_(*this, type);
    }

