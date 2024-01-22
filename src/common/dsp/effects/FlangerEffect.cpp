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
#include "FlangerEffect.h"
#include "Tunings.h"
#include <algorithm>
#include "sst/basic-blocks/dsp/MidSide.h"

const char *FlangerEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Modulation";
    case 1:
        return "Combs";
    case 2:
        return "Feedback";
    case 3:
        return "Output";
    }
    return 0;
}
int FlangerEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 9;
    case 2:
        return 17;
    case 3:
        return 23;
    }
    return 0;
}

void FlangerEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[fl_mode].set_name("Mode");
    fxdata->p[fl_mode].set_type(ct_flangermode);

    fxdata->p[fl_wave].set_name("Waveform");
    fxdata->p[fl_wave].set_type(ct_fxlfowave);

    fxdata->p[fl_rate].set_name("Rate");
    fxdata->p[fl_rate].set_type(ct_lforate);

    fxdata->p[fl_depth].set_name("Depth");
    fxdata->p[fl_depth].set_type(ct_percent);

    fxdata->p[fl_voices].set_name("Count");
    fxdata->p[fl_voices].set_type(ct_flangervoices);

    fxdata->p[fl_voice_basepitch].set_name("Base Pitch");
    fxdata->p[fl_voice_basepitch].set_type(ct_flangerpitch);

    fxdata->p[fl_voice_spacing].set_name("Spacing");
    fxdata->p[fl_voice_spacing].set_type(ct_flangerspacing);

    fxdata->p[fl_feedback].set_name("Feedback");
    fxdata->p[fl_feedback].set_type(ct_percent);

    fxdata->p[fl_damping].set_name("HF Damping");
    fxdata->p[fl_damping].set_type(ct_percent);

    fxdata->p[fl_width].set_name("Width");
    fxdata->p[fl_width].set_type(ct_decibel_narrow);

    fxdata->p[fl_mix].set_name("Mix");
    fxdata->p[fl_mix].set_type(ct_percent_bipolar);

    fxdata->p[fl_wave].posy_offset = -1;
    fxdata->p[fl_rate].posy_offset = -1;
    fxdata->p[fl_depth].posy_offset = -1;

    fxdata->p[fl_voices].posy_offset = 1;
    fxdata->p[fl_voice_basepitch].posy_offset = 1;
    fxdata->p[fl_voice_spacing].posy_offset = 1;

    fxdata->p[fl_feedback].posy_offset = 3;
    fxdata->p[fl_damping].posy_offset = 3;

    fxdata->p[fl_mode].posy_offset = 23;
    fxdata->p[fl_width].posy_offset = 7;
    fxdata->p[fl_mix].posy_offset = 7;

    configureControlsFromFXMetadata();
}
