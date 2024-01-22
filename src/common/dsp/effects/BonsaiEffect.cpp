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
#include "BonsaiEffect.h"
#include "Tunings.h"
#include <algorithm>
// #include "sst/basic-blocks/dsp/MidSide.h"

const char *BonsaiEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Input";
    case 1:
        return "Bass Boost";
    case 2:
        return "Saturation";
    case 3:
        return "Noise";
    case 4:
        return "Output";
    }
    return 0;
}
int BonsaiEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 5;
    case 2:
        return 11;
    case 3:
        return 19;
    case 4:
        return 25;
    }
    return 0;
}

void BonsaiEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[b_gain_in].set_type(ct_decibel_narrow);
    fxdata->p[b_gain_in].posy_offset = 1;

    fxdata->p[b_bass_boost].set_type(ct_bonsai_bass_boost);
    fxdata->p[b_bass_boost].posy_offset = 3;

    fxdata->p[b_bass_distort].set_type(ct_percent);
    fxdata->p[b_bass_distort].posy_offset = 3;

    fxdata->p[b_tape_bias_mode].set_type(ct_bonsai_sat_filter);
    fxdata->p[b_tape_bias_mode].posy_offset = 5;

    fxdata->p[b_tape_dist_mode].set_type(ct_bonsai_sat_mode);
    fxdata->p[b_tape_dist_mode].posy_offset = 5;

    fxdata->p[b_tape_sat].set_type(ct_percent);
    fxdata->p[b_tape_sat].posy_offset = 5;

    fxdata->p[b_noise_sensitivity].set_type(ct_percent);
    fxdata->p[b_noise_sensitivity].posy_offset = 7;

    fxdata->p[b_noise_gain].set_type(ct_decibel_narrow);
    fxdata->p[b_noise_gain].posy_offset = 7;

    fxdata->p[b_dull].set_type(ct_percent);
    fxdata->p[b_dull].posy_offset = 9;

    fxdata->p[b_gain_out].set_type(ct_decibel_narrow);
    fxdata->p[b_gain_out].posy_offset = 9;

    fxdata->p[b_mix].set_type(ct_percent);
    fxdata->p[b_mix].posy_offset = 9;

    configureControlsFromFXMetadata();
}
