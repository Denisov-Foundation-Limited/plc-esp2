#pragma once

    String ringDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        if (stackRole_() != ConfigsManagerIface::StackRole::Master || !_stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += "<span class=\"muted\">Устройство</span>";
        html += "<select id=\"ring-device\" class=\"field mini\">";
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


    bool isStackRingView_(uint32_t node_id) const
    {
        return node_id != 0 && _stack_master &&
               stackRole_() == ConfigsManagerIface::StackRole::Master;
    }


    bool sendStackRingCmd_(uint32_t node_id, bool set_state, bool state)
    {
        if (!_stack_master)
            return false;
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Ring;
        if (set_state)
        {
            doc["action"] = "set";
            JsonObject params = doc["params"].to<JsonObject>();
            params["state"] = state;
        }
        else
        {
            doc["action"] = "trigger";
        }
        const String key = stackApiKey_();
        if (key.length())
            doc["api_key"] = key;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        return _stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                     (const uint8_t *)payload, len);
    }


    bool sendStackRingCmdAll_(bool set_state, bool state)
    {
        if (!_stack_master || stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        const size_t count = _stack_master->nodeCount();
        if (count == 0)
            return false;
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = nextStackCmdId_();
        doc["feature"] = (uint8_t)StackFeature::Ring;
        if (set_state)
        {
            doc["action"] = "set";
            JsonObject params = doc["params"].to<JsonObject>();
            params["state"] = state;
        }
        else
        {
            doc["action"] = "trigger";
        }
        const String key = stackApiKey_();
        if (key.length())
            doc["api_key"] = key;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        bool ok = true;
        for (size_t i = 0; i < count; ++i)
        {
            if (!_stack_master->sendTo(_stack_master->nodeIdAt(i), (uint8_t)StackMsgType::CmdSet,
                                       (const uint8_t *)payload, len))
                ok = false;
        }
        return ok;
    }

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        if (!ctx)
            return;
        WebInterface *self = static_cast<WebInterface *>(ctx);
        self->handleStackFrame_(node_id, frame);
        if (self->_stack_cache)
            StackCache::onStackFrame_(self->_stack_cache, node_id, frame);
    }


    String ringUsedPortsJson_(PortIO::PinType type) const { return globalUsedPortsJson_(type); }

