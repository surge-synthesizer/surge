// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/surge-web/Transport.h"
#include <cassert>
#include <algorithm>
#include <initializer_list>
#include <thread>
using Surge::Web::Transport;
int main()
{
    for (const double rate : {44100., 48000.})
    {
        Transport transport;
        assert(!transport.beginBlock().playing);
        transport.advance(128, rate, 120);
        assert(transport.beginBlock().ppq == 0);
        assert(!transport.configure(true, 0, 4));
        assert(!transport.configure(true, 4, 3));
        assert(transport.configure(true, 7, 8));
        const auto samples = unsigned(rate * 30);
        for (unsigned i = 0; i < samples;)
        {
            const auto count = std::min(128u, samples - i);
            const auto state = transport.beginBlock();
            assert(state.playing && state.numerator == 7 && state.denominator == 8);
            transport.advance(count, rate, 123.45);
            i += count;
        }
        const auto position = transport.beginBlock().ppq;
        assert(std::abs(position - 61.725) < 1.e-8);
        assert(transport.configure(false, 7, 8));
        transport.beginBlock(); transport.advance(4096, rate, 220);
        assert(transport.beginBlock().ppq == position);
        transport.rewind(); assert(transport.beginBlock().ppq == 0);
        assert(transport.configure(true, 4, 4));
        transport.beginBlock(); transport.advance(unsigned(rate), rate, 60);
        assert(transport.beginBlock().ppq == 1);
        transport.advance(unsigned(rate), rate, 120);
        assert(transport.beginBlock().ppq == 3);
    }
    Transport concurrent;
    std::atomic<bool> done{false};
    std::thread ui([&] {
        for (int i=0; i<100000; ++i)
        {
            assert(concurrent.configure(true, 7, 8));
            assert(concurrent.configure(false, 3, 4));
        }
        done.store(true);
    });
    do {
        const auto state = concurrent.beginBlock();
        assert((!state.playing && state.numerator == 4 && state.denominator == 4) ||
               (state.playing && state.numerator == 7 && state.denominator == 8) ||
               (!state.playing && state.numerator == 3 && state.denominator == 4));
    } while (!done.load());
    ui.join();
}
