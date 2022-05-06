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

#include "lipol.h"
#include "SurgeStorage.h"

namespace Surge
{

class ModControl
{
  public:
    float samplerate{0}, samplerate_inv{0};
    ModControl(float sr, float sri) : samplerate(sr), samplerate_inv(sri)
    {
        lfophase = 0.0f;
        lfosandhtarget = 0.0f;

        for (int i = 0; i < LFO_TABLE_SIZE; ++i)
            sin_lfo_table[i] = sin(2.0 * M_PI * i / LFO_TABLE_SIZE);
    }
    ModControl() : ModControl(0, 0) {}

    enum mod_waves
    {
        mod_sine = 0,
        mod_tri,
        mod_saw,
        mod_noise,
        mod_snh,
        mod_square,
    };

    inline void pre_process(int mwave, float rate, float depth_val, float phase_offset)
    {
        assert(samplerate > 1000);
        bool lforeset = false;
        bool rndreset = false;
        float lfoout = lfoval.v;
        float phofs = fmod(fabs(phase_offset), 1.0);
        float thisrate = std::max(0.f, rate);
        float thisphase;

        if (thisrate > 0)
        {
            lfophase += thisrate;

            if (lfophase > 1)
            {
                lfophase = fmod(lfophase, 1.0);
            }

            thisphase = lfophase + phofs;
        }
        else
        {
            thisphase = phofs;

            if (mwave == mod_noise || mwave == mod_snh)
            {
                thisphase *= 16.f;

                if ((int)thisphase != (int)lfophase)
                {
                    rndreset = true;
                    lfophase = (float)((int)thisphase);
                }
            }
        }

        if (thisphase > 1)
        {
            thisphase = fmod(thisphase, 1.0);
        }

        /* We want to catch the first time that thisphase trips over the threshold. There's a couple
         * of ways to do this (like have a state variable), but this should work just as well. */
        if ((thisrate > 0 && thisphase - thisrate <= 0) || (thisrate == 0 && rndreset))
        {
            lforeset = true;
        }

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

            break;
        }
        case mod_tri:
        {
            lfoout = (2.f * fabs(2.f * thisphase - 1.f) - 1.f);

            lfoval.newValue(lfoout);

            break;
        }
        case mod_saw: // Gentler than a pure saw, more like a heavily skewed triangle
        {
            auto cutAt = 0.98f;
            float usephase;

            if (thisphase < cutAt)
            {
                usephase = thisphase / cutAt;
                lfoout = usephase * 2.0f - 1.f;
            }
            else
            {
                usephase = (thisphase - cutAt) / (1.0 - cutAt);
                lfoout = (1.0 - usephase) * 2.f - 1.f;
            }

            lfoval.newValue(lfoout);

            break;
        }
        case mod_square: // Gentler than a pure square, more like a trapezoid
        {
            auto cutOffset = 0.02f;
            auto m = 2.f / cutOffset;
            auto c2 = cutOffset / 2.f;

            if (thisphase < 0.5f - c2)
            {
                lfoout = 1.f;
            }
            else if ((thisphase >= 0.5 + c2) && (thisphase <= 1.f - cutOffset))
            {
                lfoout = -1.f;
            }
            else if ((thisphase > 0.5 - c2) && (thisphase < 0.5 + c2))
            {
                lfoout = -m * thisphase + (m / 2);
            }
            else
            {
                lfoout = (m * thisphase) - (2 * m) + m + 1;
            }

            lfoval.newValue(lfoout);

            break;
        }
        case mod_snh:   // Sample & Hold random
        case mod_noise: // noise (Sample & Glide smoothed random)
        {
            if (lforeset)
            {
                lfosandhtarget = (rand() / (float)RAND_MAX) - 1.f;
            }

            if (mwave == mod_noise)
            {
                // FIXME - exponential creep up. We want to get there in time related to our rate
                auto cv = lfoval.v;

                // thisphase * 0.98 prevents a glitch when LFO rate is disabled and phase offset is
                // 1 which constantly retriggers S&G
                thisrate = (rate == 0) ? thisphase * 0.98 : thisrate;
                auto diff = (lfosandhtarget - cv) * thisrate * 2 * samplerate_inv;
                lfoval.newValue(cv + diff);
            }
            else
            {
                lfoval.newValue(lfosandhtarget);
            }

            break;
        }
        }

        depth.newValue(depth_val);
    }

    inline float value() const noexcept { return lfoval.v * depth.v; }

    inline void post_process()
    {
        lfoval.process();
        depth.process();
    }

  private:
    lipol<float, true> lfoval{};
    lipol<float, true> depth{};
    float lfophase;
    float lfosandhtarget;

    static constexpr int LFO_TABLE_SIZE = 8192;
    static constexpr int LFO_TABLE_MASK = LFO_TABLE_SIZE - 1;
    float sin_lfo_table[LFO_TABLE_SIZE]{};
};
} // namespace Surge
