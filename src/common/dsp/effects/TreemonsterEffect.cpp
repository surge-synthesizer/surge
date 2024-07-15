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

#include "TreemonsterEffect.h"
#include "DebugHelpers.h"

#include "sst/basic-blocks/mechanics/block-ops.h"
namespace mech = sst::basic_blocks::mechanics;

TreemonsterEffect::TreemonsterEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : surge::sstfx::SurgeSSTFXBase<
          sst::effects::treemonster::TreeMonster<surge::sstfx::SurgeFXConfig>>(
          storage, fxdata, pd) //, lp(storage), hp(storage)
{
}

TreemonsterEffect::~TreemonsterEffect() {}

const char *TreemonsterEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Pitch Detection";
    case 1:
        return "Oscillator";
    case 2:
        return "Output";
    }
    return 0;
}
int TreemonsterEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 11;
    case 2:
        return 17;
    }
    return 0;
}

void TreemonsterEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[tm_threshold].set_name("Threshold");
    fxdata->p[tm_threshold].set_type(ct_decibel_attenuation_large);
    fxdata->p[tm_threshold].val_default.f = -24.f;
    fxdata->p[tm_threshold].posy_offset = 1;
    fxdata->p[tm_speed].set_name("Speed");
    fxdata->p[tm_speed].set_type(ct_percent);
    fxdata->p[tm_speed].val_default.f = 0.5f;
    fxdata->p[tm_speed].posy_offset = 1;
    fxdata->p[tm_hp].set_name("Low Cut");
    fxdata->p[tm_hp].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[tm_hp].posy_offset = 1;
    fxdata->p[tm_lp].set_name("High Cut");
    fxdata->p[tm_lp].set_type(ct_freq_audible_deactivatable_lp);
    fxdata->p[tm_lp].posy_offset = 1;

    fxdata->p[tm_pitch].set_name("Pitch");
    fxdata->p[tm_pitch].set_type(ct_pitch);
    fxdata->p[tm_pitch].posy_offset = 3;
    fxdata->p[tm_ring_mix].set_name("Ring Modulation");
    fxdata->p[tm_ring_mix].set_type(ct_percent);
    fxdata->p[tm_ring_mix].val_default.f = 0.5f;
    fxdata->p[tm_ring_mix].posy_offset = 3;

    fxdata->p[tm_width].set_name("Width");
    fxdata->p[tm_width].set_type(ct_percent_bipolar);
    fxdata->p[tm_width].posy_offset = 5;
    fxdata->p[tm_mix].set_name("Mix");
    fxdata->p[tm_mix].set_type(ct_percent);
    fxdata->p[tm_mix].posy_offset = 5;
    fxdata->p[tm_mix].val_default.f = 1.f;

    configureControlsFromFXMetadata();
}
