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
#include "BBDDelayLine.h"

template <size_t STAGES> void BBDDelayLine<STAGES>::prepare(double sampleRate)
{
    evenOn = true;
    FS = (float)sampleRate;
    Ts = 1.0f / FS;

    bufferPtr = 0;
    std::fill(buffer.begin(), buffer.end(), 0.0f);

    tn = 0.0f;
    evenOn = true;

    inputFilter = std::make_unique<InputFilterBank>(Ts);
    outputFilter = std::make_unique<OutputFilterBank>(Ts);
    H0 = outputFilter->calcH0();
}

template <size_t STAGES> void BBDDelayLine<STAGES>::setFilterFreq(float freqHz)
{
    inputFilter->set_freq(freqHz);
    inputFilter->set_time(tn);

    outputFilter->set_freq(freqHz);
    outputFilter->set_time(tn);
}

template class BBDDelayLine<64>;
template class BBDDelayLine<128>;
template class BBDDelayLine<256>;
template class BBDDelayLine<512>;
template class BBDDelayLine<1024>;
template class BBDDelayLine<2048>;
template class BBDDelayLine<4096>;
template class BBDDelayLine<8192>;
