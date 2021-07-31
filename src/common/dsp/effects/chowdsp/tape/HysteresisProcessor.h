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
    void set_solver(int newSolver);
    void process_block(float *dataL, float *dataR);

  private:
    enum
    {
        numSteps = 500,
    };

    template <SolverType solverType>
    void process_internal(float *dataL, float *dataR, const int numSamples);

    template <SolverType solverType>
    void process_internal_smooth(float *dataL, float *dataR, const int numSamples);

    SmoothedValue<float, ValueSmoothingTypes::Linear> drive;
    SmoothedValue<float, ValueSmoothingTypes::Linear> width;
    SmoothedValue<float, ValueSmoothingTypes::Linear> sat;
    SmoothedValue<float, ValueSmoothingTypes::Multiplicative> makeup;

    HysteresisProcessing hProcs[2];
    SolverType solver;

    Oversampling<2, BLOCK_SIZE> os;
    BiquadFilter dc_blocker;
};

} // namespace chowdsp
