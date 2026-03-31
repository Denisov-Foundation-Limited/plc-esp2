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

#include <memory>

#include <WebSocketsServer.h>

#include "core/network/stack/stack_transport.hpp"
#include "utils/rtos_lock.hpp"

class StackWsServerTransport : public StackTransport
{
public:
    StackWsServerTransport();

    void setEventHandler(EventHandler cb, void *ctx) override;
    bool begin(uint16_t port) override;
    void loop() override;
    void stop() override;
    Caps caps() const override;

    bool sendText(uint8_t client_id, const char *text) override;
    bool sendBinary(uint8_t client_id, const uint8_t *data, size_t size) override;
    bool disconnectClient(uint8_t client_id) override;

private:
    std::unique_ptr<WebSocketsServer> _server;
    EventHandler _event_cb = nullptr;
    void *_event_ctx = nullptr;
    uint16_t _port = 0;
    bool _started = false;
    mutable RtosRecursiveLock _lock;

    void onWsEvent_(uint8_t client_id, WStype_t type, uint8_t *payload, size_t len);
};
