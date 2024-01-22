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
#include "LevelDetector.h"

namespace chowdsp
{

void LevelDetector::reset(double sample_rate)
{
    exp_factor = -2.0 * M_PI * 1000.0 / sample_rate;

    // zero state
    z[0] = 0.0f;
    z[1] = 0.0f;
}

void LevelDetector::set_attack_time(float time_ms) { tau_attack = calc_time_constant(time_ms); }

void LevelDetector::set_release_time(float time_ms) { tau_release = calc_time_constant(time_ms); }

float LevelDetector::calc_time_constant(float time_ms)
{
    return time_ms < 1.0e-3f ? 0.f : std::exp(exp_factor / time_ms);
}

} // namespace chowdsp
