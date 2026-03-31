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

#include "core/network/stack/stack_device_registry.hpp"

void StackDeviceRegistry::clear()
{
    const auto guard = _lock.guard();
    memset(_devices, 0, sizeof(_devices));
}

size_t StackDeviceRegistry::onlineCount() const
{
    const auto guard = _lock.guard();
    size_t count = 0;
    for (const DeviceInfo &device : _devices)
    {
        if (device.online)
            ++count;
    }
    return count;
}

bool StackDeviceRegistry::snapshotAt(size_t idx, DeviceInfo &out) const
{
    const auto guard = _lock.guard();
    if (idx >= kMaxDevices)
        return false;
    out = _devices[idx];
    return true;
}

bool StackDeviceRegistry::snapshotByNodeId(uint32_t node_id, DeviceInfo &out) const
{
    const auto guard = _lock.guard();
    const DeviceInfo *device = findByNodeIdUnsafe_(node_id);
    if (!device)
        return false;
    out = *device;
    return true;
}

bool StackDeviceRegistry::snapshotByClientId(uint8_t client_id, DeviceInfo &out) const
{
    const auto guard = _lock.guard();
    const DeviceInfo *device = findByClientIdUnsafe_(client_id);
    if (!device)
        return false;
    out = *device;
    return true;
}

bool StackDeviceRegistry::hasClientId(uint8_t client_id) const
{
    const auto guard = _lock.guard();
    return findByClientIdUnsafe_(client_id) != nullptr;
}

StackDeviceRegistry::DeviceInfo *StackDeviceRegistry::findByNodeIdUnsafe_(uint32_t node_id)
{
    for (DeviceInfo &device : _devices)
    {
        if (device.online && device.node_id == node_id)
            return &device;
    }
    return nullptr;
}

const StackDeviceRegistry::DeviceInfo *StackDeviceRegistry::findByNodeIdUnsafe_(uint32_t node_id) const
{
    for (const DeviceInfo &device : _devices)
    {
        if (device.online && device.node_id == node_id)
            return &device;
    }
    return nullptr;
}

StackDeviceRegistry::DeviceInfo *StackDeviceRegistry::findByClientIdUnsafe_(uint8_t client_id)
{
    for (DeviceInfo &device : _devices)
    {
        if (device.online && device.client_id == client_id)
            return &device;
    }
    return nullptr;
}

const StackDeviceRegistry::DeviceInfo *StackDeviceRegistry::findByClientIdUnsafe_(uint8_t client_id) const
{
    for (const DeviceInfo &device : _devices)
    {
        if (device.online && device.client_id == client_id)
            return &device;
    }
    return nullptr;
}

bool StackDeviceRegistry::upsert(uint8_t client_id, const AuthInfo &auth, const char *resolved_ip, DeviceInfo *out)
{
    const auto guard = _lock.guard();
    DeviceInfo *device = allocateSlot_(auth.node_id, client_id);
    if (!device)
        return false;

    memset(device, 0, sizeof(*device));
    device->online = true;
    device->client_id = client_id;
    device->node_id = auth.node_id;
    device->caps = auth.caps;
    device->fw_version = auth.fw_version;
    device->connected_ms = millis();
    device->last_seen_ms = device->connected_ms;
    copyText_(device->name, sizeof(device->name), auth.name);
    copyText_(device->ip, sizeof(device->ip), auth.ip && auth.ip[0] ? auth.ip : resolved_ip);
    if (out)
        *out = *device;
    return true;
}

bool StackDeviceRegistry::removeByClientId(uint8_t client_id)
{
    const auto guard = _lock.guard();
    DeviceInfo *device = findByClientIdUnsafe_(client_id);
    if (!device)
        return false;
    memset(device, 0, sizeof(*device));
    return true;
}

bool StackDeviceRegistry::touchClient(uint8_t client_id, uint32_t now_ms)
{
    const auto guard = _lock.guard();
    DeviceInfo *device = findByClientIdUnsafe_(client_id);
    if (!device)
        return false;
    device->last_seen_ms = now_ms;
    return true;
}

void StackDeviceRegistry::copyText_(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0)
        return;
    dst[0] = '\0';
    if (!src)
        return;
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

StackDeviceRegistry::DeviceInfo *StackDeviceRegistry::allocateSlot_(uint32_t node_id, uint8_t client_id)
{
    if (DeviceInfo *device = findByNodeIdUnsafe_(node_id))
        return device;
    if (DeviceInfo *device = findByClientIdUnsafe_(client_id))
        return device;
    for (DeviceInfo &device : _devices)
    {
        if (!device.online)
            return &device;
    }
    return nullptr;
}
