// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace Surge::Web
{
// One producer (browser main thread), one consumer (audio worklet). No allocation
// or locks. MIDI timestamps are absolute AudioContext frames, not wall time.
class MidiQueue
{
  public:
    static constexpr uint32_t capacity = 1024;
    struct Event
    {
        uint64_t frame;
        std::array<uint8_t, 3> bytes;
        uint8_t length;
    };
    bool push(int status, int first, int second, double frame)
    {
        if (status < 0x80 || status >= 0xf0 || first < 0 || first > 127 ||
            second < 0 || second > 127 || !std::isfinite(frame) || frame < 0 ||
            frame > 9007199254740991.)
            return false;
        const auto write = head.load(std::memory_order_relaxed);
        if (write - tail.load(std::memory_order_acquire) >= capacity)
        {
            overflows.fetch_add(1, std::memory_order_relaxed);
            panic();
            return false;
        }
        // Several input devices can deliver older timestamps in the same turn.
        // Preserve MIDI ordering and prevent an older event overtaking note-on.
        lastFrame = std::max(lastFrame, static_cast<uint64_t>(frame));
        events[write % capacity] = {lastFrame,
            {uint8_t(status), uint8_t(first), uint8_t(second)},
            uint8_t((status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0 ? 2 : 3)};
        head.store(write + 1, std::memory_order_release);
        return true;
    }
    void panic()
    {
        lastFrame = 0;
        panicGeneration.fetch_add(1, std::memory_order_release);
    }
    uint32_t pending() const
    {
        return head.load(std::memory_order_acquire) - tail.load(std::memory_order_acquire);
    }
    uint32_t overflowCount() const { return overflows.load(std::memory_order_relaxed); }

    // Returns true when the caller must send all-sound-off on every channel.
    // Work is bounded even if the producer runs concurrently with this drain.
    template <class Deliver> bool drain(uint64_t firstFrame, uint32_t frames, Deliver deliver)
    {
        const auto generation = panicGeneration.load(std::memory_order_acquire);
        if (generation != consumedPanic)
        {
            consumedPanic = generation;
            tail.store(head.load(std::memory_order_acquire), std::memory_order_release);
            return true;
        }
        auto read = tail.load(std::memory_order_relaxed);
        const auto end = head.load(std::memory_order_acquire);
        while (read != end)
        {
            const auto &event = events[read % capacity];
            if (event.frame >= firstFrame + frames)
                break;
            deliver(event, int(event.frame > firstFrame ? event.frame - firstFrame : 0));
            ++read;
        }
        tail.store(read, std::memory_order_release);
        return false;
    }
  private:
    std::array<Event, capacity> events{};
    alignas(64) std::atomic<uint32_t> head{0};
    alignas(64) std::atomic<uint32_t> tail{0};
    std::atomic<uint32_t> panicGeneration{0}, overflows{0};
    uint32_t consumedPanic{}; // consumer only
    uint64_t lastFrame{}; // producer only
};
static_assert(std::atomic<uint32_t>::is_always_lock_free);
}
