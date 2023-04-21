/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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
#ifndef SURGE_SRC_COMMON_DSP_VEMBERTECH_SHARED_H
#define SURGE_SRC_COMMON_DSP_VEMBERTECH_SHARED_H

#include "globals.h"

inline float i2f_binary_cast(int i)
{
    float *f = (float *)&i;
    return *f;
};

const __m128 m128_mask_signbit = _mm_set1_ps(i2f_binary_cast(0x80000000));
const __m128 m128_mask_absval = _mm_set1_ps(i2f_binary_cast(0x7fffffff));
const __m128 m128_zero = _mm_set1_ps(0.0f);
const __m128 m128_half = _mm_set1_ps(0.5f);
const __m128 m128_one = _mm_set1_ps(1.0f);
const __m128 m128_two = _mm_set1_ps(2.0f);
const __m128 m128_four = _mm_set1_ps(4.0f);
const __m128 m128_1234 = _mm_set_ps(1.f, 2.f, 3.f, 4.f);
const __m128 m128_0123 = _mm_set_ps(0.f, 1.f, 2.f, 3.f);

#endif // SURGE_SRC_COMMON_DSP_VEMBERTECH_SHARED_H
