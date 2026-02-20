#pragma once

#ifndef WEB_INTERFACE_CLASS_CONTEXT
class WebInterface;
class WebInterfaceControllersRingHelper;
#else

class WebInterfaceControllersRingHelper
{
public:
    static String ringDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view)
    {
        if (web.stackRole_() != ConfigsManagerIface::StackRole::Master || !web._stack_master)
            return "";
        String html;
        html.reserve(512);
        html += "<div class=\"row\">";
        html += WebUiRu::Ring::kText;
        html += "<select id=\"ring-device\" class=\"field mini\">";
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
    static bool isStackRingView_(const WebInterface &web, uint32_t node_id)
    {
        return node_id != 0 && web._stack_master &&
               web.stackRole_() == ConfigsManagerIface::StackRole::Master;
    }
    static bool sendStackRingCmd_(WebInterface &web, uint32_t node_id, bool set_state, bool state)
    {
        if (!web._stack_master)
            return false;
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = web.nextStackCmdId_();
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
        const String key = web.stackApiKey_();
        if (key.length())
            doc["api_key"] = key;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        return web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet,
                                     (const uint8_t *)payload, len);
    }
    static bool sendStackRingCmdAll_(WebInterface &web, bool set_state, bool state)
    {
        if (!web._stack_master || web.stackRole_() != ConfigsManagerIface::StackRole::Master)
            return false;
        const size_t count = web._stack_master->nodeCount();
        if (count == 0)
            return false;
        StaticJsonDocument<192> doc;
        doc["cmd_id"] = web.nextStackCmdId_();
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
        const String key = web.stackApiKey_();
        if (key.length())
            doc["api_key"] = key;
        char payload[128] = {};
        const size_t len = serializeJson(doc, payload, sizeof(payload));
        if (len == 0)
            return false;
        bool ok = true;
        for (size_t i = 0; i < count; ++i)
        {
            if (!web._stack_master->sendTo(web._stack_master->nodeIdAt(i), (uint8_t)StackMsgType::CmdSet,
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
};

    String ringDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
    {
        return WebInterfaceControllersRingHelper::ringDeviceSelectHtml_(*this, selected_node_id, stack_view);
    }

    bool isStackRingView_(uint32_t node_id) const
    {
        return WebInterfaceControllersRingHelper::isStackRingView_(*this, node_id);
    }

    bool sendStackRingCmd_(uint32_t node_id, bool set_state, bool state)
    {
        return WebInterfaceControllersRingHelper::sendStackRingCmd_(*this, node_id, set_state, state);
    }

    bool sendStackRingCmdAll_(bool set_state, bool state)
    {
        return WebInterfaceControllersRingHelper::sendStackRingCmdAll_(*this, set_state, state);
    }

    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
    {
        WebInterfaceControllersRingHelper::onStackFrame_(ctx, node_id, frame);
    }

#endif
