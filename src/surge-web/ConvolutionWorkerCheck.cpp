// SPDX-License-Identifier: GPL-3.0-or-later
#include "dsp/effects/ConvolutionKernelWorker.h"
#include "dsp/effects/ConvolutionReloadEdits.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

using Worker = ConvolutionKernelWorker;
using Clock = std::chrono::steady_clock;
static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
template <class Predicate> static void until(Predicate predicate)
{
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (!predicate())
    {
        require(Clock::now() < deadline, "Worker timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static Worker::Samples bytes(const std::vector<float> &source)
{
    auto result = std::make_shared<std::vector<std::uint8_t>>(source.size() * sizeof(float));
    std::memcpy(result->data(), source.data(), result->size());
    return result;
}
static Worker::Result take(Worker &worker, std::size_t slot, std::uint64_t generation,
                           std::unique_ptr<ConvolutionKernel> &active)
{
    auto result = Worker::Result::pending;
    until([&] { result = worker.take(slot, generation, active); return result != Worker::Result::pending; });
    return result;
}
int main()
{
    try
    {
        {
            FxStorage live{fxslot_ains1}, candidate{fxslot_ains1};
            live.type.val.i = candidate.type.val.i = 1;
            live.p[0].valtype = candidate.p[0].valtype = vt_float;
            live.p[0].val.f = 1.f;
            candidate.p[0].val.f = .5f;
            candidate.p[0].absolute = true;
            candidate.user_data["left"] = std::make_shared<std::vector<std::uint8_t>>(16);
            ConvolutionReloadEdits edits;
            const auto references = candidate.user_data["left"].use_count();
            edits.capture(live, candidate);
            require(candidate.user_data["left"].use_count() == references,
                    "Edit baseline retained sample buffers");
            require(edits.merge(live, candidate) && candidate.p[0].val.f == .5f,
                    "Unchanged live state overwrote candidate values");
            live.p[0].val.f = 2.f;
            live.p[0].deactivated = true;
            live.p[0].midictrl = 74;
            require(edits.merge(live, candidate) && candidate.p[0].val.f == 2.f &&
                    candidate.p[0].deactivated && candidate.p[0].absolute && candidate.p[0].midictrl == 74,
                    "Pending reload lost newer values or flags");
            live.p[0].val.f = 1.f;
            live.p[0].deactivated = false;
            require(edits.merge(live, candidate) && candidate.p[0].val.f == 1.f &&
                    !candidate.p[0].deactivated, "Returning an edit to its original value was lost");
            candidate.user_data["left"] = std::make_shared<std::vector<std::uint8_t>>(16);
            live.p[0].val.f = 3.f;
            require(!edits.merge(live, candidate) && candidate.p[0].val.f == 1.f,
                    "Old transaction changed a newer IR selection");
            edits.capture(live, candidate);
            edits.finish();
            live.p[0].val.f = 4.f;
            require(!edits.merge(live, candidate) && candidate.p[0].val.f == 1.f,
                    "Finished transaction changed an explicit replacement");
        }
        std::vector<float> left(4096), right(4096);
        left[0] = .8f; left[63] = .2f; left[511] = -.1f;
        right[0] = .4f; right[127] = -.25f; right[1500] = .1f;
        Worker worker;
        Worker::Request request{1, bytes(left), bytes(right), 32000, 48000, .1f, .2f, false};
        std::unique_ptr<ConvolutionKernel> active;
        require(worker.submit(0, request), "Initial request rejected");
        require(!worker.submit(0, request), "Busy slot accepted another request");
        require(!worker.submit(Worker::capacity, request), "Out-of-bounds slot accepted");
        require(take(worker, 0, 1, active) == Worker::Result::applied, "Initial kernel failed");
        auto direct = ConvolutionKernel::prepare(left, right, 32000, 48000, .1f, .2f, false);
        require(direct && active->size == direct->size, "Prepared size differs");
        // Compare the actual FFT output across multiple head and tail partitions.
        std::array<float, 32> input{}, actual{}, expected{};
        for (int block = 0; block < 300; ++block)
        {
            for (int i = 0; i < 32; ++i) input[i] = std::sin(float(block * 32 + i) * .03f);
            active->left.process(input, actual); direct->left.process(input, expected);
            require(actual == expected, "Worker left audio differs from synchronous preparation");
            active->right.process(input, actual); direct->right.process(input, expected);
            require(actual == expected, "Worker right audio differs from synchronous preparation");
        }
        auto *retained = active.get();
        request.generation = 2;
        until([&] { return worker.submit(0, request); });
        require(take(worker, 0, 3, active) == Worker::Result::stale && active.get() == retained,
                "Stale completion replaced the current kernel");
        request.generation = 4;
        request.right = std::make_shared<std::vector<std::uint8_t>>(3);
        until([&] { return worker.submit(0, request); });
        require(take(worker, 0, 4, active) == Worker::Result::failed && active.get() == retained,
                "Malformed stereo input replaced the current kernel");
        request.right.reset();
        request.left = bytes({std::numeric_limits<float>::quiet_NaN()});
        until([&] { return worker.submit(0, request); });
        require(take(worker, 0, 4, active) == Worker::Result::failed && active.get() == retained,
                "Non-finite input replaced the current kernel");

        request.left = bytes(left); request.right = bytes(right);
        until([&] { return worker.submit(0, request); });
        auto newer = request;
        newer.left = bytes(left); // Same samples, different replacement ownership.
        auto matching = Worker::Result::pending;
        until([&] {
            matching = worker.takeMatching(0, newer, active);
            return matching != Worker::Result::pending;
        });
        require(matching == Worker::Result::stale && active.get() == retained,
                "Superseded IR ownership replaced the current kernel");
        until([&] { return worker.submit(0, request); });
        newer = request;
        newer.outputRate *= 2;
        until([&] {
            matching = worker.takeMatching(0, newer, active);
            return matching != Worker::Result::pending;
        });
        require(matching == Worker::Result::stale && active.get() == retained,
                "An obsolete kernel rate replaced the current kernel");
        request.outputRate = std::numeric_limits<float>::quiet_NaN();
        until([&] { return worker.submit(0, request); });
        until([&] {
            matching = worker.takeMatching(0, request, active);
            return matching != Worker::Result::pending;
        });
        require(matching == Worker::Result::failed && active.get() == retained,
                "Invalid rate was not rejected while retaining the current kernel");
        request.outputRate = 48000;

        // Fill every mailbox, consume it, and retry only after worker retirement.
        request.left = bytes(left); request.right = bytes(right);
        for (int cycle = 0; cycle < 8; ++cycle)
        {
            ++request.generation;
            for (std::size_t slot = 0; slot < Worker::capacity; ++slot)
                until([&] { return worker.submit(slot, request); });
            for (std::size_t slot = 0; slot < Worker::capacity; ++slot)
                require(take(worker, slot, request.generation, active) == Worker::Result::applied,
                        "Mailbox reuse failed");
        }
        until([&] { return worker.retire(0, active); });
        require(!active, "Standalone retirement did not transfer ownership");
        // Observe final sample ownership release, both on normal retirement and
        // when shutdown encounters queued or unconsumed work.
        std::atomic<int> released{0};
        std::atomic<bool> wrongThread{false};
        const auto producer = std::this_thread::get_id();
        const auto tracked = [&] {
            return Worker::Samples(new std::vector<std::uint8_t>(4096 * sizeof(float)),
                [&](const auto *data) {
                    if (std::this_thread::get_id() == producer) wrongThread.store(true);
                    delete data;
                    released.fetch_add(1, std::memory_order_release);
                });
        };
        {
            Worker retirement;
            request.left = tracked(); request.right.reset();
            require(retirement.submit(0, request), "Retirement submission failed");
            request.left.reset();
            require(take(retirement, 0, request.generation, active) == Worker::Result::applied,
                    "Retirement preparation failed");
            until([&] { return released.load(std::memory_order_acquire) == 1; });
            request.left = tracked();
            require(retirement.submit(1, request), "Cancellation submission failed");
            request.left.reset();
            retirement.cancel(1);
            // No effect remains to take this result; cancellation must reclaim it.
            until([&] { return released.load(std::memory_order_acquire) == 2; });
            require(retirement.applied() == 1, "Cancelled result was published");
            request.left = tracked();
            // The deleter runs just before idle is published, so wait for that
            // publication rather than racing it after observing released.
            until([&] { return retirement.submit(1, request); });
            request.left.reset();
        }
        require(released.load() == 3 && !wrongThread.load(), "Samples retired on producer thread");
        std::puts("Convolution worker: exact audio, bounded reuse, stale/failure retention and worker retirement passed");
        std::puts("Pending IR edits: newer values, flags, MIDI mappings and selection ownership passed");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
