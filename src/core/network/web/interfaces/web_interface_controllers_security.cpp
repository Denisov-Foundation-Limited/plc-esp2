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
size_t summarySecurityVisibleCount_(const StackUnitSnapshot::State &snapshot)
{
    if (snapshot.security_detected == 0)
        return 0;
    size_t count = snapshot.security_detect_preview_count;
    if (count == 0)
        count = 1;
    if (snapshot.security_detected > snapshot.security_detect_preview_count)
        ++count;
    return count;
}

void loadLocalSecurityItems_(SecurityController &sec, WebInterface::ScratchBuffer &scratch, size_t count)
{
    if (count == 0)
        return;
    auto guard = sec.lockGuard();
    for (size_t i = 0; i < count; ++i)
    {
        scratch.security_valid[i] = false;
        const auto *cfg = sec.configByIndex(i);
        const auto *st = sec.stateByIndex(i);
        if (!cfg || !st)
            continue;
        scratch.security_valid[i] = true;
        scratch.security_cfg[i] = *cfg;
        scratch.security_st[i] = *st;
    }
}
}

size_t WebInterfaceControllersSecurityHelper::securityLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        SecurityController &sec = web._controllers->security();
        auto guard = sec.lockGuard();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = sec.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return SecurityController::kSensorCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > SecurityController::kSensorCount ? SecurityController::kSensorCount : count;
    }

String WebInterfaceControllersSecurityHelper::securityDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"security-device\" class=\"field mini\">";
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

String WebInterfaceControllersSecurityHelper::stackSecurityStatusText_(const WebInterface &web, uint32_t node_id) {
        if (!web.network() || node_id == 0)
            return WebUiRu::kNoDataFromSlave;
        StackUnitSnapshot::State snapshot{};
        if (!web.network()->stackIndexState(node_id, snapshot))
            return WebUiRu::kNoDataFromSlave;
        String out = snapshot.security_enabled ? (snapshot.security_armed ? String("Armed") : String("Disarmed"))
                                               : String("Disabled");
        if (snapshot.security_alarm)
            out += ", alarm";
        if (snapshot.security_detected > 0)
        {
            out += ", detect: ";
            out += String((unsigned)snapshot.security_detected);
        }
        else if (web._controllers)
        {
            const size_t active = web._controllers->security().remoteDetectCount(node_id);
            if (active > 0)
            {
                out += ", detect: ";
                out += String((unsigned)active);
            }
        }
        return out;
    }

String WebInterfaceControllersSecurityHelper::stackSecurityTitle_(const WebInterface &web, uint32_t node_id) {
        String title = WebUiRu::Security::kText;
        if (!web.network() || node_id == 0)
            return title;
        StackDeviceRegistry::DeviceInfo device{};
        if (web.network()->stackDeviceSnapshotByNodeId(node_id, device))
        {
            if (device.name[0])
            {
                title += " (";
                title += String(device.name);
                title += ")";
            }
            return title;
        }
        return title;
    }

bool WebInterfaceControllersSecurityHelper::isStackSecurityView_(const WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

bool WebInterfaceControllersSecurityHelper::requestStackSecurity_(WebInterface &web, uint32_t node_id) {
        if (!web.network() || node_id == 0)
            return false;
        return web.network()->stackRoute().sendRequestSelected(web.stackPayloadMode(), node_id, "controllers",
                                                               "summary_req", nullptr, true);
    }

String WebInterfaceControllersSecurityHelper::listSecuritySensorsHtml_(WebInterface &web) {
        if (!web._controllers)
            return WebUiRu::Security::kText2;
        String items;
        items.reserve(16384);
        auto scratch_guard = web.scratchLockGuard_();
        WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
        if (!scratch)
            return WebUiRu::Security::kText2;
        SecurityController &sec = web._controllers->security();
        loadLocalSecurityItems_(sec, *scratch, SecurityController::kSensorCount);
    
        auto appendTypeOption = [&](const char *value, const char *label, bool selected) {
            items += "<option value=\"";
            items += value;
            items += "\"";
            if (selected)
                items += " selected";
            items += ">";
            items += label;
            items += "</option>";
        };
    
        auto appendRow = [&](const SecurityController::SensorConfig &cfg, const SecurityController::SensorState &st,
                             bool enabled) {
            items += "<tr><td class=\"right\"><strong>";
            items += String((unsigned)cfg.id);
            items += "</strong></td><td><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "></td><td><input class=\"field name\" type=\"text\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            web.appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"></td><td><select class=\"field mini\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_type\">";
            appendTypeOption("pir", "pir", cfg.type == SecurityController::SensorType::Pir);
            appendTypeOption("reed", "reed", cfg.type == SecurityController::SensorType::Reed);
            items += "</select></td><td><select class=\"field mini security-port\" data-type=\"dinput\" data-selected=\"";
            if (cfg.port != SecurityController::kInvalidPort)
                items += String((unsigned)cfg.port);
            items += "\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_port\"></select></td><td><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_silent\"";
            if (cfg.silent)
                items += " checked";
            items += "></td><td class=\"center\"><span class=\"status-dot ";
            items += st.active ? "status-on" : "status-off";
            items += "\"></span></td></tr>";
        };
    
        const SecurityController::SensorConfig *first_disabled = nullptr;
        const SecurityController::SensorState *first_disabled_state = nullptr;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            if (!scratch->security_valid[i])
                continue;
            const auto &cfg = scratch->security_cfg[i];
            const auto &st = scratch->security_st[i];
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Security, cfg.id))
                continue;
            if (cfg.enabled)
            {
                appendRow(cfg, st, true);
            }
            else if (can_view_disabled && !first_disabled)
            {
                first_disabled = &scratch->security_cfg[i];
                first_disabled_state = &scratch->security_st[i];
            }
        }
        if (first_disabled && first_disabled_state)
            appendRow(*first_disabled, *first_disabled_state, false);
        if (items.length() == 0)
            items = WebUiRu::Security::kText3;
        return items;
    }

String WebInterfaceControllersSecurityHelper::listSecuritySensorsTiles_(WebInterface &web, uint8_t start_idx, uint8_t end_idx) {
        if (!web._controllers)
            return WebUiRu::Security::kText4;
        if (end_idx < start_idx)
            end_idx = start_idx;
    
        String items;
        items.reserve(16384);
        auto scratch_guard = web.scratchLockGuard_();
        WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
        if (!scratch)
            return WebUiRu::Security::kText4;
        SecurityController &sec = web._controllers->security();
        loadLocalSecurityItems_(sec, *scratch, SecurityController::kSensorCount);
    
        auto appendTypeOption = [&](String &out, const char *value, const char *label, bool selected) {
            out += "<option value=\"";
            out += value;
            out += "\"";
            if (selected)
                out += " selected";
            out += ">";
            out += label;
            out += "</option>";
        };
    
        auto appendTile = [&](const SecurityController::SensorConfig &cfg, const SecurityController::SensorState &st) {
            const bool enabled = cfg.enabled;
            const bool detected = st.active;
            const bool is_reed = cfg.type == SecurityController::SensorType::Reed;
            const bool has_groups = web.hasGroups_();
            items += "<div class=\"tile js-group-item";
            if (!enabled)
                items += " disabled";
            items += "\" data-group-id=\"";
            items += String((unsigned)cfg.group_id);
            items += "\"";
            items += web.groupVisibilityStyleAttr_(cfg.group_id);
            items += " data-sensor-id=\"";
            items += String((unsigned)cfg.id);
            items += "\"><div class=\"sock-visual\"><span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sock-icon ";
            if (!enabled)
                items += "off";
            else if (detected)
                items += "alert";
            else
                items += "on";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            if (is_reed)
            {
                items += "<rect x=\"6\" y=\"18\" width=\"14\" height=\"28\" rx=\"3\" fill=\"currentColor\"/>";
                items += "<rect x=\"44\" y=\"18\" width=\"14\" height=\"28\" rx=\"3\" fill=\"currentColor\"/>";
                items += "<rect x=\"22\" y=\"30\" width=\"20\" height=\"4\" rx=\"2\" fill=\"currentColor\"/>";
            }
            else
            {
                items += "<circle cx=\"32\" cy=\"24\" r=\"6\" fill=\"currentColor\"/>";
                items += "<path d=\"M14 48c6-10 12-14 18-14s12 4 18 14\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"4\" stroke-linecap=\"round\"/>";
                items += "<path d=\"M8 20c6-6 12-10 18-12\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"3\" stroke-linecap=\"round\"/>";
                items += "<path d=\"M56 20c-6-6-12-10-18-12\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"3\" stroke-linecap=\"round\"/>";
            }
            items += "</svg></div><div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                web.appendHtmlEscaped_(items, cfg.name);
            else
                items += String(WebUiRu::Security::kNum) + String((unsigned)cfg.id);
            items += "</strong><label class=\"switch\"><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += String("<div class=\"form-row\"><label>") + WebUiRu::Security::kLabelName + "</label><input class=\"field name\" type=\"text\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            web.appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"></div>";
            items += String("<div class=\"form-row\" style=\"margin-top:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field mini\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_group\"";
            if (!has_groups)
                items += " disabled";
            items += ">";
            items += web.groupOptionsHtml_(cfg.group_id, true, true);
            items += "</select></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            if (!enabled)
                items += "status-off";
            else if (detected)
                items += "status-bad";
            else
                items += "status-on";
            items += "\"></span><span class=\"status-text\">";
            if (!enabled)
                items += WebUiRu::Security::kText5;
            else if (detected)
                items += WebUiRu::Security::kText6;
            else
                items += WebUiRu::Security::kText7;
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += WebUiRu::Security::kSelectClassFieldMiniNameSec;
            items += String((unsigned)cfg.id);
            items += "_type\">";
            appendTypeOption(items, "pir", "pir", cfg.type == SecurityController::SensorType::Pir);
            appendTypeOption(items, "reed", "reed", cfg.type == SecurityController::SensorType::Reed);
            items += "</select></div>";
            items += WebUiRu::Security::kSelectClassFieldMiniSecurityPortData;
            if (cfg.port != SecurityController::kInvalidPort)
                items += String((unsigned)cfg.port);
            items += "\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_port\"></select></div>";
            items += WebUiRu::Security::kInputTypeCheckboxNameSec;
            items += String((unsigned)cfg.id);
            items += "_silent\"";
            if (cfg.silent)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div></div></div>";
        };
    
        const size_t render_count = web.securityLocalRenderCount_();
        if (render_count == 0)
            return WebUiRu::Security::kText8;
        const size_t max_idx = render_count - 1;
        if (start_idx > max_idx)
            start_idx = (uint8_t)max_idx;
        if (end_idx > max_idx)
            end_idx = (uint8_t)max_idx;
        for (uint8_t idx = start_idx; idx <= end_idx && idx < SecurityController::kSensorCount; ++idx)
        {
            if (!scratch->security_valid[idx])
                continue;
            const auto &cfg = scratch->security_cfg[idx];
            const auto &st = scratch->security_st[idx];
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Security, cfg.id))
                continue;
            if (!web.webSessionIsAdmin_() && !cfg.enabled)
                continue;
            appendTile(cfg, st);
        }
        if (items.length() == 0)
            items = WebUiRu::Security::kText8;
        return items;
    }

size_t WebInterfaceControllersSecurityHelper::stackSecurityVisibleCount_(const WebInterface &web, uint32_t node_id) {
        if (!web._controllers || node_id == 0)
            return 0;
        const size_t remote_count = web._controllers->security().remoteDetectCount(node_id);
        if (remote_count > 0)
            return remote_count;
        if (!web.network())
            return 0;
        StackUnitSnapshot::State snapshot{};
        if (!web.network()->stackIndexState(node_id, snapshot))
            return 0;
        return summarySecurityVisibleCount_(snapshot);
    }

String WebInterfaceControllersSecurityHelper::listStackSecuritySensorsTiles_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
        if (!web._controllers || node_id == 0 || !web.network())
            return WebUiRu::Security::kText9;
        SecurityController &sec = web._controllers->security();
        const size_t total = sec.remoteDetectCount(node_id);
        String items;
        items.reserve(8192);

        if (total > 0)
        {
            const size_t start = offset;
            const size_t end = (limit == 0) ? total : ((offset + limit > total) ? total : (offset + limit));
            for (size_t i = start; i < end; ++i)
            {
                SecurityController::RemoteDetect item{};
                if (!sec.remoteDetectAt(i, item, node_id))
                    continue;
                items += "<div class=\"tile js-group-item\" data-group-id=\"0\" data-sensor-id=\"";
                items += String((unsigned)item.sensor_id);
                items += "\"><div class=\"sock-visual\"><span class=\"badge\">#";
                items += String((unsigned)item.sensor_id);
                items += "</span><svg class=\"sock-icon ";
                items += item.silent ? "on" : "alert";
                items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<circle cx=\"32\" cy=\"24\" r=\"6\" fill=\"currentColor\"/>";
                items += "<path d=\"M14 48c6-10 12-14 18-14s12 4 18 14\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"4\" stroke-linecap=\"round\"/>";
                items += "<path d=\"M8 20c6-6 12-10 18-12\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"3\" stroke-linecap=\"round\"/>";
                items += "<path d=\"M56 20c-6-6-12-10-18-12\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"3\" stroke-linecap=\"round\"/>";
                items += "</svg></div><div><div class=\"tile-head\"><strong>";
                if (item.sensor_name.length())
                    web.appendHtmlEscaped_(items, item.sensor_name.c_str());
                else
                    items += String(WebUiRu::Security::kNum) + String((unsigned)item.sensor_id);
                items += "</strong></div>";
                items += "<div class=\"status-line\"><span class=\"status-dot ";
                items += item.silent ? "status-on" : "status-bad";
                items += "\"></span><span class=\"status-text\">";
                items += item.silent ? "Silent detect" : WebUiRu::Security::kText6;
                items += "</span></div>";
                items += "<div class=\"form-grid\">";
                items += "<div class=\"form-row\"><label>Unit</label><input class=\"field\" type=\"text\" readonly value=\"";
                web.appendHtmlEscaped_(items,
                                       item.unit_name.length() ? item.unit_name.c_str() : web.stackNodeIdHex_(item.node_id).c_str());
                items += "\"></div>";
                items += "<div class=\"form-row\"><label>Sensor</label><input class=\"field\" type=\"text\" readonly value=\"";
                items += String((unsigned)item.sensor_id);
                items += "\"></div>";
                items += "<div class=\"form-row\"><label>Mode</label><input class=\"field\" type=\"text\" readonly value=\"";
                items += item.silent ? "silent" : "alarm";
                items += "\"></div>";
                items += "</div></div></div>";
            }
        }
        else
        {
            StackUnitSnapshot::State snapshot{};
            if (!web.network()->stackIndexState(node_id, snapshot) || snapshot.security_detected == 0)
                return "<div class=\"tile empty\">No active remote detections</div>";
            const size_t total_summary = summarySecurityVisibleCount_(snapshot);
            const size_t start = offset;
            const size_t end = (limit == 0) ? total_summary : ((offset + limit > total_summary) ? total_summary : (offset + limit));
            for (size_t i = start; i < end; ++i)
            {
                if (i < snapshot.security_detect_preview_count)
                {
                    const auto &preview = snapshot.security_detect_preview[i];
                    items += "<div class=\"tile js-group-item\" data-group-id=\"0\" data-sensor-id=\"";
                    items += String((unsigned)preview.id);
                    items += "\"><div class=\"sock-visual\"><span class=\"badge\">#";
                    items += String((unsigned)preview.id);
                    items += "</span><svg class=\"sock-icon alert\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                    items += "<circle cx=\"32\" cy=\"24\" r=\"6\" fill=\"currentColor\"/>";
                    items += "<path d=\"M14 48c6-10 12-14 18-14s12 4 18 14\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"4\" stroke-linecap=\"round\"/>";
                    items += "<path d=\"M8 20c6-6 12-10 18-12\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"3\" stroke-linecap=\"round\"/>";
                    items += "<path d=\"M56 20c-6-6-12-10-18-12\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"3\" stroke-linecap=\"round\"/>";
                    items += "</svg></div><div><div class=\"tile-head\"><strong>";
                    if (preview.name[0] != '\0')
                        web.appendHtmlEscaped_(items, preview.name);
                    else
                        items += String(WebUiRu::Security::kNum) + String((unsigned)preview.id);
                    items += "</strong></div>";
                    items += "<div class=\"status-line\"><span class=\"status-dot status-bad\"></span><span class=\"status-text\">";
                    items += WebUiRu::Security::kText6;
                    items += "</span></div>";
                    items += "<div class=\"form-grid\">";
                    items += "<div class=\"form-row\"><label>Unit</label><input class=\"field\" type=\"text\" readonly value=\"";
                    web.appendHtmlEscaped_(items, web.stackNodeIdHex_(node_id).c_str());
                    items += "\"></div>";
                    items += "<div class=\"form-row\"><label>Source</label><input class=\"field\" type=\"text\" readonly value=\"summary\"></div>";
                    items += "</div></div></div>";
                }
                else
                {
                    const size_t more = snapshot.security_detected > snapshot.security_detect_preview_count
                                            ? (snapshot.security_detected - snapshot.security_detect_preview_count)
                                            : 0u;
                    if (more == 0)
                        continue;
                    items += "<div class=\"tile empty\">More active sensors: ";
                    items += String((unsigned)more);
                    items += "</div>";
                }
            }
        }
        return items.length() ? items : String("<div class=\"tile empty\">No active remote detections</div>");
    }

String WebInterfaceControllersSecurityHelper::securityPortOptionsJson_(const WebInterface &web) {
        return web.socketPortOptionsJson_(PortIO::PinType::DInput);
    }

String WebInterfaceControllersSecurityHelper::securityUsedPinsJson_(const WebInterface &web) {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (web._controllers)
        {
            auto scratch_guard = web.scratchLockGuard_();
            WebInterface::ScratchBuffer *scratch = (scratch_guard.locked() ? web.scratchBuffer_() : nullptr);
            if (!scratch)
                return "[]";
            SecurityController &sec = web._controllers->security();
            loadLocalSecurityItems_(sec, *scratch, SecurityController::kSensorCount);
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                if (!scratch->security_valid[i] || !scratch->security_cfg[i].enabled)
                    continue;
                const uint8_t pin = scratch->security_cfg[i].port;
                if (pin != SecurityController::kInvalidPort && pin < PortIO::PORT_COUNT)
                    used[pin] = true;
            }
            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                if (!used[i])
                    continue;
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != PortIO::PinType::DInput)
                    continue;
                if (!first)
                    out += ",";
                out += String((unsigned)i);
                first = false;
            }
        }
        out += "]";
        return out;
    }

size_t WebInterface::securityLocalRenderCount_() const {
        return WebInterfaceControllersSecurityHelper::securityLocalRenderCount_(*this);
    }

String WebInterface::securityDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const {
        return WebInterfaceControllersSecurityHelper::securityDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

String WebInterface::stackSecurityStatusText_(uint32_t node_id) const {
        return WebInterfaceControllersSecurityHelper::stackSecurityStatusText_(*this, node_id);
    }

String WebInterface::stackSecurityTitle_(uint32_t node_id) const {
        return WebInterfaceControllersSecurityHelper::stackSecurityTitle_(*this, node_id);
    }

bool WebInterface::isStackSecurityView_(uint32_t node_id) const {
        return WebInterfaceControllersSecurityHelper::isStackSecurityView_(*this, node_id);
    }

bool WebInterface::requestStackSecurity_(uint32_t node_id) {
        return WebInterfaceControllersSecurityHelper::requestStackSecurity_(*this, node_id);
    }

String WebInterface::listSecuritySensorsHtml_() {
        return WebInterfaceControllersSecurityHelper::listSecuritySensorsHtml_(*this);
    }

String WebInterface::listSecuritySensorsTiles_(uint8_t start_idx, uint8_t end_idx) {
        return WebInterfaceControllersSecurityHelper::listSecuritySensorsTiles_(*this, start_idx, end_idx);
    }

size_t WebInterface::stackSecurityVisibleCount_(uint32_t node_id) const {
        return WebInterfaceControllersSecurityHelper::stackSecurityVisibleCount_(*this, node_id);
    }

String WebInterface::listStackSecuritySensorsTiles_(uint32_t node_id, size_t offset, size_t limit) {
        return WebInterfaceControllersSecurityHelper::listStackSecuritySensorsTiles_(*this, node_id, offset, limit);
    }

String WebInterface::securityPortOptionsJson_() const {
        return WebInterfaceControllersSecurityHelper::securityPortOptionsJson_(*this);
    }

String WebInterface::securityUsedPinsJson_() const {
        return WebInterfaceControllersSecurityHelper::securityUsedPinsJson_(*this);
    }


