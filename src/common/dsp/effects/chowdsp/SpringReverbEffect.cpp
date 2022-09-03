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

#include "SpringReverbEffect.h"

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
            clamp01(*f[spring_reverb_size]),
            clamp01(*f[spring_reverb_decay]),
            clamp01(*f[spring_reverb_reflections]),
            clamp01(*f[spring_reverb_spin]),
            clamp01(*f[spring_reverb_damping]),
            clamp01(*f[spring_reverb_chaos]),
            *f[spring_reverb_knock] > 0.5f,
        },
        BLOCK_SIZE);

    copy_block(dataL, L, BLOCK_SIZE_QUAD);
    copy_block(dataR, R, BLOCK_SIZE_QUAD);

    proc.processBlock(L, R, BLOCK_SIZE);

    mix.set_target_smoothed(clamp01(*f[spring_reverb_mix]));
    mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
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
    fxdata->p[spring_reverb_knock].set_type(ct_float_toggle);
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
