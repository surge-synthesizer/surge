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

#include "Tape.h"

namespace chowdsp
{

Tape::Tape(SurgeStorage *storage, FxStorage *fxdata, pdata *pd) : Effect(storage, fxdata, pd)
{

}

Tape::~Tape() {}

void Tape::init()
{
    hysteresis.reset(samplerate);
}

void Tape::process(float *dataL, float *dataR)
{
    hysteresis.set_params(*f[tape_drive], *f[tape_saturation], *f[tape_bias]);
    hysteresis.process_block(dataL, dataR);
}

void Tape::suspend() {}

void Tape::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[tape_drive].set_name("Drive");
    fxdata->p[tape_drive].set_type(ct_percent);
    fxdata->p[tape_drive].posy_offset = 1;
    fxdata->p[tape_drive].val_default.f = 0.5f;

    fxdata->p[tape_saturation].set_name("Saturation");
    fxdata->p[tape_saturation].set_type(ct_percent);
    fxdata->p[tape_saturation].posy_offset = 1;
    fxdata->p[tape_saturation].val_default.f = 0.5f;

    fxdata->p[tape_bias].set_name("Bias");
    fxdata->p[tape_bias].set_type(ct_percent);
    fxdata->p[tape_bias].posy_offset = 1;
    fxdata->p[tape_bias].val_default.f = 0.5f;
}

void Tape::init_default_values()
{
    fxdata->p[tape_drive].val.f = 0.5f;
    fxdata->p[tape_saturation].val.f = 0.5f;
    fxdata->p[tape_bias].val.f = 0.5f;
}

const char *Tape::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Hysteresis";
    }

    return 0;
}

int Tape::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    }

    return 0;
}

} // namespace chowdsp
