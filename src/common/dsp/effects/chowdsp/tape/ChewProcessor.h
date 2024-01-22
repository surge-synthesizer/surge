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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_CHEWPROCESSOR_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_CHEWPROCESSOR_H

#include "ChewDropout.h"
#include "DegradeFilter.h"
#include <functional>

namespace chowdsp
{

class ChewProcessor
{
  public:
    ChewProcessor();

    void set_params(float freq, float depth, float var);
    void prepare(double sr, int samplesPerBlock);
    void process_block(float *dataL, float *dataR);

  private:
    float freq = 0.0f;
    float depth = 0.0f;
    float var = 0.0f;
    float mix = 0.0f;
    float power = 0.0f;

    ChewDropout dropout;
    DegradeFilter filt[2];

    std::function<float()> urng02; // A uniform 0,2 RNG
    std::function<float()> urng01; // A uniform 0,1 RNG
    int samplesUntilChange = 1000;
    bool isCrinkled = false;
    int sampleCounter = 0;

    float sampleRate = 44100.0f;

    inline int getDryTime()
    {
        auto tScale = std::pow(freq, 0.1f);
        auto varScale = std::pow(urng02(), var);

        const auto lowSamples = (int)((1.0 - tScale) * sampleRate * varScale);
        const auto highSamples = (int)((2 - 1.99 * tScale) * sampleRate * varScale);
        return lowSamples + int(urng01() * float(highSamples - lowSamples));
    }

    inline int getWetTime()
    {
        auto tScale = std::pow(freq, 0.1f);
        auto start = 0.2f + 0.8f * depth;
        auto end = start - (0.001f + 0.01f * depth);
        auto varScale = pow(urng02(), var);

        const auto lowSamples = (int)((1.0 - tScale) * sampleRate * varScale);
        const auto highSamples =
            (int)(((1.0 - tScale) + start - end * tScale) * sampleRate * varScale);
        return lowSamples + int(urng01() * float(highSamples - lowSamples));
    }
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_CHEWPROCESSOR_H
