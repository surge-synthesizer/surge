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
#include "DelayEffect.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/Clippers.h"

namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

using namespace std;

DelayEffect::DelayEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : surge::sstfx::SurgeSSTFXBase<sst::effects::delay::Delay<surge::sstfx::SurgeFXConfig>>(
          storage, fxdata, pd)
{
    mix.set_blocksize(BLOCK_SIZE);
    pan.set_blocksize(BLOCK_SIZE);
    feedback.set_blocksize(BLOCK_SIZE);
    crossfeed.set_blocksize(BLOCK_SIZE);
}

DelayEffect::~DelayEffect() {}

const char *DelayEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Input";
    case 1:
        return "Delay Time";
    case 2:
        return "Feedback/EQ";
    case 3:
        return "Modulation";
    case 4:
        return "Output";
    }
    return 0;
}
int DelayEffect::group_label_ypos(int id)
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
        return 21;
    case 4:
        return 27;
    }
    return 0;
}

void DelayEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[dly_time_left].set_type(ct_envtime);
    fxdata->p[dly_time_right].set_type(ct_envtime_linkable_delay);
    fxdata->p[dly_feedback].set_name("Feedback");
    fxdata->p[dly_feedback].set_type(ct_dly_fb_clippingmodes);

    fxdata->p[dly_crossfeed].set_type(ct_percent_with_extend_to_bipolar);
    fxdata->p[dly_lowcut].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[dly_highcut].set_type(ct_freq_audible_deactivatable_lp);
    fxdata->p[dly_mod_rate].set_type(ct_lforate);
    fxdata->p[dly_mod_depth].set_type(ct_detuning);
    fxdata->p[dly_input_channel].set_type(ct_percent_bipolar_stereo);

    fxdata->p[dly_mix].set_type(ct_percent);
    fxdata->p[dly_width].set_type(ct_decibel_narrow);

    fxdata->p[dly_time_left].posy_offset = 5;
    fxdata->p[dly_time_right].posy_offset = 5;

    fxdata->p[dly_feedback].posy_offset = 7;
    fxdata->p[dly_crossfeed].posy_offset = 7;
    fxdata->p[dly_lowcut].posy_offset = 7;
    fxdata->p[dly_highcut].posy_offset = 7;

    fxdata->p[dly_mod_rate].posy_offset = 9;
    fxdata->p[dly_mod_depth].posy_offset = 9;
    fxdata->p[dly_input_channel].posy_offset = -15;

    fxdata->p[dly_mix].posy_offset = 9;
    fxdata->p[dly_width].posy_offset = 5;

    configureControlsFromFXMetadata();
}

void DelayEffect::init_default_values()
{
    fxdata->p[dly_time_left].val.f = -2.f;
    fxdata->p[dly_time_right].val.f = -2.f;
    fxdata->p[dly_time_right].deactivated = false;
    fxdata->p[dly_feedback].val.f = 0.5f;
    fxdata->p[dly_feedback].deform_type = 1;
    fxdata->p[dly_feedback].set_extend_range(false);
    fxdata->p[dly_crossfeed].val.f = 0.0f;
    fxdata->p[dly_crossfeed].set_extend_range(false);

    fxdata->p[dly_lowcut].val.f = -24.f;
    fxdata->p[dly_lowcut].deactivated = false;

    fxdata->p[dly_highcut].val.f = 30.f;
    fxdata->p[dly_highcut].deactivated = false;

    fxdata->p[dly_mod_rate].val.f = -2.f;
    fxdata->p[dly_mod_depth].val.f = 0.f;
    fxdata->p[dly_mod_depth].set_extend_range(false);
    fxdata->p[dly_input_channel].val.f = 0.f;
    fxdata->p[dly_mix].val.f = 0.5f;
    fxdata->p[dly_width].val.f = 0.f;
}

void DelayEffect::handleStreamingMismatches(int streamingRevision,
                                            int currentSynthStreamingRevision)
{
    if (streamingRevision <= 15)
    {
        fxdata->p[dly_lowcut].deactivated = false;
        fxdata->p[dly_highcut].deactivated = false;
        fxdata->p[dly_time_right].deactivated = false;
    }

    if (streamingRevision <= 17)
    {
        fxdata->p[dly_feedback].deform_type = 1;
    }

    if (streamingRevision <= 18)
    {
        fxdata->p[dly_feedback].set_extend_range(false);
    }

    if (streamingRevision <= 21)
    {
        fxdata->p[dly_crossfeed].set_extend_range(false);
        fxdata->p[dly_mod_depth].set_extend_range(false);
    }
}
