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

#include "core/network/stack/stack_device_registry.hpp"
#include "core/network/stack/stack_master_router.hpp"
#include "core/network/stack/stack_rs485_transport.hpp"

class Logger;

class StackRs485Server
{
public:
    using Config = StackMasterRouter::Config;
    using MessageHandler = StackMasterRouter::MessageHandler;
    using RouteHandler = StackMasterRouter::RouteHandler;
    using BinaryRouteHandler = StackMasterRouter::BinaryRouteHandler;
    using NotificationHandler = StackMasterRouter::NotificationHandler;
    using NodeEventHandler = StackMasterRouter::NodeEventHandler;

    explicit StackRs485Server(Logger &log);

    void setConfig(const Config &cfg);
    void setTransportConfig(const StackRs485Transport::Config &cfg);
    StackRs485Transport::Config transportConfig() const;
    void setMessageHandler(MessageHandler cb, void *ctx);
    void setRouteHandler(RouteHandler cb, void *ctx);
    void setBinaryRouteHandler(BinaryRouteHandler cb, void *ctx);
    void setNotificationHandler(NotificationHandler cb, void *ctx);
    void setNodeEventHandler(NodeEventHandler cb, void *ctx);

    bool begin();
    void loop();
    void stop();

    StackTransport::Caps caps() const;
    StackMasterRouter &router();
    const StackMasterRouter &router() const;
    StackRs485Transport &transport();
    const StackRs485Transport &transport() const;

private:
    StackRs485Transport _transport;
    StackDeviceRegistry _registry;
    StackMasterRouter _router;
};
