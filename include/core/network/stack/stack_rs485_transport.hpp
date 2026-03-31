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
#include <IPAddress.h>
#include <stdint.h>

#include "core/network/stack/stack_rs485_protocol.hpp"
#include "core/network/stack/stack_transport.hpp"
#include "utils/rtos_lock.hpp"

class Logger;

class StackRs485Transport : public StackTransport
{
public:
    // Transport is the lowest lock level in stack core.
    // Never call back into StackRouteAdapter while holding external locks above transport.
    struct Config
    {
        uint8_t uart_index = 0;
        uint32_t baud_rate = 115200;
        int8_t de_port = -1;
        int8_t re_port = -1;
        bool half_duplex = true;
        size_t max_frame_size = 512;
        uint16_t turnaround_tx_ms = 2;
        uint16_t turnaround_rx_ms = 2;
        uint32_t request_timeout_ms = 1000;
    };

    enum class BusState : uint8_t
    {
        Idle = 0,
        TurnaroundTx,
        Tx,
        TurnaroundRx,
        Rx
    };

    struct Diagnostics
    {
        uint16_t tx_queue_used = 0;
        uint16_t pending_used = 0;
        uint32_t request_timeouts = 0;
        uint32_t tx_queue_drops = 0;
        uint32_t pending_full_drops = 0;
        uint32_t last_tx_size = 0;
        uint32_t lock_held_ms = 0;
        BusState bus_state = BusState::Idle;
        bool started = false;
    };

    explicit StackRs485Transport(Logger &log);

    void setConfig(const Config &cfg);
    Config config() const;

    void setEventHandler(EventHandler cb, void *ctx) override;
    bool begin(uint16_t port) override;
    void loop() override;
    void stop() override;
    Caps caps() const override;

    bool sendText(uint8_t client_id, const char *text) override;
    bool sendBinary(uint8_t client_id, const uint8_t *data, size_t size) override;
    bool disconnectClient(uint8_t client_id) override;

    bool injectFrame(const uint8_t *frame, size_t size, uint8_t client_id = 1);
    size_t lastTxSize() const;
    bool copyLastTxData(uint8_t *out, size_t cap, size_t &out_size) const;
    Diagnostics diagnostics() const;

private:
    static constexpr size_t kTxBufferCap = 768;
    static constexpr size_t kQueueCap = 8;
    static constexpr size_t kPendingCap = 8;

    struct TxEntry
    {
        bool used = false;
        uint8_t client_id = 0;
        uint8_t msg_type = 0;
        StackTransport::RouteMeta meta{};
        uint8_t frame[kTxBufferCap] = {};
        size_t frame_size = 0;
    };

    struct PendingEntry
    {
        bool used = false;
        uint8_t client_id = 0;
        uint8_t msg_type = 0;
        uint32_t request_id = 0;
        uint32_t deadline_ms = 0;
    };

    Logger &_log;
    Config _cfg;
    EventHandler _event_cb = nullptr;
    void *_event_ctx = nullptr;
    bool _started = false;
    uint16_t _logical_port = 0;
    uint8_t _last_tx[kTxBufferCap] = {};
    size_t _last_tx_size = 0;
    BusState _bus_state = BusState::Idle;
    uint32_t _state_since_ms = 0;
    uint8_t _active_tx_index = 0xFFu;
    TxEntry _tx_queue[kQueueCap];
    PendingEntry _pending[kPendingCap];
    uint32_t _next_request_id = 1;
    uint32_t _request_timeout_count = 0;
    uint32_t _tx_queue_drop_count = 0;
    uint32_t _pending_full_drop_count = 0;
    mutable RtosRecursiveLock _lock;

    bool sendFrame_(uint8_t client_id, uint8_t msg_type, const uint8_t *payload, size_t size);
    bool enqueueFrame_(uint8_t client_id, uint8_t msg_type, const uint8_t *frame, size_t frame_size,
                       const StackTransport::RouteMeta &meta);
    bool tryParseRouteMeta_(uint8_t msg_type, const uint8_t *payload, size_t size, StackTransport::RouteMeta &meta);
    void assignRequestId_(StackTransport::RouteMeta &meta);
    void processBusState_();
    void checkPendingTimeouts_();
    void checkPendingTimeoutsLocked_(uint32_t now);
    void startNextTx_();
    void finishActiveTx_();
    void completePending_(const StackTransport::RouteMeta &meta);
    bool addPending_(uint8_t client_id, uint8_t msg_type, const StackTransport::RouteMeta &meta);
    int findFreeQueueSlot_() const;
    int findPendingByRequestId_(uint32_t request_id) const;
    static bool expired_(uint32_t now_ms, uint32_t deadline_ms);
};
