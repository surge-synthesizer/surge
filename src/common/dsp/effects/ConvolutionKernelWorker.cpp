// SPDX-License-Identifier: GPL-3.0-or-later
#include "ConvolutionKernelWorker.h"
#include <chrono>
#include <cmath>

namespace
{
std::span<const float> samples(const ConvolutionKernelWorker::Samples &data)
{
    if (!data || data->size() % sizeof(float)) return {};
    return {reinterpret_cast<const float *>(data->data()), data->size() / sizeof(float)};
}
}

ConvolutionKernelWorker::ConvolutionKernelWorker() : worker_([this] { run(); }) {}

ConvolutionKernelWorker::~ConvolutionKernelWorker()
{
    stopping_.store(true, std::memory_order_release);
    worker_.join();
}

bool ConvolutionKernelWorker::submit(std::size_t index, const Request &request)
{
    if (index >= capacity) return false;
    auto &slot = slots_[index];
    if (slot.state.load(std::memory_order_acquire) != State::idle) return false;
    // The worker emptied this slot before publishing idle; these assignments
    // cannot release a previous sample buffer on the producer thread.
    slot.request = request;
    slot.cancelled.store(false, std::memory_order_relaxed);
    slot.state.store(State::queued, std::memory_order_release);
    return true;
}

ConvolutionKernelWorker::Result ConvolutionKernelWorker::take(
    std::size_t index, std::uint64_t generation, std::unique_ptr<ConvolutionKernel> &active)
{
    return takeImpl(index, generation, nullptr, active);
}

ConvolutionKernelWorker::Result ConvolutionKernelWorker::takeMatching(
    std::size_t index, const Request &expected, std::unique_ptr<ConvolutionKernel> &active)
{
    return takeImpl(index, expected.generation, &expected, active);
}

ConvolutionKernelWorker::Result ConvolutionKernelWorker::takeImpl(
    std::size_t index, std::uint64_t generation, const Request *wanted,
    std::unique_ptr<ConvolutionKernel> &active)
{
    if (index >= capacity) return Result::pending;
    auto &slot = slots_[index];
    auto expected = State::ready;
    if (!slot.state.compare_exchange_strong(expected, State::consuming,
                                           std::memory_order_acquire)) return Result::pending;
    auto result = Result::stale;
    const auto &request = slot.request;
    const auto same = [](float a, float b) { return a == b || (std::isnan(a) && std::isnan(b)); };
    const bool matches = !wanted ||
        (wanted->left == request.left && wanted->right == request.right &&
         same(wanted->inputRate, request.inputRate) && same(wanted->outputRate, request.outputRate) &&
         same(wanted->start, request.start) && same(wanted->reverse, request.reverse) &&
         wanted->reverseDeactivated == request.reverseDeactivated);
    if (!slot.cancelled.load(std::memory_order_relaxed) && request.generation == generation && matches)
    {
        result = slot.kernel ? Result::applied : Result::failed;
        if (slot.kernel)
        {
            active.swap(slot.kernel);
            slot.applied.fetch_add(1, std::memory_order_relaxed);
            applied_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    slot.state.store(State::retiring, std::memory_order_release);
    return result;
}

bool ConvolutionKernelWorker::retire(std::size_t index, std::unique_ptr<ConvolutionKernel> &active)
{
    if (index >= capacity) return false;
    auto &slot = slots_[index];
    if (slot.state.load(std::memory_order_acquire) != State::idle) return false;
    active.swap(slot.kernel);
    slot.state.store(State::retiring, std::memory_order_release);
    return true;
}

void ConvolutionKernelWorker::cancel(std::size_t index)
{
    if (index < capacity) slots_[index].cancelled.store(true, std::memory_order_release);
}

void ConvolutionKernelWorker::run()
{
    while (!stopping_.load(std::memory_order_acquire))
    {
        for (auto &slot : slots_)
        {
            const auto state = slot.state.load(std::memory_order_acquire);
            if (state == State::queued)
            {
                const auto &request = slot.request;
                const auto left = samples(request.left), right = samples(request.right);
                // Reject malformed bytes rather than treating a malformed stereo
                // channel as a request to duplicate the left channel.
                if (!left.empty() && left.size() <= (1u << 22) &&
                    (!request.right || right.size() == left.size()))
                {
                    try
                    {
                        slot.kernel = ConvolutionKernel::prepare(
                            left, right, request.inputRate, request.outputRate,
                            request.start, request.reverse, request.reverseDeactivated);
                    }
                    catch (...)
                    {
                        // No exception may terminate the worker or reach audio.
                        // The consumer receives failed and retains its kernel.
                    }
                }
                slot.state.store(State::ready, std::memory_order_release);
            }
            else if (state == State::ready && slot.cancelled.load(std::memory_order_acquire))
            {
                auto expected = State::ready;
                slot.state.compare_exchange_strong(expected, State::retiring,
                                                   std::memory_order_acq_rel);
            }
            else if (state == State::retiring)
            {
                slot.kernel.reset();
                slot.request = {};
                slot.state.store(State::idle, std::memory_order_release);
            }
        }
        // Polling keeps notification syscalls off the producer's audio path.
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    // Also retire queued or unconsumed work on this thread at shutdown.
    for (auto &slot : slots_)
    {
        slot.kernel.reset();
        slot.request = {};
    }
}
