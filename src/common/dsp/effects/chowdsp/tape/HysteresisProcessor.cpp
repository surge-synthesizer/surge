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
#include "HysteresisProcessor.h"

namespace
{
static void typeConvert(const float *input, double *output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        output[i] = (double)input[i];
}

static void typeConvert(const double *input, float *output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
        output[i] = (float)input[i];
}

template <typename T>
static void interleaveSamples(const T *sourceL, const T *sourceR, T *dest, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        dest[2 * i] = sourceL[i];
        dest[2 * i + 1] = sourceR[i];
    }
}

template <typename T>
static void deinterleaveSamples(const T *source, T *destL, T *destR, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        destL[i] = source[2 * i];
        destR[i] = source[2 * i + 1];
    }
}
} // namespace

namespace chowdsp
{

void HysteresisProcessor::reset(double sample_rate)
{
    drive.reset(numSteps);
    width.reset(numSteps);
    sat.reset(numSteps);
    makeup.reset(numSteps);

    os.reset();
    dc_blocker.setBlockSize(BLOCK_SIZE);
    dc_blocker.suspend();
    dc_blocker.coeff_HP(35.0f / sample_rate, 0.707);
    dc_blocker.coeff_instantize();

#if CHOWTAPE_HYSTERESIS_USE_SIMD
    hProc.setSampleRate(sample_rate);
    hProc.reset();
#else
    for (size_t ch = 0; ch < 2; ++ch)
    {
        hProcs[ch].setSampleRate(sample_rate);
        hProcs[ch].reset();
    }
#endif
}

float calcMakeup(float width, float sat)
{
    return (1.0f + 0.6f * width) / (0.5f + 1.5f * (1.0f - sat));
}

void HysteresisProcessor::set_params(float driveVal, float satVal, float biasVal)
{
    auto widthVal = 1.0f - biasVal;
    auto makeupVal = calcMakeup(widthVal, satVal);

    drive.setTargetValue(driveVal);
    sat.setTargetValue(satVal);
    width.setTargetValue(widthVal);
    makeup.setTargetValue(makeupVal);
}

void HysteresisProcessor::set_solver(int newSolver) { solver = static_cast<SolverType>(newSolver); }

void HysteresisProcessor::process_block(float *dataL, float *dataR)
{
    bool needsSmoothing = drive.isSmoothing() || width.isSmoothing() || sat.isSmoothing();

    // upsample
    os.upsample(dataL, dataR);
    static constexpr int blockSizeUp = (int)Oversampling<2, BLOCK_SIZE>::getUpBlockSize();

    // convert from double to float
    double leftUp_d[blockSizeUp];
    typeConvert(os.leftUp, leftUp_d, blockSizeUp);

    double rightUp_d[blockSizeUp];
    typeConvert(os.rightUp, rightUp_d, blockSizeUp);

#if CHOWTAPE_HYSTERESIS_USE_SIMD
    double dataInterleaved alignas(16)[2 * blockSizeUp];
    interleaveSamples(leftUp_d, rightUp_d, dataInterleaved, blockSizeUp);

    switch (solver)
    {
    case RK2:
        if (needsSmoothing)
            process_internal_smooth_simd<RK2>(dataInterleaved, blockSizeUp);
        else
            process_internal_simd<RK2>(dataInterleaved, blockSizeUp);
        break;
    case RK4:
        if (needsSmoothing)
            process_internal_smooth_simd<RK4>(dataInterleaved, blockSizeUp);
        else
            process_internal_simd<RK4>(dataInterleaved, blockSizeUp);
        break;
    case NR4:
        if (needsSmoothing)
            process_internal_smooth_simd<NR4>(dataInterleaved, blockSizeUp);
        else
            process_internal_simd<NR4>(dataInterleaved, blockSizeUp);
        break;
    case NR8:
        if (needsSmoothing)
            process_internal_smooth_simd<NR8>(dataInterleaved, blockSizeUp);
        else
            process_internal_simd<NR8>(dataInterleaved, blockSizeUp);
        break;
    default:
        break;
    }

    deinterleaveSamples(dataInterleaved, leftUp_d, rightUp_d, blockSizeUp);

#else
    switch (solver)
    {
    case RK2:
        if (needsSmoothing)
            process_internal_smooth<RK2>(leftUp_d, rightUp_d, blockSizeUp);
        else
            process_internal<RK2>(leftUp_d, rightUp_d, blockSizeUp);
        break;
    case RK4:
        if (needsSmoothing)
            process_internal_smooth<RK4>(leftUp_d, rightUp_d, blockSizeUp);
        else
            process_internal<RK4>(leftUp_d, rightUp_d, blockSizeUp);
        break;
    case NR4:
        if (needsSmoothing)
            process_internal_smooth<NR4>(leftUp_d, rightUp_d, blockSizeUp);
        else
            process_internal<NR4>(leftUp_d, rightUp_d, blockSizeUp);
        break;
    case NR8:
        if (needsSmoothing)
            process_internal_smooth<NR8>(leftUp_d, rightUp_d, blockSizeUp);
        else
            process_internal<NR8>(leftUp_d, rightUp_d, blockSizeUp);
        break;
    default:
        break;
    }
#endif

    // convert back to float
    typeConvert(leftUp_d, os.leftUp, blockSizeUp);
    typeConvert(rightUp_d, os.rightUp, blockSizeUp);

    // downsample
    os.downsample(dataL, dataR);

    dc_blocker.process_block(dataL, dataR);
}

#if CHOWTAPE_HYSTERESIS_USE_SIMD
template <SolverType solverType>
void HysteresisProcessor::process_internal_simd(double *data, const int numSamples)
{
    for (int samp = 0; samp < 2 * numSamples; samp += 2)
    {
        auto curMakeup = makeup.getNextValue();

        auto inVec = SIMD_MM(load_pd)(&data[samp]);
        auto outVec =
            SIMD_MM(mul_pd)(hProc.process<solverType>(inVec), SIMD_MM(set1_pd)(curMakeup));
        SIMD_MM(store_pd)(&data[samp], outVec);
    }
}

template <SolverType solverType>
void HysteresisProcessor::process_internal_smooth_simd(double *data, const int numSamples)
{
    for (int samp = 0; samp < 2 * numSamples; samp += 2)
    {
        auto curDrive = drive.getNextValue();
        auto curSat = sat.getNextValue();
        auto curWidth = width.getNextValue();
        auto curMakeup = makeup.getNextValue();

        hProc.cook(curDrive, curWidth, curSat, false);

        auto inVec = SIMD_MM(load_pd)(&data[samp]);
        auto outVec =
            SIMD_MM(mul_pd)(hProc.process<solverType>(inVec), SIMD_MM(set1_pd)(curMakeup));
        SIMD_MM(store_pd)(&data[samp], outVec);
    }
}
#else
template <SolverType solverType>
void HysteresisProcessor::process_internal(double *dataL, double *dataR, const int numSamples)
{
    for (int samp = 0; samp < numSamples; samp++)
    {
        auto curMakeup = makeup.getNextValue();
        dataL[samp] = hProcs[0].process<solverType>(dataL[samp]) * curMakeup;
        dataR[samp] = hProcs[1].process<solverType>(dataR[samp]) * curMakeup;
    }
}

template <SolverType solverType>
void HysteresisProcessor::process_internal_smooth(double *dataL, double *dataR,
                                                  const int numSamples)
{
    for (int samp = 0; samp < numSamples; samp++)
    {
        auto curDrive = drive.getNextValue();
        auto curSat = sat.getNextValue();
        auto curWidth = width.getNextValue();
        auto curMakeup = makeup.getNextValue();

        hProcs[0].cook(curDrive, curWidth, curSat, false);
        hProcs[1].cook(curDrive, curWidth, curSat, false);

        dataL[samp] = hProcs[0].process<solverType>(dataL[samp]) * curMakeup;
        dataR[samp] = hProcs[1].process<solverType>(dataR[samp]) * curMakeup;
    }
}
#endif

} // namespace chowdsp
