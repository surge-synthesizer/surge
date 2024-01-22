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

#include "ConditionerEffect.h"
#include <vembertech/basic_dsp.h>
#include "SurgeParamConfig.h"

#include "globals.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"
#include "sst/basic-blocks/dsp/Clippers.h"
#include "sst/basic-blocks/dsp/MidSide.h"

namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

using namespace std;

ConditionerEffect::ConditionerEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), band1(storage), band2(storage), hp(storage)
{
    bufpos = 0;

    ampL.set_blocksize(BLOCK_SIZE);
    ampR.set_blocksize(BLOCK_SIZE);
    width.set_blocksize(BLOCK_SIZE);
    postamp.set_blocksize(BLOCK_SIZE);
}

ConditionerEffect::~ConditionerEffect() {}

void ConditionerEffect::init()
{
    setvars(true);
    ef = 0;
    bufpos = 0;
    filtered_lamax = 1.f;
    filtered_lamax2 = 1.f;
    gain = 1.f;
    memset(lamax, 0, sizeof(float) * (lookahead << 1));
    memset(delayed[0], 0, sizeof(float) * lookahead);
    memset(delayed[1], 0, sizeof(float) * lookahead);

    vu[0] = 0.f;
    vu[1] = 0.f;
    vu[2] = 1.f;
    vu[4] = 0.f;
    vu[5] = 0.f;
    assert(KNumVuSlots >= 5);
}

void ConditionerEffect::setvars(bool init)
{
    band1.coeff_peakEQ(band1.calc_omega(-2.5), 2, *pd_float[cond_bass]);
    band2.coeff_peakEQ(band2.calc_omega(4.75), 2, *pd_float[cond_treble]);
    hp.coeff_HP(hp.calc_omega(*pd_float[cond_hpwidth] / 12.0), 0.4);

    if (init)
    {
    }
}

void ConditionerEffect::process_only_control()
{
    float am = 1.0f + 0.9f * *pd_float[cond_attack];
    float rm = 1.0f + 0.9f * *pd_float[cond_release];
    float attack = 0.001f * am * am;
    float release = 0.0001f * rm * rm;

    float a = storage->vu_falloff;
    vu[0] = min(8.f, a * vu[0]);
    vu[1] = min(8.f, a * vu[1]);
    vu[4] = min(8.f, a * vu[4]);
    vu[5] = min(8.f, a * vu[5]);

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        filtered_lamax = (1 - attack) * filtered_lamax + attack;
        filtered_lamax2 = (1 - release) * filtered_lamax2 + (release)*filtered_lamax;
        if (filtered_lamax > filtered_lamax2)
            filtered_lamax2 = filtered_lamax;

        gain = 1.f / filtered_lamax2;
    }

    vu[2] = gain;
}

void ConditionerEffect::process(float *dataL, float *dataR)
{
    float am = 1.0f + 0.9f * *pd_float[cond_attack];
    float rm = 1.0f + 0.9f * *pd_float[cond_release];
    float attack = 0.001f * am * am;
    float release = 0.0001f * rm * rm;

    float a = storage->vu_falloff;
    vu[0] = min(8.f, a * vu[0]);
    vu[1] = min(8.f, a * vu[1]);
    vu[4] = min(8.f, a * vu[4]);
    vu[5] = min(8.f, a * vu[5]);

    setvars(false);

    if (!fxdata->p[cond_bass].deactivated)
    {
        band1.process_block(dataL, dataR);
    }

    if (!fxdata->p[cond_treble].deactivated)
    {
        band2.process_block(dataL, dataR);
    }

    float pregain = storage->db_to_linear(-*pd_float[cond_threshold]);

    ampL.set_target_smoothed(pregain * 0.5f * clamp1bp(1 - *pd_float[cond_balance]));
    ampR.set_target_smoothed(pregain * 0.5f * clamp1bp(1 + *pd_float[cond_balance]));

    width.set_target_smoothed(clamp1bp(*pd_float[cond_width]));
    postamp.set_target_smoothed(storage->db_to_linear(*pd_float[cond_gain]));

    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    sdsp::encodeMS<BLOCK_SIZE>(dataL, dataR, M, S);

    if (!fxdata->p[cond_hpwidth].deactivated)
    {
        hp.process_block(S);
    }

    width.multiply_block(S, BLOCK_SIZE_QUAD);
    sdsp::decodeMS<BLOCK_SIZE>(M, S, dataL, dataR);
    ampL.multiply_block(dataL, BLOCK_SIZE_QUAD);
    ampR.multiply_block(dataR, BLOCK_SIZE_QUAD);

    vu[0] = max(vu[0], mech::blockAbsMax<BLOCK_SIZE>(dataL));
    vu[1] = max(vu[1], mech::blockAbsMax<BLOCK_SIZE>(dataR));

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        float dL = delayed[0][bufpos];
        float dR = delayed[1][bufpos];

        float la = lamax[lookahead - 2];

        la = sqrt(2.f * la); // RMS test

        la = max(1.f, la); // * outscale_inv);
        filtered_lamax = (1 - attack) * filtered_lamax + attack * la;
        filtered_lamax2 = (1 - release) * filtered_lamax2 + (release)*filtered_lamax;
        if (filtered_lamax > filtered_lamax2)
            filtered_lamax2 = filtered_lamax;

        gain = mech::rcp(filtered_lamax2);

        delayed[0][bufpos] = dataL[k];
        delayed[1][bufpos] = dataR[k];

        lamax[bufpos] = max(fabsf(dataL[k]), fabsf(dataR[k]));
        lamax[bufpos] = lamax[bufpos] * lamax[bufpos]; // RMS

        int of = 0;
        for (int i = 0; i < (lookahead_bits); i++)
        {
            int nextof = of + (lookahead >> i);
            lamax[nextof + (bufpos >> (i + 1))] =
                max(lamax[of + (bufpos >> i)], lamax[of + ((bufpos >> i) ^ 0x1)]);
            of = nextof;
        }
        dataL[k] = (gain)*dL;
        dataR[k] = (gain)*dR;

        bufpos = (bufpos + 1) & (lookahead - 1);
    }

    postamp.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);

    vu[2] = gain;

    vu[4] = max(vu[4], mech::blockAbsMax<BLOCK_SIZE>(dataL));
    vu[5] = max(vu[5], mech::blockAbsMax<BLOCK_SIZE>(dataR));
}

Surge::ParamConfig::VUType ConditionerEffect::vu_type(int id)
{
    switch (id)
    {
    case 0:
        return Surge::ParamConfig::vut_vu_stereo;
    case 1:
        return Surge::ParamConfig::vut_gain_reduction;
    case 2:
        return Surge::ParamConfig::vut_vu_stereo;
    }
    return Surge::ParamConfig::vut_off;
}

int ConditionerEffect::vu_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 17;
    case 1:
        return 19;
    case 2:
        return 21;
    }
    return 0;
}

const char *ConditionerEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "EQ";
    case 1:
        return "Stereo";
    case 2:
        return "Limiter";
    case 3:
        return "Output";
    }
    return 0;
}

int ConditionerEffect::group_label_ypos(int id)
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
        return 29;
    }
    return 0;
}

void ConditionerEffect::suspend() { init(); }

void ConditionerEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[cond_bass].set_name("Bass");
    fxdata->p[cond_bass].set_type(ct_decibel_extra_narrow_deactivatable);
    fxdata->p[cond_treble].set_name("Treble");
    fxdata->p[cond_treble].set_type(ct_decibel_extra_narrow_deactivatable);

    fxdata->p[cond_width].set_name("Width");
    fxdata->p[cond_width].set_type(ct_percent_bipolar);
    fxdata->p[cond_hpwidth].set_name("Side Low Cut");
    fxdata->p[cond_hpwidth].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[cond_balance].set_name("Balance");
    fxdata->p[cond_balance].set_type(ct_percent_bipolar);

    fxdata->p[cond_threshold].set_name("Threshold");
    fxdata->p[cond_threshold].set_type(ct_decibel_attenuation);
    fxdata->p[cond_attack].set_name("Attack Rate");
    fxdata->p[cond_attack].set_type(ct_percent_bipolar);
    fxdata->p[cond_release].set_name("Release Rate");
    fxdata->p[cond_release].set_type(ct_percent_bipolar);
    fxdata->p[cond_gain].set_name("Gain");
    fxdata->p[cond_gain].set_type(ct_decibel_attenuation);

    fxdata->p[cond_bass].posy_offset = 1;
    fxdata->p[cond_treble].posy_offset = 1;

    fxdata->p[cond_width].posy_offset = 3;
    fxdata->p[cond_hpwidth].posy_offset = -7;
    fxdata->p[cond_balance].posy_offset = 5;

    fxdata->p[cond_threshold].posy_offset = 13;
    fxdata->p[cond_attack].posy_offset = 13;
    fxdata->p[cond_release].posy_offset = 13;
    fxdata->p[cond_gain].posy_offset = 15;
}

void ConditionerEffect::init_default_values()
{
    fxdata->p[cond_bass].deactivated = false;
    fxdata->p[cond_treble].deactivated = false;
    fxdata->p[cond_hpwidth].val.f = -60;
    fxdata->p[cond_hpwidth].deactivated = true;
}

void ConditionerEffect::handleStreamingMismatches(int streamingRevision,
                                                  int currentSynthStreamingRevision)
{
    if (streamingRevision <= 15)
    {
        fxdata->p[cond_bass].deactivated = false;
        fxdata->p[cond_treble].deactivated = false;
    }

    if (streamingRevision <= 16)
    {
        fxdata->p[cond_hpwidth].val.f = -60;
        fxdata->p[cond_hpwidth].deactivated = true;
    }
}
