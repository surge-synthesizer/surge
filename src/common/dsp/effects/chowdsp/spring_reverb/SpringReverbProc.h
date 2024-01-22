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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SPRING_REVERB_SPRINGREVERBPROC_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SPRING_REVERB_SPRINGREVERBPROC_H

#include <functional>

#include "../shared/SmoothedValue.h"
#include "../shared/StateVariableFilter.h"

#include "ReflectionNetwork.h"
#include "SchroederAllpass.h"

namespace chowdsp
{
class SpringReverbProc
{
  public:
    SpringReverbProc();

    struct Params
    {
        float size = 0.5f;
        float decay = 0.5f;
        float reflections = 1.0f;
        float spin = 0.5f;
        float damping = 0.5f;
        float chaos = 0.0f;
        bool shake = false;
    };

    void prepare(float sampleRate, int samplesPerBlock);
    void setParams(const Params &params, int numSamples);
    void processBlock(float *left, float *right, const int numSamples);

  private:
    DelayLine<float, DelayLineInterpolationTypes::Lagrange3rd> delay{1 << 18};
    float feedbackGain;

    StateVariableFilter<float> dcBlocker;

    static constexpr int allpassStages = 16;
    using VecType = juce::dsp::SIMDRegister<float>;
    using APFCascade = std::array<SchroederAllpass<VecType, 2>, allpassStages>;
    APFCascade vecAPFs;

    std::function<float()> urng01; // A uniform 0,1 RNG
    SmoothedValue<float, ValueSmoothingTypes::Linear> chaosSmooth;

    // SIMD register for parallel allpass filter
    // format:
    //   [0]: y_left (feedforward path output)
    //   [1]: z_left (feedback path)
    //   [2-3]: same for right channel
    float simdState alignas(16)[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float fs = 48000.0f;

    StateVariableFilter<float> lpf;

    ReflectionNetwork reflectionNetwork; // early reflections

    int shakeCounter = -1;
    std::vector<float> shakeBuffer;
    int shakeBufferSize = 0;
    float shortShakeBuffer[BLOCK_SIZE];
};
} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SPRING_REVERB_SPRINGREVERBPROC_H
