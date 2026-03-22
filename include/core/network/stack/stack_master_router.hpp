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

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "core/network/stack/stack_binary_protocol.hpp"
#include "core/network/stack/stack_device_registry.hpp"
#include "core/network/stack/stack_json_protocol.hpp"
#include "core/network/stack/stack_transport.hpp"
#include "utils/rtos_lock.hpp"

class Logger;

class StackMasterRouter
{
public:
    // Lock order contract:
    // StackMasterRouter may lock after StackRouteAdapter.
    // Inside router paths it may also touch StackTransport and StackDeviceRegistry.
    struct Config
    {
        uint16_t port = 0;
        String api_key;
        uint32_t local_node_id = 0;
    };

    using MessageHandler = void (*)(void *ctx, const StackDeviceRegistry::DeviceInfo &device, bool is_binary,
                                    const uint8_t *data, size_t size);
    using RouteHandler = void (*)(void *ctx, const StackDeviceRegistry::DeviceInfo &device,
                                  const StackJsonProtocol::RouteMessage &route);
    using BinaryRouteHandler = void (*)(void *ctx, const StackDeviceRegistry::DeviceInfo &device,
                                        const StackBinaryProtocol::RouteFrame &route);
    using NotificationHandler = void (*)(void *ctx, const StackDeviceRegistry::DeviceInfo &device,
                                         const StackJsonProtocol::NotifyMessage &notify);
    using NodeEventHandler = void (*)(void *ctx, uint32_t node_id, bool online);

    StackMasterRouter(Logger &log, StackTransport &transport, StackDeviceRegistry &registry);

    void setConfig(const Config &cfg);
    void setMessageHandler(MessageHandler cb, void *ctx);
    void setRouteHandler(RouteHandler cb, void *ctx);
    void setBinaryRouteHandler(BinaryRouteHandler cb, void *ctx);
    void setNotificationHandler(NotificationHandler cb, void *ctx);
    void setNodeEventHandler(NodeEventHandler cb, void *ctx);

    bool begin();
    void loop();
    void stop();

    const StackDeviceRegistry &registry() const;
    StackDeviceRegistry &registry();

    bool sendText(uint32_t node_id, const char *text);
    bool sendBinary(uint32_t node_id, const uint8_t *data, size_t size);

private:
    Logger &_log;
    StackTransport &_transport;
    StackDeviceRegistry &_registry;
    Config _cfg;
    MessageHandler _message_cb = nullptr;
    void *_message_ctx = nullptr;
    RouteHandler _route_cb = nullptr;
    void *_route_ctx = nullptr;
    BinaryRouteHandler _binary_route_cb = nullptr;
    void *_binary_route_ctx = nullptr;
    NotificationHandler _notify_cb = nullptr;
    void *_notify_ctx = nullptr;
    NodeEventHandler _node_event_cb = nullptr;
    void *_node_event_ctx = nullptr;
    bool _started = false;
    mutable RtosRecursiveLock _lock;
    static constexpr size_t kDisconnectReasonSlots = 8;
    struct DisconnectReasonSlot
    {
        bool used = false;
        uint8_t client_id = 0;
        char reason[40]{};
    };
    DisconnectReasonSlot _disconnect_reasons[kDisconnectReasonSlots]{};

    static void onTransportEvent_(void *ctx, const StackTransport::Event &event);
    void handleTransportEvent_(const StackTransport::Event &event);
    void handleConnected_(const StackTransport::Event &event);
    void handleDisconnected_(const StackTransport::Event &event);
    void handleTextMessage_(const StackTransport::Event &event);
    void handleBinaryMessage_(const StackTransport::Event &event);
    bool handleRouteMessage_(const StackDeviceRegistry::DeviceInfo &device, const uint8_t *data, size_t size);
    bool handleBinaryRouteMessage_(const StackDeviceRegistry::DeviceInfo &device, const uint8_t *data, size_t size);
    bool handleNotifyMessage_(const StackDeviceRegistry::DeviceInfo &device, const uint8_t *data, size_t size);
    bool authorizeClient_(uint8_t client_id, IPAddress ip, const uint8_t *data, size_t size);
    bool isAuthorized_(uint8_t client_id) const;
    void rememberDisconnectReason_(uint8_t client_id, const char *reason);
    bool takeDisconnectReason_(uint8_t client_id, char *out, size_t out_cap);
};
