#pragma once

#ifndef WEB_INTERFACE_CLASS_CONTEXT
#include <Arduino.h>

class WebInterface;
struct StackFrame;

class WebInterfaceControllersRingHelper
{
public:
    static String ringDeviceSelectHtml_(const WebInterface &web, uint32_t selected_node_id, bool stack_view);
    static bool isStackRingView_(const WebInterface &web, uint32_t node_id);
    static bool sendStackRingCmd_(WebInterface &web, uint32_t node_id, bool set_state, bool state);
    static bool sendStackRingCmdAll_(WebInterface &web, bool set_state, bool state);
    static void onStackFrame_(void *ctx, uint32_t node_id, const StackFrame &frame);
};
#else
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
