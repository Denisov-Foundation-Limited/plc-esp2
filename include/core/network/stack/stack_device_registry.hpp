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
#include <string.h>

#include "utils/rtos_lock.hpp"

class StackDeviceRegistry
{
public:
    static constexpr size_t kMaxDevices = 32;
    static constexpr size_t kNameLen = 32;
    static constexpr size_t kIpLen = 16;

    struct DeviceInfo
    {
        bool online = false;
        uint8_t client_id = 0;
        uint32_t node_id = 0;
        uint32_t caps = 0;
        uint16_t fw_version = 0;
        uint32_t connected_ms = 0;
        uint32_t last_seen_ms = 0;
        char name[kNameLen] = {};
        char ip[kIpLen] = {};
    };

    struct AuthInfo
    {
        uint32_t node_id = 0;
        uint32_t caps = 0;
        uint16_t fw_version = 0;
        const char *name = nullptr;
        const char *ip = nullptr;
    };

    void clear();
    size_t onlineCount() const;

    bool snapshotAt(size_t idx, DeviceInfo &out) const;
    bool snapshotByNodeId(uint32_t node_id, DeviceInfo &out) const;
    bool snapshotByClientId(uint8_t client_id, DeviceInfo &out) const;
    bool hasClientId(uint8_t client_id) const;

    bool upsert(uint8_t client_id, const AuthInfo &auth, const char *resolved_ip, DeviceInfo *out = nullptr);
    bool removeByClientId(uint8_t client_id);
    bool touchClient(uint8_t client_id, uint32_t now_ms);

private:
    DeviceInfo _devices[kMaxDevices]{};
    mutable RtosRecursiveLock _lock;

    static void copyText_(char *dst, size_t cap, const char *src);
    DeviceInfo *findByNodeIdUnsafe_(uint32_t node_id);
    const DeviceInfo *findByNodeIdUnsafe_(uint32_t node_id) const;
    DeviceInfo *findByClientIdUnsafe_(uint8_t client_id);
    const DeviceInfo *findByClientIdUnsafe_(uint8_t client_id) const;
    DeviceInfo *allocateSlot_(uint32_t node_id, uint8_t client_id);
};
