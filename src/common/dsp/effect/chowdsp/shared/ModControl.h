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

#pragma once

#include <vt_dsp/lipol.h>

namespace chowdsp
{

class ModControl
{
public:
    ModControl()
    {
        lfophase = 0.0f;
        lfosandhtarget = 0.0f;

        for (int i = 0; i < LFO_TABLE_SIZE; ++i)
            sin_lfo_table[i] = sin(2.0 * M_PI * i / LFO_TABLE_SIZE);
    }

    enum mod_waves
    {
        mod_sine = 0,
        mod_tri,
        mod_saw,
        mod_snh,
    };

    inline void pre_process(int mwave, float rate, float depth_val)
    {
        lfophase += rate;
        bool lforeset = false;
        if (lfophase > 1)
        {
            lforeset = true;
            lfophase -= 1;
        }
        float lfoout = lfoval.v;
        float thisphase = lfophase;
        switch (mwave)
        {
        case mod_sine:
        {
            float ps = thisphase * LFO_TABLE_SIZE;
            int psi = (int)ps;
            float psf = ps - psi;
            int psn = (psi + 1) & LFO_TABLE_MASK;
            lfoout = sin_lfo_table[psi] * (1.0 - psf) + psf * sin_lfo_table[psn];
            lfoval.newValue(lfoout);
        }
        break;
        case mod_tri:
            lfoout = (2.f * fabs(2.f * thisphase - 1.f) - 1.f);
            lfoval.newValue(lfoout);
            break;
        case mod_saw: // but we gotta be gentler than a pure saw. So do more like a heavily
                      // skewed triangle
        {
            float cutSawAt = 0.95;
            if (thisphase < cutSawAt)
            {
                auto usephase = thisphase / cutSawAt;
                lfoout = usephase * 2.0f - 1.f;
            }
            else
            {
                auto usephase = (thisphase - cutSawAt) / (1.0 - cutSawAt);
                lfoout = (1.0 - usephase) * 2.f - 1.f;
            }
            lfoval.newValue(lfoout);
            break;
        }
        case mod_snh: // S&H random noise. Needs smoothing over the jump like the triangle
        {
            if (lforeset)
            {
                lfosandhtarget = 1.f * rand() / (float)RAND_MAX - 1.f;
            }
            // FIXME - exponential creep up. We want to get there in a time related to our rate
            auto cv = lfoval.v;
            auto diff = (lfosandhtarget - cv) * rate * 2;
            lfoval.newValue(cv + diff);
        }
        break;
        }

        depth.newValue(depth_val);
    }

    inline float value() noexcept
    {
        auto y = lfoval.v * depth.v;
        lfoval.process();
        return y;
    }

    inline void post_process()
    {
        depth.process();
    }

private:
    lipol<float, true> lfoval;
    lipol<float, true> depth;
    float lfophase;
    float lfosandhtarget;

    static constexpr int LFO_TABLE_SIZE = 8192;
    static constexpr int LFO_TABLE_MASK = LFO_TABLE_SIZE - 1;
    float sin_lfo_table[LFO_TABLE_SIZE];
};

} // namespace chowdsp
