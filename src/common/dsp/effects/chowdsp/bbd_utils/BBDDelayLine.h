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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDDELAYLINE_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDDELAYLINE_H

#include <algorithm>
#include <array>
#include <memory>
#include <numeric>
#include <vector>

#include "BBDFilterBank.h"

template <size_t STAGES> class BBDDelayLine
{
  public:
    BBDDelayLine() = default;

    void prepare(double sampleRate);
    void setFilterFreq(float freqHz);

    inline void setDelayTime(float delaySec) noexcept
    {
        const auto clock_rate_hz = (2.0f * (float)STAGES) / delaySec;
        Ts_bbd = 1.0f / clock_rate_hz;

        const auto doubleTs = 2 * Ts_bbd;
        inputFilter->set_delta(doubleTs);
        outputFilter->set_delta(doubleTs);
    }

    inline float process(float u) noexcept
    {
        SSEComplex xOutAccum;
        while (tn < Ts)
        {
            if (evenOn)
            {
                inputFilter->calcG();
                auto sum = vSum(SSEComplexMulReal(inputFilter->Gcalc, inputFilter->x));
                buffer[bufferPtr++] = sum;
                bufferPtr = (bufferPtr < STAGES) ? bufferPtr : 0;
            }
            else
            {
                auto yBBD = buffer[bufferPtr];
                auto delta = yBBD - yBBD_old;
                yBBD_old = yBBD;
                outputFilter->calcG();
                xOutAccum += outputFilter->Gcalc * delta;
            }

            evenOn = !evenOn;
            tn += Ts_bbd;
        }
        tn -= Ts;

        inputFilter->process(u);
        outputFilter->process(xOutAccum);
        float sum = vSum(xOutAccum._r);
        return H0 * yBBD_old + sum;
    }

  private:
    float FS = 48000.0f;
    float Ts = 1.0f / FS;
    float Ts_bbd = Ts;

    std::unique_ptr<InputFilterBank> inputFilter;
    std::unique_ptr<OutputFilterBank> outputFilter;
    float H0 = 1.0f;

    std::array<std::complex<float>, FilterSpec::N_filt> xIn;
    std::array<std::complex<float>, FilterSpec::N_filt> xOut;

    std::array<float, STAGES> buffer;
    size_t bufferPtr = 0;

    float yBBD_old = 0.0f;
    float tn = 0.0f;
    bool evenOn = true;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_BBD_UTILS_BBDDELAYLINE_H
