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

#include "core/network/web/web_interface_stack_ops.hpp"

#include "core/network/web/web_interface.hpp"

namespace
{
void appendHtmlEscapedLocal_(String &out, const char *in)
{
    if (!in)
        return;
    while (*in)
    {
        switch (*in)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&#39;";
            break;
        default:
            out += *in;
            break;
        }
        ++in;
    }
}

String stackNodeIdHexLocal_(uint32_t value)
{
    char buf[16] = {};
    snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)value);
    return String(buf);
}

bool hasOnlineNode_(const WebInterface &web, uint32_t node_id)
{
    if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
        return false;
    StackDeviceRegistry::DeviceInfo device{};
    return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online && device.node_id != 0;
}

String deviceSelectHtml_(const WebInterface &web, const char *select_id, uint32_t selected_node_id, bool stack_view,
                         const char *span_class)
{
    if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
        return "";
    String html;
    html.reserve(512);
    html += "<div class=\"row\" style=\"margin: 6px 0 10px;\">";
    html += String("<span class=\"") + span_class + "\">" + WebUiRu::kDevice + "</span>";
    html += "<select id=\"";
    html += select_id;
    html += "\" class=\"mini\">";
    html += "<option value=\"local\"";
    if (!stack_view)
        html += " selected";
    html += ">local</option>";
    const size_t count = web.network()->stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!web.network()->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
            continue;
        html += "<option value=\"";
        html += String((unsigned long)device.node_id);
        html += "\"";
        if (stack_view && device.node_id == selected_node_id)
            html += " selected";
        html += ">";
        if (device.name[0])
            appendHtmlEscapedLocal_(html, device.name);
        else
            html += stackNodeIdHexLocal_(device.node_id);
        html += "</option>";
    }
    html += "</select></div>";
    return html;
}
}

String WebInterfaceStackOps::indexDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
{
    return deviceSelectHtml_(_web, "index-device", selected_node_id, stack_view, "status");
}

String WebInterfaceStackOps::busesDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
{
    return deviceSelectHtml_(_web, "buses-device", selected_node_id, stack_view, "muted");
}

String WebInterfaceStackOps::portsDeviceSelectHtml_(uint32_t selected_node_id, bool stack_view) const
{
    return deviceSelectHtml_(_web, "ports-device", selected_node_id, stack_view, "muted");
}

String WebInterfaceStackOps::stackBusesStatusText_(uint32_t node_id) const
{
    if (!hasOnlineNode_(_web, node_id))
        return WebUiRu::kNoDataFromSlave;
    return WebUiRu::kNoDataFromSlave;
}

String WebInterfaceStackOps::stackPortsStatusText_(uint32_t node_id) const
{
    if (!hasOnlineNode_(_web, node_id))
        return WebUiRu::kNoDataFromSlave;
    return WebUiRu::kNoDataFromSlave;
}

bool WebInterfaceStackOps::isStackBusesView_(uint32_t node_id) const
{
    return hasOnlineNode_(_web, node_id);
}

bool WebInterfaceStackOps::isStackPortsView_(uint32_t node_id) const
{
    return hasOnlineNode_(_web, node_id);
}

uint32_t WebInterfaceStackOps::parseStackNodeIdParam_(AsyncWebServerRequest *request) const
{
    if (_web.stackRole_() != ConfigsManagerIface::StackRole::Master || !_web.network())
        return 0;
    String node = _web.paramValueAny_(request, "node_id");
    if (node.length() == 0)
        node = _web.paramValueAny_(request, "node");
    if (node.length() == 0)
        return 0;
    char *end = nullptr;
    const unsigned long value = strtoul(node.c_str(), &end, 0);
    if (!end || end == node.c_str())
        return 0;
    const uint32_t node_id = (uint32_t)value;
    return hasOnlineNode_(_web, node_id) ? node_id : 0;
}

bool WebInterfaceStackOps::requestStackPorts_(uint32_t node_id)
{
    (void)node_id;
    return false;
}

bool WebInterfaceStackOps::refreshStackPorts_(uint32_t node_id)
{
    (void)node_id;
    return false;
}

bool WebInterfaceStackOps::requestStackExtenders_(uint32_t node_id)
{
    (void)node_id;
    return false;
}

bool WebInterfaceStackOps::requestStackI2c_(uint32_t node_id, bool run)
{
    (void)node_id;
    (void)run;
    return false;
}

bool WebInterfaceStackOps::requestStackOw_(uint32_t node_id, bool run)
{
    (void)node_id;
    (void)run;
    return false;
}

bool WebInterfaceStackOps::requestStackTempSensors_(uint32_t node_id)
{
    (void)node_id;
    return false;
}

bool WebInterfaceStackOps::refreshStackTempSensors_(uint32_t node_id)
{
    (void)node_id;
    return false;
}

bool WebInterfaceStackOps::requestStackIndexState_(uint32_t node_id)
{
    if (!_web.network() || node_id == 0)
        return false;
    const uint32_t now = millis();
    StackUnitSnapshot::State snapshot{};
    if (_web.network()->stackIndexState(node_id, snapshot))
    {
        const bool fresh_plc = snapshot.has_plc && (uint32_t)(now - snapshot.updated_ms) < 5000u;
        const bool fresh_rtc = snapshot.has_rtc && (uint32_t)(now - snapshot.updated_ms) < 5000u;
        if ((fresh_plc && fresh_rtc) || (snapshot.pending && (uint32_t)(now - snapshot.request_started_ms) < 1500u))
            return true;
    }
    if (!_web.network()->prepareStackIndexStateRequest(node_id, now, 5000u, 1500u))
        return false;

    const uint16_t cmd_id = nextStackCmdId_();
    DynamicJsonDocument doc(64);
    doc["cmd_id"] = cmd_id;
    return _web.network()->stackRoute().sendRequest(node_id, "system", "snapshot_req", &doc, StackRouteAdapter::Mode::Json,
                                                    true);
}

bool WebInterfaceStackOps::requestStackPlcStatus_(uint32_t node_id)
{
    return requestStackIndexState_(node_id);
}

bool WebInterfaceStackOps::requestStackRtcStatus_(uint32_t node_id)
{
    return requestStackIndexState_(node_id);
}

uint16_t WebInterfaceStackOps::nextStackCmdId_()
{
    ++_web._stack_cmd_id;
    if (_web._stack_cmd_id == 0)
        _web._stack_cmd_id = 1;
    return _web._stack_cmd_id;
}
