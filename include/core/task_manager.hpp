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
#include <stddef.h>
#include <stdint.h>
#include <type_traits>

constexpr uint8_t TASK_MGR_TSK_COUNT = 24;

template <size_t N>
class TaskManager
{
public:
    using TaskId = uint16_t;
    static constexpr TaskId kInvalidId = 0;

    using Callback = void (*)(void *);

    enum class CatchUp : uint8_t
    {
        No,
        Yes
    };

    enum class Priority : uint8_t
    {
        Highest = 0,
        High = 32,
        Normal = 128,
        Low = 192,
        Lowest = 255
    };

    static constexpr Priority DefaultPriority = Priority::Normal;
    static_assert(static_cast<uint8_t>(DefaultPriority) <= 255, "DefaultPriority out of range");

    struct Options
    {
        uint32_t interval_ms = 0; // 0 => oneshot
        uint32_t delay_ms = 0;    // start delay
        uint32_t deadline_ms = 0; // relative deadline from scheduled start, 0 => none
        Priority priority = DefaultPriority;
        bool enabled = true;
        CatchUp catch_up = CatchUp::No;
    };

    struct Handle
    {
        TaskId id = kInvalidId;
        explicit operator bool() const { return id != kInvalidId; }
    };

    TaskManager() { reset_(); }

    // C-style callback
    Handle add(Callback cb, void *ctx, const Options &opt)
    {
        if (!cb)
            return {};

        const size_t idx = findFree_();
        if (idx == kNoIndex)
            return {};

        const uint8_t pr = static_cast<uint8_t>(opt.priority);
        const uint32_t now = nowMs_();

        TaskSlot &t = tasks_[idx];
        t.id = nextId_();
        t.cb = cb;
        t.ctx = ctx;
        t.interval_ms = opt.interval_ms;
        t.oneshot = (opt.interval_ms == 0);
        t.enabled = opt.enabled;
        t.catch_up = opt.catch_up;
        t.priority = pr;

        t.next_ms = now + opt.delay_ms;

        t.deadline_rel_ms = opt.deadline_ms;
        t.deadline_abs_ms = opt.deadline_ms ? (t.next_ms + opt.deadline_ms) : 0;

        setPrioActive_(pr);
        return {t.id};
    }

    // Methods as tasks: tm.add<&T::method>(obj, opts);
    template <auto Method, typename T>
    Handle add(T &obj, const Options &opt)
    {
        static_assert(isValidMember_<decltype(Method)>(),
                      "Method must be: void (T::*)() or void (T::*)() const");
        return add(&memberThunk_<Method, T>, (void *)&obj, opt);
    }

    void loop(uint32_t budget_us = 0)
    {
        const uint32_t start_us = micros_();
        const uint32_t now = nowMs_();

        // Iterate only active priorities (bitmask)
        for (size_t w = 0; w < kPrioWords; ++w)
        {
            uint32_t bits = active_prio_[w];
            while (bits)
            {
                const uint32_t lsb = bits & (~bits + 1u); // bits & -bits (portable unsigned)
                const uint8_t pr = static_cast<uint8_t>((w << 5) + ctz32_(lsb));
                bits &= (bits - 1u);

                if (!runPriority_(pr, now, start_us, budget_us))
                    return;
            }
        }
    }

    bool enable(Handle h, bool on = true)
    {
        TaskSlot *t = findById_(h.id);
        if (!t)
            return false;
        t->enabled = on;
        return true;
    }

    bool remove(Handle h)
    {
        TaskSlot *t = findById_(h.id);
        if (!t)
            return false;

        const uint8_t pr = t->priority;
        *t = TaskSlot{};
        clearPrioActiveIfUnused_(pr);
        return true;
    }

    bool reschedule(Handle h, uint32_t delay_ms)
    {
        TaskSlot *t = findById_(h.id);
        if (!t)
            return false;

        const uint32_t now = nowMs_();
        t->next_ms = now + delay_ms;

        t->deadline_abs_ms = t->deadline_rel_ms ? (t->next_ms + t->deadline_rel_ms) : 0;
        setPrioActive_(t->priority);
        return true;
    }

    constexpr size_t capacity() const { return N; }

    size_t used() const
    {
        size_t c = 0;
        for (size_t i = 0; i < N; ++i)
            if (tasks_[i].id != kInvalidId)
                ++c;
        return c;
    }

private:
    struct TaskSlot
    {
        TaskId id = kInvalidId;
        Callback cb = nullptr;
        void *ctx = nullptr;

        uint32_t interval_ms = 0;
        uint32_t next_ms = 0;

        uint32_t deadline_rel_ms = 0; // 0 => none
        uint32_t deadline_abs_ms = 0; // 0 => none

        uint8_t priority = static_cast<uint8_t>(DefaultPriority);
        bool enabled = false;
        bool oneshot = false;
        CatchUp catch_up = CatchUp::No;
    };

    TaskSlot tasks_[N];
    TaskId id_counter_ = 1;

    // Active priorities bitmask (256 bits)
    static constexpr size_t kPrioBits = 256;
    static constexpr size_t kPrioWords = kPrioBits / 32;
    uint32_t active_prio_[kPrioWords] = {};

    // RR cursor per priority: stores (last_index + 1), 0 = none yet
    uint16_t rr_cursor_[kPrioBits] = {};

    static constexpr size_t kNoIndex = (size_t)-1;

    void reset_()
    {
        for (size_t i = 0; i < N; ++i)
            tasks_[i] = TaskSlot{};
        for (size_t w = 0; w < kPrioWords; ++w)
            active_prio_[w] = 0;
        for (size_t p = 0; p < kPrioBits; ++p)
            rr_cursor_[p] = 0;
    }

    TaskId nextId_()
    {
        ++id_counter_;
        if (id_counter_ == kInvalidId)
            ++id_counter_;
        return id_counter_;
    }

    size_t findFree_() const
    {
        for (size_t i = 0; i < N; ++i)
            if (tasks_[i].id == kInvalidId)
                return i;
        return kNoIndex;
    }

    TaskSlot *findById_(TaskId id)
    {
        if (id == kInvalidId)
            return nullptr;
        for (size_t i = 0; i < N; ++i)
            if (tasks_[i].id == id)
                return &tasks_[i];
        return nullptr;
    }

    static bool timeDue_(uint32_t now, uint32_t at)
    {
        return (int32_t)(now - at) >= 0;
    }

    // Compare deadlines where 0 means "no deadline" (treated as infinity).
    static bool earlierDeadline_(uint32_t a, uint32_t b)
    {
        if (a == 0)
            return false; // a has no deadline -> never earlier than b
        if (b == 0)
            return true; // b has no deadline -> a is earlier
        return (int32_t)(a - b) < 0;
    }

    // --- Active priority bitmask helpers ---
    static constexpr size_t prioWord_(uint8_t p) { return p >> 5; }
    static constexpr uint32_t prioMask_(uint8_t p) { return 1u << (p & 31); }

    void setPrioActive_(uint8_t p)
    {
        active_prio_[prioWord_(p)] |= prioMask_(p);
    }

    void clearPrioActiveIfUnused_(uint8_t p)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (tasks_[i].id != kInvalidId && tasks_[i].priority == p)
                return;
        }
        active_prio_[prioWord_(p)] &= ~prioMask_(p);
        rr_cursor_[p] = 0;
    }

    // Count trailing zeros for a power-of-two mask (lsb).
    static uint8_t ctz32_(uint32_t v)
    {
#if defined(__GNUC__)
        return static_cast<uint8_t>(__builtin_ctz(v));
#else
        uint8_t c = 0;
        while ((v & 1u) == 0u)
        {
            v >>= 1;
            ++c;
        }
        return c;
#endif
    }

    // Run EDF within priority; RR as tie-breaker via rotated scan start.
    bool runPriority_(uint8_t pr, uint32_t now, uint32_t start_us, uint32_t budget_us)
    {
        size_t best = kNoIndex;

        const size_t start = rr_cursor_[pr] ? (rr_cursor_[pr] % N) : 0;

        // 1) Find best runnable task by earliest deadline (0 = no deadline)
        for (size_t k = 0; k < N; ++k)
        {
            const size_t i = (start + k) % N;
            TaskSlot &t = tasks_[i];

            if (t.id == kInvalidId || !t.enabled || t.priority != pr)
                continue;
            if (!timeDue_(now, t.next_ms))
                continue;

            if (best == kNoIndex ||
                earlierDeadline_(t.deadline_abs_ms, tasks_[best].deadline_abs_ms))
            {
                best = i;
            }
        }

        if (best == kNoIndex)
            return true;

        // 2) Execute selected task
        TaskSlot &t = tasks_[best];
        t.cb(t.ctx);
        rr_cursor_[pr] = static_cast<uint16_t>(best + 1);

        if (t.oneshot)
        {
            const uint8_t p = t.priority;
            t = TaskSlot{};
            clearPrioActiveIfUnused_(p);
        }
        else
        {
            // Reschedule next
            if (t.catch_up == CatchUp::Yes)
            {
                uint32_t next = t.next_ms;
                do
                {
                    next += t.interval_ms;
                } while (timeDue_(now, next));
                t.next_ms = next;
            }
            else
            {
                t.next_ms = now + t.interval_ms;
            }

            // Recompute absolute deadline from next_ms
            t.deadline_abs_ms = t.deadline_rel_ms ? (t.next_ms + t.deadline_rel_ms) : 0;

            // ensure pr remains active
            setPrioActive_(t.priority);
        }

        if (budget_us && (micros_() - start_us) >= budget_us)
            return false;

        return true;
    }

    // --- member thunk machinery ---
    template <typename M>
    struct IsVoidNoArgMember : std::false_type
    {
    };
    template <typename C>
    struct IsVoidNoArgMember<void (C::*)()> : std::true_type
    {
    };
    template <typename C>
    struct IsVoidNoArgMember<void (C::*)() const> : std::true_type
    {
    };
    template <typename C>
    struct IsVoidNoArgMember<void (C::*)() noexcept> : std::true_type
    {
    };
    template <typename C>
    struct IsVoidNoArgMember<void (C::*)() const noexcept> : std::true_type
    {
    };

    template <typename M>
    static constexpr bool isValidMember_()
    {
        return IsVoidNoArgMember<M>::value;
    }

    template <auto Method, typename T>
    static void memberThunk_(void *p)
    {
        T *obj = static_cast<T *>(p);
        (obj->*Method)();
    }

    // Arduino hooks
    static uint32_t nowMs_()
    {
#if defined(ARDUINO)
        return (uint32_t)::millis();
#else
        using namespace std::chrono;
        return (uint32_t)duration_cast<milliseconds>(
                   steady_clock::now().time_since_epoch())
            .count();
#endif
    }
    static uint32_t micros_()
    {
#if defined(ARDUINO)
        return (uint32_t)::micros();
#else
        using namespace std::chrono;
        return (uint32_t)duration_cast<microseconds>(
                   steady_clock::now().time_since_epoch())
            .count();
#endif
    }
};
