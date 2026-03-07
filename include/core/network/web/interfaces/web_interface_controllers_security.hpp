#pragma once

#ifndef WEB_INTERFACE_CLASS_CONTEXT
class WebInterface;
class WebInterfaceControllersSecurityHelper;
#else

class WebInterfaceControllersSecurityHelper
{
public:
    static size_t securityLocalRenderCount_(const WebInterface &web)
    {
        if (!web._controllers)
            return 0;
        SecurityController &sec = web._controllers->security();
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
    static String securityDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view)
    {
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
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
    static String stackSecurityStatusText_(const WebInterface &web, uint32_t node_id)
    {
            const auto *cache = web._stack_cache->securityCache(node_id);
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
    static String stackSecurityTitle_(const WebInterface &web, uint32_t node_id)
    {
        String title = WebUiRu::Security::kText;
        if (!web._stack_master || node_id == 0)
            return title;
        const size_t count = web._stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            if (web._stack_master->nodeIdAt(i) == node_id)
            {
                String name = web._stack_master->nodeNameAt(i);
                if (name.length() > 0)
                {
                    title += " (";
                    title += name;
                    title += ")";
                }
                return title;
            }
        }
        return title;
    }
    static bool isStackSecurityView_(const WebInterface &web, uint32_t node_id)
    {
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
    }
    static bool requestStackSecurity_(WebInterface &web, uint32_t node_id)
    {
        return web._stack_cache && web._stack_cache->requestSecurity(node_id);
    }
    static String listSecuritySensorsHtml_(WebInterface &web)
    {
        if (!web._controllers)
            return WebUiRu::Security::kText2;
        String items;
        items.reserve(16384);
        SecurityController &sec = web._controllers->security();
    
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
            items += st.is_detect ? "status-on" : "status-off";
            items += "\"></span></td></tr>";
        };
    
        const SecurityController::SensorConfig *first_disabled = nullptr;
        const SecurityController::SensorState *first_disabled_state = nullptr;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = sec.configByIndex(i);
            const auto *st = sec.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Security, cfg->id))
                continue;
            if (cfg->enabled)
            {
                appendRow(*cfg, *st, true);
            }
            else if (can_view_disabled && !first_disabled)
            {
                first_disabled = cfg;
                first_disabled_state = st;
            }
        }
        if (first_disabled && first_disabled_state)
            appendRow(*first_disabled, *first_disabled_state, false);
        if (items.length() == 0)
            items = WebUiRu::Security::kText3;
        return items;
    }
    static String listSecuritySensorsTiles_(WebInterface &web, uint8_t start_idx, uint8_t end_idx)
    {
        if (!web._controllers)
            return WebUiRu::Security::kText4;
        if (end_idx < start_idx)
            end_idx = start_idx;
    
        String items;
        items.reserve(16384);
        SecurityController &sec = web._controllers->security();
    
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
            const bool detected = st.is_detect;
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
            const auto *cfg = sec.configByIndex(idx);
            const auto *st = sec.stateByIndex(idx);
            if (!cfg || !st)
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Security, cfg->id))
                continue;
            if (!web.webSessionIsAdmin_() && !cfg->enabled)
                continue;
            appendTile(*cfg, *st);
        }
        if (items.length() == 0)
            items = WebUiRu::Security::kText8;
        return items;
    }
    static size_t stackSecurityVisibleCount_(const WebInterface &web, uint32_t node_id)
    {
            const auto *cache = web._stack_cache ? web._stack_cache->securityCache(node_id) : nullptr;
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
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Security, cfg.id, node_id))
                    continue;
                if (!can_view_disabled && !cfg.enabled)
                    continue;
                ++count;
            }
            return count;
        
    }
    static String listStackSecuritySensorsTiles_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit)
    {
        const auto *cache = web._stack_cache->securityCache(node_id);
        if (!cache || !cache->has_data)
            return WebUiRu::Security::kText9;
        if (cache->item_count == 0)
            return WebUiRu::Security::kText8;
        String items;
        const size_t page_limit = (limit == 0) ? 1u : limit;
        size_t reserve = 2048u + page_limit * 900u;
        if (reserve < 12288u)
            reserve = 12288u;
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

        size_t rendered = 0;
        size_t visible_idx = 0;
        for (size_t i = 0; i < render_count && rendered < page_limit; ++i)
        {
            const auto &cfg = cache->items[i];
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Security, cfg.id, node_id))
                continue;
            if (!can_view_disabled && !cfg.enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;

            const bool enabled = cfg.enabled;
            const bool detected = cfg.detect;
            const bool is_reed = strcmp(cfg.type, "reed") == 0;
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Security, cfg.id, node_id);
            const bool can_edit_cfg = can_control;

            const bool has_groups = web.hasGroups_(node_id);
            items += "<div class=\"tile js-group-item";
            if (!enabled)
                items += " disabled";
            items += "\" data-group-id=\"";
            items += String((unsigned)cfg.group_id);
            items += "\"";
            items += web.groupVisibilityStyleAttr_(cfg.group_id, node_id);
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
            items += WebUiRu::Security::kNum;
            items += String((unsigned)cfg.id);
            items += "</strong><label class=\"switch\"><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            if (!can_edit_cfg)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";

            items += "<input class=\"field name\" type=\"text\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            web.appendHtmlEscaped_(items, cfg.name);
            items += "\"";
            if (!can_edit_cfg)
                items += " readonly";
            items += ">";
            items += String("<div class=\"form-row\" style=\"margin-top:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field mini\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_group\"";
            if (!can_edit_cfg || !has_groups)
                items += " disabled";
            items += ">";
            items += web.groupOptionsHtml_(cfg.group_id, true, true, node_id);
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
            items += "_type\"";
            if (!can_edit_cfg)
                items += " disabled";
            items += ">";
            appendTypeOption("pir", "pir", strcmp(cfg.type, "reed") != 0);
            appendTypeOption("reed", "reed", strcmp(cfg.type, "reed") == 0);
            items += "</select></div>";

            items += WebUiRu::Security::kSelectClassFieldMiniSecurityPortData;
            if (cfg.port != SecurityController::kInvalidPort)
                items += String((unsigned)cfg.port);
            items += "\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_port\"";
            if (!can_edit_cfg)
                items += " disabled";
            items += "></select></div>";

            items += WebUiRu::Security::kInputTypeCheckboxNameSec;
            items += String((unsigned)cfg.id);
            items += "_silent\"";
            if (cfg.silent)
                items += " checked";
            if (!can_edit_cfg)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div></div></div>";
            ++rendered;
        }
        if (items.length() == 0)
            items = WebUiRu::Security::kText8;
        return items;
    }
    static String securityPortOptionsJson_(const WebInterface &web)
    {
        return web.socketPortOptionsJson_(PortIO::PinType::DInput);
    }
    static String securityUsedPinsJson_(const WebInterface &web)
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (web._controllers)
        {
            SecurityController &sec = web._controllers->security();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
            {
                const auto *cfg = sec.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                const uint8_t pin = cfg->port;
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
};

    size_t securityLocalRenderCount_() const
    {
        return WebInterfaceControllersSecurityHelper::securityLocalRenderCount_(*this);
    }

    String securityDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        return WebInterfaceControllersSecurityHelper::securityDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

    String stackSecurityStatusText_(uint32_t node_id) const
    {
        return WebInterfaceControllersSecurityHelper::stackSecurityStatusText_(*this, node_id);
    }

    String stackSecurityTitle_(uint32_t node_id) const
    {
        return WebInterfaceControllersSecurityHelper::stackSecurityTitle_(*this, node_id);
    }

    bool isStackSecurityView_(uint32_t node_id) const
    {
        return WebInterfaceControllersSecurityHelper::isStackSecurityView_(*this, node_id);
    }

    bool requestStackSecurity_(uint32_t node_id)
    {
        return WebInterfaceControllersSecurityHelper::requestStackSecurity_(*this, node_id);
    }

    String listSecuritySensorsHtml_()
    {
        return WebInterfaceControllersSecurityHelper::listSecuritySensorsHtml_(*this);
    }

    String listSecuritySensorsTiles_(uint8_t start_idx, uint8_t end_idx)
    {
        return WebInterfaceControllersSecurityHelper::listSecuritySensorsTiles_(*this, start_idx, end_idx);
    }

    size_t stackSecurityVisibleCount_(uint32_t node_id) const
    {
        return WebInterfaceControllersSecurityHelper::stackSecurityVisibleCount_(*this, node_id);
    }

    String listStackSecuritySensorsTiles_(uint32_t node_id, size_t offset, size_t limit)
    {
        return WebInterfaceControllersSecurityHelper::listStackSecuritySensorsTiles_(*this, node_id, offset, limit);
    }

    String securityPortOptionsJson_() const
    {
        return WebInterfaceControllersSecurityHelper::securityPortOptionsJson_(*this);
    }

    String securityUsedPinsJson_() const
    {
        return WebInterfaceControllersSecurityHelper::securityUsedPinsJson_(*this);
    }

#endif

