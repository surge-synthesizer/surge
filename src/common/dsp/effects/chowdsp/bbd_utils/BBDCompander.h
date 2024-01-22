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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDCOMPANDER_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDCOMPANDER_H

#include "juce_core/juce_core.h"

/**
 * Signal averager used for BBD companding.
 * Borrowed from Fig. 11, from Huovilainen's DAFx-05 paper
 * https://www.dafx.de/paper-archive/2005/P_155.pdf
 */
class BBDAverager
{
  public:
    BBDAverager() = default;

    virtual void prepare(double sampleRate, int samplesPerBlock)
    {
        fs = sampleRate;
        onePole.prepare({sampleRate, (uint32)samplesPerBlock, 1});
    }

    void setCutoff(float fc)
    {
        onePole.coefficients = dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(fs, fc);
    }

    virtual inline float process(float x) noexcept
    {
        return onePole.processSample(std::pow(std::abs(x), 1.25f));
    }

  private:
    double fs = 48000.0;
    dsp::IIR::Filter<float> onePole;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BBDAverager)
};

class BBDExpander : public BBDAverager
{
  public:
    BBDExpander() = default;

    inline float process(float x) noexcept override { return BBDAverager::process(x) * x; }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BBDExpander)
};

class BBDCompressor : public BBDAverager
{
  public:
    BBDCompressor() = default;

    void prepare(double sampleRate, int samplesPerBlock) override
    {
        BBDAverager::prepare(sampleRate, samplesPerBlock);
        y1 = 0.0f;
    }

    inline float process(float x) noexcept override
    {
        float y = x / jmax(BBDAverager::process(y1), 0.0001f);
        y1 = y;
        return y;
    }

  private:
    float y1 = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BBDCompressor)
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDCOMPANDER_H
