#pragma once

#include "../shared/SmoothedValue.h"
#include "../shared/Oversampling.h"
#include "BiquadFilter.h"
#include "HysteresisProcessing.h"

namespace chowdsp
{

class HysteresisProcessor
{
  public:
    HysteresisProcessor() = default;

    void reset(double sample_rate);

    void set_params(float drive, float sat, float bias);
    void process_block(float *dataL, float *dataR);

  private:
    enum
    {
        numSteps = 500,
    };

    void process_internal(float *dataL, float *dataR, const int numSamples);
    void process_internal_smooth(float *dataL, float *dataR, const int numSamples);

    SmoothedValue<float, ValueSmoothingTypes::Linear> drive;
    SmoothedValue<float, ValueSmoothingTypes::Linear> width;
    SmoothedValue<float, ValueSmoothingTypes::Linear> sat;
    SmoothedValue<float, ValueSmoothingTypes::Multiplicative> makeup;

    HysteresisProcessing hProcs[2];
    Oversampling<2, BLOCK_SIZE> os;
    BiquadFilter dc_blocker;
};

} // namespace chowdsp
