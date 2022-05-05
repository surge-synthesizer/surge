// This is a .h on purpose. C++ template stuff.

#pragma once

#include "ChorusEffect.h"
#include <algorithm>

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
        voicepanL4[i] = _mm_set1_ps(voicepan[i][0]);
        voicepanR4[i] = _mm_set1_ps(voicepan[i][1]);
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
        feedback.set_target_smoothed(0.5f * amp_to_linear(*f[ch_feedback]));
        float rate = storage->envelope_rate_linear(-*f[1]) *
                     (fxdata->p[ch_rate].temposync ? storage->temposyncratio : 1.f);
        float tm = storage->note_to_pitch_ignoring_tuning(12 * *f[0]) *
                   (fxdata->p[ch_time].temposync ? storage->temposyncratio_inv : 1.f);
        for (int i = 0; i < v; i++)
        {
            lfophase[i] += rate;

            if (lfophase[i] > 1)
                lfophase[i] -= 1;

            float lfoout = (2.f * fabs(2.f * lfophase[i] - 1.f) - 1.f) * *f[ch_depth];
            time[i].newValue(storage->samplerate * tm * (1 + lfoout));
        }

        hp.coeff_HP(hp.calc_omega(*f[ch_lowcut] * (1.f / 12.f)), 0.707);
        lp.coeff_LP2B(lp.calc_omega(*f[ch_highcut] * (1.f / 12.f)), 0.707);
        mix.set_target_smoothed(*f[ch_mix]);
        width.set_target_smoothed(storage->db_to_linear(*f[ch_width]));
    }
}

template <int v> void ChorusEffect<v>::process(float *dataL, float *dataR)
{
    setvars(false);

    float tbufferL alignas(16)[BLOCK_SIZE];
    float tbufferR alignas(16)[BLOCK_SIZE];
    float fbblock alignas(16)[BLOCK_SIZE];

    clear_block(tbufferL, BLOCK_SIZE_QUAD);
    clear_block(tbufferR, BLOCK_SIZE_QUAD);

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        __m128 L = _mm_setzero_ps(), R = _mm_setzero_ps();

        for (int j = 0; j < v; j++)
        {
            time[j].process();
            float vtime = time[j].v;
            int i_dtime = max(BLOCK_SIZE, min((int)vtime, max_delay_length - FIRipol_N - 1));
            int rp = ((wpos - i_dtime + k) - FIRipol_N) & (max_delay_length - 1);
            int sinc = FIRipol_N * limit_range((int)(FIRipol_M * (float(i_dtime + 1) - vtime)), 0,
                                               FIRipol_M - 1);

            __m128 vo;
            vo = _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sinc]), _mm_loadu_ps(&buffer[rp]));
            vo = _mm_add_ps(vo, _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sinc + 4]),
                                           _mm_loadu_ps(&buffer[rp + 4])));
            vo = _mm_add_ps(vo, _mm_mul_ps(_mm_load_ps(&storage->sinctable1X[sinc + 8]),
                                           _mm_loadu_ps(&buffer[rp + 8])));

            L = _mm_add_ps(L, _mm_mul_ps(vo, voicepanL4[j]));
            R = _mm_add_ps(R, _mm_mul_ps(vo, voicepanR4[j]));
        }
        L = sum_ps_to_ss(L);
        R = sum_ps_to_ss(R);
        _mm_store_ss(&tbufferL[k], L);
        _mm_store_ss(&tbufferR[k], R);
    }

    if (!fxdata->p[ch_highcut].deactivated)
    {
        lp.process_block(tbufferL, tbufferR);
    }

    if (!fxdata->p[ch_lowcut].deactivated)
    {
        hp.process_block(tbufferL, tbufferR);
    }

    add_block(tbufferL, tbufferR, fbblock, BLOCK_SIZE_QUAD);
    feedback.multiply_block(fbblock, BLOCK_SIZE_QUAD);
    hardclip_block(fbblock, BLOCK_SIZE_QUAD);
    accumulate_block(dataL, fbblock, BLOCK_SIZE_QUAD);
    accumulate_block(dataR, fbblock, BLOCK_SIZE_QUAD);

    if (wpos + BLOCK_SIZE >= max_delay_length)
    {
        for (int k = 0; k < BLOCK_SIZE; k++)
        {
            buffer[(wpos + k) & (max_delay_length - 1)] = fbblock[k];
        }
    }
    else
    {
        copy_block(fbblock, &buffer[wpos], BLOCK_SIZE_QUAD);
    }

    if (wpos == 0)
        for (int k = 0; k < FIRipol_N; k++)
            buffer[k + max_delay_length] =
                buffer[k]; // copy buffer so FIR-core doesn't have to wrap

    // scale width
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(tbufferL, tbufferR, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, tbufferL, tbufferR, BLOCK_SIZE_QUAD);

    mix.fade_2_blocks_to(dataL, tbufferL, dataR, tbufferR, dataL, dataR, BLOCK_SIZE_QUAD);

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
