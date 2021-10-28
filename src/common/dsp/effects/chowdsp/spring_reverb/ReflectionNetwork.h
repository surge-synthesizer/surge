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

#include <array>

#include "globals.h"
#include "UnitConversions.h"
#include <vembertech/portable_intrinsics.h>

#include "../shared/chowdsp_DelayLine.h"
#include "../shared/Shelf.h"

namespace chowdsp
{
class ReflectionNetwork
{
  public:
    ReflectionNetwork() = default;

    void prepare(float sampleRate, int samplesPerBlock)
    {
        fs = sampleRate;

        for (auto &d : delays)
            d.prepare({sampleRate, (juce::uint32)samplesPerBlock, 2});

        shelfFilter.reset();
    }

    void setParams(float reverbSize, float t60, float mix, float damping)
    {
        constexpr float baseDelaysSec[4] = {0.07f, 0.17f, 0.23f, 0.29f};

        float feedbackArr alignas(16)[4];
        for (int i = 0; i < 4; ++i)
        {
            auto delaySamples = baseDelaysSec[i] * reverbSize * fs;
            delays[i].setDelay(delaySamples);
            feedbackArr[i] = std::pow(0.001f, delaySamples / (t60 * fs));
            feedbackArr[i] *= 0.23f * mix * (0.735f + 0.235f * reverbSize);
        }

        feedback = _mm_load_ps(feedbackArr);

        float dampDB = -1.0f - 9.0f * damping;
        shelfFilter.calcCoefs(1.0f, (float)dB_to_linear((double)dampDB), 800.0f, fs);
    }

    inline float popSample(int ch) noexcept
    {
        float output alignas(16)[4];
        for (int i = 0; i < 4; ++i)
            output[i] = delays[i].popSample(ch);

        auto outVec = _mm_load_ps(output);

        // householder matrix
        constexpr auto householderFactor = -2.0f / (float)4;
        const auto sumXhh = vSum(outVec) * householderFactor;
        outVec = _mm_add_ps(outVec, _mm_load1_ps(&sumXhh));

        outVec = _mm_mul_ps(outVec, feedback);
        return shelfFilter.processSample(vSum(outVec) / (float)4);
    }

    inline void pushSample(int ch, float x) noexcept
    {
        for (auto &d : delays)
            d.pushSample(ch, x);
    }

  private:
    float fs = 48000.0f;

    using ReflectionDelay = DelayLine<float, chowdsp::DelayLineInterpolationTypes::Lagrange3rd>;
    std::array<ReflectionDelay, 4> delays{ReflectionDelay{1 << 18}, ReflectionDelay{1 << 18},
                                          ReflectionDelay{1 << 18}, ReflectionDelay{1 << 18}};

    __m128 feedback;

    chowdsp::ShelfFilter<> shelfFilter;
};
} // namespace chowdsp
