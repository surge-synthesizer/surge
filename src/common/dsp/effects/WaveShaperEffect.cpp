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

#include "WaveShaperEffect.h"
#include "DebugHelpers.h"
#include "FastMath.h"

// http://recherche.ircam.fr/pub/dafx11/Papers/66_e.pdf

WaveShaperEffect::WaveShaperEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), halfbandIN(6, true), halfbandOUT(6, true), lpPre(storage),
      hpPre(storage), lpPost(storage), hpPost(storage)
{
    mix.set_blocksize(BLOCK_SIZE);
    boost.set_blocksize(BLOCK_SIZE);
}

WaveShaperEffect::~WaveShaperEffect() {}

void WaveShaperEffect::init() { setvars(true); }

void WaveShaperEffect::setvars(bool init)
{
    if (init)
    {
        halfbandOUT.reset();
        halfbandIN.reset();

        lpPre.suspend();
        hpPre.suspend();
        lpPost.suspend();
        hpPost.suspend();

        hpPre.coeff_HP(hpPre.calc_omega(*f[ws_prelowcut] / 12.0), 0.707);
        hpPre.coeff_instantize();

        lpPre.coeff_LP2B(lpPre.calc_omega(*f[ws_prehighcut] / 12.0), 0.707);
        lpPre.coeff_instantize();

        hpPost.coeff_HP(hpPre.calc_omega(*f[ws_postlowcut] / 12.0), 0.707);
        hpPost.coeff_instantize();

        lpPost.coeff_LP2B(lpPost.calc_omega(*f[ws_posthighcut] / 12.0), 0.707);
        lpPost.coeff_instantize();

        mix.instantize();
        boost.instantize();

        drive.newValue(db_to_amp(*f[ws_drive]));
        bias.newValue(clamp1bp(*f[ws_bias]));

        bias.instantize();
        drive.instantize();
    }
}

#define OVERSAMPLE 1

void WaveShaperEffect::process(float *dataL, float *dataR)
{
    mix.set_target_smoothed(clamp01(*f[ws_mix]));
    boost.set_target_smoothed(db_to_amp(*f[ws_postboost]));

    drive.newValue(db_to_amp(*f[ws_drive]));
    bias.newValue(clamp1bp(*f[ws_bias]));

    float wetL alignas(16)[BLOCK_SIZE], wetR alignas(16)[BLOCK_SIZE];
    copy_block(dataL, wetL, BLOCK_SIZE_QUAD);
    copy_block(dataR, wetR, BLOCK_SIZE_QUAD);

    // Apply the filters
    hpPre.coeff_HP(hpPre.calc_omega(*f[ws_prelowcut] / 12.0), 0.707);
    lpPre.coeff_LP2B(lpPre.calc_omega(*f[ws_prehighcut] / 12.0), 0.707);

    if (!fxdata->p[ws_prehighcut].deactivated)
    {
        lpPre.process_block(wetL, wetR);
    }

    if (!fxdata->p[ws_prelowcut].deactivated)
    {
        hpPre.process_block(wetL, wetR);
    }

    const auto newShape = static_cast<sst::waveshapers::WaveshaperType>(*pdata_ival[ws_shaper]);
    if (newShape != lastShape)
    {
        lastShape = newShape;

        float R[sst::waveshapers::n_waveshaper_registers];
        initializeWaveshaperRegister(lastShape, R);

        for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
        {
            wss.R[i] = _mm_set1_ps(R[i]);
        }

        wss.init = _mm_cmpneq_ps(_mm_setzero_ps(), _mm_setzero_ps());
    }

    auto wsptr = sst::waveshapers::GetQuadWaveshaper(lastShape);

    // Now upsample
    float dataOS alignas(16)[2][BLOCK_SIZE_OS];
    halfbandIN.process_block_U2(wetL, wetR, dataOS[0], dataOS[1]);

    if (wsptr)
    {
        float din alignas(16)[4] = {0, 0, 0, 0};

        for (int i = 0; i < BLOCK_SIZE_OS; ++i)
        {
            din[0] = dataOS[0][i] + bias.v;
            din[1] = dataOS[1][i] + bias.v;

            auto dat = _mm_load_ps(din);
            auto drv = _mm_set1_ps(drive.v);

            dat = wsptr(&wss, dat, drv);

            float res alignas(16)[4];

            _mm_store_ps(res, dat);

            dataOS[0][i] = res[0];
            dataOS[1][i] = res[1];

            bias.process();
            drive.process();
        }
    }

    halfbandOUT.process_block_D2(dataOS[0], dataOS[1]);

    copy_block(dataOS[0], wetL, BLOCK_SIZE_QUAD);
    copy_block(dataOS[1], wetR, BLOCK_SIZE_QUAD);

    // Apply the filters
    hpPost.coeff_HP(hpPre.calc_omega(*f[ws_postlowcut] / 12.0), 0.707);
    lpPost.coeff_LP2B(lpPre.calc_omega(*f[ws_posthighcut] / 12.0), 0.707);

    if (!fxdata->p[ws_posthighcut].deactivated)
    {
        lpPost.process_block(wetL, wetR);
    }

    if (!fxdata->p[ws_postlowcut].deactivated)
    {
        hpPost.process_block(wetL, wetR);
    }

    boost.multiply_2_blocks(wetL, wetR, BLOCK_SIZE_QUAD);
    mix.fade_2_blocks_to(dataL, wetL, dataR, wetR, dataL, dataR, BLOCK_SIZE_QUAD);
}

void WaveShaperEffect::suspend() { init(); }

const char *WaveShaperEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Pre-Filter";
    case 1:
        return "Shape";
    case 2:
        return "Post-Filter";
    case 3:
        return "Output";
    }
    return 0;
}

int WaveShaperEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 7;
    case 2:
        return 15;
    case 3:
        return 21;
    }
    return 0;
}

void WaveShaperEffect::init_ctrltypes()
{
    static WaveShaperSelectorMapper mapper;
    Effect::init_ctrltypes();

    fxdata->p[ws_prelowcut].set_name("Low Cut");
    fxdata->p[ws_prelowcut].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[ws_prehighcut].set_name("High Cut");
    fxdata->p[ws_prehighcut].set_type(ct_freq_audible_deactivatable_lp);

    fxdata->p[ws_shaper].set_name("Shape");
    fxdata->p[ws_shaper].set_type(ct_wstype);
    fxdata->p[ws_shaper].set_user_data(&mapper);

    fxdata->p[ws_bias].set_name("Bias");
    fxdata->p[ws_bias].set_type(ct_percent_bipolar);
    fxdata->p[ws_drive].set_name("Drive");
    fxdata->p[ws_drive].set_type(ct_decibel_narrow_short_extendable);

    fxdata->p[ws_postlowcut].set_name("Low Cut");
    fxdata->p[ws_postlowcut].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[ws_posthighcut].set_name("High Cut");
    fxdata->p[ws_posthighcut].set_type(ct_freq_audible_deactivatable_lp);

    fxdata->p[ws_postboost].set_name("Gain");
    fxdata->p[ws_postboost].set_type(ct_decibel_narrow_short_extendable);
    fxdata->p[ws_mix].set_name("Mix");
    fxdata->p[ws_mix].set_type(ct_percent);

    for (int i = 0; i <= ws_mix; ++i)
    {
        auto a = 1;
        if (i >= ws_shaper)
            a += 2;
        if (i >= ws_postlowcut)
            a += 2;
        if (i >= ws_postboost)
            a += 2;
        fxdata->p[i].posy_offset = a;
    }
}

void WaveShaperEffect::init_default_values()
{
    fxdata->p[ws_prelowcut].val.f = fxdata->p[ws_prelowcut].val_min.f;
    fxdata->p[ws_prelowcut].deactivated = true;
    fxdata->p[ws_prehighcut].val.f = fxdata->p[ws_prehighcut].val_max.f;
    fxdata->p[ws_prehighcut].deactivated = true;

    fxdata->p[ws_shaper].val.i = 0;
    fxdata->p[ws_bias].val.f = 0;
    fxdata->p[ws_drive].val.f = fxdata->p[ws_drive].val_default.f;

    fxdata->p[ws_postlowcut].val.f = fxdata->p[ws_postlowcut].val_min.f;
    fxdata->p[ws_postlowcut].deactivated = false;
    fxdata->p[ws_posthighcut].val.f = fxdata->p[ws_posthighcut].val_max.f;
    fxdata->p[ws_posthighcut].deactivated = false;

    fxdata->p[ws_postboost].val.f = fxdata->p[ws_drive].val_default.f;
    fxdata->p[ws_mix].val.f = 1.f;
}
