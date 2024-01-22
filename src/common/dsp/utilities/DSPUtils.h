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

#ifndef SURGE_SRC_COMMON_DSP_UTILITIES_DSPUTILS_H
#define SURGE_SRC_COMMON_DSP_UTILITIES_DSPUTILS_H

#include <algorithm>
#include <cmath>

#include "globals.h"
#include "sst/basic-blocks/dsp/BlockInterpolators.h"

inline bool within_range(int lo, int value, int hi) { return ((value >= lo) && (value <= hi)); }

template <typename T, bool first = true>
using lipol = sst::basic_blocks::dsp::lipol<T, BLOCK_SIZE, first>;

// Use custom format (x^3) to represent gain internally, but save as decibel in XML-data
inline float amp_to_linear(float x)
{
    x = std::max(0.f, x);

    return x * x * x;
}

inline float linear_to_amp(float x) { return powf(std::clamp(x, 0.0000000001f, 1.f), 1.f / 3.f); }

inline float amp_to_db(float x) { return std::clamp((float)(18.f * log2(x)), -192.f, 96.f); }

inline float db_to_amp(float x) { return std::clamp(powf(2.f, x / 18.f), 0.f, 2.f); }

#endif // SURGE_SRC_COMMON_DSP_UTILITIES_DSPUTILS_H
