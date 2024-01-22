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

#include "ToneControl.h"
#include "globals.h"

namespace chowdsp
{

namespace
{
constexpr double slewTime = 0.05;
} // namespace

ToneStage::ToneStage()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        lowGain[ch] = 1.0f;
        highGain[ch] = 1.0f;
        tFreq[ch] = 500.0f;
    }
}

void ToneStage::prepare(double sampleRate)
{
    fs = (float)sampleRate;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto resetSmoothValue = [sampleRate](SmoothGain &value) {
            value.reset(sampleRate, slewTime);
            value.setCurrentAndTargetValue(value.getTargetValue());
        };

        resetSmoothValue(lowGain[ch]);
        resetSmoothValue(highGain[ch]);
        resetSmoothValue(tFreq[ch]);

        tone[ch].reset();
        tone[ch].calcCoefs(lowGain[ch].getTargetValue(), highGain[ch].getTargetValue(),
                           tFreq[ch].getTargetValue(), fs);
    }
}

void setSmoothValues(ToneStage::SmoothGain values[2], float newValue)
{
    if (newValue == values[0].getTargetValue())
        return;

    values[0].setTargetValue(newValue);
    values[1].setTargetValue(newValue);
}

void ToneStage::setLowGain(float lowGainDB)
{
    setSmoothValues(lowGain, std::pow(10.0f, lowGainDB * 0.05f));
}
void ToneStage::setHighGain(float highGainDB)
{
    setSmoothValues(highGain, std::pow(10.0f, highGainDB * 0.05f));
}
void ToneStage::setTransFreq(float newTFreq) { setSmoothValues(tFreq, newTFreq); }

void ToneStage::processBlock(float *dataL, float *dataR)
{
    float *buffer[] = {dataL, dataR};

    for (int ch = 0; ch < 2; ++ch)
    {
        if (lowGain[ch].isSmoothing() || highGain[ch].isSmoothing() || tFreq[ch].isSmoothing())
        {
            auto *x = buffer[ch];
            for (int n = 0; n < BLOCK_SIZE; ++n)
            {
                tone[ch].calcCoefs(lowGain[ch].getNextValue(), highGain[ch].getNextValue(),
                                   tFreq[ch].getNextValue(), fs);
                x[n] = tone[ch].processSample(x[n]);
            }
        }
        else
        {
            tone[ch].processBlock(buffer[ch], BLOCK_SIZE);
        }
    }
}

//===================================================
void ToneControl::prepare(double sampleRate)
{
    toneIn.prepare(sampleRate);
    toneOut.prepare(sampleRate);
}

void ToneControl::set_params(float tone)
{
    bass = 0.0f;
    treble = 0.0f;
    if (tone > 0.0f) // boost treble
    {
        treble = std::abs(tone);
        bass = -0.5f * treble;
    }
    else if (tone < 0.0f) // boost bass
    {
        bass = std::abs(tone);
        treble = -0.5f * bass;
    }
}

void ToneControl::processBlockIn(float *dataL, float *dataR)
{
    toneIn.setLowGain(dbScale * bass);
    toneIn.setHighGain(dbScale * treble);
    toneIn.setTransFreq(800.0f); // @TODO: make this a parameter later...

    toneIn.processBlock(dataL, dataR);
}

void ToneControl::processBlockOut(float *dataL, float *dataR)
{
    toneOut.setLowGain(-1.0f * dbScale * bass);
    toneOut.setHighGain(-1.0f * dbScale * treble);
    toneOut.setTransFreq(800.0f); // @TODO: make this a parameter later...

    toneOut.processBlock(dataL, dataR);
}

} // namespace chowdsp
