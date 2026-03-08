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

size_t WebInterfaceControllersSepticHelper::septicLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        SepticController &septic = web._controllers->septic();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < SepticController::kSepticCount; ++i)
        {
            const auto *cfg = septic.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return SepticController::kSepticCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > SepticController::kSepticCount ? SepticController::kSepticCount : count;
    }

String WebInterfaceControllersSepticHelper::septicDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"septic-device\" class=\"field mini\">";
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

String WebInterfaceControllersSepticHelper::stackSepticStatusText_(const WebInterface &web, uint32_t node_id) {
            const auto *cache = web._stack_cache->septicCache(node_id);
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

bool WebInterfaceControllersSepticHelper::isStackSepticView_(const WebInterface &web, uint32_t node_id) {
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

bool WebInterfaceControllersSepticHelper::requestStackSeptic_(WebInterface &web, uint32_t node_id) {
        return web._stack_cache && web._stack_cache->requestSeptic(node_id);
    }

size_t WebInterfaceControllersSepticHelper::stackSepticVisibleCount_(const WebInterface &web, uint32_t node_id) {
        const auto *cache = web._stack_cache ? web._stack_cache->septicCache(node_id) : nullptr;
        if (!cache || !cache->has_data || !cache->items)
            return 0;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t count = 0;
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            if (cache->items[i].enabled)
                last_enabled_idx = i;
        }
        size_t render_count = cache->item_count;
        if (last_enabled_idx == SIZE_MAX)
            render_count = cache->item_count ? 1u : 0u;
        else
        {
            const size_t rc = last_enabled_idx + 2u;
            render_count = rc > cache->item_count ? cache->item_count : rc;
        }
        for (size_t i = 0; i < render_count; ++i)
        {
            const auto &cfg = cache->items[i];
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Septic, cfg.id, node_id))
                continue;
            if (!can_view_disabled && !cfg.enabled)
                continue;
            ++count;
        }
        return count;
    }

String WebInterfaceControllersSepticHelper::listStackSepticHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
            const auto *cache = web._stack_cache->septicCache(node_id);
            if (!cache || !cache->has_data)
                return WebUiRu::Septic::kText;
            if (cache->item_count == 0)
                return WebUiRu::Septic::kText2;
            String items;
            const size_t page_limit = (limit == 0) ? 1u : limit;
            size_t reserve = 1024u + page_limit * 480u;
            if (reserve < 4096u)
                reserve = 4096u;
            items.reserve(reserve);
            const bool can_view_disabled = web.webSessionIsAdmin_();
            size_t render_count = cache->item_count;
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
                const size_t count = last_enabled_idx + 2u;
                render_count = count > cache->item_count ? cache->item_count : count;
            }
            size_t rendered = 0;
            size_t visible_idx = 0;
            for (size_t i = 0; i < render_count; ++i)
            {
                if (rendered >= page_limit)
                    break;
                const auto &cfg = cache->items[i];
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Septic, cfg.id, node_id))
                    continue;
                if (!can_view_disabled && !cfg.enabled)
                    continue;
                if (visible_idx < offset)
                {
                    ++visible_idx;
                    continue;
                }
                ++visible_idx;
                const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Septic, cfg.id, node_id);
                const bool warn = cfg.warning;
                const bool alarm = cfg.alarm;
                const bool relay_warn = cfg.monitor && warn;
                const bool relay_alarm = cfg.monitor && alarm;
                const char *water_class = "water-low";
                const char *water_level = "20%";
                const char *water_label = WebUiRu::Septic::kText20;
                if (alarm)
                {
                    water_class = "water-alarm";
                    water_level = "100%";
                    water_label = WebUiRu::Septic::kText100;
                }
                else if (warn)
                {
                    water_class = "water-warn";
                    water_level = "80%";
                    water_label = WebUiRu::Septic::kText80;
                }
                const bool has_groups = web.hasGroups_(node_id);
                items += "<div class=\"tile js-group-item";
                if (!cfg.enabled)
                    items += " disabled";
                items += "\" data-group-id=\"";
                items += String((unsigned)cfg.group_id);
                items += "\"";
                items += web.groupVisibilityStyleAttr_(cfg.group_id, node_id);
                items += "\"><div class=\"septic-visual\"><div class=\"liquid ";
                items += water_class;
                items += "\" style=\"height:";
                items += water_level;
                items += ";\"></div><div class=\"level-label\">";
                items += water_label;
                items += WebUiRu::Septic::kNum;
                items += String((unsigned)cfg.id);
                items += "</strong>";
                if (!cfg.enabled)
                    items += WebUiRu::Septic::kText3;
                if (can_control)
                {
                    items += "</div><label class=\"switch\"><input type=\"checkbox\" name=\"sep";
                    items += String((unsigned)cfg.id);
                    items += "_en\"";
                    if (cfg.enabled)
                        items += " checked";
                    items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
                }
                items += "</div></div>";
                if (can_control)
                {
                    items += "<input class=\"field name\" type=\"text\" name=\"sep";
                    items += String((unsigned)cfg.id);
                    items += "_name\" value=\"";
                    if (cfg.name[0])
                        web.appendHtmlEscaped_(items, cfg.name);
                    items += "\">";
                    items += String("<div class=\"form-row\" style=\"margin-top:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field mini\" name=\"sep";
                    items += String((unsigned)cfg.id);
                    items += "_group\"";
                    if (!has_groups)
                        items += " disabled";
                    items += ">";
                    items += web.groupOptionsHtml_(cfg.group_id, true, true, node_id);
                    items += "</select></div>";
                    items += WebUiRu::Septic::kSelectClassFieldMiniSepticSelectData;
                    if (cfg.warning_port != SepticController::kInvalidPort)
                        items += String((unsigned)cfg.warning_port);
                    items += "\" name=\"sep";
                    items += String((unsigned)cfg.id);
                    items += WebUiRu::Septic::kWarnSelectClassFieldMiniSepticSelect;
                    if (cfg.alarm_port != SepticController::kInvalidPort)
                        items += String((unsigned)cfg.alarm_port);
                    items += "\" name=\"sep";
                    items += String((unsigned)cfg.id);
                    items += WebUiRu::Septic::kAlarmSelectClassFieldMiniSepticSelect;
                    if (cfg.relay_warning != SepticController::kInvalidPort)
                        items += String((unsigned)cfg.relay_warning);
                    items += "\" name=\"sep";
                    items += String((unsigned)cfg.id);
                    items += WebUiRu::Septic::kRelayWarnSelectClassFieldMiniSeptic;
                    if (cfg.relay_alarm != SepticController::kInvalidPort)
                        items += String((unsigned)cfg.relay_alarm);
                    items += "\" name=\"sep";
                    items += String((unsigned)cfg.id);
                    items += "_relay_alarm\"></select></div>";
                }
                items += "<div class=\"status-grid\"><div class=\"status-line\"><span class=\"status-dot ";
                items += warn ? "status-on" : "status-off";
                items += WebUiRu::Septic::kSpanClassStatusDot;
                items += alarm ? "status-on" : "status-off";
                items += WebUiRu::Septic::kSpanClassStatusDot2;
                items += relay_warn ? "status-on" : "status-off";
                items += WebUiRu::Septic::kSpanClassStatusDot3;
                items += relay_alarm ? "status-on" : "status-off";
                if (can_control)
                {
                    items += WebUiRu::Septic::kInputTypeCheckboxClassSepticMonitorData2;
                    items += String((unsigned)cfg.id);
                    items += "_mon\"";
                    if (cfg.monitor)
                        items += " checked";
                    if (!cfg.enabled)
                        items += " disabled";
                    items += "><span class=\"track\"><span class=\"knob\"></span></span></label><input type=\"hidden\" name=\"sep";
                    items += String((unsigned)cfg.id);
                    items += "_mon\" value=\"";
                    items += cfg.monitor ? "on" : "off";
                    items += "\"></div></div></div>";
                }
                else
                {
                    items += "</div></div></div>";
                }
                ++rendered;
            }
            if (items.length() == 0)
                items = WebUiRu::Septic::kText2;
            return items;
        
    }

String WebInterfaceControllersSepticHelper::listSepticHtml_(WebInterface &web, size_t offset, size_t limit) {
        if (!web._controllers)
            return WebUiRu::Septic::kText9;
        String items;
        items.reserve(2048);
        SepticController &septic = web._controllers->septic();
        const size_t render_count = web.septicLocalRenderCount_();
        const size_t page_limit = (limit == 0) ? 1u : limit;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t rendered = 0;
        size_t visible_idx = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            if (rendered >= page_limit)
                break;
            const auto *cfg = septic.configByIndex(i);
            const auto *st = septic.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Septic, cfg->id))
                continue;
            if (!can_view_disabled && !cfg->enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Septic, cfg->id);
            const bool has_groups = web.hasGroups_();
            const bool warn = st->warning;
            const bool alarm = st->alarm;
            const bool relay_warn = st->relay_warning;
            const bool relay_alarm = st->relay_alarm;
            const char *water_class = "water-low";
            const char *water_level = "20%";
            const char *water_label = WebUiRu::Septic::kText20;
            if (alarm)
            {
                water_class = "water-alarm";
                water_level = "100%";
                water_label = WebUiRu::Septic::kText100;
            }
            else if (warn)
            {
                water_class = "water-warn";
                water_level = "80%";
                water_label = WebUiRu::Septic::kText80;
            }
            items += "<div class=\"tile js-group-item";
            if (!cfg->enabled)
                items += " disabled";
                items += "\" data-group-id=\"";
                items += String((unsigned)cfg->group_id);
                items += "\"";
                items += web.groupVisibilityStyleAttr_(cfg->group_id);
                items += "><div class=\"septic-visual\"><div class=\"liquid ";
            items += water_class;
            items += "\" style=\"height:";
            items += water_level;
            items += ";\"></div><div class=\"level-label\">";
            items += water_label;
            items += WebUiRu::Septic::kNum;
            items += String((unsigned)cfg->id);
            items += "</strong>";
            if (!cfg->enabled)
                items += WebUiRu::Septic::kText3;
            if (can_control)
            {
                items += "</div><label class=\"switch\"><input type=\"checkbox\" name=\"sep";
                items += String((unsigned)cfg->id);
                items += "_en\"";
                if (cfg->enabled)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            }
            items += "</div></div>";
            if (can_control)
            {
                items += String("<div class=\"form-row\"><label>") + WebUiRu::Septic::kLabelName + "</label><input class=\"field name\" type=\"text\" name=\"sep";
                items += String((unsigned)cfg->id);
                items += "_name\" value=\"";
                web.appendHtmlEscaped_(items, cfg->name.c_str());
                items += "\"></div>";
                items += String("<div class=\"form-row\" style=\"margin-top:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field mini\" name=\"sep";
                items += String((unsigned)cfg->id);
                items += "_group\"";
                if (!has_groups)
                    items += " disabled";
                items += ">";
                items += web.groupOptionsHtml_(cfg->group_id, true, true);
                items += "</select></div>";
                items += WebUiRu::Septic::kSelectClassFieldMiniSepticSelectData;
                if (cfg->warning_port != SepticController::kInvalidPort)
                    items += String((unsigned)cfg->warning_port);
                items += "\" name=\"sep";
                items += String((unsigned)cfg->id);
                items += WebUiRu::Septic::kWarnSelectClassFieldMiniSepticSelect;
                if (cfg->alarm_port != SepticController::kInvalidPort)
                    items += String((unsigned)cfg->alarm_port);
                items += "\" name=\"sep";
                items += String((unsigned)cfg->id);
                items += WebUiRu::Septic::kAlarmSelectClassFieldMiniSepticSelect;
                if (cfg->relay_warning != SepticController::kInvalidPort)
                    items += String((unsigned)cfg->relay_warning);
                items += "\" name=\"sep";
                items += String((unsigned)cfg->id);
                items += WebUiRu::Septic::kRelayWarnSelectClassFieldMiniSeptic;
                if (cfg->relay_alarm != SepticController::kInvalidPort)
                    items += String((unsigned)cfg->relay_alarm);
                items += "\" name=\"sep";
                items += String((unsigned)cfg->id);
                items += "_relay_alarm\"></select></div></div>";
            }
            items += "<div class=\"status-grid\"><div class=\"status-line\"><span class=\"status-dot ";
            items += warn ? "status-on" : "status-off";
            items += WebUiRu::Septic::kSpanClassStatusDot;
            items += alarm ? "status-on" : "status-off";
            items += WebUiRu::Septic::kSpanClassStatusDot2;
            items += relay_warn ? "status-on" : "status-off";
            items += WebUiRu::Septic::kSpanClassStatusDot3;
            items += relay_alarm ? "status-on" : "status-off";
            if (can_control)
            {
                items += WebUiRu::Septic::kInputTypeCheckboxClassSepticMonitorData2;
                items += String((unsigned)cfg->id);
                items += "_mon\"";
                if (cfg->monitoring_on)
                    items += " checked";
                if (!cfg->enabled)
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label><input type=\"hidden\" name=\"sep";
                items += String((unsigned)cfg->id);
                items += "_mon\" value=\"";
                items += cfg->monitoring_on ? "on" : "off";
                items += "\"></div></div></div>";
            }
            else
            {
                items += "</div></div></div>";
            }
            ++rendered;
        }
        if (items.length() == 0)
            items = WebUiRu::Septic::kText2;
        return items;
    }

String WebInterfaceControllersSepticHelper::listSepticHtml_(WebInterface &web) {
        return listSepticHtml_(web, 0u, SIZE_MAX);
    }

String WebInterfaceControllersSepticHelper::septicPortOptionsJson_(const WebInterface &web, PortIO::PinType type) {
        return web.socketPortOptionsJson_(type);
    }

String WebInterfaceControllersSepticHelper::septicUsedPortsJson_(const WebInterface &web, PortIO::PinType type) {
        return web.globalUsedPortsJson_(type);
    }

size_t WebInterface::septicLocalRenderCount_() const {
        return WebInterfaceControllersSepticHelper::septicLocalRenderCount_(*this);
    }

String WebInterface::septicDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const {
        return WebInterfaceControllersSepticHelper::septicDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

String WebInterface::stackSepticStatusText_(uint32_t node_id) const {
        return WebInterfaceControllersSepticHelper::stackSepticStatusText_(*this, node_id);
    }

bool WebInterface::isStackSepticView_(uint32_t node_id) const {
        return WebInterfaceControllersSepticHelper::isStackSepticView_(*this, node_id);
    }

bool WebInterface::requestStackSeptic_(uint32_t node_id) {
        return WebInterfaceControllersSepticHelper::requestStackSeptic_(*this, node_id);
    }

size_t WebInterface::stackSepticVisibleCount_(uint32_t node_id) const {
        return WebInterfaceControllersSepticHelper::stackSepticVisibleCount_(*this, node_id);
    }

String WebInterface::listStackSepticHtml_(uint32_t node_id, size_t offset, size_t limit) {
        return WebInterfaceControllersSepticHelper::listStackSepticHtml_(*this, node_id, offset, limit);
    }

String WebInterface::listSepticHtml_() {
        return WebInterfaceControllersSepticHelper::listSepticHtml_(*this);
    }

String WebInterface::listSepticHtml_(size_t offset, size_t limit) {
        return WebInterfaceControllersSepticHelper::listSepticHtml_(*this, offset, limit);
    }

String WebInterface::septicPortOptionsJson_(PortIO::PinType type) const {
        return WebInterfaceControllersSepticHelper::septicPortOptionsJson_(*this, type);
    }

String WebInterface::septicUsedPortsJson_(PortIO::PinType type) const {
        return WebInterfaceControllersSepticHelper::septicUsedPortsJson_(*this, type);
    }


