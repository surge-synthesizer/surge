#pragma once

#include <cmath>
#include "../shared/SmoothedValue.h"

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
