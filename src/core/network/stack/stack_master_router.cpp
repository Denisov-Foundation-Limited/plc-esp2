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

#include "core/network/stack/stack_master_router.hpp"

#include <memory>

#include "core/network/stack/stack_binary_protocol.hpp"
#include "core/network/stack/stack_json_protocol.hpp"
#include "utils/logger.hpp"

namespace
{
void fillNotifyFromBinary_(const StackBinaryProtocol::NotifyFrame &frame, StackJsonProtocol::NotifyMessage &out)
{
    out = StackJsonProtocol::NotifyMessage{};
    out.is_binary = true;
    out.source_node = frame.source_node;
    strncpy(out.level, frame.level, sizeof(out.level) - 1);
    out.level[sizeof(out.level) - 1] = '\0';
    strncpy(out.feature, frame.feature, sizeof(out.feature) - 1);
    out.feature[sizeof(out.feature) - 1] = '\0';
    strncpy(out.code, frame.code, sizeof(out.code) - 1);
    out.code[sizeof(out.code) - 1] = '\0';
    if (frame.message && frame.message_size)
        out.message = String(reinterpret_cast<const char *>(frame.message)).substring(0, frame.message_size);
}
}

StackMasterRouter::StackMasterRouter(Logger &log, StackTransport &transport, StackDeviceRegistry &registry)
    : _log(log), _transport(transport), _registry(registry)
{
}

void StackMasterRouter::setConfig(const Config &cfg)
{
    const auto guard = _lock.guard();
    _cfg = cfg;
}

void StackMasterRouter::setMessageHandler(MessageHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _message_cb = cb;
    _message_ctx = ctx;
}

void StackMasterRouter::setRouteHandler(RouteHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _route_cb = cb;
    _route_ctx = ctx;
}

void StackMasterRouter::setBinaryRouteHandler(BinaryRouteHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _binary_route_cb = cb;
    _binary_route_ctx = ctx;
}

void StackMasterRouter::setNotificationHandler(NotificationHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _notify_cb = cb;
    _notify_ctx = ctx;
}

void StackMasterRouter::setNodeEventHandler(NodeEventHandler cb, void *ctx)
{
    const auto guard = _lock.guard();
    _node_event_cb = cb;
    _node_event_ctx = ctx;
}

bool StackMasterRouter::begin()
{
    const auto guard = _lock.guard();
    _transport.setEventHandler(&StackMasterRouter::onTransportEvent_, this);
    _registry.clear();
    _started = _transport.begin(_cfg.port);
    if (_started)
        _log.info(F("STACK"), F("Master router started: port %u"), (unsigned)_cfg.port);
    else
        _log.error(F("STACK"), F("Master router start failed: port %u"), (unsigned)_cfg.port);
    return _started;
}

void StackMasterRouter::loop()
{
    bool started = false;
    {
        const auto guard = _lock.guard();
        started = _started;
    }
    if (started)
        _transport.loop();
}

void StackMasterRouter::stop()
{
    const auto guard = _lock.guard();
    if (!_started)
        return;
    _transport.stop();
    _registry.clear();
    _started = false;
}

const StackDeviceRegistry &StackMasterRouter::registry() const
{
    return _registry;
}

StackDeviceRegistry &StackMasterRouter::registry()
{
    return _registry;
}

bool StackMasterRouter::sendText(uint32_t node_id, const char *text)
{
    StackDeviceRegistry::DeviceInfo device;
    if (!text || !text[0])
        return false;
    {
        const auto guard = _lock.guard();
        if (!_registry.snapshotByNodeId(node_id, device))
            return false;
    }
    return _transport.sendText(device.client_id, text);
}

bool StackMasterRouter::sendBinary(uint32_t node_id, const uint8_t *data, size_t size)
{
    StackDeviceRegistry::DeviceInfo device;
    if (!data || size == 0)
        return false;
    {
        const auto guard = _lock.guard();
        if (!_registry.snapshotByNodeId(node_id, device))
            return false;
    }
    return _transport.sendBinary(device.client_id, data, size);
}

void StackMasterRouter::onTransportEvent_(void *ctx, const StackTransport::Event &event)
{
    if (!ctx)
        return;
    static_cast<StackMasterRouter *>(ctx)->handleTransportEvent_(event);
}

void StackMasterRouter::handleTransportEvent_(const StackTransport::Event &event)
{
    switch (event.type)
    {
    case StackTransport::EventType::Connected:
        handleConnected_(event);
        break;
    case StackTransport::EventType::Disconnected:
        handleDisconnected_(event);
        break;
    case StackTransport::EventType::TextMessage:
        handleTextMessage_(event);
        break;
    case StackTransport::EventType::BinaryMessage:
        handleBinaryMessage_(event);
        break;
    }
}

void StackMasterRouter::handleConnected_(const StackTransport::Event &event)
{
    if (event.ip == IPAddress((uint32_t)0))
    {
        _log.info(F("STACK"), F("WS client connected: id %u ip -"), (unsigned)event.client_id);
        return;
    }
    _log.info(F("STACK"), F("WS client connected: id %u ip %s"),
              (unsigned)event.client_id, event.ip.toString().c_str());
}

void StackMasterRouter::handleDisconnected_(const StackTransport::Event &event)
{
    StackDeviceRegistry::DeviceInfo device;
    char reason[40]{};
    const bool have_reason = takeDisconnectReason_(event.client_id, reason, sizeof(reason));
    const char *reason_c = have_reason ? reason : "transport_disconnect";
    NodeEventHandler node_event_cb = nullptr;
    void *node_event_ctx = nullptr;
    if (_registry.snapshotByClientId(event.client_id, device))
    {
        _log.warn(F("STACK"), F("Device offline: node 0x%08lX name %s ip %s reason: %s"),
                  (unsigned long)device.node_id,
                  device.name[0] ? device.name : "-", device.ip[0] ? device.ip : event.ip.toString().c_str(), reason_c);
        {
            const auto guard = _lock.guard();
            node_event_cb = _node_event_cb;
            node_event_ctx = _node_event_ctx;
        }
    }
    else
    {
        if (event.ip == IPAddress((uint32_t)0))
            return;
        _log.warn(F("STACK"), F("WS client disconnected: id %u ip %s reason: %s"), (unsigned)event.client_id,
                  event.ip.toString().c_str(), reason_c);
    }
    _registry.removeByClientId(event.client_id);
    if (node_event_cb && device.node_id != 0)
        node_event_cb(node_event_ctx, device.node_id, false);
}

void StackMasterRouter::handleTextMessage_(const StackTransport::Event &event)
{
    if (!isAuthorized_(event.client_id))
    {
        if (!authorizeClient_(event.client_id, event.ip, event.data, event.size))
        {
            rememberDisconnectReason_(event.client_id, "auth_failed");
            const String reply = StackJsonProtocol::makeError("auth_failed");
            _transport.sendText(event.client_id, reply.c_str());
            _transport.disconnectClient(event.client_id);
        }
        return;
    }

    _registry.touchClient(event.client_id, millis());
    StackDeviceRegistry::DeviceInfo device;
    const bool have_device = _registry.snapshotByClientId(event.client_id, device);
    if (have_device && handleNotifyMessage_(device, event.data, event.size))
        return;
    if (have_device && handleRouteMessage_(device, event.data, event.size))
        return;
    MessageHandler message_cb = nullptr;
    void *message_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        message_cb = _message_cb;
        message_ctx = _message_ctx;
    }
    if (message_cb && have_device)
        message_cb(message_ctx, device, false, event.data, event.size);
}

void StackMasterRouter::handleBinaryMessage_(const StackTransport::Event &event)
{
    if (!isAuthorized_(event.client_id))
    {
        if (!authorizeClient_(event.client_id, event.ip, event.data, event.size))
        {
            _log.warn(F("STACK"), F("Reject unauth binary: id %u"), (unsigned)event.client_id);
            rememberDisconnectReason_(event.client_id, "unauth_binary");
            if (StackBinaryProtocol::detectKind(event.data, event.size) == StackBinaryProtocol::FrameKind::Auth)
            {
                const size_t frame_size = StackBinaryProtocol::encodedAuthReplySize("auth_failed");
                std::unique_ptr<uint8_t[]> frame(new uint8_t[frame_size]);
                size_t used = 0;
                if (frame && StackBinaryProtocol::encodeAuthReply(false, "auth_failed", frame.get(), frame_size, used))
                    _transport.sendBinary(event.client_id, frame.get(), used);
            }
            _transport.disconnectClient(event.client_id);
        }
        return;
    }

    _registry.touchClient(event.client_id, millis());
    StackDeviceRegistry::DeviceInfo device;
    const bool have_device = _registry.snapshotByClientId(event.client_id, device);
    if (have_device && handleBinaryNotifyMessage_(device, event.data, event.size))
        return;
    if (have_device && handleBinaryRouteMessage_(device, event.data, event.size))
        return;
    MessageHandler message_cb = nullptr;
    void *message_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        message_cb = _message_cb;
        message_ctx = _message_ctx;
    }
    if (message_cb && have_device)
        message_cb(message_ctx, device, true, event.data, event.size);
}

bool StackMasterRouter::handleRouteMessage_(const StackDeviceRegistry::DeviceInfo &device, const uint8_t *data, size_t size)
{
    if (StackJsonProtocol::detectKind(data, size) != StackJsonProtocol::MessageKind::Route)
        return false;

    StackJsonProtocol::RouteMessage route;
    if (!StackJsonProtocol::parseRoute(data, size, route))
    {
        _log.warn(F("STACK"), F("Route parse failed: node 0x%08lX"), (unsigned long)device.node_id);
        return true;
    }

    if (route.source_node == 0)
        route.source_node = device.node_id;

    if (route.target_node == _cfg.local_node_id || route.target_node == 0)
    {
        RouteHandler route_cb = nullptr;
        void *route_ctx = nullptr;
        {
            const auto guard = _lock.guard();
            route_cb = _route_cb;
            route_ctx = _route_ctx;
        }
        if (route_cb)
            route_cb(route_ctx, device, route);
        return true;
    }

    StackDeviceRegistry::DeviceInfo target;
    if (!_registry.snapshotByNodeId(route.target_node, target))
    {
        _log.warn(F("STACK"), F("Route target offline: src 0x%08lX dst 0x%08lX"), (unsigned long)device.node_id,
                  (unsigned long)route.target_node);
        RouteHandler route_cb = nullptr;
        void *route_ctx = nullptr;
        {
            const auto guard = _lock.guard();
            route_cb = _route_cb;
            route_ctx = _route_ctx;
        }
        if (route_cb)
            route_cb(route_ctx, device, route);
        return true;
    }

    String forwarded = StackJsonProtocol::makeRoute(route.source_node, route.target_node, route.feature, route.action,
                                                    route.payload_json, &route.meta);
    if (!_transport.sendText(target.client_id, forwarded.c_str()))
    {
        _log.warn(F("STACK"), F("Route forward failed: src 0x%08lX dst 0x%08lX"), (unsigned long)device.node_id,
                  (unsigned long)route.target_node);
    }
    else
    {
        _log.debug(F("STACK"), F("Route forward: src 0x%08lX dst 0x%08lX feature %s action %s"),
                   (unsigned long)route.source_node, (unsigned long)route.target_node, route.feature, route.action);
    }
    return true;
}

bool StackMasterRouter::handleNotifyMessage_(const StackDeviceRegistry::DeviceInfo &device, const uint8_t *data, size_t size)
{
    if (StackJsonProtocol::detectKind(data, size) != StackJsonProtocol::MessageKind::Notify)
        return false;

    StackJsonProtocol::NotifyMessage notify;
    if (!StackJsonProtocol::parseNotify(data, size, notify))
    {
        _log.warn(F("STACK"), F("Notify parse failed: node 0x%08lX"), (unsigned long)device.node_id);
        return true;
    }

    if (notify.source_node == 0)
        notify.source_node = device.node_id;

    NotificationHandler notify_cb = nullptr;
    void *notify_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        notify_cb = _notify_cb;
        notify_ctx = _notify_ctx;
    }
    if (notify_cb)
        notify_cb(notify_ctx, device, notify);
    return true;
}

bool StackMasterRouter::handleBinaryNotifyMessage_(const StackDeviceRegistry::DeviceInfo &device, const uint8_t *data, size_t size)
{
    if (StackBinaryProtocol::detectKind(data, size) != StackBinaryProtocol::FrameKind::Notify)
        return false;

    StackBinaryProtocol::NotifyFrame frame;
    if (!StackBinaryProtocol::parseNotify(data, size, frame))
    {
        _log.warn(F("STACK"), F("Binary notify parse failed: node 0x%08lX"), (unsigned long)device.node_id);
        return true;
    }

    if (frame.source_node == 0)
        frame.source_node = device.node_id;

    StackJsonProtocol::NotifyMessage notify;
    fillNotifyFromBinary_(frame, notify);
    NotificationHandler notify_cb = nullptr;
    void *notify_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        notify_cb = _notify_cb;
        notify_ctx = _notify_ctx;
    }
    if (notify_cb)
        notify_cb(notify_ctx, device, notify);
    return true;
}

bool StackMasterRouter::handleBinaryRouteMessage_(const StackDeviceRegistry::DeviceInfo &device, const uint8_t *data, size_t size)
{
    if (StackBinaryProtocol::detectKind(data, size) != StackBinaryProtocol::FrameKind::Route)
        return false;

    StackBinaryProtocol::RouteFrame route;
    if (!StackBinaryProtocol::parseRoute(data, size, route))
    {
        _log.warn(F("STACK"), F("Binary route parse failed: node 0x%08lX"), (unsigned long)device.node_id);
        return true;
    }

    if (route.source_node == 0)
        route.source_node = device.node_id;

    if (route.target_node == _cfg.local_node_id || route.target_node == 0)
    {
        BinaryRouteHandler route_cb = nullptr;
        void *route_ctx = nullptr;
        {
            const auto guard = _lock.guard();
            route_cb = _binary_route_cb;
            route_ctx = _binary_route_ctx;
        }
        if (route_cb)
            route_cb(route_ctx, device, route);
        return true;
    }

    StackDeviceRegistry::DeviceInfo target;
    if (!_registry.snapshotByNodeId(route.target_node, target))
    {
        _log.warn(F("STACK"), F("Binary route target offline: src 0x%08lX dst 0x%08lX"),
                  (unsigned long)device.node_id,
                  (unsigned long)route.target_node);
        BinaryRouteHandler route_cb = nullptr;
        void *route_ctx = nullptr;
        {
            const auto guard = _lock.guard();
            route_cb = _binary_route_cb;
            route_ctx = _binary_route_ctx;
        }
        if (route_cb)
            route_cb(route_ctx, device, route);
        return true;
    }

    const size_t frame_size = StackBinaryProtocol::encodedRouteSize(route.feature, route.action, route.payload_size);
    std::unique_ptr<uint8_t[]> frame(new uint8_t[frame_size]);
    size_t used = 0;
    if (!frame || !StackBinaryProtocol::encodeRoute(route.source_node, route.target_node, route.feature, route.action,
                                                    route.payload, route.payload_size, frame.get(), frame_size, used,
                                                    &route.meta))
    {
        _log.warn(F("STACK"), F("Binary route encode failed: src 0x%08lX dst 0x%08lX"),
                  (unsigned long)route.source_node,
                  (unsigned long)route.target_node);
        return true;
    }

    if (!_transport.sendBinary(target.client_id, frame.get(), used))
    {
        _log.warn(F("STACK"), F("Binary route forward failed: src 0x%08lX dst 0x%08lX"),
                  (unsigned long)route.source_node,
                  (unsigned long)route.target_node);
    }
    else
    {
        _log.debug(F("STACK"), F("Binary route forward: src 0x%08lX dst 0x%08lX feature %s action %s"),
                   (unsigned long)route.source_node, (unsigned long)route.target_node, route.feature, route.action);
    }
    return true;
}

bool StackMasterRouter::authorizeClient_(uint8_t client_id, IPAddress ip, const uint8_t *data, size_t size)
{
    if (!data || size == 0)
        return false;
    StackJsonProtocol::AuthMessage auth_msg;
    bool binary_reply = false;
    if (StackJsonProtocol::detectKind(data, size) == StackJsonProtocol::MessageKind::Auth)
    {
        if (!StackJsonProtocol::parseAuth(data, size, auth_msg))
        {
            _log.warn(F("STACK"), F("Auth parse failed: id %u ip %s"), (unsigned)client_id, ip.toString().c_str());
            return false;
        }
    }
    else if (StackBinaryProtocol::detectKind(data, size) == StackBinaryProtocol::FrameKind::Auth)
    {
        StackBinaryProtocol::AuthFrame auth_frame;
        if (!StackBinaryProtocol::parseAuth(data, size, auth_frame))
        {
            _log.warn(F("STACK"), F("Binary auth parse failed: id %u ip %s"), (unsigned)client_id, ip.toString().c_str());
            return false;
        }
        auth_msg.node_id = auth_frame.node_id;
        auth_msg.caps = auth_frame.caps;
        auth_msg.fw_version = auth_frame.fw_version;
        strncpy(auth_msg.name, auth_frame.name, sizeof(auth_msg.name) - 1);
        strncpy(auth_msg.ip, auth_frame.ip, sizeof(auth_msg.ip) - 1);
        strncpy(auth_msg.api_key, auth_frame.api_key, sizeof(auth_msg.api_key) - 1);
        binary_reply = true;
    }
    else
    {
        _log.warn(F("STACK"), F("Auth reject: wrong message type for client %u"), (unsigned)client_id);
        return false;
    }

    String api_key;
    {
        const auto guard = _lock.guard();
        api_key = _cfg.api_key;
    }
    if (api_key.length() && strcmp(auth_msg.api_key, api_key.c_str()) != 0)
    {
        _log.warn(F("STACK"), F("Auth reject: id %u ip %s"), (unsigned)client_id, ip.toString().c_str());
        return false;
    }

    StackDeviceRegistry::AuthInfo auth;
    auth.node_id = auth_msg.node_id;
    auth.caps = auth_msg.caps;
    auth.fw_version = auth_msg.fw_version;
    auth.name = auth_msg.name;
    auth.ip = auth_msg.ip;

    _log.info(F("STACK"), F("Auth accept: id %u ip %s node 0x%08lX name %s caps 0x%08lX fw %u"),
              (unsigned)client_id, ip.toString().c_str(), (unsigned long)auth.node_id,
              auth.name ? auth.name : "-", (unsigned long)auth.caps, (unsigned)auth.fw_version);

    StackDeviceRegistry::DeviceInfo device;
    if (!_registry.upsert(client_id, auth, ip.toString().c_str(), &device))
    {
        _log.warn(F("STACK"), F("Registry full: reject node 0x%08lX"), (unsigned long)auth.node_id);
        return false;
    }

    _log.info(F("STACK"), F("Slave connected: id %u node 0x%08lX name %s ip %s"), (unsigned)client_id,
              (unsigned long)device.node_id, device.name[0] ? device.name : "-",
              device.ip[0] ? device.ip : ip.toString().c_str());
    if (binary_reply)
    {
        const size_t frame_size = StackBinaryProtocol::encodedAuthReplySize("authorized");
        std::unique_ptr<uint8_t[]> frame(new uint8_t[frame_size]);
        size_t used = 0;
        if (frame && StackBinaryProtocol::encodeAuthReply(true, "authorized", frame.get(), frame_size, used))
            _transport.sendBinary(client_id, frame.get(), used);
    }
    else
    {
        const String reply = StackJsonProtocol::makeOk("authorized");
        _transport.sendText(client_id, reply.c_str());
    }
    NodeEventHandler node_event_cb = nullptr;
    void *node_event_ctx = nullptr;
    {
        const auto guard = _lock.guard();
        node_event_cb = _node_event_cb;
        node_event_ctx = _node_event_ctx;
    }
    if (node_event_cb)
        node_event_cb(node_event_ctx, device.node_id, true);
    return true;
}

bool StackMasterRouter::isAuthorized_(uint8_t client_id) const
{
    return _registry.hasClientId(client_id);
}

void StackMasterRouter::rememberDisconnectReason_(uint8_t client_id, const char *reason)
{
    if (!reason || !reason[0])
        return;
    const auto guard = _lock.guard();
    DisconnectReasonSlot *slot = nullptr;
    for (size_t i = 0; i < kDisconnectReasonSlots; ++i)
    {
        if (_disconnect_reasons[i].used && _disconnect_reasons[i].client_id == client_id)
        {
            slot = &_disconnect_reasons[i];
            break;
        }
        if (!slot && !_disconnect_reasons[i].used)
            slot = &_disconnect_reasons[i];
    }
    if (!slot)
        slot = &_disconnect_reasons[0];
    slot->used = true;
    slot->client_id = client_id;
    strncpy(slot->reason, reason, sizeof(slot->reason) - 1);
    slot->reason[sizeof(slot->reason) - 1] = '\0';
}

bool StackMasterRouter::takeDisconnectReason_(uint8_t client_id, char *out, size_t out_cap)
{
    if (!out || out_cap == 0)
        return false;
    out[0] = '\0';
    const auto guard = _lock.guard();
    for (size_t i = 0; i < kDisconnectReasonSlots; ++i)
    {
        DisconnectReasonSlot &slot = _disconnect_reasons[i];
        if (!slot.used || slot.client_id != client_id)
            continue;
        strncpy(out, slot.reason, out_cap - 1);
        out[out_cap - 1] = '\0';
        memset(&slot, 0, sizeof(slot));
        return true;
    }
    return false;
}
