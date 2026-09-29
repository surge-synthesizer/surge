// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/surge-web/MidiQueue.h"
#include <cassert>
#include <limits>
#include <thread>
#include <vector>
using Surge::Web::MidiQueue;
int main()
{
    MidiQueue queue;
    assert(!queue.push(0x70, 60, 100, 0));
    assert(!queue.push(0xf0, 60, 100, 0));
    assert(!queue.push(0x90, 128, 100, 0));
    assert(!queue.push(0x90, 60, 100, std::numeric_limits<double>::infinity()));
    assert(queue.push(0x91, 60, 100, 132));
    assert(queue.push(0xe1, 0, 64, 140));
    assert(queue.push(0xd1, 90, 0, 135)); // ordered after bend, even if delivered late
    std::vector<int> offsets, types, lengths;
    auto collect = [&](const auto &event, int offset) {
        offsets.push_back(offset); types.push_back(event.bytes[0]); lengths.push_back(event.length);
    };
    assert(!queue.drain(0, 128, collect));
    assert(offsets.empty());
    assert(!queue.drain(128, 128, collect));
    assert((offsets == std::vector<int>{4, 12, 12}));
    assert((types == std::vector<int>{0x91, 0xe1, 0xd1}));
    assert((lengths == std::vector<int>{3, 3, 2}));
    assert(queue.pending() == 0);
    for (uint32_t i = 0; i < MidiQueue::capacity; ++i) assert(queue.push(0x90, 60, 1, i));
    assert(!queue.push(0x80, 60, 0, 2000));
    assert(queue.overflowCount() == 1);
    assert(queue.drain(0, 128, collect)); // panic clears even future notes
    assert(queue.pending() == 0);
    assert(queue.push(0x90, 61, 127, 0));
    assert(!queue.drain(4096, 128, collect)); // late events delivered at offset zero
    assert(offsets.back() == 0);
    queue.panic();
    assert(queue.drain(0, 128, collect));

    MidiQueue concurrent;
    constexpr uint32_t count = 100000;
    std::thread producer([&] {
        for (uint32_t i = 0; i < count; ++i)
        {
            while (concurrent.pending() >= MidiQueue::capacity - 1) std::this_thread::yield();
            assert(concurrent.push(0x90, i % 128, 100, i));
        }
    });
    uint32_t received = 0;
    while (received != count)
        assert(!concurrent.drain(0, count + 1, [&](const auto &event, int offset) {
            assert(event.frame == received);
            assert(event.bytes[1] == received % 128);
            assert(offset == int(received));
            ++received;
        }));
    producer.join();
    assert(concurrent.overflowCount() == 0);
}
