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
#include "ChewProcessor.h"
#include <random>

namespace chowdsp
{

ChewProcessor::ChewProcessor()
{
    std::random_device rd;
    auto gen02 = std::minstd_rand(rd());
    std::uniform_real_distribution<float> distro02(0.0f, 2.0f);
    urng02 = std::bind(distro02, gen02);

    auto gen01 = std::minstd_rand(rd());
    std::uniform_real_distribution<float> distro01(0.0f, 1.0f);
    urng01 = std::bind(distro01, gen01);
}

void ChewProcessor::set_params(float new_freq, float new_depth, float new_var)
{
    freq = new_freq;
    depth = new_depth;
    var = new_var;
}

void ChewProcessor::prepare(double sr, int samplesPerBlock)
{
    sampleRate = (float)sr;

    dropout.prepare(sr);
    filt[0].reset(sampleRate, int(sr * 0.02));
    filt[1].reset(sampleRate, int(sr * 0.02));

    isCrinkled = false;
    samplesUntilChange = getDryTime();
    sampleCounter = 0;
}

void ChewProcessor::process_block(float *dataL, float *dataR)
{
    const float highFreq = std::min(22000.0f, 0.49f * sampleRate);
    const float freqChange = highFreq - 5000.0f;

    if (freq == 0.0f)
    {
        mix = 0.0f;
        filt[0].setFreq(highFreq);
        filt[1].setFreq(highFreq);
    }
    else if (freq == 1.0f)
    {
        mix = 1.0f;
        power = 3.0f * depth;
        filt[0].setFreq(highFreq - freqChange * depth);
        filt[1].setFreq(highFreq - freqChange * depth);
    }
    else if (sampleCounter >= samplesUntilChange)
    {
        sampleCounter = 0;
        isCrinkled = !isCrinkled;

        if (isCrinkled) // start crinkle
        {
            mix = 1.0f;
            power = (1.0f + urng02()) * depth;
            filt[0].setFreq(highFreq - freqChange * depth);
            filt[1].setFreq(highFreq - freqChange * depth);
            samplesUntilChange = getWetTime();
        }
        else // end crinkle
        {
            mix = 0.0f;
            filt[0].setFreq(highFreq);
            filt[1].setFreq(highFreq);
            samplesUntilChange = getDryTime();
        }
    }
    else
    {
        power = (1.0f + urng02()) * depth;
        if (isCrinkled)
        {
            filt[0].setFreq(highFreq - freqChange * depth);
            filt[1].setFreq(highFreq - freqChange * depth);
        }
    }

    dropout.setMix(mix);
    dropout.setPower(1.0f + power);

    dropout.process(dataL, dataR);
    filt[0].process(dataL, BLOCK_SIZE);
    filt[1].process(dataR, BLOCK_SIZE);

    sampleCounter += BLOCK_SIZE;
}

} // namespace chowdsp
