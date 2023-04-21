
#include "InputBlenderEffect.h"
InputBlenderEffect::InputBlenderEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd) {}
InputBlenderEffect::~InputBlenderEffect() {}
void InputBlenderEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();
    effect_slot_type slot_type = getSlotType(fxdata->fxslot);

    // -----  Audio Input
    fxdata->p[ibp_audio_input_channel].set_name("Channel");
    fxdata->p[ibp_audio_input_channel].set_type(ct_vocoder_modulator_mode);
    fxdata->p[ibp_audio_input_channel].posy_offset = 1;

    fxdata->p[ibp_audio_input_pan].set_name("Pan");
    fxdata->p[ibp_audio_input_pan].set_type(ct_percent_bipolar_stereo);
    fxdata->p[ibp_audio_input_pan].posy_offset = 1;

    fxdata->p[ibp_audio_input_level].set_name("Level");
    fxdata->p[ibp_audio_input_level].set_type(ct_decibel_attenuation);
    fxdata->p[ibp_audio_input_level].posy_offset = 1;

    // -----  Effect Input
    fxdata->p[ibp_effect_input_channel].set_name("Channel");
    fxdata->p[ibp_effect_input_channel].set_type(ct_vocoder_modulator_mode);
    fxdata->p[ibp_effect_input_channel].posy_offset = 3;

    fxdata->p[ibp_effect_input_pan].set_name("Pan");
    fxdata->p[ibp_effect_input_pan].set_type(ct_percent_bipolar_stereo);
    fxdata->p[ibp_effect_input_pan].posy_offset = 3;

    fxdata->p[ibp_effect_input_level].set_name("Level");
    fxdata->p[ibp_effect_input_level].set_type(ct_decibel_attenuation);
    fxdata->p[ibp_effect_input_level].posy_offset = 3;

    // -----  Scene Input
    fxdata->p[ibp_scene_input_channel].set_name("Channel");
    fxdata->p[ibp_scene_input_channel].set_type(ct_vocoder_modulator_mode);
    fxdata->p[ibp_scene_input_channel].posy_offset = 5;

    fxdata->p[ibp_scene_input_pan].set_name("Pan");
    fxdata->p[ibp_scene_input_pan].set_type(ct_percent_bipolar_stereo);
    fxdata->p[ibp_scene_input_pan].posy_offset = 5;

    fxdata->p[ibp_scene_input_level].set_name("Level");
    fxdata->p[ibp_scene_input_level].set_type(ct_decibel_attenuation);
    fxdata->p[ibp_scene_input_level].posy_offset = 5;

    // -----  Output

    fxdata->p[ibp_output_width].set_name("Width");
    fxdata->p[ibp_output_width].set_type(ct_percent_bipolar_stereo);
    fxdata->p[ibp_output_width].posy_offset = 7;

    fxdata->p[ibp_output_mix].set_name("Mix");
    fxdata->p[ibp_output_mix].set_type(ct_percent);
    fxdata->p[ibp_output_mix].posy_offset = 7;

}

void InputBlenderEffect::init_default_values()
{
    fxdata->p[ibp_audio_input_channel].val.i = 0;
    fxdata->p[ibp_audio_input_level].val.f = 0.0;
    fxdata->p[ibp_effect_input_level].val.f = 0.0;
}
const char *InputBlenderEffect::group_label(int id) {
    switch (id)
    {
    case 0:
        return "Audio Input";
    case 1:
        return "Effect Input";
    case 3:
        return "Scene Input";
    case 4:
        return "Output";
    }
    return 0;
}


int InputBlenderEffect::group_label_ypos(int id) {
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 9;
    case 3:
        return 17;
    case 4:
        return 25;
    }
    return 0;
}
void InputBlenderEffect::process(float *dataL, float *dataR)
{
    Effect::process(dataL, dataR);
}
InputBlenderEffect::effect_slot_type InputBlenderEffect::getSlotType(fxslot_positions p)
{
    switch (p)
    {
        case fxslot_ains1:
        case fxslot_ains2:
        case fxslot_ains3:
        case fxslot_ains4:
            return InputBlenderEffect::a_insert_slot;
        case fxslot_bins1:
        case fxslot_bins2:
        case fxslot_bins3:
        case fxslot_bins4:
            return InputBlenderEffect::b_insert_slot;
        default:
            return InputBlenderEffect::global_slot;
    }
}
