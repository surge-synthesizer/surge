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
#include "FloatyDelayEffect.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"
#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/Clippers.h"

namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

using namespace std;

FloatyDelayEffect::FloatyDelayEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : surge::sstfx::SurgeSSTFXBase<
          sst::effects::floatydelay::FloatyDelay<surge::sstfx::SurgeFXConfig>>(storage, fxdata, pd)
{
}

FloatyDelayEffect::~FloatyDelayEffect() {}

const char *FloatyDelayEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Delay";
    case 1:
        return "Feedback";
    case 2:
        return "Warp";
    case 3:
        return "Output";
    }
    return 0;
}
int FloatyDelayEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 7;
    case 2:
        return 15;
    case 3:
        return 25;
    }
    return 0;
}

void FloatyDelayEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[fld_time].set_type(ct_floaty_delay_time);
    fxdata->p[fld_playrate].set_type(ct_floaty_delay_playrate);

    fxdata->p[fld_feedback].set_type(ct_percent);
    fxdata->p[fld_cutoff].set_type(ct_freq_audible);
    fxdata->p[fld_resonance].set_type(ct_percent);

    fxdata->p[fld_warp_rate].set_type(ct_floaty_warp_time);
    fxdata->p[fld_warp_width].set_type(ct_percent);
    fxdata->p[fld_pitch_warp_depth].set_type(ct_percent);
    fxdata->p[fld_filt_warp_depth].set_type(ct_percent);

    fxdata->p[fld_HP_freq].set_type(ct_freq_audible);
    fxdata->p[fld_mix].set_type(ct_percent);

    fxdata->p[fld_time].posy_offset = 1;
    fxdata->p[fld_playrate].posy_offset = 1;

    fxdata->p[fld_feedback].posy_offset = 3;
    fxdata->p[fld_cutoff].posy_offset = 3;
    fxdata->p[fld_resonance].posy_offset = 3;

    fxdata->p[fld_warp_rate].posy_offset = 5;
    fxdata->p[fld_warp_width].posy_offset = 5;
    fxdata->p[fld_pitch_warp_depth].posy_offset = 5;
    fxdata->p[fld_filt_warp_depth].posy_offset = 5;

    fxdata->p[fld_HP_freq].posy_offset = 7;
    fxdata->p[fld_mix].posy_offset = 7;

    configureControlsFromFXMetadata();
}

void FloatyDelayEffect::init_default_values()
{
    fxdata->p[fld_mix].val.f = .3f;
    fxdata->p[fld_HP_freq].val.f = -60.f;

    fxdata->p[fld_time].val.f = -1.73697f;
    fxdata->p[fld_playrate].val.f = 1.f;
    fxdata->p[fld_feedback].val.f = .5f;

    fxdata->p[fld_warp_rate].val.f = 0.f;
    fxdata->p[fld_pitch_warp_depth].val.f = 0.f;
    fxdata->p[fld_filt_warp_depth].val.f = 0.f;
    fxdata->p[fld_warp_width].val.f = 0.f;

    fxdata->p[fld_cutoff].val.f = 0.f;
    fxdata->p[fld_resonance].val.f = .5f;
}

void FloatyDelayEffect::handleStreamingMismatches(int streamingRevision,
                                                  int currentSynthStreamingRevision)
{
}
