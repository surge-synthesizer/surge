#include "BBDDelayLine.h"

template <size_t STAGES> void BBDDelayLine<STAGES>::prepare(double sampleRate)
{
    evenOn = true;
    FS = (float)sampleRate;
    Ts = 1.0f / FS;

    bufferPtr = 0;
    std::fill(buffer.begin(), buffer.end(), 0.0f);

    using namespace FilterSpec;
    H0 = 0.0f;
    for (size_t i = 0; i < N_filt; ++i)
    {
        inputFilters.push_back(InputFilter(iFiltRoot[i], iFiltPole[i], Ts, &xIn[i]));
        outputFilters.push_back(OutputFilter(oFiltRoot[i], oFiltPole[i], Ts, &xOut[i]));
        H0 -= std::real(outputFilters.back().getGCoef());
    }

    tn = 0.0f;
    evenOn = true;

    waveshape2.reset(sampleRate);
}

template <size_t STAGES> void BBDDelayLine<STAGES>::setFilterFreq(float freqHz)
{
    for (auto &iFilt : inputFilters)
    {
        iFilt.set_freq(freqHz);
        iFilt.set_time(tn);
    }

    for (auto &oFilt : outputFilters)
    {
        oFilt.set_freq(freqHz);
        oFilt.set_time(tn);
    }
}

template <size_t STAGES> void BBDDelayLine<STAGES>::setDelayTime(float delaySec)
{
    auto clock_rate_hz = (2.0f * (float)STAGES) / delaySec;
    Ts_bbd = 1.0f / clock_rate_hz;

    for (auto &iFilt : inputFilters)
        iFilt.set_delta(2 * Ts_bbd);

    for (auto &oFilt : outputFilters)
        oFilt.set_delta(2 * Ts_bbd);
}

template class BBDDelayLine<64>;
template class BBDDelayLine<128>;
template class BBDDelayLine<256>;
template class BBDDelayLine<512>;
template class BBDDelayLine<1024>;
template class BBDDelayLine<2048>;
template class BBDDelayLine<4096>;
template class BBDDelayLine<8192>;
