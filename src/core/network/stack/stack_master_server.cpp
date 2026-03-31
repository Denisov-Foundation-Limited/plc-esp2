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

#include "core/network/stack/stack_master_server.hpp"

#include "utils/logger.hpp"

StackMasterServer::StackMasterServer(Logger &log) : _router(log, _transport, _registry)
{
}

void StackMasterServer::setConfig(const Config &cfg)
{
    _router.setConfig(cfg);
}

void StackMasterServer::setMessageHandler(MessageHandler cb, void *ctx)
{
    _router.setMessageHandler(cb, ctx);
}

void StackMasterServer::setRouteHandler(RouteHandler cb, void *ctx)
{
    _router.setRouteHandler(cb, ctx);
}

void StackMasterServer::setBinaryRouteHandler(BinaryRouteHandler cb, void *ctx)
{
    _router.setBinaryRouteHandler(cb, ctx);
}

void StackMasterServer::setNotificationHandler(NotificationHandler cb, void *ctx)
{
    _router.setNotificationHandler(cb, ctx);
}

void StackMasterServer::setNodeEventHandler(NodeEventHandler cb, void *ctx)
{
    _router.setNodeEventHandler(cb, ctx);
}

bool StackMasterServer::begin()
{
    return _router.begin();
}

void StackMasterServer::loop()
{
    _router.loop();
}

void StackMasterServer::stop()
{
    _router.stop();
}

StackTransport::Caps StackMasterServer::caps() const
{
    return _transport.caps();
}

StackMasterRouter &StackMasterServer::router()
{
    return _router;
}

const StackMasterRouter &StackMasterServer::router() const
{
    return _router;
}

StackDeviceRegistry &StackMasterServer::registry()
{
    return _registry;
}

const StackDeviceRegistry &StackMasterServer::registry() const
{
    return _registry;
}
