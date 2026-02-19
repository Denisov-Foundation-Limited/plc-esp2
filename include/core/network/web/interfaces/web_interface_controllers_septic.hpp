#pragma once

    size_t septicLocalRenderCount_() const
    {
        if (!_controllers)
            return 0;
        SepticController &septic = _controllers->septic();
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

    String septicDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"septic-device\" class=\"field mini\">";
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


    String stackSepticStatusText_(uint32_t node_id) const
    {
        if (_stack_cache)
        {
            const auto *cache = _stack_cache->septicCache(node_id);
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
        const StackSepticCache *cache = findStackSepticCache_(node_id, false);
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


    bool isStackSepticView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }


    bool requestStackSeptic_(uint32_t node_id)
    {
        if (_stack_cache)
            return _stack_cache->requestSeptic(node_id);
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSepticCache *cache = findStackSepticCache_(node_id, true);
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
        doc["feature"] = (uint8_t)StackFeature::Septic;
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


    String listStackSepticHtml_(uint32_t node_id)
    {
        if (_stack_cache)
        {
            const auto *cache = _stack_cache->septicCache(node_id);
            if (!cache || !cache->has_data)
                return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
            if (cache->item_count == 0)
                return "<div class=\"tile empty\"><strong>Септик отсутствует</strong></div>";
            String items;
            size_t reserve = 1024u + cache->item_count * 480u;
            if (reserve < 4096u)
                reserve = 4096u;
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
                if (!webAclCanViewItem_(UsersRegistry::AclController::Septic, cfg.id, node_id))
                    continue;
                if (!webSessionIsAdmin_() && !cfg.enabled)
                    continue;
                const bool warn = cfg.warning;
                const bool alarm = cfg.alarm;
                const char *water_class = "water-low";
                const char *water_level = "20%";
                const char *water_label = "Уровень: 20%";
                if (alarm)
                {
                    water_class = "water-alarm";
                    water_level = "100%";
                    water_label = "Уровень: 100%";
                }
                else if (warn)
                {
                    water_class = "water-warn";
                    water_level = "80%";
                    water_label = "Уровень: 80%";
                }
                items += "<div class=\"tile";
                if (!cfg.enabled)
                    items += " disabled";
                items += "\"><div class=\"septic-visual\"><div class=\"liquid ";
                items += water_class;
                items += "\" style=\"height:";
                items += water_level;
                items += ";\"></div><div class=\"level-label\">";
                items += water_label;
                items += "</div></div><div><div class=\"tile-head\"><div><strong>Септик #";
                items += String((unsigned)cfg.id);
                items += "</strong>";
                if (!cfg.enabled)
                    items += " <span class=\"badge\">выкл</span>";
                items += "</div></div>";
                items += "<div class=\"status-grid\">";
                items += "<div class=\"status-line\"><span class=\"status-dot ";
                items += warn ? "status-on" : "status-off";
                items += "\"></span><span>Датчик предупреждения</span></div>";
                items += "<div class=\"status-line\"><span class=\"status-dot ";
                items += alarm ? "status-bad" : "status-off";
                items += "\"></span><span>Датчик аварии</span></div>";
                items += "</div><div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep";
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
            return items;
        }

        const StackSepticCache *cache = findStackSepticCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Септик отсутствует</strong></div>";
        String items;
        size_t reserve = 1024u + cache->item_count * 480u;
        if (reserve < 4096u)
            reserve = 4096u;
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
            const StackSepticItem &cfg = cache->items[i];
            if (!webAclCanViewItem_(UsersRegistry::AclController::Septic, cfg.id, node_id))
                continue;
            if (!webSessionIsAdmin_() && !cfg.enabled)
                continue;
            const bool warn = cfg.warning;
            const bool alarm = cfg.alarm;
            const char *water_class = "water-low";
            const char *water_level = "20%";
            const char *water_label = "Уровень: 20%";
            if (alarm)
            {
                water_class = "water-alarm";
                water_level = "100%";
                water_label = "Уровень: 100%";
            }
            else if (warn)
            {
                water_class = "water-warn";
                water_level = "80%";
                water_label = "Уровень: 80%";
            }
            items += "<div class=\"tile";
            if (!cfg.enabled)
                items += " disabled";
            items += "\"><div class=\"septic-visual\"><div class=\"liquid ";
            items += water_class;
            items += "\" style=\"height:";
            items += water_level;
            items += ";\"></div><div class=\"level-label\">";
            items += water_label;
            items += "</div></div><div><div class=\"tile-head\"><div><strong>Септик #";
            items += String((unsigned)cfg.id);
            items += "</strong>";
            if (!cfg.enabled)
                items += " <span class=\"badge\">выкл</span>";
            items += "</div></div>";
            items += "<div class=\"status-grid\">";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += warn ? "status-on" : "status-off";
            items += "\"></span><span>Датчик предупреждения</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += alarm ? "status-on" : "status-off";
            items += "\"></span><span>Датчик тревоги</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot status-off\"></span><span>Реле предупреждения: н/д</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot status-off\"></span><span>Реле тревоги: н/д</span></div>";
            items += "</div><div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep";
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
        return items;
    }


    String listSepticHtml_()
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
        String items;
        items.reserve(2048);
        SepticController &septic = _controllers->septic();
        const size_t render_count = septicLocalRenderCount_();
        const bool can_view_disabled = webSessionIsAdmin_();
        for (size_t i = 0; i < render_count; ++i)
        {
            const auto *cfg = septic.configByIndex(i);
            const auto *st = septic.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!webAclCanViewItem_(UsersRegistry::AclController::Septic, cfg->id))
                continue;
            if (!can_view_disabled && !cfg->enabled)
                continue;
            const bool warn = st->warning;
            const bool alarm = st->alarm;
            const bool relay_warn = st->relay_warning;
            const bool relay_alarm = st->relay_alarm;
            const char *water_class = "water-low";
            const char *water_level = "20%";
            const char *water_label = "Уровень: 20%";
            if (alarm)
            {
                water_class = "water-alarm";
                water_level = "100%";
                water_label = "Уровень: 100%";
            }
            else if (warn)
            {
                water_class = "water-warn";
                water_level = "80%";
                water_label = "Уровень: 80%";
            }
            items += "<div class=\"tile";
            if (!cfg->enabled)
                items += " disabled";
            items += "\"><div class=\"septic-visual\"><div class=\"liquid ";
            items += water_class;
            items += "\" style=\"height:";
            items += water_level;
            items += ";\"></div><div class=\"level-label\">";
            items += water_label;
            items += "</div></div><div><div class=\"tile-head\"><div><strong>Септик #";
            items += String((unsigned)cfg->id);
            items += "</strong>";
            if (!cfg->enabled)
                items += " <span class=\"badge\">выкл</span>";
            items += "</div><label class=\"switch\"><input type=\"checkbox\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_en\"";
            if (cfg->enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div><input class=\"field name\" type=\"text\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg->name.c_str());
            items += "\"><div class=\"form-grid\"><div class=\"form-row\"><label>Предупр.</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg->warning_port != SepticController::kInvalidPort)
                items += String((unsigned)cfg->warning_port);
            items += "\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_warn\"></select></div><div class=\"form-row\"><label>Тревога</label><select class=\"field mini septic-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg->alarm_port != SepticController::kInvalidPort)
                items += String((unsigned)cfg->alarm_port);
            items += "\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_alarm\"></select></div><div class=\"form-row\"><label>Реле предупр.</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
            if (cfg->relay_warning != SepticController::kInvalidPort)
                items += String((unsigned)cfg->relay_warning);
            items += "\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_relay_warn\"></select></div><div class=\"form-row\"><label>Реле тревоги</label><select class=\"field mini septic-select\" data-type=\"relay\" data-selected=\"";
            if (cfg->relay_alarm != SepticController::kInvalidPort)
                items += String((unsigned)cfg->relay_alarm);
            items += "\" name=\"sep";
            items += String((unsigned)cfg->id);
            items += "_relay_alarm\"></select></div></div><div class=\"status-grid\"><div class=\"status-line\"><span class=\"status-dot ";
            items += warn ? "status-on" : "status-off";
            items += "\"></span><span>Датчик предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
            items += alarm ? "status-on" : "status-off";
            items += "\"></span><span>Датчик тревоги</span></div><div class=\"status-line\"><span class=\"status-dot ";
            items += relay_warn ? "status-on" : "status-off";
            items += "\"></span><span>Реле предупреждения</span></div><div class=\"status-line\"><span class=\"status-dot ";
            items += relay_alarm ? "status-on" : "status-off";
            items += "\"></span><span>Реле тревоги</span></div></div><div class=\"form-row\" style=\"margin-top:8px;\"><label>Мониторинг</label><label class=\"switch\"><input type=\"checkbox\" class=\"septic-monitor\" data-action=\"sep";
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
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Септик отсутствует</strong></div>";
        return items;
    }


    String septicPortOptionsJson_(PortIO::PinType type) const
    {
        return socketPortOptionsJson_(type);
    }


    String septicUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            auto mark_used = [](bool used[], uint8_t port)
            {
                if (port < PortIO::PORT_COUNT)
                    used[port] = true;
            };
            SepticController &septic = _controllers->septic();
            bool used[PortIO::PORT_COUNT] = {};
            SocketController &sockets = _controllers->sockets();
            ThermoController &thermo = _controllers->thermo();
            TankController &tanks = _controllers->tanks();
            SecurityController &security = _controllers->security();

            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = sockets.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->button_port);
                mark_used(used, cfg->relay_port);
            }
            for (size_t i = 0; i < ThermoController::kDeviceCount; ++i)
            {
                const auto *cfg = thermo.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->heat_port);
                mark_used(used, cfg->cool_port);
                mark_used(used, cfg->button_port);
            }
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = tanks.configByIndex(i);
                if (!cfg)
                    continue;
                mark_used(used, cfg->level_low);
                mark_used(used, cfg->level_mid);
                mark_used(used, cfg->level_full);
                mark_used(used, cfg->relay_valve);
                mark_used(used, cfg->relay_pump);
                mark_used(used, cfg->relay_alarm);
            }
            if (security.sirenPort() != SecurityController::kInvalidPort)
                mark_used(used, security.sirenPort());

            for (size_t i = 0; i < SepticController::kSepticCount; ++i)
            {
                const auto *cfg = septic.configByIndex(i);
                if (!cfg)
                    continue;
                if (type == PortIO::PinType::DInput)
                {
                    const uint8_t w = cfg->warning_port;
                    const uint8_t a = cfg->alarm_port;
                    if (w != SepticController::kInvalidPort)
                        mark_used(used, w);
                    if (a != SepticController::kInvalidPort)
                        mark_used(used, a);
                }
                else if (type == PortIO::PinType::Relay)
                {
                    const uint8_t rw = cfg->relay_warning;
                    const uint8_t ra = cfg->relay_alarm;
                    if (rw != SepticController::kInvalidPort)
                        mark_used(used, rw);
                    if (ra != SepticController::kInvalidPort)
                        mark_used(used, ra);
                }
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

