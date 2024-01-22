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
#include "PhaserEffect.h"

#include "globals.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/MidSide.h"

namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

void PhaserEffect::init_ctrltypes()
{
    static struct PhaserDeactivate : public ParameterDynamicDeactivationFunction
    {
        bool getValue(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            if (idx == ph_spread)
            {
                return fx->p[ph_stages].val.i == 1;
            }

            return false;
        }
    } phGroupDeact;

    Effect::init_ctrltypes();

    fxdata->p[ph_mod_wave].set_name("Waveform");
    fxdata->p[ph_mod_wave].set_type(ct_fxlfowave);
    fxdata->p[ph_mod_rate].set_name("Rate");
    fxdata->p[ph_mod_rate].set_type(ct_lforate_deactivatable);
    fxdata->p[ph_mod_depth].set_name("Depth");
    fxdata->p[ph_mod_depth].set_type(ct_percent);
    fxdata->p[ph_stereo].set_name("Stereo");
    fxdata->p[ph_stereo].set_type(ct_percent);

    fxdata->p[ph_stages].set_name("Count");
    fxdata->p[ph_stages].set_type(ct_phaser_stages);
    fxdata->p[ph_spread].set_name("Spread");
    fxdata->p[ph_spread].set_type(ct_percent);
    fxdata->p[ph_center].set_name("Center");
    fxdata->p[ph_center].set_type(ct_percent_bipolar);
    fxdata->p[ph_sharpness].set_name("Sharpness");
    fxdata->p[ph_sharpness].set_type(ct_percent_bipolar);
    fxdata->p[ph_feedback].set_name("Feedback");
    fxdata->p[ph_feedback].set_type(ct_percent_bipolar);

    fxdata->p[ph_tone].set_name("Tone");
    fxdata->p[ph_tone].set_type(ct_percent_bipolar_deactivatable);

    fxdata->p[ph_width].set_name("Width");
    fxdata->p[ph_width].set_type(ct_decibel_narrow);
    fxdata->p[ph_mix].set_name("Mix");
    fxdata->p[ph_mix].set_type(ct_percent);

    fxdata->p[ph_mod_wave].posy_offset = -19;
    fxdata->p[ph_mod_rate].posy_offset = -3;
    fxdata->p[ph_mod_depth].posy_offset = -3;
    fxdata->p[ph_stereo].posy_offset = -3;

    fxdata->p[ph_stages].posy_offset = -5;
    fxdata->p[ph_center].posy_offset = 15;
    fxdata->p[ph_spread].posy_offset = -5;
    fxdata->p[ph_sharpness].posy_offset = 13;
    fxdata->p[ph_feedback].posy_offset = 17;

    fxdata->p[ph_tone].posy_offset = 1;

    fxdata->p[ph_width].posy_offset = 13;
    fxdata->p[ph_mix].posy_offset = 17;

    fxdata->p[ph_spread].dynamicDeactivation = &phGroupDeact;

    configureControlsFromFXMetadata();
}

const char *PhaserEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Modulation";
    case 1:
        return "Stages";
    case 2:
        return "Filter";
    case 3:
        return "Output";
    }
    return 0;
}
int PhaserEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 11;
    case 2:
        return 23;
    case 3:
        return 27;
    }
    return 0;
}
