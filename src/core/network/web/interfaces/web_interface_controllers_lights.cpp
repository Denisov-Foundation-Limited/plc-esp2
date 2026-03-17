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

size_t WebInterfaceControllersLightsHelper::lightsLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        SocketController &sockets = web._controllers->sockets();
        auto guard = sockets.lockGuard();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < SocketController::kLightCount; ++i)
        {
            const auto *cfg = sockets.lightConfigByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return SocketController::kLightCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > SocketController::kLightCount ? SocketController::kLightCount : count;
    }

String WebInterfaceControllersLightsHelper::lightsDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"lights-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = web._stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = web._stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = web._stack_master->nodeNameAt(i);
            if (name.length() > 0)
                web.appendHtmlEscaped_(html, name.c_str());
            else
                html += web.stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }

String WebInterfaceControllersLightsHelper::stackLightsStatusText_(const WebInterface &web, uint32_t node_id) {
            const auto *cache = web._stack_cache->lightsCache(node_id);
            if (!cache)
                return WebUiRu::kNoDataFromSlave;
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

bool WebInterfaceControllersLightsHelper::isStackLightsView_(const WebInterface &web, uint32_t node_id) {
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

void WebInterfaceControllersLightsHelper::handleStackLightsToggle_(WebInterface &web, AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie) {
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
        const auto *cache = web._stack_cache ? web._stack_cache->lightsCache(node_id) : nullptr;
        const StackCache::StackLightItem *item = nullptr;
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
                web.requestStackLights_(node_id);
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
        doc["action"] = "set_lights";
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

        // Immediately request a fresh snapshot; the switch request itself returns simple ack.
        web.requestStackLights_(node_id);
        (void)desired_known;
        (void)desired;
        web.sendText_(request, 200, "text/plain", "OK", set_cookie);
    }

bool WebInterfaceControllersLightsHelper::requestStackLights_(WebInterface &web, uint32_t node_id) {
        return web._stack_cache && web._stack_cache->requestLights(node_id);
    }

String WebInterfaceControllersLightsHelper::listLightsHtml_(WebInterface &web, uint8_t start_id, uint8_t end_id) {
        if (!web._controllers)
            return WebUiRu::Lights::kText;
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
        auto guard = sockets.lockGuard();
        bool tmp_state = false;
        auto appendRow = [&](const SocketController::LightConfig &cfg, bool enabled) {
            const bool can_edit = web.webSessionIsAdmin_();
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Lights, cfg.id);
            const bool has_groups = web.hasGroups_();
            const bool on = enabled && sockets.lightRelayState(cfg.id, tmp_state) ? tmp_state : false;
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
            items += "<path fill=\"currentColor\" d=\"M32 4c-9.9 0-18 8.1-18 18 0 7.1 4.1 13.2 10 16.2V50c0 2.2 1.8 4 4 4h8c2.2 0 4-1.8 4-4V38.2c5.9-3 10-9.1 10-16.2 0-9.9-8.1-18-18-18zm6 42H26v-4h12v4zm0-8H26v-4h12v4z\"/>";
            items += "</svg>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\">";
            items += "<strong>";
            items += WebUiRu::Lights::kTitlePrefix;
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
            items += String("<div class=\"form-row\"><label>") + WebUiRu::Lights::kLabelName + "</label><input class=\"field name\" type=\"text\" name=\"s";
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
            items += on ? WebUiRu::Lights::kText3 : WebUiRu::Lights::kText4;
            items += "</span>";
            items += "</div>";
            items += "<div class=\"form-grid\">";
            items += WebUiRu::Lights::kText5;
            items += "<select class=\"field mini socket-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.button_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_btn\"";
            if (!can_edit)
                items += " disabled";
            items += "></select></div>";
            items += WebUiRu::Lights::kText6;
            items += "<select class=\"field mini socket-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.relay_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_relay\"";
            if (!can_edit)
                items += " disabled";
            items += "></select></div>";
            items += WebUiRu::Lights::kText7;
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
    
        const size_t render_count = web.lightsLocalRenderCount_();
        const bool can_view_disabled = web.webSessionIsAdmin_();
        for (size_t i = 0; i < render_count; ++i)
        {
            const auto *cfg = sockets.lightConfigByIndex(i);
            if (!cfg)
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Lights, cfg->id))
                continue;
            if (cfg->id < start_id || cfg->id > end_id)
                continue;
            if (!can_view_disabled && !cfg->enabled)
                continue;
            appendRow(*cfg, cfg->enabled);
        }
        if (items.length() == 0)
            items = WebUiRu::Lights::kText8;
        return items;
    }

size_t WebInterfaceControllersLightsHelper::stackLightsVisibleCount_(const WebInterface &web, uint32_t node_id) {
            const auto *cache = web._stack_cache ? web._stack_cache->lightsCache(node_id) : nullptr;
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
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Lights, cfg.id, node_id))
                    continue;
                if (!can_view_disabled && !cfg.enabled)
                    continue;
                ++count;
            }
            return count;
        
    }

String WebInterfaceControllersLightsHelper::listStackLightsHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
            const auto *cache = web._stack_cache->lightsCache(node_id);
            if (!cache || !cache->has_data)
                return WebUiRu::Lights::kText9;
            if (cache->item_count == 0)
                return WebUiRu::Lights::kText8;
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
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Lights, cfg.id, node_id))
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
                const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Lights, cfg.id, node_id);
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
                items += "<path fill=\"currentColor\" d=\"M32 4c-9.9 0-18 8.1-18 18 0 7.1 4.1 13.2 10 16.2V50c0 2.2 1.8 4 4 4h8c2.2 0 4-1.8 4-4V38.2c5.9-3 10-9.1 10-16.2 0-9.9-8.1-18-18-18zm6 42H26v-4h12v4zm0-8H26v-4h12v4z\"/>";
                items += "</svg>";
                items += "</div>";
                items += "<div>";
                items += "<div class=\"tile-head\">";
                items += "<strong>";
                items += WebUiRu::Lights::kTitlePrefix;
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
                items += String("<div class=\"form-row\"><label>") + WebUiRu::Lights::kLabelName + "</label><input class=\"field name\" type=\"text\" name=\"s";
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
                items += on ? WebUiRu::Lights::kText3 : WebUiRu::Lights::kText4;
                items += "</span></div>";
                items += "<div class=\"form-grid\">";
                items += WebUiRu::Lights::kText5;
                items += "<select class=\"field mini socket-select\" data-type=\"dinput\" data-selected=\"";
                if (cfg.button_port != SocketController::kInvalidPort)
                    items += String((unsigned)cfg.button_port);
                items += "\" name=\"s";
                items += String((unsigned)cfg.id);
                items += "_btn\"";
                if (!can_edit)
                    items += " disabled";
                items += "></select></div>";
                items += WebUiRu::Lights::kText6;
                items += "<select class=\"field mini socket-select\" data-type=\"relay\" data-selected=\"";
                if (cfg.relay_port != SocketController::kInvalidPort)
                    items += String((unsigned)cfg.relay_port);
                items += "\" name=\"s";
                items += String((unsigned)cfg.id);
                items += "_relay\"";
                if (!can_edit)
                    items += " disabled";
                items += "></select></div>";
                items += WebUiRu::Lights::kText7;
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
                items = WebUiRu::Lights::kText8;
            return items;
        
    }

size_t WebInterface::lightsLocalRenderCount_() const {
        return WebInterfaceControllersLightsHelper::lightsLocalRenderCount_(*this);
    }

String WebInterface::lightsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const {
        return WebInterfaceControllersLightsHelper::lightsDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

String WebInterface::stackLightsStatusText_(uint32_t node_id) const {
        return WebInterfaceControllersLightsHelper::stackLightsStatusText_(*this, node_id);
    }

bool WebInterface::isStackLightsView_(uint32_t node_id) const {
        return WebInterfaceControllersLightsHelper::isStackLightsView_(*this, node_id);
    }

void WebInterface::handleStackLightsToggle_(AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie) {
        WebInterfaceControllersLightsHelper::handleStackLightsToggle_(*this, request, node_id, set_cookie);
    }

bool WebInterface::requestStackLights_(uint32_t node_id) {
        return WebInterfaceControllersLightsHelper::requestStackLights_(*this, node_id);
    }

String WebInterface::listLightsHtml_(uint8_t start_id, uint8_t end_id) {
        return WebInterfaceControllersLightsHelper::listLightsHtml_(*this, start_id, end_id);
    }

size_t WebInterface::stackLightsVisibleCount_(uint32_t node_id) const {
        return WebInterfaceControllersLightsHelper::stackLightsVisibleCount_(*this, node_id);
    }

String WebInterface::listStackLightsHtml_(uint32_t node_id, size_t offset, size_t limit) {
        return WebInterfaceControllersLightsHelper::listStackLightsHtml_(*this, node_id, offset, limit);
    }


