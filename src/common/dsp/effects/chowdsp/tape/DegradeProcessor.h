#pragma once

#include "DegradeFilter.h"
#include "DegradeNoise.h"
#include <vembertech/lipol.h>

namespace chowdsp
{

class DegradeProcessor
{
  public:
    DegradeProcessor();

    void set_params(float depthParam, float amtParam, float varParam);
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void process_block(float *dataL, float *dataR);

  private:
    float depthParam;
    float amtParam;
    float varParam;

    DegradeNoise noiseProc[2];
    DegradeFilter filterProc[2];
    lipol_ps gain alignas(16);

    std::function<float()> urng; // A uniform -0.5,0.5 RNG

    float fs = 44100.0f;
};

} // namespace chowdsp
