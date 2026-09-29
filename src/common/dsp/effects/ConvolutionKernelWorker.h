// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ConvolutionKernel.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

// A bounded single-producer/worker mailbox. The producer must serialize submit
// and take calls (the browser audio/patch-loader ownership handoff supplies this).
// Construct and destroy off the audio thread; stop the producer before destruction.
// Sample buffers must remain immutable while shared with this service.
class ConvolutionKernelWorker
{
  public:
    static constexpr std::size_t capacity = 32;
    using Samples = std::shared_ptr<const std::vector<std::uint8_t>>;
    struct Request
    {
        std::uint64_t generation{};
        Samples left, right;
        float inputRate{}, outputRate{}, start{}, reverse{};
        bool reverseDeactivated{true};
    };
    enum class Result { pending, applied, stale, failed };

    ConvolutionKernelWorker();
    ~ConvolutionKernelWorker();
    ConvolutionKernelWorker(const ConvolutionKernelWorker &) = delete;
    ConvolutionKernelWorker &operator=(const ConvolutionKernelWorker &) = delete;

    // No allocation, destruction, locks, thread creation or notifications here.
    // A full slot rejects the request without consuming its sample references.
    bool submit(std::size_t slot, const Request &request);
    // Only the matching generation can replace active. The old kernel and all
    // request references are reclaimed by the worker, including failed/stale jobs.
    Result take(std::size_t slot, std::uint64_t generation,
                std::unique_ptr<ConvolutionKernel> &active);
    Result takeMatching(std::size_t slot, const Request &expected,
                        std::unique_ptr<ConvolutionKernel> &active);
    bool retire(std::size_t slot, std::unique_ptr<ConvolutionKernel> &active);
    // Nonblocking invalidation when an effect is reset or removed. The worker
    // reclaims queued/ready work even if no replacement effect ever polls it.
    void cancel(std::size_t slot);
    std::uint32_t applied() const { return applied_.load(std::memory_order_relaxed); }
    std::uint32_t appliedForSlot(std::size_t slot) const
    {
        return slot < capacity ? slots_[slot].applied.load(std::memory_order_relaxed) : 0;
    }

  private:
    enum class State { idle, queued, ready, consuming, retiring };
    static_assert(std::atomic<State>::is_always_lock_free);
    struct Slot
    {
        std::atomic<State> state{State::idle};
        std::atomic<bool> cancelled{false};
        std::atomic<std::uint32_t> applied{0};
        Request request;
        std::unique_ptr<ConvolutionKernel> kernel;
    };
    std::array<Slot, capacity> slots_;
    std::atomic<bool> stopping_{false};
    std::atomic<std::uint32_t> applied_{0};
    std::thread worker_;
    void run();
    Result takeImpl(std::size_t slot, std::uint64_t generation,
                    const Request *expected, std::unique_ptr<ConvolutionKernel> &active);
};
