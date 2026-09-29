// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>

namespace Surge::Web
{
// UI commands are atomic; musical position belongs exclusively to the audio
// thread. Position advances by rendered samples, never by an animation timer.
class Transport
{
  public:
    struct State { bool playing; int numerator, denominator; double ppq; };
    bool configure(bool playing, int numerator, int denominator)
    {
        if (numerator < 1 || numerator > 32 || denominator < 1 || denominator > 32 ||
            (denominator & (denominator - 1))) return false;
        request.store((playing ? 1u : 0u) | (uint32_t(numerator) << 1) |
                      (uint32_t(denominator) << 7), std::memory_order_release);
        return true;
    }
    void rewind() { reset.fetch_add(1, std::memory_order_release); }
    State requested() const
    {
        const auto value = request.load(std::memory_order_acquire);
        return {bool(value & 1), int((value >> 1) & 63), int((value >> 7) & 63), 0};
    }
    State beginBlock()
    {
        const auto generation = reset.load(std::memory_order_acquire);
        if (generation != consumedReset) { ppq = 0.; consumedReset = generation; }
        current = requested(); current.ppq = ppq;
        return current;
    }
    // Audio thread, or exclusive offline ownership only.
    double position() const { return ppq; }
    void advance(unsigned samples, double sampleRate, double bpm)
    {
        if (current.playing && sampleRate > 0 && bpm > 0 && std::isfinite(bpm))
            ppq += samples / sampleRate * bpm / 60.;
    }
  private:
    std::atomic<uint32_t> request{(4u << 1) | (4u << 7)}, reset{0};
    uint32_t consumedReset{};
    double ppq{};
    State current{};
};
}
