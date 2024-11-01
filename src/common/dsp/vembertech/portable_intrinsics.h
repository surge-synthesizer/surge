/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */
#ifndef SURGE_SRC_COMMON_DSP_VEMBERTECH_PORTABLE_INTRINSICS_H
#define SURGE_SRC_COMMON_DSP_VEMBERTECH_PORTABLE_INTRINSICS_H

#define vFloat SIMD_M128

#define vZero SIMD_MM(setzero_ps)()

#define vAdd SIMD_MM(add_ps)
#define vSub SIMD_MM(sub_ps)
#define vMul SIMD_MM(mul_ps)
#define vMAdd(a, b, c) SIMD_MM(add_ps)(SIMD_MM(mul_ps)(a, b), c)
#define vNMSub(a, b, c) SIMD_MM(sub_ps)(c, SIMD_MM(mul_ps)(a, b))
#define vNeg(a) vSub(vZero, a)

#define vAnd SIMD_MM(and_ps)
#define vOr SIMD_MM(or_ps)

#define vCmpGE SIMD_MM(cmpge_ps)

#define vMax SIMD_MM(max_ps)
#define vMin SIMD_MM(min_ps)

#define vLoad SIMD_MM(load_ps)

inline vFloat vLoad1(float f) { return SIMD_MM(load1_ps)(&f); }

inline vFloat vSqrtFast(vFloat v) { return SIMD_MM(rcp_ps)(SIMD_MM(rsqrt_ps)(v)); }

inline float vSum(vFloat x)
{
    auto a = SIMD_MM(add_ps)(x, SIMD_MM(movehl_ps)(x, x));
    a = SIMD_MM(add_ss)(a, SIMD_MM(shuffle_ps)(a, a, SIMD_MM_SHUFFLE(0, 0, 0, 1)));
    float f;
    SIMD_MM(store_ss)(&f, a);

    return f;
}

#endif // SURGE_SRC_COMMON_DSP_VEMBERTECH_PORTABLE_INTRINSICS_H
