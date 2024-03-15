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

#include "TapeEffect.h"
#include <vembertech/basic_dsp.h>

#include "sst/basic-blocks/mechanics/block-ops.h"
namespace mech = sst::basic_blocks::mechanics;

namespace chowdsp
{

TapeEffect::TapeEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), lossFilter(storage, 48)
{
    mix.set_blocksize(BLOCK_SIZE);
    makeup.set_blocksize(BLOCK_SIZE);
}

TapeEffect::~TapeEffect() {}

void TapeEffect::init()
{
    TapeEffect::sampleRateReset();

    mech::clear_block<BLOCK_SIZE>(L);
    mech::clear_block<BLOCK_SIZE>(R);

    mix.set_target(1.f);
    mix.instantize();

    makeup.set_target(std::pow(10.0f, 9.0f / 20.0f));
    makeup.instantize();
}

void TapeEffect::sampleRateReset()
{
    hysteresis.reset(storage->samplerate);
    toneControl.prepare(storage->samplerate);
    lossFilter.prepare(storage->samplerate, BLOCK_SIZE);
    degrade.prepareToPlay((float)storage->samplerate, BLOCK_SIZE);
    chew.prepare((float)storage->samplerate, BLOCK_SIZE);
}

void TapeEffect::process(float *dataL, float *dataR)
{
    mech::copy_from_to<BLOCK_SIZE>(dataL, L);
    mech::copy_from_to<BLOCK_SIZE>(dataR, R);

    if (!fxdata->p[tape_drive].deactivated)
    {
        auto thd = clamp01(*pd_float[tape_drive]);
        auto ths = clamp01(*pd_float[tape_saturation]);
        auto thb = clamp01(*pd_float[tape_bias]);
        auto tht = clamp1bp(*pd_float[tape_tone]);
        const auto hysteresisMode = fxdata->p[tape_drive].deform_type;

        hysteresis.set_params(thd, ths, thb);
        hysteresis.set_solver(hysteresisMode);
        toneControl.set_params(tht);

        toneControl.processBlockIn(L, R);
        hysteresis.process_block(L, R);

        makeup.multiply_2_blocks(L, R, BLOCK_SIZE_QUAD);
    }

    if (!fxdata->p[tape_speed].deactivated)
    {
        auto tls = limit_range(*pd_float[tape_speed], 1.0f, 30.0f);
        auto tlsp = limit_range(*pd_float[tape_spacing], 0.1f, 20.0f);
        auto tlg = limit_range(*pd_float[tape_gap], 1.0f, 50.0f);
        auto tlt = limit_range(*pd_float[tape_thickness], 0.1f, 50.0f);

        lossFilter.set_params(tls, tlsp, tlg, tlt);
        lossFilter.process(L, R);
    }

    if (!fxdata->p[tape_degrade_depth].deactivated)
    {
        auto tdd = clamp01(*pd_float[tape_degrade_depth]);
        auto tda = clamp01(*pd_float[tape_degrade_amount]);
        auto tdv = clamp01(*pd_float[tape_degrade_variance]);
        auto chew_freq = 0.9f - (tda * 0.8f);
        auto chew_depth = tdd * 0.15f;

        chew.set_params(chew_freq, chew_depth, tdv);
        chew.process_block(L, R);

        degrade.set_params(tdd, tda, tdv);
        degrade.process_block(L, R);
    }

    mix.set_target_smoothed(clamp01(*pd_float[tape_mix]));
    mix.fade_2_blocks_inplace(dataL, L, dataR, R, BLOCK_SIZE_QUAD);
}

void TapeEffect::suspend() {}

const char *TapeEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Hysteresis";
    case 1:
        return "Loss";
    case 2:
        return "Degrade";
    case 3:
        return "Output";
    }

    return 0;
}

int TapeEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 11;
    case 2:
        return 21;
    case 3:
        return 29;
    }

    return 0;
}

void TapeEffect::init_ctrltypes()
{
    /*
     * The actually deactivation status is on gain, so reflect that down
     * to freq and bw using the dynamic deactivation mechanism
     */
    static struct TapeEffectDeact : public ParameterDynamicDeactivationFunction
    {
        bool getValue(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            switch (idx)
            {
            case tape_saturation:
            case tape_bias:
            case tape_tone:
                return fx->p[tape_drive].deactivated;
            case tape_gap:
            case tape_spacing:
            case tape_thickness:
                return fx->p[tape_speed].deactivated;
            case tape_degrade_amount:
            case tape_degrade_variance:
                return fx->p[tape_degrade_depth].deactivated;
            }

            return false;
        }
        Parameter *getPrimaryDeactivationDriver(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            switch (idx)
            {
            case tape_saturation:
            case tape_bias:
            case tape_tone:
                return &(fx->p[tape_drive]);
            case tape_gap:
            case tape_spacing:
            case tape_thickness:
                return &(fx->p[tape_speed]);
            case tape_degrade_amount:
            case tape_degrade_variance:
                return &(fx->p[tape_degrade_depth]);
            }
            return nullptr;
        }
    } tapeGroupDeact;

    Effect::init_ctrltypes();

    fxdata->p[tape_drive].set_name("Drive");
    fxdata->p[tape_drive].set_type(ct_tape_drive);
    fxdata->p[tape_drive].posy_offset = 1;
    fxdata->p[tape_drive].val_default.f = 0.85f;
    fxdata->p[tape_saturation].set_name("Saturation");
    fxdata->p[tape_saturation].set_type(ct_percent);
    fxdata->p[tape_saturation].posy_offset = 1;
    fxdata->p[tape_saturation].val_default.f = 0.5f;
    fxdata->p[tape_saturation].dynamicDeactivation = &tapeGroupDeact;
    fxdata->p[tape_bias].set_name("Bias");
    fxdata->p[tape_bias].set_type(ct_percent);
    fxdata->p[tape_bias].posy_offset = 1;
    fxdata->p[tape_bias].val_default.f = 0.5f;
    fxdata->p[tape_bias].dynamicDeactivation = &tapeGroupDeact;
    fxdata->p[tape_tone].set_name("Tone");
    fxdata->p[tape_tone].set_type(ct_percent_bipolar);
    fxdata->p[tape_tone].posy_offset = 1;
    fxdata->p[tape_tone].val_default.f = 0.0f;
    fxdata->p[tape_tone].dynamicDeactivation = &tapeGroupDeact;

    fxdata->p[tape_speed].set_name("Speed");
    fxdata->p[tape_speed].set_type(ct_tape_speed);
    fxdata->p[tape_speed].posy_offset = 3;
    fxdata->p[tape_gap].set_name("Gap");
    fxdata->p[tape_gap].set_type(ct_tape_microns);
    fxdata->p[tape_gap].posy_offset = 3;
    fxdata->p[tape_gap].val_min.f = 1.0f;
    fxdata->p[tape_gap].val_max.f = 50.0f;
    fxdata->p[tape_gap].val_default.f = 10.0f;
    fxdata->p[tape_gap].dynamicDeactivation = &tapeGroupDeact;
    fxdata->p[tape_spacing].set_name("Spacing");
    fxdata->p[tape_spacing].set_type(ct_tape_microns);
    fxdata->p[tape_spacing].posy_offset = 3;
    fxdata->p[tape_spacing].val_min.f = 0.1f;
    fxdata->p[tape_spacing].val_max.f = 20.0f;
    fxdata->p[tape_spacing].val_default.f = 0.1f;
    fxdata->p[tape_spacing].dynamicDeactivation = &tapeGroupDeact;
    fxdata->p[tape_thickness].set_name("Thickness");
    fxdata->p[tape_thickness].set_type(ct_tape_microns);
    fxdata->p[tape_thickness].posy_offset = 3;
    fxdata->p[tape_thickness].val_min.f = 0.1f;
    fxdata->p[tape_thickness].val_max.f = 50.0f;
    fxdata->p[tape_thickness].val_default.f = 0.1f;
    fxdata->p[tape_thickness].dynamicDeactivation = &tapeGroupDeact;

    fxdata->p[tape_degrade_depth].set_name("Depth");
    fxdata->p[tape_degrade_depth].set_type(ct_percent_deactivatable);
    fxdata->p[tape_degrade_depth].posy_offset = 5;
    fxdata->p[tape_degrade_depth].val_default.f = 0.0f;
    fxdata->p[tape_degrade_amount].set_name("Amount");
    fxdata->p[tape_degrade_amount].set_type(ct_percent);
    fxdata->p[tape_degrade_amount].posy_offset = 5;
    fxdata->p[tape_degrade_amount].val_default.f = 0.0f;
    fxdata->p[tape_degrade_amount].dynamicDeactivation = &tapeGroupDeact;
    fxdata->p[tape_degrade_variance].set_name("Variance");
    fxdata->p[tape_degrade_variance].set_type(ct_percent);
    fxdata->p[tape_degrade_variance].posy_offset = 5;
    fxdata->p[tape_degrade_variance].val_default.f = 0.0f;
    fxdata->p[tape_degrade_variance].dynamicDeactivation = &tapeGroupDeact;

    fxdata->p[tape_mix].set_name("Mix");
    fxdata->p[tape_mix].set_type(ct_percent);
    fxdata->p[tape_mix].posy_offset = 7;
    fxdata->p[tape_mix].val_default.f = 1.f;
}

void TapeEffect::init_default_values()
{
    fxdata->p[tape_drive].val.f = 0.85f;
    fxdata->p[tape_drive].deactivated = false;
    fxdata->p[tape_drive].deform_type = SolverType::RK4;
    fxdata->p[tape_saturation].val.f = 0.5f;
    fxdata->p[tape_bias].val.f = 0.5f;
    fxdata->p[tape_tone].val.f = 0.0f;

    fxdata->p[tape_speed].val.f = 30.0f;
    fxdata->p[tape_speed].deactivated = false;
    fxdata->p[tape_spacing].val.f = 0.1f;
    fxdata->p[tape_gap].val.f = 1.0f;
    fxdata->p[tape_thickness].val.f = 0.1f;

    fxdata->p[tape_degrade_depth].val.f = 0.0f;
    fxdata->p[tape_degrade_depth].deactivated = false;
    fxdata->p[tape_degrade_amount].val.f = 0.0f;
    fxdata->p[tape_degrade_variance].val.f = 0.0f;

    fxdata->p[tape_mix].val.f = 1.f;
}
} // namespace chowdsp
