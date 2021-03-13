#pragma once

#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

#include "BBDFilters.h"
#include "BBDNonlin.h"

template <size_t STAGES> class BBDDelayLine
{
  public:
    BBDDelayLine() = default;

    void prepare(double sampleRate);
    void setWaveshapeParams(float drive) { waveshape2.setDrive(drive); }
    void setFilterFreq(float freqHz);
    void setDelayTime(float delaySec);

    inline float process(float u)
    {
        std::array<std::complex<float>, FilterSpec::N_filt> xOutAccum;
        std::fill(xOutAccum.begin(), xOutAccum.end(), std::complex<float>());

        while (tn < Ts)
        {
            if (evenOn)
            {
                std::array<std::complex<float>, FilterSpec::N_filt> gIn;
                float sum = 0.0f;
                for (size_t i = 0; i < FilterSpec::N_filt; ++i)
                {
                    gIn[i] = inputFilters[i].calcGUp();
                    sum += xIn[i].real() * gIn[i].real() - xIn[i].imag() * gIn[i].imag();
                }
                buffer[bufferPtr++] = sum;
                bufferPtr = (bufferPtr < STAGES) ? bufferPtr : 0;
            }
            else
            {
                auto yBBD = buffer[bufferPtr];
                auto delta = yBBD - yBBD_old;
                yBBD_old = yBBD;
                for (size_t i = 0; i < FilterSpec::N_filt; ++i)
                    xOutAccum[i] += outputFilters[i].calcGUp() * delta;
            }

            evenOn = !evenOn;
            tn += Ts_bbd;
        }
        tn -= Ts;

        for (auto &iFilt : inputFilters)
        {
            iFilt.calcGDown();
            iFilt.process(u);
        }

        float sum = 0.0f;
        for (size_t i = 0; i < FilterSpec::N_filt; ++i)
        {
            outputFilters[i].calcGDown();
            outputFilters[i].process(xOutAccum[i]);
            sum += xOut[i].real();
        }

        float output = H0 * yBBD_old + sum;

        return waveshape2.processSample(output);
    }

  private:
    float FS = 48000.0f;
    float Ts = 1.0f / FS;
    float Ts_bbd = Ts;

    std::vector<InputFilter> inputFilters;
    std::vector<OutputFilter> outputFilters;
    float H0 = 1.0f;

    std::array<std::complex<float>, FilterSpec::N_filt> xIn;
    std::array<std::complex<float>, FilterSpec::N_filt> xOut;

    std::array<float, STAGES> buffer;
    size_t bufferPtr = 0;

    float yBBD_old = 0.0f;
    float tn = 0.0f;
    bool evenOn = true;

    BBDNonlin waveshape2;
};
