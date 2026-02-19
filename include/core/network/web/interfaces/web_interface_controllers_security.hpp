#pragma once

    size_t securityLocalRenderCount_() const
    {
        if (!_controllers)
            return 0;
        SecurityController &sec = _controllers->security();
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

    String securityDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"security-device\" class=\"field mini\">";
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


    String stackSecurityStatusText_(uint32_t node_id) const
    {
        if (_stack_cache)
        {
            const auto *cache = _stack_cache->securityCache(node_id);
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
        const StackSecurityCache *cache = findStackSecurityCache_(node_id, false);
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


    String stackSecurityTitle_(uint32_t node_id) const
    {
        String title = "Датчики";
        if (!_stack_master || node_id == 0)
            return title;
        const size_t count = _stack_master->nodeCount();
        for (size_t i = 0; i < count; ++i)
        {
            if (_stack_master->nodeIdAt(i) == node_id)
            {
                String name = _stack_master->nodeNameAt(i);
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


    bool isStackSecurityView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }


    bool requestStackSecurity_(uint32_t node_id)
    {
        if (_stack_cache)
            return _stack_cache->requestSecurity(node_id);
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSecurityCache *cache = findStackSecurityCache_(node_id, true);
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
        doc["feature"] = (uint8_t)StackFeature::Security;
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


    String listSecuritySensorsHtml_()
    {
        if (!_controllers)
            return "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></td></tr>";
        String items;
        items.reserve(16384);
        SecurityController &sec = _controllers->security();

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
            appendHtmlEscaped_(items, cfg.name.c_str());
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
        const bool can_view_disabled = webSessionIsAdmin_();
        for (size_t i = 0; i < SecurityController::kSensorCount; ++i)
        {
            const auto *cfg = sec.configByIndex(i);
            const auto *st = sec.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!webAclCanViewItem_(UsersRegistry::AclController::Security, cfg->id))
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
            items = "<tr><td colspan=\"7\" style=\"color:#94a3b8\"><strong>Датчики отсутствуют</strong></td></tr>";
        return items;
    }


    String listSecuritySensorsTiles_(uint8_t start_idx, uint8_t end_idx)
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
        if (end_idx < start_idx)
            end_idx = start_idx;

        String items;
        items.reserve(16384);
        SecurityController &sec = _controllers->security();

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
            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
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
                appendHtmlEscaped_(items, cfg.name);
            else
                items += String(F("Датчик #")) + String((unsigned)cfg.id);
            items += "</strong><label class=\"switch\"><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<input class=\"field name\" type=\"text\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\">";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            if (!enabled)
                items += "status-off";
            else if (detected)
                items += "status-bad";
            else
                items += "status-on";
            items += "\"></span><span class=\"status-text\">";
            if (!enabled)
                items += "Отключен";
            else if (detected)
                items += "Сработал";
            else
                items += "Активен";
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Тип</label><select class=\"field mini\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_type\">";
            appendTypeOption(items, "pir", "pir", cfg.type == SecurityController::SensorType::Pir);
            appendTypeOption(items, "reed", "reed", cfg.type == SecurityController::SensorType::Reed);
            items += "</select></div>";
            items += "<div class=\"form-row\"><label>Порт</label><select class=\"field mini security-port\" data-type=\"dinput\" data-selected=\"";
            if (cfg.port != SecurityController::kInvalidPort)
                items += String((unsigned)cfg.port);
            items += "\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_port\"></select></div>";
            items += "<div class=\"form-row\"><label>Тихий</label><label class=\"switch\"><input type=\"checkbox\" name=\"sec";
            items += String((unsigned)cfg.id);
            items += "_silent\"";
            if (cfg.silent)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div></div></div>";
        };

        const size_t render_count = securityLocalRenderCount_();
        if (render_count == 0)
            return "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
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
            if (!webAclCanViewItem_(UsersRegistry::AclController::Security, cfg->id))
                continue;
            if (!webSessionIsAdmin_() && !cfg->enabled)
                continue;
            appendTile(*cfg, *st);
        }
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
        return items;
    }


    String listStackSecuritySensorsTiles_(uint32_t node_id)
    {
        if (_stack_cache)
        {
            const auto *cache = _stack_cache->securityCache(node_id);
            if (!cache || !cache->has_data)
                return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
            if (cache->item_count == 0)
                return "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
            String items;
            size_t reserve = 2048u + cache->item_count * 420u;
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
                if (!webAclCanViewItem_(UsersRegistry::AclController::Security, cfg.id, node_id))
                    continue;
                if (!webSessionIsAdmin_() && !cfg.enabled)
                    continue;
                items += "<div class=\"tile\">";
                items += "<div class=\"sock-visual\">";
                items += "<span class=\"badge\">#";
                items += String((unsigned)cfg.id);
                items += "</span>";
                items += "<svg class=\"sock-icon ";
                if (cfg.detect)
                    items += "alert";
                else
                    items += "off";
                items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
                if (strcmp(cfg.type, "reed") == 0)
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
                items += "</svg>";
                items += "</div>";
                items += "<div>";
                items += "<div class=\"tile-head\"><strong>";
                if (cfg.name[0])
                    appendHtmlEscaped_(items, cfg.name);
                else
                    items += String(F("Датчик #")) + String((unsigned)cfg.id);
                items += "</strong>";
                if (cfg.silent)
                    items += "<span class=\"badge\">silent</span>";
                items += "</div>";
                items += "<div class=\"status-line\"><span class=\"status-dot ";
                if (!cfg.enabled)
                    items += "status-off";
                else if (cfg.detect)
                    items += "status-bad";
                else
                    items += "status-on";
                items += "\"></span><span class=\"status-text\">";
                if (!cfg.enabled)
                    items += "Отключен";
                else if (cfg.detect)
                    items += "Сработал";
                else
                    items += "Активен";
                items += "</span></div>";
                items += "<div class=\"form-grid\">";
                items += "<div class=\"form-row\"><label>Тип</label><div class=\"field mini\">";
                if (cfg.type[0])
                    appendHtmlEscaped_(items, cfg.type);
                else
                    items += "--";
                items += "</div></div>";
                items += "<div class=\"form-row\"><label>Порт</label><div class=\"field mini\">";
                if (cfg.port != SecurityController::kInvalidPort)
                    items += String((unsigned)cfg.port);
                else
                    items += "--";
                items += "</div></div>";
                items += "</div></div></div>";
            }
            return items;
        }

        const StackSecurityCache *cache = findStackSecurityCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Датчики отсутствуют</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 420u;
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
            const StackSecuritySensorItem &cfg = cache->items[i];
            if (!webAclCanViewItem_(UsersRegistry::AclController::Security, cfg.id, node_id))
                continue;
            if (!webSessionIsAdmin_() && !cfg.enabled)
                continue;
            items += "<div class=\"tile\">";
            items += "<div class=\"sock-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sock-icon ";
            if (cfg.detect)
                items += "alert";
            else
                items += "off";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            if (strcmp(cfg.type, "reed") == 0)
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
            items += "</svg>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Датчик";
            items += "</strong></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += cfg.detect ? "status-bad" : "status-off";
            items += "\"></span>";
            items += "<span class=\"status-text\">";
            items += cfg.detect ? "Сработал" : "ОК";
            items += "</span></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Тип</label><input class=\"field mini\" type=\"text\" value=\"";
            items += cfg.type;
            items += "\" readonly></div>";
            items += "<div class=\"form-row\"><label>Порт</label><input class=\"field mini\" type=\"text\" value=\"";
            if (cfg.port != SecurityController::kInvalidPort)
                items += String((unsigned)cfg.port);
            else
                items += "--";
            items += "\" readonly></div>";
            items += "<div class=\"form-row\"><label>Тихий</label><input class=\"field mini\" type=\"text\" value=\"";
            items += cfg.silent ? "yes" : "no";
            items += "\" readonly></div>";
            items += "</div>";
            items += "</div></div>";
        }
        return items;
    }


    String securityPortOptionsJson_() const
    {
        return socketPortOptionsJson_(PortIO::PinType::DInput);
    }


    String securityUsedPinsJson_() const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            SecurityController &sec = _controllers->security();
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

