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

Tape::Tape(SurgeStorage *storage, FxStorage *fxdata, pdata *pd) : Effect(storage, fxdata, pd),
    lossFilter (storage, 48)
{
}

Tape::~Tape() {}

void Tape::init()
{
    hysteresis.reset(samplerate);
    lossFilter.prepare(samplerate, BLOCK_SIZE);
}

void Tape::process(float *dataL, float *dataR)
{
    hysteresis.set_params(*f[tape_drive], *f[tape_saturation], *f[tape_bias]);
    hysteresis.process_block(dataL, dataR);

    lossFilter.set_params(*f[tape_speed], *f[tape_spacing], *f[tape_gap], *f[tape_thick]);
    lossFilter.process(dataL, dataR);
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

    fxdata->p[tape_speed].set_name("Speed");
    fxdata->p[tape_speed].set_type(ct_percent);
    fxdata->p[tape_speed].posy_offset = 2;
    fxdata->p[tape_speed].val_default.f = 1.0f;

    fxdata->p[tape_gap].set_name("Gap");
    fxdata->p[tape_gap].set_type(ct_percent);
    fxdata->p[tape_gap].posy_offset = 2;
    fxdata->p[tape_gap].val_default.f = 0.0f;

    fxdata->p[tape_spacing].set_name("Spacing");
    fxdata->p[tape_spacing].set_type(ct_percent);
    fxdata->p[tape_spacing].posy_offset = 2;
    fxdata->p[tape_spacing].val_default.f = 0.0f;

    fxdata->p[tape_thick].set_name("Thickness");
    fxdata->p[tape_thick].set_type(ct_percent);
    fxdata->p[tape_thick].posy_offset = 2;
    fxdata->p[tape_thick].val_default.f = 0.0f;
}

void Tape::init_default_values()
{
    fxdata->p[tape_drive].val.f = 0.5f;
    fxdata->p[tape_saturation].val.f = 0.5f;
    fxdata->p[tape_bias].val.f = 0.5f;

    fxdata->p[tape_speed].val.f = 1.0f;
    fxdata->p[tape_spacing].val.f = 0.0f;
    fxdata->p[tape_gap].val.f = 0.0f;
    fxdata->p[tape_thick].val.f = 0.0f;
}

const char *Tape::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Hysteresis";
    case 1:
        return "Loss";
    }

    return 0;
}

int Tape::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 8;
    }

    return 0;
}

} // namespace chowdsp
