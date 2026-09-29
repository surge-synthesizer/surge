// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <type_traits>

// Latest complete value from one serialized producer to one serialized consumer.
// Owners may move between threads only under external synchronization. Values
// may be superseded before consumption; this is state, not an event queue.
// Three slots keep each owner's current slot separate from the exchange slot.
// Neither operation waits, allocates, retries or accesses the other owner's slot.
template <class Value> class ControlSnapshot
{
    static_assert(std::is_trivially_copyable_v<Value>);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static constexpr unsigned dirty = 4, indexMask = 3;
    std::array<Value, 3> values{};
    unsigned producerSlot{2};
    unsigned consumerSlot{0};
    std::atomic<unsigned> exchangeSlot{1};

  public:
    void publish(const Value &next) noexcept
    {
        values[producerSlot] = next;
        producerSlot = exchangeSlot.exchange(producerSlot | dirty, std::memory_order_acq_rel) &
                       indexMask;
    }

    // The consumer retains its last value when nothing new has been published.
    // Copy to caller-owned storage so no slot reference can escape ownership.
    bool consume(Value &result) noexcept
    {
        if (!(exchangeSlot.load(std::memory_order_acquire) & dirty)) return false;
        consumerSlot = exchangeSlot.exchange(consumerSlot, std::memory_order_acq_rel) & indexMask;
        result = values[consumerSlot];
        return true;
    }
};
