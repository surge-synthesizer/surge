// SPDX-License-Identifier: GPL-3.0-or-later
#include "dsp/effects/EffectRetirementWorker.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>

static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
template <class Predicate> static void until(Predicate predicate)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!predicate())
    {
        require(std::chrono::steady_clock::now() < deadline, "Retirement worker timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
struct ObservedEffect : Effect
{
    ObservedEffect(std::thread::id producer, std::atomic<unsigned> &count,
                   std::atomic<bool> &wrongThread, std::atomic<bool> &release)
        : Effect(nullptr, nullptr, nullptr), producer(producer), count(count),
          wrongThread(wrongThread), release(release) {}
    ~ObservedEffect() override
    {
        if (std::this_thread::get_id() == producer) wrongThread.store(true);
        while (!release.load()) std::this_thread::yield();
        count.fetch_add(1);
    }
    std::thread::id producer;
    std::atomic<unsigned> &count;
    std::atomic<bool> &wrongThread, &release;
};
int main()
{
    std::atomic<unsigned> effects{0}, samples{0};
    std::atomic<bool> wrongThread{false}, release{false};
    const auto producer = std::this_thread::get_id();
    try
    {
        auto worker = std::make_unique<EffectRetirementWorker>();
        const auto make = [&]() -> std::unique_ptr<Effect> {
            return std::make_unique<ObservedEffect>(producer, effects, wrongThread, release);
        };
        const auto data = [&] {
            EffectRetirementWorker::UserData result;
            result["left"] = std::shared_ptr<std::vector<uint8_t>>(
                new std::vector<uint8_t>(4096), [&](auto *p) {
                    if (std::this_thread::get_id() == producer) wrongThread.store(true);
                    delete p; samples.fetch_add(1);
                });
            return result;
        };
        // Block the worker in the first destructor so every slot can be filled
        // deterministically. A full queue must not consume the rejected object.
        bool filled = true;
        for (std::size_t slot = 0; slot < EffectRetirementWorker::capacity; ++slot)
        {
            auto effect = make();auto metadata = data();
            filled &= worker->retire(slot, effect, metadata) && !effect && metadata.empty();
        }
        auto extra = make(); auto metadata = data();
        auto *identity = extra.get();const auto buffer = metadata.at("left").get();
        const bool rejected = !worker->retire(0, extra, metadata) && extra.get() == identity &&
                              metadata.at("left").get() == buffer;
        // Always release before asserting, so a failed test cannot strand join().
        release.store(true);
        require(filled && rejected, "Queue did not preserve bounded ownership");
        until([&] { return worker->available(0); });
        require(worker->retire(0, extra, metadata), "Retirement lane could not be reused");
        worker.reset(); // Shutdown must drain all remaining items on the worker.
        require(effects == EffectRetirementWorker::capacity + 1 &&
                    samples == EffectRetirementWorker::capacity + 1,
                "Retirement lost an effect or sample buffer");
        require(!wrongThread, "Effect or sample destroyed on producer thread");
        std::puts("Effect retirement: bounded ownership, reuse, worker destruction and shutdown passed");
        return 0;
    }
    catch (const std::exception &e)
    {
        release.store(true);
        std::fprintf(stderr, "%s\n", e.what());return 1;
    }
}
