#pragma once

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
