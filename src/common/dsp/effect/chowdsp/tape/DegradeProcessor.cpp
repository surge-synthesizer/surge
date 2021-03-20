#include "DegradeProcessor.h"

namespace chowdsp
{

DegradeProcessor::DegradeProcessor()
{
    std::random_device rd;
    auto gen = std::minstd_rand(rd());
    std::uniform_real_distribution<float> distro(-0.5f, 0.5f);
    urng = std::bind(distro, gen);

    gain.set_blocksize(BLOCK_SIZE);
    gain.set_target(1.0f);
}

void DegradeProcessor::set_params(float depthParam, float amtParam, float varParam)
{
    float freqHz = 200.0f * std::pow(20000.0f / 200.0f, 1.0f - amtParam);
    float gainDB = -24.0f * depthParam;

    for (int ch = 0; ch < 2; ++ch)
    {
        noiseProc[ch].setGain(0.5f * depthParam * amtParam);
        filterProc[ch].setFreq(
            std::min(freqHz + (varParam * (freqHz / 0.6f) * urng()), 0.49f * fs));
    }

    gainDB = std::min(varParam * 36.0f * urng(), 3.0f);
    gain.set_target_smoothed(std::pow(10.0f, gainDB * 0.05f));
}

void DegradeProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    fs = (float)sampleRate;

    for (int ch = 0; ch < 2; ++ch)
    {
        noiseProc[ch].prepare();
        filterProc[ch].reset((float)sampleRate, 20);
    }
}

void DegradeProcessor::process_block(float *dataL, float *dataR)
{
    noiseProc[0].processBlock(dataL, BLOCK_SIZE);
    noiseProc[1].processBlock(dataR, BLOCK_SIZE);

    filterProc[0].process(dataL, BLOCK_SIZE);
    filterProc[1].process(dataR, BLOCK_SIZE);

    gain.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
}

} // namespace chowdsp
