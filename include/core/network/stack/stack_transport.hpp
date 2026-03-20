#pragma once

#include <Arduino.h>
#include <stdint.h>

class StackTransportConnection
{
public:
    using DataHandler = void (*)(void *ctx, StackTransportConnection *conn, const uint8_t *data, size_t len);
    using EventHandler = void (*)(void *ctx, StackTransportConnection *conn);
    using ErrorHandler = void (*)(void *ctx, StackTransportConnection *conn, int8_t err);

    virtual ~StackTransportConnection() = default;

    virtual bool connected() const = 0;
    virtual bool canSend() const = 0;
    virtual size_t write(const uint8_t *data, size_t len) = 0;
    virtual void close(bool now) = 0;
    virtual void destroy() = 0;
    virtual String remoteIp() const = 0;

    virtual void setDataHandler(DataHandler cb, void *ctx) = 0;
    virtual void setDisconnectHandler(EventHandler cb, void *ctx) = 0;
    virtual void setErrorHandler(ErrorHandler cb, void *ctx) = 0;
    virtual void setConnectHandler(EventHandler cb, void *ctx) = 0;
};

class StackTransportServer
{
public:
    using ClientHandler = void (*)(void *ctx, StackTransportConnection *conn);

    virtual ~StackTransportServer() = default;
    virtual void setClientHandler(ClientHandler cb, void *ctx) = 0;
    virtual void begin() = 0;
};

class StackTransportClient
{
public:
    virtual ~StackTransportClient() = default;

    virtual bool connected() const = 0;
    virtual bool canSend() const = 0;
    virtual size_t write(const uint8_t *data, size_t len) = 0;
    virtual void close(bool now) = 0;
    virtual bool connect(const char *host, uint16_t port) = 0;

    virtual void setDataHandler(StackTransportConnection::DataHandler cb, void *ctx) = 0;
    virtual void setDisconnectHandler(StackTransportConnection::EventHandler cb, void *ctx) = 0;
    virtual void setErrorHandler(StackTransportConnection::ErrorHandler cb, void *ctx) = 0;
    virtual void setConnectHandler(StackTransportConnection::EventHandler cb, void *ctx) = 0;
};
