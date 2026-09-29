// SPDX-License-Identifier: GPL-3.0-or-later
#include "EffectRetirementWorker.h"
#include <chrono>

EffectRetirementWorker::EffectRetirementWorker() : worker_([this] { run(); }) {}
EffectRetirementWorker::~EffectRetirementWorker()
{
    stopping_.store(true, std::memory_order_release);
    worker_.join();
}
bool EffectRetirementWorker::available(std::size_t index) const
{
    return index < capacity && !slots_[index].occupied.load(std::memory_order_acquire);
}
bool EffectRetirementWorker::retire(std::size_t index, std::unique_ptr<Effect> &effect, UserData &data)
{
    if (!available(index)) return false;
    auto &slot = slots_[index];
    slot.effect.swap(effect);
    slot.data.swap(data);
    slot.occupied.store(true, std::memory_order_release);
    return true;
}
void EffectRetirementWorker::run()
{
    for (;;)
    {
        const bool stopping = stopping_.load(std::memory_order_acquire);
        for (auto &slot : slots_)
        {
            if (!slot.occupied.load(std::memory_order_acquire)) continue;
            slot.effect.reset();
            // Release hash buckets as well as samples on this thread. The next
            // producer receives an empty map with no old allocation to reclaim.
            UserData{}.swap(slot.data);
            retired_.fetch_add(1, std::memory_order_relaxed);
            slot.occupied.store(false, std::memory_order_release);
        }
        if (stopping) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}
