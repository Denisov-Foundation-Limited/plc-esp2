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

size_t WebInterfaceControllersTanksHelper::tanksLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        TankController &tanks = web._controllers->tanks();
        auto guard = tanks.lockGuard();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = tanks.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return TankController::kTankCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > TankController::kTankCount ? TankController::kTankCount : count;
    }

String WebInterfaceControllersTanksHelper::tanksDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"tanks-device\" class=\"field mini\">";
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

String WebInterfaceControllersTanksHelper::stackTanksStatusText_(const WebInterface &web, uint32_t node_id) {
        const auto *cache = web._stack_cache->tanksCache(node_id);
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

bool WebInterfaceControllersTanksHelper::isStackTanksView_(const WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

bool WebInterfaceControllersTanksHelper::requestStackTanks_(WebInterface &web, uint32_t node_id) {
        return web._stack_cache && web._stack_cache->requestTanks(node_id);
    }

size_t WebInterfaceControllersTanksHelper::stackTanksVisibleCount_(const WebInterface &web, uint32_t node_id) {
        const auto *cache = web._stack_cache ? web._stack_cache->tanksCache(node_id) : nullptr;
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
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Tanks, cfg.id, node_id))
                continue;
            if (!can_view_disabled && !cfg.enabled)
                continue;
            ++count;
        }
        return count;
    }

String WebInterfaceControllersTanksHelper::listStackTanksHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
        const auto *cache = web._stack_cache ? web._stack_cache->tanksCache(node_id) : nullptr;
        if (!cache || !cache->has_data)
            return WebUiRu::Tanks::kText;
        if (cache->item_count == 0)
            return WebUiRu::Tanks::kText2;
    
        String items;
        const size_t page_limit = (limit == 0) ? 1u : limit;
        size_t reserve = 2048u + page_limit * 520u;
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
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Tanks, cfg.id, node_id))
                continue;
            if (!can_view_disabled && !cfg.enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Tanks, cfg.id, node_id);
            const bool can_admin = web.webSessionIsAdmin_();
            const char *level = "0%";
            const char *level_class = "level-empty";
            unsigned level_pct = 0;
            if (cfg.level_full)
            {
                level = "99%";
                level_class = "level-full";
                level_pct = 99;
            }
            else if (cfg.level_mid)
            {
                level = "66%";
                level_class = "level-mid";
                level_pct = 66;
            }
            else if (cfg.level_low)
            {
                level = "33%";
                level_class = "level-low";
                level_pct = 33;
            }
    
            const bool has_groups = web.hasGroups_(node_id);
            items += "<div class=\"tile js-group-item";
            if (!cfg.enabled)
                items += " disabled";
            items += "\" data-group-id=\"";
            items += String((unsigned)cfg.group_id);
            items += "\"";
            items += web.groupVisibilityStyleAttr_(cfg.group_id, node_id);
            items += "\">";
            items += "<div>";
            items += "<div class=\"tank-visual\">";
            items += "<div class=\"tank-fill ";
            items += level_class;
            items += "\" style=\"height:";
            items += String(level_pct);
            items += "%\"></div>";
            items += "<div class=\"tank-label\">";
            items += level;
            items += "</div></div>";
            items += "<div class=\"status-line\">";
            items += "<span class=\"badge\">ID ";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "</div></div>";
            items += "<div>";
            items += "<div class=\"tile-head\">";
            items += "<strong>";
            items += WebUiRu::Tanks::kTitlePrefix;
            items += String((unsigned)cfg.id);
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (cfg.enabled)
                items += " checked";
                if (!(can_admin && can_control))
                    items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "</div>";
            items += "<div class=\"status-row\">";
            items += "<span class=\"tank-status-dot ";
            items += (cfg.pump_on ? "tank-status-on" : "tank-status-off");
            items += WebUiRu::Tanks::kText11;
            items += "<span class=\"tank-status-dot ";
            items += (cfg.valve_on ? "tank-status-on" : "tank-status-off");
            items += WebUiRu::Tanks::kText12;
            items += "<span class=\"tank-status-dot ";
            items += (cfg.alarm_on ? "tank-status-bad" : "tank-status-off");
            items += WebUiRu::Tanks::kText14;
            items += "</div>";
            items += "<input class=\"field name\" type=\"text\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            if (cfg.name[0])
                web.appendHtmlEscaped_(items, cfg.name);
            else
                items += WebUiRu::Tanks::kText3;
            items += "\"";
            if (!(can_admin && can_control))
                items += " readonly";
            items += ">";
            items += String("<div class=\"form-row full\" style=\"margin-top:8px;margin-bottom:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_group\"";
            if (!(can_admin && can_control) || !has_groups)
                items += " disabled";
            items += ">";
            items += web.groupOptionsHtml_(cfg.group_id, true, true, node_id);
            items += "</select></div>";
            items += "<div class=\"form-grid\">";
            items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData;
            if (cfg.low != TankController::kInvalidPort)
                items += String((unsigned)cfg.low);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_low\"";
            if (!(can_admin && can_control))
                items += " disabled";
            items += "></select></div>";
            items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData2;
            if (cfg.mid != TankController::kInvalidPort)
                items += String((unsigned)cfg.mid);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_mid\"";
            if (!(can_admin && can_control))
                items += " disabled";
            items += "></select></div>";
            items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData3;
            if (cfg.full != TankController::kInvalidPort)
                items += String((unsigned)cfg.full);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_full\"";
            if (!(can_admin && can_control))
                items += " disabled";
            items += "></select></div>";
            items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData4;
            if (cfg.valve != TankController::kInvalidPort)
                items += String((unsigned)cfg.valve);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_valve\"";
            if (!(can_admin && can_control))
                items += " disabled";
            items += "></select></div>";
            items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData5;
            if (cfg.pump != TankController::kInvalidPort)
                items += String((unsigned)cfg.pump);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_pump\"";
            if (!(can_admin && can_control))
                items += " disabled";
            items += "></select></div>";
            items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData6;
            if (cfg.alarm != TankController::kInvalidPort)
                items += String((unsigned)cfg.alarm);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_alarm\"";
            if (!(can_admin && can_control))
                items += " disabled";
            items += "></select></div>";
            items += "</div>";
            if (can_control)
            {
                items += WebUiRu::Tanks::kInputTypeCheckboxClassTankPowerData;
                items += String((unsigned)cfg.id);
                items += "_power\"";
                if (cfg.power_on)
                    items += " checked";
                if (!cfg.enabled)
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label><input type=\"hidden\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_power\" value=\"";
                items += cfg.power_on ? "on" : "off";
                items += "\"></div>";
            }
            items += "</div></div>";
            ++rendered;
        }
        if (items.length() == 0)
            items = WebUiRu::Tanks::kText2;
        return items;
    }

String WebInterfaceControllersTanksHelper::listTanksHtml_(WebInterface &web, size_t offset, size_t limit) {
        if (!web._controllers)
            return WebUiRu::Tanks::kText8;
        String items;
        items.reserve(16384);
        TankController &tanks = web._controllers->tanks();
        auto guard = tanks.lockGuard();
    
        auto appendRow = [&](const TankController::TankConfig &cfg, const TankController::TankState &st,
                             bool enabled) {
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Tanks, cfg.id);
            const bool can_admin = web.webSessionIsAdmin_();
            const bool has_groups = web.hasGroups_();
            const char *level = "0%";
            const char *level_class = "level-empty";
            unsigned level_pct = 0;
            if (st.level_full)
            {
                level = "99%";
                level_class = "level-full";
                level_pct = 99;
            }
            else if (st.level_mid)
            {
                level = "66%";
                level_class = "level-mid";
                level_pct = 66;
            }
            else if (st.level_low)
            {
                level = "33%";
                level_class = "level-low";
                level_pct = 33;
            }
    
            items += "<div class=\"tile js-group-item";
            if (!enabled)
                items += " disabled";
                items += "\" data-group-id=\"";
                items += String((unsigned)cfg.group_id);
                items += "\"";
                items += web.groupVisibilityStyleAttr_(cfg.group_id);
                items += ">";
            items += "<div>";
            items += "<div class=\"tank-visual\">";
            items += "<div class=\"tank-fill ";
            items += level_class;
            items += "\" style=\"height:";
            items += String(level_pct);
            items += "%\"></div>";
            items += "<div class=\"tank-label\">";
            items += level;
            items += "</div></div>";
            items += "<div class=\"status-line\">";
            items += "<span class=\"badge\">ID ";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<span class=\"badge\">";
            items += enabled ? WebUiRu::Tanks::kText9 : WebUiRu::Tanks::kText10;
            items += "</span>";
            items += "</div></div>";
            items += "<div>";
            items += "<div class=\"tile-head\">";
            items += "<strong>";
            items += WebUiRu::Tanks::kTitlePrefix;
            items += String((unsigned)cfg.id);
            items += "</strong>";
            if (can_control)
            {
                items += "<label class=\"switch\"><input type=\"checkbox\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_en\"";
                if (enabled)
                    items += " checked";
                if (!can_admin)
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            }
            items += "</div>";
            items += "<div class=\"status-row\">";
            items += "<span class=\"tank-status-dot ";
            items += (st.pump_on ? "tank-status-on" : "tank-status-off");
            items += WebUiRu::Tanks::kText11;
            items += "<span class=\"tank-status-dot ";
            items += (st.valve_on ? "tank-status-on" : "tank-status-off");
            items += WebUiRu::Tanks::kText12;
            items += "<span class=\"tank-status-dot ";
            items += (st.alarm_on ? "tank-status-bad" : "tank-status-off");
            items += WebUiRu::Tanks::kText14;
            items += "</div>";
            if (can_control)
            {
                items += String("<div class=\"form-row\"><label>") + WebUiRu::Tanks::kLabelName + "</label><input class=\"field name\" type=\"text\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_name\" value=\"";
                web.appendHtmlEscaped_(items, cfg.name.c_str());
                items += "\"";
                if (!can_admin)
                    items += " readonly";
                items += "></div>";
                items += String("<div class=\"form-row full\" style=\"margin-top:8px;margin-bottom:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_group\"";
                if (!can_admin || !has_groups)
                    items += " disabled";
                items += ">";
                items += web.groupOptionsHtml_(cfg.group_id, true, true);
                items += "</select></div>";
                items += "<div class=\"form-grid\">";
                items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData;
                if (cfg.level_low != TankController::kInvalidPort)
                    items += String((unsigned)cfg.level_low);
                items += "\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_low\"";
                if (!can_admin)
                    items += " disabled";
                items += "></select></div>";
                items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData2;
                if (cfg.level_mid != TankController::kInvalidPort)
                    items += String((unsigned)cfg.level_mid);
                items += "\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_mid\"";
                if (!can_admin)
                    items += " disabled";
                items += "></select></div>";
                items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData3;
                if (cfg.level_full != TankController::kInvalidPort)
                    items += String((unsigned)cfg.level_full);
                items += "\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_full\"";
                if (!can_admin)
                    items += " disabled";
                items += "></select></div>";
                items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData4;
                if (cfg.relay_valve != TankController::kInvalidPort)
                    items += String((unsigned)cfg.relay_valve);
                items += "\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_valve\"";
                if (!can_admin)
                    items += " disabled";
                items += "></select></div>";
                items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData5;
                if (cfg.relay_pump != TankController::kInvalidPort)
                    items += String((unsigned)cfg.relay_pump);
                items += "\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_pump\"";
                if (!can_admin)
                    items += " disabled";
                items += "></select></div>";
                items += WebUiRu::Tanks::kSelectClassFieldMiniTankSelectData6;
                if (cfg.relay_alarm != TankController::kInvalidPort)
                    items += String((unsigned)cfg.relay_alarm);
                items += "\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_alarm\"";
                if (!can_admin)
                    items += " disabled";
                items += "></select></div>";
                items += "<div>";
                items += WebUiRu::Tanks::kInputTypeCheckboxClassTankPowerData2;
                items += String((unsigned)cfg.id);
                items += "_power\"";
                if (cfg.power_on)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label><input type=\"hidden\" name=\"k";
                items += String((unsigned)cfg.id);
                items += "_power\" value=\"";
                items += cfg.power_on ? "on" : "off";
                items += "\"></div>";
            }
            items += "</div>";
            items += "</div></div></div>";
        };
    
        const size_t render_count = web.tanksLocalRenderCount_();
        const size_t page_limit = (limit == 0) ? 1u : limit;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t rendered = 0;
        size_t visible_idx = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            if (rendered >= page_limit)
                break;
            const auto *cfg = tanks.configByIndex(i);
            const auto *st = tanks.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Tanks, cfg->id))
                continue;
            if (!can_view_disabled && !cfg->enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            appendRow(*cfg, *st, cfg->enabled);
            ++rendered;
        }
        if (items.length() == 0)
            items = WebUiRu::Tanks::kText13;
        return items;
    }

String WebInterfaceControllersTanksHelper::listTanksHtml_(WebInterface &web) {
        return listTanksHtml_(web, 0u, SIZE_MAX);
    }

String WebInterfaceControllersTanksHelper::tankPortOptionsJson_(const WebInterface &web, PortIO::PinType type) {
        return web.socketPortOptionsJson_(type);
    }

String WebInterfaceControllersTanksHelper::tankUsedPortsJson_(const WebInterface &web, PortIO::PinType type) {
        return web.globalUsedPortsJson_(type);
    }

size_t WebInterface::tanksLocalRenderCount_() const {
        return WebInterfaceControllersTanksHelper::tanksLocalRenderCount_(*this);
    }

String WebInterface::tanksDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const {
        return WebInterfaceControllersTanksHelper::tanksDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

String WebInterface::stackTanksStatusText_(uint32_t node_id) const {
        return WebInterfaceControllersTanksHelper::stackTanksStatusText_(*this, node_id);
    }

bool WebInterface::isStackTanksView_(uint32_t node_id) const {
        return WebInterfaceControllersTanksHelper::isStackTanksView_(*this, node_id);
    }

bool WebInterface::requestStackTanks_(uint32_t node_id) {
        return WebInterfaceControllersTanksHelper::requestStackTanks_(*this, node_id);
    }

size_t WebInterface::stackTanksVisibleCount_(uint32_t node_id) const {
        return WebInterfaceControllersTanksHelper::stackTanksVisibleCount_(*this, node_id);
    }

String WebInterface::listStackTanksHtml_(uint32_t node_id, size_t offset, size_t limit) {
        return WebInterfaceControllersTanksHelper::listStackTanksHtml_(*this, node_id, offset, limit);
    }

String WebInterface::listTanksHtml_() {
        return WebInterfaceControllersTanksHelper::listTanksHtml_(*this);
    }

String WebInterface::listTanksHtml_(size_t offset, size_t limit) {
        return WebInterfaceControllersTanksHelper::listTanksHtml_(*this, offset, limit);
    }

String WebInterface::tankPortOptionsJson_(PortIO::PinType type) const {
        return WebInterfaceControllersTanksHelper::tankPortOptionsJson_(*this, type);
    }

String WebInterface::tankUsedPortsJson_(PortIO::PinType type) const {
        return WebInterfaceControllersTanksHelper::tankUsedPortsJson_(*this, type);
    }


