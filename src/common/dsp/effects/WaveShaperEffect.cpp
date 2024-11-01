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

#include "WaveShaperEffect.h"
#include "DebugHelpers.h"
#include "sst/basic-blocks/dsp/FastMath.h"

#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"
namespace mech = sst::basic_blocks::mechanics;

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

        hpPre.coeff_HP(hpPre.calc_omega(*pd_float[ws_prelowcut] / 12.0), 0.707);
        hpPre.coeff_instantize();

        lpPre.coeff_LP2B(lpPre.calc_omega(*pd_float[ws_prehighcut] / 12.0), 0.707);
        lpPre.coeff_instantize();

        hpPost.coeff_HP(hpPre.calc_omega(*pd_float[ws_postlowcut] / 12.0), 0.707);
        hpPost.coeff_instantize();

        lpPost.coeff_LP2B(lpPost.calc_omega(*pd_float[ws_posthighcut] / 12.0), 0.707);
        lpPost.coeff_instantize();

        mix.instantize();
        boost.instantize();

        drive.newValue(db_to_amp(*pd_float[ws_drive]));
        bias.newValue(clamp1bp(*pd_float[ws_bias]));

        bias.instantize();
        drive.instantize();
    }
}

#define OVERSAMPLE 1

void WaveShaperEffect::process(float *dataL, float *dataR)
{
    mix.set_target_smoothed(clamp01(*pd_float[ws_mix]));
    boost.set_target_smoothed(db_to_amp(*pd_float[ws_postboost]));

    /*
     * OK so what's all this? Well the network of halfbands and so on
     * attenuates the input before you get to the FX which, for waveshapers,
     * matters a lot. We wanted to meet the constraint that
     *
     * sin -> filterbank waveshaper -> output
     * sin -> fxbank waveshaper with no pre/post and no bias -> output
     *
     * produced the same response at the same drives. To do that we need
     * to compensate for the scale factor difference and also for the
     * halfband filter dropping our amplitude. Moreover, we need to do
     * that in the db calculation also, which we replicate here to change
     * the clip limits.
     */
    const auto scalef = 3.f, oscalef = 1.f / 3.f, hbfComp = 2.f;

    auto x = scalef * *pd_float[ws_drive];
    auto dnv = limit_range(powf(2.f, x / 18.f), 0.f, 8.f);
    drive.newValue(dnv);
    bias.newValue(clamp1bp(*pd_float[ws_bias]));

    float wetL alignas(16)[BLOCK_SIZE], wetR alignas(16)[BLOCK_SIZE];
    mech::copy_from_to<BLOCK_SIZE>(dataL, wetL);
    mech::copy_from_to<BLOCK_SIZE>(dataR, wetR);

    // Apply the filters
    hpPre.coeff_HP(hpPre.calc_omega(*pd_float[ws_prelowcut] / 12.0), 0.707);
    lpPre.coeff_LP2B(lpPre.calc_omega(*pd_float[ws_prehighcut] / 12.0), 0.707);

    if (!fxdata->p[ws_prehighcut].deactivated)
    {
        lpPre.process_block(wetL, wetR);
    }

    if (!fxdata->p[ws_prelowcut].deactivated)
    {
        hpPre.process_block(wetL, wetR);
    }

    const auto newShape = static_cast<sst::waveshapers::WaveshaperType>(*pd_int[ws_shaper]);
    if (newShape != lastShape)
    {
        lastShape = newShape;

        float R[sst::waveshapers::n_waveshaper_registers];
        initializeWaveshaperRegister(lastShape, R);

        for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
        {
            wss.R[i] = SIMD_MM(set1_ps)(R[i]);
        }

        wss.init = SIMD_MM(cmpneq_ps)(SIMD_MM(setzero_ps)(), SIMD_MM(setzero_ps)());
    }

    auto wsptr = sst::waveshapers::GetQuadWaveshaper(lastShape);

    // Now upsample
    float dataOS alignas(16)[2][BLOCK_SIZE_OS];
    halfbandIN.process_block_U2(wetL, wetR, dataOS[0], dataOS[1], BLOCK_SIZE_OS);

    if (wsptr)
    {
        float din alignas(16)[4] = {0, 0, 0, 0};

        for (int i = 0; i < BLOCK_SIZE_OS; ++i)
        {
            din[0] = hbfComp * scalef * dataOS[0][i] + bias.v;
            din[1] = hbfComp * scalef * dataOS[1][i] + bias.v;

            auto dat = SIMD_MM(load_ps)(din);
            auto drv = SIMD_MM(set1_ps)(drive.v);

            dat = wsptr(&wss, dat, drv);

            float res alignas(16)[4];

            SIMD_MM(store_ps)(res, dat);

            dataOS[0][i] = res[0] * oscalef;
            dataOS[1][i] = res[1] * oscalef;

            bias.process();
            drive.process();
        }
    }

    halfbandOUT.process_block_D2(dataOS[0], dataOS[1], BLOCK_SIZE_OS);

    mech::copy_from_to<BLOCK_SIZE>(dataOS[0], wetL);
    mech::copy_from_to<BLOCK_SIZE>(dataOS[1], wetR);

    // Apply the filters
    hpPost.coeff_HP(hpPre.calc_omega(*pd_float[ws_postlowcut] / 12.0), 0.707);
    lpPost.coeff_LP2B(lpPre.calc_omega(*pd_float[ws_posthighcut] / 12.0), 0.707);

    if (!fxdata->p[ws_posthighcut].deactivated)
    {
        lpPost.process_block(wetL, wetR);
    }

    if (!fxdata->p[ws_postlowcut].deactivated)
    {
        hpPost.process_block(wetL, wetR);
    }

    boost.multiply_2_blocks(wetL, wetR, BLOCK_SIZE_QUAD);
    mix.fade_2_blocks_inplace(dataL, wetL, dataR, wetR, BLOCK_SIZE_QUAD);
}

void WaveShaperEffect::suspend() { init(); }

const char *WaveShaperEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Pre-Shaper";
    case 1:
        return "Shaper";
    case 2:
        return "Post-Shaper";
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
