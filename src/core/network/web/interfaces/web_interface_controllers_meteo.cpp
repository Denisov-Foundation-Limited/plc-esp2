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

size_t WebInterfaceControllersMeteoHelper::meteoLocalRenderCount_(const WebInterface &web) {
        if (!web._controllers)
            return 0;
        MeteoController &meteo = web._controllers->meteo();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return MeteoController::kSensorCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > MeteoController::kSensorCount ? MeteoController::kSensorCount : count;
    }

String WebInterfaceControllersMeteoHelper::meteoDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view) {
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += String("<span class=\"muted\">") + WebUiRu::kDevice + "</span>";
        html += "<select id=\"meteo-device\" class=\"field mini\">";
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

String WebInterfaceControllersMeteoHelper::stackMeteoStatusText_(const WebInterface &web, uint32_t node_id) {
            const auto *cache = web._stack_cache->meteoCache(node_id);
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

bool WebInterfaceControllersMeteoHelper::isStackMeteoView_(const WebInterface &web, uint32_t node_id) {
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
    }

bool WebInterfaceControllersMeteoHelper::requestStackMeteo_(WebInterface &web, uint32_t node_id) {
        return web._stack_cache && web._stack_cache->requestMeteo(node_id);
    }

size_t WebInterfaceControllersMeteoHelper::stackMeteoVisibleCount_(const WebInterface &web, uint32_t node_id) {
            const auto *cache = web._stack_cache ? web._stack_cache->meteoCache(node_id) : nullptr;
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
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Meteo, cfg.id, node_id))
                    continue;
                if (!can_view_disabled && !cfg.enabled)
                    continue;
                ++count;
            }
            return count;
        
    }

String WebInterfaceControllersMeteoHelper::listStackMeteoHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
            if (web._stack_cache)
                web.requestStackTempSensors_(node_id);
            const auto *cache = web._stack_cache->meteoCache(node_id);
            if (!cache || !cache->has_data)
                return WebUiRu::Meteo::kText;
            if (cache->item_count == 0)
                return WebUiRu::Meteo::kText2;
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
            const auto *ds_cache = web._stack_cache ? web._stack_cache->tempSensorsCache(node_id) : nullptr;
            for (size_t i = 0; i < render_count && rendered < page_limit; ++i)
            {
                const auto &cfg = cache->items[i];
                if (!web.webAclCanViewItem_(UsersRegistry::AclController::Meteo, cfg.id, node_id))
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
                const bool can_edit_stack = can_admin;
                char temp_buf[12] = {};
                char hum_buf[12] = {};
                const char *temp = "--";
                const char *hum = "--";
                if (cfg.has_temp)
                {
                    dtostrf(cfg.temp_c, 0, 1, temp_buf);
                    temp = temp_buf;
                }
                if (cfg.has_hum)
                {
                    dtostrf(cfg.hum, 0, 1, hum_buf);
                    hum = hum_buf;
                }
                char age_buf[16] = {};
                const char *age = "-";
                if (cfg.has_read)
                {
                    snprintf(age_buf, sizeof(age_buf), "%lus", (unsigned long)cfg.age_s);
                    age = age_buf;
                }
                const bool has_data = cfg.has_temp || cfg.has_hum;
                const bool ok_on = has_data && cfg.ok;
                const char *status_class = "status-na";
                if (has_data)
                    status_class = cfg.ok ? "status-ok" : "status-err";
    
                String type_value = cfg.type;
                type_value.toLowerCase();
                if (type_value != "dht22" && type_value != "ds18b20")
                    type_value = "none";
    
                const bool has_groups = web.hasGroups_(node_id);
                items += "<div class=\"tile js-group-item";
                if (!cfg.enabled)
                    items += " disabled";
                items += "\" data-group-id=\"";
                items += String((unsigned)cfg.group_id);
                items += "\"";
                items += web.groupVisibilityStyleAttr_(cfg.group_id, node_id);
                items += " data-sensor-id=\"";
                items += String((unsigned)cfg.id);
                items += "\">";
                items += "<div class=\"sensor-visual\">";
                items += "<span class=\"badge\">#";
                items += String((unsigned)cfg.id);
                items += "</span>";
                items += "<svg class=\"sensor-icon ";
                if (!ok_on)
                    items += "na";
                items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<path fill=\"currentColor\" d=\"M32 6c-5.5 0-10 4.5-10 10v19.2c-2.6 2.4-4 5.7-4 9.3 0 7.2 5.8 13 13 13s13-5.8 13-13c0-3.6-1.4-6.9-4-9.3V16c0-5.5-4.5-10-10-10zm6 33.1V16c0-3.3-2.7-6-6-6s-6 2.7-6 6v23.1l-0.9 0.9c-1.8 1.7-2.8 3.9-2.8 6.4 0 4.9 4 9 9 9s9-4 9-9c0-2.5-1-4.8-2.8-6.4l-0.5-0.5z\"/>";
                items += "<rect x=\"30\" y=\"20\" width=\"4\" height=\"20\" rx=\"2\" fill=\"currentColor\"/>";
                items += "</svg>";
                items += "<div class=\"sensor-readout\">";
                items += "<div class=\"sensor-value\"><span class=\"sensor-temp-value\">";
                items += temp;
                items += WebUiRu::Meteo::kC;
                items += "</span>";
                if (cfg.has_hum)
                {
                    items += "<div class=\"sensor-hum\"><svg class=\"sensor-hum-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                    items += "<path fill=\"currentColor\" d=\"M32 6c7 12 16 22 16 34 0 8.8-7.2 16-16 16S16 48.8 16 40c0-12 9-22 16-34z\"/>";
                    items += "</svg><div class=\"sensor-value sensor-hum-value\">";
                    items += hum;
                    items += "</div><div class=\"sensor-unit\">%</div></div>";
                }
                items += "</div></div>";
                items += "<div>";
                items += "<div class=\"tile-head\"><strong>";
                if (cfg.name[0])
                    web.appendHtmlEscaped_(items, cfg.name);
                else
                    items += WebUiRu::Meteo::kText3;
                items += "</strong>";
                items += "<label class=\"switch\"><input type=\"checkbox\" class=\"meteo-enable\" name=\"m";
                items += String((unsigned)cfg.id);
                items += "_en\"";
                if (cfg.enabled)
                    items += " checked";
                if (!can_edit_stack)
                    items += " disabled";
                items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
                items += WebUiRu::Meteo::kText12;
                items += "<option value=\"local\" selected>local</option></select></div>";
                items += WebUiRu::Meteo::kInputClassFieldNameMeteoNameType;
                items += String((unsigned)cfg.id);
                items += "_name\" value=\"";
                if (cfg.name[0])
                    web.appendHtmlEscaped_(items, cfg.name);
                else
                    items += WebUiRu::Meteo::kText3;
                items += "\"";
                if (!can_edit_stack)
                    items += " readonly";
                items += "></div>";
                items += String("<div class=\"form-row\" style=\"margin-top:8px;margin-bottom:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field\" name=\"m";
                items += String((unsigned)cfg.id);
                items += "_group\"";
                if (!can_edit_stack || !has_groups)
                    items += " disabled";
                items += ">";
                items += web.groupOptionsHtml_(cfg.group_id, true, true, node_id);
                items += "</select></div>";
                items += "<div class=\"form-grid\">";
                items += WebUiRu::Meteo::kSelectClassFieldMeteoTypeNameM;
                items += String((unsigned)cfg.id);
                items += "_type\"";
                if (!can_edit_stack)
                    items += " disabled";
                items += ">";
                items += "<option value=\"none\"";
                if (type_value == "none")
                    items += " selected";
                items += ">none</option>";
                items += "<option value=\"ds18b20\"";
                if (type_value == "ds18b20")
                    items += " selected";
                items += ">ds18b20</option>";
                items += "<option value=\"dht22\"";
                if (type_value == "dht22")
                    items += " selected";
                items += ">dht22</option>";
                items += "</select></div>";
                items += WebUiRu::Meteo::kSelectClassFieldMiniMeteoPinData;
                if (cfg.pin >= 0)
                    items += String(cfg.pin);
                items += "\" name=\"m";
                items += String((unsigned)cfg.id);
                items += "_pin\"";
                if (!can_edit_stack)
                    items += " disabled";
                items += ">";
                if (cfg.pin >= 0)
                {
                    items += "<option value=\"";
                    items += String(cfg.pin);
                    items += "\" selected>";
                    items += String(cfg.pin);
                    items += "</option>";
                }
                else
                {
                    items += "<option value=\"\" selected>-</option>";
                }
                items += "</select></div>";
                items += WebUiRu::Meteo::kSelectClassFieldAddrMeteoAddrName;
                items += String((unsigned)cfg.id);
                items += "_addr\"";
                if (!can_edit_stack)
                    items += " disabled";
                items += ">";
                String addr_value = cfg.addr[0] ? String(cfg.addr) : String();
                bool addr_found = false;
                items += "<option value=\"\"";
                if (!addr_value.length())
                    items += " selected";
                items += ">-</option>";
                if (ds_cache && ds_cache->has_data && ds_cache->items)
                {
                    for (size_t ai = 0; ai < ds_cache->item_count; ++ai)
                    {
                        const auto &a = ds_cache->items[ai];
                        if (!a.addr[0])
                            continue;
                        const bool selected = (addr_value.length() && addr_value == a.addr);
                        items += "<option value=\"";
                        web.appendHtmlEscaped_(items, a.addr);
                        items += "\"";
                        if (selected)
                        {
                            items += " selected";
                            addr_found = true;
                        }
                        if (a.used && !selected)
                            items += " disabled";
                        items += ">";
                        web.appendHtmlEscaped_(items, a.addr);
                        items += "</option>";
                    }
                }
                if (addr_value.length() && !addr_found)
                {
                    items += "<option value=\"";
                    web.appendHtmlEscaped_(items, addr_value.c_str());
                    items += "\" selected>";
                    web.appendHtmlEscaped_(items, addr_value.c_str());
                    items += "</option>";
                }
                items += "</select></div>";
                items += "</div>";
                items += "<div class=\"status-line\"><span class=\"status-dot sensor-status-dot ";
                items += status_class;
                items += "\"></span><span class=\"meteo-age-text\">";
                items += WebUiRu::Meteo::kText13;
                items += age;
                items += "</span></div>";
                items += "</div></div>";
                ++rendered;
            }
            if (items.length() == 0)
                items = WebUiRu::Meteo::kText2;
            return items;
        
    }

String WebInterfaceControllersMeteoHelper::listMeteoHtml_(WebInterface &web, size_t offset, size_t limit) {
        if (!web._controllers)
            return WebUiRu::Meteo::kText11;
        String items;
        items.reserve(16384);
        MeteoController &meteo = web._controllers->meteo();
        const uint32_t now = millis();
        static constexpr size_t kDs18Max = 32;
        char ds18_list[kDs18Max][17] = {};
        size_t ds18_count = 0;
        meteo.listDs18b20Serials(ds18_list, kDs18Max, ds18_count);
        char ds18_used[MeteoController::kSensorCount][17] = {};
        size_t ds18_used_count = 0;
        for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            if (!cfg)
                continue;
            if (cfg->type != MeteoController::SensorType::Ds18b20 || !cfg->ds18_addr_set)
                continue;
            char hex[17] = {};
            MeteoController::formatHexAddr(cfg->ds18_addr, hex);
            bool exists = false;
            for (size_t j = 0; j < ds18_used_count; ++j)
            {
                if (strcmp(ds18_used[j], hex) == 0)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists && ds18_used_count < MeteoController::kSensorCount)
            {
                strncpy(ds18_used[ds18_used_count], hex, sizeof(ds18_used[ds18_used_count]) - 1);
                ++ds18_used_count;
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
    
        auto appendRow = [&](const MeteoController::SensorConfig &cfg, const MeteoController::SensorState &st,
                             bool enabled) {
            const bool can_edit = web.webSessionIsAdmin_() &&
                                  web.webAclCanControlItem_(UsersRegistry::AclController::Meteo, cfg.id);
            const bool has_read = st.last_read_ms != 0;
            char temp_buf[12] = {};
            char hum_buf[12] = {};
            char age_buf[16] = {};
            const char *temp = "--";
            const char *hum = "--";
            String ok = "<span class=\"status-dot sensor-status-dot status-na\" title=\"N/A\"></span>";
            const char *age = "-";
    
            if (st.has_temp)
            {
                dtostrf(st.temp_c, 0, 1, temp_buf);
                temp = temp_buf;
            }
            if (st.has_humidity)
            {
                dtostrf(st.humidity, 0, 1, hum_buf);
                hum = hum_buf;
            }
            if (has_read)
            {
                ok = st.ok ? "<span class=\"status-dot sensor-status-dot status-ok\" title=\"OK\"></span>"
                           : "<span class=\"status-dot sensor-status-dot status-err\" title=\"ERR\"></span>";
                const uint32_t age_s = (uint32_t)((now - st.last_read_ms) / 1000u);
                snprintf(age_buf, sizeof(age_buf), "%lus", (unsigned long)age_s);
                age = age_buf;
            }
    
            String pin;
            if (cfg.type == MeteoController::SensorType::Dht22 && cfg.dht_pin != MeteoController::kInvalidPin)
                pin = String((unsigned)cfg.dht_pin);
    
            String addr;
            if (cfg.type == MeteoController::SensorType::Ds18b20 && cfg.ds18_addr_set)
            {
                char hex[17] = {};
                MeteoController::formatHexAddr(cfg.ds18_addr, hex);
                addr = hex;
            }
    
            const bool has_remote = (cfg.source_node_id != 0 && cfg.source_sensor_id != 0);
            MeteoController::SensorType ui_type = cfg.type;
            if (has_remote)
            {
                MeteoController::SensorType remote_type = MeteoController::SensorType::None;
                if (web.meteoRemoteType_(cfg.source_node_id, cfg.source_sensor_id, remote_type) &&
                    remote_type != MeteoController::SensorType::None)
                    ui_type = remote_type;
            }
            const bool show_hum = (ui_type == MeteoController::SensorType::Dht22);
            const bool ok_on = has_read && st.ok;
    
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
            items += "\">";
            items += "<div class=\"sensor-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sensor-icon ";
            if (!ok_on)
                items += "na";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            items += "<path fill=\"currentColor\" d=\"M32 6c-5.5 0-10 4.5-10 10v19.2c-2.6 2.4-4 5.7-4 9.3 0 7.2 5.8 13 13 13s13-5.8 13-13c0-3.6-1.4-6.9-4-9.3V16c0-5.5-4.5-10-10-10zm6 33.1V16c0-3.3-2.7-6-6-6s-6 2.7-6 6v23.1l-0.9 0.9c-1.8 1.7-2.8 3.9-2.8 6.4 0 4.9 4 9 9 9s9-4 9-9c0-2.5-1-4.8-2.8-6.4l-0.5-0.5z\"/>";
            items += "<rect x=\"30\" y=\"20\" width=\"4\" height=\"20\" rx=\"2\" fill=\"currentColor\"/>";
            items += "</svg>";
            items += "<div class=\"sensor-readout\">";
            items += "<div class=\"sensor-value\"><span class=\"sensor-temp-value\">";
            items += temp;
            items += WebUiRu::Meteo::kC;
            items += "</span>";
            if (show_hum)
            {
                items += "<div class=\"sensor-hum\"><svg class=\"sensor-hum-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<path fill=\"currentColor\" d=\"M32 6c7 12 16 22 16 34 0 8.8-7.2 16-16 16S16 48.8 16 40c0-12 9-22 16-34z\"/>";
                items += "</svg><div class=\"sensor-value sensor-hum-value\">";
                items += hum;
                items += "</div><div class=\"sensor-unit\">%</div></div>";
            }
            items += "</div>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            items += WebUiRu::Meteo::kTitlePrefix;
            items += String((unsigned)cfg.id);
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"meteo-enable\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            if (!can_edit)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += WebUiRu::Meteo::kText12;
            items += web.meteoRemoteNodeOptionsHtml_(cfg.source_node_id);
            items += "</select></div>";
            items += WebUiRu::Meteo::kInputClassFieldNameMeteoNameType;
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            web.appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"";
            if (!can_edit)
                items += " readonly";
            items += "></div>";
            items += String("<div class=\"form-row\" style=\"margin-top:8px;margin-bottom:8px\"><label>") + WebUiRu::GroupsPage::kLabel + "</label><select class=\"field\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_group\"";
            if (!can_edit || !has_groups)
                items += " disabled";
            items += ">";
            items += web.groupOptionsHtml_(cfg.group_id, true, true);
            items += "</select></div>";
            items += WebUiRu::Meteo::kSelectClassFieldNameMeteoSourceName;
            items += String((unsigned)cfg.id);
            items += "_src\">";
            items += web.meteoRemoteSensorOptionsHtml_(cfg.source_sensor_id, cfg.source_node_id);
            items += "</select></div>";
            items += "<div class=\"form-grid\">";
            items += WebUiRu::Meteo::kSelectClassFieldMeteoTypeNameM;
            items += String((unsigned)cfg.id);
            items += "_type\"";
            if (!can_edit)
                items += " disabled";
            items += ">";
            appendTypeOption("none", "none", ui_type == MeteoController::SensorType::None);
            appendTypeOption("ds18b20", "ds18b20", ui_type == MeteoController::SensorType::Ds18b20);
            appendTypeOption("dht22", "dht22", ui_type == MeteoController::SensorType::Dht22);
            items += "</select></div>";
            items += WebUiRu::Meteo::kSelectClassFieldMiniMeteoPinData;
            items += pin;
            items += "\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_pin\"></select></div>";
            items += WebUiRu::Meteo::kSelectClassFieldAddrMeteoAddrName;
            items += String((unsigned)cfg.id);
            items += "_addr\"";
            if (!can_edit)
                items += " disabled";
            items += ">";
            items += "<option value=\"\">-</option>";
            bool addr_found = false;
            for (size_t i = 0; i < ds18_count; ++i)
            {
                bool used = false;
                for (size_t j = 0; j < ds18_used_count; ++j)
                {
                    if (strcmp(ds18_used[j], ds18_list[i]) == 0)
                    {
                        used = true;
                        break;
                    }
                }
                if (used && (!addr.length() || addr != ds18_list[i]))
                    continue;
                items += "<option value=\"";
                items += ds18_list[i];
                items += "\"";
                if (addr.length() && addr == ds18_list[i])
                {
                    items += " selected";
                    addr_found = true;
                }
                items += ">";
                items += ds18_list[i];
                items += "</option>";
            }
            if (addr.length() && !addr_found)
            {
                items += "<option value=\"";
                items += addr;
                items += "\" selected>";
                items += addr;
                items += "</option>";
            }
            items += "</select></div>";
            items += "</div>";
            items += "<div class=\"status-line\">";
            items += ok;
            items += "<span class=\"meteo-age-text\">";
            items += WebUiRu::Meteo::kText13;
            items += age;
            items += "</span></div>";
            items += "</div></div>";
        };
    
        const size_t render_count = web.meteoLocalRenderCount_();
        const size_t page_limit = (limit == 0) ? 1u : limit;
        const bool can_view_disabled = web.webSessionIsAdmin_();
        size_t rendered = 0;
        size_t visible_idx = 0;
        for (size_t i = 0; i < render_count; ++i)
        {
            if (rendered >= page_limit)
                break;
            const auto *cfg = meteo.configByIndex(i);
            const auto *st = meteo.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!web.webAclCanViewItem_(UsersRegistry::AclController::Meteo, cfg->id))
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
            items = WebUiRu::Meteo::kText2;
        return items;
    }

String WebInterfaceControllersMeteoHelper::listMeteoHtml_(WebInterface &web) {
        return listMeteoHtml_(web, 0u, SIZE_MAX);
    }

String WebInterfaceControllersMeteoHelper::meteoPortOptionsJson_(const WebInterface &web) {
        return web.socketPortOptionsJson_(PortIO::PinType::Sensor);
    }

String WebInterfaceControllersMeteoHelper::meteoUsedPinsJson_(const WebInterface &web) {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (web._controllers)
        {
            MeteoController &meteo = web._controllers->meteo();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < MeteoController::kSensorCount; ++i)
            {
                const auto *cfg = meteo.configByIndex(i);
                if (!cfg)
                    continue;
                if (cfg->type != MeteoController::SensorType::Dht22)
                    continue;
                const uint8_t pin = cfg->dht_pin;
                if (pin != MeteoController::kInvalidPin && pin < PortIO::PORT_COUNT)
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

String WebInterfaceControllersMeteoHelper::meteoSensorOptionsHtml_(const WebInterface &web, uint8_t selected_id, uint32_t selected_node_id,
                                          const uint8_t used_local[MeteoController::kSensorCount + 1],
                                          const uint32_t *used_remote, size_t used_remote_count) {
        (void)used_remote;
        (void)used_remote_count;
        String out;
        out += "<option value=\"\">-</option>";
        if (!web._controllers)
            return out;
        MeteoController &meteo = web._controllers->meteo();
        for (uint8_t id = 1; id <= MeteoController::kSensorCount; ++id)
        {
            const auto *cfg = meteo.config(id);
            if (!cfg || !cfg->enabled)
                continue;
            const bool is_selected = (selected_node_id == 0 && id == selected_id);
            const bool is_used = (id <= MeteoController::kSensorCount) && (used_local[id] > 0) && !is_selected;
            if (is_used)
                continue;
            out += "<option value=\"";
            out += String((unsigned)id);
            out += "\"";
            if (is_selected)
                out += " selected";
            out += ">";
            String label_name;
            if (cfg->name.length())
            {
                label_name = cfg->name;
            }
            else if (cfg->source_node_id && cfg->source_sensor_id)
            {
                label_name = web.meteoRemoteSensorName_(cfg->source_node_id, cfg->source_sensor_id);
            }
            out += String((unsigned)id);
            if (label_name.length())
            {
                out += ": ";
                web.appendHtmlEscaped_(out, label_name.c_str());
            }
            out += "</option>";
        }
        if (selected_node_id != 0 && selected_id != 0)
        {
            const String remote_name = web.meteoRemoteSensorName_(selected_node_id, selected_id);
            out += "<option value=\"";
            out += String((unsigned long)selected_node_id);
            out += ":";
            out += String((unsigned)selected_id);
            out += "\" selected>";
            if (remote_name.length())
                web.appendHtmlEscaped_(out, remote_name.c_str());
            else
                out += String((unsigned)selected_id);
            out += "</option>";
        }
        return out;
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteSensorOptionsHtml_(const WebInterface &web, uint8_t selected_id, uint32_t selected_node_id) {
        String out;
        out += "<option value=\"\">-</option>";
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
        {
            const size_t slots = web._stack_slave->remoteMeteoCacheSlots();
            for (size_t i = 0; i < slots; ++i)
            {
                const auto &cache = web._stack_slave->remoteMeteoCacheAt(i);
                if (cache.node_id == 0 || !cache.has_data || !cache.items)
                    continue;
                const String node_label = cache.node_name.length()
                                              ? cache.node_name
                                              : String("Node ") + String((unsigned long)cache.node_id);
                for (size_t s = 0; s < cache.item_count; ++s)
                {
                    const auto &it = cache.items[s];
                    if (!it.enabled)
                        continue;
                    const bool is_selected = (selected_node_id == cache.node_id && it.id == selected_id);
                    out += "<option value=\"";
                    out += String((unsigned long)cache.node_id);
                    out += ":";
                    out += String((unsigned)it.id);
                    out += "\"";
                    out += " data-node=\"";
                    out += String((unsigned long)cache.node_id);
                    out += "\"";
                    if (is_selected)
                        out += " selected";
                    out += ">";
                    web.appendHtmlEscaped_(out, node_label.c_str());
                    out += ": ";
                    if (it.name[0])
                        web.appendHtmlEscaped_(out, it.name);
                    else
                        out += String((unsigned)it.id);
                    out += "</option>";
                }
            }
            return out;
        }
    
        if (web._stack_master && web.stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = web._stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = web._stack_master->nodeIdAt(i);
                if (node_id == 0)
                    continue;
                const StackCache::StackMeteoCache *cache = web.stackCache().meteoCache(node_id);
                if (!cache || !cache->has_data)
                    continue;
                String node_label;
                if (web._stack_master)
                {
                    String ip;
                    uint16_t fw_ver = 0;
                    web._stack_master->nodeInfo(node_id, node_label, ip, fw_ver);
                }
                if (!node_label.length())
                    node_label = String("Node ") + String((unsigned long)node_id);
                for (size_t s = 0; s < cache->item_count; ++s)
                {
                    const auto &it = cache->items[s];
                    if (!it.enabled)
                        continue;
                    const bool is_selected = (selected_node_id == node_id && it.id == selected_id);
                    out += "<option value=\"";
                    out += String((unsigned long)node_id);
                    out += ":";
                    out += String((unsigned)it.id);
                    out += "\"";
                    out += " data-node=\"";
                    out += String((unsigned long)node_id);
                    out += "\"";
                    if (is_selected)
                        out += " selected";
                    out += ">";
                    web.appendHtmlEscaped_(out, node_label.c_str());
                    out += ": ";
                    if (it.name[0])
                        web.appendHtmlEscaped_(out, it.name);
                    else
                        out += String((unsigned)it.id);
                    out += "</option>";
                }
            }
        }
        return out;
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteNodeOptionsHtml_(const WebInterface &web, uint32_t selected_node_id) {
        String out;
        out += "<option value=\"local\"";
        if (selected_node_id == 0)
            out += " selected";
        out += ">local</option>";
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
        {
            const size_t slots = web._stack_slave->remoteMeteoCacheSlots();
            for (size_t i = 0; i < slots; ++i)
            {
                const auto &cache = web._stack_slave->remoteMeteoCacheAt(i);
                if (cache.node_id == 0 || !cache.has_data || !cache.items)
                    continue;
                const String node_label = cache.node_name.length()
                                              ? cache.node_name
                                              : String("Node ") + String((unsigned long)cache.node_id);
                out += "<option value=\"";
                out += String((unsigned long)cache.node_id);
                out += "\"";
                if (selected_node_id == cache.node_id)
                    out += " selected";
                out += ">";
                web.appendHtmlEscaped_(out, node_label.c_str());
                out += "</option>";
            }
            return out;
        }
        if (web._stack_master && web.stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = web._stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = web._stack_master->nodeIdAt(i);
                if (node_id == 0)
                    continue;
                String node_label;
                String ip;
                uint16_t fw_ver = 0;
                web._stack_master->nodeInfo(node_id, node_label, ip, fw_ver);
                if (!node_label.length())
                    node_label = String("Node ") + String((unsigned long)node_id);
                out += "<option value=\"";
                out += String((unsigned long)node_id);
                out += "\"";
                if (selected_node_id == node_id)
                    out += " selected";
                out += ">";
                web.appendHtmlEscaped_(out, node_label.c_str());
                out += "</option>";
            }
        }
        return out;
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteLabel_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id) {
        if (node_id == 0 || sensor_id == 0)
            return "";
        String node_label;
        const char *sensor_name = nullptr;
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
        {
            const auto *cache = web._stack_slave->remoteMeteoCache(node_id);
            if (cache && cache->has_data && cache->items)
            {
                if (cache->node_name.length())
                    node_label = cache->node_name;
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id == sensor_id)
                    {
                        if (it.name[0])
                            sensor_name = it.name;
                        break;
                    }
                }
            }
        }
        else if (web._stack_master && web.stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = web.stackCache().meteoCache(node_id);
            if (cache && cache->has_data)
            {
                String ip;
                uint16_t fw_ver = 0;
                web._stack_master->nodeInfo(node_id, node_label, ip, fw_ver);
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id == sensor_id)
                    {
                        if (it.name[0])
                            sensor_name = it.name;
                        break;
                    }
                }
            }
        }
        if (!node_label.length())
            node_label = String("Node ") + String((unsigned long)node_id);
        String out = node_label;
        out += ": ";
        if (sensor_name && sensor_name[0])
            out += sensor_name;
        else
            out += String((unsigned)sensor_id);
        return out;
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteSensorName_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id) {
        if (node_id == 0 || sensor_id == 0)
            return "";
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
        {
            const auto *cache = web._stack_slave->remoteMeteoCache(node_id);
            if (cache && cache->has_data && cache->items)
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != sensor_id)
                        continue;
                    if (it.name[0])
                        return String(it.name);
                    return String((unsigned)sensor_id);
                }
            }
            return "";
        }
        if (web._stack_master && web.stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = web.stackCache().meteoCache(node_id);
            if (cache && cache->has_data)
            {
                for (size_t i = 0; i < cache->item_count; ++i)
                {
                    const auto &it = cache->items[i];
                    if (it.id != sensor_id)
                        continue;
                    if (it.name[0])
                        return String(it.name);
                    return String((unsigned)sensor_id);
                }
            }
        }
        return "";
    }

bool WebInterfaceControllersMeteoHelper::meteoRemoteType_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id, MeteoController::SensorType &out) {
        out = MeteoController::SensorType::None;
        if (node_id == 0 || sensor_id == 0)
            return false;
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave && web._stack_slave)
        {
            const auto *cache = web._stack_slave->remoteMeteoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
                return false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != sensor_id)
                    continue;
                out = web.parseMeteoTypeName_(it.type);
                return true;
            }
            return false;
        }
        if (web._stack_master && web.stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = web.stackCache().meteoCache(node_id);
            if (!cache || !cache->has_data)
                return false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != sensor_id)
                    continue;
                out = web.parseMeteoTypeName_(it.type);
                return true;
            }
        }
        return false;
    }

bool WebInterfaceControllersMeteoHelper::isMeteoSensorActive_(const WebInterface &web, uint8_t id) {
        if (!web._controllers)
            return false;
        const auto *cfg = web._controllers->meteo().config(id);
        return cfg && cfg->enabled;
    }

bool WebInterfaceControllersMeteoHelper::isRemoteMeteoSensorActive_(const WebInterface &web, uint32_t node_id, uint8_t id) {
        if (web.stackRole_() == ConfigsManagerIface::StackRole::Slave)
        {
            if (!web._stack_slave)
                return false;
            const auto *cache = web._stack_slave->remoteMeteoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
                return false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id == id)
                    return it.enabled;
            }
            return false;
        }
        if (!web._stack_master || web.stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        const StackCache::StackMeteoCache *cache = web.stackCache().meteoCache(node_id);
        if (!cache || !cache->has_data)
            return false;
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const auto &it = cache->items[i];
            if (it.id == id)
                return it.enabled;
        }
        return false;
    }

size_t WebInterface::meteoLocalRenderCount_() const {
        return WebInterfaceControllersMeteoHelper::meteoLocalRenderCount_(*this);
    }

String WebInterface::meteoDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const {
        return WebInterfaceControllersMeteoHelper::meteoDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

String WebInterface::stackMeteoStatusText_(uint32_t node_id) const {
        return WebInterfaceControllersMeteoHelper::stackMeteoStatusText_(*this, node_id);
    }

bool WebInterface::isStackMeteoView_(uint32_t node_id) const {
        return WebInterfaceControllersMeteoHelper::isStackMeteoView_(*this, node_id);
    }

bool WebInterface::requestStackMeteo_(uint32_t node_id) {
        return WebInterfaceControllersMeteoHelper::requestStackMeteo_(*this, node_id);
    }

size_t WebInterface::stackMeteoVisibleCount_(uint32_t node_id) const {
        return WebInterfaceControllersMeteoHelper::stackMeteoVisibleCount_(*this, node_id);
    }

String WebInterface::listStackMeteoHtml_(uint32_t node_id, size_t offset, size_t limit) {
        return WebInterfaceControllersMeteoHelper::listStackMeteoHtml_(*this, node_id, offset, limit);
    }

String WebInterface::listMeteoHtml_() {
        return WebInterfaceControllersMeteoHelper::listMeteoHtml_(*this);
    }

String WebInterface::listMeteoHtml_(size_t offset, size_t limit) {
        return WebInterfaceControllersMeteoHelper::listMeteoHtml_(*this, offset, limit);
    }

String WebInterface::meteoPortOptionsJson_() const {
        return WebInterfaceControllersMeteoHelper::meteoPortOptionsJson_(*this);
    }

String WebInterface::meteoUsedPinsJson_() const {
        return WebInterfaceControllersMeteoHelper::meteoUsedPinsJson_(*this);
    }

String WebInterface::meteoSensorOptionsHtml_(uint8_t selected_id, uint32_t selected_node_id,
                                   const uint8_t used_local[MeteoController::kSensorCount + 1],
                                   const uint32_t *used_remote, size_t used_remote_count) const {
        return WebInterfaceControllersMeteoHelper::meteoSensorOptionsHtml_(*this, selected_id, selected_node_id, used_local, used_remote, used_remote_count);
    }

String WebInterface::meteoRemoteSensorOptionsHtml_(uint8_t selected_id, uint32_t selected_node_id) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteSensorOptionsHtml_(*this, selected_id, selected_node_id);
    }

String WebInterface::meteoRemoteNodeOptionsHtml_(uint32_t selected_node_id) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteNodeOptionsHtml_(*this, selected_node_id);
    }

String WebInterface::meteoRemoteLabel_(uint32_t node_id, uint8_t sensor_id) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteLabel_(*this, node_id, sensor_id);
    }

String WebInterface::meteoRemoteSensorName_(uint32_t node_id, uint8_t sensor_id) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteSensorName_(*this, node_id, sensor_id);
    }

bool WebInterface::meteoRemoteType_(uint32_t node_id, uint8_t sensor_id, MeteoController::SensorType &out) const {
        return WebInterfaceControllersMeteoHelper::meteoRemoteType_(*this, node_id, sensor_id, out);
    }

bool WebInterface::isMeteoSensorActive_(uint8_t id) const {
        return WebInterfaceControllersMeteoHelper::isMeteoSensorActive_(*this, id);
    }

bool WebInterface::isRemoteMeteoSensorActive_(uint32_t node_id, uint8_t id) const {
        return WebInterfaceControllersMeteoHelper::isRemoteMeteoSensorActive_(*this, node_id, id);
    }


