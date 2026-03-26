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

namespace
{
void loadLocalLightItems_(SocketController &sockets, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = sockets.lockGuard();
    bool tmp_state = false;
    for (size_t i = 0; i < count; ++i)
    {
        scratch.light_valid[i] = false;
        scratch.light_relay_on[i] = false;
        const auto *cfg = sockets.lightConfigByIndex(i);
        if (!cfg)
            continue;
        scratch.light_valid[i] = true;
        scratch.light_cfg[i] = *cfg;
        scratch.light_relay_on[i] = cfg->enabled && sockets.lightRelayState(cfg->id, tmp_state) ? tmp_state : false;
    }
}

bool stackLightsState_(const WebInterface &web, uint32_t node_id, StackUnitSnapshot::State &out)
{
    return web.network() && node_id != 0 && web.network()->stackIndexState(node_id, out);
}

bool stackLightsCacheState_(const WebInterface &web, uint32_t node_id, StackUnitSnapshot::CacheState &out)
{
    return web.network() && node_id != 0 && web.network()->stackIndexCacheState(node_id, out);
}

bool stackLightsRequestState_(const WebInterface &web, uint32_t node_id, StackUnitSnapshot::RequestState &out)
{
    return web.network() && node_id != 0 && web.network()->stackIndexRequestState(node_id, out);
}

bool requestNextStackLightsPage_(WebInterface &web, uint32_t node_id, uint16_t offset, uint16_t limit)
{
    if (!web.network() || node_id == 0 || limit == 0)
        return false;
    DynamicJsonDocument req(64);
    req["offset"] = offset;
    req["limit"] = limit;
    const bool sent = web.network()->stackRoute().sendRequest(node_id, "lights", "snapshot_req", &req,
                                                              StackRouteAdapter::Mode::Json, true);
    if (!sent)
        web.network()->clearStackPageRequest(StackUnitSnapshot::PageKind::Lights, node_id);
    return sent;
}

size_t stackLightsRenderCount_(const WebInterface &web, uint32_t node_id, uint8_t loaded_count, bool can_view_disabled)
{
    if (!can_view_disabled || !web.network())
        return loaded_count;
    size_t last_enabled_idx = SIZE_MAX;
    web.network()->forEachStackLight(node_id, loaded_count, [&](uint8_t index, const StackUnitSnapshot::SocketItem &item) {
        if (item.enabled)
            last_enabled_idx = index;
    });
    if (last_enabled_idx == SIZE_MAX)
        return loaded_count ? 1u : 0u;
    const size_t rc = last_enabled_idx + 2u;
    return rc > loaded_count ? loaded_count : rc;
}
}

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
        if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
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

String WebInterfaceControllersLightsHelper::stackLightsStatusText_(const WebInterface &web, uint32_t node_id) {
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::RequestState request{};
            if (!stackLightsState_(web, node_id, snapshot))
                return WebUiRu::kNoDataFromSlave;
            if (stackLightsRequestState_(web, node_id, request) &&
                request.pending &&
                (uint32_t)(millis() - request.started_ms) > 15000u)
                return WebUiRu::Sockets::kText10;
            if (request.pending)
                return "";
            if (snapshot.updated_ms == 0)
                return WebUiRu::kNoDataFromSlave;
            return WebUiRu::kStatusOk;

    }

bool WebInterfaceControllersLightsHelper::isStackLightsView_(const WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

void WebInterfaceControllersLightsHelper::handleStackLightsToggle_(WebInterface &web, AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie) {
        if (!web.network())
        {
            web.sendText_(request, 400, "text/plain", "Stack unavailable", set_cookie);
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
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        StackUnitSnapshot::RequestState request_state{};
        const bool has_snapshot = stackLightsState_(web, node_id, snapshot);
        const bool has_cache = stackLightsCacheState_(web, node_id, cache);
        const bool has_request = stackLightsRequestState_(web, node_id, request_state);
        StackUnitSnapshot::SocketItem item{};
        const bool has_item = has_snapshot && web.network()->stackIndexLightById(node_id, id, item);
    
        if (action == "state")
        {
            const bool stale = !has_snapshot || snapshot.updated_ms == 0 ||
                (uint32_t)(millis() - snapshot.updated_ms) > 1500u;
            const bool partial = has_snapshot && has_cache && snapshot.lights_enabled > cache.light_count;
            if (stale || partial)
            {
                web.requestStackLights_(node_id);
                web.sendText_(request, 200, "text/plain", "pending", set_cookie);
                return;
            }
            if (has_request && request_state.pending)
            {
                web.sendText_(request, 200, "text/plain", "pending", set_cookie);
                return;
            }
            if (!has_item)
            {
                web.sendText_(request, 200, "text/plain", "unknown", set_cookie);
                return;
            }
            web.sendText_(request, 200, "text/plain", (item.enabled && item.state) ? "on" : "off", set_cookie);
            return;
        }
    
        StaticJsonDocument<224> doc;
        doc["source"] = "localweb";
        if (const auto *u = web.sessionUser_())
            doc["source_user"] = u->username;
        JsonArray items = doc["items"].to<JsonArray>();
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
            if (has_item)
            {
                desired = !item.state;
                desired_known = true;
            }
        }

        if (!web.network()->stackRoute().sendEvent(node_id, "sockets", "set_lights", &doc, StackRouteAdapter::Mode::Json))
        {
            web.sendText_(request, 400, "text/plain", "Send failed", set_cookie);
            return;
        }

        web.requestStackLights_(node_id);
        web.requestStackIndexState_(node_id);
        (void)desired_known;
        (void)desired;
        web.sendText_(request, 200, "text/plain", "OK", set_cookie);
    }

void WebInterfaceControllersLightsHelper::handleStackLightsEnable_(WebInterface &web, AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie) {
        if (!web.network())
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
        if (id == 0 || !web.webAclCanControlItem_(UsersRegistry::AclController::Lights, id, node_id))
        {
            web.sendText_(request, 403, "text/plain", "ACL deny", set_cookie);
            return;
        }
        const String enabled_str = web.paramValueAny_(request, "enabled");
        const bool enabled = (enabled_str == "1" || enabled_str == "true" || enabled_str == "on");
        StaticJsonDocument<192> doc;
        doc["source"] = "localweb";
        if (const auto *u = web.sessionUser_())
            doc["source_user"] = u->username;
        JsonArray items = doc["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = id;
        o["enabled"] = enabled;
        if (!web.network()->stackRoute().sendEvent(node_id, "sockets", "set_lights", &doc, StackRouteAdapter::Mode::Json))
        {
            web.sendText_(request, 400, "text/plain", "Send failed", set_cookie);
            return;
        }
        web.requestStackLights_(node_id);
        web.requestStackIndexState_(node_id);
        web.requestStackPorts_(node_id);
        String dbg = String("{\"ok\":true");
        dbg += ",\"id\":\"" + id_str + "\"";
        dbg += ",\"enabled\":\"" + enabled_str + "\"";
        dbg += ",\"parsed\":" + String(enabled ? "true" : "false");
        dbg += ",\"result\":\"" + String(enabled ? "1" : "0") + "\"";
        dbg += "}";
        web.sendText_(request, 200, "application/json", dbg, set_cookie);
    }

bool WebInterfaceControllersLightsHelper::requestStackLights_(WebInterface &web, uint32_t node_id) {
        if (!web.network() || node_id == 0)
            return false;
        const uint32_t now = millis();
        StackUnitSnapshot::State snapshot{};
        StackUnitSnapshot::CacheState cache{};
        StackUnitSnapshot::RequestState request{};
        const bool has_snapshot = web.network()->stackIndexState(node_id, snapshot);
        const bool has_cache = web.network()->stackIndexCacheState(node_id, cache);
        const bool has_request = web.network()->stackIndexRequestState(node_id, request);
        if (!has_snapshot || snapshot.updated_ms == 0 ||
            (uint32_t)(now - snapshot.updated_ms) > 5000u)
        {
            const bool refresh = web.requestStackIndexState_(node_id);
            DynamicJsonDocument req(64);
            req["offset"] = 0;
            req["limit"] = StackUnitSnapshot::kPageSize;
            const bool lights_req = web.network()->stackRoute().sendRequest(node_id, "lights", "snapshot_req", &req,
                                                                            StackRouteAdapter::Mode::Json, true);
            return refresh || lights_req;
        }
        if (has_request && request.pending && (uint32_t)(now - request.started_ms) < 1500u)
            return true;
        if (has_cache && snapshot.lights_enabled > cache.light_count)
        {
            if (!web.network()->prepareStackPageRequest(StackUnitSnapshot::PageKind::Lights, node_id, now,
                                                        cache.light_count, 4000u))
                return true;
            return requestNextStackLightsPage_(web, node_id, cache.light_count, StackUnitSnapshot::kPageSize);
        }
        return true;
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
        auto scratch_guard = web.scratchLockGuard_();
        WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
        if (!scratch)
            return WebUiRu::Lights::kText;
        SocketController &sockets = web._controllers->sockets();
        loadLocalLightItems_(sockets, *scratch, SocketController::kLightCount);
        auto appendRow = [&](const SocketController::LightConfig &cfg, bool enabled, bool on) {
            const bool can_edit = web.webSessionIsAdmin_();
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Lights, cfg.id);
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
            if (!scratch->light_valid[i])
                continue;
            const auto &cfg = scratch->light_cfg[i];
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Lights, cfg.id))
                continue;
            if (cfg.id < start_id || cfg.id > end_id)
                continue;
            if (!can_view_disabled && !cfg.enabled)
                continue;
            appendRow(cfg, cfg.enabled, scratch->light_relay_on[i]);
        }
        if (items.length() == 0)
            items = WebUiRu::Lights::kText8;
        return items;
    }

size_t WebInterfaceControllersLightsHelper::stackLightsVisibleCount_(const WebInterface &web, uint32_t node_id) {
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::CacheState cache{};
            if (!stackLightsState_(web, node_id, snapshot) || !stackLightsCacheState_(web, node_id, cache))
                return 0;
            if (cache.light_count == 0)
                return 0;
            const bool can_view_disabled = web.webSessionIsAdmin_();
            const size_t render_count = stackLightsRenderCount_(web, node_id, cache.light_count, can_view_disabled);
            size_t count = 0;
            web.network()->forEachStackLight(node_id, (uint8_t)render_count, [&](uint8_t, const StackUnitSnapshot::SocketItem &cfg) {
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Lights, cfg.id, snapshot.node_id))
                    return;
                if (!can_view_disabled && !cfg.enabled)
                    return;
                ++count;
            });
            return count;
        
    }

String WebInterfaceControllersLightsHelper::listStackLightsHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
            StackUnitSnapshot::State snapshot{};
            StackUnitSnapshot::CacheState cache{};
            if (!stackLightsState_(web, node_id, snapshot) || !stackLightsCacheState_(web, node_id, cache))
                return WebUiRu::Lights::kText9;
            if (cache.light_count == 0)
                return WebUiRu::Lights::kText8;
            String items;
            const size_t page_limit = (limit == 0) ? 1u : limit;
            size_t reserve = 2048u + page_limit * 420u;
            if (reserve < 8192u)
                reserve = 8192u;
            items.reserve(reserve);
            const bool can_view_disabled = web.webSessionIsAdmin_();
            const size_t render_count = stackLightsRenderCount_(web, node_id, cache.light_count, can_view_disabled);
            size_t rendered = 0;
            size_t visible_idx = 0;
            web.network()->forEachStackLight(node_id, (uint8_t)render_count, [&](uint8_t, const StackUnitSnapshot::SocketItem &cfg) {
                if (rendered >= page_limit)
                    return;
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Lights, cfg.id, node_id))
                    return;
                if (!can_view_disabled && !cfg.enabled)
                    return;
                if (visible_idx < offset)
                {
                    ++visible_idx;
                    return;
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
            });
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

void WebInterface::handleStackLightsEnable_(AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie) {
        WebInterfaceControllersLightsHelper::handleStackLightsEnable_(*this, request, node_id, set_cookie);
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


