// SPDX-License-Identifier: GPL-3.0-or-later
// Render identical fixed-coefficient Tri-pole kernels for native/Wasm comparison.
// Hex output preserves every float bit through Emscripten's text stdout bridge.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "sst/filters/TriPoleFilter.h"

using namespace sst::filters;

template <int Subtype> bool render()
{
    QuadFilterUnitState state{};
    // Four independent SIMD lanes exercise different coefficients and inputs.
    alignas(16) float coefficients[7][4];
    const float poles[]{0.002f, 0.02f, 0.2f, 0.6f};
    for (int lane = 0; lane < 4; ++lane)
    {
        coefficients[0][lane] = poles[lane] * 0.998f;
        coefficients[1][lane] = 1.f / (1.f + coefficients[0][lane]);
        coefficients[2][lane] = poles[lane] * 1.0012f;
        coefficients[3][lane] = 1.f / (1.f + coefficients[2][lane]);
        coefficients[4][lane] = poles[lane];
        coefficients[5][lane] = 1.f / (1.f + coefficients[4][lane]);
        coefficients[6][lane] = -2.f - lane * 4.f;
    }
    for (int i = 0; i < 7; ++i)
        state.C[i] = SIMD_MM(load_ps)(coefficients[i]);
    uint32_t seed = 12345;
    for (int frame = 0; frame < 20000; ++frame)
    {
        alignas(16) float input[4], output[4];
        for (auto &sample : input)
        {
            seed = seed * 1664525u + 1013904223u;
            sample = (int32_t(seed >> 8) - 8388608) * 0x1p-26f;
        }
        const auto result = TriPoleFilter::process<static_cast<FilterSubType>(Subtype)>(
            &state, SIMD_MM(load_ps)(input));
        SIMD_MM(store_ps)(output, result);
        for (int lane = 0; lane < 4; ++lane)
        {
            if (!std::isfinite(output[lane]))
            {
                std::fprintf(stderr, "Non-finite subtype %d, frame %d, lane %d\n",
                             Subtype, frame, lane);
                return false;
            }
            uint32_t bits;
            std::memcpy(&bits, &output[lane], sizeof(bits));
            std::printf("%08x%c", bits, lane == 3 ? '\n' : ' ');
        }
    }
    return true;
}

int main()
{
    return render<0>() && render<1>() && render<2>() && render<3>() && render<4>() &&
                   render<5>() && render<6>() && render<7>() && render<8>() && render<9>() &&
                   render<10>() && render<11>()
               ? 0
               : 1;
}
