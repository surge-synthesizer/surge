/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_HYSTERESISPROCESSOR_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_HYSTERESISPROCESSOR_H

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

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_HYSTERESISPROCESSOR_H
