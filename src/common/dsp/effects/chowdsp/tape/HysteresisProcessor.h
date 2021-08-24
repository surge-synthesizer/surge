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

    SmoothedValue<float, ValueSmoothingTypes::Linear> drive;
    SmoothedValue<float, ValueSmoothingTypes::Linear> width;
    SmoothedValue<float, ValueSmoothingTypes::Linear> sat;
    SmoothedValue<float, ValueSmoothingTypes::Multiplicative> makeup;

#if CHOWTAPE_HYSTERESIS_USE_SIMD
    template <SolverType solverType> void process_internal_simd(double *data, const int numSamples);

    template <SolverType solverType>
    void process_internal_smooth_simd(double *data, const int numSamples);

    HysteresisProcessing hProc;
#else
    template <SolverType solverType>
    void process_internal(double *dataL, double *dataR, const int numSamples);

    template <SolverType solverType>
    void process_internal_smooth(double *dataL, double *dataR, const int numSamples);

    HysteresisProcessing hProcs[2];
#endif

    SolverType solver;

    Oversampling<2, BLOCK_SIZE> os;
    BiquadFilter dc_blocker;
};

} // namespace chowdsp
