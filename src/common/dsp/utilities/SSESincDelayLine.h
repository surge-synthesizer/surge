/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

/*
 * This is a template class which encapsulates the SSE based SINC
 * interpolation in COMBquad_SSE2,just made available for other uses
 */

#ifndef SURGE_SSESINCDELAYLINE_H
#define SURGE_SSESINCDELAYLINE_H

#include "SurgeStorage.h"
#include "basic_dsp.h" // for 'sum_ps_to_ss'

template <int COMB_SIZE> // power of two
struct SSESincDelayLine
{
    static constexpr int comb_size = COMB_SIZE;

    float buffer alignas(16)[COMB_SIZE + FIRipol_N];
    int wp = 0;

    const float *sinctable{nullptr}; // a pointer copy of the storage member
    SSESincDelayLine(const float *st) : sinctable(st) { clear(); }

    inline void write(float f)
    {
        buffer[wp] = f;
        buffer[wp + (wp < FIRipol_N) * COMB_SIZE] = f;
        wp = (wp + 1) & (COMB_SIZE - 1);
    }

    inline float read(float delay)
    {
        auto iDelay = (int)delay;
        auto fracDelay = delay - iDelay;
        auto sincTableOffset = (int)((1 - fracDelay) * FIRipol_M) * FIRipol_N * 2;

        // So basically we interpolate around FIRipol_N (the 12 sample sinc)
        // remembering that FIRoffset is the offset to center your table at
        // a point ( it is FIRipol_N >> 1)
        int readPtr = (wp - iDelay - (FIRipol_N >> 1)) & (COMB_SIZE - 1);

        // And so now do what we do in COMBSSE2Quad
        __m128 a = _mm_loadu_ps(&buffer[readPtr]);
        __m128 b = _mm_loadu_ps(&sinctable[sincTableOffset]);
        __m128 o = _mm_mul_ps(a, b);

        a = _mm_loadu_ps(&buffer[readPtr + 4]);
        b = _mm_loadu_ps(&sinctable[sincTableOffset + 4]);
        o = _mm_add_ps(o, _mm_mul_ps(a, b));

        a = _mm_loadu_ps(&buffer[readPtr + 8]);
        b = _mm_loadu_ps(&sinctable[sincTableOffset + 8]);
        o = _mm_add_ps(o, _mm_mul_ps(a, b));

        float res;
        _mm_store_ss(&res, sum_ps_to_ss(o));

        return res;
    }

    inline float readLinear(float delay)
    {
        auto iDelay = (int)delay;
        auto frac = delay - iDelay;
        int RP = (wp - iDelay) & (COMB_SIZE - 1);
        int RPP = RP == 0 ? COMB_SIZE - 1 : RP - 1;
        return buffer[RP] * (1 - frac) + buffer[RPP] * frac;
    }

    inline float readZOH(float delay)
    {
        auto iDelay = (int)delay;
        int RP = (wp - iDelay) & (COMB_SIZE - 1);
        int RPP = RP == 0 ? COMB_SIZE - 1 : RP - 1;
        return buffer[RPP];
    }

    inline void clear()
    {
        memset((void *)buffer, 0, (COMB_SIZE + FIRipol_N) * sizeof(float));
        wp = 0;
    }
};

#endif // SURGE_SSESINCDELAYLINE_H
