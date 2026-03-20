#pragma once

#include <AsyncTCP.h>

#include "core/network/stack/stack_transport.hpp"

class AsyncTcpStackConnection : public StackTransportConnection
{
public:
    explicit AsyncTcpStackConnection(AsyncClient *client = nullptr, bool self_owned = false);
    ~AsyncTcpStackConnection() override;

    void attach(AsyncClient *client);

    bool connected() const override;
    bool canSend() const override;
    size_t write(const uint8_t *data, size_t len) override;
    void close(bool now) override;
    void destroy() override;
    String remoteIp() const override;

    void setDataHandler(DataHandler cb, void *ctx) override;
    void setDisconnectHandler(EventHandler cb, void *ctx) override;
    void setErrorHandler(ErrorHandler cb, void *ctx) override;
    void setConnectHandler(EventHandler cb, void *ctx) override;

private:
    AsyncClient *_client = nullptr;
    bool _self_owned = false;
    DataHandler _data_cb = nullptr;
    void *_data_ctx = nullptr;
    EventHandler _disconnect_cb = nullptr;
    void *_disconnect_ctx = nullptr;
    ErrorHandler _error_cb = nullptr;
    void *_error_ctx = nullptr;
    EventHandler _connect_cb = nullptr;
    void *_connect_ctx = nullptr;

    void bindCallbacks_();
    void clearCallbacks_();
};

class AsyncTcpStackServerTransport : public StackTransportServer
{
public:
    explicit AsyncTcpStackServerTransport(uint16_t port);

    void setClientHandler(ClientHandler cb, void *ctx) override;
    void begin() override;

private:
    AsyncServer _server;
    ClientHandler _client_cb = nullptr;
    void *_client_ctx = nullptr;
};

class AsyncTcpStackClientTransport : public StackTransportClient
{
public:
    AsyncTcpStackClientTransport();

    bool connected() const override;
    bool canSend() const override;
    size_t write(const uint8_t *data, size_t len) override;
    void close(bool now) override;
    bool connect(const char *host, uint16_t port) override;

    void setDataHandler(StackTransportConnection::DataHandler cb, void *ctx) override;
    void setDisconnectHandler(StackTransportConnection::EventHandler cb, void *ctx) override;
    void setErrorHandler(StackTransportConnection::ErrorHandler cb, void *ctx) override;
    void setConnectHandler(StackTransportConnection::EventHandler cb, void *ctx) override;

private:
    AsyncClient _client;
    AsyncTcpStackConnection _conn;
};
