#pragma once

    size_t socketsLocalRenderCount_() const
    {
        if (!_controllers)
            return 0;
        SocketController &sockets = _controllers->sockets();
        size_t last_enabled_idx = SIZE_MAX;
        for (size_t i = 0; i < SocketController::kSocketCount; ++i)
        {
            const auto *cfg = sockets.configByIndex(i);
            if (cfg && cfg->enabled)
                last_enabled_idx = i;
        }
        if (last_enabled_idx == SIZE_MAX)
            return SocketController::kSocketCount ? 1u : 0u;
        const size_t count = last_enabled_idx + 2u;
        return count > SocketController::kSocketCount ? SocketController::kSocketCount : count;
    }

    String listSocketsHtml_(uint8_t start_id, uint8_t end_id)
    {
        if (!_controllers)
            return "<div class=\"tile empty\"><strong>Контроллеры недоступны</strong></div>";
        String items;
        if (start_id == 0)
            start_id = 1;
        if (end_id < start_id)
            end_id = start_id;
        size_t reserve = 2048u + (size_t)(end_id - start_id + 1) * 700u;
        if (reserve < 16384u)
            reserve = 16384u;
        items.reserve(reserve);
        SocketController &sockets = _controllers->sockets();
        bool tmp_state = false;
        auto appendRow = [&](const SocketController::SocketConfig &cfg, bool enabled) {
            const bool can_edit = webSessionIsAdmin_();
            const bool can_control = webAclCanControlItem_(UsersRegistry::AclController::Sockets, cfg.id);
            const bool on = enabled && sockets.relayState(cfg.id, tmp_state) ? tmp_state : false;
            items += "<div class=\"tile";
            if (!enabled)
                items += " disabled";
            items += "\">";
            items += "<div class=\"sock-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sock-icon ";
            items += on ? "on" : "off";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            items += "<path fill=\"currentColor\" d=\"M16 10h32c3.3 0 6 2.7 6 6v32c0 3.3-2.7 6-6 6H16c-3.3 0-6-2.7-6-6V16c0-3.3 2.7-6 6-6zm0 4c-1.1 0-2 .9-2 2v32c0 1.1.9 2 2 2h32c1.1 0 2-.9 2-2V16c0-1.1-.9-2-2-2H16z\"/>";
            items += "<circle cx=\"24\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
            items += "<circle cx=\"40\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
            items += "<rect x=\"28\" y=\"36\" width=\"8\" height=\"10\" rx=\"2\" fill=\"currentColor\"/>";
            items += "</svg>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\">";
            items += "<strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Розетка";
            items += "</strong>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-enable\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_en\"";
            if (enabled)
                items += " checked";
            if (!can_edit)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label>";
            items += "</div>";
            items += "<input class=\"field name\" type=\"text\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_name\" value=\"";
            appendHtmlEscaped_(items, cfg.name.c_str());
            items += "\"";
            if (!can_edit)
                items += " disabled";
            items += ">";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += on ? "status-on" : "status-off";
            items += "\"></span>";
            items += "<span class=\"status-text\">";
            items += on ? "Включена" : "Выключена";
            items += "</span>";
            items += "</div>";
            items += "<div class=\"form-grid\">";
            items += "<div class=\"form-row\"><label>Кнопка</label>";
            items += "<select class=\"field mini socket-select\" data-type=\"dinput\" data-selected=\"";
            if (cfg.button_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.button_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_btn\"";
            if (!can_edit)
                items += " disabled";
            items += "></select></div>";
            items += "<div class=\"form-row\"><label>Реле</label>";
            items += "<select class=\"field mini socket-select\" data-type=\"relay\" data-selected=\"";
            if (cfg.relay_port != SocketController::kInvalidPort)
                items += String((unsigned)cfg.relay_port);
            items += "\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_relay\"";
            if (!can_edit)
                items += " disabled";
            items += "></select></div>";
            items += "<div class=\"form-row\"><label>Перекл.</label>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-toggle\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\"";
            if (on)
                items += " checked";
            if (!enabled || !can_control)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div>";
            items += "<input type=\"hidden\" name=\"s";
            items += String((unsigned)cfg.id);
            items += "_action\" value=\"\">";
            items += "</div></div>";
        };

        const size_t render_count = socketsLocalRenderCount_();
        const bool can_view_disabled = webSessionIsAdmin_();
        for (size_t i = 0; i < render_count; ++i)
        {
            const auto *cfg = sockets.configByIndex(i);
            if (!cfg)
                continue;
            if (!webAclCanViewItem_(UsersRegistry::AclController::Sockets, cfg->id))
                continue;
            if (cfg->id < start_id || cfg->id > end_id)
                continue;
            if (!can_view_disabled && !cfg->enabled)
                continue;
            appendRow(*cfg, cfg->enabled);
        }
        if (items.length() == 0)
            items = "<div class=\"tile empty\"><strong>Розетки отсутствуют</strong></div>";
        return items;
    }


    String listStackSocketsHtml_(uint32_t node_id)
    {
        StackSocketsCache *cache = findStackSocketsCache_(node_id, false);
        if (!cache || !cache->has_data)
            return "<div class=\"tile empty\"><strong>Ожидаем данные со слейва</strong></div>";
        if (cache->item_count == 0)
            return "<div class=\"tile empty\"><strong>Розетки отсутствуют</strong></div>";
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
            const StackSocketItem &cfg = cache->items[i];
            if (!webAclCanViewItem_(UsersRegistry::AclController::Sockets, cfg.id, node_id))
                continue;
            if (!webSessionIsAdmin_() && !cfg.enabled)
                continue;
            const bool can_control = webAclCanControlItem_(UsersRegistry::AclController::Sockets, cfg.id, node_id);
            const bool on = cfg.state;
            items += "<div class=\"tile\">";
            items += "<div class=\"sock-visual\">";
            items += "<span class=\"badge\">#";
            items += String((unsigned)cfg.id);
            items += "</span>";
            items += "<svg class=\"sock-icon ";
            items += on ? "on" : "off";
            items += "\" viewBox=\"0 0 64 64\" aria-hidden=\"true\">";
            items += "<path fill=\"currentColor\" d=\"M16 10h32c3.3 0 6 2.7 6 6v32c0 3.3-2.7 6-6 6H16c-3.3 0-6-2.7-6-6V16c0-3.3 2.7-6 6-6zm0 4c-1.1 0-2 .9-2 2v32c0 1.1.9 2 2 2h32c1.1 0 2-.9 2-2V16c0-1.1-.9-2-2-2H16z\"/>";
            items += "<circle cx=\"24\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
            items += "<circle cx=\"40\" cy=\"26\" r=\"4\" fill=\"currentColor\"/>";
            items += "<rect x=\"28\" y=\"36\" width=\"8\" height=\"10\" rx=\"2\" fill=\"currentColor\"/>";
            items += "</svg>";
            items += "</div>";
            items += "<div>";
            items += "<div class=\"tile-head\"><strong>";
            if (cfg.name[0])
                appendHtmlEscaped_(items, cfg.name);
            else
                items += "Розетка";
            items += "</strong></div>";
            items += "<div class=\"status-line\"><span class=\"status-dot ";
            items += on ? "status-on" : "status-off";
            items += "\"></span>";
            items += "<span class=\"status-text\">";
            items += on ? "Включена" : "Выключена";
            items += "</span></div>";
            items += "<div class=\"form-row\"><label>Перекл.</label>";
            items += "<label class=\"switch\"><input type=\"checkbox\" class=\"socket-toggle\" data-id=\"";
            items += String((unsigned)cfg.id);
            items += "\"";
            if (on)
                items += " checked";
            if (!can_control)
                items += " disabled";
            items += "><span class=\"track\"><span class=\"knob\"></span></span></label></div>";
            items += "</div></div>";
        }
        return items;
    }


    String socketsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"sockets-device\" class=\"field mini\">";
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


    String stackSocketsStatusText_(uint32_t node_id) const
    {
        const StackSocketsCache *cache = findStackSocketsCache_(node_id, false);
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


    bool isStackSocketsView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }


    void handleStackSocketsToggle_(AsyncWebServerRequest *request, uint32_t node_id, bool set_cookie)
    {
        if (!_stack_master)
        {
            sendText_(request, 400, "text/plain", "Stack master missing", set_cookie);
            return;
        }
        const String id_str = paramValueAny_(request, "id");
        if (!id_str.length())
        {
            sendText_(request, 400, "text/plain", "Missing id", set_cookie);
            return;
        }
        const uint8_t id = (uint8_t)id_str.toInt();
        if (id == 0)
        {
            sendText_(request, 400, "text/plain", "Invalid id", set_cookie);
            return;
        }
        String action = paramValueAny_(request, "action");
        action.trim();
        action.toLowerCase();
        StackSocketsCache *cache = findStackSocketsCache_(node_id, false);
        StackSocketItem *item = cache ? findStackSocketItem_(*cache, id) : nullptr;

        if (action == "state")
        {
            if (!cache || !cache->has_data ||
                (uint32_t)(millis() - cache->updated_ms) > 1500u)
            {
                requestStackSockets_(node_id);
            }
            if (!item)
            {
                sendText_(request, 200, "text/plain", "unknown", set_cookie);
                return;
            }
            sendText_(request, 200, "text/plain", item->state ? "on" : "off", set_cookie);
            return;
        }

        StaticJsonDocument<192> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Sockets;
        doc["action"] = "set";
        JsonObject params = doc["params"].to<JsonObject>();
        JsonArray items = params["items"].to<JsonArray>();
        JsonObject o = items.add<JsonObject>();
        o["id"] = id;

        bool desired_known = false;
        bool desired = false;
        if (action == "on" || action == "off")
        {
            desired = (action == "on");
            desired_known = true;
            o["state"] = desired;
        }
        else
        {
            o["toggle"] = true;
            if (item)
            {
                desired = !item->state;
                desired_known = true;
            }
        }

        char payload[160] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0 || !_stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                               (const uint8_t *)payload, len))
        {
            sendText_(request, 400, "text/plain", "Send failed", set_cookie);
            return;
        }

        if (desired_known && item)
            item->state = desired;
        sendText_(request, 200, "text/plain", desired_known ? (desired ? "on" : "off") : "pending", set_cookie);
    }


    bool requestStackSockets_(uint32_t node_id)
    {
        if (!_stack_master)
            return false;
        if (stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        StackSocketsCache *cache = findStackSocketsCache_(node_id, true);
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
        doc["feature"] = (uint8_t)StackFeature::Sockets;
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


    String socketPortOptionsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        uint8_t next_id[11] = {};
        for (uint8_t i = 0; i < PortIO::PORT_COUNT; ++i)
        {
            const auto &p = ActiveBoardProfile::PORTS[i];
            if (p.caps == Cap::None || p.type != type)
                continue;
            if (p.backend == PortIO::Backend::Extender)
            {
                if (!_ext)
                    continue;
                const uint8_t dev = p.u.ext.dev;
                const auto *devs = _ext->devs();
                if (!devs || dev >= _ext->devCount())
                    continue;
                if (devs[dev].type != Extender::Type::MCP23017)
                    continue;
                if (!_ext->isPresent(dev))
                    continue;
            }
            uint8_t ui_id = p.ui_id;
            if (ui_id == 0)
            {
                const uint8_t loc = locationIndex_(p.location);
                if (loc < 11)
                    ui_id = ++next_id[loc];
                else
                    ui_id = 0;
            }
            if (!first)
                out += ",";
            out += "{\"v\":";
            out += String((unsigned)i);
            out += ",\"l\":\"";
            appendPortLabel_(out, type, p, ui_id);
            out += "\"}";
            first = false;
        }
        out += "]";
        return out;
    }


    String socketUsedPortsJson_(PortIO::PinType type) const
    {
        String out;
        out.reserve(128);
        out += "[";
        bool first = true;
        if (_controllers)
        {
            SocketController &sockets = _controllers->sockets();
            bool used[PortIO::PORT_COUNT] = {};
            for (size_t i = 0; i < SocketController::kSocketCount; ++i)
            {
                const auto *cfg = sockets.configByIndex(i);
                if (!cfg)
                    continue;
                const uint8_t btn = cfg->button_port;
                const uint8_t relay = cfg->relay_port;
                if (btn != SocketController::kInvalidPort && btn < PortIO::PORT_COUNT)
                    used[btn] = true;
                if (relay != SocketController::kInvalidPort && relay < PortIO::PORT_COUNT)
                    used[relay] = true;
            }
            for (size_t i = 0; i < SocketController::kLightCount; ++i)
            {
                const auto *cfg = sockets.lightConfigByIndex(i);
                if (!cfg)
                    continue;
                const uint8_t btn = cfg->button_port;
                const uint8_t relay = cfg->relay_port;
                if (btn != SocketController::kInvalidPort && btn < PortIO::PORT_COUNT)
                    used[btn] = true;
                if (relay != SocketController::kInvalidPort && relay < PortIO::PORT_COUNT)
                    used[relay] = true;
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

