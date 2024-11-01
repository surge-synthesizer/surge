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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHORUSEFFECTIMPL_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHORUSEFFECTIMPL_H

#include "ChorusEffect.h"
#include <algorithm>

#include "globals.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"
#include "sst/basic-blocks/dsp/Clippers.h"

using std::max;
using std::min;

template <int v>
ChorusEffect<v>::ChorusEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), lp(storage), hp(storage)
{
    mix.set_blocksize(BLOCK_SIZE);
    feedback.set_blocksize(BLOCK_SIZE);
}

template <int v> ChorusEffect<v>::~ChorusEffect() {}

template <int v> void ChorusEffect<v>::init()
{
    memset(buffer, 0, (max_delay_length + FIRipol_N) * sizeof(float));
    wpos = 0;
    envf = 0;
    const float gainscale = 1 / sqrt((float)v);

    for (int i = 0; i < v; i++)
    {
        time[i].setRate(0.001);
        float x = i;
        x /= (float)(v - 1);
        lfophase[i] = x;
        x = 2.f * x - 1.f;
        voicepan[i][0] = sqrt(0.5 - 0.5 * x) * gainscale;
        voicepan[i][1] = sqrt(0.5 + 0.5 * x) * gainscale;
        voicepanL4[i] = SIMD_MM(set1_ps)(voicepan[i][0]);
        voicepanR4[i] = SIMD_MM(set1_ps)(voicepan[i][1]);
    }

    setvars(true);
}

template <int v> void ChorusEffect<v>::setvars(bool init)
{
    if (init)
    {
        feedback.set_target(0.5f * amp_to_linear(fxdata->p[ch_feedback].val.f));
        hp.coeff_HP(hp.calc_omega(fxdata->p[ch_lowcut].val.f / 12.0), 0.707);
        lp.coeff_LP2B(lp.calc_omega(fxdata->p[ch_highcut].val.f / 12.0), 0.707);
        mix.set_target(fxdata->p[ch_mix].val.f);
        width.set_target(storage->db_to_linear(fxdata->p[ch_width].val.f));
    }
    else
    {
        feedback.set_target_smoothed(0.5f * amp_to_linear(*pd_float[ch_feedback]));
        float rate = storage->envelope_rate_linear(-*pd_float[1]) *
                     (fxdata->p[ch_rate].temposync ? storage->temposyncratio : 1.f);
        float tm = storage->note_to_pitch_ignoring_tuning(12 * *pd_float[0]) *
                   (fxdata->p[ch_time].temposync ? storage->temposyncratio_inv : 1.f);
        for (int i = 0; i < v; i++)
        {
            lfophase[i] += rate;

            if (lfophase[i] > 1)
                lfophase[i] -= 1;

            float lfoout = (2.f * fabs(2.f * lfophase[i] - 1.f) - 1.f) * *pd_float[ch_depth];
            time[i].newValue(storage->samplerate * tm * (1 + lfoout));
        }

        hp.coeff_HP(hp.calc_omega(*pd_float[ch_lowcut] * (1.f / 12.f)), 0.707);
        lp.coeff_LP2B(lp.calc_omega(*pd_float[ch_highcut] * (1.f / 12.f)), 0.707);
        mix.set_target_smoothed(*pd_float[ch_mix]);
        width.set_target_smoothed(storage->db_to_linear(*pd_float[ch_width]));
    }
}

template <int v> void ChorusEffect<v>::process(float *dataL, float *dataR)
{
    namespace mech = sst::basic_blocks::mechanics;
    namespace sdsp = sst::basic_blocks::dsp;

    setvars(false);

    float tbufferL alignas(16)[BLOCK_SIZE];
    float tbufferR alignas(16)[BLOCK_SIZE];
    float fbblock alignas(16)[BLOCK_SIZE];

    mech::clear_block<BLOCK_SIZE>(tbufferL);
    mech::clear_block<BLOCK_SIZE>(tbufferR);

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        auto L = SIMD_MM(setzero_ps)(), R = SIMD_MM(setzero_ps)();

        for (int j = 0; j < v; j++)
        {
            time[j].process();
            float vtime = time[j].v;
            int i_dtime = max(BLOCK_SIZE, min((int)vtime, max_delay_length - FIRipol_N - 1));
            int rp = ((wpos - i_dtime + k) - FIRipol_N) & (max_delay_length - 1);
            int sinc = FIRipol_N * limit_range((int)(FIRipol_M * (float(i_dtime + 1) - vtime)), 0,
                                               FIRipol_M - 1);

            SIMD_M128 vo;
            vo = SIMD_MM(mul_ps)(SIMD_MM(load_ps)(&storage->sinctable1X[sinc]),
                                 SIMD_MM(loadu_ps)(&buffer[rp]));
            vo = SIMD_MM(add_ps)(vo,
                                 SIMD_MM(mul_ps)(SIMD_MM(load_ps)(&storage->sinctable1X[sinc + 4]),
                                                 SIMD_MM(loadu_ps)(&buffer[rp + 4])));
            vo = SIMD_MM(add_ps)(vo,
                                 SIMD_MM(mul_ps)(SIMD_MM(load_ps)(&storage->sinctable1X[sinc + 8]),
                                                 SIMD_MM(loadu_ps)(&buffer[rp + 8])));

            L = SIMD_MM(add_ps)(L, SIMD_MM(mul_ps)(vo, voicepanL4[j]));
            R = SIMD_MM(add_ps)(R, SIMD_MM(mul_ps)(vo, voicepanR4[j]));
        }
        L = mech::sum_ps_to_ss(L);
        R = mech::sum_ps_to_ss(R);
        SIMD_MM(store_ss)(&tbufferL[k], L);
        SIMD_MM(store_ss)(&tbufferR[k], R);
    }

    if (!fxdata->p[ch_highcut].deactivated)
    {
        lp.process_block(tbufferL, tbufferR);
    }

    if (!fxdata->p[ch_lowcut].deactivated)
    {
        hp.process_block(tbufferL, tbufferR);
    }

    mech::add_block<BLOCK_SIZE>(tbufferL, tbufferR, fbblock);
    feedback.multiply_block(fbblock, BLOCK_SIZE_QUAD);
    sdsp::hardclip_block<BLOCK_SIZE>(fbblock);
    mech::accumulate_from_to<BLOCK_SIZE>(dataL, fbblock);
    mech::accumulate_from_to<BLOCK_SIZE>(dataR, fbblock);

    if (wpos + BLOCK_SIZE >= max_delay_length)
    {
        for (int k = 0; k < BLOCK_SIZE; k++)
        {
            buffer[(wpos + k) & (max_delay_length - 1)] = fbblock[k];
        }
    }
    else
    {
        mech::copy_from_to<BLOCK_SIZE>(fbblock, &buffer[wpos]);
    }

    if (wpos == 0)
        for (int k = 0; k < FIRipol_N; k++)
            buffer[k + max_delay_length] =
                buffer[k]; // copy buffer so FIR-core doesn't have to wrap

    // scale width
    applyWidth(tbufferL, tbufferR, width);

    mix.fade_2_blocks_inplace(dataL, tbufferL, dataR, tbufferR, BLOCK_SIZE_QUAD);

    wpos += BLOCK_SIZE;
    wpos = wpos & (max_delay_length - 1);
}

template <int v> void ChorusEffect<v>::suspend() { init(); }

template <int v> void ChorusEffect<v>::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[ch_rate].set_name("Rate");
    fxdata->p[ch_rate].set_type(ct_lforate);
    fxdata->p[ch_depth].set_name("Depth");
    fxdata->p[ch_depth].set_type(ct_percent);

    fxdata->p[ch_time].set_name("Time");
    fxdata->p[ch_time].set_type(ct_chorusmodtime);
    fxdata->p[ch_feedback].set_name("Feedback");
    fxdata->p[ch_feedback].set_type(ct_percent);

    fxdata->p[ch_lowcut].set_name("Low Cut");
    fxdata->p[ch_lowcut].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[ch_highcut].set_name("High Cut");
    fxdata->p[ch_highcut].set_type(ct_freq_audible_deactivatable_lp);

    fxdata->p[ch_mix].set_name("Mix");
    fxdata->p[ch_mix].set_type(ct_percent);
    fxdata->p[ch_width].set_name("Width");
    fxdata->p[ch_width].set_type(ct_decibel_narrow);

    fxdata->p[ch_rate].posy_offset = -1;
    fxdata->p[ch_depth].posy_offset = -1;

    fxdata->p[ch_time].posy_offset = 7;
    fxdata->p[ch_feedback].posy_offset = 3;

    fxdata->p[ch_lowcut].posy_offset = 5;
    fxdata->p[ch_highcut].posy_offset = 5;

    fxdata->p[ch_mix].posy_offset = 9;
    fxdata->p[ch_width].posy_offset = 5;
}
template <int v> const char *ChorusEffect<v>::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Modulation";
    case 1:
        return "Delay";
    case 2:
        return "EQ";
    case 3:
        return "Output";
    }
    return 0;
}
template <int v> int ChorusEffect<v>::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 7;
    case 2:
        return 13;
    case 3:
        return 19;
    }
    return 0;
}

template <int v> void ChorusEffect<v>::init_default_values()
{
    fxdata->p[ch_time].val.f = -6.f;
    fxdata->p[ch_rate].val.f = -2.f;
    fxdata->p[ch_depth].val.f = 0.3f;
    fxdata->p[ch_feedback].val.f = 0.5f;

    fxdata->p[ch_lowcut].val.f = -3.f * 12.f;
    fxdata->p[ch_lowcut].deactivated = false;

    fxdata->p[ch_highcut].val.f = 3.f * 12.f;
    fxdata->p[ch_highcut].deactivated = false;

    fxdata->p[ch_mix].val.f = 1.f;
    fxdata->p[ch_width].val.f = 0.f;
}

template <int v>
void ChorusEffect<v>::handleStreamingMismatches(int streamingRevision,
                                                int currentSynthStreamingRevision)
{
    if (streamingRevision <= 15)
    {
        fxdata->p[ch_lowcut].deactivated = false;
        fxdata->p[ch_highcut].deactivated = false;
    }
}

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHORUSEFFECTIMPL_H
