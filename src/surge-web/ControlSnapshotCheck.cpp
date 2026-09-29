// SPDX-License-Identifier: GPL-3.0-or-later
#include "dsp/ControlSnapshot.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>

using State = std::array<uint32_t, 64>;
static State state(uint32_t generation)
{
    State result{};
    for (size_t i = 0; i < result.size(); ++i) result[i] = generation + i * 17;
    return result;
}
int main()
{
    ControlSnapshot<State> snapshot;
    State received = state(99);
    if (snapshot.consume(received) || received != state(99)) return 1;
    snapshot.publish(state(1));
    snapshot.publish(state(2));
    if (!snapshot.consume(received) || received != state(2)) return 2;
    if (snapshot.consume(received) || received != state(2)) return 3;

    constexpr uint32_t last = 1000000;
    std::atomic<bool> done{false};
    std::thread writer([&] {
        for (uint32_t i = 3; i <= last; ++i) snapshot.publish(state(i));
        done.store(true, std::memory_order_release);
    });
    uint32_t previous = 2, observations = 0;
    bool valid = true;
    do
    {
        if (snapshot.consume(received))
        {
            valid &= received[0] > previous && received == state(received[0]);
            previous = received[0];
            ++observations;
        }
    } while (!done.load(std::memory_order_acquire));
    writer.join();
    if (snapshot.consume(received))
    {
        valid &= received[0] > previous && received == state(received[0]);
        previous = received[0];
    }
    if (!valid || previous != last || !observations) return 4;
    // Verify that the retired consumer slot can safely rejoin the producer pool.
    snapshot.publish(state(last + 1));
    if (!snapshot.consume(received) || received != state(last + 1)) return 5;
    std::cout << "Coherent snapshot handoffs: " << observations << "; final generation " << previous
              << '\n';
}
