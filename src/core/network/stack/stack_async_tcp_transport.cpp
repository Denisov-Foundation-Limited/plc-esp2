#include "core/network/stack/stack_async_tcp_transport.hpp"

namespace
{
void noopData_(void *, AsyncClient *, void *, size_t) {}
void noopEvent_(void *, AsyncClient *) {}
void noopError_(void *, AsyncClient *, int8_t) {}
} // namespace

AsyncTcpStackConnection::AsyncTcpStackConnection(AsyncClient *client, bool self_owned)
{
    _self_owned = self_owned;
    attach(client);
}

AsyncTcpStackConnection::~AsyncTcpStackConnection()
{
    clearCallbacks_();
}

void AsyncTcpStackConnection::attach(AsyncClient *client)
{
    clearCallbacks_();
    _client = client;
    bindCallbacks_();
}

bool AsyncTcpStackConnection::connected() const
{
    return _client && _client->connected();
}

bool AsyncTcpStackConnection::canSend() const
{
    return _client && _client->canSend();
}

size_t AsyncTcpStackConnection::write(const uint8_t *data, size_t len)
{
    if (!_client || !data || len == 0)
        return 0;
    return _client->write(reinterpret_cast<const char *>(data), len);
}

void AsyncTcpStackConnection::close(bool now)
{
    if (_client)
        _client->close(now);
}

void AsyncTcpStackConnection::destroy()
{
    clearCallbacks_();
    if (_self_owned)
        delete this;
}

String AsyncTcpStackConnection::remoteIp() const
{
    return _client ? _client->remoteIP().toString() : String();
}

void AsyncTcpStackConnection::setDataHandler(DataHandler cb, void *ctx)
{
    _data_cb = cb;
    _data_ctx = ctx;
}

void AsyncTcpStackConnection::setDisconnectHandler(EventHandler cb, void *ctx)
{
    _disconnect_cb = cb;
    _disconnect_ctx = ctx;
}

void AsyncTcpStackConnection::setErrorHandler(ErrorHandler cb, void *ctx)
{
    _error_cb = cb;
    _error_ctx = ctx;
}

void AsyncTcpStackConnection::setConnectHandler(EventHandler cb, void *ctx)
{
    _connect_cb = cb;
    _connect_ctx = ctx;
}

void AsyncTcpStackConnection::bindCallbacks_()
{
    if (!_client)
        return;
    _client->onData(
        [](void *arg, AsyncClient *, void *data, size_t len) {
            AsyncTcpStackConnection *self = static_cast<AsyncTcpStackConnection *>(arg);
            if (self && self->_data_cb)
                self->_data_cb(self->_data_ctx, self, static_cast<const uint8_t *>(data), len);
        },
        this);
    _client->onDisconnect(
        [](void *arg, AsyncClient *) {
            AsyncTcpStackConnection *self = static_cast<AsyncTcpStackConnection *>(arg);
            if (self && self->_disconnect_cb)
                self->_disconnect_cb(self->_disconnect_ctx, self);
        },
        this);
    _client->onError(
        [](void *arg, AsyncClient *, int8_t err) {
            AsyncTcpStackConnection *self = static_cast<AsyncTcpStackConnection *>(arg);
            if (self && self->_error_cb)
                self->_error_cb(self->_error_ctx, self, err);
        },
        this);
    _client->onConnect(
        [](void *arg, AsyncClient *) {
            AsyncTcpStackConnection *self = static_cast<AsyncTcpStackConnection *>(arg);
            if (self && self->_connect_cb)
                self->_connect_cb(self->_connect_ctx, self);
        },
        this);
}

void AsyncTcpStackConnection::clearCallbacks_()
{
    if (!_client)
        return;
    _client->onData(noopData_, nullptr);
    _client->onDisconnect(noopEvent_, nullptr);
    _client->onError(noopError_, nullptr);
    _client->onConnect(noopEvent_, nullptr);
}

AsyncTcpStackServerTransport::AsyncTcpStackServerTransport(uint16_t port) : _server(port) {}

void AsyncTcpStackServerTransport::setClientHandler(ClientHandler cb, void *ctx)
{
    _client_cb = cb;
    _client_ctx = ctx;
}

void AsyncTcpStackServerTransport::begin()
{
    _server.onClient(
        [](void *arg, AsyncClient *client) {
            AsyncTcpStackServerTransport *self = static_cast<AsyncTcpStackServerTransport *>(arg);
            if (!self || !self->_client_cb || !client)
                return;
            AsyncTcpStackConnection *conn = new AsyncTcpStackConnection(client, true);
            self->_client_cb(self->_client_ctx, conn);
        },
        this);
    _server.begin();
}

AsyncTcpStackClientTransport::AsyncTcpStackClientTransport() : _conn(&_client) {}

bool AsyncTcpStackClientTransport::connected() const
{
    return _conn.connected();
}

bool AsyncTcpStackClientTransport::canSend() const
{
    return _conn.canSend();
}

size_t AsyncTcpStackClientTransport::write(const uint8_t *data, size_t len)
{
    return _conn.write(data, len);
}

void AsyncTcpStackClientTransport::close(bool now)
{
    _conn.close(now);
}

bool AsyncTcpStackClientTransport::connect(const char *host, uint16_t port)
{
    return _client.connect(host, port);
}

void AsyncTcpStackClientTransport::setDataHandler(StackTransportConnection::DataHandler cb, void *ctx)
{
    _conn.setDataHandler(cb, ctx);
}

void AsyncTcpStackClientTransport::setDisconnectHandler(StackTransportConnection::EventHandler cb, void *ctx)
{
    _conn.setDisconnectHandler(cb, ctx);
}

void AsyncTcpStackClientTransport::setErrorHandler(StackTransportConnection::ErrorHandler cb, void *ctx)
{
    _conn.setErrorHandler(cb, ctx);
}

void AsyncTcpStackClientTransport::setConnectHandler(StackTransportConnection::EventHandler cb, void *ctx)
{
    _conn.setConnectHandler(cb, ctx);
}
