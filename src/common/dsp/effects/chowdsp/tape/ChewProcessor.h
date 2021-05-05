#pragma once

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
