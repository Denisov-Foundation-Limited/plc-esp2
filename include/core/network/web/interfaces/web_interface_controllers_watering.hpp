#pragma once

    size_t wateringLocalRenderCount_() const
    {
        if (!_controllers)
            return 0;
        WateringController &watering = _controllers->watering();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < WateringController::kRuleCount; ++i)
        {
            const auto *cfg = watering.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return WateringController::kRuleCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > WateringController::kRuleCount ? WateringController::kRuleCount : count;
    }

    String wateringDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"watering-device\" class=\"field mini\">";
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


    String stackWateringStatusText_(uint32_t node_id) const
    {
        const StackWateringCache *cache = findStackWateringCache_(node_id, false);
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


    bool isStackWateringView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }


    bool requestStackWatering_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackWateringCache *cache = findStackWateringCache_(node_id, true);
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
        doc["feature"] = (uint8_t)StackFeature::Watering;
        doc["action"] = "get";
        JsonObject params = doc["params"].to<JsonObject>();
        params["offset"] = 0;
        params["limit"] = (uint16_t)WateringController::kRuleCount;
        char payload[128] = {};
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


    String listStackWateringHtml_(uint32_t node_id)
    {
        StackWateringCache *cache = findStackWateringCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Правила отсутствуют</strong></div>";
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
            const StackWateringItem &cfg = cache->items[i];
            if (!webAclCanViewItem_(UsersRegistry::AclController::Watering, cfg.id, node_id))
                continue;
            if (!webSessionIsAdmin_() && !cfg.enabled)
                continue;
            const char *state_label = cfg.active ? "активно" : (cfg.paused ? "пауза" : "ожидание");
            items += "<div class=\"tile\" data-active=\"";
            items += cfg.active ? "1\">" : "0\">";
            items += "<div class=\"watering-visual\"><div class=\"tile-head\"><strong>Правило ";
            items += String((unsigned)cfg.id);
            items += "</strong><span class=\"badge\">";
            items += cfg.enabled ? "вкл" : "выкл";
            items += "</span></div><svg class=\"watering-icon\" viewBox=\"0 0 24 24\" fill=\"currentColor\" aria-hidden=\"true\"><path d=\"M12 2c-2.3 3.5-6 7.4-6 11a6 6 0 0 0 12 0c0-3.6-3.7-7.5-6-11zm0 18a4 4 0 0 1-4-4c0-2.2 2.3-5.2 4-7.7 1.7 2.5 4 5.5 4 7.7a4 4 0 0 1-4 4z\"/></svg><div class=\"status-line\"><span class=\"muted\">Состояние</span><span class=\"status-value\">";
            items += state_label;
            items += "</span></div></div>";
            items += "<div><div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Правило полива";
            items += "</strong></div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Монитор</label><div class=\"field mini\">";
            items += cfg.status ? "вкл" : "выкл";
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Кран</label><div class=\"field mini\">";
            if (cfg.port != WateringController::kInvalidPort)
                items += String((unsigned)cfg.port);
            else
                items += "--";
            items += "</div></div>";
            items += "<div class=\"form-row full\"><label>Дни</label><div class=\"field\">";
            static const uint8_t kWeekdayMap[7] = {2, 3, 4, 5, 6, 7, 1};
            static const char *kWeekdayLabels[7] = {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
            bool any_day = false;
            for (size_t wi = 0; wi < 7; ++wi)
            {
                const uint8_t dow = kWeekdayMap[wi];
                if (cfg.weekdays_mask & (uint8_t)(1u << (dow - 1u)))
                {
                    if (any_day)
                        items += " ";
                    items += kWeekdayLabels[wi];
                    any_day = true;
                }
            }
            if (!any_day)
                items += "--";
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Время</label><div class=\"field mini\">";
            if (cfg.weekdays_mask && cfg.duration_sec && cfg.hour <= 23 && cfg.minute <= 59)
            {
                char buf[8] = {};
                snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)cfg.hour, (unsigned)cfg.minute);
                items += buf;
            }
            else
            {
                items += "--";
            }
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Бак</label><div class=\"field mini\">";
            if (cfg.tank_id)
                items += String((unsigned)cfg.tank_id);
            else
                items += "--";
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Длит. (мин)</label><div class=\"field mini\">";
            if (cfg.duration_sec)
                items += String((unsigned long)((cfg.duration_sec + 59) / 60));
            else
                items += "--";
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Время 2</label><div class=\"field mini\">";
            if (cfg.weekdays_mask && cfg.duration2_sec && cfg.hour2 <= 23 && cfg.minute2 <= 59)
            {
                char buf2[8] = {};
                snprintf(buf2, sizeof(buf2), "%02u:%02u", (unsigned)cfg.hour2, (unsigned)cfg.minute2);
                items += buf2;
            }
            else
            {
                items += "--";
            }
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Длит.2 (мин)</label><div class=\"field mini\">";
            if (cfg.duration2_sec)
                items += String((unsigned long)((cfg.duration2_sec + 59) / 60));
            else
                items += "--";
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Время 3</label><div class=\"field mini\">";
            if (cfg.weekdays_mask && cfg.duration3_sec && cfg.hour3 <= 23 && cfg.minute3 <= 59)
            {
                char buf3[8] = {};
                snprintf(buf3, sizeof(buf3), "%02u:%02u", (unsigned)cfg.hour3, (unsigned)cfg.minute3);
                items += buf3;
            }
            else
            {
                items += "--";
            }
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Длит.3 (мин)</label><div class=\"field mini\">";
            if (cfg.duration3_sec)
                items += String((unsigned long)((cfg.duration3_sec + 59) / 60));
            else
                items += "--";
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Продолжать</label><div class=\"field mini\">";
            items += cfg.resume_after_refill ? "вкл" : "выкл";
            items += "</div></div>";
            items += "<div class=\"form-row full\"><label>Уровень >=</label><div class=\"field mini\">";
            if (cfg.tank_id && cfg.resume_after_refill)
            {
                const char *level = "low";
                if (cfg.resume_level == 1)
                    level = "mid";
                else if (cfg.resume_level == 2)
                    level = "full";
                items += level;
            }
            else
            {
                items += "--";
            }
            items += "</div></div>";
            items += "</div></div></div>";
        }
        return items;
    }



    String listWateringHtml_()
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
        String items;
        items.reserve(16384);
        WateringController &watering = _controllers->watering();
        auto appendRule = [&](const WateringController::RuleConfig &cfg, const WateringController::RuleState &st)
        {
            const char *state_label = st.active ? "активно" : (st.paused ? "пауза" : "ожидание");
            items += "<div class=\"tile\" data-active=\"";
            items += st.active ? "1\">" : "0\">";
            items += "<div class=\"watering-visual\"><div class=\"tile-head\"><strong>Правило ";
            items += String((unsigned)cfg.id);
            items += "</strong><span class=\"badge\">";
            items += cfg.enabled ? "вкл" : "выкл";
            items += "</span></div><svg class=\"watering-icon\" viewBox=\"0 0 24 24\" fill=\"currentColor\" aria-hidden=\"true\"><path d=\"M12 2c-2.3 3.5-6 7.4-6 11a6 6 0 0 0 12 0c0-3.6-3.7-7.5-6-11zm0 18a4 4 0 0 1-4-4c0-2.2 2.3-5.2 4-7.7 1.7 2.5 4 5.5 4 7.7a4 4 0 0 1-4 4z\"/></svg><div class=\"status-line\"><span class=\"muted\">Состояние</span><span class=\"status-value\">";
            items += state_label;
            items += "</span></div></div>";
            items += "<div><div class=\"tile-head\"><strong>";
            if (cfg.name.length())
                appendHtmlEscaped_(items, cfg.name.c_str());
            else
                items += "Правило полива";
            items += "</strong><input type=\"hidden\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_en\" value=\"0\"><label class=\"switch\"><input type=\"checkbox\" value=\"1\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (cfg.enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<input class=\"field name\" type=\"text\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"><div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Монитор</label><label class=\"switch\"><input type=\"checkbox\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_status\"";
            if (st.status)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<div class=\"form-row\"><label>Кран</label><select class=\"field mini watering-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.port != WateringController::kInvalidPort)
                items += String((unsigned)cfg.port);
            items += "\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_port\"></select></div>";
            items += "<div class=\"form-row full\"><label>Дни</label><div class=\"weekday-group\">";
            static const uint8_t kWeekdayMap[7] = {2, 3, 4, 5, 6, 7, 1};
            static const char *kWeekdayLabels[7] = {"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
            for (size_t wi = 0; wi < 7; ++wi)
            {
                const uint8_t dow = kWeekdayMap[wi];
                items += "<label class=\"weekday-item\"><input type=\"checkbox\" name=\"w";
                items += String((unsigned)cfg.id);
                items += "_d";
                items += String((unsigned)dow);
                items += "\"";
                if (cfg.weekdays_mask & (uint8_t)(1u << (dow - 1u)))
                    items += " checked";
                items += "><span>";
                items += kWeekdayLabels[wi];
                items += "</span></label>";
            }
            items += "</div></div>";
            items += "<div class=\"form-row\"><label>Время</label><input class=\"field mini\" type=\"time\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_time\" value=\"";
            if (cfg.weekdays_mask && cfg.duration_sec && cfg.hour <= 23 && cfg.minute <= 59)
            {
                char buf[8] = {};
                snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)cfg.hour, (unsigned)cfg.minute);
                items += buf;
            }
            items += "\"></div>";
            items += "<div class=\"form-row\"><label>Длит. (мин)</label><input class=\"field mini\" type=\"number\" min=\"1\" step=\"1\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_dur\" value=\"";
            if (cfg.duration_sec)
                items += String((unsigned long)((cfg.duration_sec + 59) / 60));
            items += "\"></div>";
            items += "<div class=\"form-row\"><label>Время 2</label><input class=\"field mini\" type=\"time\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_time2\" value=\"";
            if (cfg.weekdays_mask && cfg.duration2_sec && cfg.hour2 <= 23 && cfg.minute2 <= 59)
            {
                char buf2[8] = {};
                snprintf(buf2, sizeof(buf2), "%02u:%02u", (unsigned)cfg.hour2, (unsigned)cfg.minute2);
                items += buf2;
            }
            items += "\"></div>";
            items += "<div class=\"form-row\"><label>Длит.2 (мин)</label><input class=\"field mini\" type=\"number\" min=\"0\" step=\"1\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_dur2\" value=\"";
            if (cfg.duration2_sec)
                items += String((unsigned long)((cfg.duration2_sec + 59) / 60));
            items += "\"></div>";
            items += "<div class=\"form-row\"><label>Время 3</label><input class=\"field mini\" type=\"time\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_time3\" value=\"";
            if (cfg.weekdays_mask && cfg.duration3_sec && cfg.hour3 <= 23 && cfg.minute3 <= 59)
            {
                char buf3[8] = {};
                snprintf(buf3, sizeof(buf3), "%02u:%02u", (unsigned)cfg.hour3, (unsigned)cfg.minute3);
                items += buf3;
            }
            items += "\"></div>";
            items += "<div class=\"form-row\"><label>Длит.3 (мин)</label><input class=\"field mini\" type=\"number\" min=\"0\" step=\"1\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_dur3\" value=\"";
            if (cfg.duration3_sec)
                items += String((unsigned long)((cfg.duration3_sec + 59) / 60));
            items += "\"></div>";
            items += "<div class=\"form-row\"><label>Бак</label><select class=\"field mini watering-select\" data-type=\"tank\" data-selected=\"";
            if (cfg.tank_id)
                items += String((unsigned)cfg.tank_id);
            items += "\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_tank\"></select></div>";
            items += "<div class=\"form-row tank-dependent\"><label>Продолжать</label><label class=\"switch\"><input type=\"checkbox\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_resume\"";
            if (cfg.resume_after_refill)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "<div class=\"form-row full tank-dependent resume-dependent\"><label>Уровень >=</label><select class=\"field mini\" name=\"w";
            items += String((unsigned)cfg.id);
            items += "_resume_level\"><option value=\"low\"";
            if (cfg.resume_level == 0)
                items += " selected";
            items += ">low</option><option value=\"mid\"";
            if (cfg.resume_level == 1)
                items += " selected";
            items += ">mid</option><option value=\"full\"";
            if (cfg.resume_level == 2)
                items += " selected";
            items += ">full</option></select></div>";
            items += "</div></div></div>";
        };

        const size_t render_count = wateringLocalRenderCount_();
        const bool can_view_disabled = webSessionIsAdmin_();
        for (size_t i = 0; i < render_count; ++i)
        {
            const auto *cfg = watering.configByIndex(i);
            const auto *st = watering.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (!webAclCanViewItem_(UsersRegistry::AclController::Watering, cfg->id))
                continue;
            if (!can_view_disabled && !cfg->enabled)
                continue;
            appendRule(*cfg, *st);
        }
        return items;
    }


    String wateringPortOptionsJson_() const
    {
        return socketPortOptionsJson_(PortIO::PinType::Relay);
    }


    String wateringTankOptionsJson_() const
    {
        String out;
        out.reserve(256);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            const TankController &tanks = _controllers->tanks();
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = tanks.configByIndex(i);
                if (!cfg || !cfg->enabled)
                    continue;
                if (!first)
                    out += ",";
                out += "{\"v\":";
                out += String((unsigned)cfg->id);
                out += ",\"l\":\"";
                if (cfg->name.length())
                    appendJsonEscaped_(out, cfg->name);
                else
                    out += String("Tank #") + String((unsigned)cfg->id);
                out += "\"}";
                first = false;
            }
        }
        out += "]";
        return out;
    }



