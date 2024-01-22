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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_CHEWDROPOUT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_CHEWDROPOUT_H

#include <cmath>
#include "../shared/SmoothedValue.h"
#include "globals.h"

namespace chowdsp
{

class ChewDropout
{
  public:
    ChewDropout() = default;

    void setMix(float newMix) { mixSmooth.setTargetValue(newMix); }

    void setPower(float newPow)
    {
        for (int ch = 0; ch < 2; ++ch)
            powerSmooth[ch].setTargetValue(newPow);
    }

    void prepare(double sr)
    {
        mixSmooth.reset(sr, 0.01);
        mixSmooth.setCurrentAndTargetValue(mixSmooth.getTargetValue());

        for (int ch = 0; ch < 2; ++ch)
        {
            powerSmooth[ch].reset(sr, 0.005);
            powerSmooth[ch].setCurrentAndTargetValue(powerSmooth[ch].getTargetValue());
        }
    }

    void process(float *dataL, float *dataR)
    {
        if (mixSmooth.getTargetValue() == 0.0f && !mixSmooth.isSmoothing())
            return;

        for (int n = 0; n < BLOCK_SIZE; ++n)
        {
            auto mix = mixSmooth.getNextValue();
            dataL[n] = mix * dropout(dataL[n], 0) + (1.0f - mix) * dataL[n];
            dataR[n] = mix * dropout(dataR[n], 1) + (1.0f - mix) * dataR[n];
        }
    }

    inline float dropout(float x, int ch)
    {
        float sign = 0.0f;
        if (x > 0.0f)
            sign = 1.0f;
        else if (x < 0.0f)
            sign = -1.0f;

        return std::pow(std::abs(x), powerSmooth[ch].getNextValue()) * sign;
    }

  private:
    SmoothedValue<float, chowdsp::ValueSmoothingTypes::Linear> mixSmooth;
    SmoothedValue<float, chowdsp::ValueSmoothingTypes::Linear> powerSmooth[2];
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_CHEWDROPOUT_H
