#pragma once

    String thermoDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"thermo-device\" class=\"field mini\">";
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


    String stackThermoStatusText_(uint32_t node_id) const
    {
        const StackThermoCache *cache = findStackThermoCache_(node_id, false);
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


    bool isStackThermoView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }


    bool requestStackThermo_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackThermoCache *cache = findStackThermoCache_(node_id, true);
        if (!cache)
            return false;
        const uint32_t now = millis();
        if (cache->pending)
            return false;
        if (cache->has_data && (uint32_t)(now - cache->updated_ms) < 1500u)
            return false;
        const uint16_t cmd_id = nextStackCmdId_();
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = cmd_id;
        doc["feature"] = (uint8_t)StackFeature::Thermo;
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
        return true;
    }


    String listStackThermoHtml_(uint32_t node_id)
    {
        StackThermoCache *cache = findStackThermoCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Термо отсутствует</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 620u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
        const StackMeteoCache *meteo_cache = findStackMeteoCache_(node_id, false);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackThermoItem &cfg = cache->items[i];
            const bool mode_off = strcmp(cfg.mode, "off") == 0;
            const bool mode_heat = strcmp(cfg.mode, "heat") == 0;
            const bool mode_cool = strcmp(cfg.mode, "cool") == 0;
            const char *mode_label = "авто";
            if (mode_off)
                mode_label = "выкл";
            else if (mode_heat)
                mode_label = "нагрев";
            else if (mode_cool)
                mode_label = "охлаждение";

            const char *state_label = "ожидание";
            const char *state_class = "status-idle";
            if (cfg.heat_on)
            {
                state_label = "нагрев";
                state_class = "status-heat";
            }
            else if (cfg.cool_on)
            {
                state_label = "охлаждение";
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

            const char *sensor_label = "нет";
            const char *sensor_suffix = "";
            char sensor_buf[16] = {};
            if (cfg.sensor != 0)
            {
                bool found = false;
                if (meteo_cache && meteo_cache->has_data)
                {
                    for (size_t s = 0; s < meteo_cache->item_count; ++s)
                    {
                        const StackMeteoItem &ms = meteo_cache->items[s];
                        if (ms.id == cfg.sensor)
                        {
                            found = true;
                            if (ms.has_temp)
                            {
                                dtostrf(ms.temp_c, 0, 1, sensor_buf);
                                sensor_label = sensor_buf;
                                sensor_suffix = "°C";
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
                    sensor_label = "--";
            }

            items += "<div class=\"tile";
            if (!cfg.enabled)
                items += " disabled";
            items += "\"><div class=\"thermo-left\"><div class=\"thermo-visual\"><div class=\"temp-pill sensor\">Текущая: <span class=\"temp-value\">";
            items += sensor_label;
            items += sensor_suffix;
            items += "</span></div><div class=\"temp-pill target\">Цель: <span class=\"temp-value\">";
            items += String(cfg.target, 1);
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
            items += "<div class=\"status-line\"><span class=\"status-dot ";
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
            items += "<div><div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Термо";
            items += "</strong><span class=\"badge\">ID ";
            items += String((unsigned)cfg.id);
            items += "</span></div>";
            items += "<div class=\"status-line\"><span class=\"badge\">Питание: ";
            items += cfg.power_on ? "on" : "off";
            items += "</span><span class=\"badge\">Режим: ";
            items += mode_label;
            items += "</span></div>";
            items += "<div class=\"status-line\"><span class=\"badge\">Датчик: ";
            if (cfg.sensor != 0)
                items += String((unsigned)cfg.sensor);
            else
                items += "--";
            items += "</span></div>";
            items += "</div></div>";
        }
        return items;
    }


    String listThermoHtml_()
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Thermo unavailable</strong></div>";
        String items;
        items.reserve(16384);
        ThermoController &thermo = _controllers->thermo();
        MeteoController &meteo = _controllers->meteo();
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
            const MeteoController::SensorState *sensor_st = nullptr;
            bool remote_has_temp = false;
            float remote_temp_c = 0.0f;
            if (cfg.sensor_id != ThermoController::kInvalidSensor)
            {
                if (cfg.sensor_node_id != 0)
                {
                    if (stackRole_() == ConfigsManagerIface::StackRole::Slave && _stack_slave)
                    {
                        const auto *sc = _stack_slave->remoteMeteoCache(cfg.sensor_node_id);
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
                        const auto *remote_cache = stackCache().meteoCache(cfg.sensor_node_id);
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

            const char *sensor_label = "нет";
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

            const char *mode_label = "авто";
            if (cfg.mode == ThermoController::Mode::Off)
                mode_label = "выкл";
            else if (cfg.mode == ThermoController::Mode::Heat)
                mode_label = "нагрев";
            else if (cfg.mode == ThermoController::Mode::Cool)
                mode_label = "охлаждение";

            const char *state_label = "ожидание";
            const char *state_class = "status-idle";
            if (st.heat_on)
            {
                state_label = "нагрев";
                state_class = "status-heat";
            }
            else if (st.cool_on)
            {
                state_label = "охлаждение";
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

            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
            items += "\"><div class=\"thermo-left\"><div class=\"thermo-visual\"><div class=\"temp-pill sensor\">Текущая: <span class=\"temp-value\">";
            items += sensor_label;
            items += sensor_suffix;
            items += "</span>";
            items += "</div><div class=\"temp-pill target\">Цель: <span class=\"temp-value\">";
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
            items += "</div><div><div class=\"tile-head\"><div><strong>Термо #";
            items += String((unsigned)cfg.id);
            items += "</strong> <span class=\"badge\">";
            items += mode_label;
            items += "</span>";
            if (!enabled)
                items += " <span class=\"badge\">выкл</span>";
            items += "</div><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-enable\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div><input class=\"field name\" type=\"text\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"><div class=\"form-grid\"><div class=\"form-row\"><label>Активн.</label><label class=\"switch\"><input type=\"checkbox\" class=\"thermo-power\" data-action=\"t";
            items += String((unsigned)cfg.id);
            items += "_power\"";
            const bool ui_power_on = enabled ? st.power_on : false;
            if (ui_power_on)
                items += " checked";
            if (!enabled)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "<span class=\"status-dot ";
            items += state_class;
            items += "\"></span><span class=\"status-value ";
            if (strcmp(state_class, "status-heat") == 0)
                items += "status-text-heat";
            else if (strcmp(state_class, "status-cool") == 0)
                items += "status-text-cool";
            else
                items += "status-text-idle";
            items += "\">";
            items += state_label;
            items += "</span></div><input type=\"hidden\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_en_force\" value=\"\">";
            items += "<div class=\"form-row full\"><label>Датчик</label><select class=\"field mini\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_sensor\">";
            items += meteoSensorOptionsHtml_(cfg.sensor_id, cfg.sensor_node_id, sensor_used, remote_used, remote_used_count);
            items += "</select></div><div class=\"form-row\"><label>Режим</label><select class=\"field mini\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_mode\"><option value=\"off\"";
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
            items += ">auto</option></select></div><div class=\"form-row\"><label>Цель</label><input class=\"field temp\" type=\"number\" step=\"1\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_target\" value=\"";
            items += String((int)(cfg.target_c + 0.5f));
            items += "\"></div><div class=\"form-row\"><label>Гист.</label><input class=\"field temp\" type=\"number\" step=\"1\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_hyst\" value=\"";
            items += String((int)(cfg.hysteresis + 0.5f));
            items += "\"></div><div class=\"form-row\"><label>Нагрев</label><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.heat_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.heat_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_heat\"></select></div><div class=\"form-row\"><label>Охлажд</label><select class=\"field mini thermo-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.cool_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.cool_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_cool\"></select></div><div class=\"form-row\"><label>Кнопка</label><select class=\"field mini thermo-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.button_port != ThermoController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_button\"></select></div></div>";
            items += "<input type=\"hidden\" name=\"t";
            items += String((unsigned)cfg.id);
            items += "_power\" value=\"\">";
            items += "</div></div>";
        };

        const ThermoController::DeviceConfig *first_disabled = nullptr;
        const ThermoController::DeviceState *first_disabled_state = nullptr;
        for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
        {
            const auto *cfg = thermo.configByIndex(i);
            const auto *st = thermo.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (cfg->enabled)
            {
                appendTile(*cfg, *st, true);
            }
            else if (!first_disabled)
            {
                first_disabled = cfg;
                first_disabled_state = st;
            }
        }
        if (first_disabled && first_disabled_state)
            appendTile(*first_disabled, *first_disabled_state, false);
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Thermo empty</strong></div>";
        return items;
    }


    String thermoPortOptionsJson_(PortIO::PinType type) const
    {
        return socketPortOptionsJson_(type);
    }


    String thermoUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            ThermoController &thermo = _controllers->thermo();
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

