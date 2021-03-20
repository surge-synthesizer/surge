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

#include "TapeEffect.h"

namespace chowdsp
{

TapeEffect::TapeEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), lossFilter(storage, 48)
{
    mix.set_blocksize(BLOCK_SIZE);
}

TapeEffect::~TapeEffect() {}

void TapeEffect::init()
{
    hysteresis.reset(samplerate);
    lossFilter.prepare(samplerate, BLOCK_SIZE);

    clear_block(L, BLOCK_SIZE_QUAD);
    clear_block(R, BLOCK_SIZE_QUAD);

    mix.set_target(1.f);
    mix.instantize();
}

void TapeEffect::process(float *dataL, float *dataR)
{
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        L[i] = dataL[i];
        R[i] = dataR[i];
    }

    if (!fxdata->p[tape_drive].deactivated)
    {
        hysteresis.set_params(*f[tape_drive], *f[tape_saturation], *f[tape_bias]);
        hysteresis.process_block(L, R);
    }

    if (!fxdata->p[tape_speed].deactivated)
    {
        lossFilter.set_params(*f[tape_speed], *f[tape_spacing], *f[tape_gap], *f[tape_thickness]);
        lossFilter.process(L, R);
    }

    mix.set_target_smoothed(limit_range(*f[tape_mix], 0.f, 1.f));
    mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
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
        return "Degrade (not implemented!)";
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
        return 9;
    case 2:
        return 19;
    case 3:
        return 27;
    }

    return 0;
}

void TapeEffect::init_ctrltypes()
{
    /*
     * The actualy deactivation status is on gain, so reflet that down
     * to freq and bw using the dynamic deactivation mechanism
     */
    static struct TapeEffectDeact : public ParameterDynamicDeactivationFunction
    {
        const bool getValue(Parameter *p) override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            switch (idx)
            {
            case tape_saturation:
            case tape_bias:
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
        Parameter *getPrimaryDeactivationDriver(Parameter *p) override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            switch (idx)
            {
            case tape_saturation:
            case tape_bias:
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
    fxdata->p[tape_drive].set_type(ct_percent_deactivatable);
    fxdata->p[tape_drive].posy_offset = 1;
    fxdata->p[tape_drive].val_default.f = 0.5f;
    fxdata->p[tape_drive].deactivated = false;
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

    fxdata->p[tape_speed].set_name("Speed");
    fxdata->p[tape_speed].set_type(ct_percent_deactivatable);
    fxdata->p[tape_speed].posy_offset = 3;
    fxdata->p[tape_speed].val_default.f = 1.0f;
    fxdata->p[tape_speed].deactivated = false;
    fxdata->p[tape_gap].set_name("Gap");
    fxdata->p[tape_gap].set_type(ct_percent);
    fxdata->p[tape_gap].posy_offset = 3;
    fxdata->p[tape_gap].val_default.f = 0.0f;
    fxdata->p[tape_gap].dynamicDeactivation = &tapeGroupDeact;
    fxdata->p[tape_spacing].set_name("Spacing");
    fxdata->p[tape_spacing].set_type(ct_percent);
    fxdata->p[tape_spacing].posy_offset = 3;
    fxdata->p[tape_spacing].val_default.f = 0.0f;
    fxdata->p[tape_spacing].dynamicDeactivation = &tapeGroupDeact;
    fxdata->p[tape_thickness].set_name("Thickness");
    fxdata->p[tape_thickness].set_type(ct_percent);
    fxdata->p[tape_thickness].posy_offset = 3;
    fxdata->p[tape_thickness].val_default.f = 0.0f;
    fxdata->p[tape_thickness].dynamicDeactivation = &tapeGroupDeact;

    fxdata->p[tape_degrade_depth].set_name("Depth");
    fxdata->p[tape_degrade_depth].set_type(ct_percent_deactivatable);
    fxdata->p[tape_degrade_depth].posy_offset = 5;
    fxdata->p[tape_degrade_depth].val_default.f = 0.5f;
    fxdata->p[tape_degrade_depth].deactivated = false;
    fxdata->p[tape_degrade_amount].set_name("Amount");
    fxdata->p[tape_degrade_amount].set_type(ct_percent);
    fxdata->p[tape_degrade_amount].posy_offset = 5;
    fxdata->p[tape_degrade_amount].val_default.f = 0.5f;
    fxdata->p[tape_degrade_amount].dynamicDeactivation = &tapeGroupDeact;
    fxdata->p[tape_degrade_variance].set_name("Variance");
    fxdata->p[tape_degrade_variance].set_type(ct_percent);
    fxdata->p[tape_degrade_variance].posy_offset = 5;
    fxdata->p[tape_degrade_variance].val_default.f = 0.5f;
    fxdata->p[tape_degrade_variance].dynamicDeactivation = &tapeGroupDeact;

    fxdata->p[tape_mix].set_name("Mix");
    fxdata->p[tape_mix].set_type(ct_percent);
    fxdata->p[tape_mix].posy_offset = 7;
    fxdata->p[tape_mix].val_default.f = 1.f;
}

void TapeEffect::init_default_values()
{
    fxdata->p[tape_drive].val.f = 0.5f;
    fxdata->p[tape_saturation].val.f = 0.5f;
    fxdata->p[tape_bias].val.f = 0.5f;

    fxdata->p[tape_speed].val.f = 1.0f;
    fxdata->p[tape_spacing].val.f = 0.0f;
    fxdata->p[tape_gap].val.f = 0.0f;
    fxdata->p[tape_thickness].val.f = 0.0f;

    fxdata->p[tape_degrade_depth].val.f = 0.5f;
    fxdata->p[tape_degrade_amount].val.f = 0.5f;
    fxdata->p[tape_degrade_variance].val.f = 0.5f;

    fxdata->p[tape_mix].val.f = 1.f;
}
} // namespace chowdsp
