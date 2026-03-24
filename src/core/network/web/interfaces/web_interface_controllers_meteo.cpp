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
        auto guard = meteo.lockGuard();
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
        if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
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

String WebInterfaceControllersMeteoHelper::stackMeteoStatusText_(const WebInterface &web, uint32_t node_id) {
            (void)web;
            (void)node_id;
            return WebUiRu::kNoDataFromSlave;
    }

bool WebInterfaceControllersMeteoHelper::isStackMeteoView_(const WebInterface &web, uint32_t node_id) {
        if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackDeviceRegistry::DeviceInfo device{};
        return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
    }

bool WebInterfaceControllersMeteoHelper::requestStackMeteo_(WebInterface &web, uint32_t node_id) {
        (void)web;
        (void)node_id;
        return false;
    }

size_t WebInterfaceControllersMeteoHelper::stackMeteoVisibleCount_(const WebInterface &web, uint32_t node_id) {
            (void)web;
            (void)node_id;
            return 0;
    }

String WebInterfaceControllersMeteoHelper::listStackMeteoHtml_(WebInterface &web, uint32_t node_id, size_t offset, size_t limit) {
            (void)web;
            (void)node_id;
            (void)offset;
            (void)limit;
            return WebUiRu::Meteo::kText;
    }

String WebInterfaceControllersMeteoHelper::listMeteoHtml_(WebInterface &web, size_t offset, size_t limit) {
        if (!web._controllers)
            return WebUiRu::Meteo::kText11;
        String items;
        items.reserve(16384);
        MeteoController &meteo = web._controllers->meteo();
        auto guard = meteo.lockGuard();
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
            auto guard = meteo.lockGuard();
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
        auto guard = meteo.lockGuard();
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
        (void)web;
        (void)selected_id;
        (void)selected_node_id;
        return "<option value=\"\">-</option>";
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteNodeOptionsHtml_(const WebInterface &web, uint32_t selected_node_id) {
        (void)web;
        String out;
        out += "<option value=\"local\"";
        if (selected_node_id == 0)
            out += " selected";
        out += ">local</option>";
        return out;
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteLabel_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id) {
        (void)web;
        (void)node_id;
        (void)sensor_id;
        return "";
    }

String WebInterfaceControllersMeteoHelper::meteoRemoteSensorName_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id) {
        (void)web;
        (void)node_id;
        (void)sensor_id;
        return "";
    }

bool WebInterfaceControllersMeteoHelper::meteoRemoteType_(const WebInterface &web, uint32_t node_id, uint8_t sensor_id, MeteoController::SensorType &out) {
        out = MeteoController::SensorType::None;
        (void)web;
        (void)node_id;
        (void)sensor_id;
        return false;
    }

bool WebInterfaceControllersMeteoHelper::isMeteoSensorActive_(const WebInterface &web, uint8_t id) {
        if (!web._controllers)
            return false;
        MeteoController &meteo = web._controllers->meteo();
        auto guard = meteo.lockGuard();
        const auto *cfg = meteo.config(id);
        return cfg && cfg->enabled;
    }

bool WebInterfaceControllersMeteoHelper::isRemoteMeteoSensorActive_(const WebInterface &web, uint32_t node_id, uint8_t id) {
        (void)web;
        (void)node_id;
        (void)id;
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


