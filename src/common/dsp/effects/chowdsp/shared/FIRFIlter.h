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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_FIRFILTER_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_FIRFILTER_H

#include <numeric>

namespace chowdsp
{

/*
** Simple class for FIR filtering.
** For the original implementation,
** see: https://github.com/jatinchowdhury18/FIRBenchmarks/blob/master/src/InnerProdFIR.h
*/
class FIRFilter
{
  public:
    FIRFilter(int order) : order(order)
    {
        h = new float[order];
        z[0] = new float[2 * order];
        z[1] = new float[2 * order];
    }

    ~FIRFilter()
    {
        delete[] h;
        delete[] z[0];
        delete[] z[1];
    }

    void reset()
    {
        zPtr = 0;
        std::fill(z[0], &z[0][2 * order], 0.0f);
        std::fill(z[1], &z[1][2 * order], 0.0f);
    }

    void setCoefs(float *coefs) { std::copy(coefs, &coefs[order], h); }

    inline void process(float *dataL, float *dataR, const int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            // insert input into double-buffered state
            z[0][zPtr] = dataL[n];
            z[0][zPtr + order] = dataL[n];
            z[1][zPtr] = dataR[n];
            z[1][zPtr + order] = dataR[n];

            // compute inner products
            dataL[n] = std::inner_product(z[0] + zPtr, z[0] + zPtr + order, h, 0.0f);
            dataR[n] = std::inner_product(z[1] + zPtr, z[1] + zPtr + order, h, 0.0f);

            zPtr = (zPtr == 0 ? order - 1 : zPtr - 1); // iterate state pointer in reverse
        }
    }

    inline void processBypassed(float *dataL, float *dataR, const int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            // insert input into double-buffered state
            z[0][zPtr] = dataL[n];
            z[0][zPtr + order] = dataL[n];
            z[1][zPtr] = dataR[n];
            z[1][zPtr + order] = dataR[n];

            zPtr = (zPtr == 0 ? order - 1 : zPtr - 1); // iterate state pointer in reverse
        }
    }

  protected:
    float *h;
    const int order;

  private:
    float *z[2];
    int zPtr = 0;
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_FIRFILTER_H
