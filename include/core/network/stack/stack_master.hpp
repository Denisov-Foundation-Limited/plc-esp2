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
#include <vector>

#include <AsyncTCP.h>

#include "core/network/stack/stack_protocol.hpp"
#include "core/network/stack/stack_types.hpp"

class StackMaster
{
public:
    using FrameHandler = void (*)(void *ctx, uint32_t node_id, const StackFrame &frame);
    using EventHandler = void (*)(void *ctx, uint32_t node_id, bool online);

    explicit StackMaster(AsyncServer &server) : _server(&server) {}

    void setFrameHandler(FrameHandler cb, void *ctx)
    {
        _frame_cb = cb;
        _frame_ctx = ctx;
    }

    void setEventHandler(EventHandler cb, void *ctx)
    {
        _event_cb = cb;
        _event_ctx = ctx;
    }

    void begin()
    {
        if (!_server)
            return;
        _server->onClient([this](void *s, AsyncClient *c) { onClient_(s, c); }, this);
        _server->begin();
    }

    void loop() {}

    size_t nodeCount() const
    {
        size_t count = 0;
        for (const Session *s : _sessions)
            if (s && s->has_id && s->client)
                ++count;
        return count;
    }

    uint32_t nodeIdAt(size_t idx) const
    {
        size_t pos = 0;
        for (const Session *s : _sessions)
        {
            if (!s || !s->has_id || !s->client)
                continue;
            if (pos == idx)
                return s->node_id;
            ++pos;
        }
        return 0;
    }

    String nodeNameAt(size_t idx) const
    {
        size_t pos = 0;
        for (const Session *s : _sessions)
        {
            if (!s || !s->has_id || !s->client)
                continue;
            if (pos == idx)
                return s->name;
            ++pos;
        }
        return "";
    }

    bool sendTo(uint32_t node_id, uint8_t type, const uint8_t *payload, size_t len)
    {
        Session *s = findByNode_(node_id);
        if (!s || !s->client || !s->client->connected())
            return false;
        std::vector<uint8_t> buf;
        StackCodec::encode(type, payload, len, buf);
        if (buf.empty())
            return false;
        s->client->write((const char *)buf.data(), buf.size());
        return true;
    }

    void broadcast(uint8_t type, const uint8_t *payload, size_t len)
    {
        for (Session *s : _sessions)
            if (s && s->client && s->client->connected())
            {
                std::vector<uint8_t> buf;
                StackCodec::encode(type, payload, len, buf);
                if (!buf.empty())
                    s->client->write((const char *)buf.data(), buf.size());
            }
    }

private:
    struct Session
    {
        AsyncClient *client = nullptr;
        StackCodec codec;
        uint32_t node_id = 0;
        bool has_id = false;
        String name;
        uint32_t last_seen_ms = 0;
    };

    AsyncServer *_server = nullptr;
    std::vector<Session *> _sessions;
    FrameHandler _frame_cb = nullptr;
    void *_frame_ctx = nullptr;
    EventHandler _event_cb = nullptr;
    void *_event_ctx = nullptr;

    void onClient_(void *ctx, AsyncClient *client)
    {
        (void)ctx;
        Session *s = new Session();
        s->client = client;
        s->last_seen_ms = millis();
        _sessions.push_back(s);

        client->onData(
            [](void *arg, AsyncClient *c, void *data, size_t len) {
                StackMaster *self = static_cast<StackMaster *>(arg);
                self->onData_(c, (const uint8_t *)data, len);
            },
            this);
        client->onDisconnect(
            [](void *arg, AsyncClient *c) {
                StackMaster *self = static_cast<StackMaster *>(arg);
                self->onDisconnect_(c);
            },
            this);
        client->onError(
            [](void *arg, AsyncClient *c, int8_t) {
                StackMaster *self = static_cast<StackMaster *>(arg);
                self->onDisconnect_(c);
            },
            this);
    }

    void onData_(AsyncClient *client, const uint8_t *data, size_t len)
    {
        Session *s = findByClient_(client);
        if (!s)
            return;
        s->last_seen_ms = millis();
        FrameCtx ctx{this, s};
        s->codec.feed(
            data, len,
            [](void *ctx, const StackFrame &frame) {
                FrameCtx *fc = static_cast<FrameCtx *>(ctx);
                fc->self->handleFrame_(fc->session, frame);
            },
            &ctx);
    }

    void onDisconnect_(AsyncClient *client)
    {
        for (size_t i = 0; i < _sessions.size(); ++i)
        {
            Session *s = _sessions[i];
            if (s && s->client == client)
            {
                if (s->has_id)
                    notifyEvent_(s->node_id, false);
                delete s;
                _sessions.erase(_sessions.begin() + (int)i);
                return;
            }
        }
    }

    Session *findByClient_(AsyncClient *client)
    {
        for (Session *s : _sessions)
            if (s && s->client == client)
                return s;
        return nullptr;
    }

    Session *findByNode_(uint32_t node_id)
    {
        for (Session *s : _sessions)
            if (s && s->has_id && s->node_id == node_id)
                return s;
        return nullptr;
    }

    struct FrameCtx
    {
        StackMaster *self = nullptr;
        Session *session = nullptr;
    };

    void handleFrame_(Session *session, const StackFrame &frame)
    {
        if (frame.type == (uint8_t)StackMsgType::Hello)
        {
            StackHello hello{};
            if (StackHello::decode(frame.payload.data(), frame.payload.size(), hello))
            {
                Session *s = findByNode_(hello.node_id);
                if (!s)
                {
                    // try to bind to first unbound session
                    for (Session *cand : _sessions)
                    {
                        if (cand && !cand->has_id)
                        {
                            s = cand;
                            break;
                        }
                    }
                }
                if (s)
                {
                    s->node_id = hello.node_id;
                    s->has_id = true;
                    s->name = hello.name;
                }
                notifyEvent_(hello.node_id, true);
            }
        }
        if (_frame_cb)
            _frame_cb(_frame_ctx, nodeIdFromFrame_(session, frame), frame);
    }

    uint32_t nodeIdFromFrame_(Session *session, const StackFrame &frame)
    {
        if (frame.type != (uint8_t)StackMsgType::Hello)
            return session && session->has_id ? session->node_id : 0;
        StackHello hello{};
        if (!StackHello::decode(frame.payload.data(), frame.payload.size(), hello))
            return 0;
        return hello.node_id;
    }

    void notifyEvent_(uint32_t node_id, bool online)
    {
        if (_event_cb)
            _event_cb(_event_ctx, node_id, online);
    }
};
