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

#include "SpringReverbEffect.h"

#include "sst/basic-blocks/mechanics/block-ops.h"
namespace mech = sst::basic_blocks::mechanics;

namespace chowdsp
{
SpringReverbEffect::SpringReverbEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd)
{
}

void SpringReverbEffect::init()
{
    proc.prepare(storage->samplerate, BLOCK_SIZE);

    mix.set_target(1.f);
    mix.instantize();
}

void SpringReverbEffect::process(float *dataL, float *dataR)
{
    proc.setParams(
        {
            clamp01(*pd_float[spring_reverb_size]),
            clamp01(*pd_float[spring_reverb_decay]),
            clamp01(*pd_float[spring_reverb_reflections]),
            clamp01(*pd_float[spring_reverb_spin]),
            clamp01(*pd_float[spring_reverb_damping]),
            clamp01(*pd_float[spring_reverb_chaos]),
            *pd_float[spring_reverb_knock] > 0.5f,
        },
        BLOCK_SIZE);

    mech::copy_from_to<BLOCK_SIZE>(dataL, L);
    mech::copy_from_to<BLOCK_SIZE>(dataR, R);

    proc.processBlock(L, R, BLOCK_SIZE);

    mix.set_target_smoothed(clamp01(*pd_float[spring_reverb_mix]));
    mix.fade_2_blocks_inplace(dataL, L, dataR, R, BLOCK_SIZE_QUAD);
}

void SpringReverbEffect::suspend() { init(); }

void SpringReverbEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[spring_reverb_size].set_name("Size");
    fxdata->p[spring_reverb_size].set_type(ct_percent);
    fxdata->p[spring_reverb_size].val_default.f = 0.5f;
    fxdata->p[spring_reverb_size].posy_offset = 1;

    fxdata->p[spring_reverb_decay].set_name("Decay");
    fxdata->p[spring_reverb_decay].set_type(ct_spring_decay);
    fxdata->p[spring_reverb_decay].val_default.f = 0.5f;
    fxdata->p[spring_reverb_decay].posy_offset = 1;

    fxdata->p[spring_reverb_reflections].set_name("Reflections");
    fxdata->p[spring_reverb_reflections].set_type(ct_percent);
    fxdata->p[spring_reverb_reflections].val_default.f = 1.0f;
    fxdata->p[spring_reverb_reflections].posy_offset = 1;

    fxdata->p[spring_reverb_damping].set_name("HF Damping");
    fxdata->p[spring_reverb_damping].set_type(ct_percent);
    fxdata->p[spring_reverb_damping].val_default.f = 0.5f;
    fxdata->p[spring_reverb_damping].posy_offset = 1;

    fxdata->p[spring_reverb_spin].set_name("Spin");
    fxdata->p[spring_reverb_spin].set_type(ct_percent);
    fxdata->p[spring_reverb_spin].val_default.f = 0.5f;
    fxdata->p[spring_reverb_spin].posy_offset = 3;

    fxdata->p[spring_reverb_chaos].set_name("Chaos");
    fxdata->p[spring_reverb_chaos].set_type(ct_percent);
    fxdata->p[spring_reverb_chaos].val_default.f = 0.0f;
    fxdata->p[spring_reverb_chaos].posy_offset = 3;

    fxdata->p[spring_reverb_knock].set_name("Knock");
    fxdata->p[spring_reverb_knock].set_type(ct_float_toggle); // so that it can be modulated
    fxdata->p[spring_reverb_knock].val_default.f = 0.0f;
    fxdata->p[spring_reverb_knock].posy_offset = 3;

    fxdata->p[spring_reverb_mix].set_name("Mix");
    fxdata->p[spring_reverb_mix].set_type(ct_percent);
    fxdata->p[spring_reverb_mix].val_default.f = 0.5f;
    fxdata->p[spring_reverb_mix].posy_offset = 5;
}

void SpringReverbEffect::init_default_values()
{
    fxdata->p[spring_reverb_size].val.f = 0.5f;
    fxdata->p[spring_reverb_decay].val.f = 0.5f;
    fxdata->p[spring_reverb_reflections].val.f = 1.0f;
    fxdata->p[spring_reverb_damping].val.f = 0.5f;
    fxdata->p[spring_reverb_spin].val.f = 0.5f;
    fxdata->p[spring_reverb_chaos].val.f = 0.0f;
    fxdata->p[spring_reverb_knock].val.f = 0.0f;
    fxdata->p[spring_reverb_mix].val.f = 0.5f;
}

const char *SpringReverbEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Reverb";
    case 1:
        return "Modulation";
    case 2:
        return "Output";
    }
    return 0;
}

int SpringReverbEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 11;
    case 2:
        return 19;
    }
    return 0;
}

} // namespace chowdsp
