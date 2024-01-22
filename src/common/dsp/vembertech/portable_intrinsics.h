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

#define vFloat __m128

#define vZero _mm_setzero_ps()

#define vAdd _mm_add_ps
#define vSub _mm_sub_ps
#define vMul _mm_mul_ps
#define vMAdd(a, b, c) _mm_add_ps(_mm_mul_ps(a, b), c)
#define vNMSub(a, b, c) _mm_sub_ps(c, _mm_mul_ps(a, b))
#define vNeg(a) vSub(vZero, a)

#define vAnd _mm_and_ps
#define vOr _mm_or_ps

#define vCmpGE _mm_cmpge_ps

#define vMax _mm_max_ps
#define vMin _mm_min_ps

#define vLoad _mm_load_ps

inline vFloat vLoad1(float f) { return _mm_load1_ps(&f); }

inline vFloat vSqrtFast(vFloat v) { return _mm_rcp_ps(_mm_rsqrt_ps(v)); }

inline float vSum(vFloat x)
{
    __m128 a = _mm_add_ps(x, _mm_movehl_ps(x, x));
    a = _mm_add_ss(a, _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 1)));
    float f;
    _mm_store_ss(&f, a);

    return f;
}

#endif // SURGE_SRC_COMMON_DSP_VEMBERTECH_PORTABLE_INTRINSICS_H
