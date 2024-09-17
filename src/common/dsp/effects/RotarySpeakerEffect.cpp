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
#include "RotarySpeakerEffect.h"

using namespace std;

const char *RotarySpeakerEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Speaker";
    case 1:
        return "Amp";
    case 2:
        return "Modulation";
    case 3:
        return "Output";
    }
    return 0;
}
int RotarySpeakerEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 7;
    case 2:
        return 13;
    case 3:
        return 19;
    }
    return 0;
}

void RotarySpeakerEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[rot_horn_rate].set_name("Horn Rate");
    fxdata->p[rot_horn_rate].set_type(ct_lforate);
    fxdata->p[rot_rotor_rate].set_name("Rotor Rate");
    fxdata->p[rot_rotor_rate].set_type(ct_percent200);
    fxdata->p[rot_drive].set_name("Drive");
    fxdata->p[rot_drive].set_type(ct_rotarydrive);
    fxdata->p[rot_waveshape].set_name("Model");
    fxdata->p[rot_waveshape].set_type(ct_distortion_waveshape);
    fxdata->p[rot_doppler].set_name("Doppler");
    fxdata->p[rot_doppler].set_type(ct_percent);
    fxdata->p[rot_tremolo].set_name("Tremolo");
    fxdata->p[rot_tremolo].set_type(ct_percent);
    fxdata->p[rot_width].set_name("Width");
    fxdata->p[rot_width].set_type(ct_decibel_narrow);
    fxdata->p[rot_mix].set_name("Mix");
    fxdata->p[rot_mix].set_type(ct_percent);

    fxdata->p[rot_rotor_rate].val_default.f = 0.7;

    fxdata->p[rot_horn_rate].posy_offset = 1;
    fxdata->p[rot_rotor_rate].posy_offset = -3;

    fxdata->p[rot_drive].posy_offset = 1;
    fxdata->p[rot_waveshape].posy_offset = -3;

    fxdata->p[rot_doppler].posy_offset = 11;
    fxdata->p[rot_tremolo].posy_offset = 11;

    fxdata->p[rot_width].posy_offset = 7;
    fxdata->p[rot_mix].posy_offset = 7;

    configureControlsFromFXMetadata();
}

void RotarySpeakerEffect::handleStreamingMismatches(int streamingRevision,
                                                    int currentSynthStreamingRevision)
{
    if (streamingRevision <= 12)
    {
        fxdata->p[rot_rotor_rate].val.f = 0.7;
        fxdata->p[rot_drive].val.f = 0.f;
        fxdata->p[rot_drive].deactivated = true;
        fxdata->p[rot_waveshape].val.i = 0;
        fxdata->p[rot_width].val.f = 1.f;
        fxdata->p[rot_mix].val.f = 1.f;
    }
}