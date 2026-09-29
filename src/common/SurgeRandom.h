/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SURGE_RANDOM_H
#define SURGE_RANDOM_H

#include <cstdint>
#include <random>
#if SURGE_WEB
#include <emscripten.h>
#endif

namespace Surge
{
// Seeds for stochastic audio generators, never for cryptography. std::random_device
// may open /dev/urandom, which proxies filesystem work to the browser main thread
// and is unavailable in an AudioWorklet. Keep the original generators and
// distributions; only obtain their initial seed through a worklet-safe primitive.
#if SURGE_WEB
struct AudioRandomDevice
{
    uint32_t operator()() const
    {
        return static_cast<uint32_t>(emscripten_random() * 4294967296.0);
    }
};
#else
using AudioRandomDevice = std::random_device;
#endif
} // namespace Surge
#endif
