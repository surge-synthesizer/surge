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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_DEGRADEFILTER_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_DEGRADEFILTER_H

#include <cmath>
#include "../shared/SmoothedValue.h"

namespace chowdsp
{

/** Lowpass filter for tape degrade effect */
class DegradeFilter
{
  public:
    DegradeFilter() { freq.reset(numSteps); }
    ~DegradeFilter() {}

    void reset(float sampleRate, int steps = 0)
    {
        fs = sampleRate;
        for (int n = 0; n < 2; ++n)
            z[n] = 0.0f;

        if (steps > 0)
            freq.reset(steps);

        freq.setCurrentAndTargetValue(freq.getTargetValue());
        calcCoefs(freq.getCurrentValue());
    }

    inline void calcCoefs(float fc)
    {
        float wc = 2 * M_PI * fc / fs;
        float c = 1.0f / std::tan(wc / 2.0f);
        float a0 = c + 1.0f;

        b[0] = 1 / a0;
        b[1] = b[0];
        a[1] = (1.0f - c) / a0;
    }

    inline void process(float *buffer, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            if (freq.isSmoothing())
                calcCoefs(freq.getNextValue());

            buffer[n] = processSample(buffer[n]);
        }
    }

    inline float processSample(float x)
    {
        float y = z[1] + x * b[0];
        z[1] = x * b[1] - y * a[1];
        return y;
    }

    void setFreq(float newFreq) { freq.setTargetValue(newFreq); }

  private:
    SmoothedValue<float, chowdsp::ValueSmoothingTypes::Multiplicative> freq = 20000.0f;
    float fs = 44100.0f;
    const int numSteps = 200;

    float a[2] = {1.0f, 0.0f};
    float b[2] = {1.0f, 0.0f};
    float z[2] = {1.0f, 0.0f};
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_DEGRADEFILTER_H
