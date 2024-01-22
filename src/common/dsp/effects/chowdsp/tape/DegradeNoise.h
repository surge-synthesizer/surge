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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_DEGRADENOISE_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_DEGRADENOISE_H

#include <random>
#include <functional>

namespace chowdsp
{

/** Noise for tape degrade effect */
class DegradeNoise
{
  public:
    DegradeNoise()
    {
        std::random_device rd;
        auto gen = std::minstd_rand(rd());
        std::uniform_real_distribution<float> distro(-0.5f, 0.5f);
        urng = std::bind(distro, gen);
    }

    ~DegradeNoise() {}

    void setGain(float newGain) { curGain = newGain; }

    void prepare() { prevGain = curGain; }

    void processBlock(float *buffer, int numSamples)
    {
        if (curGain == prevGain)
        {
            for (int n = 0; n < numSamples; ++n)
                buffer[n] += urng() * curGain;
        }
        else
        {
            for (int n = 0; n < numSamples; ++n)
                buffer[n] += urng() * ((curGain * (float)n / (float)numSamples) +
                                       (prevGain * (1.0f - (float)n / (float)numSamples)));

            prevGain = curGain;
        }
    }

  private:
    float curGain = 0.0f;
    float prevGain = curGain;

    std::function<float()> urng; // A uniform -0.5,0.5 RNG
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_DEGRADENOISE_H
