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

#include "core/network/stack/stack_rs485_server.hpp"

StackRs485Server::StackRs485Server(Logger &log) : _transport(log), _router(log, _transport, _registry)
{
}

void StackRs485Server::setConfig(const Config &cfg)
{
    _router.setConfig(cfg);
}

void StackRs485Server::setTransportConfig(const StackRs485Transport::Config &cfg)
{
    _transport.setConfig(cfg);
}

StackRs485Transport::Config StackRs485Server::transportConfig() const
{
    return _transport.config();
}

void StackRs485Server::setMessageHandler(MessageHandler cb, void *ctx)
{
    _router.setMessageHandler(cb, ctx);
}

void StackRs485Server::setRouteHandler(RouteHandler cb, void *ctx)
{
    _router.setRouteHandler(cb, ctx);
}

void StackRs485Server::setBinaryRouteHandler(BinaryRouteHandler cb, void *ctx)
{
    _router.setBinaryRouteHandler(cb, ctx);
}

void StackRs485Server::setNotificationHandler(NotificationHandler cb, void *ctx)
{
    _router.setNotificationHandler(cb, ctx);
}

void StackRs485Server::setNodeEventHandler(NodeEventHandler cb, void *ctx)
{
    _router.setNodeEventHandler(cb, ctx);
}

bool StackRs485Server::begin()
{
    return _router.begin();
}

void StackRs485Server::loop()
{
    _router.loop();
}

void StackRs485Server::stop()
{
    _router.stop();
}

StackTransport::Caps StackRs485Server::caps() const
{
    return _transport.caps();
}

StackMasterRouter &StackRs485Server::router()
{
    return _router;
}

const StackMasterRouter &StackRs485Server::router() const
{
    return _router;
}

StackRs485Transport &StackRs485Server::transport()
{
    return _transport;
}

const StackRs485Transport &StackRs485Server::transport() const
{
    return _transport;
}
