#pragma once

    String tanksDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"tanks-device\" class=\"field mini\">";
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


    String stackTanksStatusText_(uint32_t node_id) const
    {
        const StackTankCache *cache = findStackTanksCache_(node_id, false);
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


    bool isStackTanksView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }


    bool requestStackTanks_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackTankCache *cache = findStackTanksCache_(node_id, true);
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
        doc["feature"] = (uint8_t)StackFeature::Tanks;
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


    String listStackTanksHtml_(uint32_t node_id)
    {
        StackTankCache *cache = findStackTanksCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Баки отсутствуют</strong></div>";
        String items;
        size_t reserve = 2048u + cache->item_count * 520u;
        if (reserve < 8192u)
            reserve = 8192u;
        items.reserve(reserve);
        for (size_t i = 0; i < cache->item_count; ++i)
        {
            const StackTankItem &cfg = cache->items[i];
            const char *level = "0%";
            const char *level_class = "level-empty";
            unsigned level_pct = 0;
            if (cfg.level_full)
            {
                level = "99%";
                level_class = "level-full";
                level_pct = 99;
            }
            else if (cfg.level_mid)
            {
                level = "66%";
                level_class = "level-mid";
                level_pct = 66;
            }
            else if (cfg.level_low)
            {
                level = "33%";
                level_class = "level-low";
                level_pct = 33;
            }

            items += "<div class=\"tile";
            if (!cfg.enabled)
                items += " disabled";
            items += "\">";
            items += "<div>";
            items += "<div class=\"tank-visual\">";
            items += "<div class=\"tank-fill ";
            items += level_class;
            items += "\" style=\"height:";
            items += String(level_pct);
            items += "%\"></div>";
            items += "<div class=\"tank-label\">";
            items += level;
            items += "</div></div>";
            items += "<div class=\"status-line\">";
            items += "<span class=\"badge\">ID ";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<span class=\"badge\">";
            items += cfg.power_on ? "питание on" : "питание off";
            items += "</span>";
            items += "</div></div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Бак";
            items += "</strong></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += cfg.valve_on ? "status-on" : "status-off";
            items += "\"></span><span>Клапан</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += cfg.pump_on ? "status-on" : "status-off";
            items += "\"></span><span>Насос</span></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += cfg.alarm_on ? "status-on" : "status-off";
            items += "\"></span><span>Авария</span></div>";
            items += "</div></div>";
        }
        return items;
    }



    String listTanksHtml_()
    {
        if (!_controllers)
            return "<div class=\"tile\" style=\"color:#94a3b8\"><strong>Контроллеры недоступны</strong></div>";
        String items;
        items.reserve(16384);
        TankController &tanks = _controllers->tanks();

        auto appendRow = [&](const TankController::TankConfig &cfg, const TankController::TankState &st,
                             bool enabled) {
            const char *level = "0%";
            const char *level_class = "level-empty";
            unsigned level_pct = 0;
            if (st.level_full)
            {
                level = "99%";
                level_class = "level-full";
                level_pct = 99;
            }
            else if (st.level_mid)
            {
                level = "66%";
                level_class = "level-mid";
                level_pct = 66;
            }
            else if (st.level_low)
            {
                level = "33%";
                level_class = "level-low";
                level_pct = 33;
            }

            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
            items += "\">";
            items += "<div>";
            items += "<div class=\"tank-visual\">";
            items += "<div class=\"tank-fill ";
            items += level_class;
            items += "\" style=\"height:";
            items += String(level_pct);
            items += "%\"></div>";
            items += "<div class=\"tank-label\">";
            items += level;
            items += "</div></div>";
            items += "<div class=\"status-line\">";
            items += "<span class=\"badge\">ID ";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<span class=\"badge\">";
            items += enabled ? "вкл" : "выкл";
            items += "</span>";
            items += "</div></div>";
            items += "<div>";
            items += "<div class=\"tile-head\">";
            items += "<strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Бак";
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "</div>";
            items += "<input class=\"field name\" type=\"text\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\">";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Низкий</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.level_low != TankController::kInvalidPort)
                items += String((unsigned)cfg.level_low);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_low\"></select></div>";
            items += "<div class=\"form-row\"><label>Средний</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.level_mid != TankController::kInvalidPort)
                items += String((unsigned)cfg.level_mid);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_mid\"></select></div>";
            items += "<div class=\"form-row\"><label>Полный</label><select class=\"field mini tank-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.level_full != TankController::kInvalidPort)
                items += String((unsigned)cfg.level_full);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_full\"></select></div>";
            items += "<div class=\"form-row\"><label>Клапан</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_valve != TankController::kInvalidPort)
                items += String((unsigned)cfg.relay_valve);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_valve\"></select></div>";
            items += "<div class=\"form-row\"><label>Насос</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_pump != TankController::kInvalidPort)
                items += String((unsigned)cfg.relay_pump);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_pump\"></select></div>";
            items += "<div class=\"form-row\"><label>Индикатор</label><select class=\"field mini tank-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_alarm != TankController::kInvalidPort)
                items += String((unsigned)cfg.relay_alarm);
            items += "\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_alarm\"></select></div>";
            items += "<div>";
            items += "<div class=\"form-row\"><label>Питание</label><label class=\"switch\"><input type=\"checkbox\" class=\"tank-power\" data-action=\"k";
            items += String((unsigned)cfg.id);
            items += "_power\"";
            if (cfg.power_on)
                items += " checked";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label><input type=\"hidden\" name=\"k";
            items += String((unsigned)cfg.id);
            items += "_power\" value=\"";
            items += cfg.power_on ? "on" : "off";
            items += "\"></div>";
            items += "<div class=\"status-row\">";
            items += "<span class=\"status-dot ";
            items += st.pump_on ? "status-on" : "status-off";
            items += "\"></span><span>Насос</span>";
            items += "<span class=\"status-dot ";
            items += st.valve_on ? "status-on" : "status-off";
            items += "\"></span><span>Клапан</span>";
            items += "</div></div>";
            items += "</div></div></div>";
        };

        const TankController::TankConfig *first_disabled = nullptr;
        const TankController::TankState *first_disabled_state = nullptr;
        for (size_t i = 0; i < TankController::kTankCount; ++i)
        {
            const auto *cfg = tanks.configByIndex(i);
            const auto *st = tanks.stateByIndex(i);
            if (!cfg || !st)
                continue;
            if (cfg->enabled)
            {
                appendRow(*cfg, *st, true);
            }
            else if (!first_disabled)
            {
                first_disabled = cfg;
                first_disabled_state = st;
            }
        }
        if (first_disabled && first_disabled_state)
            appendRow(*first_disabled, *first_disabled_state, false);
        if (items.length() == 0)
            items = "<div class=\"tile\" style=\"color:#94a3b8\"><strong>Баки отсутствуют</strong></div>";
        return items;
    }



    String tankPortOptionsJson_(PortIO::PinType type) const
    {
        return socketPortOptionsJson_(type);
    }


    String tankUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            TankController &tanks = _controllers->tanks();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < TankController::kTankCount; ++i)
            {
                const auto *cfg = tanks.configByIndex(i);
                if (!cfg)
                    continue;
                if (type == PortIO::PinType::DInput)
                {
                    const uint8_t low = cfg->level_low;
                    const uint8_t mid = cfg->level_mid;
                    const uint8_t full = cfg->level_full;
                    if (low != TankController::kInvalidPort && low < PortIO::PORT_COUNT)
                        used[low] = true;
                    if (mid != TankController::kInvalidPort && mid < PortIO::PORT_COUNT)
                        used[mid] = true;
                    if (full != TankController::kInvalidPort && full < PortIO::PORT_COUNT)
                        used[full] = true;
                }
                else if (type == PortIO::PinType::Relay)
                {
                    const uint8_t valve = cfg->relay_valve;
                    const uint8_t pump = cfg->relay_pump;
                    const uint8_t alarm = cfg->relay_alarm;
                    if (valve != TankController::kInvalidPort && valve < PortIO::PORT_COUNT)
                        used[valve] = true;
                    if (pump != TankController::kInvalidPort && pump < PortIO::PORT_COUNT)
                        used[pump] = true;
                    if (alarm != TankController::kInvalidPort && alarm < PortIO::PORT_COUNT)
                        used[alarm] = true;
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

