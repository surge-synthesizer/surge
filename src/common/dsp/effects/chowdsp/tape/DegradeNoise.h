#pragma once

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
