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
    if (!web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
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
    const size_t count = web.network()->stackOnlineDeviceCount();
    for (size_t i = 0; i < count; ++i)
    {
        StackDeviceRegistry::DeviceInfo device{};
        if (!web.network()->stackDeviceSnapshotAt(i, device) || !device.online || device.node_id == 0)
            continue;
        const uint32_t id = device.node_id;
        html += "<option value=\"";
        html += String((unsigned long)id);
        html += "\"";
        if (stack_view && id == selected_node_id)
            html += " selected";
        html += ">";
        String name = device.name[0] ? String(device.name) : String();
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
    if (node_id == 0 || !web.network() || web.network()->stackRole() != ConfigsManagerIface::StackRole::Master)
        return false;
    StackDeviceRegistry::DeviceInfo device{};
    return web.network()->stackDeviceSnapshotByNodeId(node_id, device) && device.online;
}

bool WebInterfaceControllersRingHelper::sendStackRingCmd_(WebInterface &web, uint32_t node_id, bool set_state,
                                                          bool state)
{
    (void)web;
    (void)node_id;
    (void)set_state;
    (void)state;
    return false;
}

bool WebInterfaceControllersRingHelper::sendStackRingCmdAll_(WebInterface &web, bool set_state, bool state)
{
    (void)web;
    (void)set_state;
    (void)state;
    return false;
}
