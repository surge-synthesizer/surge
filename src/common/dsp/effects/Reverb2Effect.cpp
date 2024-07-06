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
#include "Reverb2Effect.h"

Reverb2Effect::Reverb2Effect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : surge::sstfx::SurgeSSTFXBase<sst::effects::reverb2::Reverb2<surge::sstfx::SurgeFXConfig>>(
          storage, fxdata, pd)
{
}

Reverb2Effect::~Reverb2Effect() {}

const char *Reverb2Effect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Pre-Delay";
    case 1:
        return "Reverb";
    case 2:
        return "EQ";
    case 3:
        return "Output";
    }
    return 0;
}

int Reverb2Effect::group_label_ypos(int id)
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
        return 23;
    }
    return 0;
}

void Reverb2Effect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[rev2_predelay].set_name("Pre-Delay");
    fxdata->p[rev2_predelay].set_type(ct_reverbpredelaytime);

    fxdata->p[rev2_room_size].set_name("Room Size");
    fxdata->p[rev2_room_size].set_type(ct_percent_bipolar);
    fxdata->p[rev2_decay_time].set_name("Decay Time");
    fxdata->p[rev2_decay_time].set_type(ct_reverbtime);
    fxdata->p[rev2_diffusion].set_name("Diffusion");
    fxdata->p[rev2_diffusion].set_type(ct_percent);
    fxdata->p[rev2_buildup].set_name("Buildup");
    fxdata->p[rev2_buildup].set_type(ct_percent);
    fxdata->p[rev2_modulation].set_name("Modulation");
    fxdata->p[rev2_modulation].set_type(ct_percent);

    fxdata->p[rev2_hf_damping].set_name("HF Damping");
    fxdata->p[rev2_hf_damping].set_type(ct_percent);
    fxdata->p[rev2_lf_damping].set_name("LF Damping");
    fxdata->p[rev2_lf_damping].set_type(ct_percent);

    fxdata->p[rev2_width].set_name("Width");
    fxdata->p[rev2_width].set_type(ct_decibel_narrow);
    fxdata->p[rev2_mix].set_name("Mix");
    fxdata->p[rev2_mix].set_type(ct_percent);

    for (int i = rev2_predelay; i < rev2_num_params; ++i)
    {
        auto a = 1;
        if (i >= rev2_room_size)
            a += 2;
        if (i >= rev2_lf_damping)
            a += 2;
        if (i >= rev2_width)
            a += 2;
        fxdata->p[i].posy_offset = a;
    }

    configureControlsFromFXMetadata();
}
