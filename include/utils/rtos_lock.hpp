#pragma once

#include <stdint.h>

#ifndef RTOS_LOCK_DIAG
#define RTOS_LOCK_DIAG 0
#endif

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#if RTOS_LOCK_DIAG
#include <freertos/task.h>
#endif
#endif

class RtosRecursiveLock
{
public:
    class Guard
    {
    public:
        explicit Guard(const RtosRecursiveLock &lock, uint32_t timeout_ms = kWaitForeverMs)
            : _lock(lock), _locked(lock.lock(timeout_ms))
        {
        }

        ~Guard()
        {
            if (_locked)
                _lock.unlock();
        }

        Guard(const Guard &) = delete;
        Guard &operator=(const Guard &) = delete;

        bool locked() const { return _locked; }

    private:
        static constexpr uint32_t kWaitForeverMs = 0xFFFFFFFFu;

        const RtosRecursiveLock &_lock;
        bool _locked = false;
    };

    RtosRecursiveLock() = default;
    RtosRecursiveLock(const RtosRecursiveLock &) = delete;
    RtosRecursiveLock &operator=(const RtosRecursiveLock &) = delete;

    bool lock(uint32_t timeout_ms = 0xFFFFFFFFu) const
    {
#if defined(ESP32)
        ensureCreated_();
        if (_mtx == nullptr)
            return false;
        const TickType_t ticks = (timeout_ms == 0xFFFFFFFFu) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
        if (xSemaphoreTakeRecursive(_mtx, ticks) != pdTRUE)
            return false;
#if RTOS_LOCK_DIAG
        const TaskHandle_t current = xTaskGetCurrentTaskHandle();
        if (_owner == current)
        {
            ++_depth;
        }
        else
        {
            _owner = current;
            _depth = 1;
            _lock_tick = xTaskGetTickCount();
        }
#endif
        return true;
#else
        (void)timeout_ms;
        return true;
#endif
    }

    void unlock() const
    {
#if defined(ESP32)
        if (_mtx != nullptr)
        {
#if RTOS_LOCK_DIAG
            const TaskHandle_t current = xTaskGetCurrentTaskHandle();
            if (_owner == current)
            {
                if (_depth > 1)
                    --_depth;
                else
                {
                    _depth = 0;
                    _owner = nullptr;
                    _lock_tick = 0;
                }
            }
#endif
            xSemaphoreGiveRecursive(_mtx);
        }
#endif
    }

    Guard guard(uint32_t timeout_ms = 0xFFFFFFFFu) const
    {
        return Guard(*this, timeout_ms);
    }

    const char *ownerName() const
    {
#if defined(ESP32) && RTOS_LOCK_DIAG
        return _owner ? pcTaskGetName(_owner) : nullptr;
#else
        return nullptr;
#endif
    }

    uint32_t heldMs() const
    {
#if defined(ESP32) && RTOS_LOCK_DIAG
        if (_owner == nullptr || _lock_tick == 0)
            return 0;
        const TickType_t now = xTaskGetTickCount();
        return (uint32_t)(now - _lock_tick) * (uint32_t)portTICK_PERIOD_MS;
#else
        return 0;
#endif
    }

private:
    void ensureCreated_() const
    {
#if defined(ESP32)
        if (_mtx != nullptr)
            return;
        portENTER_CRITICAL(&_init_mux);
        if (_mtx == nullptr)
            _mtx = xSemaphoreCreateRecursiveMutex();
        portEXIT_CRITICAL(&_init_mux);
#endif
    }

#if defined(ESP32)
    mutable SemaphoreHandle_t _mtx = nullptr;
    mutable portMUX_TYPE _init_mux = portMUX_INITIALIZER_UNLOCKED;
#if RTOS_LOCK_DIAG
    mutable TaskHandle_t _owner = nullptr;
    mutable UBaseType_t _depth = 0;
    mutable TickType_t _lock_tick = 0;
#endif
#endif
};
