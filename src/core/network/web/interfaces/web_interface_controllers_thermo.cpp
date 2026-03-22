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

size_t WebInterfaceControllersThermoHelper::thermoLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        ThermoController &thermo = web._controllers->thermo();
        auto guard = thermo.lockGuard();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return ThermoController::kDeviceCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > ThermoController::kDeviceCount ? ThermoController::kDeviceCount : count;
    }

String WebInterfaceControllersThermoHelper::thermoDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"thermo-device\" class=\"field mini\">";
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

String WebInterfaceControllersThermoHelper::stackThermoStatusText_(const WebInterface &web, uint32_t node_id) {
            const auto *cache = web._stack_cache->thermoCache(node_id);
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

bool WebInterfaceControllersThermoHelper::isStackThermoView_(const WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

bool WebInterfaceControllersThermoHelper::requestStackThermo_(WebInterface &web, uint32_t node_id) {
        return web._stack_cache && web._stack_cache->requestThermo(node_id);
    }

size_t WebInterfaceControllersThermoHelper::stackThermoVisibleCount_(const WebInterface &web, uint32_t node_id) {
            const auto *cache = web._stack_cache ? web._stack_cache->thermoCache(node_id) : nullptr;
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
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Thermo, cfg.id, node_id))
                    continue;
                if (!can_view_disabled && !cfg.enabled)
                    continue;
                ++count;
            }
            return count;
        
    }

String WebInterfaceControllersThermoHelper::listStackThermoHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
            const auto *cache = web._stack_cache->thermoCache(node_id);
            if (!cache || !cache->has_data)
                return WebUiRu::Thermo::kText;
            if (cache->item_count == 0)
                return WebUiRu::Thermo::kText2;
            String items;
            const size_t page_limit = (limit == 0) ? 1u : limit;
            size_t reserve = 2048u + page_limit * 620u;
            if (reserve < 8192u)
                reserve = 8192u;
            items.reserve(reserve);
            const auto *meteo_cache = web._stack_cache->meteoCache(node_id);
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
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Thermo, cfg.id, node_id))
                    continue;
                if (!can_view_disabled && !cfg.enabled)
                    continue;
                if (visible_idx < offset)
                {
                    ++visible_idx;
                    continue;
                }
                ++visible_idx;
                const bool can_admin = web.webSessionIsAdmin_();
                const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Thermo, cfg.id, node_id);
                const bool mode_off = strcmp(cfg.mode, "off") == 0;
                const bool mode_heat = strcmp(cfg.mode, "heat") == 0;
                const bool mode_cool = strcmp(cfg.mode, "cool") == 0;
                const char *mode_label = WebUiRu::Thermo::kText3;
                if (mode_off)
                    mode_label = WebUiRu::Thermo::kText4;
                else if (mode_heat)
                    mode_label = WebUiRu::Thermo::kText5;
                else if (mode_cool)
                    mode_label = WebUiRu::Thermo::kText6;
    
                const char *state_label = WebUiRu::Thermo::kText7;
                const char *state_class = "status-idle";
                if (cfg.heat_on)
                {
                    state_label = WebUiRu::Thermo::kText5;
                    state_class = "status-heat";
                }
                else if (cfg.cool_on)
                {
                    state_label = WebUiRu::Thermo::kText6;
                    state_class = "status-cool";
                }
                bool show_heat = true;
                bool show_cool = true;
                String heat_class = "icon heat ";
                String cool_class = "icon cool ";
                if (mode_off)
                {
                    show_heat = false;
                    show_cool = false;
                }
                else if (mode_heat)
                {
                    show_cool = false;
                    heat_class += cfg.heat_on ? "active" : "inactive";
                }
                else if (mode_cool)
                {
                    show_heat = false;
                    cool_class += cfg.cool_on ? "active" : "inactive";
                }
                else
                {
                    heat_class += cfg.heat_on ? "active" : "inactive";
                    cool_class += cfg.cool_on ? "active" : "inactive";
                }
    
                const char *sensor_label = WebUiRu::Thermo::kText8;
                const char *sensor_suffix = "";
                char sensor_buf[16] = {};
                if (cfg.sensor != 0)
                {
                    bool found = false;
                    if (meteo_cache && meteo_cache->has_data)
                    {
                        for (size_t s = 0; s < meteo_cache->item_count; ++s)
                        {
                            const auto &ms = meteo_cache->items[s];
                            if (ms.id == cfg.sensor)
                            {
                                found = true;
                                if (ms.has_temp)
                                {
                                    dtostrf(ms.temp_c, 0, 1, sensor_buf);
                                    sensor_label = sensor_buf;
                                    sensor_suffix = WebUiRu::Thermo::kC;
                                }
                                else
                                {
                                    sensor_label = "--";
                                }
                                break;
                            }
                        }
                    }
                    if (!found && cfg.sensor != 0)
                    {
                        sensor_label = "--";
                    }
                }
    
                const bool has_groups = web.hasGroups_(node_id);
                items += "<div class=\"tile js-group-item";
                if (!cfg.enabled)
                    items += " disabled";
                items += "\" data-group-id=\"";
                items += String((unsigned)cfg.group_id);
                items += "\"";
                items += web.groupVisibilityStyleAttr_(cfg.group_id, node_id);
                items += WebUiRu::Thermo::kText9;
                items += sensor_label;
                items += sensor_suffix;
                items += "</span>";
                items += WebUiRu::Thermo::kText15;
                items += String((int)(cfg.target + 0.5f));
                items += "&deg;C</span></div>";
                if (show_heat)
                {
                    items += "<svg class=\"";
                    items += heat_class;
                    items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"22\" y=\"30\" width=\"76\" height=\"60\" rx=\"10\"/><line x1=\"36\" y1=\"40\" x2=\"36\" y2=\"80\"/><line x1=\"52\" y1=\"40\" x2=\"52\" y2=\"80\"/><line x1=\"68\" y1=\"40\" x2=\"68\" y2=\"80\"/><line x1=\"84\" y1=\"40\" x2=\"84\" y2=\"80\"/></svg>";
                }
                if (show_cool)
                {
                    items += "<svg class=\"";
                    items += cool_class;
                    items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"18\" y=\"28\" width=\"84\" height=\"46\" rx=\"10\"/><line x1=\"28\" y1=\"44\" x2=\"92\" y2=\"44\"/><line x1=\"28\" y1=\"56\" x2=\"92\" y2=\"56\"/><line x1=\"40\" y1=\"78\" x2=\"34\" y2=\"92\"/><line x1=\"60\" y1=\"78\" x2=\"60\" y2=\"94\"/><line x1=\"80\" y1=\"78\" x2=\"86\" y2=\"92\"/></svg>";
                }
                items += "</div>";
                items += WebUiRu::Thermo::kNum;
                items += String((unsigned)cfg.id);
                items += "</strong> <span class=\"badge\">";
                items += mode_label;
                items += "</span>";
                if (!cfg.enabled)
                    items += WebUiRu::Thermo::kText16;
                items += "</div><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-enable\" name=\"t";
                items += String((unsigned)cfg.id);
                items += "_en\"";
                if (cfg.enabled)
                    items += " checked";
                if (!(can_admin && can_control))
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
                items += "</div>";
                items += "<input class=\"field name\" type=\"text\" name=\"t";
                items += String((unsigned)cfg.id);
                items += "_name\" value=\"";
                if (cfg.name[0])
                    web.appendHtmlEscaped_(items, cfg.name);
                else
                    items += WebUiRu::Thermo::kText11;
                items += "\"";
                if (!(can_admin && can_control))
                    items += " readonly";
                items += ">";
                items += String("<div class=\"form-row\" style=\"margin-top:8px;margin-bottom:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field mini\" name=\"t";
                items += String((unsigned)cfg.id);
                items += "_group\"";
                if (!(can_admin && can_control) || !has_groups)
                    items += " disabled";
                items += ">";
                items += web.groupOptionsHtml_(cfg.group_id, true, true, node_id);
                items += "</select></div>";
                items += "<div class=\"form-grid\"><div class=\"form-row\"><label>";
                items += WebUiRu::Thermo::kLabelActive;
                items += "</label><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-power\" data-action=\"t";
                items += String((unsigned)cfg.id);
                items += "_power\"";
                if (cfg.power_on)
                    items += " checked";
                if (!cfg.enabled || !can_control)
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
                items += "<div class=\"form-row\"><label>";
                items += WebUiRu::Thermo::kLabelStatus;
                items += "</label><div class=\"status-line\" style=\"margin:0;\"><span class=\"status-dot ";
                items += state_class;
                items += "\"></span><span><span class=\"status-value ";
                if (strcmp(state_class, "status-heat") == 0)
                    items += "status-text-heat";
                else if (strcmp(state_class, "status-cool") == 0)
                    items += "status-text-cool";
                else
                    items += "status-text-idle";
                items += "\">";
                items += state_label;
                items += "</span></span></div></div>";
                items += "<input type=\"hidden\" name=\"t";
                items += String((unsigned)cfg.id);
                items += "_en_force\" value=\"\">";
                items += WebUiRu::Thermo::kSelectClassFieldMiniNameT;
                items += String((unsigned)cfg.id);
                items += "_sensor\"";
                if (!can_admin || !can_control)
                    items += " disabled";
                items += ">";
                bool selected_sensor_present = (cfg.sensor == 0);
                if (meteo_cache && meteo_cache->has_data)
                {
                    for (size_t s = 0; s < meteo_cache->item_count; ++s)
                    {
                        const auto &ms = meteo_cache->items[s];
                        items += "<option value=\"";
                        items += String((unsigned)ms.id);
                        items += "\"";
                        if (ms.id == cfg.sensor)
                        {
                            items += " selected";
                            selected_sensor_present = true;
                        }
                        items += ">";
                        if (ms.name[0])
                            web.appendHtmlEscaped_(items, ms.name);
                        else
                            items += String("#") + String((unsigned)ms.id);
                        items += "</option>";
                    }
                }
                if (!selected_sensor_present && cfg.sensor != 0)
                {
                    items += "<option value=\"";
                    items += String((unsigned)cfg.sensor);
                    items += "\" selected>#";
                    items += String((unsigned)cfg.sensor);
                    items += "</option>";
                }
                items += WebUiRu::Thermo::kSelectClassFieldMiniNameT2;
                items += String((unsigned)cfg.id);
                items += "_mode\"";
                if (!can_control)
                    items += " disabled";
                items += "><option value=\"off\"";
                if (mode_off)
                    items += " selected";
                items += ">off</option><option value=\"heat\"";
                if (mode_heat)
                    items += " selected";
                items += ">heat only</option><option value=\"cool\"";
                if (mode_cool)
                    items += " selected";
                items += ">cool only</option><option value=\"auto\"";
                if (!mode_off && !mode_heat && !mode_cool)
                    items += " selected";
                items += WebUiRu::Thermo::kAutoInputClassFieldTempTypeNumber;
                items += String((unsigned)cfg.id);
                items += "_target\" value=\"";
                items += String((int)(cfg.target + 0.5f));
                items += "\"";
                if (!can_control)
                    items += " disabled";
                items += WebUiRu::Thermo::kInputClassFieldTempTypeNumberStep;
                items += String((unsigned)cfg.id);
                items += "_hyst\" value=\"";
                items += String((int)(cfg.hyst + 0.5f));
                items += "\"";
                if (!can_control)
                    items += " disabled";
                items += WebUiRu::Thermo::kSelectClassFieldMiniThermoSelectData;
                if (cfg.heat != ThermoController::kInvalidPort)
                    items += String((unsigned)cfg.heat);
                items += "\" name=\"t";
                items += String((unsigned)cfg.id);
                items += "_heat\"";
                if (!can_admin || !can_control)
                    items += " disabled";
                items += WebUiRu::Thermo::kSelectClassFieldMiniThermoSelectData2;
                if (cfg.cool != ThermoController::kInvalidPort)
                    items += String((unsigned)cfg.cool);
                items += "\" name=\"t";
                items += String((unsigned)cfg.id);
                items += "_cool\"";
                if (!can_admin || !can_control)
                    items += " disabled";
                items += WebUiRu::Thermo::kSelectClassFieldMiniThermoSelectData3;
                if (cfg.button != ThermoController::kInvalidPort)
                    items += String((unsigned)cfg.button);
                items += "\" name=\"t";
                items += String((unsigned)cfg.id);
                items += "_button\"";
                if (!can_admin || !can_control)
                    items += " disabled";
                items += "></select></div></div>";
                items += "<input type=\"hidden\" name=\"t";
                items += String((unsigned)cfg.id);
                items += "_power\" value=\"";
                items += cfg.power_on ? "on" : "off";
                items += "\">";
                items += "</div></div>";
                ++rendered;
            }
            if (items.length() == 0)
                items = WebUiRu::Thermo::kText2;
            return items;
        
    }

String WebInterfaceControllersThermoHelper::listThermoHtml_(WebInterface &web, size_t offset, size_t limit) {
        if (!web._controllers)
            return "<div class=\"tile empty\"><strong>Thermo unavailable</strong></div>";
        String items;
        items.reserve(16384);
        ThermoController &thermo = web._controllers->thermo();
        MeteoController &meteo = web._controllers->meteo();
        auto thermo_guard = thermo.lockGuard();
        auto meteo_guard = meteo.lockGuard();
        uint8_t sensor_used[MeteoController::kSensorCount + 1] = {};
        uint32_t remote_used[ThermoController::kDeviceCount] = {};
        size_t remote_used_count = 0;
    
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            if (!cfg || !cfg->enabled)
                continue;
            if (cfg->sensor_id == 0 || cfg->sensor_id > MeteoController::kSensorCount)
                continue;
            if (cfg->sensor_node_id == 0)
            {
                sensor_used[cfg->sensor_id]++;
            }
            else if (remote_used_count < ThermoController::kDeviceCount)
            {
                remote_used[remote_used_count++] = (cfg->sensor_node_id << 8) | cfg->sensor_id;
            }
        }
    
        auto appendTile = [&](const ThermoController::DeviceConfig &cfg, const ThermoController::DeviceState &st,
                              bool enabled) {
            const bool can_admin = web.webSessionIsAdmin_();
            const bool can_control = web.webAclCanControlItem_(UsersRegistry::AclController::Thermo, cfg.id);
            const MeteoController::SensorState *sensor_st = nullptr;
            bool remote_has_temp = false;
            float remote_temp_c = 0.0f;
            if (cfg.sensor_id != ThermoController::kInvalidSensor)
            {
                if (cfg.sensor_node_id != 0)
                {
                    if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
                    {
                        const auto *sc = web._stack_slave->remoteMeteoCache(cfg.sensor_node_id);
                        if (sc && sc->has_data)
                        {
                            for (size_t s = 0; s < sc->item_count; ++s)
                            {
                                const auto &it = sc->items[s];
                                if (it.id == cfg.sensor_id)
                                {
                                    remote_has_temp = it.has_temp;
                                    remote_temp_c = it.temp_c;
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        const auto *remote_cache = web.stackCache().meteoCache(cfg.sensor_node_id);
                        if (remote_cache && remote_cache->has_data)
                        {
                            for (size_t s = 0; s < remote_cache->item_count; ++s)
                            {
                                const auto &it = remote_cache->items[s];
                                if (it.id == cfg.sensor_id)
                                {
                                    remote_has_temp = it.has_temp;
                                    remote_temp_c = it.temp_c;
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    for (size_t s = 0; s < MeteoController::kSensorCount; ++s)
                    {
                        const auto *scfg = meteo.configByIndex(s);
                        if (scfg && scfg->id == cfg.sensor_id)
                        {
                            sensor_st = meteo.stateByIndex(s);
                            break;
                        }
                    }
                }
            }
    
            const char *sensor_label = WebUiRu::Thermo::kText8;
            const char *sensor_suffix = "";
            char sensor_buf[16] = {};
            if (cfg.sensor_id != ThermoController::kInvalidSensor)
            {
                if (cfg.sensor_node_id != 0)
                {
                    if (remote_has_temp)
                    {
                        dtostrf(remote_temp_c, 0, 1, sensor_buf);
                        sensor_label = sensor_buf;
                        sensor_suffix = "&deg;C";
                    }
                    else
                    {
                        sensor_label = "--";
                    }
                }
                else
                {
                    if (sensor_st && sensor_st->has_temp)
                    {
                        dtostrf(sensor_st->temp_c, 0, 1, sensor_buf);
                        sensor_label = sensor_buf;
                        sensor_suffix = "&deg;C";
                    }
                    else
                    {
                        sensor_label = "--";
                    }
                }
            }
    
            const char *mode_label = WebUiRu::Thermo::kText3;
            if (cfg.mode == ThermoController::Mode::Off)
                mode_label = WebUiRu::Thermo::kText4;
            else if (cfg.mode == ThermoController::Mode::Heat)
                mode_label = WebUiRu::Thermo::kText5;
            else if (cfg.mode == ThermoController::Mode::Cool)
                mode_label = WebUiRu::Thermo::kText6;
    
            const char *state_label = WebUiRu::Thermo::kText7;
            const char *state_class = "status-idle";
            if (st.heat_on)
            {
                state_label = WebUiRu::Thermo::kText5;
                state_class = "status-heat";
            }
            else if (st.cool_on)
            {
                state_label = WebUiRu::Thermo::kText6;
                state_class = "status-cool";
            }
            bool show_heat = true;
            bool show_cool = true;
            String heat_class = "icon heat ";
            String cool_class = "icon cool ";
            if (cfg.mode == ThermoController::Mode::Off)
            {
                show_heat = false;
                show_cool = false;
            }
            else if (cfg.mode == ThermoController::Mode::Heat)
            {
                show_cool = false;
                heat_class += st.heat_on ? "active" : "inactive";
            }
            else if (cfg.mode == ThermoController::Mode::Cool)
            {
                show_heat = false;
                cool_class += st.cool_on ? "active" : "inactive";
            }
            else
            {
                heat_class += st.heat_on ? "active" : "inactive";
                cool_class += st.cool_on ? "active" : "inactive";
            }
    
            const bool has_groups = web.hasGroups_();
            items += "<div class=\"tile js-group-item";
            if (!enabled)
                items += " disabled";
            items += "\" data-group-id=\"";
            items += String((unsigned)cfg.group_id);
            items += "\"";
            items += web.groupVisibilityStyleAttr_(cfg.group_id);
            items += WebUiRu::Thermo::kText9;
            items += sensor_label;
            items += sensor_suffix;
            items += "</span>";
            items += WebUiRu::Thermo::kText15;
            items += String((int)(cfg.target_c + 0.5f));
            items += "&deg;C</span></div>";
            if (show_heat)
            {
                items += "<svg class=\"";
                items += heat_class;
                items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"22\" y=\"30\" width=\"76\" height=\"60\" rx=\"10\"/><line x1=\"36\" y1=\"40\" x2=\"36\" y2=\"80\"/><line x1=\"52\" y1=\"40\" x2=\"52\" y2=\"80\"/><line x1=\"68\" y1=\"40\" x2=\"68\" y2=\"80\"/><line x1=\"84\" y1=\"40\" x2=\"84\" y2=\"80\"/></svg>";
            }
            if (show_cool)
            {
                items += "<svg class=\"";
                items += cool_class;
                items += "\" viewBox=\"0 0 120 120\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"6\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"18\" y=\"28\" width=\"84\" height=\"46\" rx=\"10\"/><line x1=\"28\" y1=\"44\" x2=\"92\" y2=\"44\"/><line x1=\"28\" y1=\"56\" x2=\"92\" y2=\"56\"/><line x1=\"40\" y1=\"78\" x2=\"34\" y2=\"92\"/><line x1=\"60\" y1=\"78\" x2=\"60\" y2=\"94\"/><line x1=\"80\" y1=\"78\" x2=\"86\" y2=\"92\"/></svg>";
            }
            items += "</div>";
            items += WebUiRu::Thermo::kNum;
            items += String((unsigned)cfg.id);
            items += "</strong> <span class=\"badge\">";
            items += mode_label;
            items += "</span>";
            if (!enabled)
                items += WebUiRu::Thermo::kText16;
            items += "</div><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-enable\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            if (!(can_admin && can_control))
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "</div>";
            items += String("<div class=\"form-row\"><label>") + WebUiRu::Thermo::kLabelName + "</label><input class=\"field name\" type=\"text\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            web.appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"";
            if (!(can_admin && can_control))
                items += " readonly";
            items += "></div>";
            items += String("<div class=\"form-row\" style=\"margin-top:8px;margin-bottom:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field mini\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_group\"";
            if (!(can_admin && can_control) || !has_groups)
                items += " disabled";
            items += ">";
            items += web.groupOptionsHtml_(cfg.group_id, true, true);
            items += "</select></div>";
            items += "<div class=\"form-grid\"><div class=\"form-row\"><label>";
            items += WebUiRu::Thermo::kLabelActive;
            items += "</label><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-power\" data-action=\"t";
            items += String((unsigned)cfg.id);
            items += "_power\"";
            const bool ui_power_on = enabled ? st.power_on : false;
            if (ui_power_on)
                items += " checked";
            if (!enabled || !can_control)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<div class=\"form-row\"><label>";
            items += WebUiRu::Thermo::kLabelStatus;
            items += "</label><div class=\"status-line\" style=\"margin:0;\"><span class=\"status-dot ";
            items += state_class;
            items += "\"></span><span><span class=\"status-value ";
            if (strcmp(state_class, "status-heat") == 0)
                items += "status-text-heat";
            else if (strcmp(state_class, "status-cool") == 0)
                items += "status-text-cool";
            else
                items += "status-text-idle";
            items += "\">";
            items += state_label;
            items += "</span></span></div></div>";
            items += "<input type=\"hidden\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_en_force\" value=\"\">";
            items += WebUiRu::Thermo::kSelectClassFieldMiniNameT;
            items += String((unsigned)cfg.id);
            items += "_sensor\"";
            if (!can_admin || !can_control)
                items += " disabled";
            items += ">";
            items += web.meteoSensorOptionsHtml_(cfg.sensor_id, cfg.sensor_node_id, sensor_used, remote_used, remote_used_count);
            items += WebUiRu::Thermo::kSelectClassFieldMiniNameT2;
            items += String((unsigned)cfg.id);
            items += "_mode\"";
            if (!can_control)
                items += " disabled";
            items += "><option value=\"off\"";
            if (cfg.mode == ThermoController::Mode::Off)
                items += " selected";
            items += ">off</option><option value=\"heat\"";
            if (cfg.mode == ThermoController::Mode::Heat)
                items += " selected";
            items += ">heat only</option><option value=\"cool\"";
            if (cfg.mode == ThermoController::Mode::Cool)
                items += " selected";
            items += ">cool only</option><option value=\"auto\"";
            if (cfg.mode == ThermoController::Mode::Auto)
                items += " selected";
            items += WebUiRu::Thermo::kAutoInputClassFieldTempTypeNumber;
            items += String((unsigned)cfg.id);
            items += "_target\" value=\"";
            items += String((int)(cfg.target_c + 0.5f));
            items += "\"";
            if (!can_control)
                items += " disabled";
            items += WebUiRu::Thermo::kInputClassFieldTempTypeNumberStep;
            items += String((unsigned)cfg.id);
            items += "_hyst\" value=\"";
            items += String((int)(cfg.hysteresis + 0.5f));
            items += "\"";
            if (!can_control)
                items += " disabled";
            items += WebUiRu::Thermo::kSelectClassFieldMiniThermoSelectData;
            if (cfg.heat_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.heat_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_heat\"";
            if (!can_admin || !can_control)
                items += " disabled";
            items += WebUiRu::Thermo::kSelectClassFieldMiniThermoSelectData2;
            if (cfg.cool_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.cool_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_cool\"";
            if (!can_admin || !can_control)
                items += " disabled";
            items += WebUiRu::Thermo::kSelectClassFieldMiniThermoSelectData3;
            if (cfg.button_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_button\"";
            if (!can_admin || !can_control)
                items += " disabled";
            items += "></select></div></div>";
            items += "<input type=\"hidden\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_power\" value=\"\">";
            items += "</div></div>";
        };
    
        const size_t render_count = web.thermoLocalRenderCount_();
        const size_t page_limit = (limit == 0) ? 1u : limit;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t rendered = 0;
        size_t visible_idx = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            if (rendered >= page_limit)
                break;
            const auto *cfg = thermo.configByIndex(i);
            const auto *st = thermo.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Thermo, cfg->id))
                continue;
            if (!can_view_disabled && !cfg->enabled)
                continue;
            if (visible_idx < offset)
            {
                ++visible_idx;
                continue;
            }
            ++visible_idx;
            appendTile(*cfg, *st, cfg->enabled);
            ++rendered;
        }
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Thermo empty</strong></div>";
        return items;
    }

String WebInterfaceControllersThermoHelper::listThermoHtml_(WebInterface &web) {
        return listThermoHtml_(web, 0u, SIZE_MAX);
    }

String WebInterfaceControllersThermoHelper::thermoPortOptionsJson_(const WebInterface &web, PortIO::PinType type) {
        return web.socketPortOptionsJson_(type);
    }

String WebInterfaceControllersThermoHelper::thermoUsedPortsJson_(const WebInterface &web, PortIO::PinType type) {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (web._controllers)
        {
            ThermoController &thermo = web._controllers->thermo();
            auto guard = thermo.lockGuard();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = thermo.configByIndex(i);
                if (!cfg)
                    continue;
                const uint8_t heat = cfg->heat_port;
                const uint8_t cool = cfg->cool_port;
                const uint8_t button = cfg->button_port;
                if (heat != ThermoController::kInvalidPort && heat < PortIO::PORT_COUNT)
                    used[heat] = true;
                if (cool != ThermoController::kInvalidPort && cool < PortIO::PORT_COUNT)
                    used[cool] = true;
                if (button != ThermoController::kInvalidPort && button < PortIO::PORT_COUNT)
                    used[button] = true;
            }
            for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
            {
                if (!used[i])
                    continue;
                const auto &p = ActiveBoardProfile::PORTS[i];
                if (p.caps == Cap::None || p.type != type)
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

size_t WebInterface::thermoLocalRenderCount_() const {
        return WebInterfaceControllersThermoHelper::thermoLocalRenderCount_(*this);
    }

String WebInterface::thermoDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const {
        return WebInterfaceControllersThermoHelper::thermoDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

String WebInterface::stackThermoStatusText_(uint32_t node_id) const {
        return WebInterfaceControllersThermoHelper::stackThermoStatusText_(*this, node_id);
    }

bool WebInterface::isStackThermoView_(uint32_t node_id) const {
        return WebInterfaceControllersThermoHelper::isStackThermoView_(*this, node_id);
    }

bool WebInterface::requestStackThermo_(uint32_t node_id) {
        return WebInterfaceControllersThermoHelper::requestStackThermo_(*this, node_id);
    }

size_t WebInterface::stackThermoVisibleCount_(uint32_t node_id) const {
        return WebInterfaceControllersThermoHelper::stackThermoVisibleCount_(*this, node_id);
    }

String WebInterface::listStackThermoHtml_(uint32_t node_id, size_t offset, size_t limit) {
        return WebInterfaceControllersThermoHelper::listStackThermoHtml_(*this, node_id, offset, limit);
    }

String WebInterface::listThermoHtml_() {
        return WebInterfaceControllersThermoHelper::listThermoHtml_(*this);
    }

String WebInterface::listThermoHtml_(size_t offset, size_t limit) {
        return WebInterfaceControllersThermoHelper::listThermoHtml_(*this, offset, limit);
    }

String WebInterface::thermoPortOptionsJson_(PortIO::PinType type) const {
        return WebInterfaceControllersThermoHelper::thermoPortOptionsJson_(*this, type);
    }

String WebInterface::thermoUsedPortsJson_(PortIO::PinType type) const {
        return WebInterfaceControllersThermoHelper::thermoUsedPortsJson_(*this, type);
    }


