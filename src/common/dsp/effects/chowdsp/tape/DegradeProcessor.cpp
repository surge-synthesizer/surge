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
