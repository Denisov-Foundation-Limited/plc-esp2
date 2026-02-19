#pragma once

    size_t meteoLocalRenderCount_() const
    {
        if (!_controllers)
            return 0;
        MeteoController &meteo = _controllers->meteo();
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

    String meteoDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"meteo-device\" class=\"field mini\">";
        html += "<option value=\"local\"";
        if (!stack_view)
            html += " selected";
        html += ">local</option>";
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t id = _stack_master->nodeIdAt(i);
            html += "<option value=\"";
            html += String((unsigned long)id);
            html += "\"";
            if (stack_view && id == selected_node_id)
                html += " selected";
            html += ">";
            String name = _stack_master->nodeNameAt(i);
            if (name.length() > 0)
                appendHtmlEscaped_(html, name.c_str());
            else
                html += stackNodeIdHex_(id);
            html += "</option>";
        }
        html += "</select></div>";
        return html;
    }


    String stackMeteoStatusText_(uint32_t node_id) const
    {
        if (_stack_cache)
        {
            const auto *cache = _stack_cache->meteoCache(node_id);
            if (!cache)
                return "Нет данных со слейва";
            if (cache->pending)
                return "Запрос данных со слейва...";
            if (!cache->last_ok && cache->last_error.length())
            {
                String msg = "Ошибка: ";
                msg += cache->last_error;
                return msg;
            }
            if (!cache->has_data)
                return "Нет данных со слейва";
            return "OK";
        }
        const StackMeteoCache *cache = findStackMeteoCache_(node_id, false);
        if (!cache)
            return "Нет данных со слейва";
        if (cache->pending)
            return "Запрос данных со слейва...";
        if (!cache->last_ok && cache->last_error.length())
        {
            String msg = "Ошибка: ";
            msg += cache->last_error;
            return msg;
        }
        if (!cache->has_data)
            return "Нет данных со слейва";
        return "OK";
    }


    bool isStackMeteoView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }


    bool requestStackMeteo_(uint32_t node_id)
    {
        if (_stack_cache)
            return _stack_cache->requestMeteo(node_id);
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackMeteoCache *cache = findStackMeteoCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
        {
            if ((uint32_t)(now - cache->updated_ms) > 15000u)
            {
                cache->pending = false;
                cache->pending_cmd_id = 0;
            }
            else
            {
                return false;
            }
        }
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Meteo;
        doc["action"] = "get";
        char payload[96] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        if (!_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdGet,
                                   (const uint8_t *)payload, len))
            return false;
        cache->pending = true;
        cache->pending_cmd_id = cmd_id;
        cache->updated_ms = now;
        return true;
    }


    String listStackMeteoHtml_(uint32_t node_id)
    {
        if (_stack_cache)
        {
            const auto *cache = _stack_cache->meteoCache(node_id);
            if (!cache || !cache->has_data)
                return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
            if (cache->item_count == 0)
                return "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
            String items;
            size_t reserve = 2048u + cache->item_count * 520u;
            if (reserve < 8192u)
                reserve = 8192u;
            items.reserve(reserve);
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
            for (size_t i = 0; i < render_count; ++i)
            {
                const auto &cfg = cache->items[i];
                if (!webAclCanViewItem_(UsersRegistry::AclController::Meteo, cfg.id, node_id))
                    continue;
                if (!webSessionIsAdmin_() && !cfg.enabled)
                    continue;
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
                const bool has_data = cfg.has_temp || cfg.has_hum;
                const bool ok_on = has_data && cfg.ok;
                const char *status_class = "status-na";
                if (has_data)
                    status_class = cfg.ok ? "status-ok" : "status-err";

                String type_label = cfg.type;
                type_label.toLowerCase();
                if (type_label == "dht22")
                    type_label = "DHT22";
                else if (type_label == "ds18b20")
                    type_label = "DS18B20";
                else if (type_label.length() == 0)
                    type_label = "none";

                items += "<div class=\"tile";
                if (!cfg.enabled)
                    items += " disabled";
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
                items += "<div class=\"sensor-value\">";
                items += temp;
                items += "</div><div class=\"sensor-unit\">°C</div>";
                if (cfg.has_hum)
                {
                    items += "<div class=\"sensor-hum\"><svg class=\"sensor-hum-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                    items += "<path fill=\"currentColor\" d=\"M32 6c7 12 16 22 16 34 0 8.8-7.2 16-16 16S16 48.8 16 40c0-12 9-22 16-34z\"/>";
                    items += "</svg><div class=\"sensor-value\">";
                    items += hum;
                    items += "</div><div class=\"sensor-unit\">%</div></div>";
                }
                items += "</div></div>";
                items += "<div>";
                items += "<div class=\"tile-head\"><strong>";
                if (cfg.name[0])
                    appendHtmlEscaped_(items, cfg.name);
                else
                    items += "Датчик";
                items += "</strong></div>";
                items += "<div class=\"status-line\"><span class=\"status-dot ";
                items += status_class;
                items += "\"></span><span class=\"status-text\">";
                if (!cfg.enabled)
                    items += "Отключен";
                else if (!has_data)
                    items += "Нет данных";
                else
                    items += cfg.ok ? "ОК" : "Ошибка";
                items += "</span></div>";
                items += "<div class=\"form-grid\">";
                items += "<div class=\"form-row\"><label>Тип</label><div class=\"field mini\">";
                appendHtmlEscaped_(items, type_label.c_str());
                items += "</div></div>";
                items += "<div class=\"form-row\"><label>Пин</label><div class=\"field mini\">";
                if (cfg.pin >= 0)
                    items += String(cfg.pin);
                else
                    items += "--";
                items += "</div></div>";
                items += "<div class=\"form-row full\"><label>Адрес</label><div class=\"field\">";
                if (cfg.addr[0])
                    appendHtmlEscaped_(items, cfg.addr);
                else
                    items += "--";
                items += "</div></div>";
                items += "</div>";
                items += "</div></div>";
            }
            return items;
        }

        const StackMeteoCache *cache = findStackMeteoCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 520u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
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
        for (size_t i = 0; i < render_count; ++i)
        {
            const StackMeteoItem &cfg = cache->items[i];
            if (!webAclCanViewItem_(UsersRegistry::AclController::Meteo, cfg.id, node_id))
                continue;
            if (!webSessionIsAdmin_() && !cfg.enabled)
                continue;
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
            const bool has_data = cfg.has_temp || cfg.has_hum;
            const bool ok_on = has_data && cfg.ok;
            const char *status_class = "status-na";
            if (has_data)
                status_class = cfg.ok ? "status-ok" : "status-err";

            String type_label = cfg.type;
            type_label.toLowerCase();
            if (type_label == "dht22")
                type_label = "DHT22";
            else if (type_label == "ds18b20")
                type_label = "DS18B20";
            else if (type_label.length() == 0)
                type_label = "none";

            items += "<div class=\"tile";
            if (!cfg.enabled)
                items += " disabled";
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
            items += "<div class=\"sensor-value\">";
            items += temp;
            items += "</div><div class=\"sensor-unit\">°C</div>";
            if (cfg.has_hum)
            {
                items += "<div class=\"sensor-hum\"><svg class=\"sensor-hum-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<path fill=\"currentColor\" d=\"M32 6c7 12 16 22 16 34 0 8.8-7.2 16-16 16S16 48.8 16 40c0-12 9-22 16-34z\"/>";
                items += "</svg><div class=\"sensor-value\">";
                items += hum;
                items += "</div><div class=\"sensor-unit\">%</div></div>";
            }
            items += "</div></div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Датчик";
            items += "</strong></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += status_class;
            items += "\"></span><span class=\"status-text\">";
            if (!cfg.enabled)
                items += "Отключен";
            else if (!has_data)
                items += "Нет данных";
            else
                items += cfg.ok ? "ОК" : "Ошибка";
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Тип</label><div class=\"field mini\">";
            appendHtmlEscaped_(items, type_label.c_str());
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Пин</label><div class=\"field mini\">";
            if (cfg.pin >= 0)
                items += String(cfg.pin);
            else
                items += "--";
            items += "</div></div>";
            items += "<div class=\"form-row full\"><label>Адрес</label><div class=\"field\">";
            if (cfg.addr[0])
                appendHtmlEscaped_(items, cfg.addr);
            else
                items += "--";
            items += "</div></div>";
            items += "</div>";
            items += "</div></div>";
        }
        return items;
    }


    String listMeteoHtml_()
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Метео недоступно</strong></div>";
        String items;
        items.reserve(16384);
        MeteoController &meteo = _controllers->meteo();
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
            const bool has_read = st.last_read_ms != 0;
            char temp_buf[12] = {};
            char hum_buf[12] = {};
            char age_buf[16] = {};
            const char *temp = "--";
            const char *hum = "--";
            String ok = "<span class=\"status-dot status-na\" title=\"N/A\"></span>";
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
                ok = st.ok ? "<span class=\"status-dot status-ok\" title=\"OK\"></span>"
                           : "<span class=\"status-dot status-err\" title=\"ERR\"></span>";
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
                if (meteoRemoteType_(cfg.source_node_id, cfg.source_sensor_id, remote_type) &&
                    remote_type != MeteoController::SensorType::None)
                    ui_type = remote_type;
            }
            const bool show_hum = (ui_type == MeteoController::SensorType::Dht22);
            const bool ok_on = has_read && st.ok;

            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
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
            items += "<div class=\"sensor-value\">";
            items += temp;
            items += "</div><div class=\"sensor-unit\">°C</div>";
            if (show_hum)
            {
                items += "<div class=\"sensor-hum\"><svg class=\"sensor-hum-icon\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                items += "<path fill=\"currentColor\" d=\"M32 6c7 12 16 22 16 34 0 8.8-7.2 16-16 16S16 48.8 16 40c0-12 9-22 16-34z\"/>";
                items += "</svg><div class=\"sensor-value\">";
                items += hum;
                items += "</div><div class=\"sensor-unit\">%</div></div>";
            }
            items += "</div>";
            items += "</div>";
            items += "<div>";
            String remote_label;
            if (has_remote)
                remote_label = meteoRemoteLabel_(cfg.source_node_id, cfg.source_sensor_id);
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Датчик";
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"meteo-enable\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<div class=\"form-row full\" style=\"margin-bottom:8px;\"><label>Устройство</label><select class=\"field meteo-device\">";
            items += meteoRemoteNodeOptionsHtml_(cfg.source_node_id);
            items += "</select></div>";
            items += "<div class=\"form-row name-local\"><label>Имя</label><input class=\"field name meteo-name\" type=\"text\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"></div>";
            items += "<div class=\"form-row name-remote\" style=\"display:none;\"><label>Имя</label><select class=\"field name meteo-source\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_src\">";
            items += meteoRemoteSensorOptionsHtml_(cfg.source_sensor_id, cfg.source_node_id);
            items += "</select></div>";
            if (remote_label.length())
            {
                items += "<div class=\"muted\">";
                appendHtmlEscaped_(items, remote_label.c_str());
                items += "</div>";
            }
            items += "<div class=\"status-line\">";
            items += ok;
            items += "<span>Давность: ";
            items += age;
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Тип</label><select class=\"field meteo-type\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_type\">";
            appendTypeOption("none", "none", ui_type == MeteoController::SensorType::None);
            appendTypeOption("ds18b20", "ds18b20", ui_type == MeteoController::SensorType::Ds18b20);
            appendTypeOption("dht22", "dht22", ui_type == MeteoController::SensorType::Dht22);
            items += "</select></div>";
            items += "<div class=\"form-row pin-cell\"><label>Пин</label><select class=\"field mini meteo-pin\" data-selected=\"";
            items += pin;
            items += "\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_pin\"></select></div>";
            items += "<div class=\"form-row addr-cell full\"><label>Адрес</label><select class=\"field addr meteo-addr\" name=\"m";
            items += String((unsigned)cfg.id);
            items += "_addr\">";
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
            items += "</div></div>";
        };

        const size_t render_count = meteoLocalRenderCount_();
        const bool can_view_disabled = webSessionIsAdmin_();
        for (size_t i = 0; i < render_count; ++i)
        {
            const auto *cfg = meteo.configByIndex(i);
            const auto *st = meteo.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!webAclCanViewItem_(UsersRegistry::AclController::Meteo, cfg->id))
                continue;
            if (!can_view_disabled && !cfg->enabled)
                continue;
            appendRow(*cfg, *st, cfg->enabled);
        }
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
        return items;
    }


    String meteoPortOptionsJson_() const
    {
        return socketPortOptionsJson_(PortIO::PinType::Sensor);
    }


    String meteoUsedPinsJson_() const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            MeteoController &meteo = _controllers->meteo();
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


    String meteoSensorOptionsHtml_(uint8_t selected_id, uint32_t selected_node_id,
                                   const uint8_t used_local[MeteoController::kSensorCount + 1],
                                   const uint32_t *used_remote, size_t used_remote_count) const
    {
        String out;
        out += "<option value=\"\">-</option>";
        if (!_controllers)
            return out;
        MeteoController &meteo = _controllers->meteo();
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
                label_name = meteoRemoteSensorName_(cfg->source_node_id, cfg->source_sensor_id);
            }
            out += String((unsigned)id);
            if (label_name.length())
            {
                out += ": ";
                appendHtmlEscaped_(out, label_name.c_str());
            }
            out += "</option>";
        }
        if (selected_node_id != 0 && selected_id != 0)
        {
            const String remote_name = meteoRemoteSensorName_(selected_node_id, selected_id);
            out += "<option value=\"";
            out += String((unsigned long)selected_node_id);
            out += ":";
            out += String((unsigned)selected_id);
            out += "\" selected>";
            if (remote_name.length())
                appendHtmlEscaped_(out, remote_name.c_str());
            else
                out += String((unsigned)selected_id);
            out += "</option>";
        }
        return out;
    }


    String meteoRemoteSensorOptionsHtml_(uint8_t selected_id, uint32_t selected_node_id) const
    {
        String out;
        out += "<option value=\"\">-</option>";
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const size_t slots = _stack_slave->remoteMeteoCacheSlots();
            for (size_t i = 0; i < slots; ++i)
            {
                const auto &cache = _stack_slave->remoteMeteoCacheAt(i);
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
                    appendHtmlEscaped_(out, node_label.c_str());
                    out += ": ";
                    if (it.name[0])
                        appendHtmlEscaped_(out, it.name);
                    else
                        out += String((unsigned)it.id);
                    out += "</option>";
                }
            }
            return out;
        }

        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                if (node_id == 0)
                    continue;
                const StackCache::StackMeteoCache *cache = stackCache().meteoCache(node_id);
                if (!cache || !cache->has_data)
                    continue;
                String node_label;
                if (_stack_master)
                {
                    String ip;
                    uint16_t fw_ver = 0;
                    _stack_master->nodeInfo(node_id, node_label, ip, fw_ver);
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
                    appendHtmlEscaped_(out, node_label.c_str());
                    out += ": ";
                    if (it.name[0])
                        appendHtmlEscaped_(out, it.name);
                    else
                        out += String((unsigned)it.id);
                    out += "</option>";
                }
            }
        }
        return out;
    }


    String meteoRemoteNodeOptionsHtml_(uint32_t selected_node_id) const
    {
        String out;
        out += "<option value=\"local\"";
        if (selected_node_id == 0)
            out += " selected";
        out += ">local</option>";
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const size_t slots = _stack_slave->remoteMeteoCacheSlots();
            for (size_t i = 0; i < slots; ++i)
            {
                const auto &cache = _stack_slave->remoteMeteoCacheAt(i);
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
                appendHtmlEscaped_(out, node_label.c_str());
                out += "</option>";
            }
            return out;
        }
        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const size_t count = _stack_master->nodeCount();
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t node_id = _stack_master->nodeIdAt(i);
                if (node_id == 0)
                    continue;
                String node_label;
                String ip;
                uint16_t fw_ver = 0;
                _stack_master->nodeInfo(node_id, node_label, ip, fw_ver);
                if (!node_label.length())
                    node_label = String("Node ") + String((unsigned long)node_id);
                out += "<option value=\"";
                out += String((unsigned long)node_id);
                out += "\"";
                if (selected_node_id == node_id)
                    out += " selected";
                out += ">";
                appendHtmlEscaped_(out, node_label.c_str());
                out += "</option>";
            }
        }
        return out;
    }


    String meteoRemoteLabel_(uint32_t node_id, uint8_t sensor_id) const
    {
        if (node_id == 0 || sensor_id == 0)
            return "";
        String node_label;
        const char *sensor_name = nullptr;
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const auto *cache = _stack_slave->remoteMeteoCache(node_id);
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
        else if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = stackCache().meteoCache(node_id);
            if (cache && cache->has_data)
            {
                String ip;
                uint16_t fw_ver = 0;
                _stack_master->nodeInfo(node_id, node_label, ip, fw_ver);
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


    String meteoRemoteSensorName_(uint32_t node_id, uint8_t sensor_id) const
    {
        if (node_id == 0 || sensor_id == 0)
            return "";
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const auto *cache = _stack_slave->remoteMeteoCache(node_id);
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
        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = stackCache().meteoCache(node_id);
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


    bool meteoRemoteType_(uint32_t node_id, uint8_t sensor_id, MeteoController::SensorType &out) const
    {
        out = MeteoController::SensorType::None;
        if (node_id == 0 || sensor_id == 0)
            return false;
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
        {
            const auto *cache = _stack_slave->remoteMeteoCache(node_id);
            if (!cache || !cache->has_data || !cache->items)
                return false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != sensor_id)
                    continue;
                out = parseMeteoTypeName_(it.type);
                return true;
            }
            return false;
        }
        if (_stack_master && stackRole_() == ConfigsManagerIface::StackRole::Master)
        {
            const auto *cache = stackCache().meteoCache(node_id);
            if (!cache || !cache->has_data)
                return false;
            for (size_t i = 0; i < cache->item_count; ++i)
            {
                const auto &it = cache->items[i];
                if (it.id != sensor_id)
                    continue;
                out = parseMeteoTypeName_(it.type);
                return true;
            }
        }
        return false;
    }


    bool isMeteoSensorActive_(uint8_t id) const
    {
        if (!_controllers)
            return false;
        const auto *cfg = _controllers->meteo().config(id);
        return cfg && cfg->enabled;
    }


    bool isRemoteMeteoSensorActive_(uint32_t node_id, uint8_t id) const
    {
        if (stackRole_() == ConfigsManagerIface::StackRole::Slave)
        {
            if (!_stack_slave)
                return false;
            const auto *cache = _stack_slave->remoteMeteoCache(node_id);
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
        if (!_stack_master || stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        const StackCache::StackMeteoCache *cache = stackCache().meteoCache(node_id);
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


