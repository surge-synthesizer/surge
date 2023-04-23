
#include "AudioInputEffect.h"
AudioInputEffect::AudioInputEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd) {}
AudioInputEffect::~AudioInputEffect() {}
void AudioInputEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    // -----  Audio Input
    fxdata->p[in_audio_input_channel].set_name("Channel");
    fxdata->p[in_audio_input_channel].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_audio_input_channel].posy_offset = 1;

    fxdata->p[in_audio_input_pan].set_name("Pan");
    fxdata->p[in_audio_input_pan].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_audio_input_pan].posy_offset = 1;

    fxdata->p[in_audio_input_level].set_name("Level");
    fxdata->p[in_audio_input_level].set_type(ct_decibel_attenuation);
    fxdata->p[in_audio_input_level].posy_offset = 1;

    // -----  Effect Input
    fxdata->p[in_effect_input_channel].set_name("Channel");
    fxdata->p[in_effect_input_channel].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_effect_input_channel].posy_offset = 3;

    fxdata->p[in_effect_input_pan].set_name("Pan");
    fxdata->p[in_effect_input_pan].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_effect_input_pan].posy_offset = 3;

    fxdata->p[in_effect_input_level].set_name("Level");
    fxdata->p[in_effect_input_level].set_type(ct_decibel_attenuation);
    fxdata->p[in_effect_input_level].posy_offset = 3;

    // -----  Scene Input
    effect_slot_type slot_type = getSlotType(fxdata->fxslot);
    if (slot_type == a_insert_slot || slot_type == b_insert_slot)
    {
        fxdata->p[in_scene_input_channel].set_name("Channel");
        fxdata->p[in_scene_input_channel].set_type(ct_percent_bipolar_stereo);
        fxdata->p[in_scene_input_channel].posy_offset = 5;

        fxdata->p[in_scene_input_pan].set_name("Pan");
        fxdata->p[in_scene_input_pan].set_type(ct_percent_bipolar_stereo);
        fxdata->p[in_scene_input_pan].posy_offset = 5;

        fxdata->p[in_scene_input_level].set_name("Level");
        fxdata->p[in_scene_input_level].set_type(ct_decibel_attenuation);
        fxdata->p[in_scene_input_level].posy_offset = 5;

    }

    // -----  Output

    fxdata->p[in_output_width].set_name("Width");
    fxdata->p[in_output_width].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_output_width].posy_offset = 7;

    fxdata->p[in_output_mix].set_name("Mix");
    fxdata->p[in_output_mix].set_type(ct_percent);
    fxdata->p[in_output_mix].posy_offset = 7;

}

void AudioInputEffect::init_default_values()
{
    fxdata->p[in_audio_input_channel].val.i = 3; // p0
    fxdata->p[in_audio_input_pan].val.f = 0.0;   //p1
    fxdata->p[in_audio_input_level].val.f = 0.0;  //p2


    fxdata->p[in_effect_input_channel].val.i = 3; //p3
    fxdata->p[in_effect_input_pan].val.f = 0.0;  //p4
    fxdata->p[in_effect_input_level].val.f = 0.0; //p5


    fxdata->p[in_scene_input_channel].val.i = 3; //p6
    fxdata->p[in_scene_input_pan].val.f = 0.0; //p7
    fxdata->p[in_scene_input_level].val.f = 0.0; //p8

    fxdata->p[in_output_width].val.f = 0.0; //p9
    fxdata->p[in_output_mix].val.f = 1.0;  //p10
}
const char *AudioInputEffect::group_label(int id) {
    std::vector group_labels = {{"Audio Input", "Effect Input", "Scene Input", "Output"}};
    effect_slot_type slot_type = getSlotType(fxdata->fxslot);
    if (slot_type == a_insert_slot)
        group_labels[2] = "Scene B Input";
    else if (slot_type == b_insert_slot)
        group_labels[2] = "Scene A Input";
    else
        group_labels.erase(group_labels.begin() + 2);
    if (id>=0 && id < group_labels.size())
        return group_labels[id];
    return 0;
}


int AudioInputEffect::group_label_ypos(int id) {
    std::vector ypos = {1, 9, 17, 25};
    effect_slot_type slot_type = getSlotType(fxdata->fxslot);
    if (slot_type != a_insert_slot && slot_type != b_insert_slot)
        ypos.erase(ypos.begin() + 2);
    if (id>=0 && id < ypos.size())
        return ypos[id];
    return 0;
}
void AudioInputEffect::process(float *dataL, float *dataR)
{
    Effect::process(dataL, dataR);
}
AudioInputEffect::effect_slot_type AudioInputEffect::getSlotType(fxslot_positions p)
{
    switch (p)
    {
        case fxslot_ains1:
        case fxslot_ains2:
        case fxslot_ains3:
        case fxslot_ains4:
            return AudioInputEffect::a_insert_slot;
        case fxslot_bins1:
        case fxslot_bins2:
        case fxslot_bins3:
        case fxslot_bins4:
            return AudioInputEffect::b_insert_slot;
        case fxslot_send1:
        case fxslot_send2:
        case fxslot_send3:
        case fxslot_send4:
            return AudioInputEffect::send_slot;
        default:
            return AudioInputEffect::global_slot;
    }
}
