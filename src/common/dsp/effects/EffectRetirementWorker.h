// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Effect.h"
#include <array>
#include <atomic>
#include <thread>

// One serialized engine owner publishes; one worker destroys. Construct/join
// off audio, and stop the producer before destruction. Retired effect destructors
// must not mutate live engine state (detach such hooks before publishing).
class EffectRetirementWorker
{
  public:
    static constexpr std::size_t capacity = n_fx_slots;
    using UserData = decltype(FxStorage::user_data);
    EffectRetirementWorker();
    ~EffectRetirementWorker();
    EffectRetirementWorker(const EffectRetirementWorker &) = delete;
    EffectRetirementWorker &operator=(const EffectRetirementWorker &) = delete;

    // The worker never occupies an empty slot. After available(), the serialized
    // producer can prepare a replacement and retire into that slot without races.
    bool available(std::size_t slot) const;
    // Full: leave both arguments untouched. Success: swap into empty storage,
    // without allocation, destruction, locks or notification on the producer.
    bool retire(std::size_t slot, std::unique_ptr<Effect> &effect, UserData &data);
    std::uint32_t retired() const { return retired_.load(std::memory_order_relaxed); }

  private:
    struct Slot
    {
        std::atomic<bool> occupied{false};
        std::unique_ptr<Effect> effect;
        UserData data;
    };
    static_assert(std::atomic<bool>::is_always_lock_free);
    std::array<Slot, capacity> slots_;
    std::atomic<bool> stopping_{false};
    std::atomic<std::uint32_t> retired_{0};
    std::thread worker_;
    void run();
};
