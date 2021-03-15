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
