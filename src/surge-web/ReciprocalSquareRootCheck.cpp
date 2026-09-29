// SPDX-License-Identifier: GPL-3.0-or-later
// Native ARM64 check for the Wasm Reciprocal square root approximation.
#include "dsp/vembertech/ReciprocalSquareRootEstimate.h"
#include <simde/x86/sse.h>
#include <cstdio>
#include <limits>
#include <initializer_list>
#if !defined(SIMDE_ARM_NEON_A32V7_NATIVE)
#error "This reference check requires native ARM NEON"
#endif

static float fromBits(uint32_t bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
static uint32_t toBits(float value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
static float reference(float value)
{
    return simde_mm_cvtss_f32(simde_mm_rsqrt_ps(simde_mm_set1_ps(value)));
}
int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--print-table") == 0)
    {
        for (int i = 0; i < 512; ++i)
            std::printf("0x%08xu,%s", toBits(reference(fromBits(
                ((i < 256 ? 127u : 128u) << 23) | ((i % 256) << 15)))),
                i % 8 == 7 ? "\n" : " ");
        return 0;
    }
    unsigned checked = 0;
    auto check = [&](uint32_t bits) {
        const float value = fromBits(bits);
        const auto expected = toBits(reference(value));
        const auto actual = toBits(Surge::DSP::reciprocalSquareRootEstimate(value));
        ++checked;
        if (actual == expected) return true;
        std::fprintf(stderr, "Input %08x: expected %08x, got %08x\n", bits, expected, actual);
        return false;
    };
    // Every normal exponent, at both ends of each mantissa quantization bucket.
    for (uint32_t exponent = 1; exponent < 255; ++exponent)
        for (uint32_t bucket = 0; bucket < 256; ++bucket)
            for (uint32_t tail : {0u, 0x7fffu})
                if (!check((exponent << 23) | (bucket << 15) | tail)) return 1;
    // Include all positive subnormals: normalization must preserve their estimate.
    for (uint32_t bits = 0; bits < 0x800000; ++bits)
        if (!check(bits)) return 1;
    uint32_t random = 12345;
    for (int i = 0; i < 2000000; ++i)
    {
        random = random * 1664525u + 1013904223u;
        const auto bits = random & 0x7fffffff;
        if ((bits >> 23) != 255 && !check(bits)) return 1;
    }
    if (Surge::DSP::reciprocalSquareRootEstimate(std::numeric_limits<float>::infinity()) != 0.f ||
        !std::isnan(Surge::DSP::reciprocalSquareRootEstimate(-1.f)) ||
        !std::isnan(Surge::DSP::reciprocalSquareRootEstimate(std::numeric_limits<float>::quiet_NaN())))
        return 1;
    std::printf("Reciprocal square root matches native NEON for %u values\n", checked);
}
