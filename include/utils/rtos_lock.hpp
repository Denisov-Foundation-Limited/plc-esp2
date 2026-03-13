#pragma once

#include <stdint.h>

#if defined(ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
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
        return xSemaphoreTakeRecursive(_mtx, ticks) == pdTRUE;
#else
        (void)timeout_ms;
        return true;
#endif
    }

    void unlock() const
    {
#if defined(ESP32)
        if (_mtx != nullptr)
            xSemaphoreGiveRecursive(_mtx);
#endif
    }

    Guard guard(uint32_t timeout_ms = 0xFFFFFFFFu) const
    {
        return Guard(*this, timeout_ms);
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
#endif
};
