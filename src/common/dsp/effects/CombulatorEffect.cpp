/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
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
    : Effect(storage, fxdata, pd), halfbandIN(6, true), halfbandOUT(6, true), lp(storage),
      hp(storage)
{
    lp.setBlockSize(BLOCK_SIZE);
    hp.setBlockSize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);

    qfus = new sst::filters::QuadFilterUnitState[2]();

    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            for (int q = 0; q < sst::filters::n_filter_registers; ++q)
            {
                Reg[e][c][q] = 0;
            }
            WP[e][c] = 0;
        }
    }

    memset(filterDelay, 0, 3 * 2 * (MAX_FB_COMB_EXTENDED + FIRipol_N) * sizeof(float));

    // http://www.cs.cmu.edu/~music/icm-online/readings/panlaws/
    for (int i = 0; i < PANLAW_SIZE; ++i)
    {
        auto piby2 = M_PI / 2.0;
        double panAngle = 1.0 * i / (PANLAW_SIZE - 1) * piby2;
        panL[i] = sqrt((piby2 - panAngle) / piby2 * cos(panAngle));
        panR[i] = sqrt(panAngle * sin(panAngle) / piby2);
    }

    // A very simple envelope follower
    envA = pow(0.01, 1.0 / (5 * storage->dsamplerate_os * 0.001));
    envR = pow(0.01, 1.0 / (5 * storage->dsamplerate_os * 0.001));
    envV[0] = 0.f;
    envV[1] = 0.f;

    noiseGen[0][0] = 0.f;
    noiseGen[1][0] = 0.f;
    noiseGen[0][1] = 0.f;
    noiseGen[1][1] = 0.f;
}

CombulatorEffect::~CombulatorEffect() { delete[] qfus; }

void CombulatorEffect::init()
{
    sampleRateReset();
    setvars(true);
    bi = 0;
    lp.suspend();

    memset(filterDelay, 0, 3 * 2 * (MAX_FB_COMB_EXTENDED + FIRipol_N) * sizeof(float));
    envV[0] = 0.f;
    envV[1] = 0.f;

    noiseGen[0][0] = 0.f;
    noiseGen[1][0] = 0.f;
    noiseGen[0][1] = 0.f;
    noiseGen[1][1] = 0.f;
}

void CombulatorEffect::setvars(bool init)
{
    for (int i = 0; i < 3; ++i)
    {
        if (i == 0)
        {
            freq[i].newValue(*f[combulator_freq1]);
        }
        else
        {
            freq[i].newValue(*f[combulator_freq1] + *f[combulator_freq1 + i]);
        }

        gain[i].newValue(amp_to_linear(limit_range(*f[combulator_gain1 + i], 0.f, 2.f)));
    }

    noisemix.newValue(clamp01(*f[combulator_noise_mix]));
    feedback.newValue(*f[combulator_feedback]);
    tone.newValue(clamp1bp(*f[combulator_tone]));
    pan2.newValue(clamp1bp(*f[combulator_pan2]));
    pan3.newValue(clamp1bp(*f[combulator_pan3]));

    negone.set_target(-1.f);

    if (init)
    {
        for (int i = 0; i < 3; ++i)
        {
            freq[i].instantize();
            gain[i].instantize();
        }

        feedback.instantize();

        pan2.instantize();
        pan3.instantize();
        tone.instantize();
        noisemix.instantize();

        mix.set_target(1.f);
        mix.instantize();

        negone.set_target(-1.f);
        negone.instantize();

        lp.coeff_instantize();
        hp.coeff_instantize();

        halfbandOUT.reset();
        halfbandIN.reset();
    }
    else
    {
        // lowpass range is from MIDI note 136 down to 57 (~21.1 kHz down to 220 Hz)
        // highpass range is from MIDI note 34 to 136(~61 Hz to ~21.1 kHz)
        float clo = -12, cmid = 67, chi = -33;
        float hpCutoff = chi;
        float lpCutoff = cmid;

        if (tone.v > 0)
        {
            // OK so cool scale the hp cutoff
            auto tv = tone.v;
            hpCutoff = tv * (cmid - chi) + chi;
        }
        else
        {
            auto tv = -tone.v;
            lpCutoff = tv * (clo - cmid) + cmid;
        }

        lp.coeff_LP(lp.calc_omega((lpCutoff / 12.0) - 2.f), 0.707);
        hp.coeff_HP(hp.calc_omega((hpCutoff / 12.0) - 2.f), 0.707);
    }
}

inline void set1f(__m128 &m, int i, float f) { *((float *)&m + i) = f; }

inline float get1f(__m128 m, int i) { return *((float *)&m + i); }

void CombulatorEffect::sampleRateReset()
{
    for (int e = 0; e < 3; ++e)
        for (int c = 0; c < 2; ++c)
            coeff[e][c].setSampleRateAndBlockSize((float)storage->dsamplerate_os, BLOCK_SIZE_OS);
}

void CombulatorEffect::process(float *dataL, float *dataR)
{
    setvars(false);

    // Upsample the input
    float dataOS alignas(16)[2][BLOCK_SIZE_OS];
    halfbandIN.process_block_U2(dataL, dataR, dataOS[0], dataOS[1]);

    /*
     * Select the coefficients. Here you have to base yourself on the mode switch and
     * call CoeffMaker with one fo the known models and subtypes, along with the feedback
     * and the frequency of the particular band.
     */
    using namespace sst::filters;
    int type = fut_comb_pos, subtype = 1;

    auto filtptr =
        GetQFPtrFilterUnit(static_cast<FilterType>(type),
                           static_cast<FilterSubType>(subtype | QFUSubtypeMasks::EXTENDED_COMB));

    auto fbscaled = (feedback.v < 0.f ? -1.f : 1.f) * sqrt(abs(feedback.v));

    /*
     * So now set up across the voices (e for 'entry' to match SurgeVoice) and the channels (c)
     */
    bool useTuning = fxdata->p[combulator_freq1].extend_range;
    for (int e = 0; e < 3; ++e)
    {
        for (int c = 0; c < 2; ++c)
        {
            coeff[e][c].MakeCoeffs(
                freq[e].v, fbscaled, static_cast<FilterType>(type),
                static_cast<FilterSubType>(subtype | QFUSubtypeMasks::EXTENDED_COMB), storage,
                useTuning);

            coeff[e][c].updateState(qfus[c], e);

            for (int i = 0; i < n_filter_registers; i++)
            {
                set1f(qfus[c].R[i], e, Reg[e][c][i]);
            }

            qfus[c].DB[e] = filterDelay[e][c];
            qfus[c].WP[e] = WP[e][c];

            qfus[c].active[e] = 0xFFFFFFFF;
            qfus[c].active[3] = 0;
        }
    }

    /* Run the filters */
    float noise[2];

    for (int s = 0; s < BLOCK_SIZE_OS; ++s)
    {
        // Envelope Follower Update
        for (int c = 0; c < 2; ++c)
        {
            auto v = dataOS[c][s];
            auto e = envV[c];
            if (v > e)
                e = envA * (e - v) + v;
            else
                e = envR * (e - v) + v;
            envV[c] = e;
            noise[c] =
                noisemix.v * 3.f * envV[c] *
                correlated_noise_o2mk2_storagerng(noiseGen[c][0], noiseGen[c][1], 0, storage);
        }

        auto l128 = _mm_setzero_ps();
        auto r128 = _mm_setzero_ps();

        // FIXME - we want to interpolate the non-integral part if we like this
        int panIndex2 = (int)((limit_range(pan2.v, -1.f, 1.f) + 1) * ((PANLAW_SIZE - 1) / 2)) &
                        (PANLAW_SIZE - 1);
        int panIndex3 = (int)((limit_range(pan3.v, -1.f, 1.f) + 1) * ((PANLAW_SIZE - 1) / 2)) &
                        (PANLAW_SIZE - 1);

        if (filtptr)
        {
            l128 = filtptr(&(qfus[0]), _mm_set1_ps(dataOS[0][s] + noise[0]));
            r128 = filtptr(&(qfus[1]), _mm_set1_ps(dataOS[1][s] + noise[1]));
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
            float panl = 0.59, panr = 0.59;

            if (i == 1)
            {
                panl = panL[panIndex2];
                panr = panR[panIndex2];
            }
            else if (i == 2)
            {
                // The other way!
                panl = panL[panIndex3];
                panr = panR[panIndex3];
            }

            mixl += tl[i] * gain[i].v * panl / 0.59;
            mixr += tr[i] * gain[i].v * panr / 0.59;
        }

        // soft-clip output for good measure
        mixl = storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft, mixl);
        mixr = storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft, mixr);

        dataOS[0][s] = mixl;
        dataOS[1][s] = mixr;

        // lag class only works at BLOCK_SIZE time, not BLOCK_SIZE_OS, so call process every
        // other sample
        if (s % 2 == 0)
        {
            for (auto i = 0; i < 3; ++i)
            {
                freq[i].process();
                gain[i].process();

                if (i == 0)
                {
                    feedback.process();
                    pan2.process();
                    pan3.process();
                    tone.process();
                    noisemix.process();
                }
            }
        }
    }

    /* preserve those registers and stuff */
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

    if (!fxdata->p[combulator_tone].deactivated)
    {
        lp.process_block(L, R);
        hp.process_block(L, R);
    }

    auto cm = clamp01(*f[combulator_mix]);

    mix.set_target_smoothed(cm);
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
        return 5;
    case 2:
        return 17;
    case 3:
        return 25;
    }
    return 0;
}

void CombulatorEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[combulator_noise_mix].set_name("Extra Noise");
    fxdata->p[combulator_noise_mix].set_type(ct_percent);
    fxdata->p[combulator_noise_mix].posy_offset = 1;

    fxdata->p[combulator_freq1].set_name("Center");
    fxdata->p[combulator_freq1].set_type(ct_freq_audible_with_very_low_lowerbound);
    fxdata->p[combulator_freq1].posy_offset = 3;
    fxdata->p[combulator_freq2].set_name("Offset 1");
    fxdata->p[combulator_freq2].set_type(ct_pitch);
    fxdata->p[combulator_freq2].posy_offset = 3;
    fxdata->p[combulator_freq3].set_name("Offset 2");
    fxdata->p[combulator_freq3].set_type(ct_pitch);
    fxdata->p[combulator_freq3].posy_offset = 3;
    fxdata->p[combulator_feedback].set_name("Feedback");
    fxdata->p[combulator_feedback].set_type(ct_percent_bipolar);
    fxdata->p[combulator_feedback].posy_offset = 3;
    fxdata->p[combulator_tone].set_name("Tone");
    fxdata->p[combulator_tone].set_type(ct_percent_bipolar_deactivatable);
    fxdata->p[combulator_tone].posy_offset = 3;

    fxdata->p[combulator_gain1].set_name("Comb 1");
    fxdata->p[combulator_gain1].set_type(ct_amplitude);
    fxdata->p[combulator_gain1].posy_offset = 5;
    fxdata->p[combulator_gain2].set_name("Comb 2");
    fxdata->p[combulator_gain2].set_type(ct_amplitude);
    fxdata->p[combulator_gain2].posy_offset = 5;
    fxdata->p[combulator_gain3].set_name("Comb 3");
    fxdata->p[combulator_gain3].set_type(ct_amplitude);
    fxdata->p[combulator_gain3].posy_offset = 5;

    fxdata->p[combulator_pan2].set_name("Pan 2");
    fxdata->p[combulator_pan2].set_type(ct_percent_bipolar_pan);
    fxdata->p[combulator_pan2].posy_offset = 7;
    fxdata->p[combulator_pan3].set_name("Pan 3");
    fxdata->p[combulator_pan3].set_type(ct_percent_bipolar_pan);
    fxdata->p[combulator_pan3].posy_offset = 7;
    fxdata->p[combulator_mix].set_name("Mix");
    fxdata->p[combulator_mix].set_type(ct_percent);
    fxdata->p[combulator_mix].posy_offset = 7;
}

void CombulatorEffect::init_default_values()
{
    fxdata->p[combulator_noise_mix].val.f = 0.0f;

    fxdata->p[combulator_freq1].val.f = -9.0f;
    fxdata->p[combulator_freq2].val.f = 0.25f;
    fxdata->p[combulator_freq3].val.f = -0.25f;
    fxdata->p[combulator_feedback].val.f = 0.75f;
    fxdata->p[combulator_tone].val.f = fxdata->p[combulator_tone].val_max.f;

    fxdata->p[combulator_gain1].val.f = 0.7937005f; // -6 dB on the dot
    fxdata->p[combulator_gain2].val.f = 0.7937005f;
    fxdata->p[combulator_gain3].val.f = 0.7937005f;

    fxdata->p[combulator_pan2].val.f = 0.25f;
    fxdata->p[combulator_pan3].val.f = -0.25f;
    fxdata->p[combulator_tone].val.f = 0;
    fxdata->p[combulator_tone].deactivated = false;
    fxdata->p[combulator_mix].val.f = 1.0f;
}

void CombulatorEffect::handleStreamingMismatches(int streamingRevision,
                                                 int currentSynthStreamingRevision)
{
    if (streamingRevision <= 17)
    {
        fxdata->p[combulator_tone].deactivated = false;
    }
}