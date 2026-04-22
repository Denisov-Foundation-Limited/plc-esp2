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
void loadLocalSepticItems_(SepticController &septic, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = septic.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        scratch.septic_valid[i] = false;
        const auto *cfg = septic.configByIndex(i);
        const auto *st = septic.stateByIndex(i);
        if (!cfg || !st)
            continue;
        scratch.septic_valid[i] = true;
        scratch.septic_cfg[i] = *cfg;
        scratch.septic_st[i] = *st;
    }
}

bool waitForStackSepticState_(WebInterface &web, uint32_t node_id, uint32_t timeout_ms = 700u)
{
    if (!web.network() || node_id == 0)
        return false;
    const uint32_t started_ms = millis();
    while ((uint32_t)(millis() - started_ms) < timeout_ms)
    {
        StackUnitSnapshot::State snapshot{};
        if (web.network()->stackIndexState(node_id, snapshot) && snapshot.updated_ms != 0)
            return true;
        delay(25);
    }
    return false;
}
}

size_t WebInterfaceControllersSepticHelper::septicLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        SepticController &septic = web._controllers->septic();
        auto guard = septic.lockGuard();
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
        if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
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

String WebInterfaceControllersSepticHelper::stackSepticStatusText_(const WebInterface &web, uint32_t node_id) {
            if (!web.network() || node_id == 0)
                return WebUiRu::kNoDataFromSlave;
            StackUnitSnapshot::State snapshot{};
            if (!web.network()->stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
                return WebUiRu::kNoDataFromSlave;
            if (snapshot.septic_enabled == 0)
                return WebUiRu::Septic::kDisabledStatus;
            if (snapshot.septic_alert > 0)
                return "ALARM";
            if (snapshot.septic_warning > 0)
                return "WARN";
            return WebUiRu::kStatusOk;
    }

bool WebInterfaceControllersSepticHelper::isStackSepticView_(const WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

bool WebInterfaceControllersSepticHelper::requestStackSeptic_(WebInterface &web, uint32_t node_id) {
        if (!web.network() || node_id == 0)
            return false;
        const bool refresh = web.requestStackIndexState_(node_id);
        const bool sent = web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id,
                                                                          "controllers", "summary_req", nullptr, true);
        return refresh || sent;
    }

size_t WebInterfaceControllersSepticHelper::stackSepticVisibleCount_(const WebInterface &web, uint32_t node_id) {
        StackUnitSnapshot::State snapshot{};
        if (!web.network() || !web.network()->stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
        {
            const_cast<WebInterface &>(web).requestStackSeptic_(node_id);
            waitForStackSepticState_(const_cast<WebInterface &>(web), node_id);
            if (!web.network() || !web.network()->stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
                return 0;
        }
        return snapshot.septic_enabled > 0 ? 1u : 0u;
    }

String WebInterfaceControllersSepticHelper::listStackSepticHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
            (void)offset;
            (void)limit;
            StackUnitSnapshot::State snapshot{};
            if (!web.network() || !web.network()->stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
            {
                web.requestStackSeptic_(node_id);
                waitForStackSepticState_(web, node_id);
                if (!web.network() || !web.network()->stackIndexState(node_id, snapshot) || snapshot.updated_ms == 0)
                    return WebUiRu::kNoDataFromSlave;
            }
            if (snapshot.septic_enabled == 0)
                return WebUiRu::Septic::kText;

            const bool warn = snapshot.septic_warning > 0;
            const bool alarm = snapshot.septic_alert > 0;
            const bool relay_warn = snapshot.septic_relay_warning_on;
            const bool relay_alarm = snapshot.septic_relay_alarm_on;
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Septic, 1, node_id);
            const bool has_groups = web.hasGroups_(node_id);
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

            String items;
            items.reserve(3072);
            items += "<div class=\"tile js-group-item\" data-group-id=\"";
            items += String((unsigned)snapshot.septic_group_id);
            items += "\"";
            items += web.groupVisibilityStyleAttr_(snapshot.septic_group_id, node_id);
            items += "><div class=\"septic-visual\"><div class=\"liquid ";
            items += water_class;
            items += "\" style=\"height:";
            items += water_level;
            items += ";\"></div><div class=\"level-label\">";
            items += water_label;
            items += "</div></div><div><div class=\"tile-head\"><div><strong>";
            if (snapshot.septic_name[0])
                web.appendHtmlEscaped_(items, snapshot.septic_name);
            else
                items += "Септик #1";
            items += "</strong>";
            if (!snapshot.septic_enabled)
                items += " <span class=\"badge\">выкл</span>";
            items += "</div></div>";

            if (can_control)
            {
                items += String("<div class=\"form-row\"><label>") + WebUiRu::Septic::kLabelName +
                         "</label><input class=\"field name\" type=\"text\" name=\"sep1_name\" value=\"";
                web.appendHtmlEscaped_(items, snapshot.septic_name);
                items += "\"></div>";

                items += String("<div class=\"form-row\" style=\"margin-top:8px\"><label>") + WebUiRu::GroupsPage::kLabel +
                         "</label><select class=\"field mini\" name=\"sep1_group\"";
                if (!has_groups)
                    items += " disabled";
                items += ">";
                items += web.groupOptionsHtml_(snapshot.septic_group_id, true, true, node_id);
                items += "</select></div>";

                items += "<div class=\"form-grid\">";
                items += "<div class=\"form-row\"><label>Предупр.</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
                if (snapshot.septic_warning_port != SepticController::kInvalidPort)
                    items += String((unsigned)snapshot.septic_warning_port);
                items += "\" name=\"sep1_warn\"></select></div>";
                items += "<div class=\"form-row\"><label>Тревога</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
                if (snapshot.septic_alarm_port != SepticController::kInvalidPort)
                    items += String((unsigned)snapshot.septic_alarm_port);
                items += "\" name=\"sep1_alarm\"></select></div>";
                items += "<div class=\"form-row\"><label>Реле предупр.</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
                if (snapshot.septic_relay_warning_port != SepticController::kInvalidPort)
                    items += String((unsigned)snapshot.septic_relay_warning_port);
                items += "\" name=\"sep1_relay_warn\"></select></div>";
                items += "<div class=\"form-row\"><label>Реле тревоги</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
                if (snapshot.septic_relay_alarm_port != SepticController::kInvalidPort)
                    items += String((unsigned)snapshot.septic_relay_alarm_port);
                items += "\" name=\"sep1_relay_alarm\"></select></div>";
                items += "</div>";
            }
            else
            {
                items += String("<div class=\"form-row\"><label>") + WebUiRu::Septic::kLabelName +
                         "</label><input class=\"field name\" type=\"text\" readonly value=\"";
                web.appendHtmlEscaped_(items, snapshot.septic_name);
                items += "\"></div>";

                items += String("<div class=\"form-row\" style=\"margin-top:8px\"><label>") + WebUiRu::GroupsPage::kLabel +
                         "</label><select class=\"field mini\" disabled>";
                items += web.groupOptionsHtml_(snapshot.septic_group_id, true, true, node_id);
                items += "</select></div>";

                items += "<div class=\"form-grid\">";
                items += "<div class=\"form-row\"><label>Предупр.</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
                if (snapshot.septic_warning_port != SepticController::kInvalidPort)
                    items += String((unsigned)snapshot.septic_warning_port);
                items += "\" disabled></select></div>";
                items += "<div class=\"form-row\"><label>Тревога</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
                if (snapshot.septic_alarm_port != SepticController::kInvalidPort)
                    items += String((unsigned)snapshot.septic_alarm_port);
                items += "\" disabled></select></div>";
                items += "<div class=\"form-row\"><label>Реле предупр.</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
                if (snapshot.septic_relay_warning_port != SepticController::kInvalidPort)
                    items += String((unsigned)snapshot.septic_relay_warning_port);
                items += "\" disabled></select></div>";
                items += "<div class=\"form-row\"><label>Реле тревоги</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
                if (snapshot.septic_relay_alarm_port != SepticController::kInvalidPort)
                    items += String((unsigned)snapshot.septic_relay_alarm_port);
                items += "\" disabled></select></div>";
                items += "</div>";
            }

            items += "<div class=\"status-grid\"><div class=\"status-line\"><span class=\"status-dot ";
            items += warn ? "status-on" : "status-off";
            items += "\"></span><span>Датчик предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
            items += alarm ? "status-on" : "status-off";
            items += "\"></span><span>Датчик тревоги</span></div><div class=\"status-line\"><span class=\"status-dot ";
            items += relay_warn ? "status-on" : "status-off";
            items += "\"></span><span>Реле предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
            items += relay_alarm ? "status-on" : "status-off";
            items += "\"></span><span>Реле тревоги</span></div></div>";
            items += "<div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep1_mon\"";
            if (snapshot.septic_monitoring_on)
                items += " checked";
            if (!can_control || !snapshot.septic_enabled)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<input type=\"hidden\" name=\"sep1_mon\" value=\"";
            items += snapshot.septic_monitoring_on ? "on" : "off";
            items += "\"></div></div>";
            return items;
    }

String WebInterfaceControllersSepticHelper::listSepticHtml_(WebInterface &web, size_t offset, size_t limit) {
        if (!web._controllers)
            return WebUiRu::Septic::kText9;
        String items;
        items.reserve(2048);
        auto scratch_guard = web.scratchLockGuard_();
        WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
        if (!scratch)
            return WebUiRu::Septic::kText9;
        SepticController &septic = web._controllers->septic();
        loadLocalSepticItems_(septic, *scratch, SepticController::kSepticCount);
        const size_t render_count = web.septicLocalRenderCount_();
        const size_t page_limit = (limit == 0) ? 1u : limit;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t rendered = 0;
        size_t visible_idx = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            if (rendered >= page_limit)
                break;
            if (!scratch->septic_valid[i])
                continue;
            const auto &cfg = scratch->septic_cfg[i];
            const auto &st = scratch->septic_st[i];
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Septic, cfg.id))
                continue;
            if (!can_view_disabled && !cfg.enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Septic, cfg.id);
            const bool has_groups = web.hasGroups_();
            const bool warn = st.warning;
            const bool alarm = st.alarm;
            const bool relay_warn = st.relay_warning;
            const bool relay_alarm = st.relay_alarm;
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
            if (!cfg.enabled)
                items += " disabled";
            items += "\" data-group-id=\"";
            items += String((unsigned)cfg.group_id);
            items += "\"";
            items += web.groupVisibilityStyleAttr_(cfg.group_id);
            items += ">";

            items += "<div class=\"septic-visual\">";
            items += "<div class=\"liquid ";
            items += water_class;
            items += "\" style=\"height:";
            items += water_level;
            items += ";\"></div>";
            items += "<div class=\"level-label\">";
            items += water_label;
            items += "</div>";
            items += "</div>";

            items += "<div>";
            items += "<div class=\"tile-head\"><div><strong>Септик #";
            items += String((unsigned)cfg.id);
            items += "</strong>";
            if (!cfg.enabled)
                items += " <span class=\"badge\">выкл</span>";
            items += "</div>";
            if (can_control)
            {
                items += "<label class=\"switch\"><input type=\"checkbox\" name=\"sep";
                items += String((unsigned)cfg.id);
                items += "_en\"";
                if (cfg.enabled)
                    items += " checked";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            }
            items += "</div>";

            if (can_control)
            {
                items += String("<div class=\"form-row\"><label>") + WebUiRu::Septic::kLabelName + "</label><input class=\"field name\" type=\"text\" name=\"sep";
                items += String((unsigned)cfg.id);
                items += "_name\" value=\"";
                web.appendHtmlEscaped_(items, cfg.name.c_str());
                items += "\"></div>";

                items += String("<div class=\"form-row\" style=\"margin-top:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field mini\" name=\"sep";
                items += String((unsigned)cfg.id);
                items += "_group\"";
                if (!has_groups)
                    items += " disabled";
                items += ">";
                items += web.groupOptionsHtml_(cfg.group_id, true, true);
                items += "</select></div>";

                items += "<div class=\"form-grid\">";

                items += "<div class=\"form-row\"><label>Предупр.</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
                if (cfg.warning_port != SepticController::kInvalidPort)
                    items += String((unsigned)cfg.warning_port);
                items += "\" name=\"sep";
                items += String((unsigned)cfg.id);
                items += "_warn\"></select></div>";

                items += "<div class=\"form-row\"><label>Тревога</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
                if (cfg.alarm_port != SepticController::kInvalidPort)
                    items += String((unsigned)cfg.alarm_port);
                items += "\" name=\"sep";
                items += String((unsigned)cfg.id);
                items += "_alarm\"></select></div>";

                items += "<div class=\"form-row\"><label>Реле предупр.</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
                if (cfg.relay_warning != SepticController::kInvalidPort)
                    items += String((unsigned)cfg.relay_warning);
                items += "\" name=\"sep";
                items += String((unsigned)cfg.id);
                items += "_relay_warn\"></select></div>";

                items += "<div class=\"form-row\"><label>Реле тревоги</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
                if (cfg.relay_alarm != SepticController::kInvalidPort)
                    items += String((unsigned)cfg.relay_alarm);
                items += "\" name=\"sep";
                items += String((unsigned)cfg.id);
                items += "_relay_alarm\"></select></div>";
                items += "</div>";
            }

            items += "<div class=\"status-grid\">";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += warn ? "status-on" : "status-off";
            items += "\"></span><span>Датчик предупреждения</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += alarm ? "status-on" : "status-off";
            items += "\"></span><span>Датчик тревоги</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += relay_warn ? "status-on" : "status-off";
            items += "\"></span><span>Реле предупреждения</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += relay_alarm ? "status-on" : "status-off";
            items += "\"></span><span>Реле тревоги</span></div>";
            items += "</div>";

            if (can_control)
            {
                items += "<div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep";
                items += String((unsigned)cfg.id);
                items += "_mon\"";
                if (cfg.monitoring_on)
                    items += " checked";
                if (!cfg.enabled)
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label><input type=\"hidden\" name=\"sep";
                items += String((unsigned)cfg.id);
                items += "_mon\" value=\"";
                items += cfg.monitoring_on ? "on" : "off";
                items += "\"></div>";
            }

            items += "</div></div>";
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


