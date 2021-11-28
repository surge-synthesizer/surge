/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

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
