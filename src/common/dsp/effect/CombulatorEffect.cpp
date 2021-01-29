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

#include "CombulatorEffect.h"
#include "DebugHelpers.h"

CombulatorEffect::CombulatorEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), halfbandIN(6, true), halfbandOUT(6, true)
{
    width.set_blocksize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);

    qfus = (QuadFilterUnitState *)_aligned_malloc(2 * sizeof(QuadFilterUnitState), 16);

    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            for (int q = 0; q < n_filter_registers; ++q)
            {
                Reg[e][c][q] = 0;
            }
        }
    }
}

CombulatorEffect::~CombulatorEffect() { _aligned_free(qfus); }

void CombulatorEffect::init()
{
    setvars(true);
    bi = 0;
}

void CombulatorEffect::setvars(bool init)
{
    if (init)
    {

        for (int i = 0; i < 3; ++i)
        {
            cutoff[i].instantize();
            resonance[i].instantize();
            bandGain[i].instantize();
        }

        mix.set_target(1.f);

        width.instantize();
        mix.instantize();

        halfbandOUT.reset();
        halfbandIN.reset();
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            cutoff[i].newValue(*f[combulator_freq1 + i * 3]);
            resonance[i].newValue(*f[combulator_feedback]);
            bandGain[i].newValue(amp_to_linear(*f[combulator_gain1 + i * 3]));
        }
    }

    width.set_target_smoothed(db_to_linear(*f[combulator_width]));
}

inline void set1f(__m128 &m, int i, float f)
{
    *((float *)&m + i) = f;
}

inline float get1f(__m128 m, int i)
{
    return *((float *)&m + i);
}

void CombulatorEffect::process(float *dataL, float *dataR)
{
    if (bi == 0)
    {
        setvars(false);
    }
    bi = (bi + 1) & slowrate_m1;

    /*
     * Strategy: Upsample, Set the coefficients, run the filter, Downsample
     */

    // Upsample phase. This wqorks and needs no more attention.
    float dataOS alignas(16)[2][BLOCK_SIZE_OS];
    halfbandIN.process_block_U2(dataL, dataR, dataOS[0], dataOS[1]);

    /*
     * Select the coefficients. Here you have to base yourself on the mode switch and
     * call CoeffMaker with one fo the known models and subtypes, along with the resonance
     * and the frequency of the particular band.
     */
    int type = fut_comb_pos, subtype = 1;

    FilterUnitQFPtr filtptr = GetQFPtrFilterUnit(type, subtype);

    for (int i = 0; i < 3; ++i)
    {
        cutoff[i].newValue(*f[combulator_freq1 + i * 3]);
        resonance[i].newValue(*f[combulator_feedback]);
        bandGain[i].newValue(amp_to_linear(*f[combulator_gain1 + i * 3]));
    }

    /*
     * So now set up across the voices (e for 'entry' to match SurgeVoice) and the channels (c)
     */
    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            coeff[e][c].MakeCoeffs(cutoff[e].v, resonance[e].v, type, subtype, storage);

            for (int i = 0; i < n_cm_coeffs; i++)
            {
                set1f(qfus[c].C[i], e, coeff[e][c].C[i]);
                set1f(qfus[c].dC[i], e, coeff[e][c].dC[i]);
            }

            for (int i = 0; i < n_filter_registers; i++)
            {
                set1f(qfus[c].R[i], e, Reg[e][c][i]);
            }

            qfus[c].DB[e] = filterDelay[e][c];
            qfus[c].WP[e] = WP[e][c];
            qfus[c].WP[0] = subtype;

            qfus[c].active[e] = 0xFFFFFFFF;
            qfus[c].active[3] = 0;
        }
    }


    /* Run the filters. You need to grab a smoothed gain here rather than the hardcoded 0.3 */
    for (int s = 0; s < BLOCK_SIZE_OS; ++s)
    {
        auto l128 = _mm_setzero_ps();
        auto r128 = _mm_setzero_ps();

        if (filtptr)
        {
            l128 = filtptr(&(qfus[0]), _mm_set1_ps(dataOS[0][s]));
            r128 = filtptr(&(qfus[1]), _mm_set1_ps(dataOS[1][s]));
        }
        else
        {
            l128 = _mm_set1_ps(dataOS[0][s]);
            r128 = _mm_set1_ps(dataOS[1][s]);
        }

        float mixl = 0, mixr = 0;
        float tl[4], tr[4];

        _mm_store_ps(tl, l128);
        _mm_store_ps(tr, r128);

        for (int i = 0; i < 3; ++i)
        {
            mixl += tl[i] * bandGain[i].v;
            mixr += tr[i] * bandGain[i].v;

            // lag class only works at BLOCK_SIZE time, not BLOCK_SIZE_OS, so call process every other sample
            if (s % 2 == 0)
            {
                cutoff[i].process();
                resonance[i].process();
                bandGain[i].process();
            }
        }

        dataOS[0][s] = mixl;
        dataOS[1][s] = mixr;
    }

    /* and preserve those registers and stuff. This all works */
    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < n_cm_coeffs; i++)
        {
            for (int e = 0; e < 3; ++e)
            {
                coeff[e][c].C[i] = get1f(qfus[c].C[i], e);
            }
        }
        for (int i = 0; i < n_filter_registers; i++)
        {
            for (int e = 0; e < 3; ++e)
            {
                Reg[e][c][i] = get1f(qfus[c].R[i], e);
            }
        }
    }

    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            WP[e][c] = qfus[c].WP[e];
        }
    }

    /* Downsample out */
    halfbandOUT.process_block_D2(dataOS[0], dataOS[1]);
    copy_block(dataOS[0], L, BLOCK_SIZE_QUAD);
    copy_block(dataOS[1], R, BLOCK_SIZE_QUAD);

    // Add width and mix
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(L, R, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, L, R, BLOCK_SIZE_QUAD);

    mix.set_target_smoothed(limit_range(*f[combulator_mix], -1.f, 1.f));
    mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
}

void CombulatorEffect::suspend() { init(); }

const char *CombulatorEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Input";
    case 1:
        return "Combs";
    case 2:
        return "Mix";
    case 3:
        return "Output";
    }
    return 0;
}
int CombulatorEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 7;
    case 2:
        return 17;
    case 3:
        return 27;
    }
    return 0;
}

void CombulatorEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[combulator_input_gain].set_name("Gain");
    fxdata->p[combulator_input_gain].set_type(ct_decibel_narrow);
    fxdata->p[combulator_input_gain].posy_offset = 1;
    fxdata->p[combulator_noise_mix].set_name("Noise Mix");
    fxdata->p[combulator_noise_mix].set_type(ct_percent);
    fxdata->p[combulator_noise_mix].posy_offset = 1;

    fxdata->p[combulator_freq1].set_name("Frequency 1");
    fxdata->p[combulator_freq1].set_type(ct_freq_audible);
    fxdata->p[combulator_freq1].posy_offset = 3;
    fxdata->p[combulator_freq2].set_name("Frequency 2");
    fxdata->p[combulator_freq2].set_type(ct_freq_audible);
    fxdata->p[combulator_freq2].posy_offset = 3;
    fxdata->p[combulator_freq3].set_name("Frequency 3");
    fxdata->p[combulator_freq3].set_type(ct_freq_audible);
    fxdata->p[combulator_freq3].posy_offset = 3;
    fxdata->p[combulator_feedback].set_name("Feedback");
    fxdata->p[combulator_feedback].set_type(ct_percent);
    fxdata->p[combulator_feedback].posy_offset = 3;

    fxdata->p[combulator_gain1].set_name("Comb 1");
    fxdata->p[combulator_gain1].set_type(ct_amplitude);
    fxdata->p[combulator_gain1].posy_offset = 5;
    fxdata->p[combulator_gain2].set_name("Comb 2");
    fxdata->p[combulator_gain2].set_type(ct_amplitude);
    fxdata->p[combulator_gain2].posy_offset = 5;
    fxdata->p[combulator_gain3].set_name("Comb 3");
    fxdata->p[combulator_gain3].set_type(ct_amplitude);
    fxdata->p[combulator_gain3].posy_offset = 5;
    fxdata->p[combulator_lowpass].set_name("High Cut");
    fxdata->p[combulator_lowpass].set_type(ct_freq_audible);
    fxdata->p[combulator_lowpass].posy_offset = 5;

    fxdata->p[combulator_width].set_name("Width");
    fxdata->p[combulator_width].set_type(ct_decibel_narrow);
    fxdata->p[combulator_width].posy_offset = 7;
    fxdata->p[combulator_mix].set_name("Mix");
    fxdata->p[combulator_mix].set_type(ct_percent);
    fxdata->p[combulator_mix].posy_offset = 7;
}

void CombulatorEffect::init_default_values()
{
    for (int i = 0; i < combulator_num_ctrls; i++)
    {
        fxdata->p[i].val.f = 0.0f;
    }

}
