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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_EXCITER_LEVELDETECTOR_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_EXCITER_LEVELDETECTOR_H

#include <cmath>
#include "DSPUtils.h"

namespace chowdsp
{

/*
** Simple level detector using a ballistic filter,
** with a diode rectifier characteristic.
*/
class LevelDetector
{
  public:
    LevelDetector() = default;

    void reset(double sample_rate);
    void set_attack_time(float time_ms);
    void set_release_time(float time_ms);

    inline float process_sample(float x, size_t ch) noexcept
    {
        x = diode_rect(x);
        auto tau = x > z[ch] ? tau_attack : tau_release;
        z[ch] = x + tau * (z[ch] - x);
        return z[ch];
    }

  private:
    float calc_time_constant(float time_ms);

    inline float diode_rect(float x) const noexcept
    {
        constexpr float alpha = 0.05f / 0.0259f;
        constexpr float beta = 0.2f;
        return 25.0f * beta * (std::exp(alpha * x) - 1.0f);
    }

    float tau_attack = 0.0f;
    float tau_release = 0.0f;
    float exp_factor = 0.0f;

    float z[2];
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_EXCITER_LEVELDETECTOR_H
