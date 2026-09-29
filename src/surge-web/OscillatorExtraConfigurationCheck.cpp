// SPDX-License-Identifier: GPL-3.0-or-later
#include "dsp/OscillatorExtraConfiguration.h"
#include <atomic>
#include <iostream>
#include <thread>

static OscillatorExtraConfiguration value(int seed)
{
    OscillatorExtraConfiguration result;
    result.nData = 16;
    for (int i = 0; i < 64; ++i) result.data[i] = seed + i;
    return result;
}
static bool coherent(const OscillatorExtraConfiguration &v)
{
    if (v.nData != 16) return false;
    for (int i = 0; i < 64; ++i) if (v.data[i] != v.data[0] + i) return false;
    return true;
}
int main()
{
    BrowserOscillatorExtraConfiguration config;
    config = value(1);
    if (!coherent(config.audioValue())) return 1;
    std::atomic<bool> held{false}, checked{false};
    std::thread writer([&] {
        auto edit = config.edit();
        edit->data[0] = 100; // Deliberately incomplete, unpublished transaction.
        held.store(true, std::memory_order_release);
        while (!checked.load(std::memory_order_acquire)) std::this_thread::yield();
        *edit.operator->() = value(100);
    });
    while (!held.load(std::memory_order_acquire)) std::this_thread::yield();
    const bool wouldWait = !config.tryLock().owns_lock();
    const auto old = config.audioValue();
    checked.store(true, std::memory_order_release);
    writer.join();
    if (!wouldWait || !coherent(old) || old.data[0] != 1) return 2;
    if (config.audioValue().data[0] != 100) return 3;

    std::atomic<bool> done{false};
    std::atomic<bool> valid{true};
    std::thread ui([&] {
        for (int i = 1; i <= 100000; ++i)
        {
            auto edit = config.edit();
            *edit.operator->() = value(i);
            // Undo captures under the same recursive writer ownership.
            if (!coherent(config.read())) valid.store(false);
        }
        done.store(true, std::memory_order_release);
    });
    unsigned resets = 0;
    do
    {
        // Model the callback's queued type reset: skip if UI owns the model.
        auto guard = config.tryLock();
        if (guard.owns_lock())
        {
            config = value(-1000);
            ++resets;
        }
        if (!coherent(config.audioValue())) valid.store(false);
    } while (!done.load(std::memory_order_acquire));
    ui.join();
    config = value(200000);
    if (!valid.load() || config.audioValue().data[0] != 200000) return 4;
    std::cout << "Coherent UI publications and nonblocking type resets: " << resets << '\n';
}
