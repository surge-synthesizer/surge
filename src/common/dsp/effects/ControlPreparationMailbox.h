// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <memory>
#include <type_traits>

// One serialized audio producer/consumer and one control service owner. A single
// in-flight request bounds memory. Only control constructs or reclaims Prepared.
// Destroy after both owners have stopped using the mailbox.
template <class Request, class Prepared> class ControlPreparationMailbox
{
    enum State : unsigned { idle, requested, ready, failed, consumed };
    static_assert(std::is_trivially_copyable_v<Request>);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::atomic<unsigned> state{idle};
    Request request{};
    std::unique_ptr<Prepared> prepared;
  public:
    bool submit(const Request &next) noexcept
    {
        if (state.load(std::memory_order_acquire) != idle) return false;
        request = next;
        state.store(requested, std::memory_order_release);
        return true;
    }
    template <class Factory> void service(Factory &&factory)
    {
        const auto current = state.load(std::memory_order_acquire);
        if (current == consumed)
        {
            prepared.reset();
            state.store(idle, std::memory_order_release);
        }
        else if (current == requested)
        {
            try
            {
                prepared = factory(request);
                state.store(prepared ? ready : failed, std::memory_order_release);
            }
            catch (...)
            {
                state.store(failed, std::memory_order_release);
                throw;
            }
        }
    }
    template <class Consumer> void consume(Consumer &&consumer) noexcept
    {
        static_assert(noexcept(consumer(request, prepared.get())));
        const auto current = state.load(std::memory_order_acquire);
        if (current != ready && current != failed) return;
        consumer(request, current == ready ? prepared.get() : nullptr);
        state.store(consumed, std::memory_order_release);
    }
};
