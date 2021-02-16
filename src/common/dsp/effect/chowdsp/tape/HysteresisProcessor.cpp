#include "HysteresisProcessor.h"

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

    for (size_t ch = 0; ch < 2; ++ch)
    {
        hProcs[ch].setSampleRate(sample_rate);
        hProcs[ch].reset();
    }
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

void HysteresisProcessor::process_block(float *dataL, float *dataR)
{
    bool needsSmoothing = drive.isSmoothing() || width.isSmoothing() || sat.isSmoothing();

    os.upsample(dataL, dataR);

    if (needsSmoothing)
        process_internal_smooth(os.leftUp, os.rightUp, os.getUpBlockSize());
    else
        process_internal(os.leftUp, os.rightUp, os.getUpBlockSize());

    os.downsample(dataL, dataR);

    dc_blocker.process_block(dataL, dataR);
}

void HysteresisProcessor::process_internal(float *dataL, float *dataR, const int numSamples)
{
    for (int samp = 0; samp < numSamples; samp++)
    {
        auto curMakeup = makeup.getNextValue();
        dataL[samp] = (float)hProcs[0].process((double)dataL[samp]) * curMakeup;
        dataR[samp] = (float)hProcs[1].process((double)dataR[samp]) * curMakeup;
    }
}

void HysteresisProcessor::process_internal_smooth(float *dataL, float *dataR, const int numSamples)
{
    for (int samp = 0; samp < numSamples; samp++)
    {
        auto curDrive = drive.getNextValue();
        auto curSat = sat.getNextValue();
        auto curWidth = width.getNextValue();
        auto curMakeup = makeup.getNextValue();

        hProcs[0].cook(curDrive, curWidth, curSat);
        hProcs[1].cook(curDrive, curWidth, curSat);

        dataL[samp] = (float)hProcs[0].process((double)dataL[samp]) * curMakeup;
        dataR[samp] = (float)hProcs[1].process((double)dataR[samp]) * curMakeup;
    }
}

} // namespace chowdsp
