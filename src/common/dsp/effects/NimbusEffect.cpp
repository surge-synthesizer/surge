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

#include "NimbusEffect.h"
#include "sst/effects/NimbusImpl.h"

#include "DebugHelpers.h"
#include "fmt/core.h"

NimbusEffect::NimbusEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : parent_t(storage, fxdata, pd)
{

    sampleRateReset();
}

const char *NimbusEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Configuration";
    case 1:
        return "Grain";
    case 2:
        return "Playback";
    case 3:
        return "Output";
    }
    return 0;
}

int NimbusEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 7;
    case 2:
        return 21;
    case 3:
        return 27;
    }
    return 0;
}

void NimbusEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    // Dynamic names and bipolarity support
    static struct DynTexDynamicNameBip : public ParameterDynamicNameFunction,
                                         ParameterDynamicBoolFunction
    {
        const char *getName(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            static std::string res;
            auto mode = fx->p[nmb_mode].val.i;

            switch (mode)
            {
            case 0:
                if (idx == nmb_density)
                    res = "Density";
                if (idx == nmb_texture)
                    res = "Texture";
                if (idx == nmb_size)
                    res = "Size";
                break;
            case 1:
            case 2:
                if (idx == nmb_density)
                    res = "Diffusion";
                if (idx == nmb_texture)
                    res = "Filter";
                if (idx == nmb_size)
                    res = "Size";
                break;
            case 3:
                if (idx == nmb_density)
                    res = "Smear";
                if (idx == nmb_texture)
                    res = "Texture";
                if (idx == nmb_size)
                    res = "Warp";
                break;
            }

            return res.c_str();
        }

        bool getValue(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            auto mode = fx->p[nmb_mode].val.i;

            auto isBipolar = false;
            switch (mode)
            {
            case 0:
                if (idx == nmb_density)
                    isBipolar = true;
                break;
            case 1:
            case 2:
                if (idx == nmb_texture)
                    isBipolar = true;
                break;
            case 3:
                if (idx != nmb_size)
                    isBipolar = true;
                break;
            }

            return isBipolar;
        }
    } dynTexDynamicNameBip;

    static struct SpreadDeactivator : public ParameterDynamicDeactivationFunction
    {
        bool getValue(const Parameter *p) const
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto mode = fx->p[nmb_mode].val.i;

            return mode != 0;
        }
    } spreadDeact;

    int ypos = 1;
    fxdata->p[nmb_mode].set_name("Mode");
    fxdata->p[nmb_mode].set_type(ct_nimbusmode);
    fxdata->p[nmb_mode].posy_offset = ypos;

    fxdata->p[nmb_quality].set_name("Quality");
    fxdata->p[nmb_quality].set_type(ct_nimbusquality);
    fxdata->p[nmb_quality].posy_offset = ypos;

    ypos += 2;
    fxdata->p[nmb_position].set_name("Position");
    fxdata->p[nmb_position].set_type(ct_percent);
    fxdata->p[nmb_position].posy_offset = ypos;
    fxdata->p[nmb_size].set_name("Size");
    fxdata->p[nmb_size].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    fxdata->p[nmb_size].dynamicName = &dynTexDynamicNameBip;
    fxdata->p[nmb_size].dynamicBipolar = &dynTexDynamicNameBip;

    fxdata->p[nmb_size].val_default.f = 0.5;
    fxdata->p[nmb_size].posy_offset = ypos;
    fxdata->p[nmb_pitch].set_name("Pitch");
    fxdata->p[nmb_pitch].set_type(ct_pitch4oct);
    fxdata->p[nmb_pitch].posy_offset = ypos;

    fxdata->p[nmb_density].set_name("Density");
    fxdata->p[nmb_density].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    fxdata->p[nmb_density].posy_offset = ypos;
    fxdata->p[nmb_density].dynamicName = &dynTexDynamicNameBip;
    fxdata->p[nmb_density].dynamicBipolar = &dynTexDynamicNameBip;

    fxdata->p[nmb_texture].set_name("Texture");
    fxdata->p[nmb_texture].set_type(ct_percent_bipolar_w_dynamic_unipolar_formatting);
    fxdata->p[nmb_texture].posy_offset = ypos;
    fxdata->p[nmb_texture].dynamicName = &dynTexDynamicNameBip;
    fxdata->p[nmb_texture].dynamicBipolar = &dynTexDynamicNameBip;

    fxdata->p[nmb_spread].set_name("Spread");
    fxdata->p[nmb_spread].set_type(ct_percent);
    fxdata->p[nmb_spread].dynamicDeactivation = &spreadDeact;
    fxdata->p[nmb_spread].posy_offset = ypos;

    ypos += 2;
    fxdata->p[nmb_freeze].set_name("Freeze");
    fxdata->p[nmb_freeze].set_type(ct_float_toggle); // so that it can be modulated
    fxdata->p[nmb_freeze].posy_offset = ypos;
    fxdata->p[nmb_feedback].set_name("Feedback");
    fxdata->p[nmb_feedback].set_type(ct_percent);
    fxdata->p[nmb_feedback].posy_offset = ypos;

    ypos += 2;
    fxdata->p[nmb_reverb].set_name("Reverb");
    fxdata->p[nmb_reverb].set_type(ct_percent);
    fxdata->p[nmb_reverb].posy_offset = ypos;

    fxdata->p[nmb_mix].set_name("Mix");
    fxdata->p[nmb_mix].set_type(ct_percent);
    fxdata->p[nmb_mix].val_default.f = 0.5;
    fxdata->p[nmb_mix].posy_offset = ypos;

    configureControlsFromFXMetadata();
}

// Just to be safe, explicitly instantiate the base class.
template struct surge::sstfx::SurgeSSTFXBase<
    sst::effects::nimbus::Nimbus<surge::sstfx::SurgeFXConfig>>;