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

#include "core/network/web/interfaces/web_interface_controllers_ring.hpp"

#include "core/network/web/web_interface.hpp"

String WebInterfaceControllersRingHelper::ringDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id,
                                                                bool stack_view)
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

bool WebInterfaceControllersRingHelper::isStackRingView_(const WebInterface &web, uint32_t node_id)
{
    return node_id != 0 && web._stack_master && web.stackRole_() == ConfigsManagerIface::StackRole::Master;
}

bool WebInterfaceControllersRingHelper::sendStackRingCmd_(WebInterface &web, uint32_t node_id, bool set_state,
                                                          bool state)
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
    return web._stack_master->sendTo(node_id, (uint8_t)StackMsgType::CmdSet, (const uint8_t *)payload, len);
}

bool WebInterfaceControllersRingHelper::sendStackRingCmdAll_(WebInterface &web, bool set_state, bool state)
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

void WebInterfaceControllersRingHelper::onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame)
{
    if (!ctx)
        return;
    WebInterface *self = static_cast<WebInterface *>(ctx);
    self->handleStackFrame_(node_id, frame);
}
