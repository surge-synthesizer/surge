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
#include "Reverb1Effect.h"

#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"
namespace mech = sst::basic_blocks::mechanics;

using namespace std;

Reverb1Effect::Reverb1Effect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : surge::sstfx::SurgeSSTFXBase<sst::effects::reverb1::Reverb1<surge::sstfx::SurgeFXConfig>>(
          storage, fxdata, pd)
{
}

const char *Reverb1Effect::group_label(int id)
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
int Reverb1Effect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 5;
    case 2:
        return 15;
    case 3:
        return 25;
    }
    return 0;
}

void Reverb1Effect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[rev1_predelay].set_type(ct_envtime);
    fxdata->p[rev1_predelay].modulateable = false;

    fxdata->p[rev1_shape].set_type(ct_reverbshape);
    fxdata->p[rev1_shape].modulateable = false;

    fxdata->p[rev1_roomsize].set_type(ct_percent);
    fxdata->p[rev1_roomsize].modulateable = false;
    fxdata->p[rev1_decaytime].set_type(ct_reverbtime);
    fxdata->p[rev1_damping].set_type(ct_percent);

    fxdata->p[rev1_lowcut].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[rev1_freq1].set_type(ct_freq_audible);
    fxdata->p[rev1_gain1].set_type(ct_decibel);
    fxdata->p[rev1_highcut].set_type(ct_freq_audible_deactivatable_lp);

    fxdata->p[rev1_mix].set_type(ct_percent);
    fxdata->p[rev1_width].set_type(ct_decibel_narrow);

    // fxdata->p[rev1_variation].set_name("Irregularity");
    // fxdata->p[rev1_variation].set_type(ct_percent);

    fxdata->p[rev1_predelay].posy_offset = 1;

    fxdata->p[rev1_shape].posy_offset = 3;
    fxdata->p[rev1_decaytime].posy_offset = 3;
    fxdata->p[rev1_roomsize].posy_offset = 3;
    fxdata->p[rev1_damping].posy_offset = 3;

    fxdata->p[rev1_lowcut].posy_offset = 5;
    fxdata->p[rev1_freq1].posy_offset = 5;
    fxdata->p[rev1_gain1].posy_offset = 5;
    fxdata->p[rev1_highcut].posy_offset = 5;

    fxdata->p[rev1_mix].posy_offset = 9;
    fxdata->p[rev1_width].posy_offset = 5;

    configureControlsFromFXMetadata();
}

void Reverb1Effect::init_default_values()
{
    fxdata->p[rev1_predelay].val.f = -4.f;
    fxdata->p[rev1_shape].val.i = 0;
    fxdata->p[rev1_decaytime].val.f = 1.f;
    fxdata->p[rev1_roomsize].val.f = 0.5f;
    fxdata->p[rev1_damping].val.f = 0.2f;
    fxdata->p[rev1_freq1].val.f = 0.0f;
    fxdata->p[rev1_gain1].val.f = 0.0f;

    fxdata->p[rev1_lowcut].val.f = -24.0f;
    fxdata->p[rev1_lowcut].deactivated = false;

    fxdata->p[rev1_highcut].val.f = 72.0f;
    fxdata->p[rev1_highcut].deactivated = false;

    fxdata->p[rev1_mix].val.f = 0.5f;
    fxdata->p[rev1_width].val.f = 0.0f;

    // fxdata->p[rev1_variation].val.f = 0.f;
}

void Reverb1Effect::handleStreamingMismatches(int streamingRevision,
                                              int currentSynthStreamingRevision)
{
    if (streamingRevision <= 15)
    {
        fxdata->p[rev1_lowcut].deactivated = false;
        fxdata->p[rev1_highcut].deactivated = false;
    }
}
